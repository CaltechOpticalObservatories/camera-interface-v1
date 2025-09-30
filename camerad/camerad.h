/** ---------------------------------------------------------------------------
 * @file     camerad.h
 * @brief    
 * @author   David Hale <dhale@astro.caltech.edu>
 * @date     
 * @modified 
 *
 */

#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <csignal>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#ifdef ASTROCAM
#include "astrocam.h"       //!< for any Leach interface, ARC-64 or ARC-66
#elif STA_ARCHON
#include "archon.h"         //!< for STA-Archon
#endif

#include "logentry.h"
#include "config.h"
#include "network.h"

#define  N_THREADS    10    //!< total number of threads spawned by server, one for blocking and the remainder for non-blocking

#define  BUFSIZE      1024  //!< size of the input command buffer
#define  CONN_TIMEOUT 3000  //<! incoming (non-blocking) connection timeout in milliseconds

namespace Camera {
    const std::string DAEMON_NAME = "camerad"; /// when run as a daemon, this is my name

    // Camera::Server class must inherit appropriate interface class
    //
#ifdef ASTROCAM
  class Server : public AstroCam::Interface {
#elif STA_ARCHON
    class Server : public Archon::Interface {
#endif

    private:
    public:
        int nbport; //!< non-blocking port
        int blkport; //!< blocking port
        int asyncport; //!< asynchronous message port
        int cmd_num;

        Server() : nbport(-1), blkport(-1), asyncport(-1), cmd_num(0) {
        }

        ~Server() {
            close(this->nonblocking_socket);
            close(this->blocking_socket);
            close_log(); // close the logfile, if open
        }

        std::string asyncgroup; //!< asynchronous multicast group

        int nonblocking_socket;
        int blocking_socket;

        Network::TcpSocket nonblocking;

        std::mutex conn_mutex; //!< mutex to protect against simultaneous access to Accept()

        /***** Camera::Server::exit_cleanly *************************************/
        /**
         * @brief  handles ctrl-C and exits
         * @return nothing
         *
         */
        void exit_cleanly(void) {
            std::string function = "Camera::Server::exit_cleanly";
            this->disconnect_controller();
            logwrite(function, "server exiting");
            exit(EXIT_SUCCESS);
        }

        /***** Camera::Server::exit_cleanly *************************************/


        /***** Camera::Server::configure_server *********************************/
        /**
         * @brief      parse server-related keys from the configuration file
         * @details    the config file was read by server.config.read_config() in main()
         * @return     ERROR or NO_ERROR
         *
         */
        long configure_server() {
            std::string function = "Camera::Server::configure_server";
            std::stringstream message;
            int applied = 0;
            long error;

            // loop through the entries in the configuration file, stored in config class
            //
            for (int entry = 0; entry < this->config.n_entries; entry++) {
                // NBPORT
                if (config.param[entry].compare(0, 6, "NBPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        this->camera.log_error(function, "bad NBPORT: unable to convert to integer");
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        this->camera.log_error(function, "NBPORT number out of integer range");
                        return (ERROR);
                    }
                    this->nbport = port;
                    message.str("");
                    message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
                    logwrite(function, message.str());
                    this->camera.async.enqueue(message.str());
                    applied++;
                }

                // BLKPORT
                if (config.param[entry].compare(0, 7, "BLKPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        this->camera.log_error(function, "bad BLKPORT: unable to convert to integer");
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        this->camera.log_error(function, "BLKPORT number out of integer range");
                        return (ERROR);
                    }
                    this->blkport = port;
                    message.str("");
                    message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
                    logwrite(function, message.str());
                    this->camera.async.enqueue(message.str());
                    applied++;
                }

                // ASYNCPORT
                if (config.param[entry].compare(0, 9, "ASYNCPORT") == 0) {
                    int port;
                    try {
                        port = std::stoi(config.arg[entry]);
                    } catch (std::invalid_argument &) {
                        this->camera.log_error(function, "bad ASYNCPORT: unable to convert to integer");
                        return (ERROR);
                    }
                    catch (std::out_of_range &) {
                        this->camera.log_error(function, "ASYNCPORT number out of integer range");
                        return (ERROR);
                    }
                    this->asyncport = port;
                    message.str("");
                    message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
                    logwrite(function, message.str());
                    this->camera.async.enqueue(message.str());
                    applied++;
                }

                // ASYNCGROUP
                if (config.param[entry].compare(0, 10, "ASYNCGROUP") == 0) {
                    this->asyncgroup = config.arg[entry];
                    message.str("");
                    message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
                    logwrite(function, message.str());
                    this->camera.async.enqueue(message.str());
                    applied++;
                }

                // LONGERROR
                if (config.param[entry].compare(0, 9, "LONGERROR") == 0) {
                    std::string dontcare;
                    if (this->camera.longerror(config.arg[entry], dontcare) == ERROR) {
                        this->camera.log_error(function, "setting longerror");
                        return (ERROR);
                    }
                    message.str("");
                    message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
                    logwrite(function, message.str());
                    this->camera.async.enqueue(message.str());
                    applied++;
                }
            } // end loop through the entries in the configuration file

            message.str("");
            if (applied == 0) {
                message << "ERROR: ";
                error = ERROR;
            } else {
                error = NO_ERROR;
            }
            message << "applied " << applied << " configuration lines to server";
            error == NO_ERROR ? logwrite(function, message.str()) : this->camera.log_error(function, message.str());
            return error;
        }

        /***** Camera::Server::configure_server *********************************/
    }; // end class Server
} // end namespace Camera
