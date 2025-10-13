/**
 * @file    archon_interface.cpp
 * @brief   implementation of Archon Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_interface.h"
#include "archon_controller.h"

namespace Camera {

  ArchonInterface::ArchonInterface() :
    controller(set_controller(std::make_unique<ArchonController>())) {
    controller.set_interface(this);
  }
  ArchonInterface::~ArchonInterface() {
  }


  /***** Camera::ArchonInterface::abort ***************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::abort( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::abort");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::abort ***************************************/


  /***** Camera::ArchonInterface::autodir *************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::autodir( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::autodir");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::autodir *************************************/


  /***** Camera::ArchonInterface::basename ************************************/
  /**
   * @brief      set or get the image basename
   * @param[in]  args       requested base name
   * @param[out] retstring  current base name
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::basename( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::basename");
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
  /***** Camera::ArchonInterface::basename ************************************/


  /***** Camera::ArchonInterface::bias ****************************************/
  /**
   * @brief      set a bias voltage
   * @param[in]  args       contains <module> <chan> <voltage>
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::bias( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bias");
    std::stringstream message;
    std::vector<std::string> tokens;
    std::stringstream biasconfig;
    int module;
    int channel;
    float voltage;
    float vmin, vmax;
    bool readonly=true;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BIAS;
      retstring.append( " <module> <chan> <voltage>\n" );
      retstring.append( "  set a bias voltage\n" );
      return HELP;
    }

    // must have loaded firmware
    //
    if ( ! this->controller.is_firmwareloaded ) {
      logwrite( function, "ERROR firmware not loaded" );
      return ERROR;
    }

    Tokenize(args, tokens, " ");

    if (tokens.size() == 2) {
      readonly = true;

    } else if (tokens.size() == 3) {
      readonly = false;

    } else {
      message.str(""); message << "incorrect number of arguments: " << args << ": expected module channel [voltage]";
      logwrite( function, message.str() );
      return ERROR;
    }

    try {
      module  = std::stoi( tokens[0] );
      channel = std::stoi( tokens[1] );
      if (!readonly) voltage = std::stof( tokens[2] );
    }
    catch (const std::exception &e) {
      message.str(""); message << "ERROR parsing \"" << args << "\": expected <module> <channel> [ voltage ]: "
                               << e.what();
      logwrite( function, message.str() );
      return ERROR;

    }

    // Check that the module number is valid
    //
    if ( (module < 0) || (module > NMODS) ) {
      message.str(""); message << "module " << module << ": outside range {0:" << NMODS << "}";
      logwrite( function, message.str() );
      return ERROR;
    }

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    //
    switch ( this->controller.modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        logwrite( function, message.str() );
        return ERROR;
        break;
      case 3:  // LVBias
      case 9:  // LVXBias
        biasconfig << "MOD" << module << "/LV";
        vmin = -14.0;
        vmax = +14.0;
        break;
      case 4:  // HVBias
      case 8:  // HVXBias
        biasconfig << "MOD" << module << "/HV";
        vmin =   0.0;
        vmax = +31.0;
        break;
      default:
        message.str(""); message << "module " << module << " not a bias board";
        logwrite( function, message.str() );
        return ERROR;
        break;
    }

    // Check that the channel number is valid
    // and add it to the bias configuration string.
    //
    if ( (channel < 1) || (channel > 30) ) {
      message.str(""); message << "bias channel " << module << ": outside range {1:30}";
      logwrite( function, message.str() );
      return ERROR;
    }
    if ( (channel > 0) && (channel < 25) ) {
      biasconfig << "LC_V" << channel;
    }
    if ( (channel > 24) && (channel < 31) ) {
      channel -= 24;
      biasconfig << "HC_V" << channel;
    }

    if ( (voltage < vmin) || (voltage > vmax) ) {
      message.str(""); message << "bias voltage " << voltage << ": outside range {" << vmin << ":" << vmax << "}";
      logwrite( function, message.str() );
      return ERROR;
    }

    // Locate this line in the configuration so that it can be written to the Archon
    //
    std::string key   = biasconfig.str();
    std::string value = std::to_string(voltage);
    bool changed      = false;
    long error;

    // If no voltage suppled (readonly) then just read the configuration and exit
    //
    if (readonly) {
      message.str("");
      error = this->get_configmap_value(key, voltage);
      if (error != NO_ERROR) {
        message << "reading bias " << key;

      } else {
        retstring = std::to_string(voltage);
        message << "read bias " << key << "=" << voltage;
      }
      logwrite( function, message.str() );
      return error;
    }

    // Write the config line to update the bias voltage
    //
    error = this->controller.write_config_key(key.c_str(), value.c_str(), changed);

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if (error == NO_ERROR) error = this->controller.send_cmd(applystr.str());

    if (error != NO_ERROR) {
      message << "writing bias configuration: " << key << "=" << value;

    } else if (!changed) {
      message << "bias configuration: " << key << "=" << value <<" unchanged";

    } else {
      message << "updated bias configuration: " << key << "=" << value;
    }

    logwrite(function, message.str());

    return error;
  }
  /***** Camera::ArchonInterface::bias ****************************************/


  /***** Camera::ArchonInterface::bin *****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::bin( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bin");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::bin *****************************************/


  /***** Camera::ArchonInterface::connect_controller **************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::connect_controller( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::connect_controller");
    std::stringstream message;
    long error;

    // nothing to do if already connected
    if ( controller.archon.isconnected() ) {
      logwrite(function, "camera connection already open");
      return NO_ERROR;
    }

    // initialize camera connection
    try {
      if ( controller.archon.Connect() < 0 ) throw std::runtime_error("could not connect");
    }
    catch (const std::exception &e) {
      logwrite(function, "ERROR "+std::string(e.what()));
      retstring = "could not connect";
      return ERROR;
    }

    message.str("");
    message << "socket connection to host " << controller.archon.gethost()
            << " port " << controller.archon.getport()
            << " established on fd " << controller.archon.getfd();
    logwrite(function, message.str());

    // get the Archon system information for installed modules
    std::string reply;
    error = controller.send_cmd( SYSTEM, reply );     // first the whole reply in one string

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                    // then each line in a separate token "lines"

    for ( const auto& line : lines ) {
      Tokenize( line, tokens, "_=" );                 // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;             // need 3 tokens

      std::string version;
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->controller.backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( ( tokens[0].compare( 0, 3, "MOD" ) == 0 ) && ( tokens[1] == "TYPE" ) ) {
        try {
          module = std::stoi( tokens[0].substr(3) );
          type   = std::stoi( tokens[2] );

        } catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert module or type from " << tokens[0] << "=" << tokens[1] << " to integer";
          logwrite( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "module " << tokens[0].substr(3) << " or type " << tokens[1] << " out of range";
          logwrite( function, message.str() );
          return ERROR;
        }

      } else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= NMODS) ) {
        try {
          this->controller.modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->controller.modversion.at(module-1) = version;    // store the type in a vector indexed by module
        }
        catch (const std::exception &e) {
          message.str(""); message << "requested module " << module << " out of range {1:" << NMODS << "}";
          logwrite( function, message.str() );
        }
      }
      else {                                          // else should never happen
        message.str(""); message << "module " << module << " outside range {1:" << NMODS << "}";
        logwrite( function, message.str() );
        return ERROR;
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      int adchans=0;
      if ( type ==  2 ) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );  // ADC module (type=2) found
      if ( type == 17 ) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );  // ADM module (type=17) found
      controller.gain.resize( adchans );
      controller.offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( ( type == 2 || type == 17 ) && ( module < 5 || module > 8 ) ) {
        message.str(""); message << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        logwrite( function, message.str() );
        return ERROR;
      }
    } // end for ( auto line : lines )

    // empty the Archon log
    //
    error = this->controller.fetchlog();

    return error;
  }
  /***** Camera::ArchonInterface::connect_controller **************************/


  /***** Camera::ArchonInterface::get_configmap_value *************************/
  /**
   * @brief      get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param[in]  key_in is the KEY
   * @param[out] value_out reference to variable to contain the VALUE
   * @return     ERROR | NO_ERROR
   *
   * This is a template class function so the &value_out reference can be any type.
   * If the key_in KEY is not found then an error message is logged and ERROR is
   * returned, otherwise the VALUE associated with key_in is assigned to &value_out, 
   * and NO_ERROR is returned.
   *
   */
  template <class T>
  long ArchonInterface::get_configmap_value(std::string key_in, T& value_out) {
    const std::string function("Camera::ArchonInterface::get_configmap_value");
    std::stringstream message;

    if ( this->controller.configmap.find(key_in) != this->controller.configmap.end() ) {
      std::istringstream( this->controller.configmap[key_in].value  ) >> value_out;
      return NO_ERROR;
    }
    else {
      message.str(""); message << "ERROR requested key: " << key_in << " not found in configuration";
      logwrite( function, message.str() );
      return ERROR;
    }
  }
  /***** Camera::ArchonInterface::get_configmap_value *************************/


  /***** Camera::ArchonInterface::disconnect_controller ***********************/
  /**
   * @brief      general description
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       not used
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     HELP|ERROR|NO_ERROR
   *
   */
  long ArchonInterface::disconnect_controller(const std::string args, std::string &retstring) {
    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_CLOSE;
      retstring.append( " \n" );
      retstring.append( "  Closes host connection to Archon controller.\n" );
      return HELP;
    }
    return disconnect_controller();
  }
  /***** Camera::ArchonInterface::disconnect_controller ***********************/
  /**
   * @brief      specialized version closes connection to Archon
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::disconnect_controller() {
    const std::string function("Camera::ArchonInterface::disconnect_controller");
    long error = controller.archon.Close();
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");
    }
    else {
      logwrite( function, "ERROR disconnecting Archon" );
    }
    return error;
  }
  /***** Camera::ArchonInterface::disconnect_controller ***********************/


  /***** Camera::ArchonInterface::exptime *************************************/
  /**
   * @brief      set/get the exposure time
   * @details    exposure time is in the units set by longexposure() or the
   *             unit can be optionally set here
   * @param[in]  exptime_in  "<time> [ s | ms ]" exposure time in current units
   * @param[out] retstring   return string
   * @return     ERROR | NO_ERROR
   *
   * This function calls "set_parameter()" and "get_parameter()" using
   * the "exptime" parameter (which must already be defined in the ACF file).
   *
   */
  long ArchonInterface::exptime( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::exptime");
    std::stringstream message;
    logwrite(function, "not yet implemented");
    long ret=NO_ERROR;
/***
    if ( !exptime_in.empty() ) {
      if ( exptime_in.find(".") != std::string::npos ) {
      logwrite( function, "must be a whole number" );
      retstring="invalid_argument";
      return ERROR;
      }
      std::vector<std::string> tokens;
      Tokenize( exptime_in, tokens, " " );
      try {
        uint32_t exptime=0;
        std::string unit;
        if ( tokens.size() > 0 ) exptime = static_cast<uint32_t>( std::stoul( tokens.at(0) ) );
        if ( tokens.size() > 1 ) unit    = tokens.at(1);
        if ( tokens.size() < 1 || tokens.size() > 2 ) {
          logwrite( function, "expected <exptime> [ s | ms ]" );
          retstring="invalid_argument";
          return ERROR;
        }

        if ( exptime < 0 || exptime > Archon::MAX_EXPTIME ) throw std::out_of_range("value out of range");

        // If a unit was supplied then call longexposure() to handle that
        //
        if ( ! unit.empty() ) {
          if ( unit=="s" )  ret = this->longexposure( "true", retstring );
          else
          if ( unit=="ms" ) ret = this->longexposure( "false", retstring );
          else {
            logwrite( function, "expected <exptime> [ s | ms ]" );
            return ERROR;
          }
          // longexposure() could fail if there's an error talking to Archon
          // or if longexposure is not supported by the ACF.
          //
          if ( ret != NO_ERROR ) {
            logwrite( function, "unable to set selected unit. exptime not set." );
            retstring="invalid_argument";
            return ERROR;
          }
        }

        // set the parameter on the Archon
        //
        std::stringstream cmd;
        cmd << "exptime " << tokens.at(0);
        ret = this->set_parameter( cmd.str() );

        // If Archon was updated then update the class
        //
        if ( ret == NO_ERROR ) {
          if ( ! unit.empty() ) this->camera_info.exposure_time.unit( unit );
          this->camera_info.exposure_time.value( exptime );
        }
      }
      catch (std::exception &e) {
        message.str(""); message << "parsing exposure time: " << exptime_in << ": " << e.what();
        logwrite( function, message.str() );
        return ERROR;
      }
    }

    // add exposure time to system keys db
    //
    message.str(""); message << "EXPTIME=" << this->camera_info.exposure_time.value()
                             << " // exposure time in " << this->camera_info.exposure_time.unit();
    this->systemkeys.addkey( message.str() );

    // prepare the return value
    //
    message.str(""); message << this->camera_info.exposure_time.value() << " " << this->camera_info.exposure_time.unit();
    retstring = message.str();

    message.str(""); message << "exposure time is " << retstring;
    logwrite(function, message.str());

    return ret;
***/
    return ERROR;
  }
  /***** Camera::ArchonInterface::exptime *************************************/


  /***** Camera::ArchonInterface::expose **************************************/
  /**
   * @brief      
   * @param      
   * @param      
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::expose( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::expose");

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_EXPOSE;
      retstring.append( " <tbd>\n" );
      retstring.append( "  TBD\n" );
      return HELP;
    }

    int nseq=1;

    if (!args.empty()) {
      try { nseq = std::stoi(args);
      }
      catch (const std::exception &e) {
        retstring="ERROR reading nseq: "+std::string(e.what());
        logwrite(function, retstring);
        return ERROR;
      }
    }

    int nseq_remaining = nseq;

    while (nseq_remaining-- > 0) {
      do_expose(camera_info.nexp);
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::expose **************************************/


  /***** Camera::ArchonInterface::read_acf ************************************/
  /**
   * @brief      loads the ACF file into host only
   * @details    This is a wrapper to provide outside access to load the ACF
   *             file with write_to_archon = false.
   * @param[in]  filename  (optional) fully qualified path to ACF file. Pull from
   *                       config file if not supplied.
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::read_acf(const std::string &filename) {
    return( this->controller.load_acf(filename, false) );
  }
  /***** Camera::ArchonInterface::read_acf ************************************/


  /***** Camera::ArchonInterface::load_firmware *******************************/
  /**
   * @brief      general description of load firmware
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       fully qualified path of ACF
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::load_firmware(const std::string &args, std::string &retstring) {
    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_LOAD;
      retstring.append( " <acf-file>\n" );
      retstring.append( "  Loads the ACF file and applies the complete Archon configuration.\n" );
      retstring.append( "  Archon power will be off after this operation.\n" );
      return HELP;
    }
    return load_firmware(args);
  }
  /***** Camera::ArchonInterface::load_firmware *******************************/
  /**
   * @brief      loads the ACF file and applies the complete Archon Configuration
   * @param[in]  args       fully qualified path of ACF
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::load_firmware(const std::string &acffile) {
    // load the ACF file and write to Archon configuration memory
    //
    long error = this->controller.load_acf(acffile);

    // Parse and apply the complete system configuration from configuration memory.
    // Detector power will be off after this.
    //
    if (error == NO_ERROR) error = this->controller.send_cmd(APPLYALL);

    // read/clear Archon's internal error log
    //
    if (error != NO_ERROR) this->controller.fetchlog();

    // set the mode to DEFAULT
    //
    if (error == NO_ERROR) error = this->set_camera_mode(std::string("DEFAULT"));

    // TODO set exptime
    // TODO other instrument-specific defaults?

    return error;
  }
  /***** Camera::ArchonInterface::load_firmware *******************************/


  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param      acffile, specified ACF to load
   * @param      retstring, reference to string for return values
   * @return     ERROR | NO_ERROR | HELP
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing( const std::string args, std::string &retstring ) {
    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_LOADTIMING;
      retstring.append( " <timing.acf>\n" );
      retstring.append( "  Loads <timing.acf> file into Archon, then sends the LOADTIMING command\n" );
      retstring.append( "  which parses and compiles only the timing script and parameters.\n" );
      return HELP;
    }
    return( this->load_timing(args) );
  }
  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @param      acffile, specified ACF to load
   * @return     ERROR | NO_ERROR
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing(const std::string &filename) {
    // load the ACF file into configuration memory
    //
    long error = this->controller.load_acf( filename );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->controller.send_cmd(LOADTIMING);

    return error;
  }
  /***** Camera::ArchonInterface::load_timing *********************************/


  /***** Camera::ArchonInterface::set_camera_mode *****************************/
  /**
   * @brief      set a camera "mode"
   * @details    Allowable camera modes are defined in the ACF file and specify
   *             a set of camera settings which can be used to change camera
   *             settings.
   *             This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       requested mode name (case-insensitive)
   * @param[out  retstring  current mode name
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_camera_mode(std::string args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::set_camera_mode");
    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_MODE;
      retstring.append( " <name>\n" );
      retstring.append( "  Applies camera settings associated with MODE_<name> specified in the ACF file.\n");
      return HELP;
    }
    return( this->set_camera_mode(args) );
  }
  /***** Camera::ArchonInterface::set_camera_mode *****************************/
  /**
   * @brief      set a camera "mode"
   * @details    This version is for internal use.
   * @param[in]  mode       requested mode name (must be capitalized)
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_camera_mode(const std::string &mode) {
    const std::string function("Camera::ArchonInterface::set_camera_mode");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::set_camera_mode *****************************/


  /***** Camera::ArchonInterface::native **************************************/
  /**
   * @brief      send native commands directly to Archon and log result
   * @details    sends the command directly to Archon but does not parse any
   *             reply. @TODO publish the reply?
   * @param[in]  args       command to send to Archon Controller
   * @param[out] retstring  reply from Archon
   * @return     ERROR|NO_ERROR|BUSY, return from controller.send_cmd() call
   *
   */
  long ArchonInterface::native( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::native");

    // Help
    //
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_NATIVE;
      retstring.append( " <cmd>\n" );
      retstring.append( "  Sends <cmd> directly to Archon without parsing the reply, other than\n" );
      retstring.append( "  to confirm that it did reply.\n" );
      return HELP;
    }

    long error = this->controller.send_cmd(args, retstring);

    if (!retstring.empty()) {
      logwrite(function, retstring);
      // TODO publish the reply?
    }

    return error;
  }
  /***** Camera::ArchonInterface::native **************************************/


  /***** Camera::ArchonInterface::power ***************************************/
  /**
   * @brief      turn on controller bias power supplies
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::power( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::power");
    std::stringstream message;
    long error=NO_ERROR;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_POWER;
      retstring.append( " [ on | off ]\n" );
      retstring.append( "  Turn on|off Archon bias power supplies. If no arg supplied then\n" );
      retstring.append( "  return current state.\n" );
      return HELP;
    }

    // nothing to do if no connection open to controller
    if (!this->controller.archon.isconnected()) {
      logwrite( function, "ERROR connection not open to controller" );
      return ERROR;
    }


    // set the Archon power state as requested
    //
    if ( !args.empty() ) {
      if ( caseCompareString(args, "on") ) {
        // send POWERON command to Archon and wait 2s to ensure stable
        if ( (error=this->controller.send_cmd( POWERON )) == NO_ERROR ) {
          std::this_thread::sleep_for( std::chrono::seconds(2) );
        }
      }
      else
      if ( caseCompareString(args, "off") ) {
        // send POWEROFF command to Archon and wait 200ms to ensure off
        if ( (error=this->controller.send_cmd( POWEROFF )) == NO_ERROR ) {
          std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        }
      }
      else {
        logwrite(function, "ERROR expected {ON|OFF}");
        return ERROR;
      }
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR setting Archon power "+args);
        return ERROR;
      }
    }

    // Read the Archon power state directly from Archon
    //
    std::string power;
    error = this->controller.get_status_key( "POWER", power );

    if ( error != NO_ERROR ) return ERROR;

    int status=-1;

    try { status = std::stoi( power ); }
    catch (const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    // set the power status (or not) depending on the value extracted from the STATUS message
    //
    switch( status ) {
      case -1:                                                  // no POWER token found in status message
        logwrite(function, "ERROR finding power in Archon status message" );
        return ERROR;
      case  0:                                                  // usually an internal error
        this->controller.power_status = "UNKNOWN";
        break;
      case  1:                                                  // no configuration applied
        this->controller.power_status = "NOT_CONFIGURED";
        break;
      case  2:                                                  // power is off
        this->controller.power_status = "OFF";
        break;
      case  3:                                                  // some modules powered, some not
        this->controller.power_status = "INTERMEDIATE";
        break;
      case  4:                                                  // power is on
        this->controller.power_status = "ON";
        break;
      case  5:                                                  // system is in standby
        this->controller.power_status = "STANDBY";
        break;
      default:                                                  // should be impossible
        logwrite(function, "ERROR unknown power status: "+std::to_string(status));
        return ERROR;
    }

    message.str(""); message << "POWER:" << this->controller.power_status;
//  this->camera.async.enqueue( message.str() );

    retstring = this->controller.power_status;

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::power ***************************************/


  /***** Camera::ArchonInterface::test ****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::test( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::test");

    // initialize the exposure mode to ExposureModeCCD and call that expose
    //
    logwrite(function, "----- calling exposure_mode->expose() for exposure mode CCD -----");
    exposure_mode = std::make_unique<ExposureModeCCD>(this);
    if (exposure_mode) exposure_mode->expose();

    // initialize the exposure mode to ExposureModeRXRV and call that expose
    //
    logwrite(function, "----- calling exposure_mode->expose() for exposure mode RXRV -----");
    exposure_mode = std::make_unique<ExposureModeRXRV>(this);
    if (exposure_mode) exposure_mode->expose();

    if (!exposure_mode) {
      logwrite(function, "ERROR exposure mode undefined!");
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::test ****************************************/


  /***** Camera::ArchonInterface::allocate_framebuf ***************************/
  /**
   * @brief      allocate memory for frame buffer
   * @details    wraps Camera::Controller::allocate_framebuf
   * @param[in]  reqsz  requested frame buffer size in bytes
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::allocate_framebuf(uint32_t reqsz) {
    return controller.allocate_framebuf(reqsz);
  }
  /***** Camera::ArchonInterface::allocate_framebuf ***************************/


  long ArchonInterface::read_frame() {
    controller.read_frame(Camera::ArchonController::FRAME_IMAGE);
    return NO_ERROR;
  }


  long ArchonInterface::do_expose(int nexp) {
    const std::string function("Camera::ArchonInterface::do_expose");
    long error=NO_ERROR;

    logwrite(function, "here");

    // spawn two threads, a producer and a consumer
    //
    // The producer triggers the exposure and collect images into a FIFO queue.
    // The consumer pops images out of the queue for processing.
    //
    std::thread producer(&ArchonInterface::image_acquisition_thread, this);
    std::thread consumer(&ArchonInterface::image_processing_thread, this);

    producer.join();
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      is_producer_finished=true;
      error |= (is_producer_error ? ERROR : NO_ERROR);
    }

    consumer.join();

    return NO_ERROR;
  }


  void ArchonInterface::image_acquisition_thread() {
    const std::string function("Camera::ArchonInterface::image_acquisition_thread");
    logwrite(function, "here");
  }


  void ArchonInterface::image_processing_thread() {
    const std::string function("Camera::ArchonInterface::image_processing_thread");
    logwrite(function, "here");
  }

}
