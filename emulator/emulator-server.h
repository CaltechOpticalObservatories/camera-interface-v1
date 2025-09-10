/** ---------------------------------------------------------------------------
 * @file     emulator-server.h
 * @brief    include file for Emulator::Server
 * @author   David Hale <dhale@astro.caltech.edu>
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

#include "emulator-archon.h"

#include "logentry.h"
#include "config.h"
#include "network.h"

namespace Emulator {

  constexpr const int BUFSIZE = 1024;    //!< size of the input command buffer
  constexpr const int TIMEOUT = 3000;    //!< incoming (non-blocking) connection timeout in milliseconds

  class Server : public Archon::Interface {
    public:
      int emulatorport;                  //!< emulator port
      int nbport;                        //!< non-blocking port
      int blkport;                       //!< blocking port

      Server( std::string instr ) : Interface(instr), emulatorport(-1), nbport(-1), blkport(-1) { }

      ~Server() {
        close(this->blocking_socket);
      }

      int nonblocking_socket;
      int blocking_socket;

      Network::TcpSocket nonblocking;

      std::mutex conn_mutex;             //!< mutex to protect against simultaneous access to Accept()

      /***** Emulator::Server::exit_cleanly ***********************************/
      /**
       * @brief      exit
       *
       */
      void exit_cleanly(void) {
        std::cerr << "server exiting\n";
        exit(EXIT_SUCCESS);
      }
      /***** Emulator::Server::exit_cleanly ***********************************/


      /***** Emulator::Server::configure_server *******************************/
      /**
       * @brief
       * @return     ERROR or NO_ERROR
       *
       */
      long configure_server() {
        std::string function = "(Emulator::Server::configure_server) ";
        std::stringstream message;
        int applied=0;
        long error;

        // loop through the entries in the configuration file, stored in config class
        //
        for (int entry=0; entry < this->config.n_entries; entry++) {

          if ( config.param[entry] == "INSTRUMENT" ) {
          }

          // EMULATOR_PORT
          if (config.param[entry].compare(0, 13, "EMULATOR_PORT")==0) {
            int port;
            try {
              port = std::stoi( config.arg[entry] );
            }
            catch (std::invalid_argument &) {
              std::cerr << function << "ERROR: bad EMULATOR_PORT: unable to convert to integer\n";
              return(ERROR);
            }
            catch (std::out_of_range &) {
              std::cerr << function << "EMULATOR_PORT number out of integer range\n";
              return(ERROR);
            }
            this->emulatorport = port;
            applied++;
          }

          // NBPORT
          if (config.param[entry].compare(0, 6, "NBPORT")==0) {
            int port;
            try {
              port = std::stoi( config.arg[entry] );
            }
            catch (std::invalid_argument &) {
              std::cerr << function << "ERROR: bad NBPORT: unable to convert to integer\n";
              return(ERROR);
            }
            catch (std::out_of_range &) {
              std::cerr << function << "NBPORT number out of integer range\n";
              return(ERROR);
            }
            this->nbport = port;
            applied++;
          }

          // BLKPORT
          if (config.param[entry].compare(0, 7, "BLKPORT")==0) {
            int port;
            try {
              port = std::stoi( config.arg[entry] );
            }
            catch (std::invalid_argument &) {
              std::cerr << function << "ERROR: bad BLKPORT: unable to convert to integer\n";
              return(ERROR);
            }
            catch (std::out_of_range &) {
              std::cerr << function << "BLKPORT number out of integer range\n";
              return(ERROR);
            }
            this->blkport = port;
            applied++;
          }

        } // end loop through the entries in the configuration file

        message.str("");
        if (applied==0) {
          message << "ERROR: ";
          error = ERROR;
        } 
        else {
          error = NO_ERROR;
        } 
        message << "applied " << applied << " configuration lines to server";
        std::cerr << function << message.str() << "\n";
        return error;
      }
      /***** Emulator::Server::configure_server *******************************/

  };  // end class Server

} // end namespace Emulator
