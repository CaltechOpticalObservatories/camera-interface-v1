/**
 * @file    sd_notify.h
 * @brief   minimal sd_notify(3) shim for systemd service notifications
 * @details Sends service-manager notifications (READY=1, WATCHDOG=1) over the
 *          AF_UNIX datagram socket named by $NOTIFY_SOCKET, with no dependency
 *          on libsystemd. Used by camerad and by the camerad watchdog.
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Every function is a silent no-op when $NOTIFY_SOCKET is unset, so a program
 * using these calls runs identically whether or not it is launched by systemd.
 *
 */

#pragma once

#include <cstddef>     // offsetof(3)
#include <cstdlib>     // getenv(3)
#include <cstring>     // memset(3), memcpy(3), strlen(3)
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>    // close(2)


/***** Systemd ***************************************************************/
/**
 * @namespace Systemd
 * @brief     contains functions for notifying the systemd service manager
 *
 */
namespace Systemd {

    /***** Systemd::sd_notify ***************************************************/
    /**
     * @brief      send one newline-free state string to the service manager
     * @param[in]  state  the notification string, E.G. "READY=1" or "WATCHDOG=1"
     * @return     true on success, false if not under systemd or on error
     *
     * Connects an AF_UNIX SOCK_DGRAM socket to $NOTIFY_SOCKET and sends one
     * datagram. Both the path form ("/run/...") and the abstract-namespace
     * form (leading '@') of NOTIFY_SOCKET are supported.
     *
     */
    inline bool sd_notify(const std::string &state) {
        const char *path = getenv("NOTIFY_SOCKET");
        if (path == nullptr || *path == '\0') return false; // not under systemd

        int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return false;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        size_t len = strlen(path);
        if (len >= sizeof(addr.sun_path)) { close(fd); return false; }
        memcpy(addr.sun_path, path, len);
        socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) + len;

        // an abstract-namespace socket has a leading '@' that maps to a NUL
        //
        if (addr.sun_path[0] == '@') addr.sun_path[0] = '\0';

        ssize_t nsent = sendto(fd, state.data(), state.size(), MSG_NOSIGNAL,
                               (struct sockaddr *)&addr, addrlen);
        close(fd);
        return (nsent == (ssize_t)state.size());
    }
    /***** Systemd::sd_notify ***************************************************/


    /***** Systemd::sd_notify_ready *********************************************/
    /**
     * @brief      notify the service manager that startup is complete
     * @return     true on success, false otherwise
     *
     */
    inline bool sd_notify_ready() { return sd_notify("READY=1"); }
    /***** Systemd::sd_notify_ready *********************************************/


    /***** Systemd::sd_notify_watchdog ******************************************/
    /**
     * @brief      send a keep-alive heartbeat for a WatchdogSec= service
     * @return     true on success, false otherwise
     *
     */
    inline bool sd_notify_watchdog() { return sd_notify("WATCHDOG=1"); }
    /***** Systemd::sd_notify_watchdog ******************************************/

}
/***** Systemd ***************************************************************/
