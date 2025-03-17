#include <iostream>
#include "astrocam_interface.h"

namespace AstroCam {

  /***** AstroCam::Interface::publish_snapshot ********************************/
  /**
   * @brief      publishes snapshot of my telemetry
   * @details    This publishes a JSON message containing a snapshot of my
   *             telemetry.
   *
   */
  void Interface::publish_snapshot() {
    std::string dontcare;
    this->publish_snapshot(dontcare);
  }
  void Interface::publish_snapshot(std::string &retstring) {
    nlohmann::json jmessage_out;
    jmessage_out["source"]     = "camerad";

    try {
      this->publisher->publish( jmessage_out );
    }
    catch ( const std::exception &e ) {
      logwrite( "AstroCam::Interface::publish_snapshot",
                "ERROR publishing message: "+std::string(e.what()) );
      return;
    }
  }
  /***** AstroCam::Interface::publish_snapshot ********************************/


  /***** AstroCam::Interface::buffer ******************************************/
  /**
   * @brief      set/get mapped PCI image buffer
   * @details    This function uses the ARC API to allocate PCI/e buffer space
   *             for doing the DMA transfers.
   * @param[in]  args      string containing <dev>|<chan> [ <bytes> | <rows> <cols> ]
   * @param[out] retstring reference to a string for return values
   * @return     ERROR | NO_ERROR | HELP
   *
   * Function returns only ERROR or NO_ERROR on error or success. The 
   * reference to retstring is used to return the size of the allocated
   * buffer.
   *
   * Since this allocates the amount of memory needed for DMA transfer, it
   * must therefore include the total number of pixels, I.E. overscans too.
   *
   */
  long Interface::buffer( std::string args, std::string &retstring ) {
    const std::string function = "AstroCam::Interface::buffer";
    std::stringstream message;
    uint32_t try_bufsize=0;

    // Help
    //
    if ( args == "?" ) {
      retstring = CAMERAD_BUFFER;
      retstring.append( " <chan> | <dev#> [ <bytes> | <rows> <cols> ]\n" );
      retstring.append( "  Allocate PCI buffer space for performing DMA transfers for specified device.\n" );
      retstring.append( "  Provide either a single value in <bytes> or two values as <rows> <cols>.\n" );
      retstring.append( "  If no args supplied then buffer size for dev#|chan is returned (in Bytes).\n" );
      retstring.append( "  Specify <chan> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.channel << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      retstring.append( "       or <dev#> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.devnum << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      return HELP;
    }

    // If no connected devices then nothing to do here
    //
    if ( this->numdev == 0 ) {
      logwrite(function, "ERROR: no connected devices");
      retstring="not_connected";
      return( ERROR );
    }

    int dev;
    std::string chan;

    long error = this->extract_dev_chan( args, dev, chan, retstring );

    if ( chan.empty() ) chan = "(empty)";

    auto it = this->controller.find( dev );

    if ( it == this->controller.end() ) {
      message.str(""); message << "ERROR dev# " << dev << " for chan " << chan << " not configured. "
                               << "Expected <chan> | <dev#> [ <bytes> | <rows> <cols> ]";
      logwrite( function, message.str() );
      retstring="invalid_argument";
      return( ERROR );
    }

    auto &con = it->second;  // need reference so that values can be modified here

    if ( error != NO_ERROR ) return error;

    // If there is a buffer or array size provided then parse it,
    // then try to map the buffer size.
    //
    if ( ! retstring.empty() ) {

      // Still have to tokenize retstring now, to get the requested buffer size.
      //
      // retstring can contain 1 value to allocate that number of bytes
      // retstring can contain 2 values to allocate a buffer of rows x cols
      // Anything other than 1 or 2 tokens is invalid.
      //
      std::vector<std::string> tokens;
      switch( Tokenize( retstring, tokens, " " ) ) {
        case 1:  // allocate specified number of bytes
                 try_bufsize = (uint32_t)parse_val( tokens.at(0) );
                 break;
        case 2:  // allocate (col x row) bytes
                 try_bufsize = (uint32_t)parse_val(tokens.at(0)) * (uint32_t)parse_val(tokens.at(1)) * sizeof(uint16_t);
                 break;
        default: // bad
                 message.str(""); message << "ERROR: invalid arguments: " << retstring << ": expected <bytes> or <rows> <cols>";
                 logwrite(function, message.str());
                 retstring="invalid_argument";
                 return(ERROR);
                 break;
      }

      // Now try to re-map the buffer size for the requested device.
      //
      try {
        con.pArcDev->reMapCommonBuffer( try_bufsize );
      }
      catch( const std::exception &e ) {
        message.str(""); message << "ERROR mapping buffer for dev " << dev << " chan " << chan << ": " << e.what();
        logwrite(function, message.str());
        retstring="arc_exception";
        return( ERROR );
      }
      catch(...) {
        message.str(""); message << "ERROR unknown exception mapping buffer for dev " << dev << " chan " << chan;
        logwrite(function, message.str());
        retstring="exception";
        return( ERROR );
      }

      con.set_bufsize( try_bufsize );  // save the new buffer size to the controller map for this dev
    }  // end if ! retstring.empty()

    retstring = std::to_string( con.get_bufsize() );

    return(NO_ERROR);
  }
  /***** AstroCam::Interface::buffer ******************************************/


/**
  long Interface::connect_controller( std::string args, std::string &retstring ) {
    std::cerr << "AstroCam::Interface::connect_controller here\n";

    arc::gen3::CArcPCI::findDevices();

    this->numdev = arc::gen3::CArcPCI::deviceCount();

    retstring=std::to_string(numdev);

    return NO_ERROR;
  }
**/

  /***** AstroCam::Interface::do_connect_controller ***************************/
  /**
   * @brief      opens a connection to the PCI/e device(s)
   * @param[in]  devices_in  optional string containing space-delimited list of devices
   * @param[out] retstring   reference to string to return error or help
   * @return     ERROR | NO_ERROR | HELP
   *
   * Input parameter devices_in defaults to empty string which will attempt to
   * connect to all detected devices.
   *
   * If devices_in is specified (and not empty) then it must contain a space-delimited
   * list of device numbers to open. A public vector devnums will hold these device
   * numbers. This vector will be updated here to represent only the devices that
   * are actually connected.
   *
   * All devices requested must be connected in order to return success. It is
   * considered an error condition if not all requested can be connected. If the
   * user wishes to connect to only the device(s) available then the user must
   * call with the specific device(s). In other words, it's all (requested) or nothing.
   *
   */
  long Interface::connect_controller( const std::string devices_in, std::string &retstring ) {
    std::string function = "AstroCam::Interface::do_connect_controller";
    std::stringstream message;
    long error = NO_ERROR;

    // Help
    //
    if ( devices_in == "?" || devices_in == "help" ) {
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
      logwrite(function, "ERROR: no devices found");
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
    // space-delimited string devices_in. The devices to open
    // are stored in a public vector "devnums".
    //

    // If no string is given then use vector of configured devices. The configured_devnums
    // vector contains a list of devices defined in the config file with the
    // keyword CONTROLLER=(<PCIDEV> <CHAN> <FT> <FIRMWARE>).
    //
    if ( devices_in.empty() ) this->devnums = this->configured_devnums;
    else {
      // Otherwise, tokenize the device list string and build devnums from the tokens
      //
      this->devnums.clear();                          // empty devnums vector since it's being built here
      std::vector<std::string> tokens;
      Tokenize(devices_in, tokens, " ");
      for ( const auto &n : tokens ) {                // For each token in the devices_in string,
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

    // Open only the devices specified by the devnums vector
    //
    for ( size_t i = 0; i < this->devnums.size(); ) {
      int dev = this->devnums[i];
      auto dev_found = this->controller.find( dev );

      if ( dev_found == this->controller.end() ) {
        message.str(""); message << "ERROR: devnum " << dev << " not found in controller definition. check config file";
        logwrite( function, message.str() );
        this->controller[dev].inactive=true;      // flag the non-connected controller as inactive
        this->disconnect_controller(dev);
        retstring="unknown_device";
        error = ERROR;
        break;
      }
      else this->controller[dev].inactive=false;

      try {
        // Open the PCI device if not already open
        // (otherwise just reset and test connection)
        //
        if ( ! this->controller[dev].connected ) {
          message.str(""); message << "opening " << this->controller[dev].devname;
          logwrite(function, message.str());
          this->controller[dev].pArcDev->open(dev);
        }
        else {
          message.str(""); message << this->controller[dev].devname << " already open";
          logwrite(function, message.str());
        }

        // Reset the PCI device
        //
        message.str(""); message << "resetting " << this->controller[dev].devname;
        logwrite(function, message.str());
        try {
          this->controller[dev].pArcDev->reset();
        }
        catch (const std::exception &e) {
          message.str(""); message << "ERROR resetting " << this->controller[dev].devname << ": " << e.what();
          logwrite(function, message.str());
          error = ERROR;
        }

        // Is Controller Connected?  (tested with a TDL command)
        //
        this->controller[dev].connected = this->controller[dev].pArcDev->isControllerConnected();
        message.str(""); message << this->controller[dev].devname << (this->controller[dev].connected ? "" : " not" ) << " connected to ARC controller"
                                 << (this->controller[dev].connected ? " for channel " : "" )
                                 << (this->controller[dev].connected ? this->controller[dev].channel : "" );
        logwrite(function, message.str());

        // If not connected then this should remove it from the devnums list
        //
        if ( !this->controller[dev].connected ) this->disconnect_controller(dev);

        // Now that controller is open, update it with the current image size
        // that has been stored in the class. Create an arg string in the same
        // format as that found in the config file.
        //
        std::stringstream args;
        std::string retstring;
        args << dev << " "
             << this->controller[dev].detrows << " "
             << this->controller[dev].detcols << " "
             << this->controller[dev].osrows  << " "
             << this->controller[dev].oscols  << " "
             << this->camera_info.binning[Camera::ROW] << " "
             << this->camera_info.binning[Camera::COL];

        // If image_size fails then close only this controller,
        // which allows operating without this one if needed.
        //
        if ( this->image_size( args.str(), retstring ) != NO_ERROR ) {  // set IMAGE_SIZE here after opening
          message.str(""); message << "ERROR setting image size for " << this->controller[dev].devname << ": " << retstring;
//        this->camera.async.enqueue_and_log( function, message.str() );
          this->controller[dev].inactive=true;      // flag the non-connected controller as inactive
          this->disconnect_controller(dev);
          error = ERROR;
        }
      }
      catch ( const std::exception &e ) { // arc::gen3::CArcPCI::open and reset may throw exceptions
        message.str(""); message << "ERROR opening " << this->controller[dev].devname
                                 << " channel " << this->controller[dev].channel << ": " << e.what();
//      this->camera.async.enqueue_and_log( function, message.str() );
        this->controller[dev].inactive=true;      // flag the non-connected controller as inactive
        this->disconnect_controller(dev);
        retstring="exception";
        error = ERROR;
      }
      // A call to disconnect_controller() can modify the size of devnums,
      // so only if the loop index i is still valid with respect to the current
      // size of devnums should it be incremented.
      //
      if ( i < devnums.size() ) ++i;
    }

    // Log the list of connected devices
    //
    message.str(""); message << "connected devices { ";
    for (const auto &devcheck : this->devnums) { message << devcheck << " "; } message << "}";
    logwrite(function, message.str());

    // check the size of the devnums now, against the size requested
    //
    if ( this->devnums.size() != requested_device_count ) {
      message.str(""); message << "ERROR: " << this->devnums.size() <<" connected device(s) but "
                               << requested_device_count << " requested";
      logwrite( function, message.str() );

      // disconnect/deconstruct everything --
      //
      // If the user might want to use what is available then the user
      // must call again, requesting only what is available. It is an
      // error if the interface cannot deliver what was requested.
      //
      this->disconnect_controller();

      retstring="bad_device_count";
      error = ERROR;
    }

    // Start a thread to monitor the state of things (if not already running)
    //
    if ( !this->state_monitor_thread_running.load() ) {
      std::thread( &AstroCam::Interface::state_monitor_thread, this ).detach();
      std::unique_lock<std::mutex> state_lock( this->state_lock );
      if ( !this->state_monitor_condition.wait_for( state_lock,
                                                    std::chrono::milliseconds(1000),
                                                    [this] { return this->state_monitor_thread_running.load(); } ) ) {
        logwrite( function, "ERROR: state_monitor_thread did not start" );
        retstring="internal_error";
        error = ERROR;
      }
    }

    // As the last step to opening the controller, this is where I've chosen
    // to initialize the Shutter class, required before using the shutter.
    //
    if ( this->camera.bonn_shutter ) {
      if ( this->camera.shutter.init() != NO_ERROR ) {
        retstring="shutter_error";
        error = ERROR;
      }
    }

    return( error );
  }
  /***** AstroCam::Interface::do_connect_controller ***************************/


  /***** AstroCam::Interface::do_disconnect_controller ************************/
  /**
   * @brief      closes the connection to the specified PCI/e device
   * @return     ERROR or NO_ERROR
   *
   * This function is overloaded
   *
   */
  long Interface::disconnect_controller( int dev ) {
    std::string function = "AstroCam::Interface::disconnect_controller";
    std::stringstream message;

    if ( !this->is_camera_idle() ) {
      logwrite( function, "ERROR: cannot close controller while camera is active" );
      return ERROR;
    }

    // close indicated PCI device and remove dev from devnums
    //
    try {
      if ( this->controller.at(dev).pArcDev == nullptr ) {
        message.str(""); message << "ERROR no ARC device for dev " << dev;
        logwrite( function, message.str() );
        return ERROR;
      }
      message.str(""); message << "closing " << this->controller.at(dev).devname;
      logwrite(function, message.str());
      this->controller.at(dev).pArcDev->close();  // throws nothing, no error handling
      this->controller.at(dev).connected=false;
      // remove dev from devnums
      //
      auto it = std::find( this->devnums.begin(), this->devnums.end(), dev );
      if ( it != this->devnums.end() ) {
        this->devnums.erase(it);
      }
    }
    catch ( std::out_of_range &e ) {
      message.str(""); message << "dev " << dev << " not found: " << e.what();
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** AstroCam::Interface::do_disconnect_controller ************************/


  /***** AstroCam::Interface::do_disconnect_controller ************************/
  /**
   * @brief      closes the connection to all PCI/e devices
   * @return     ERROR or NO_ERROR
   *
   * no error handling. can only fail if the camera is busy.
   *
   * This function is overloaded
   *
   */
  long Interface::disconnect_controller() {
    std::string function = "AstroCam::Interface::disconnect_controller";
    std::stringstream message;
    long error = NO_ERROR;

    if ( !this->is_camera_idle() ) {
      logwrite( function, "ERROR: cannot close controller while camera is active" );
      return( ERROR );
    }

    // close all of the PCI devices
    //
    for ( auto &con : this->controller ) {
      message.str(""); message << "closing " << con.second.devname;
      logwrite(function, message.str());
      if ( con.second.pArcDev != nullptr ) con.second.pArcDev->close();  // throws nothing
      con.second.connected=false;
    }

    this->devnums.clear();   // no devices open
    this->numdev = 0;        // no devices open
    return error;
  }
  /***** AstroCam::Interface::do_disconnect_controller ************************/


  /***** AstroCam::Interface::extract_dev_chan ********************************/
  /**
   * @brief      extract a dev#, channel name, and optional string from provided args
   * @details    The dev# | chan must exist in a configured controller object.
   *             The additional string is optional and can be anything.
   * @param[in]  args       expected: <dev#>|<chan> [ <string> ]
   * @param[out] dev        reference to returned dev#
   * @param[out] chan       reference to returned channel
   * @param[out] retstring  carries error message, or any remaining strings, if present
   *
   */
  long Interface::extract_dev_chan( std::string args, int &dev, std::string &chan, std::string &retstring ) {
    std::string function = "AstroCam::Interface::extract_dev_chan";
    std::stringstream message;

    // make sure input references are initialized
    //
    dev=-1;
    chan.clear();
    retstring.clear();

    std::vector<std::string> tokens;
    std::string tryme;                  // either a dev or chan, TBD

    // Tokenize args to extract requested <dev>|<chan> and <string>, as appropriate
    //
    Tokenize( args, tokens, " " );
    size_t ntok = tokens.size();
    if ( ntok < 1 ) {                   // need at least one token
      logwrite( function, "ERROR: bad arguments. expected <dev> | <chan> [ <string> ]" );
      retstring="invalid_argument";
      return ERROR;
    }
    else
    if ( ntok == 1 ) {                  // dev|chan only
      tryme = tokens[0];
    }
    else {                              // dev|chan plus string
      tryme = tokens[0];
      retstring.clear();
      for ( size_t i=1; i<ntok; i++ ) {   // If more than one token in string, then put
        retstring.append( tokens[i] );  // them all back together and return as one string.
        retstring.append( ( i+1 < ntok ? " " : "" ) );
      }
    }

    // Try to convert the "tryme" to integer. If successful then it's a dev#,
    // and if that fails then check if it's a known channel.
    //
    try {
      dev = std::stoi( tryme );   // convert to integer
      if ( dev < 0 ) {            // don't let the user input a negative number
        logwrite( function, "ERROR: dev# must be >= 0" );
        retstring="invalid_argument";
        return ERROR;
      }
    }
    catch ( std::out_of_range & ) { logwrite( function, "ERROR: out of range" ); retstring="exception"; return ERROR; }
    catch ( std::invalid_argument & ) { }    // ignore this for now, it just means "tryme" is not a number

    // If the stoi conversion failed then we're out here with a negative dev.
    // If it succeeded then all we have is a number >= 0.
    // But now check that either the dev# is a known devnum or the tryme is a known channel.
    //
    for ( const auto &con : this->controller ) {
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] con.first=" << con.first << " con.second.channel=" << con.second.channel
                               << " .devnum=" << con.second.devnum << " .inactive=" << (con.second.inactive?"T":"F");
      logwrite( function, message.str() );
#endif
      if ( con.second.inactive ) continue;   // skip controllers flagged as inactive
      if ( con.second.channel == tryme ) {   // check to see if it matches a configured channel.
        dev  = con.second.devnum;
        chan = tryme;
        break;
      }
      if ( con.second.devnum == dev ) {      // check for a known dev#
        chan = con.second.channel;
        break;
      }
    }

    // By now, these must both be known.
    //
    if ( dev < 0 || chan.empty() || this->controller.find(dev)==this->controller.end() ) {
      message.str(""); message << "unrecognized channel or device \"" << tryme << "\"";
      logwrite( function, message.str() );
#ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] dev=" << dev << " chan=" << chan << " controller.find(dev)==controller.end ? "
                               << ((this->controller.find(dev)==this->controller.end()) ? "true" : "false" );
      logwrite( function, message.str() );
#endif
      retstring="invalid_argument";
      return( ERROR );
    }

    return( NO_ERROR );
  }
  /***** AstroCam::Interface::extract_dev_chan ********************************/


  /***** AstroCam::Interface::geometry ****************************************/
  /**
   * @brief      set/get geometry
   * @param[in]  args       contains <dev>|<chan> [ <rows> <cols> ]
   * @param[out] retstring  reference to string for return value
   * @return     ERROR | NO_ERROR | HELP
   *
   * The <rows> <cols> set here is the total number of rows and cols that
   * will be read, therefore this includes any pre/overscan pixels.
   *
   */
  long Interface::geometry(std::string args, std::string &retstring) {
    std::string function = "AstroCam::Interface::geometry";
    std::stringstream message;

    // Help
    //
    if ( args == "?" ) {
      retstring = CAMERAD_GEOMETRY;
      retstring.append( " <chan> | <dev#> [ <rows> <cols> ]\n" );
      retstring.append( "  Configures geometry of the detector for the specified device, including\n" );
      retstring.append( "  any overscans. In other words, these are the number of rows and columns that\n" );
      retstring.append( "  will be read out. Camera controller connection must first be open.\n" );
      retstring.append( "  If no args are supplied then the current geometry is returned.\n" );
      retstring.append( "  Specify <chan> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.channel << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      retstring.append( "       or <dev#> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.devnum << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      return HELP;
    }

    // If no connected devices then nothing to do here
    //
    if ( this->numdev == 0 ) {
      logwrite(function, "ERROR: no connected devices");
      retstring="not_connected";
      return( ERROR );
    }

    int dev;
    std::string chan;

    if ( this->extract_dev_chan( args, dev, chan, retstring ) != NO_ERROR ) return ERROR;

    // If geometry was in args then extract_dev_chan() put it in the retstring.
    // If geometry was not supplied then retstring is empty.
    // Tokenize retstring to get the rows and cols. There can be
    // either 2 tokens (set geometry) or 0 tokens (get geometry).
    //
    std::vector<std::string> tokens;
    Tokenize( retstring, tokens, " " );

    if ( tokens.size() == 2 ) {
      int setrows=-1;
      int setcols=-1;
      try {
        setrows = std::stoi( tokens.at( 0 ) );
        setcols = std::stoi( tokens.at( 1 ) );
      }
      catch ( std::exception &e ) {
        message.str(""); message << "ERROR: converting " << args << " to integer: " << e.what();
        logwrite( function, message.str() );
        retstring="invalid_argument";
        return( ERROR );
      }

      if ( setrows < 1 || setcols < 1 ) {
        logwrite( function, "ERROR: rows cols must be > 0" );
        retstring="invalid_argument";
        return( ERROR );
      }

      message.str(""); message << "[DEBUG] " << this->controller[dev].devname
                                             << " chan " << this->controller[dev].channel << " rows:" << setrows << " cols:" << setcols;
      logwrite( function, message.str() );

      // Write the geometry to the selected controllers
      //
      std::stringstream cmd;

      cmd.str(""); cmd << "WRM 0x400001 " << setcols;
      if ( this->native( dev, cmd.str(), retstring ) != NO_ERROR ) return ERROR;

      cmd.str(""); cmd << "WRM 0x400002 " << setrows;
      if ( this->native( dev, cmd.str(), retstring ) != NO_ERROR ) return ERROR;
    }
    else if ( tokens.size() != 0 ) {                 // some other number of args besides 0 or 2 is confusing
      message.str(""); message << "ERROR: expected [ rows cols ] but received \"" << retstring << "\"";
      logwrite( function, message.str() );
      retstring="bad_arguments";
      return( ERROR );
    }

    // Now read back the geometry from each controller
    //
    std::stringstream rs;
    std::stringstream cmd;
    std::string getrows, getcols;

    // Return value from this->native() is of the form "dev:0xVALUE"
    // so must parse the hex value after the colon.
    //
    cmd.str(""); cmd << "RDM 0x400001 ";
    if ( this->native( dev, cmd.str(), getcols ) != NO_ERROR ) return ERROR;
    this->controller[dev].cols = (uint32_t)parse_val( getcols.substr( getcols.find(":")+1 ) );

    cmd.str(""); cmd << "RDM 0x400002 ";
    if ( this->native( dev, cmd.str(), getrows ) != NO_ERROR ) return ERROR;
    this->controller[dev].rows = (uint32_t)parse_val( getrows.substr( getrows.find(":")+1 ) );

    rs << this->controller[dev].rows << " " << this->controller[dev].cols;

    retstring = rs.str();    // Form the return string from the read-back rows cols

    return( NO_ERROR );
  }
  /***** AstroCam::Interface::geometry ****************************************/


  /***** AstroCam::Interface::image_size **************************************/
  /**
   * @brief      set/get image size parameters and allocate PCI memory
   * @details    Sets/gets ROWS COLS OSROWS OSCOLS BINROWS BINCOLS for a
   *             given device|channel. This calls geometry() and buffer() to
   *             set the image geometry on the controller and allocate a PCI buffer.
   * @param[in]  args       contains DEV|CHAN [ [ ROWS COLS OSROWS OSCOLS BINROWS BINCOLS ]
   * @param[out] retstring  reference to string for return value
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::image_size( std::string args, std::string &retstring, const bool save_as_default ) {
    std::string function = "AstroCam::Interface::image_size";
    std::stringstream message;

    // Help
    //
    if ( args == "?" ) {
      retstring = CAMERAD_IMSIZE;
      retstring.append( " <chan> | <dev#> [ [ <rows> <cols> <osrows> <oscols> <binrows> <bincols> ]\n" );
      retstring.append( "  Configures image parameters used to set image size in the controller,\n" );
      retstring.append( "  allocate needed PCI buffer space and for FITS header keywords.\n" );
      retstring.append( "  <bin____> represents the binning factor for each axis.\n" );
      retstring.append( "  Camera controller connection must first be open.\n" );
      retstring.append( "  If no args are supplied then the current parameters for dev|chan are returned.\n" );
      retstring.append( "  Specify <chan> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.channel << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      retstring.append( "       or <dev#> from { " );
      message.str("");
      for ( const auto &con : this->controller ) {
        if ( con.second.inactive ) continue;  // skip controllers flagged as inactive
        message << con.second.devnum << " ";
      }
      message << "}\n";
      retstring.append( message.str() );
      return HELP;
    }

    // Don't allow any changes while any exposure activity is running.
    //
    if ( !this->is_camera_idle() ) {
      logwrite( function, "ERROR: all exposure activity must be stopped before changing image parameters" );
      retstring="camera_busy";
      return( ERROR );
    }

    // Get the requested dev# and channel from supplied args.
    // After calling extract_dev_chan(), dev can be trusted as
    // a valid index without needing a try/catch.
    //
    int dev=-1;
    std::string chan;
    if ( this->extract_dev_chan( args, dev, chan, retstring ) != NO_ERROR ) return ERROR;

    // retstring now should contain [ ROWS COLS OSROWS OSCOLS BINROWS BINCOLS ]
    // It can contain 0 or 6 tokens.
    //
    std::vector<std::string> tokens;
    Tokenize( retstring, tokens, " " );

    if ( ! tokens.empty() ) {

      // Having other than 6 tokens is automatic disqualification so get out now
      //
      if ( tokens.size() != 6 ) {
        message.str(""); message << "ERROR: invalid arguments: " << retstring << ": expected <rows> <cols> <osrows> <oscols> <binrows> <bincols>";
        logwrite( function, message.str() );
        retstring="invalid_argument";
        return( ERROR );
      }

      int rows=-1, cols=-1, osrows=-1, oscols=-1, binrows=-1, bincols=-1;

      try {
        rows    = std::stoi( tokens.at(0) );
        cols    = std::stoi( tokens.at(1) );
        osrows  = std::stoi( tokens.at(2) );
        oscols  = std::stoi( tokens.at(3) );
        binrows = std::stoi( tokens.at(4) );
        bincols = std::stoi( tokens.at(5) );
      }
      catch ( std::exception &e ) {
        message.str(""); message << "ERROR: exception parsing \"" << retstring << "\": " << e.what();
        logwrite( function, message.str() );
        retstring="invalid_argument";
        return( ERROR );
      }

      // Check image size
      //
      if ( rows<1 || cols<1 || osrows<0 || oscols<0 || binrows<1 || bincols<1 ) {
        message.str(""); message << "ERROR: invalid image size " << rows << " " << cols << " "
                                 << osrows << " " << oscols << " " << binrows << " " << bincols;
        logwrite( function, message.str() );
        retstring="invalid_argument";
        return( ERROR );
      }

      message.str(""); message << "[DEBUG] imsize: " << rows << " " << cols << " "
                               << osrows << " " << oscols << " " << binrows << " " << bincols;
      logwrite( function, message.str() );

      // Store the geometry in the class. This is the new detector geometry,
      // unchanged by binning, so that when reverting to binning=1 from some
      // binnnig factor, this is the default image size to revert to.
      //
      this->controller[dev].detrows = rows;
      this->controller[dev].detcols = cols;
      this->controller[dev].osrows0 = osrows;
      this->controller[dev].oscols0 = oscols;

      // Binning is the same for all devices so it's stored in the camera info class.
      //
      this->camera_info.binning[Camera::ROW] = binrows;
      this->camera_info.binning[Camera::COL] = bincols;

      // Assign the binning also to the controller info
      //
      this->controller[dev].info.binning[Camera::ROW] = this->camera_info.binning[Camera::ROW];
      this->controller[dev].info.binning[Camera::COL] = this->camera_info.binning[Camera::COL];

      // If binned by a non-evenly-divisible factor then skip that
      // many at the start. These will be removed from the image.
      //
      this->controller[dev].skipcols = cols % bincols;
      this->controller[dev].skiprows = rows % binrows;

      cols -= this->controller[dev].skipcols;
      rows -= this->controller[dev].skiprows;

      // Adjust the number of overscans to make them evenly divisible
      // by the binning factor.
      //
      oscols -= ( oscols % bincols );
      osrows -= ( osrows % binrows );

      // Now that the rows/cols and osrows/oscols have been adjusted for
      // binning, store them in the class as detector_pixels for this controller.
      //
      this->controller[dev].info.detector_pixels[Camera::COL] = cols + oscols;
      this->controller[dev].info.detector_pixels[Camera::ROW] = rows + osrows;

      // ROI is the full detector
      this->controller[dev].info.region_of_interest[0] = 1;
      this->controller[dev].info.region_of_interest[1] = this->controller[dev].info.detector_pixels[0];
      this->controller[dev].info.region_of_interest[2] = 1;
      this->controller[dev].info.region_of_interest[3] = this->controller[dev].info.detector_pixels[1];

      this->controller[dev].info.ismex = true;
      this->controller[dev].info.bitpix = 16;
      this->controller[dev].info.frame_type = Camera::FRAME_RAW;

      // A call to set_axes() will store the binned image dimensions
      // in controller[dev].info.axes[*]. Those dimensions will
      // be used to set the image geometry with geometry() and
      // the PCI buffer with buffer().
      //
      if ( this->controller[dev].info.set_axes() != NO_ERROR ) {
        message.str(""); message << "ERROR setting axes for device " << dev;
//      this->camera.async.enqueue_and_log( "CAMERAD", function, message.str() );
        return( ERROR );
      }

      // if requested, store the imsize values as a string in the class for default recovery
      //
      if ( save_as_default ) {
        message.str(""); message << rows << " " << cols << " " << osrows << " " << oscols << " " << binrows << " " << bincols;
        this->controller[dev].imsize_args = message.str();
        logwrite( function, "saved as default for chan "+chan+": "+message.str() );
      }

      // This is as far as we can get without a connected controller.
      // If not connected then all we've done is save this info to the class,
      // which will be used after the controller is connected.
      //
      if ( this->controller[dev].connected ) {

        // because set_axes() doesn't scale overscan
        //
        oscols /= bincols;
        osrows /= binrows;

        // save the binning-adjusted overscans to the class
        //
        this->controller[dev].osrows = osrows;
        this->controller[dev].oscols = oscols;

        // allocate PCI buffer and set geometry now
        //
        std::stringstream geostring;
        std::string retstring;
        geostring << dev << " "
                  << this->controller[dev].info.axes[Camera::ROW] << " "
                  << this->controller[dev].info.axes[Camera::COL];

        if ( this->buffer( geostring.str(), retstring ) != NO_ERROR ) {
          message.str(""); message << "ERROR: allocating buffer for chan " << this->controller[dev].channel
                                   << " " << this->controller[dev].devname;
          logwrite( function, message.str() );
          return ERROR;
        }

        if ( this->geometry( geostring.str(), retstring ) != NO_ERROR ) {
          message.str(""); message << "ERROR: setting geometry for chan " << this->controller[dev].channel;
          logwrite( function, message.str() );
          return ERROR;
        }

        // Set the Bin Parameters to the controller using the 3-letter command,
        // "SBP <NPBIN> <NP_SKIP> <NSBIN> <NS_SKIP>"
        //
        std::stringstream cmd;
        cmd << "SBP "
            << this->camera_info.binning[Camera::ROW] << " "
            << this->controller[dev].skiprows << " "
            << this->camera_info.binning[Camera::COL] << " "
            << this->controller[dev].skipcols;
        if ( this->native( dev, cmd.str(), retstring ) != NO_ERROR ) return ERROR;

/**********
        // Add image size related keys specific to this controller in the controller's extension
        //
        this->controller[dev].info.systemkeys.add_key( "IMG_ROWS", this->controller[dev].info.axes[Camera::ROW], "image rows", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "IMG_COLS", this->controller[dev].info.axes[Camera::COL], "image cols", EXT, chan );

        this->controller[dev].info.systemkeys.add_key( "OS_ROWS", osrows, "overscan rows", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "OS_COLS", oscols, "overscan cols", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "SKIPROWS", skiprows, "skipped rows", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "SKIPCOLS", skipcols, "skipped cols", EXT, chan );

        int L=0, B=0;
        switch ( this->controller[ dev ].info.readout_type ) {
          case L1:     B=1; break;
          case U1:     L=1; B=1; break;
          case U2:     L=1; break;
          case SPLIT1: B=1; break;
          case FT1:    B=1; break;
          default:     L=0; B=0;
        }

        int ltv2 = B * osrows / this->camera_info.binning[Camera::ROW];
        int ltv1 = L * oscols / this->camera_info.binning[Camera::COL];

message.str(""); message << "[DEBUG] B=" << B << " L=" << L << " osrows=" << osrows << " oscols=" << oscols
                         << " binning_row=" << this->camera_info.binning[Camera::ROW] << " binning_col=" << this->camera_info.binning[Camera::COL]
                         << " ltv2=" << ltv2 << " ltv1=" << ltv1;
logwrite(function,message.str() );

        this->controller[dev].info.systemkeys.add_key( "LTV2", ltv2, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "LTV1", ltv1, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "CRPIX1A", ltv1+1, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "CRPIX2A", ltv2+1, "", EXT, chan );

        this->controller[dev].info.systemkeys.add_key( "BINSPEC", this->camera_info.binning[Camera::COL], "binning in spectral direction", EXT, chan );  // TODO
        this->controller[dev].info.systemkeys.add_key( "BINSPAT", this->camera_info.binning[Camera::ROW], "binning in spatial direction", EXT, chan );  // TODO

        this->controller[dev].info.systemkeys.add_key( "CDELT1A", 
                                                       this->controller[dev].info.dispersion*this->camera_info.binning[Camera::ROW],
                                                       "Dispersion in Angstrom/pixel", EXT, this->controller[dev].channel );
        this->controller[dev].info.systemkeys.add_key( "CRVAL1A", 
                                                       this->controller[dev].info.minwavel,
                                                       "Reference value in Angstrom", EXT, this->controller[dev].channel );

        // These keys are for proper mosaic display.
        // Adjust GAPY to taste.
        //
        int GAPY=20;
        int channeldevnum = this->controller[dev].devnum;
        int crval2 = ( this->controller[dev].info.axes[Camera::ROW] / this->camera_info.binning[Camera::ROW] + GAPY ) * channeldevnum;

        this->controller[dev].info.systemkeys.add_key( "CRPIX1", 0, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "CRPIX2", 0, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "CRVAL1", 0, "", EXT, chan );
        this->controller[dev].info.systemkeys.add_key( "CRVAL2", crval2, "", EXT, chan );

        // Add ___SEC keywords to the extension header for this channel
        //
        std::stringstream sec;

        sec.str(""); sec << "[" << this->controller[dev].info.region_of_interest[0] << ":" << this->controller[dev].info.region_of_interest[1]
                         << "," << this->controller[dev].info.region_of_interest[2] << ":" << this->controller[dev].info.region_of_interest[3] << "]";
        this->controller[dev].info.systemkeys.add_key( "CCDSEC", sec.str(), "physical format of CCD", EXT, chan );

        sec.str(""); sec << "[" << this->controller[dev].info.region_of_interest[0] + skipcols << ":" << cols
                         << "," << this->controller[dev].info.region_of_interest[2] + skiprows << ":" << rows << "]";
        this->controller[dev].info.systemkeys.add_key( "DATASEC", sec.str(), "section containing the CCD data", EXT, chan );

        sec.str(""); sec << '[' << cols << ":" << cols+oscols
                         << "," << this->controller[dev].info.region_of_interest[2] + skiprows << ":" << rows+osrows << "]";
        this->controller[dev].info.systemkeys.add_key( "BIASSEC", sec.str(), "overscan section", EXT, chan );
**********/
      }
      else {
        message.str(""); message << "saved but not sent to controller because chan " << this->controller[dev].channel << " is not connected";
        logwrite( function, message.str() );
      }

    }  // end if !tokens.empty()

    // Return the values stored in the class
    //
    message.str("");
    message << this->controller[dev].detrows << " " << this->controller[dev].detcols << " "
            << this->controller[dev].osrows << " " << this->controller[dev].oscols << " "
            << this->camera_info.binning[Camera::ROW] << " " << this->camera_info.binning[Camera::COL]
            << ( this->controller[dev].connected ? "" : " [inactive]" );
    logwrite( function, message.str() );
    retstring = message.str();

    return( NO_ERROR );
  }
  /***** AstroCam::Interface::image_size **************************************/


  /***** AstroCam::Interface::native ******************************************/
  /**
   * @brief      send a 3-letter command to specified or all open Leach controllers
   * @details    See details in overloaded native(std::string args, std::string &retstring)
   * @param[in]  args  string containing optional DEV|CHAN plus command and arguments
   * @return     NO_ERROR on success, ERROR on error
   *
   */
  long Interface::native( std::string args ) {
    std::string dontcare;
    return this->native( args, dontcare );
  }
  /***** AstroCam::Interface::native ******************************************/
  /**
   * @brief      send a 3-letter command to specified or all open Leach controllers
   * @details    The input args are checked for the presence of a recognized
   *             channel name or device number. If one is present then the
   *             remaining part of args is sent to that specified controller only,
   *             otherwise the entire args string is sent to all open controllers.
   * @param[in]  args       string containing optional DEV|CHAN plus command and arguments
   * @param[out] retstring  return string
   * @return     NO_ERROR on success, ERROR on error
   *
   * args can be <CMD>
   *             <CMD> <ARG> ...
   *             <DEV|CHAN> <CMD>
   *             <DEV|CHAN> <CMD> <ARG> ...
   */
  long Interface::native( std::string args, std::string &retstring ) {
    // Try to get the requested dev# and channel from supplied args.
    // If extract_dev_chan() returns NO_ERROR, dev can be trusted as
    // a valid index without needing a try/catch and cmdstr will hold
    // the full command (args minus the dev/chan).
    //
    int dev=-1;
    std::string chan, cmdstr;
    if (this->extract_dev_chan( args, dev, chan, cmdstr )==NO_ERROR) {
      // found a dev so send native command to this dev only
      std::string dontcare;
      return this->native( dev, cmdstr, dontcare );
    }
    else {
      // didn't find a dev in args so build vector of all open controllers
      std::vector<uint32_t> selectdev;
      for ( const auto &dev : this->devnums ) {
        if ( this->controller[dev].connected ) selectdev.push_back( dev );
      }
      // this will send the native command to all controllers in that vector
      return this->native( selectdev, args, retstring );
    }
  }
  /***** AstroCam::Interface::native ******************************************/


  /***** AstroCam::Interface::native ******************************************/
  /**
   * @brief      send a 3-letter command to Leach controllers specified by vector
   * @param[in]  selectdev  vector of devices to use
   * @param[in]  cmdstr     string containing command and arguments
   * @return     NO_ERROR on success, ERROR on error
   *
   */
  long Interface::native(std::vector<uint32_t> selectdev, std::string cmdstr) {
    // Use the erase-remove idiom to remove disconnected devices from selectdev
    //
    selectdev.erase( std::remove_if( selectdev.begin(), selectdev.end(),
                     [this](uint32_t dev) { return !this->controller[dev].connected; } ),
                     selectdev.end() );

    std::string retstring;
    return this->native( selectdev, cmdstr, retstring );
  }
  /***** AstroCam::Interface::native ******************************************/


  /***** AstroCam::Interface::native ******************************************/
  /**
   * @brief      send a 3-letter command to individual Leach controller by devnum
   * @param[in]  dev        individual device to use
   * @param[in]  cmdstr     string containing command and arguments
   * @param[out] retstring  reference to string to contain reply
   * @return     NO_ERROR on success, ERROR on error
   *
   */
  long Interface::native( int dev, std::string cmdstr, std::string &retstring ) {
    std::vector<uint32_t> selectdev;
    if ( this->controller[dev].connected ) selectdev.push_back( dev );
    return this->native( selectdev, cmdstr, retstring );
  }
  /***** AstroCam::Interface::native ******************************************/


  /***** AstroCam::Interface::native ******************************************/
  /**
   * @brief      send a 3-letter command to Leach controllers specified by vector
   * @param[in]  selectdev  vector of devices to use
   * @param[in]  cmdstr     string containing command and arguments
   * @param[out] retstring  reference to string to contain reply
   * @return     NO_ERROR | ERROR | HELP
   *
   */
  long Interface::native( std::vector<uint32_t> selectdev, std::string cmdstr, std::string &retstring ) {
    std::string function = "AstroCam::Interface::native";
    std::stringstream message;
    std::vector<std::string> tokens;

    // If no connected devices then nothing to do here
    //
    if (this->numdev == 0) {
      logwrite(function, "ERROR: no connected devices");
      retstring="not_connected";
      return(ERROR);
    }

    if ( cmdstr.empty() ) {
      logwrite(function, "ERROR: missing command");
      retstring="invalid_argument";
      return(ERROR);
    }

    // If no command passed then nothing to do here
    //
    if ( cmdstr == "?" ) {
      retstring = CAMERAD_NATIVE;
      retstring.append( " <CMD> [ <ARG1> [ < ARG2> [ <ARG3> [ <ARG4> ] ] ] ]\n" );
      retstring.append( "  send 3-letter command <CMD> with up to four optional args to all open ARC controllers\n" );
      retstring.append( "  Input <CMD> is not case-sensitive and any values default to base-10\n" );
      retstring.append( "  unless preceeded by 0x to indicate base-16 (e.g. rdm 0x400001).\n" );
      return HELP;
    }

    // Ensure command and args are uppercase
    //
    try {
      std::transform( cmdstr.begin(), cmdstr.end(), cmdstr.begin(), ::toupper );
    }
    catch (...) {
      logwrite(function, "ERROR: converting command to uppercase");
      return( ERROR );
    }

    std::vector<uint32_t> cmd;      // this vector will contain the cmd and any arguments
    uint32_t c0, c1, c2;

    Tokenize(cmdstr, tokens, " ");

    int nargs = tokens.size() - 1;  // one token is for the command, this is number of args

    // max 4 arguments
    //
    if (nargs > 4) {
      message.str(""); message << "ERROR: too many arguments: " << nargs << " (max 4)";
      logwrite(function, message.str());
      retstring="invalid_argument";
      return(ERROR);
    }

    try {
      // first token is command and require a 3-letter command
      //
      if (tokens.at(0).length() != 3) {
        message.str(""); message << "ERROR: bad command " << tokens.at(0) << ": native command requires 3 letters";
        logwrite(function, message.str());
        retstring="bad_command";
        return(ERROR);
      }

      // Change the 3-letter (ASCII) command into hex byte representation
      // and push it into cmd vector.
      //
      c0  = (uint32_t)tokens.at(0).at(0); c0 = c0 << 16;
      c1  = (uint32_t)tokens.at(0).at(1); c1 = c1 <<  8;
      c2  = (uint32_t)tokens.at(0).at(2);

      cmd.push_back( c0 | c1 | c2 );

      // convert each arg into a number and push into cmd vector
      //
      tokens.erase( tokens.begin() );                 // first, erase the command from the tokens
      for ( const auto &tok : tokens ) {
        cmd.push_back( (uint32_t)parse_val( tok ) );  // then push each converted arg into cmd vector
      }

      // Log the complete command (with arg list) that will be sent
      //
      message.str("");   message << "sending command:"
                                 << std::setfill('0') << std::setw(2) << std::hex << std::uppercase;
      for (const auto &arg : cmd) message << " 0x" << arg;
      logwrite(function, message.str());
    }
    catch(std::out_of_range &) {  // should be impossible but here for safety
      message.str(""); message << "ERROR: unable to parse command ";
      for (const auto &check : tokens) message << check << " ";
      message << ": out of range";
      logwrite(function, message.str());
      retstring="out_of_range";
      return(ERROR);
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR forming command: " << e.what();
      logwrite(function, message.str());
      retstring="exception";
      return(ERROR);
    }
    catch(...) { logwrite(function, "unknown error forming command"); retstring="exception"; return(ERROR); }

    // Send the command to each selected device via a separate thread
    //
    {                                       // start local scope for this stuff
    std::vector<std::thread> threads;       // local scope vector stores all of the threads created here
    for ( const auto &dev : selectdev ) {   // spawn a thread for each device in selectdev
      std::thread thr( std::ref(AstroCam::Interface::dothread_native), std::ref(this->controller[dev]), cmd );
      threads.push_back(std::move(thr));    // push the thread into a vector
    }

    try {
      for (std::thread & thr : threads) {   // loop through the vector of threads
        if ( thr.joinable() ) thr.join();   // if thread object is joinable then join to this function. (not to each other)
      }
    }
    catch(const std::exception &e) {
      message.str(""); message << "ERROR joining threads: " << e.what();
      logwrite(function, message.str());
      retstring="exception";
      return(ERROR);
    }
    catch(...) { logwrite(function, "unknown error joining threads"); retstring="exception"; return(ERROR); }
    }                                       // end local scope cleans up this stuff

    // Check to see if all retvals are the same by comparing them all to the first.
    //
    std::uint32_t check_retval;
    try {
      check_retval = this->controller[selectdev.at(0)].retval;    // save the first one in the controller vector
    }
    catch(std::out_of_range &) {
      logwrite(function, "ERROR: no device found. Is the controller connected?");
      retstring="out_of_range";
      return(ERROR);
    }

    bool allsame = true;
    for ( const auto &dev : selectdev ) { if (this->controller[dev].retval != check_retval) { allsame = false; } }

    // If all the return values are equal then return only one value...
    //
    if ( allsame ) {
      this->retval_to_string( check_retval, retstring );        // this sets retstring = to_string( retval )
    }

    // ...but if any retval is different from another then we have to report each one.
    //
    else {
      std::stringstream rs;
      for ( const auto &dev : selectdev ) {
        this->retval_to_string( this->controller[dev].retval, retstring );          // this sets retstring = to_string( retval )
        rs << std::dec << this->controller[dev].devnum << ":" << retstring << " ";  // build up a stringstream of each controller's reply
      }
      retstring = rs.str();  // re-use retstring to contain all of the replies
    }

    // Log the return values
    ///< TODO @todo need a way to send these back to the calling function
    //
    for ( const auto &dev : selectdev ) {
      message.str(""); message << this->controller[dev].devname << " returns " << std::dec << this->controller[dev].retval
                               << " (0x" << std::hex << std::uppercase << this->controller[dev].retval << ")";
      logwrite(function, message.str());
    }
    return ( NO_ERROR );
  }
  /***** AstroCam::Interface::native ******************************************/


  /***** AstroCam::Interface::retval_to_string ********************************/
  /**
   * @brief      private function to convert ARC return values (long) to string
   * @param[in]  retval     integer return value from controller
   * @param[out] retstring  reference to a string for return value in ASCII
   *
   * In addition to converting longs to string, if the retval is a particular
   * commonly used code, then set the ASCII characters for those codes,
   * such as DON, ERR, etc. instead of returning a string value of their
   * numeric counterpart.
   *
   */
  void Interface::retval_to_string( std::uint32_t retval, std::string& retstring ) {
    // replace some common values with their ASCII equivalents
    //
    if ( retval == 0x00455252 ) { retstring = "ERR";  }
    else
    if ( retval == 0x00444F4E ) { retstring = "DON";  }
    else
    if ( retval == 0x544F5554 ) { retstring = "TOUT"; }
    else
    if ( retval == 0x524F5554 ) { retstring = "ROUT"; }
    else
    if ( retval == 0x48455252 ) { retstring = "HERR"; }
    else
    if ( retval == 0x00535952 ) { retstring = "SYR";  }
    else
    if ( retval == 0x00525354 ) { retstring = "RST";  }
    else
    if ( retval == 0x00434E52 ) { retstring = "CNR";  }

    // otherwise just convert the numerical value to a string represented in hex
    //
    else {
      std::stringstream rs;
      rs << "0x" << std::hex << std::uppercase << retval;
      retstring = rs.str();
    }
    return;
  }
  /***** AstroCam::Interface::retval_to_string ********************************/


  /***** AstroCam::Interface::dothread_native *********************************/
  /**
   * @brief      run in a thread to actually send the command
   * @param[in]  con  reference to Controller object
   * @param[in]  cmd  vector containing command and args
   *
   */
  void Interface::dothread_native( Controller &con, std::vector<uint32_t> cmd ) {
    std::string function = "AstroCam::Interface::dothread_native";
    std::stringstream message;
    uint32_t command;

    try {
      if ( cmd.size() > 0 ) command = cmd.at(0);

      // ARC_API now uses an initialized_list object for the TIM_ID, command, and arguments.
      // The list object must be instantiated with a fixed size at compile time.
      //
      if (cmd.size() == 1) con.retval = con.pArcDev->command( { TIM_ID, cmd.at(0) } );
      else
      if (cmd.size() == 2) con.retval = con.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1) } );
      else
      if (cmd.size() == 3) con.retval = con.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2) } );
      else
      if (cmd.size() == 4) con.retval = con.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2), cmd.at(3) } );
      else
      if (cmd.size() == 5) con.retval = con.pArcDev->command( { TIM_ID, cmd.at(0), cmd.at(1), cmd.at(2), cmd.at(3), cmd.at(4) } );
      else {
        message.str(""); message << "ERROR: invalid number of command arguments: " << cmd.size() << " (expecting 1,2,3,4,5)";
        logwrite(function, message.str());
        con.retval = 0x455252;
      }
    }
    catch(const std::runtime_error &e) {
      message.str(""); message << "ERROR sending 0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
                                                     << command << " to " << con.devname << ": " << e.what();
      logwrite(function, message.str());
      con.retval = 0x455252;
      return;
    }
    catch(std::out_of_range &) {  // impossible
      logwrite(function, "ERROR: indexing command argument");
      con.retval = 0x455252;
      return;
    }
    catch(...) {
      message.str(""); message << "unknown error sending 0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
                                                             << command << " to " << con.devname;
      logwrite(function, message.str());
      con.retval = 0x455252;
      return;
    }
    return;
  }
  /***** AstroCam::Interface::dothread_native *********************************/


  /***** AstroCam::Interface::state_monitor_thread ****************************/
  void Interface::state_monitor_thread() {
    const std::string function = "AstroCam::Interface::state_monitor_thread";
    std::stringstream message;
    std::vector<uint32_t> selectdev;

    // notify that the thread is running
    //
    logwrite(function, "starting");
    {
      std::unique_lock<std::mutex> state_lock(this->state_lock);
      this->state_monitor_thread_running = true;
    }
    this->state_monitor_condition.notify_all();
    logwrite(function, "running");

    while ( true ) {
      std::unique_lock<std::mutex> state_lock(this->state_lock);
      this->state_monitor_condition.wait(state_lock);

      while ( this->is_camera_idle() ) {
        selectdev.clear();
        message.str(""); message << "enabling detector idling for channel(s)";
        for ( const auto &dev : this->devnums ) {
          logwrite(function, std::to_string(dev));
          if ( this->controller[dev].connected ) {
            selectdev.push_back(dev);
            message << " " << this->controller[dev].channel;
          }
        }
        if ( selectdev.size() > 0 ) {
          long ret = this->native( selectdev, std::string("IDL") );
          logwrite( function, (ret==NO_ERROR ? "NOTICE: " : "ERROR")+message.str() );
        }
        // Wait for the conditions to change before checking again
        this->state_monitor_condition.wait( state_lock );
      }
    }

    // notify that the thread is terminating
    // should be impossible
    //
    {
      std::unique_lock<std::mutex> state_lock(this->state_lock);
      this->state_monitor_thread_running = false;
    }
    this->state_monitor_condition.notify_all();
    logwrite(function, "terminating");

    return;
  }
  /***** AstroCam::Interface::state_monitor_thread ****************************/
}
