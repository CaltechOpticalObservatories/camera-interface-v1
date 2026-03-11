/**
 * @file    camera_server.cpp
 * @brief   implementation of camera daemon server
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "camera_server.h"

namespace Camera {

  /***** Camera::Server::Server ***********************************************/
  /**
   * @brief      Server constructor
   *
   */
  Server::Server() :
    blkport(-1),
    id_pool(N_THREADS),
    cmd_num(0)
  {
    message_server.set_handler( std::bind(&Camera::Server::dispatch, this,
                                                                     std::placeholders::_1,
                                                                     std::placeholders::_2) );

    interface=Camera::Interface::create();  // factory funcion creates the appropriate interface type
    interface->set_server(this);            // pointer back to this Server instance
  }
  /***** Camera::Server::Server ***********************************************/


  /***** Camera::Server::~Server **********************************************/
  /**
   * @brief      Server destructor
   *
   */
  Server::~Server() {
  }
  /***** Camera::Server::~Server **********************************************/


  /***** Camera::Server::configure_server *************************************/
  /**
   * @brief      parse the configuration file for server-related parameters
   * @details    The config file has already been read into the Config class.
   * @throws     std::runtime_error
   *
   */
  void Server::configure_server() {
    const std::string function("Camera::Server::configure_server");
    logwrite(function, "");

    if (interface->configfile.n_rows < 1) throw std::runtime_error("empty configuration");

    // iterate through each row in config file
    for (int row=0; row < interface->configfile.n_rows; row++) {

      // BLKPORT
      if (interface->configfile.param[row]=="BLKPORT") {
        try {
          this->blkport = std::stoi( interface->configfile.arg[row] );
        }
        catch (const std::exception &e) {
          std::ostringstream oss;
          oss << "parsing " << interface->configfile.param[row]
                            << "=" << interface->configfile.arg[row] << ": " << e.what();
          throw std::runtime_error(oss.str());
        }
      }
    }
  }
  /***** Camera::Server::configure_server *************************************/


  /***** Camera::Server::exit_cleanly *****************************************/
  /**
   * @brief      exit the server
   *
   */
  void Server::exit_cleanly() {
    const std::string function("Camera::Server::exit_cleanly");
    this->interface->disconnect_controller();
    this->message_server.stop();
    logwrite(function, "server exiting");
    exit(EXIT_SUCCESS);
  }
  /***** Camera::Server::exit_cleanly *****************************************/


  /***** Camera::Server::block_main *******************************************/
  /**
   * @brief      main function for blocking connection thread
   * @param[in]  sock  shared pointer to Network::TcpSocket socket object
   *
   * accepts a socket connection and processes the request by
   * calling function doit()
   *
   */
  void Server::block_main( std::shared_ptr<Network::TcpSocket> sock ) {
    this->threads_active.fetch_add(1);  // atomically increment threads_busy counter
    sock->Close();
    this->threads_active.fetch_sub(1);  // atomically increment threads_busy counter
    this->id_pool.release_number( sock->id );
    return;
  }
  /***** Camera::Server::block_main *******************************************/


  /***** Camera::Server::doit *************************************************/
  /**
   * @brief      the workhorse of each thread connection
   * @details    incoming commands are parsed here and acted upon
   * @param[in]  sock  Network::TcpSocket socket object
   *
   */
  std::string Server::dispatch(const std::string &message, Message::AckFunction send_ack) {
    const std::string function("Camera::Server::dispatch");
    std::string cmd, args;
    bool connection_open=true;
    long ret;


        std::size_t cmd_sep = message.find_first_of(" "); // find the first space, which separates command from argument list

        cmd = message.substr(0, cmd_sep);                 // cmd is everything up until that space

        if (cmd_sep == std::string::npos) {            // If no space was found,
          args.clear();                                // then the arg list is empty,
        }
        else {
          args= message.substr(cmd_sep+1);                // otherwise args is everything after that space.
        }

        std::ostringstream oss;
        oss << "received command " << ++cmd_id << ": " << message;
        logwrite(function, oss.str());

      //
      // Process commands here
      //

      ret = NOTHING;
      std::string retstring;

      if ( cmd == "-h" || cmd == "--help" || cmd == "help" || cmd == "?" ) {
        retstring="camera { <CMD> } [<ARG>...]\n";
        retstring.append( "  where <CMD> is one of:\n" );
        for ( const auto &s : CAMERAD_SYNTAX ) {
          retstring.append("  "); retstring.append( s ); retstring.append( "\n" );
        }
        ret = HELP;
      }
      else
      if ( cmd == CAMERAD_ABORT ) {
        ret = interface->abort(args, retstring);
      }
      else
      if ( cmd == CAMERAD_AUTODIR ) {
        ret = interface->autodir(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BASENAME ) {
        ret = interface->basename(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BIAS ) {
        ret = interface->bias(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BIN ) {
        ret = interface->bias(args, retstring);
      }
      else
      if ( cmd == CAMERAD_CLOSE ) {
        ret = interface->disconnect_controller(args, retstring);
      }
      else
      if ( cmd == CAMERAD_EXIT ) {
        // Start a thread that waits before sending SIGTERM.
        // The wait allows the reply to propagate back to the client
        // so that it has a clean exit.
        //
        std::thread([]() { std::this_thread::sleep_for(std::chrono::milliseconds(250));
                           kill(getpid(), SIGTERM);
                         }).detach();
        retstring="goodbye";
        ret = NO_ERROR;
      }
      else
      if ( cmd == CAMERAD_EXPTIME ) {
        ret = interface->exptime(args, retstring);
      }
      else
      if ( cmd == CAMERAD_EXPOSE ) {
        ret = interface->expose(args, retstring);
      }
      else
      if ( cmd == CAMERAD_EXPOSUREMODE ) {
        ret = interface->exposure_mode(args, retstring);
      }
      else
      if ( cmd == CAMERAD_LOAD ) {
        ret = interface->load_firmware(args, retstring);
      }
      else
      if ( cmd == CAMERAD_OPEN ) {
        ret = interface->connect_controller(args, retstring);
      }
      else
      if ( cmd == CAMERAD_NATIVE ) {
        ret = interface->native(args, retstring);
      }
      else
      if ( cmd == CAMERAD_POWER ) {
        ret = interface->power(args, retstring);
      }
      else
      if ( cmd == CAMERAD_TEST ) {
        ret = interface->test(args, retstring);
      }
      /**
       * instrument-specific commands
       */
      else
      if ( cmd == "hispec_this" ) {
        ret = interface->instrument_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == "hispec_that" ) {
        ret = interface->instrument_cmd(cmd, args, retstring);
      }
      /**
       * controller-specific commands
       */
      else
      if ( cmd == CAMERAD_LOADTIMING ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == CAMERAD_READACF ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == "getp" ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == "inreg" ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == "setp" ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == CAMERAD_MODE ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }
      else
      if ( cmd == "bob" ) {
        ret = interface->controller_cmd(cmd, args, retstring);
      }

      // unknown commands generate an error
      //
      else {
        logwrite(function, "ERROR unknown command: "+cmd);
        ret = ERROR;
      }

      // If retstring not empty then append "DONE" or "ERROR" depending on value of ret,
      // and log the reply along with the command number. Write the reply back to the socket.
      //
      // Don't append anything nor log the reply if the command was just requesting help.
      //
      if (ret != NOTHING) {
        if ( ret != HELP && !retstring.empty() ) retstring.append( " " );
        if ( ret != HELP && ret != JSON ) retstring.append( ret == NO_ERROR ? "DONE" : "ERROR" );

        if ( ret == JSON ) {
          logwrite(function, "command ("+std::to_string(cmd_id)+") reply with JSON message");
        }
        else
        if ( ! retstring.empty() && ret != HELP ) {
          retstring.append( "\n" );
          logwrite(function, "command ("+std::to_string(cmd_id)+") reply: "+retstring);
        }
      }
      return retstring;
  }
  /***** Camera::Server::doit *************************************************/

}
