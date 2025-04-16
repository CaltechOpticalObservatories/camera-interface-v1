#include "astrocam_interface.h"

namespace Camera {

  AstroCamInterface::AstroCamInterface() = default;
  AstroCamInterface::~AstroCamInterface() = default;

  void AstroCamInterface::myfunction() {
    std::string function("Camera::AstroCamInterface::myfunction");
    logwrite(function, "AstroCam implementation of myfunction");
  }

  /***** Camera::AstroCamInterface::connect_controller ************************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::connect_controller( const std::string args, std::string &retstring ) {
    std::string function("Camera::AstroCamInterface::do_connect_controller");
    std::stringstream message;
    long error = NO_ERROR;

    // Help
    //
    if ( args == "?" || args == "help" ) {
      retstring = CAMERAD_OPEN;
      retstring.append( " [ <devnums> ]\n" );
      retstring.append( "  Opens a connection to the indicated PCI/e device(s) where <devnums>\n" );
      retstring.append( "  is an optional space-delimited list of device numbers.\n" );
      retstring.append( "  e.g. \"0 1\" to open PCI devices 0 and 1\n" );
      retstring.append( "  If no list is provided then all detected devices will be opened.\n" );
      retstring.append( "  Opening an ARC device requires that the controller is present and powered on.\n" );
      return HELP;
    }

    // Find the installed devices
    //
    arc::gen3::CArcPCI::findDevices();

    this->numdev = arc::gen3::CArcPCI::deviceCount();

    // Nothing to do if there are no devices detected.
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR no PCI devices found");
      retstring="no_devices";
      return ERROR;
    }

    // Log all PCI devices found
    //
    const std::string* pdevNames;                                             // pointer to device list returned from ARC API
    std::vector<std::string> devNames;                                        // my local copy so I can manipulate it
    pdevNames = arc::gen3::CArcPCI::getDeviceStringList();
    for ( uint32_t i=0; i < arc::gen3::CArcPCI::deviceCount(); i++ ) {
      if ( !pdevNames[i].empty() ) {
        devNames.push_back( pdevNames[i].substr(0, pdevNames[i].size()-1) );  // throw out last character (non-printing)
      }
      message.str(""); message << "found " << devNames.back();
      logwrite(function, message.str());
    }

    // Log PCI devices configured
    //
    if ( this->configured_devnums.empty() ) {
      logwrite( function, "ERROR: no devices configured. Need CONTROLLER keyword in config file." );
      retstring="not_configured";
      return ERROR;
    }

    for ( const auto &dev : this->configured_devnums ) {
      message.str(""); message << "device " << dev << " configured";
      logwrite( function, message.str() );
    }

    // Look at the requested device(s) to open, which are in the
    // space-delimited string args. The devices to open are stored
    // in a public vector "devnums".
    //

    // If no string is given then use vector of configured devices. The configured_devnums
    // vector contains a list of devices defined in the config file with the
    // keyword CONTROLLER=(<PCIDEV> <CHAN> <FT> <FIRMWARE>).
    //
    if ( args.empty() ) this->devnums = this->configured_devnums;
    else {
      // Otherwise, tokenize the device list string and build devnums from the tokens
      //
      this->devnums.clear();                          // empty devnums vector since it's being built here
      std::vector<std::string> tokens;
      Tokenize(args, tokens, " ");
      for ( const auto &n : tokens ) {                // For each token in the args string,
        try {
          int dev = std::stoi( n );                   // convert to int
          if ( std::find( this->devnums.begin(), this->devnums.end(), dev ) == this->devnums.end() ) { // If it's not already in the vector,
            this->devnums.push_back( dev );                                                            // then push into devnums vector.
          }
        }
        catch (const std::exception &e) {
          message.str(""); message << "ERROR parsing device number " << n << ": " << e.what();
          logwrite(function, message.str());
          retstring="invalid_argument";
          return(ERROR);
        }
        catch(...) {
          logwrite(function, "ERROR unknown exception getting device number");
          retstring="exception";
          return ERROR;
        }
      }
    }

    // For each requested dev in devnums, if there is a matching controller in the config file,
    // then get the devname and store it in the controller map.
    //
    for ( const auto &dev : this->devnums ) {
      if ( this->controller.find( dev ) != this->controller.end() ) {
        this->controller[ dev ].devname = devNames[dev];
      }
    }

    // The size of devnums at this point is the number of devices that will
    // be _requested_ to be opened. This should match the number of opened
    // devices at the end of this function.
    //
    size_t requested_device_count = this->devnums.size();

    /*** to be completed ***/

    return( error );
  }
  /***** Camera::AstroCamInterface::connect_controller ************************/


  /***** Camera::AstroCamInterface::disconnect_controller *********************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::disconnect_controller( const std::string args, std::string &retstring ) {
    return ERROR;
  }
  /***** Camera::AstroCamInterface::disconnect_controller *********************/

}

