/**
 * @file    camerad_watchdog.cpp
 * @brief   liveness watchdog for the camerad daemon
 * @details Periodically probes camerad's non-blocking command port with a
 *          side-effect-free "ping" and restarts the systemd unit when the
 *          daemon stops answering. Heartbeats systemd's WatchdogSec= from
 *          inside the probe loop so that a wedged watchdog is itself caught.
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * This is a standalone supervisor intended to run as a systemd Type=notify
 * service alongside camerad. It reuses the camera-interface utility libraries
 * (Config, logentry, Network) so it reads the same configuration file and logs
 * the same way camerad does.
 *
 * Each probe opens a fresh connection, sends "ping", expects "pong", and closes
 * -- the standard liveness-probe pattern, which also exercises the accept path
 * every cycle and completes well inside camerad's per-connection idle timeout.
 * camerad answers "ping" without logging the command, and does not log a
 * probe-only connection at all, so a healthy system produces no log traffic for
 * the periodic health checks. A startup grace window holds off probing while
 * camerad first comes up.
 *
 * Usage: camerad-watchdog <config-file> [unit-name]
 *        where unit-name is the systemd unit to supervise, default "camerad".
 *
 */

#include <algorithm>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "logentry.h"
#include "network.h"
#include "sd_notify.h"


/***** Watchdog **************************************************************/
/**
 * @namespace Watchdog
 * @brief     contains the camerad liveness-watchdog implementation
 *
 */
namespace Watchdog {

    const std::string DAEMON_NAME = "camerad-watchdog"; /// name used for logging

    constexpr int PROBE_INTERVAL_SEC = 5;    /// seconds between probes
    constexpr int PROBE_TIMEOUT_MS   = 2000; /// per-probe reply budget in msec
    constexpr int FAIL_THRESHOLD     = 3;    /// consecutive misses before a restart
    constexpr int COOLDOWN_SEC       = 30;   /// minimum seconds between restarts
    constexpr int STARTUP_GRACE_SEC  = 30;   /// delay before the first probe at startup

    const std::string PROBE_HOST = "127.0.0.1"; /// camerad listens locally
    const std::string SYSTEMCTL  = "/usr/bin/systemctl";


    /***** Watchdog::probe ****************************************************/
    /**
     * @brief      open a fresh connection, send "ping", verify the "pong" reply
     * @param[in]  port  camerad non-blocking command port
     * @return     true if the daemon replied "pong", false otherwise
     *
     * A new connection is opened and closed for every probe. This is the
     * standard liveness-probe pattern and matches how camerad expects clients to
     * behave (connect, command, reply, disconnect), completing well within the
     * server's per-connection idle timeout.
     *
     */
    bool probe(int port) {
        Network::TcpSocket sock(PROBE_HOST, port);

        if (sock.Connect() < 0)               { sock.Close(); return false; }
        if (sock.Write("ping\n") < 0)         { sock.Close(); return false; }

        // wait for "pong" within the reply budget
        //
        if (sock.Poll(PROBE_TIMEOUT_MS) <= 0) { sock.Close(); return false; }

        std::string reply;
        if (sock.Read(reply, '\n') <= 0)      { sock.Close(); return false; }
        sock.Close();

        // strip line framing and compare against the expected reply
        //
        reply.erase(std::remove(reply.begin(), reply.end(), '\r'), reply.end());
        reply.erase(std::remove(reply.begin(), reply.end(), '\n'), reply.end());

        return (reply == "pong");
    }
    /***** Watchdog::probe ****************************************************/


    /***** Watchdog::unit_is_active ********************************************/
    /**
     * @brief      check whether the systemd unit is currently active
     * @param[in]  unit  systemd unit name
     * @return     true if "systemctl is-active --quiet <unit>" exits 0
     *
     * Used to avoid fighting a commanded stop: when the unit is intentionally
     * stopped it is not active, and the watchdog must not restart it.
     *
     */
    bool unit_is_active(const std::string &unit) {
        pid_t pid = fork();
        if (pid == 0) {
            // child: silence systemctl and exec it
            //
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); }
            execl(SYSTEMCTL.c_str(), "systemctl", "is-active", "--quiet", unit.c_str(), (char *)nullptr);
            _exit(127); // exec failed
        }
        int status = 0;
        if (pid > 0) waitpid(pid, &status, 0);
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    /***** Watchdog::unit_is_active ********************************************/


    /***** Watchdog::restart_unit *********************************************/
    /**
     * @brief      restart the systemd unit via systemctl
     * @param[in]  unit  systemd unit name
     *
     */
    void restart_unit(const std::string &unit) {
        std::string function = "Watchdog::restart_unit";
        logwrite(function, "restarting unit " + unit);

        pid_t pid = fork();
        if (pid == 0) {
            execl(SYSTEMCTL.c_str(), "systemctl", "restart", unit.c_str(), (char *)nullptr);
            _exit(127); // exec failed
        }
        int status = 0;
        if (pid > 0) waitpid(pid, &status, 0);
    }
    /***** Watchdog::restart_unit *********************************************/

}
/***** Watchdog **************************************************************/


/***** main ******************************************************************/
/**
 * @brief      the main function
 * @param[in]  argc  argument count
 * @param[in]  argv  argument list: <config-file> [unit-name]
 * @return     0 on normal exit, non-zero on a startup error
 *
 */
int main(int argc, char **argv) {
    std::string function = "Watchdog::main";
    std::stringstream message;

    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <config-file> [unit-name]\n";
        return 2;
    }

    std::string unit = (argc >= 3 ? std::string(argv[2]) : std::string("camerad"));

    // Read the camerad configuration file to learn the command port and the
    // log path, so the watchdog tracks the same config as the daemon.
    //
    Config config(argv[1]);
    try {
        config.read_config();
    }
    catch (const std::exception &e) {
        std::cerr << "ERROR reading config file: " << e.what() << "\n";
        return 1;
    }

    int nbport = -1;
    std::string log_path;
    for (int entry = 0; entry < config.n_rows; entry++) {
        if (config.param[entry] == "NBPORT")  nbport = std::stoi(config.arg[entry]);
        if (config.param[entry] == "LOGPATH") log_path = config.arg[entry];
    }

    if (nbport < 0) {
        std::cerr << "ERROR NBPORT not found in " << argv[1] << "\n";
        return 1;
    }
    if (log_path.empty()) log_path = "/tmp";

    // initialize logging the same way camerad does
    //
    init_log(Watchdog::DAEMON_NAME, log_path, "true", "");

    message.str(""); message << "watching unit \"" << unit << "\" via "
                             << Watchdog::PROBE_HOST << ":" << nbport;
    logwrite(function, message.str());

    // notify systemd we are ready (no-op when not running under Type=notify)
    //
    Systemd::sd_notify_ready();

    // Startup grace: give camerad time to come up before probing for liveness,
    // so a slow first start is not mistaken for a hang. WatchdogSec= is still
    // heartbeated throughout the wait.
    //
    message.str(""); message << "startup grace " << Watchdog::STARTUP_GRACE_SEC
                             << "s before first probe";
    logwrite(function, message.str());
    {
        time_t grace_end = time(nullptr) + Watchdog::STARTUP_GRACE_SEC;
        while (time(nullptr) < grace_end) {
            Systemd::sd_notify_watchdog();
            sleep(Watchdog::PROBE_INTERVAL_SEC);
        }
    }

    int fails = 0;
    time_t last_restart = 0;

    while (true) {
        // heartbeat systemd's WatchdogSec= from inside the probe loop
        //
        Systemd::sd_notify_watchdog();

        if (Watchdog::probe(nbport)) {
            if (fails > 0) {
                message.str(""); message << "probe recovered after " << fails << " miss(es)";
                logwrite(function, message.str());
            }
            fails = 0;
        }
        else {
            ++fails;
            message.str(""); message << "probe miss " << fails << " of " << Watchdog::FAIL_THRESHOLD;
            logwrite(function, message.str());

            if (fails >= Watchdog::FAIL_THRESHOLD) {
                time_t now = time(nullptr);

                if (now - last_restart < Watchdog::COOLDOWN_SEC) {
                    logwrite(function, "in cooldown, not restarting yet");
                }
                else if (!Watchdog::unit_is_active(unit)) {
                    // a commanded stop leaves the unit inactive: don't fight it
                    //
                    logwrite(function, "unit not active (commanded stop?), standing down");
                    fails = 0;
                }
                else {
                    Watchdog::restart_unit(unit);
                    last_restart = now;
                    fails = 0;
                }
            }
        }

        sleep(Watchdog::PROBE_INTERVAL_SEC);
    }

    return 0;
}
/***** main ******************************************************************/
