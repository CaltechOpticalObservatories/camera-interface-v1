#include "astrocam_interface.h"

namespace Camera {

  AstroCamInterface::AstroCamInterface() = default;
  AstroCamInterface::~AstroCamInterface() = default;


  /***** Camera::AstroCamInterface::abort *************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long AstroCamInterface::abort( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::abort");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::AstroCamInterface::abort *************************************/


  /***** Camera::AstroCamInterface::autodir ***********************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR 
   *
   */
  long AstroCamInterface::autodir( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::autodir");
    logwrite(function, "not yet implemented");
    return ERROR;
  } 
  /***** Camera::AstroCamInterface::autodir ***********************************/


  /***** Camera::AstroCamInterface::basename **********************************/
  /**
   * @brief      set or get the image basename
   * @param[in]  args       requested base name
   * @param[out] retstring  current base name
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long AstroCamInterface::basename( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::basename");
    long error=NO_ERROR;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BASENAME;
      retstring.append( " [ <name> ]\n" );
      retstring.append( "  set or get image basename\n" );
      return HELP;
    }

    // Base name cannot contain a "/" because that would be a subdirectory,
    // and subdirectories are not checked here, only by imdir command.
    //
    if ( args.find('/') != std::string::npos ) {
      logwrite( function, "ERROR basename cannot contain '/' character" );
      error = ERROR;
    }
    // if name is supplied the set the image name
    else if ( !args.empty() ) {
      this->camera_info.base_name = args;
      error = NO_ERROR;
    }

    // In any case, log and return the current value.
    //
    logwrite(function, "base name is "+std::string(this->camera_info.base_name));
    retstring = this->camera_info.base_name;

    return error;
  }
  /***** Camera::AstroCamInterface::basename **********************************/


  /***** Camera::AstroCamInterface::bias **************************************/
  /**
   * @brief      set a bias voltage
   * @details    not implemented for AstroCam
   * @param[in]  args       contains <module> <chan> <voltage>
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long AstroCamInterface::bias( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::bias");
    logwrite(function, "ERROR not implemented");
    return ERROR;
  }
  /***** Camera::AstroCamInterface::bias **************************************/


  /***** Camera::AstroCamInterface::bin ***************************************/
  /**
   * @brief      set binning
   * @param[in]  args       
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long AstroCamInterface::bin( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::bin");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::AstroCamInterface::bin ***************************************/


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
    const std::string function("Camera::AstroCamInterface::disconnect_controller");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::disconnect_controller *********************/


  /***** Camera::AstroCamInterface::exptime ***********************************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::exptime( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::exptime");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::exptime ***********************************/


  /***** Camera::AstroCamInterface::expose ************************************/
  /**
   * @brief      
   * @param      
   * @param      
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long AstroCamInterface::expose( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::expose");
    logwrite(function, "not yet implemented");

    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_EXPOSE;
      retstring.append( " <tbd>\n" );
      retstring.append( "  TBD\n" );
      return HELP;
    }
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::expose ************************************/


  /***** Camera::AstroCamInterface::load_firmware *****************************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::load_firmware( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::load_firmware");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::load_firmware *****************************/


  /***** Camera::AstroCamInterface::native ************************************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::native( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::native");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::native ************************************/


  /***** Camera::AstroCamInterface::power *************************************/
  /**
   * @brief
   *
   */
  long AstroCamInterface::power( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::power");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::power *************************************/


  /***** Camera::AstroCamInterface::test **************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long AstroCamInterface::test( const std::string args, std::string &retstring ) {
    const std::string function("Camera::AstroCamInterface::test");

    // initialize the exposure mode to Expose_CCD and call that expose
    //
    logwrite(function, "calling exposure_mode->expose() for Expose_CCD");
    this->exposure_mode = std::make_unique<Expose_CCD>(this);
    if (this->exposure_mode) this->exposure_mode->expose();

    if (!this->exposure_mode) {
      logwrite(function, "ERROR exposure mode undefined!");
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::AstroCamInterface::test **************************************/

}

