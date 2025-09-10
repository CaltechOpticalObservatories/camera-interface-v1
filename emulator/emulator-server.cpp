/**
 * @file    emulator-server.cpp
 * @brief   this is the main server
 * @details spawns threads to handle requests, receives and parses commands
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "emulator-server.h"

//Emulator::Server server;
std::unique_ptr<Emulator::Server> server;

/***** signal_handler *********************************************************/
/**
 * @brief      handles ctrl-C
 * @param[in]  signo
 *
 */
void signal_handler(int signo) {
  std::string function = "(Emulator::signal_handler) ";
  switch (signo) {
    case SIGINT:
      std::cerr << function << "received INT\n";
      server->exit_cleanly();                  // shutdown the server
      break;
    case SIGHUP:
      std::cerr << function << "caught SIGHUP\n";
      server->configure_controller();
      break;
    case SIGPIPE:
      std::cerr << function << "caught SIGPIPE\n";
      break;
    default:
      server->exit_cleanly();                  // shutdown the server
      break;
  }
  return;
}
/***** signal_handler *********************************************************/


int  main(int argc, char **argv);           // main thread (just gets things started)
void block_main(Network::TcpSocket sock);   // this thread handles requests on blocking port
void doit(Network::TcpSocket sock);         // the worker thread


/***** main *******************************************************************/
/**
 * @brief      the main function
 * @param[in]  argc
 * @param[in]  argv
 * @return     0
 *
 */
int main(int argc, char **argv) {
  std::string function = "(Emulator::main) ";
  std::stringstream message;

  signal(SIGINT, signal_handler);
  signal(SIGPIPE, signal_handler);
  signal(SIGHUP, signal_handler);

  std::string instrument;

  if ( cmdOptionExists( argv, argv+argc, "-i" ) ) {
    char* instrument_cstr = getCmdOption( argv, argv+argc, "-i" );
    if ( instrument_cstr != nullptr ) {
      instrument = std::string( instrument_cstr );
      server = std::make_unique<Emulator::Server>(instrument);
    }
    else {
      std::cout << get_timestamp() << function << "ERROR missing or invalid instrument specified with -i <instrument>\n";
      exit(1);
    }
  }
  else {
    std::cout << get_timestamp() << function << "ERROR no instrument specified with -i <instrument>\n";
    exit(1);
  }

  // If an ImageInfo class pointer is not created then the instrument type
  // was wrong.
  //
  if ( server->image == nullptr ) {
    std::cerr << get_timestamp() << function
              << "ERROR could not construct ImageInfo class for instrument " << instrument << ".\n";
    exit(1);
  }
  else {
    std::cout << get_timestamp() << function << "constructed instrument \"" << instrument << "\"\n";
  }

  if ( cmdOptionExists( argv, argv+argc, "-f" ) ) {
    char* filename = getCmdOption( argv, argv+argc, "-f" );
    if ( filename ) {
      server->config.filename = std::string( filename );
    }
  }

  // get the configuration file from the command line
  //
  long ret;
  if (argc>1) {
    server->config.filename = std::string( argv[1] );
    ret = server->config.read_config(server->config);      // read configuration file specified on command line
  }
  else {
    std::cerr << function << "ERROR: no configuration file specified\n";
    server->exit_cleanly();
  }

  std::cerr << function << server->config.n_entries << " lines read from " << server->config.filename << "\n";

  if (ret==NO_ERROR) ret=server->configure_server();      // get needed values out of read-in configuration file for the server

  if (ret==NO_ERROR) ret=server->configure_controller();  // get needed values out of read-in configuration file for the controller

  if (ret != NO_ERROR) {
    std::cerr << function << "ERROR: unable to configure system\n";
    server->exit_cleanly();
  }

  if ( server->emulatorport == -1 ) {
    std::cerr << function << "ERROR: emulator server port not configured\n";
    server->exit_cleanly();
  }

  // create a thread for a single listening port
  // The TcpSocket object is instantiated with (PORT#, BLOCKING_STATE, POLL_TIMEOUT_MSEC, THREAD_ID#)
  //

  Network::TcpSocket s(server->emulatorport, true, -1, 0); // instantiate TcpSocket object
  if ( s.Listen() < 0 ) {                                 // create a listening socket
    std::cerr << function << "ERROR: could not create listening socket on port " << server->emulatorport << "\n";
    server->exit_cleanly();
  }
  std::thread(block_main, s).detach();                    // spawn a thread to handle requests on this socket

  for (;;) pause();                                       // main thread suspends
  return 0;
}
/***** main *******************************************************************/


/***** block_main *************************************************************/
/**
 * @brief      main function for blocking connection thread
 * @param[in]  sock  server listening socket object
 *
 * accepts a socket connection and processes the request by
 * calling function doit()
 *
 * This thread never terminates.
 *
 */
void block_main(Network::TcpSocket sock) {
  while(1) {
    sock.Accept();
    doit(sock);                   // call function to do the work
    sock.Close();
  }
  return;
}
/***** block_main *************************************************************/


/***** doit *******************************************************************/
/**
 * @brief      the workhorse of each thread connetion
 * @details    stays open until closed by client
 * @param[in]  sock  server listening socket object
 *
 */
void doit(Network::TcpSocket sock) {
  std::string function = "(Emulator::doit) ";
  char  buf[Emulator::BUFSIZE+1];
  long  ret;
  std::stringstream message;
  std::stringstream retstream;
  std::string cmd, ref;
  std::vector<std::string> tokens;

  bool connection_open=true;

  while (connection_open) {
    memset(buf,  '\0', Emulator::BUFSIZE);  // init buffers

    retstream.str("");            // empty the return message stream

    // Wait (poll) connected socket for incoming data...
    //
    int pollret;
    if ( ( pollret=sock.Poll() ) <= 0 ) {
      if (pollret==0) {
        std::cerr << function << "Poll timeout on thread " << sock.id << "\n";
      }
      if (pollret <0) {
        std::cerr << function << "Poll error on thread " << sock.id << ": " << strerror(errno) << "\n";
      }
      break;                      // this will close the connection
    }

    // Data available, now read from connected socket...
    //
    if ( ( ret=sock.Read( buf, static_cast<size_t>(Emulator::BUFSIZE) ) ) <= 0 ) {
      if (ret<0) {                // could be an actual read error
        std::cerr << function << "Read error: " << strerror(errno) << "\n";
      }
      break;                      // Breaking out of the while loop will close the connection.
                                  // This probably means that the client has terminated abruptly, 
                                  // having sent FIN but not stuck around long enough
                                  // to accept CLOSE and give the LAST_ACK.
    }

    // convert the input buffer into a string and remove any trailing linefeed
    // and carriage return
    //
    std::string sbuf = buf;
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\r' ), sbuf.end());
    sbuf.erase(std::remove(sbuf.begin(), sbuf.end(), '\n' ), sbuf.end());

    try {
      // ignore empty commands or commands that don't begin with '>'
      //
      if ( ( sbuf.size() == 0 ) || ( (sbuf.size() > 0) && (sbuf.at(0) != '>') ) ) continue;

      // extract the command reference for the checksum
      //
      if ( sbuf.size() >= 3 ) {
        ref = sbuf.substr(1,2);
        cmd = sbuf.substr(3);
      }
    }
    catch ( std::runtime_error &e ) {
      std::stringstream errstream; errstream << e.what();
      std::cerr << function << "error parsing arguments: " << errstream.str() << "\n";
    }
    catch ( ... ) {
      std::cerr << function << "unknown error parsing buffer: " << sbuf << "\n";
    }

    /**
     * process commands here
     * 
     * Most of these don't have to do anything but they're all listed here for completeness,
     * in the order that they appear in the Archon manual.
     *
     * Archon returns "<" plus the message reference to acknowledge the command,
     * or "?" plus reference on error, or nothing when it doesn't undertand something.
     *
     */

#ifdef STA_ARCHON
    if (cmd.compare("SYSTEM")==0) {
                    std::string retstring;
                    ret = server->system_report( cmd, retstring );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref << retstring;
                    }
    else
    if (cmd.compare("STATUS")==0) {
                    std::string retstring;
                    ret = server->status_report( retstring );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref << retstring;
                    }
    else
    if (cmd.compare("TIMER")==0) {
                    std::string retstring;
                    ret = server->timer_report( retstring );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref << "TIMER=" << retstring;
                    }
    else
    if (cmd.compare("FRAME")==0) {
                    std::string retstring;
                    ret = server->frame_report( retstring );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref << retstring;
                    }
    else
    if (cmd.compare("FETCHLOG")==0) {
                    retstream << "<" << ref << "(null)";
                    }
    else
    if (cmd.compare(0,4,"LOCK")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare(0,5,"FETCH")==0) {
                    ret = server->fetch_data( ref, cmd, sock );
                    retstream.str("");      // FETCH returns no message
                    }
    else
    if (cmd.compare(0,7,"WCONFIG")==0) {
                    ret = server->wconfig( cmd );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref;
                    }
    else
    if (cmd.compare(0,7,"RCONFIG")==0) {
                    std::string retstring;
                    ret = server->rconfig( cmd, retstring );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref << retstring;
                    }
    else
    if (cmd.compare("CLEARCONFIG")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("APPLYALL")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("APPLYSYSTEM")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("POWERON")==0) {
                    server->poweron = true;
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("POWEROFF")==0) {
                    server->poweron = false;
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("LOADTIMING")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("LOADPARAMS")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare(0,9,"LOADPARAM")==0) {
                    ret = server->write_parameter( cmd.substr(14) );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref;
                    }
    else
    if (cmd.compare(0,9,"PREPPARAM")==0) {
//                  server->write_parameter( cmd.substr(14) );
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare(0,13,"FASTLOADPARAM")==0) {
                    ret = server->write_parameter( cmd.substr(14) );
                    retstream << ( ret==ERROR ? "?" : "<" ) << ref;
                    }
    else
    if (cmd.compare(0,13,"FASTPREPPARAM")==0) {
//                  server->write_parameter( cmd.substr(14) );
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("RESETTIMING")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("HOLDTIMING")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("RELEASETIMING")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare(0,8,"APPLYMOD")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare(0,8,"APPLYDIO")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("APPLYCDS")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("POLLOFF")==0) {
                    retstream << "<" << ref;
                    }
    else
    if (cmd.compare("POLLON")==0) {
                    retstream << "<" << ref;
                    }
    else {  // Archon simply ignores things it doesn't understand
    }
#endif

    if ( ! retstream.str().empty() ) {
      retstream << "\n";
      if ( (ret=sock.Write(retstream.str())) < 0 ) {
        std::cerr << function << "ret=" << ret << " err=" << std::strerror(errno) << "\n"; connection_open=false;
      }
    }
  }

  std::cerr << function << "socket connection closed\n";

  sock.Close();
  return;
}
/***** doit *******************************************************************/

