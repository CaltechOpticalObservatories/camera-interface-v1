/**
 * @file    camera_server.cpp
 * @brief   implementation of camera daemon server
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "camera_server.h"

// The CONTROLLER_xxxx is defined by the CMakeLists file 
// and selects which Interface implementation to use.
//
#ifdef CONTROLLER_ARCHON
  #include "archon_interface.h"
  using ControllerType = Camera::ArchonInterface;
#elif CONTROLLER_ASTROCAM
  #include "astrocam_interface.h"
  using ControllerType = Camera::AstroCamInterface;
#else
  #error "ERROR controller not defined"
#endif


namespace Camera {


  /***** Camera::Server::Server ***********************************************/
  /**
   * @brief      Server constructor
   *
   */
  Server::Server() : interface(nullptr), id_pool(N_THREADS) {
    interface = new ControllerType();   // instantiate specific controller implementation
    interface->set_server(this);        // pointer back to this Server instance
  }
  /***** Camera::Server::Server ***********************************************/


  /***** Camera::Server::~Server **********************************************/
  /**
   * @brief      Server destructor
   *
   */
  Server::~Server() {
    delete interface;
  }
  /***** Camera::Server::~Server **********************************************/


  /***** Camera::Server::exit_cleanly *****************************************/
  /**
   * @brief      exit the server
   *
   */
  void Server::exit_cleanly() {
    const std::string function("Camera::Server::exit_cleanly");
    this->interface->disconnect_controller();
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
    this->doit(*sock);
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
  void Server::doit( Network::TcpSocket sock ) {
    const std::string function("Camera::Server::doit");
    std::stringstream message;
    std::string cmd, args;
    bool connection_open=true;
    long ret;

    while (connection_open) {

      // wait (poll) connected socket for incomming data
      //
      int pollret;
      if ( ( pollret=sock.Poll() ) <= 0 ) {
        if (pollret==0) {
          message.str(""); message << "Poll timeout on fd " << sock.getfd() << " thread " << sock.id;
          logwrite(function, message.str());
        }
        if (pollret <0) {
          message.str(""); message << "Poll error on fd " << sock.getfd() << " thread " << sock.id << ": " << strerror(errno);
          logwrite(function, message.str());
        }
        break;                      // this will close the connection
      }

      // Data available, now read from connected socket...
      //
      std::string sbuf;
      char delim='\n';
      if ( ( ret=sock.Read( sbuf, delim ) ) <= 0 ) {
        if (ret<0) {                // could be an actual read error
          message.str(""); message << "Read error on fd " << sock.getfd() << ": " << strerror(errno);
          logwrite(function, message.str());
        }
        if (ret==-2) {
          message.str(""); message << "timeout reading from fd " << sock.getfd();
          logwrite( function, message.str() );
         }
        break;                      // Breaking out of the while loop will close the connection.
                                    // This probably means that the client has terminated abruptly, 
                                    // having sent FIN but not stuck around long enough
                                    // to accept CLOSE and give the LAST_ACK.
      }

      // convert the input buffer into a string and remove any trailing linefeed
      // and carriage return
      //
      sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\r' ), sbuf.end());
      sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\n' ), sbuf.end());

      if (sbuf.empty()) {sock.Write("\n"); continue;}  // acknowledge empty command so client doesn't time out

      try {
        std::size_t cmd_sep = sbuf.find_first_of(" "); // find the first space, which separates command from argument list

        cmd = sbuf.substr(0, cmd_sep);                 // cmd is everything up until that space

        if (cmd.empty()) {sock.Write("\n"); continue;} // acknowledge empty command so client doesn't time out

        if (cmd_sep == std::string::npos) {            // If no space was found,
          args.clear();                                // then the arg list is empty,
        }
        else {
          args= sbuf.substr(cmd_sep+1);                // otherwise args is everything after that space.
        }

        if ( ++this->cmd_num == INT_MAX ) this->cmd_num = 0;

        message.str(""); message << "thread " << sock.id << " received command on fd " << sock.getfd()
                                 << " (" << this->cmd_num << ") : " << cmd << " " << args;
        logwrite(function, message.str());
      }
      catch (const std::exception &e) {
        message.str(""); message << "ERROR parsing command from thread " << sock.id << " fd " << sock.getfd()
                                 << " (" << this->cmd_num << ") \"" << cmd << " " << args << "\" : "
                                 << e.what();
        logwrite(function, message.str());
        cmd = "_EXCEPTION_";         // skips command processing below
      }

      //
      // Process commands here
      //

      ret = NOTHING;
      std::string retstring;

      if ( cmd == "_EXCEPTION_" ) {  // skips checking commands
      }
      else
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
        this->interface->abort(args, retstring);
      }
      else
      if ( cmd == CAMERAD_AUTODIR ) {
        this->interface->autodir(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BASENAME ) {
        this->interface->basename(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BIAS ) {
        this->interface->bias(args, retstring);
      }
      else
      if ( cmd == CAMERAD_BIN ) {
        this->interface->bias(args, retstring);
      }
      else
      if ( cmd == CAMERAD_CLOSE ) {
        this->interface->disconnect_controller(args, retstring);
      }
      else
      if ( cmd == CAMERAD_EXIT ) {
        this->exit_cleanly();
      }
      else
      if ( cmd == CAMERAD_EXPTIME ) {
        this->interface->exptime(args, retstring);
      }
      else
      if ( cmd == CAMERAD_EXPOSE ) {
        this->interface->expose(args, retstring);
      }
      else
      if ( cmd == CAMERAD_LOAD ) {
        this->interface->load_firmware(args, retstring);
      }
      else
      if ( cmd == CAMERAD_OPEN ) {
        this->interface->connect_controller(args, retstring);
      }
      else
      if ( cmd == CAMERAD_NATIVE ) {
        this->interface->native(args, retstring);
      }
      else
      if ( cmd == CAMERAD_POWER ) {
        this->interface->power(args, retstring);
      }
      else
      if ( cmd == CAMERAD_TEST ) {
        this->interface->test(args, retstring);
      }
#ifdef CONTROLLER_ARCHON
      else
      if ( cmd == CAMERAD_LOADTIMING ) {
        dynamic_cast<ArchonInterface*>(interface)->load_timing(args, retstring);
      }
#endif
#ifdef CONTROLLER_BOB
      else
      if ( cmd == "bob" ) {
        dynamic_cast<BobInterface*>(interface)->bob_only();
      }
#endif

      // unknown commands generate an error
      //
      else {
        message.str(""); message << "ERROR unknown command: " << cmd;
        logwrite(function, message.str());
        ret = ERROR;
      }

      // If retstring not empty then append "DONE" or "ERROR" depending on value of ret,
      // and log the reply along with the command number. Write the reply back to the socket.
      //
      // Don't append anything nor log the reply if the command was just requesting help.
      //
      if (ret != NOTHING) {
        if ( ! retstring.empty() ) retstring.append( " " );
        if ( ret != HELP && ret != JSON ) retstring.append( ret == NO_ERROR ? "DONE" : "ERROR" );

        if ( ret == JSON ) {
          message.str(""); message << "command (" << this->cmd_num << ") reply with JSON message";
          logwrite( function, message.str() );
        }
        else
        if ( ! retstring.empty() && ret != HELP ) {
          retstring.append( "\n" );
          message.str(""); message << "command (" << this->cmd_num << ") reply: " << retstring;
          logwrite( function, message.str() );
        }

      if ( sock.Write( retstring ) < 0 ) connection_open=false;
      }

      if (!sock.isblocking()) break;       // Non-blocking connection exits immediately.
                                           // Keep blocking connection open for interactive session.
    }
    return;
  }
  /***** Camera::Server::doit *************************************************/

}
