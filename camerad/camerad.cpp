/**
 * @file    camerad.cpp
 * @brief   this is the main daemon for communicating with the camera
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "camerad.h"

/***** main *******************************************************************/
/**
 * @brief      the main function
 * @param[in]  argc  argument count
 * @param[in]  argv  array holds arguments passed on command line
 * @return     0
 *
 */
int main( int argc, char** argv ) {
  const std::string function("main");

  // Unless specifically requested to run in foreground,
  // immediately daemonize.
  //
  if (!hasOption(argc, argv, "--foreground")) {
    logwrite(function, "starting daemon");
    Daemon::daemonize( "camerad", "/tmp", "/dev/null", "/tmp/camerad.stderr", "", false );
    std::cerr << get_timestamp() << "  (" << function << ") daemonized. child process running" << std::endl;
  }
  else logwrite(function, "starting");

  // the child process instantiates a Server object
  //
  Camera::Server camerad;

  // read the config file and configure the various components
  //
  try {
    camerad.interface->configfile.filename = getOptionArg(argc, argv, "--config");
    camerad.interface->configfile.read_config();
    camerad.configure_server();
    camerad.interface->configure_controller();
    camerad.interface->configure_interface();
    camerad.interface->configure_instrument();
  }
  catch (const std::exception &e) {
    logwrite(function, "ERROR configuring system: "+std::string(e.what()));
    exit(1);
  }

  // dynamically create a new listening socket and thread
  // to handle each connection request
  //
  Network::TcpSocket sock_block(camerad.blkport, true, -1, 0);  // instantiate a TcpSocket with blocking port
  if ( sock_block.Listen() < 0 ) {
    std::cerr << "ERROR could not create listening socket\n";
    exit(1);
  }

  while ( true ) {
    auto newid = camerad.id_pool.get_next_number();  // get next available number from the pool
    {
    std::lock_guard<std::mutex> lock(camerad.sock_block_mutex);
    camerad.socklist[newid] = std::make_shared<Network::TcpSocket>(sock_block);  // create a new socket
    camerad.socklist[newid]->id = newid;             // update the id of the copied socket
    camerad.socklist[newid]->Accept();               // accept connections on the new socket
    }

    // Create a new thread to handle the connection
    //
    std::thread( &Camera::Server::block_main, &camerad, camerad.socklist[newid] ).detach();
  }

  for (;;) pause();                                  // main thread suspends

  return 0;
}

