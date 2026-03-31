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
  auto filename = getOptionArg(argc, argv, "--config");

  if (filename.empty()) {
    logwrite(function, "ERROR --config <filename> is required");
    exit(1);
  }
  camerad.interface->configfile.filename = filename;

  try {
    camerad.interface->configfile.read_config();
    camerad.configure_server();
    camerad.interface->configure_controller();
    camerad.interface->configure_interface();
    camerad.interface->configure_instrument();
  }
  catch (const std::exception &e) {
    logwrite(function, "ERROR configuring system: "+std::string(e.what()));
    camerad.message_server.stop();
    exit(1);
  }

  for (;;) pause();                                  // main thread suspends

  return 0;
}

