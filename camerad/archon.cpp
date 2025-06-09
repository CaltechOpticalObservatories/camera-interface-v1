/**
 * @file    archon.cpp
 * @brief   camera interface functions
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "archon.h"

#include <sstream>   // for std::stringstream
#include <iomanip>   // for setfil, setw, etc.
#include <iostream>  // for hex, uppercase, etc.
#include <algorithm> 
#include <cctype>
#include <string>
#include <fstream>
#include <array>
#include <utility>

namespace Archon {

  // Archon::Interface constructor
  //
  Interface::Interface() {
    this->archon_busy = false;
    this->modeselected = false;
    this->firmwareloaded = false;
    this->msgref = 0;
    this->lastframe = 0;
    this->frame.index = 0;
    this->frame.next_index = 0;
    this->abort = false;
    this->taplines = 0;
    this->configlines = 0;
    this->logwconfig = false;
    this->image_data = nullptr;
    this->image_data_bytes = 0;
    this->image_data_allocated = 0;
    this->is_longexposure_set = false;
    this->is_window = false;
    this->is_autofetch = false;
    this->win_hstart = 0;
    this->win_hstop = 2047;
    this->win_vstart = 0;
    this->win_vstop = 2047;
    this->tapline0_store = "";
    this->taplines_store = 0;

    this->n_hdrshift = 16;
    this->backplaneversion="";

    this->trigin_state="disabled";
    this->trigin_expose = 0;
    this->trigin_untimed = 0;
    this->trigin_readout = 0;

    this->lastcubeamps = this->camera.cubeamps();

    this->trigin_expose_enable   = DEF_TRIGIN_EXPOSE_ENABLE;
    this->trigin_expose_disable  = DEF_TRIGIN_EXPOSE_DISABLE;
    this->trigin_untimed_enable  = DEF_TRIGIN_UNTIMED_ENABLE;
    this->trigin_untimed_disable = DEF_TRIGIN_UNTIMED_DISABLE;
    this->trigin_readout_enable  = DEF_TRIGIN_READOUT_ENABLE;
    this->trigin_readout_disable = DEF_TRIGIN_READOUT_DISABLE;

    this->shutenable_enable      = DEF_SHUTENABLE_ENABLE;
    this->shutenable_disable     = DEF_SHUTENABLE_DISABLE;

    // pre-size the modtype and modversion vectors to hold the max number of modules
    //
    this->modtype.resize( nmods );
    this->modversion.resize( nmods );

    // TODO I should change these to STL maps instead
    //
    this->frame.bufsample.resize( Archon::nbufs );
    this->frame.bufcomplete.resize( Archon::nbufs );
    this->frame.bufmode.resize( Archon::nbufs );
    this->frame.bufbase.resize( Archon::nbufs );
    this->frame.bufframen.resize( Archon::nbufs );
    this->frame.bufwidth.resize( Archon::nbufs );
    this->frame.bufheight.resize( Archon::nbufs );
    this->frame.bufpixels.resize( Archon::nbufs );
    this->frame.buflines.resize( Archon::nbufs );
    this->frame.bufrawblocks.resize( Archon::nbufs );
    this->frame.bufrawlines.resize( Archon::nbufs );
    this->frame.bufrawoffset.resize( Archon::nbufs );
    this->frame.buftimestamp.resize( Archon::nbufs );
    this->frame.bufretimestamp.resize( Archon::nbufs );
    this->frame.buffetimestamp.resize( Archon::nbufs );
  }

  // Archon::Interface deconstructor
  //
  Interface::~Interface() = default;


  /**************** Archon::Interface::interface ******************************/
  long Interface::interface(std::string &iface) {
    std::string function = "Archon::Interface::interface";
    iface = "STA-Archon";
    logwrite(function, iface);
    return 0;
  }
  /**************** Archon::Interface::interface ******************************/


  /***Note from danw: I couldn't get this to link without adding a dummy method here no idea why
   **/

  long Interface::power(std::string state_in, std::string& retstring)
   {
     return do_power(state_in, retstring);
   }

  
  
  /***** Archon::Interface::do_power ******************************************/
  /**
   * @brief      set/get the power state
   * @param[in]  state_in    input string contains requested power state
   * @param[out] retstring   return string contains the current power state
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::do_power(std::string state_in, std::string &retstring) {
    std::string function = "Archon::Interface::do_power";
    std::stringstream message;
    long error = ERROR;

    if ( !this->archon.isconnected() ) {                        // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return( ERROR );
    }

    // set the Archon power state as requested
    //
    if ( !state_in.empty() ) {                                  // received something
      std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
      if ( state_in == "ON" ) {
        error = this->archon_cmd( POWERON );                    // send POWERON command to Archon
        if ( error == NO_ERROR ) std::this_thread::sleep_for( std::chrono::seconds(2) );         // wait 2s to ensure power is stable
      }
      else
      if ( state_in == "OFF" ) {
        error = this->archon_cmd( POWEROFF );                   // send POWEROFF command to Archon
        if ( error == NO_ERROR ) std::this_thread::sleep_for( std::chrono::milliseconds(200) );  // wait 200ms to ensure power is off
      }
      else {
        message.str(""); message << "unrecognized argument " << state_in << ": expected {on|off}";
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
      if ( error != NO_ERROR ) {
        message.str(""); message << "setting Archon power " << state_in;
        this->camera.log_error( function, message.str() );
        return( ERROR );
      }
    }

    // Read the Archon power state directly from Archon
    //
    std::string power;
    error = get_status_key( "POWER", power );

    if ( error != NO_ERROR ) return( ERROR );

    int status=-1;

    try { status = std::stoi( power ); }
    catch (std::invalid_argument &) {
      this->camera.log_error( function, "unable to convert power status message to integer" );
      return(ERROR);
    }
    catch (std::out_of_range &) {
      this->camera.log_error( function, "power status out of range" );
      return(ERROR);
    }

    // set the power status (or not) depending on the value extracted from the STATUS message
    //
    switch( status ) {
      case -1:                                                  // no POWER token found in status message
        this->camera.log_error( function, "unable to find power in Archon status message" );
        return( ERROR );
      case  0:                                                  // usually an internal error
        this->camera.power_status = "UNKNOWN";
        break;
      case  1:                                                  // no configuration applied
        this->camera.power_status = "NOT_CONFIGURED";
        break;
      case  2:                                                  // power is off
        this->camera.power_status = "OFF";
        break;
      case  3:                                                  // some modules powered, some not
        this->camera.power_status = "INTERMEDIATE";
        break;
      case  4:                                                  // power is on
        this->camera.power_status = "ON";
        break;
      case  5:                                                  // system is in standby
        this->camera.power_status = "STANDBY";
        break;
      default:                                                  // should be impossible
        message.str(""); message << "unknown power status: " << status;
        this->camera.log_error( function, message.str() );
        return( ERROR );
    }

    message.str(""); message << "POWER:" << this->camera.power_status;
    this->camera.async.enqueue( message.str() );

    retstring = this->camera.power_status;

    return(NO_ERROR);
  }
  /***** Archon::Interface::do_power ******************************************/



  /***** Archon::Interface::configure_controller ******************************/
  /**
   * @brief      parse controller-related keys from the configuration file
   * @details    the config file was read by server.config.read_config() in main()
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::configure_controller() {
    std::string function = "Archon::Interface::configure_controller";
    std::stringstream message;
    int applied=0;
    long error;

    // Must re-init all values to start-up defaults in case this function is 
    // called again to re-load the config file (such as if a HUP is received)
    // and the new config file may not have everything defined.
    // This ensures nothing is carried over from any previous config.
    //
    this->camera_info.hostname = "";
    this->archon.sethost( "" );
    this->camera_info.port = -1;
    this->archon.setport( -1 );

    this->n_hdrshift = 16;

    this->camera.firmware[0] = "";

    this->exposeparam = "";
    this->longexposeparam.clear();
    this->trigin_exposeparam = "";
    this->trigin_untimedparam = "";
    this->trigin_readoutparam = "";

    this->trigin_expose_enable   = DEF_TRIGIN_EXPOSE_ENABLE;
    this->trigin_expose_disable  = DEF_TRIGIN_EXPOSE_DISABLE;
    this->trigin_untimed_enable  = DEF_TRIGIN_UNTIMED_ENABLE;
    this->trigin_untimed_disable = DEF_TRIGIN_UNTIMED_DISABLE;
    this->trigin_readout_enable  = DEF_TRIGIN_READOUT_ENABLE;
    this->trigin_readout_disable = DEF_TRIGIN_READOUT_DISABLE;

    this->shutenable_enable      = DEF_SHUTENABLE_ENABLE;
    this->shutenable_disable     = DEF_SHUTENABLE_DISABLE;

    // loop through the entries in the configuration file, stored in config class
    //
    for (int entry=0; entry < this->config.n_entries; entry++) {

      if (config.param[entry].compare(0, 9, "ARCHON_IP")==0) {
        this->camera_info.hostname = config.arg[entry];
        this->archon.sethost( config.arg[entry] );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 11, "ARCHON_PORT")==0) {              // ARCHON_PORT
        int port;
        try {
          port = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert port number to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "port number out of integer range" );
          return ERROR;
        }
        this->camera_info.port = port;
        this->archon.setport(port);
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 12, "AMPS_AS_CUBE")==0) {
        std::string dontcare;
        if ( this->camera.cubeamps( config.arg[entry], dontcare ) == ERROR ) {
          this->camera.log_error( function, "setting cubeamps" );
          return ERROR;
        }
      }

      if (config.param[entry].compare(0, 12, "EXPOSE_PARAM")==0) {             // EXPOSE_PARAM
        this->exposeparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if ( config.param[entry] == "LONGEXPOSE_PARAM" ) {                       // LONGEXPOSE_PARAM
        this->longexposeparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 19, "TRIGIN_EXPOSE_PARAM")==0) {      // TRIGIN_EXPOSE_PARAM
        this->trigin_exposeparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 20, "TRIGIN_UNTIMED_PARAM")==0) {     // TRIGIN_UNTIMED_PARAM
        this->trigin_untimedparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 20, "TRIGIN_READOUT_PARAM")==0) {     // TRIGIN_READOUT_PARAM
        this->trigin_readoutparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 20, "TRIGIN_EXPOSE_ENABLE")==0) {     // TRIGIN_EXPOSE_ENABLE
        int enable;
        try {
          enable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_EXPOSE_ENABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_EXPOSE_ENABLE out of integer range" );
          return ERROR;
        }
        this->trigin_expose_enable = enable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 21, "TRIGIN_EXPOSE_DISABLE")==0) {    // TRIGIN_EXPOSE_DISABLE
        int disable;
        try {
          disable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_EXPOSE_DISABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_EXPOSE_DISABLE out of integer range" );
          return ERROR;
        }
        this->trigin_expose_disable = disable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 21, "TRIGIN_UNTIMED_ENABLE")==0) {    // TRIGIN_UNTIMED_ENABLE
        int enable;
        try {
          enable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_UNTIMED_ENABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_UNTIMED_ENABLE out of integer range" );
          return ERROR;
        }
        this->trigin_untimed_enable = enable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 22, "TRIGIN_UNTIMED_DISABLE")==0) {   // TRIGIN_UNTIMED_DISABLE
        int disable;
        try {
          disable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_UNTIMED_DISABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_UNTIMED_DISABLE out of integer range" );
          return ERROR;
        }
        this->trigin_untimed_disable = disable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 21, "TRIGIN_READOUT_ENABLE")==0) {    // TRIGIN_READOUT_ENABLE
        int enable;
        try {
          enable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_READOUT_ENABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_READOUT_ENABLE out of integer range" );
          return ERROR;
        }
        this->trigin_readout_enable = enable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 22, "TRIGIN_READOUT_DISABLE")==0) {   // TRIGIN_READOUT_DISABLE
        int disable;
        try {
          disable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert TRIGIN_READOUT_DISABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "TRIGIN_READOUT_DISABLE out of integer range" );
          return ERROR;
        }
        this->trigin_readout_disable = disable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 16, "SHUTENABLE_PARAM")==0) {         // SHUTENABLE_PARAM
        this->shutenableparam = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 17, "SHUTENABLE_ENABLE")==0) {        // SHUTENABLE_ENABLE
        int enable;
        try {
          enable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert SHUTENABLE_ENABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "SHUTENABLE_ENABLE out of integer range" );
          return ERROR;
        }
        this->shutenable_enable = enable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 18, "SHUTENABLE_DISABLE")==0) {       // SHUTENABLE_DISABLE
        int disable;
        try {
          disable = std::stoi( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert SHUTENABLE_DISABLE to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "SHUTENABLE_DISABLE out of integer range" );
          return ERROR;
        }
        this->shutenable_disable = disable;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      // .firmware and .readout_time are STL maps but (for now) only one Archon per computer
      // so map always to 0
      //
      if (config.param[entry].compare(0, 16, "DEFAULT_FIRMWARE")==0) {
        this->camera.firmware[0] = config.arg[entry];
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 16, "HDR_SHIFT")==0) {
        std::string dontcare;
        this->hdrshift( config.arg[entry], dontcare );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 12, "READOUT_TIME")==0) {
        int readtime;
        try {
          readtime = std::stoi ( config.arg[entry] );

        } catch (std::invalid_argument &) {
          this->camera.log_error( function, "unable to convert readout time to integer" );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "readout time out of integer range" );
          return ERROR;
        }
        this->camera.readout_time[0] = readtime;
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 5, "IMDIR")==0) {
        this->camera.imdir( config.arg[entry] );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 7, "DIRMODE")==0) {
        std::string s( config.arg[entry] );
        std::stringstream mode_bit;
        mode_t mode=0;
        for ( size_t i=0; i < s.length(); i++ ) {
          try {
            mode = (mode << 3);
            mode_bit.str(""); mode_bit << s.at(i);
            mode |= std::stoi( mode_bit.str() );

          } catch (std::invalid_argument &) {
            this->camera.log_error( function, "unable to convert mode bit to integer" );
            return ERROR;

          } catch (std::out_of_range &) {
            this->camera.log_error( function, "out of range converting dirmode bit" );
            return ERROR;
          }
        }
        this->camera.set_dirmode( mode );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

      if (config.param[entry].compare(0, 8, "BASENAME")==0) {
        this->camera.basename( config.arg[entry] );
        message.str(""); message << "CONFIG:" << config.param[entry] << "=" << config.arg[entry];
        logwrite( function, message.str() );
        this->camera.async.enqueue( message.str() );
        applied++;
      }

    }

    message.str("");
    if (applied==0) {
      message << "ERROR: ";
      error = ERROR;

    } else {
      error = NO_ERROR;
    }
    message << "applied " << applied << " configuration lines to controller";
    error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() ) ;
    return error;
  }
  /***** Archon::Interface::configure_controller ******************************/


  /**************** Archon::Interface::prepare_image_buffer *******************/
  /**
   * @fn     prepare_image_buffer
   * @brief  prepare image_data buffer, allocating memory as needed
   * @param  none
   * @return NO_ERROR if successful or ERROR on error
   *
   */
  long Interface::prepare_image_buffer() {
    std::string function = "Archon::Interface::prepare_image_buffer";
    std::stringstream message;

    // If there is already a correctly-sized buffer allocated,
    // then don't do anything except initialize that space to zero.
    //
    if ( (this->image_data != nullptr)     &&
         (this->image_data_bytes != 0) &&
         (this->image_data_allocated == this->image_data_bytes) ) {
      memset(this->image_data, 0, this->image_data_bytes);
      message.str(""); message << "initialized " << this->image_data_bytes << " bytes of image_data memory";
      logwrite(function, message.str());

    } else {
        // If memory needs to be re-allocated, delete the old buffer
      if (this->image_data != nullptr) {
        logwrite(function, "deleting old image_data buffer");
        delete [] this->image_data;
        this->image_data=nullptr;
      }
      // Allocate new memory
      //
      if (this->image_data_bytes != 0) {
        this->image_data = new char[this->image_data_bytes];
        this->image_data_allocated=this->image_data_bytes;
        message.str(""); message << "allocated " << this->image_data_bytes << " bytes for image_data";
        logwrite(function, message.str());

      } else {
        this->camera.log_error( function, "cannot allocate zero-length image memory" );
        return ERROR;
      }
    }

    return NO_ERROR;
  }
  /**************** Archon::Interface::prepare_image_buffer *******************/


  /**************** Archon::Interface::connect_controller *********************/
  /**
   * @fn     connect_controller
   * @brief
   * @param  none (devices_in here for future expansion)
   * @return 
   *
   */
  long Interface::connect_controller(const std::string& devices_in="") {
    std::string function = "Archon::Interface::connect_controller";
    std::stringstream message;
    int adchans=0;
    long   error = ERROR;

    if ( this->archon.isconnected() ) {
      logwrite(function, "camera connection already open");
      return NO_ERROR;
    }

    // Initialize the camera connection
    //
    logwrite(function, "opening a connection to the camera system");

    if ( this->archon.Connect() != 0 ) {
      message.str(""); message << "connecting to " << this->camera_info.hostname << ":" << this->camera_info.port << ": " << strerror(errno);
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    message.str("");
    message << "socket connection to " << this->camera_info.hostname << ":" << this->camera_info.port << " "
            << "established on fd " << this->archon.getfd();
    logwrite(function, message.str());

    // Get the current system information for the installed modules
    //
    std::string reply;
    error = this->archon_cmd( SYSTEM, reply );        // first the whole reply in one string

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
        if ( tokens[1] == "VERSION" ) this->backplaneversion = tokens[2];
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
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "module " << tokens[0].substr(3) << " or type " << tokens[1] << " out of range";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }

      } else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= nmods) ) {
        try {
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module

        } catch (std::out_of_range &) {
          message.str(""); message << "requested module " << module << " out of range {1:" << nmods;
          this->camera.log_error( function, message.str() );
        }

      } else {                                          // else should never happen
        message.str(""); message << "module " << module << " outside range {1:" << nmods << "}";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      if ( type ==  2 ) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );  // ADC module (type=2) found
      if ( type == 17 ) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );  // ADM module (type=17) found
      this->gain.resize( adchans );
      this->offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( ( type == 2 || type == 17 ) && ( module < 5 || module > 8 ) ) {
        message.str(""); message << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

    } // end for ( auto line : lines )

    // empty the Archon log
    //
    error = this->fetchlog();

    // Make sure the following systemkeys are added.
    // They can be changed at any time by a command but since they have defaults
    // they don't require a command so this ensures they get into the systemkeys db.
    //
    std::stringstream keystr;
    keystr << "HDRSHIFT=" << this->n_hdrshift << "// number of HDR right-shift bits";
    this->systemkeys.addkey( keystr.str() );

    return error;
  }
  /**************** Archon::Interface::connect_controller *********************/


  /**************** Archon::Interface::disconnect_controller ******************/
  /**
   * @fn     disconnect_controller
   * @brief
   * @param  none
   * @return 
   *
   */
  long Interface::disconnect_controller() {
    std::string function = "Archon::Interface::disconnect_controller";
    long error;
    if (!this->archon.isconnected()) {
      logwrite(function, "connection already closed");
      return (NO_ERROR);
    }
    // close the socket file descriptor to the Archon controller
    //
    error = this->archon.Close();

    // Free the memory
    //
    if (this->image_data != nullptr) {
      logwrite(function, "releasing allocated device memory");
      delete [] this->image_data;
      this->image_data=nullptr;
    }

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");

    } else {
        // Throw an error for any other errors
      this->camera.log_error( function, "disconnecting Archon camera" );
    }

    return error;
  }
  /**************** Archon::Interface::disconnect_controller ******************/


  /**************** Archon::Interface::native *********************************/
  /**
   * @fn     native
   * @brief  send native commands directly to Archon and log result
   * @param  std::string cmd
   * @return long ret from archon_cmd() call
   *
   * This function simply calls archon_cmd() then breaks the reply into
   * space-delimited tokens and puts each token into the asynchronous
   * message queue. The result is that the reply comes out one line at
   * a time on the async port.
   *
   */
  long Interface::native(const std::string& cmd) {
    std::string function = "Archon::Interface::native";
    std::stringstream message;
    std::string reply;
    std::vector<std::string> tokens;
    long ret = archon_cmd(cmd, reply);
    if (!reply.empty()) {
      // Tokenize the reply and put each non-empty token into the asynchronous message queue.
      // The reply message begins and ends with "CMD:BEGIN" and "CMD:END" and
      // each line of the reply is prepended with "CMD:" where CMD is the native command
      // which generated the message.
      //
      message << cmd << ":BEGIN";
      this->camera.async.enqueue( message.str() );

      Tokenize(reply, tokens, " ");
      for (const auto & token : tokens) {
        if ( ! token.empty() && token != "\n" ) {
          message.str(""); message << cmd << ":" << token;
          this->camera.async.enqueue( message.str() );
        }
      }
      message.str(""); message << cmd << ":END";
      this->camera.async.enqueue( message.str() );
    }
    return ret;
  }
  /**************** Archon::Interface::native *********************************/


  /**************** Archon::Interface::archon_cmd *****************************/
  /**
   * @fn     archon_cmd
   * @brief  send a command to Archon
   * @param  cmd
   * @param  reply (optional)
   * @return ERROR, BUSY or NO_ERROR
   *
   */
  long Interface::archon_cmd(std::string cmd) { // use this form when the calling
   std::string reply;                          // function doesn't need to look at the reply
    return( archon_cmd(cmd, reply) );
  }
  long Interface::archon_cmd(std::string cmd, std::string &reply) {
    std::string function = "Archon::Interface::archon_cmd";
    std::stringstream message;
    int     retval;
    char    check[4];
    char    buffer[4096];                       //!< temporary buffer for holding Archon replies
    int     error = NO_ERROR;

    if (!this->archon.isconnected()) {          // nothing to do if no connection open to controller
      this->camera.log_error( function, "connection not open to controller" );
      return ERROR;
    }

    if (this->archon_busy) {                    // only one command at a time
      message.str(""); message << "Archon busy: ignored command " << cmd;
      this->camera.log_error( function, message.str() );
      return BUSY;
    }

    /**
     * Hold a scoped lock for the duration of this function, 
     * to prevent multiple threads from accessing the Archon.
     */
    const std::lock_guard<std::mutex> lock(this->archon_mutex);
    this->archon_busy = true;

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    this->msgref = (this->msgref + 1) % 256;       // increment msgref for each new command sent
    std::stringstream ssprefix;
    ssprefix << ">"
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << this->msgref;
    std::string prefix=ssprefix.str();
    try {
      std::transform( prefix.begin(), prefix.end(), prefix.begin(), ::toupper );    // make uppercase

    } catch (...) {
      message.str(""); message << "converting Archon command: " << prefix << " to uppercase";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // This allows sending commands that don't get logged,
    // by prepending QUIET, which gets removed here if present.
    //
    bool quiet=false;
    if ( cmd.find(QUIET)==0 ) {
      cmd.erase(0, QUIET.length());
      quiet=true;
    }

    std::stringstream  sscmd;         // sscmd = stringstream, building command
    sscmd << prefix << cmd << "\n";
    std::string scmd = sscmd.str();   // scmd = string, command to send

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", this->msgref)

    // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
    //
    if ( !quiet && (cmd.compare(0,7,"WCONFIG") != 0) &&
                   (cmd.compare(0,5,"TIMER") != 0)   &&
                   (cmd.compare(0,6,"STATUS") != 0)  &&
                   (cmd.compare(0,5,"FRAME") != 0) ) {
      // erase newline for logging purposes
      std::string fcmd = scmd;
      try {
          fcmd.erase(fcmd.find('\n'), 1);
      } catch(...) { }
      message.str(""); message << "sending command: " << fcmd;
      logwrite(function, message.str());
    }

    // optionally log WCONFIG commands
    //
    if ( this->logwconfig && cmd.find("WCONFIG")!=std::string::npos ) logwrite( function, strip_newline(cmd) );

    // send the command
    //
    if ( (this->archon.Write(scmd)) == -1) {
      this->camera.log_error( function, "writing to camera socket");
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_frame).
    // Must also distinguish this from the FETCHLOG command, for which we do wait
    // for a normal reply.
    //
    // The scoped mutex lock will be released automatically upon return.
    //
    if ( (cmd.compare(0,5,"FETCH")==0)
        && (cmd.compare(0,8,"FETCHLOG")!=0) ) return (NO_ERROR);

    // For all other commands, receive the reply
    //
    reply.clear();                                   // zero reply buffer
    do {
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) {
            message.str("");
            message << "Poll timeout waiting for response from Archon command (maybe unrecognized command?)";
            error = TIMEOUT;
        }
        if (retval<0)  {
            message.str("");
            message << "Poll error waiting for response from Archon command";
            error = ERROR;
        }
        if ( error != NO_ERROR ) this->camera.log_error( function, message.str() );
        break;
      }
      memset(buffer, '\0', 2048);                    // init temporary buffer
      retval = this->archon.Read(buffer, 2048);      // read into temp buffer
      if (retval <= 0) {
        this->camera.log_error( function, "reading Archon" );
        break; 
      }
      reply.append(buffer);                          // append read buffer into the reply string
    } while(retval>0 && reply.find('\n') == std::string::npos);

    // If there was an Archon error then clear the busy flag and get out now
    //
    if ( error != NO_ERROR ) {
        this->archon_busy = false;
        return error;
    }

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (reply.compare(0, 1, "?")==0) {  // "?" means Archon experienced an error processing command
      error = ERROR;
      message.str(""); message << "Archon controller returned error processing command: " << cmd;
      this->camera.log_error( function, message.str() );

    } else if (reply.compare(0, 3, check)!=0) {  // First 3 bytes of reply must equal checksum else reply doesn't belong to command
        error = ERROR;
        // std::string hdr = reply;
        try {
          scmd.erase(scmd.find('\n'), 1);
        } catch(...) { }
        message.str(""); message << "command-reply mismatch for command: " + scmd + ": expected " + check + " but received " + reply ;
        this->camera.log_error( function, message.str() );
    } else {                                           // command and reply are a matched pair
      error = NO_ERROR;

      // log the command as long as it's not a STATUS, TIMER, WCONFIG or FRAME command
      if ( !quiet && (cmd.compare(0,7,"WCONFIG") != 0) &&
                     (cmd.compare(0,5,"TIMER") != 0)   &&
                     (cmd.compare(0,6,"STATUS") != 0)  &&
                     (cmd.compare(0,5,"FRAME") != 0) ) {
        message.str("");
        message << "command 0x" << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << this->msgref << " success";
        logwrite(function, message.str());
      }

      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the semaphore (still had the mutex this entire function)
    //
    this->archon_busy = false;

    return error;
  }
  /**************** Archon::Interface::archon_cmd *****************************/


  /**************** Archon::Interface::read_parameter *************************/
  /**
   * @fn     read_parameter
   * @brief  read a parameter from Archon configuration memory
   * @param  paramname  char pointer to name of paramter
   * @param  value  reference to string for return value
   * @return ERROR on error, NO_ERROR if okay.
   *
   * The string reference contains the value of the parameter
   * to be returned to the user.
   *
   * No direct calls to Archon -- this function uses archon_cmd()
   * which in turn handles all of the Archon in-use locking.
   *
   */
  long Interface::read_parameter(const std::string& paramname, std::string &value) {
    std::string function = "Archon::Interface::read_parameter";
    std::stringstream message;
    std::stringstream cmd;
    std::string reply;
    long   error   = NO_ERROR;

    if (this->parammap.find(paramname) == this->parammap.end()) {
      message.str(""); message << "parameter \"" << paramname << "\" not found in ACF";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // form the RCONFIG command to send to Archon
    //
    cmd.str("");
    cmd << "RCONFIG"
        << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
        << this->parammap[paramname.c_str()].line;
    error = this->archon_cmd(cmd.str(), reply);               // send RCONFIG command here

    if ( error != NO_ERROR ) {
      message.str(""); message << "ERROR: sending archon_cmd(" << cmd.str() << ")";
      logwrite( function, message.str() );
      return error;
    }

    try {
        reply.erase(reply.find('\n'), 1);
    } catch(...) { }  // strip newline

    // reply should now be of the form PARAMETERn=PARAMNAME=VALUE,
    // and we want just the VALUE here
    //

    size_t loc;
    value = reply;
    if (value.compare(0, 9, "PARAMETER") == 0) {                                      // value: PARAMETERn=PARAMNAME=VALUE
      if ( (loc=value.find('=')) != std::string::npos ) value = value.substr(++loc);  // value: PARAMNAME=VALUE
      else {
        value="NaN";
        error = ERROR;
      }
      if ( (loc=value.find('=')) != std::string::npos ) value = value.substr(++loc);  // value: VALUE
      else {
        value="NaN";
        error = ERROR;
      }

    } else {
      value="NaN";
      error = ERROR;
    }

    if (error==ERROR) {
      message << "malformed reply: " << reply << " to Archon command " << cmd.str() << ": Expected PARAMETERn=PARAMNAME=VALUE";
      this->camera.log_error( function, message.str() );

    } else {
      message.str(""); message << paramname << " = " << value;
      logwrite(function, message.str());
    }
    return error;
  }
  /**************** Archon::Interface::read_parameter *************************/


  /**************** Archon::Interface::prep_parameter *************************/
  /**
   * @fn     prep_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::prep_parameter(const std::string& paramname, std::string value) {
    std::string function = "Archon::Interface::prep_parameter";
    std::stringstream message;
    std::stringstream scmd;
    long error = NO_ERROR;

    // Prepare to apply it to the system -- will be loaded on next EXTLOAD signal
    //
    scmd << "FASTPREPPARAM " << paramname << " " << value;
    error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      message << "ERROR: prepping parameter \"" << paramname << "=" << value;
    }

    logwrite( function, message.str() );
    return error;
  }
  /**************** Archon::Interface::prep_parameter *************************/


  /**************** Archon::Interface::load_parameter *************************/
  /**
   * @fn     load_parameter
   * @brief  
   * @param  
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   */
  long Interface::load_parameter(std::string paramname, std::string value) {
    std::string function = "Archon::Interface::load_parameter";
    std::stringstream message;
    std::stringstream scmd;
    long error = NO_ERROR;

    scmd << "FASTLOADPARAM " << paramname << " " << value;
    error = this->archon_cmd(scmd.str());

    if (error != NO_ERROR) {
      message << "ERROR: loading parameter \"" << paramname << "=" << value << "\" into Archon";

    } else {
      message << "parameter \"" << paramname << "=" << value << "\" loaded into Archon";
    }

    logwrite( function, message.str() );
    return error;
  }
  /**************** Archon::Interface::load_parameter *************************/


  /**************** Archon::Interface::fetchlog *******************************/
  /**
   * @fn     fetchlog
   * @brief  fetch the archon log entry and log the response
   * @param  none
   * @return NO_ERROR or ERROR,  return value from archon_cmd call
   *
   * Send the FETCHLOG command to, then read the reply from Archon.
   * Fetch until the log is empty. Log the response.
   *
   */
  long Interface::fetchlog() {
    std::string function = "Archon::Interface::fetchlog";
    std::string reply;
    std::stringstream message;
    long  retval;

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->archon_cmd(FETCHLOG, reply)) != NO_ERROR ) {          // send command here
        logwrite( function, "ERROR: calling FETCHLOG" );
        return retval;
      }
      if (reply != "(null)") {
        try {
            reply.erase(reply.find('\n'), 1);
        } catch(...) { }             // strip newline
        logwrite(function, reply);                                           // log reply here
      }
    } while (reply != "(null)");                                             // stop when reply is (null)

    return retval;
  }
  /**************** Archon::Interface::fetchlog *******************************/


  /**************** Archon::Interface::load_timing ****************************/
  /**
   * @fn     load_timing
   * @brief  loads the ACF file and applies the timing script and parameters only
   * @param  acffile, specified ACF to load
   * @param  retstring, reference to string for return values // TODO not yet implemented
   * @return 
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command 
   * which parses and compiles only the timing script and parameters.  
   *
   */
  long Interface::load_timing(std::string acffile, std::string &retstring) {
    return( this->load_timing( acffile ) );
  }
  long Interface::load_timing(std::string acffile) {
    std::string function = "Archon::Interface::load_timing";

    // load the ACF file into configuration memory
    //
    long error = this->load_acf( acffile );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->archon_cmd(LOADTIMING);

    return error;
  }
  /**************** Archon::Interface::load_timing ****************************/


  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief  loads the ACF file and applies the complete system configuration
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version takes a single argument for the acf file to load.
   *
   * This function loads the ACF file and then sends an APPLYALL which
   * parses and applies the complete system configuration from the 
   * configuration memory just loaded. The detector power will be off.
   *
   */
  long Interface::load_firmware(std::string acffile) {
    // load the ACF file into configuration memory
    //
    long error = this->load_acf( acffile );

    // Parse and apply the complete system configuration from configuration memory.
    // Detector power will be off after this.
    //
    if (error == NO_ERROR) error = this->archon_cmd(APPLYALL);

    if ( error != NO_ERROR ) this->fetchlog();

    // If no errors then automatically set the mode to DEFAULT.
    // This should come after APPLYALL in case any new parameters need to be written,
    // which shouldn't be done until after they have been applied.
    //
    if ( error == NO_ERROR ) error = this->set_camera_mode( std::string( "DEFAULT" ) );

    return error;
  }
  /**************** Archon::Interface::load_firmware **************************/
  /**
   * @fn     load_firmware
   * @brief
   * @param  none
   * @return 
   *
   * This function is overloaded.
   *
   * This version is for future compatibility.
   * The multiple-controller version will pass a reference to a return string.
   *
   */
  long Interface::load_firmware(std::string acffile, std::string &retstring) {
    return( this->load_firmware( acffile ) );
  }
  /**************** Archon::Interface::load_firmware **************************/


  /**************** Archon::Interface::load_acf *******************************/
  /**
   * @fn     load_acf
   * @brief  loads the ACF file into configuration memory (no APPLY!)
   * @param  acffile
   * @return ERROR or NO_ERROR
   *
   * This function loads the specfied file into configuration memory.
   * While the ACF is being read, an internal database (STL map) is being 
   * created to allow lookup access to the ACF file or parameters.
   *
   * The [MODE_XXX] sections are also parsed and parameters as a function
   * of mode are saved in their own database.
   *
   * This function only loads (WCONFIGxxx) the configuration memory; it does
   * not apply it to the system. Therefore, this function must be followed
   * with a LOADTIMING or APPLYALL command, for example.
   *
   */
  long Interface::load_acf(std::string acffile) {
    const std::string function("Archon::Interface::load_acf");
    std::stringstream message;
    std::fstream filestream;  // I/O stream class
    std::string line;         // the line read from the acffile
    std::string mode;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::stringstream sscmd;
    std::string key, value;

    int      linecount;  // the Archon configuration line number is required for writing back to config memory
    long     error=NO_ERROR;
    bool     parse_config=false;

    // get the acf filename, either passed here or from loaded default
    //
    if ( acffile.empty() ) {
      acffile = this->camera.firmware[0];

    } else {
      this->camera.firmware[0] = acffile;
    }

    // try to open the file
    //
    try {
      filestream.open(acffile, std::ios_base::in);

    } catch(...) {
      message << "opening acf file " << acffile << ": " << std::strerror(errno);
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    if ( ! filestream.is_open() || ! filestream.good() ) {
      message << "acf file " << acffile << " could not be opened";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    logwrite(function, acffile);

    // The CPU in Archon is single threaded, so it checks for a network 
    // command, then does some background polling (reading bias voltages etc.),
    // then checks again for a network command.  "POLLOFF" disables this 
    // background checking, so network command responses are very fast.  
    // The downside is that bias voltages, temperatures, etc. are not updated
    // until you give a "POLLON". 
    //
    error = this->archon_cmd(POLLOFF);

    // clear configuration memory for this controller
    //
    if (error == NO_ERROR) error = this->archon_cmd(CLEARCONFIG);

    if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: could not prepare Archon for new ACF" );
        return error;
    }

    // Any failure after clearing the configuration memory will mean
    // no firmware is loaded.
    //
    this->firmwareloaded = false;

    modemap.clear();                             // file is open, clear all modes

    linecount = 0;                               // init Archon configuration line number

    while ( getline(filestream, line) ) {        // note that getline discards the newline "\n" character

      // don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
      //
      if (line == "[CONFIG]") { parse_config=true;  continue; }
      if (line == "\n"      ) { parse_config=false; continue; }
      if (line == "[SYSTEM]") { parse_config=false; continue; }

      std::string savedline = line;              // save un-edited line for error reporting

      // parse mode sections, looking for "[MODE_xxxxx]"
      //
      if (line.substr(0,6)=="[MODE_") {          // this is a mode section
        try {
          line.erase(line.find('['), 1);         // erase the opening square bracket
          line.erase(line.find(']'), 1);         // erase the closing square bracket

        } catch(...) {
          message.str(""); message << "malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          this->camera.log_error( function, message.str() );
          filestream.close();
          return ERROR;
        }
        if ( ! line.empty() ) {                  // What's remaining should be MODE_xxxxx
          mode = line.substr(5);                 // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

          // got a mode, now check if one of this name has already been located
          // and put into the modemap
          //
          if ( this->modemap.find(mode) != this->modemap.end() ) {
            message.str(""); message << "duplicate definition of mode: " << mode << ": load aborted";
            this->camera.log_error( function, message.str() );
	        filestream.close();
            return ERROR;

          } else {
            parse_config = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
                                                 // this also ensures something is saved in the modemap for this mode
          }

        } else {                                   // somehow there's no xxx left after removing "[MODE_" and "]"
          message.str(""); message << "malformed mode section: " << savedline << ": expected [MODE_xxxx]";
          this->camera.log_error( function, message.str() );
          filestream.close();
          return ERROR;
        }
      }

      // Everything else is for parsing configuration lines so if we didn't get [CONFIG] then
      // skip to the next line.
      //
      if (!parse_config) continue;

      // replace any TAB characters with a space
      //
      string_replace_char(line, "\t", " ");

      // replace any backslash characters with a forward slash
      //
      string_replace_char(line, "\\", "/");

      // erase all quotes
      try {
          line.erase( std::remove(line.begin(), line.end(), '"'), line.end() );
      } catch(...) { }

      // Initialize key, value strings used to form WCONFIG KEY=VALUE command.
      // As long as key stays empty then the WCONFIG command is not written to the Archon.
      // This is what keeps TAGS: in the [MODE_xxxx] sections from being written to Archon,
      // because these do not populate key.
      //
      key="";
      value="";

      //  ************************************************************
      // Store actual Archon parameters in their own STL map IN ADDITION to the map
      // in which all other keywords are store, so that they can be accessed in
      // a different way.  Archon PARAMETER KEY=VALUE paris are formatted as:
      // PARAMETERn=ParameterName=value
      // where "PARAMETERn" is the key and "ParameterName=value" is the value.
      // However, it is logical to access them by ParameterName only. That is what the
      // parammap is for, hence the need for this STL map indexed on only the "ParameterName"
      // portion of the value. Conversely, the configmap is indexed by the key.
      // 
      // parammap stores ONLY the parameters, which are identified as PARAMETERxx="paramname=value"
      // configmap stores every configuration line sent to Archon (which includes parameters)
      //
      // In order to modify these keywords in Archon, the entire above phrase
      // (KEY=VALUE pair) must be preserved along with the line number on which it 
      // occurs in the config file.
      // ************************************************************

      // Look for TAGS: in the .acf file mode sections
      //
      // If tag is "ACF:" then it's a .acf line (could be a parameter or configuration)
      //
      if (line.compare(0,4,"ACF:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(4);              // strip off the "ACF:" portion
        std::string acf_key, acf_value;

        try {
          Tokenize(line, tokens, "="); // separate into tokens by "="

          if (tokens.size() == 1) {                    // KEY=, the VALUE is empty
            acf_key   = tokens[0];
            acf_value = "";

          } else if (tokens.size() == 2) {             // KEY=VALUE
                acf_key   = tokens[0];
                acf_value = tokens[1];

          } else {
                message.str(""); message << "malformed ACF line: " << savedline << ": expected KEY=VALUE";
                this->camera.log_error( function, message.str() );
	            filestream.close();
                return ERROR;
          }

          bool keymatch = false;

          // If this key is in the main parammap then store it in the modemap's parammap for this mode
          if (this->parammap.find( acf_key ) != this->parammap.end()) {
            this->modemap[mode].parammap[ acf_key ].name  = acf_key;
            this->modemap[mode].parammap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is in the main configmap, then store it in the modemap's configmap for this mode
          //
          if (this->configmap.find( acf_key ) != this->configmap.end()) {
            this->modemap[mode].configmap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is neither in the parammap nor in the configmap then return an error
          //
          if ( ! keymatch ) {
            message.str("");
            message << "[MODE_" << mode << "] ACF directive: " << acf_key << "=" << acf_value << " is not a valid parameter or configuration key";
            logwrite(function, message.str());
            filestream.close();
            return ERROR;
          }

        } catch ( ... ) {
          message.str(""); message << "extracting KEY=VALUE pair from ACF line: " << savedline;
          this->camera.log_error( function, message.str() );
          filestream.close();
          return ERROR;
        }
        // end if (line.compare(0,4,"ACF:")==0)

      } else if (line.compare(0,5,"ARCH:")==0) {
          // The "ARCH:" tag is for internal (Archon_interface) variables
          // using the KEY=VALUE format.
          //
          std::vector<std::string> tokens;
          line = line.substr(5);                                            // strip off the "ARCH:" portion
          Tokenize(line, tokens, "=");                                      // separate into KEY, VALUE tokens
          if (tokens.size() != 2) {
            message.str(""); message << "malformed ARCH line: " << savedline << ": expected ARCH:KEY=VALUE";
            this->camera.log_error( function, message.str() );
	        filestream.close();
            return ERROR;
          }
          if ( tokens[0] == "NUM_DETECT" ) {
            this->modemap[mode].geometry.num_detect = std::stoi( tokens[1] );

          } else if ( tokens[0] == "HORI_AMPS" ) {
            this->modemap[mode].geometry.amps[0] = std::stoi( tokens[1] );

          } else if ( tokens[0] == "VERT_AMPS" ) {
            this->modemap[mode].geometry.amps[1] = std::stoi( tokens[1] );

          } else {
            message.str(""); message << "unrecognized internal parameter specified: "<< tokens[0];
            this->camera.log_error( function, message.str() );
	        filestream.close();
            return ERROR;
          }
          // end else if (line.compare(0,5,"ARCH:")==0)

      } else if (line.compare(0,5,"FITS:")==0) {
          // the "FITS:" tag is used to write custom keyword entries of the form "FITS:KEYWORD=VALUE/COMMENT"
          //
          std::vector<std::string> tokens;
          line = line.substr(5);           // strip off the "FITS:" portion

          // First, tokenize on the equal sign "=".
          // The token left of "=" is the keyword. Immediate right is the value
          Tokenize(line, tokens, "=");
          if (tokens.size() != 2) {    // need at least two tokens at this point
            message.str(""); message << "malformed FITS command: " << savedline << ": expected KEYWORD=value/comment";
            this->camera.log_error( function, message.str() );
            filestream.close();
            return ERROR;
          }
          keyword   = tokens[0].substr(0,8); // truncate keyword to 8 characters
          keystring = tokens[1];                    // tokenize the rest in a moment
          keycomment = "";                          // initialize comment, assumed empty unless specified below

          // Next, tokenize on the slash "/".
          // The token left of "/" is the value. Anything to the right is a comment.
          //
          Tokenize(keystring, tokens, "/");

          if (tokens.empty()) {          // no tokens found means no "/" characeter which means no comment
            keyvalue = keystring;        // therefore the keyvalue is the entire string
          }

          if (not tokens.empty()) {         // at least one token
            keyvalue = tokens[0];
          }

          if (tokens.size() == 2) {      // If there are two tokens here then the second is a comment
            keycomment = tokens[1];
          }

          if (tokens.size() > 2) {       // everything below this has been covered
            message.str(""); message << "malformed FITS command: " << savedline << ": expected KEYWORD=VALUE/COMMENT";
            this->camera.log_error( function, message.str() );
            message.str(""); message << "too many \"/\" in comment string? " << keystring;
            this->camera.log_error( function, message.str() );
            filestream.close();
            return ERROR;
          }

          // Save all the user keyword information in a map for later
          this->modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
          this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->camera_info.userkeys.get_keytype(keyvalue);
          this->modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
          this->modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
          // end if (line.compare(0,5,"FITS:")==0)
          //
          // ----- all done looking for "TAGS:" -----
          //

      } else if ( (line.compare(0,11,"PARAMETERS=")!=0) &&   // not the "PARAMETERS=xx line
            (line.compare(0, 9,"PARAMETER"  )==0) ) {  // but must start with "PARAMETER"
          // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
          //

          std::vector<std::string> tokens;
          Tokenize(line, tokens, "=");                  // separate into PARAMETERn, ParameterName, value tokens

          if (tokens.size() != 3) {
            message.str(""); message << "malformed paramter line: " << savedline << ": expected PARAMETERn=Param=value";
            this->camera.log_error( function, message.str() );
            filestream.close();
            return ERROR;
          }

          // Tokenize broke everything up at the "=" and
          // we need all three parts, but we also need a version containing the last
          // two parts together, "ParameterName=value" so re-assemble them here.
          //
          std::stringstream paramnamevalue;
          paramnamevalue << tokens[1] << "=" << tokens[2];             // reassemble ParameterName=value string

          // build an STL map "configmap" indexed on PARAMETERn, the part before the first "=" sign
          //
          this->configmap[ tokens[0] ].line  = linecount;              // configuration line number
          this->configmap[ tokens[0] ].value = paramnamevalue.str();   // configuration value for PARAMETERn

          // build an STL map "parammap" indexed on ParameterName so that we can look up by the actual name
          //
          this->parammap[ tokens[1] ].key   = tokens[0];          // PARAMETERn
          this->parammap[ tokens[1] ].name  = tokens[1] ;         // ParameterName
          this->parammap[ tokens[1] ].value = tokens[2];          // value
          this->parammap[ tokens[1] ].line  = linecount;          // line number

          // assemble a KEY=VALUE pair used to form the WCONFIG command
          key   = tokens[0];                                      // PARAMETERn
          value = paramnamevalue.str();                           // ParameterName=value
          // end If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...

      } else {
          // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
          // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
          // only one token is made
          //
          std::vector<std::string> tokens;
          // Tokenize will return a size=1 even if there are no delimiters,
          // so work around this by first checking for delimiters
          // before calling Tokenize.
          //
          if (line.find_first_of('=', 0) == std::string::npos) {
            continue;
          }

          Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
          if (tokens.empty()) {
            continue;                                             // nothing to do here if no tokens (ie no "=")
          }

          key = tokens[0];                                        // not empty so at least one token is the KEY
          value.clear();                                          // VALUE can be empty (e.g. labels not required)

          if ( tokens.size() > 1 ) {                              // at least one more token is the value
            value = tokens[1];                                    // VALUE (there is a second token)
          }

          if ( tokens.size() > 2 ) {                              // more tokens are possible
            for ( size_t i=2; i<tokens.size(); ++i ) {            // loop through remaining tokens
              value += "=" + tokens[i];                           // and put them back together as the VALUE
            }
          }

          this->configmap[ tokens[0] ].line  = linecount;
          this->configmap[ tokens[0] ].value = value;
      } // end else

      // Form the WCONFIG command to Archon and
      // write the config line to the controller memory (if key is not empty).
      //
      if ( !key.empty() ) {                                     // value can be empty but key cannot
        sscmd.str("");
        sscmd << "WCONFIG"
              << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
              << linecount
              << key << "=" << value << "\n";
        // send the WCONFIG command here
        if (error == NO_ERROR) error = this->archon_cmd(sscmd.str());
        linecount++;
      } // end if ( !key.empty() && !value.empty() )
    } // end while ( getline(filestream, line) )

    this->configlines = linecount;  // save the number of configuration lines

    // re-enable background polling
    //
    if (error == NO_ERROR) error = this->archon_cmd(POLLON);

    filestream.close();
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon config file OK");
      this->firmwareloaded = true;

      // add firmware filename and checksum to systemkeys keyword database
      //
      this->systemkeys.addkey( "FIRMWARE=" + acffile + "// controller firmware" );

      std::string hash;
      md5_file( acffile, hash );
      this->systemkeys.addkey( "FIRM_MD5=" + hash + "// MD5 checksum of firmware" );
    }

    // If there was an Archon error then read the Archon error log
    //
    if (error != NO_ERROR) error = this->fetchlog();

    this->modeselected = false;           // require that a mode be selected after loading new firmware

    return error;
  }
  /**************** Archon::Interface::load_acf *******************************/


  /**************** Archon::Interface::set_camera_mode ************************/
  /**
   * @fn     set_camera_mode
   * @brief  
   * @param  none
   * @return 
   *
   */
  long Interface::set_camera_mode(std::string mode) {
    std::string function = "Archon::Interface::set_camera_mode";
    std::stringstream message;
    bool configchanged = false;
    bool paramchanged = false;
    long error;

    // No point in trying anything if no firmware has been loaded yet
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "no firmware loaded" );
      return ERROR;
    }

    std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

    // The requested mode must have been read in the current ACF file
    // and put into the modemap...
    //
    if (this->modemap.find(mode) == this->modemap.end()) {
      message.str(""); message << "undefined mode " << mode << " in ACF file " << this->camera.firmware[0];
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // load specific mode settings from .acf and apply to Archon
    //
    if ( load_mode_settings(mode) != NO_ERROR) {
      message.str(""); message << "ERROR: failed to load mode settings for mode: " << mode;
      logwrite( function, message.str() );
      return ERROR;
    }

    // set internal variables based on new .acf values loaded
    //
    error = NO_ERROR;
    if (error==NO_ERROR) error = get_configmap_value("FRAMEMODE", this->modemap[mode].geometry.framemode);
    if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->modemap[mode].geometry.linecount);
    if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->modemap[mode].geometry.pixelcount);
    if (error==NO_ERROR) error = get_configmap_value("RAWENABLE", this->modemap[mode].rawenable);
    if (error==NO_ERROR) error = get_configmap_value("RAWSEL", this->rawinfo.adchan);
    if (error==NO_ERROR) error = get_configmap_value("RAWSAMPLES", this->rawinfo.rawsamples);
    if (error==NO_ERROR) error = get_configmap_value("RAWENDLINE", this->rawinfo.rawlines);

    #ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] mode=" << mode << " RAWENABLE=" << this->modemap[mode].rawenable 
            << " RAWSAMPLES=" << this->rawinfo.rawsamples << " RAWLINES=" << this->rawinfo.rawlines;
    logwrite(function, message.str());
    #endif

    // get out if any errors at this point
    //
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: one or more internal variables missing from configmap" ); return error; }

    int num_detect = this->modemap[mode].geometry.num_detect;             // for convenience

    // set current number of Archon buffers and resize local memory
    // get out if an error
    //
    int bigbuf=-1;
    if (error==NO_ERROR) error = get_configmap_value("BIGBUF", bigbuf);   // get value of BIGBUF from loaded acf file
    this->camera_info.activebufs = (bigbuf==1) ? 2 : 3;                   // set number of active buffers based on BIGBUF
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to read BIGBUF from ACF" ); return error; }

    // There is one special reserved mode name, "RAW"
    //
    if (mode=="RAW") {
      this->camera_info.detector_pixels[0] = this->rawinfo.rawsamples;
      this->camera_info.detector_pixels[1] = this->rawinfo.rawlines; 
      this->camera_info.detector_pixels[1]++;
      // frame_type will determine the bits per pixel and where the detector_axes come from
      this->camera_info.frame_type = Camera::FRAME_RAW;
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (RAWSAMPLES) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (RAWENDLINE) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
      #endif

    } else {
        // Any other mode falls under here
      if (error==NO_ERROR) error = get_configmap_value("PIXELCOUNT", this->camera_info.detector_pixels[0]);
      if (error==NO_ERROR) error = get_configmap_value("LINECOUNT", this->camera_info.detector_pixels[1]);
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] mode=" << mode; logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0]
                               << " amps[0] = " << this->modemap[mode].geometry.amps[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1]
                               << " amps[1] = " << this->modemap[mode].geometry.amps[1];
      logwrite(function, message.str());
      #endif
      this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps[0];
      this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps[1];
      this->camera_info.frame_type = Camera::FRAME_IMAGE;
      // ROI is the full detector
      this->camera_info.region_of_interest[0] = 1;
      this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
      this->camera_info.region_of_interest[2] = 1;
      this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];
      // Binning factor (no binning)
      this->camera_info.binning[0] = 1;
      this->camera_info.binning[1] = 1;
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[0] (PIXELCOUNT) = " << this->camera_info.detector_pixels[0];
      logwrite(function, message.str());
      message.str(""); message << "[DEBUG] this->camera_info.detector_pixels[1] (LINECOUNT) = " << this->camera_info.detector_pixels[1];
      logwrite(function, message.str());
      #endif
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to get PIXELCOUNT,LINECOUNT from ACF" ); return error; }
    }

    // set bitpix based on SAMPLEMODE
    //
    int samplemode=-1;
    if (error==NO_ERROR) error = get_configmap_value("SAMPLEMODE", samplemode); // SAMPLEMODE=0 for 16bpp, =1 for 32bpp
    if ( error != NO_ERROR ) { logwrite( function, "ERROR: unable to get SAMPLEMODE from ACF" ); return error; }
    if (samplemode < 0) { this->camera.log_error( function, "bad or missing SAMPLEMODE from ACF" ); return ERROR; }
    this->camera_info.bitpix = (samplemode==0) ? 16 : 32;

    // Load parameters and Apply CDS/Deint configuration if any of them changed
    if ((error == NO_ERROR) && paramchanged)  error = this->archon_cmd(LOADPARAMS);  // TODO I think paramchanged is never set!
    if ((error == NO_ERROR) && configchanged) error = this->archon_cmd(APPLYCDS);    // TODO I think configchanged is never set!

    // Get the current frame buffer status
    if (error == NO_ERROR) error = this->get_frame_status();
    if (error != NO_ERROR) {
      logwrite( function, "ERROR: unable to get frame status" );
      return error;
    }

    // Set axes, image dimensions, calculate image_memory, etc.
    // Raw will always be 16 bpp (USHORT).
    // Image can be 16 or 32 bpp depending on SAMPLEMODE setting in ACF.
    // Call set_axes(datatype) with the FITS data type needed, which will set the info.datatype variable.
    //
    error = this->camera_info.set_axes();                                                 // 16 bit raw is unsigned short int
/*********
    if (this->camera_info.frame_type == Camera::FRAME_RAW) {
      error = this->camera_info.set_axes(USHORT_IMG);                                     // 16 bit raw is unsigned short int
    }
    if (this->camera_info.frame_type == Camera::FRAME_IMAGE) {
      if (this->camera_info.bitpix == 16) error = this->camera_info.set_axes(SHORT_IMG);  // 16 bit image is short int
      else
      if (this->camera_info.bitpix == 32) error = this->camera_info.set_axes(FLOAT_IMG);  // 32 bit image is float
      else {
        message.str(""); message << "bad bitpix " << this->camera_info.bitpix << ": expected 16 | 32";
        this->camera.log_error( function, message.str() );
        return (ERROR);
      }
    }
*********/
    if (error != NO_ERROR) {
      this->camera.log_error( function, "setting axes" );
      return (ERROR);
    }

    // allocate image_data in blocks because the controller outputs data in units of blocks
    //
    this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

    if (this->image_data_bytes == 0) {
      this->camera.log_error( function, "image data size is zero! check NUM_DETECT, HORI_AMPS, VERT_AMPS in .acf file" );
      error = ERROR;
    }

    this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
    this->modeselected = true;                             // a valid mode has been selected

    message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
    logwrite(function, message.str());

    // Calculate amplifier sections
    //
    int rows = this->modemap[mode].geometry.linecount;     // rows per section
    int cols = this->modemap[mode].geometry.pixelcount;    // cols per section

    int hamps = this->modemap[mode].geometry.amps[0];      // horizontal amplifiers
    int vamps = this->modemap[mode].geometry.amps[1];      // vertical amplifiers

    int x0=-1, x1, y0, y1;                                 // for indexing
    std::vector<long> coords;                              // vector of coordinates, convention is x0,x1,y0,y1
    int framemode = this->modemap[mode].geometry.framemode;

    this->camera_info.amp_section.clear();                 // vector of coords vectors, one set of coords per extension

    for ( int y=0; y<vamps; y++ ) {
      for ( int x=0; x<hamps; x++ ) {
        if ( framemode == 2 ) {
          x0 = x; x1=x+1;
          y0 = y; y1=y+1;

        } else {
          x0++;   x1=x0+1;
          y0 = 0; y1=1;
        }
        coords.clear();
        coords.push_back( (x0*cols + 1) );                 // x0 is xstart
        coords.push_back( (x1)*cols );                     // x1 is xstop, xrange = x0:x1
        coords.push_back( (y0*rows + 1) );                 // y0 is ystart
        coords.push_back( (y1)*rows );                     // y1 is ystop, yrange = y0:y1

        this->camera_info.amp_section.push_back( coords ); // (x0,x1,y0,y1) for this extension

      }
    }
    message.str(""); message << "identified " << this->camera_info.amp_section.size() << " amplifier sections";
    logwrite( function, message.str() );

    #ifdef LOGLEVEL_DEBUG
    int ext=0;
    for ( const auto &sec : this->camera_info.amp_section ) {
      message.str(""); message << "[DEBUG] extension " << ext++ << ":";
      for ( const auto &xy : sec ) {
        message << " " << xy;
      }
      logwrite( function, message.str() );
    }
    #endif

    return error;
  }
  /**************** Archon::Interface::set_camera_mode ************************/


  /**************** Archon::Interface::load_mode_settings *********************/
  /**
   * @fn     load_mode_settings
   * @brief  load into Archon settings specified in mode section of .acf file
   * @param  camera mode
   * @return none
   *
   * At the end of the .acf file are optional sections for each camera
   * observing mode. These sections can contain any number of configuration
   * lines and parameters to set for the given mode. Those lines are read
   * when the configuration file is loaded. This function writes them to
   * the Archon controller.
   */
  long Interface::load_mode_settings(std::string mode) {
    std::string function = "Archon::Interface::load_mode_settings";
    std::stringstream message;

    long error=NO_ERROR;
    cfg_map_t::iterator   cfg_it;
    param_map_t::iterator param_it;
    bool paramchanged  = false;
    bool configchanged = false;

    std::stringstream errstr;

    /**
     * iterate through configmap, writing each config key in the map
     */
    for (cfg_it  = this->modemap[mode].configmap.begin();
         cfg_it != this->modemap[mode].configmap.end();
         cfg_it++) {
      error = this->write_config_key( cfg_it->first.c_str(), cfg_it->second.value.c_str(), configchanged );
      if (error != NO_ERROR) {
        errstr  << "ERROR: writing config key:" << cfg_it->first << " value:" << cfg_it->second.value << " for mode " << mode;
        break;
      }
    }

    /**
     * if no errors from writing config keys, then
     * iterate through the parammap, writing each parameter in the map
     */
    if (error == NO_ERROR) {
      for (param_it  = this->modemap[mode].parammap.begin();
           param_it != this->modemap[mode].parammap.end();
           param_it++) {
        error = this->write_parameter( param_it->first.c_str(), param_it->second.value.c_str(), paramchanged );
        message.str(""); message << "paramchanged=" << (paramchanged?"true":"false");
        logwrite(function, message.str());
        if (error != NO_ERROR) {
          errstr  << "ERROR: writing parameter key:" << param_it->first << " value:" << param_it->second.value << " for mode " << mode;
          break;
        }
      }
    }

    /**
     * apply the new settings to the system here, only if something changed
     */
    if ( (error == NO_ERROR) && paramchanged  ) error = this->archon_cmd(LOADPARAMS);
    if ( (error == NO_ERROR) && configchanged ) error = this->archon_cmd(APPLYCDS);

    if (error == NO_ERROR) {
      message.str(""); message << "loaded mode: " << mode;
      logwrite(function, message.str());

    } else {
      logwrite( function, errstr.str() );
      return error;
    }

    // The new mode could contain a ShutterEnable param,
    // and if it does then the server needs to know about that.
    //
    if ( !this->shutenableparam.empty() ) {

      // first read the parameter
      //
      std::string lshutten;
      if ( read_parameter( this->shutenableparam, lshutten ) != NO_ERROR ) { 
        message.str(""); message << "ERROR: reading \"" << this->shutenableparam << "\" parameter from Archon";
        logwrite( function, message.str() );
        return ERROR;
      }

      // parse the parameter value
      // and convert it to a string for the shutter command
      //
      std::string shuttenstr;
      if ( lshutten == "1" ) shuttenstr = "enable";
      else if ( lshutten == "0" ) shuttenstr = "disable";
      else {
        message.str(""); message << "ERROR: unrecognized shutter enable parameter value " << lshutten << ": expected {0,1}";
        logwrite( function, message.str() );
        return ERROR;
      }

      // Tell the server
      //
      std::string dontcare;
      if ( this->shutter( shuttenstr, dontcare ) != NO_ERROR ) { logwrite( function, "ERROR: setting shutter enable parameter" ); return ERROR; }
    }

    /**
     * read back some TAPLINE information
     */
    if (error==NO_ERROR) error = get_configmap_value("TAPLINES", this->taplines); // total number of taps

    std::vector<std::string> tokens;
    std::stringstream        tap;
    std::string              adchan;

    // Remove all GAIN* and OFFSET* keywords from the systemkeys database
    // because the new mode could have different channels (so simply 
    // over-writing existing keys might not be sufficient). 
    //
    // The new GAIN* and OFFSET* system keys will be added in the next loop.
    //
    this->systemkeys.EraseKeys( "GAIN" );
    this->systemkeys.EraseKeys( "OFFSET" );

    // Loop through every tap to get the offset for each
    //
    for (int tapn=0; tapn<this->taplines; tapn++) {
      tap.str("");
      tap << "TAPLINE" << tapn;  // used to find the tapline in the configmap

      // The value of TAPLINEn = ADxx,gain,offset --
      // tokenize by comma to separate out each parameter...
      //
      Tokenize(this->configmap[tap.str().c_str()].value, tokens, ",");

      // If all three tokens present (A?xx,gain,offset) then parse it,
      // otherwise it's an unused tap, and we can skip it.
      //
      if (tokens.size() == 3) { // defined tap has three tokens
        adchan = tokens[0];     // AD channel is the first (0th) token
        char chars[] = "ADMLR"; // characters to remove in order to get just the AD channel number

        // Before removing these characters, set the max allowed AD number based on the tapline syntax.
        // "ADxx,gain,offset" is for ADC module
        // "AMxx,gain,offset" is for ADM module
        //
        int admax = 0;
        if ( adchan.find("AD") != std::string::npos ) admax = MAXADCCHANS;
        else if ( adchan.find("AM") != std::string::npos ) admax = MAXADMCHANS;
        else {
          message.str(""); message << "bad tapline syntax. Expected ADn or AMn but got " << adchan;
          this->camera.log_error( function, message.str() );
          return ERROR;
        }

        // remove AD, AM, L, R from the adchan string, to get just the AD channel number
        //
        for (unsigned int j = 0; j < strlen(chars); j++) {
          adchan.erase(std::remove(adchan.begin(), adchan.end(), chars[j]), adchan.end());
        }

        // AD# in TAPLINE is 1-based (numbered 1,2,3,...)
        // but convert here to 0-based (numbered 0,1,2,...) and check value before using
        //
        int adnum;
        try {
          adnum = std::stoi(adchan) - 1;

        } catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert AD number \'" << adchan << "\' to integer";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          this->camera.log_error( function, "AD number out of integer range" );
          return ERROR;
        }

        if ( (adnum < 0) || (adnum > admax) ) {
          message.str(""); message << "ADC channel " << adnum << " outside range {0:" << admax << "}";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        // Now that adnum is OK, convert next two tokens to gain, offset
        //
        int gain_try=0, offset_try=0;
        try {
          gain_try   = std::stoi( tokens[1] );      // gain as function of AD channel
          offset_try = std::stoi( tokens[2] );      // offset as function of AD channel

        } catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert GAIN \"" << tokens[1] << "\" and/or OFFSET \"" << tokens[2] << "\" to integer";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "GAIN " << tokens[1] << ", OFFSET " << tokens[2] << " outside integer range";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        // Now assign the gain,offsets to their appropriate position in the vectors
        //
        try {
          this->gain.at( adnum )   = gain_try;      // gain as function of AD channel
          this->offset.at( adnum ) = offset_try;    // offset as function of AD channel

          // Add the gain/offset as system header keywords.
          //
          std::stringstream keystr;
          keystr.str(""); keystr << "GAIN" << std::setfill('0') << std::setw(2) << adnum << "=" << this->gain.at( adnum ) << "// gain for AD chan " << adnum;
          this->systemkeys.addkey( keystr.str() );
          keystr.str(""); keystr << "OFFSET" << std::setfill('0') << std::setw(2) << adnum << "=" << this->offset.at( adnum ) << "// offset for AD chan " << adnum;
          this->systemkeys.addkey( keystr.str() );

        } catch ( std::out_of_range & ) {
          message.str(""); message << "AD# " << adnum << " outside range {0:" << (this->gain.size() & this->offset.size()) << "}";
          this->camera.log_error( function, message.str() );
          if ( this->gain.empty() || this->offset.empty() ) {
            this->camera.log_error( function, "gain/offset vectors are empty: no ADC or ADM board installed?" );
          }
          return ERROR;
        }
        #ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] tap #" << tapn << " ad#" << adnum;
        logwrite( function, message.str() );
        #endif
      } // end if (tokens.size() == 3)
    }   // end for (int tapn=0; tapn<this->taplines; tapn++)

    return error;
  }
  /**************** Archon::Interface::load_mode_settings *********************/


  /**************** Archon::Interface::get_frame_status ***********************/
  /**
   * @fn     get_frame_status
   * @brief  get the current frame buffer status
   * @param  none
   * @return 
   *
   * Sends the "FRAME" command to Archon, reads back the reply, then parses the
   * reply and stores parameters into the frame structure (of type frame_data_t).
   *
   */
  long Interface::get_frame_status() {
    std::string function = "Archon::Interface::get_frame_status";
    std::string reply;
    std::stringstream message;
    int   newestframe, newestbuf;
    long  error=NO_ERROR;

    // send FRAME command to get frame buffer status
    //

    if (this->is_autofetch) {
      logwrite( function, "AUTOFETCH MODE: not sending FRAME command");
    } else {
      if ( (error = this->archon_cmd(FRAME, reply)) ) {
        if ( error == ERROR ) logwrite( function, "ERROR: sending FRAME command" );  // don't log here if BUSY
        return error;
      }
    }

    // First Tokenize breaks the single, continuous reply string into vector of individual strings,
    // from "TIMER=xxxx RBUF=xxxx " to:
    //   tokens[0] : TIMER=xxxx
    //   tokens[1] : RBUF=xxxx
    //   tokens[2] : etc.
    std::vector<std::string> tokens;
    Tokenize(reply, tokens, " ");

    // loop over all tokens in reply
    for (const auto & token : tokens) {

      // Second Tokenize separates the parameter from the value
      // subtokens[0] = keyword
      // subtokens[1] = value
      std::vector<std::string> subtokens;
      subtokens.clear();
      Tokenize(token, subtokens, "=");

      // Each entry in the FRAME message must have two tokens, one for each side of the "=" equal sign
      // (in other words there must be two subtokens per token)
      if (subtokens.size() != 2) {
        message.str("");
        message << "expected 2 but received invalid number of tokens (" << subtokens.size() << ") in FRAME message:";
        for (const auto & subtoken : subtokens) message << " " << subtoken;
        this->camera.log_error( function, message.str() );
        return ERROR;  // We could continue; but if one is bad then we could miss seeing a larger problem
      }

      int bufnum=0;
      int value=0;
      uint64_t lvalue=0;

      // Parse subtokens[1] (value) based on subtoken[0] (keyword)

      // timer is a string
      if (subtokens[0]=="TIMER") {
          this->frame.timer = subtokens[1];

      } else {
          // everything else is going to be a number
          // use "try...catch" to catch exceptions converting strings to numbers
        try {
            // for all "BUFnSOMETHING=VALUE" we want the bufnum "n"
          if (subtokens[0].compare(0, 3, "BUF")==0) {
              // extract the "n" here which is 1-based (1,2,3)
              bufnum = std::stoi( subtokens[0].substr(3, 1) );
          }

            // for "BUFnBASE=xxx" the value is uint64
          if (subtokens[0].substr(4)=="BASE" ) {
            lvalue  = std::stol( subtokens[1] );   // this value will get assigned to the corresponding parameter

            // for any "xxxTIMESTAMPxxx" the value is uint64
          } else if (subtokens[0].find("TIMESTAMP") != std::string::npos) {
            lvalue  = std::stol( subtokens[1] );   // this value will get assigned to the corresponding parameter

            // everything else is an int
          } else {
              value  = std::stoi( subtokens[1] );  // this value will get assigned to the corresponding parameter
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "unable to convert buffer: " << subtokens[0] << " or value: " << subtokens[1] << " from FRAME message to integer. Expected BUFnSOMETHING=nnnn";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "buffer: " << subtokens[0] << " or value: " << subtokens[1] << " from FRAME message outside integer range. Expected BUFnSOMETHING=nnnn";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
      } // end else everything else is a number

      // subtokens value has been parsed based on keyword

      // get currently locked buffers
      if (subtokens[0]=="RBUF")  this->frame.rbuf  = value; // locked for reading
      if (subtokens[0]=="WBUF")  this->frame.wbuf  = value; // locked for writing

      // The next group are BUFnSOMETHING=VALUE
      // Extract the "n" which must be a number from 1 to Archon::nbufs
      // After getting the buffer number we assign the corresponding value.
      //
      if (subtokens[0].compare(0, 3, "BUF")==0) {
          bufnum = std::stoi( subtokens[0].substr(3, 1) );
          // verify buffer number (1,2, or 3)
          if (bufnum < 1 || bufnum > Archon::nbufs) {
          message.str(""); message << "buffer number " << bufnum << " from FRAME message outside range {1:" << Archon::nbufs << "}";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        bufnum--;   // subtract 1 because it is 1-based in the message but need 0-based for the indexing
        // Assign value to appropriate variable in frame structure
        if (subtokens[0].substr(4) == "SAMPLE")      this->frame.bufsample[bufnum]      =  value;
        if (subtokens[0].substr(4) == "COMPLETE")    this->frame.bufcomplete[bufnum]    =  value;
        if (subtokens[0].substr(4) == "MODE")        this->frame.bufmode[bufnum]        =  value;
        if (subtokens[0].substr(4) == "BASE")        this->frame.bufbase[bufnum]        = lvalue;
        if (subtokens[0].substr(4) == "FRAME")       this->frame.bufframen[bufnum]      =  value;
        if (subtokens[0].substr(4) == "WIDTH")       this->frame.bufwidth[bufnum]       =  value;
        if (subtokens[0].substr(4) == "HEIGHT")      this->frame.bufheight[bufnum]      =  value;
        if (subtokens[0].substr(4) == "PIXELS")      this->frame.bufpixels[bufnum]      =  value;
        if (subtokens[0].substr(4) == "LINES")       this->frame.buflines[bufnum]       =  value;
        if (subtokens[0].substr(4) == "RAWBLOCKS")   this->frame.bufrawblocks[bufnum]   =  value;
        if (subtokens[0].substr(4) == "RAWLINES")    this->frame.bufrawlines[bufnum]    =  value;
        if (subtokens[0].substr(4) == "RAWOFFSET")   this->frame.bufrawoffset[bufnum]   =  value;
        if (subtokens[0].substr(4) == "TIMESTAMP")   this->frame.buftimestamp[bufnum]   = lvalue;
        if (subtokens[0].substr(4) == "RETIMESTAMP") this->frame.bufretimestamp[bufnum] = lvalue;
        if (subtokens[0].substr(4) == "FETIMESTAMP") this->frame.buffetimestamp[bufnum] = lvalue;
      } // end if token is BUFnSOMETHiNG

    }   // end loop over all returned tokens from FRAME reply

    // FRAME reply has now been parsed into frame structure variables
    // now update frame.frame, frame.index, and frame.next_index

    // Current frame.index
    newestbuf   = this->frame.index;

    // Is frame.index a legal index?
    if (this->frame.index < (int)this->frame.bufframen.size()) {
        // get the current frame number
        newestframe = this->frame.bufframen[this->frame.index];

    } else {
        // frame.index value is illegal
        message.str(""); message << "newest buf " << this->frame.index << " from FRAME message exceeds number of buffers " << this->frame.bufframen.size();
        this->camera.log_error( function, message.str() );
        return ERROR;
    }

    int num_zero = 0;   // count zero buffers

    // loop through the buffers
    for (int bc=0; bc<Archon::nbufs; bc++) {

      // count number of zero-valued buffers
      if ( this->frame.bufframen[bc] == 0 ) num_zero++;

      // Is the latest frame greater than the current one and is it complete?
      if ( (this->frame.bufframen[bc] > newestframe) && this->frame.bufcomplete[bc] ) {
          // if so, update frame and buffer numbers
          newestframe = this->frame.bufframen[bc];
          newestbuf   = bc;
      }
    }

    // In start-up case, all frame buffers are zero
    if (num_zero == Archon::nbufs) {
        // initialize frame number and buffer index both to zero
        newestframe = 0;
        newestbuf   = 0;
    }

    // save the newest frame number and index of newest buffer.
    this->frame.frame = newestframe;
    this->frame.index = newestbuf;

    // startup condition for next_frame:
    // frame number is 1 and corresponding buffer is not complete
    if ( ( this->frame.bufframen[ this->frame.index ] ) == 1 && this->frame.bufcomplete[ this->frame.index ] == 0 ) {
          this->frame.next_index = 0;

    } else {
        // Index of next frame is this->frame.index+1
        this->frame.next_index = this->frame.index + 1;

        // frame.next_index wraps to 0 when it reaches the maximum number of active buffers.
        if (this->frame.next_index >= this->camera_info.activebufs) {
            this->frame.next_index = 0;
        }
    }

    return error;
  }
  /**************** Archon::Interface::get_frame_status ***********************/


  /**************** Archon::Interface::print_frame_status *********************/
  /**
   * @fn     print_frame_status
   * @brief  print the Archon frame buffer status
   * @param  none
   * @return none
   *
   * Writes the Archon frame buffer status to the log file,
   * I.E. information in this->frame structure, obtained from
   * the "FRAME" command. See Archon::Interface::get_frame_status()
   *
   */
  void Interface::print_frame_status() {
    std::string function = "Archon::Interface::print_frame_status";
    std::stringstream message;
    int bufn;
    char statestr[Archon::nbufs][4];

    // write as log message
    //
    message.str(""); message << "    buf base       rawoff     frame ready lines rawlines rblks width height state";
    logwrite(function, message.str());
    message.str(""); message << "    --- ---------- ---------- ----- ----- ----- -------- ----- ----- ------ -----";
    logwrite(function, message.str());
    message.str("");
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      memset(statestr[bufn], '\0', 4);
      if ( (this->frame.rbuf-1) == bufn)   strcat(statestr[bufn], "R");
      if ( (this->frame.wbuf-1) == bufn)   strcat(statestr[bufn], "W");
      if ( this->frame.bufcomplete[bufn] ) strcat(statestr[bufn], "C");
    }
    for (bufn=0; bufn < Archon::nbufs; bufn++) {
      message << std::setw(3) << (bufn==this->frame.index ? "-->" : "") << " ";                       // buf
      message << std::setw(3) << bufn+1 << " ";
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufbase[bufn] << " ";                                                    // base
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frame.bufrawoffset[bufn] << " ";                                               // rawoff
      message << std::setfill(' ') << std::setw(5) << std::dec << this->frame.bufframen[bufn] << " "; // frame
      message << std::setw(5) << this->frame.bufcomplete[bufn] << " ";                                // ready
      message << std::setw(5) << this->frame.buflines[bufn] << " ";                                   // lines
      message << std::setw(8) << this->frame.bufrawlines[bufn] << " ";                                // rawlines
      message << std::setw(5) << this->frame.bufrawblocks[bufn] << " ";                               // rblks
      message << std::setw(5) << this->frame.bufwidth[bufn] << " ";                                   // width
      message << std::setw(6) << this->frame.bufheight[bufn] << " ";                                  // height
      message << std::setw(5) << statestr[bufn] << " ";                                                      // state
      message << std::setw(5) << this->frame.bufpixels[bufn];
      logwrite(function, message.str());
      message.str("");
    }
  }
  /**************** Archon::Interface::print_frame_status *********************/


  /**************** Archon::Interface::lock_buffer ****************************/
  /**
   * @fn     lock_buffer
   * @brief  lock Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::lock_buffer(int buffer) {
    std::string function = "Archon::Interface::lock_buffer";
    std::stringstream message;
    std::stringstream sscmd;

    sscmd.str("");
    sscmd << "LOCK" << buffer;
    if ( this->archon_cmd(sscmd.str()) ) {
      message.str(""); message << "ERROR: sending Archon command to lock frame buffer " << buffer;
      logwrite( function, message.str() );
      return ERROR;
    }
    return (NO_ERROR);
  }
  /**************** Archon::Interface::lock_buffer ****************************/


  /**************** Archon::Interface::get_timer ******************************/
  /** 
   * @fn     get_timer
   * @brief  read the 64 bit interal timer from the Archon controller
   * @param  *timer pointer to type unsigned long int
   * @return errno on error, 0 if okay.
   *
   * Sends the "TIMER" command to Archon, reads back the reply, and stores the
   * value as (unsigned long int) in the argument variable pointed to by *timer.
   *
   * This is an internal 64 bit timer/counter. One tick of the counter is 10 ns.
   *
   */
  long Interface::get_timer(unsigned long int *timer) {
    std::string function = "Archon::Interface::get_timer";
    std::string reply;
    std::stringstream message, timer_ss;
    std::vector<std::string> tokens;
    long error;

    // send TIMER command to get frame buffer status
    //
    if ( (error = this->archon_cmd(TIMER, reply)) != NO_ERROR ) {
      return error;
    }

    Tokenize(reply, tokens, "=");                   // Tokenize the reply

    // Reponse should be "TIMER=xxxx\n" so there needs
    // to be two tokens
    //
    if (tokens.size() != 2) {
      message.str(""); message << "unrecognized timer response: " << reply << ". Expected TIMER=xxxx";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // Second token must be a hexidecimal string
    //
    std::string timer_str = tokens[1]; 
    try {
        timer_str.erase(timer_str.find('\n'), 1);
    } catch(...) { }  // remove newline
    if (!std::all_of(timer_str.begin(), timer_str.end(), ::isxdigit)) {
      message.str(""); message << "unrecognized timer value: " << timer_str << ". Expected hexadecimal string";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // convert from hex string to integer and save return value
    //
    timer_ss << std::hex << tokens[1];
    timer_ss >> *timer;
    return NO_ERROR;
  }
  /**************** Archon::Interface::get_timer ******************************/


  /**************** Archon::Interface::fetch **********************************/
  /**
   * @fn     fetch
   * @brief  fetch Archon frame buffer
   * @param  int frame buffer to lock
   * @return 
   *
   */
  long Interface::fetch(uint64_t bufaddr, uint32_t bufblocks) {
    std::string function = "Archon::Interface::fetch";
    std::stringstream message;
    uint32_t maxblocks = (uint32_t)(1.5E9 / this->camera_info.activebufs / 1024 );
    uint64_t maxoffset = this->frame.bufbase[this->frame.index];
    uint64_t maxaddr = maxoffset + maxblocks;

    if ( bufaddr > maxaddr ) {
      message.str(""); message << "fetch Archon buffer requested address 0x" << std::hex << bufaddr << " exceeds 0x" << maxaddr;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }
    if ( bufblocks > maxblocks ) {
      message.str(""); message << "fetch Archon buffer requested blocks 0x" << std::hex << bufblocks << " exceeds 0x" << maxblocks;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    std::stringstream sscmd;
    sscmd << "FETCH"
          << std::setfill('0') << std::setw(8) << std::hex
          << bufaddr
          << std::setfill('0') << std::setw(8) << std::hex
          << bufblocks;
    std::string scmd = sscmd.str();
    try {
      std::transform( scmd.begin(), scmd.end(), scmd.begin(), ::toupper );  // make uppercase

    } catch (...) {
      message.str(""); message << "converting command: " << sscmd.str() << " to uppercase";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // disable sending fetch command in autofetch mode
    if (!this->is_autofetch) {
      if (this->archon_cmd(scmd) == ERROR) {
        logwrite( function, "ERROR: sending FETCH command. Aborting read." );
        this->archon_cmd(UNLOCK);                                             // unlock all buffers
        return ERROR;
      }
    }

    message.str(""); message << "reading " << (this->camera_info.frame_type==Camera::FRAME_RAW?"raw":"image") << " with " << scmd;
    logwrite(function, message.str());
    return NO_ERROR;
  }
  /**************** Archon::Interface::fetch **********************************/


  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @fn     read_frame
   * @brief  read latest Archon frame buffer
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This is function is overloaded.
   *
   * This version, with no parameter, is the one that is called by the server.
   * The decision is made here if the frame to be read is a RAW or an IMAGE 
   * frame based on this->camera_info.current_observing_mode, then the 
   * overloaded version of read_frame(frame_type) is called with the appropriate 
   * frame type of IMAGE or RAW.
   *
   * This function WILL call write_frame(...) to write data after reading it.
   *
   */
  long Interface::read_frame() {
    std::string function = "Archon::Interface::read_frame";
    std::stringstream message;
    long error = NO_ERROR;

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

    int rawenable = this->modemap[this->camera_info.current_observing_mode].rawenable;

    if (rawenable == -1) {
      this->camera.log_error( function, "RAWENABLE is undefined" );
      return ERROR;
    }

    // RAW-only
    //
    if (this->camera_info.current_observing_mode == "RAW") {              // "RAW" is the only reserved mode name

      // the RAWENABLE parameter must be set in the ACF file, in order to read RAW data
      //
      if (rawenable==0) {
        this->camera.log_error( function, "observing mode is RAW but RAWENABLE=0 -- change mode or set RAWENABLE?" );
        return ERROR;

      } else {
        error = this->read_frame(Camera::FRAME_RAW);                              // read raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading raw frame" ); return error; }
        error = this->write_frame();                                              // write raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing raw frame" ); return error; }
      }

    } else {
        // IMAGE, or IMAGE+RAW
        // datacube was already set = true in the expose function
      error = this->read_frame(Camera::FRAME_IMAGE);                              // read image frame
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading image frame" ); return error; }
      error = this->write_frame();                                                // write image frame
      if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing image frame" ); return error; }

      // If mode is not RAW but RAWENABLE=1, then we will first read an image
      // frame (just done above) and then a raw frame (below). To do that we
      // must switch to raw mode then read the raw frame. Afterward, switch back
      // to the original mode, for any subsequent exposures.
      //
      if (rawenable == 1) {
        #ifdef LOGLEVEL_DEBUG
        logwrite(function, "[DEBUG] rawenable is set -- IMAGE+RAW file will be saved");
        logwrite(function, "[DEBUG] switching to mode=RAW");
        #endif
        std::string orig_mode = this->camera_info.current_observing_mode; // save the original mode, so we can come back to it
        error = this->set_camera_mode("raw");                             // switch to raw mode
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: switching to raw mode" ); return error; }

        #ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling read_frame(Camera::FRAME_RAW) if error=0"; logwrite(function, message.str());
        #endif
        error = this->read_frame(Camera::FRAME_RAW);                      // read raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: reading raw frame" ); return error; }
        #ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] calling write_frame() for raw data if error=0"; logwrite(function, message.str());
        #endif
        error = this->write_frame();                                      // write raw frame
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: writing raw frame" ); return error; }
        #ifdef LOGLEVEL_DEBUG
        message.str(""); message << "error=" << error << "[DEBUG] switching back to original mode if error=0"; logwrite(function, message.str());
        #endif
        error = this->set_camera_mode(orig_mode);                         // switch back to the original mode
        if ( error != NO_ERROR ) { logwrite( function, "ERROR: switching back to previous mode" ); return error; }
      }
    }

    return error;
  }
  /**************** Archon::Interface::read_frame *****************************/

  /**************** Archon::Interface::hread_frame *****************************/
  /**
   * @fn     hread_frame
   * @brief  read latest Archon frame buffer
   * @param  frame_type
   * @return ERROR or NO_ERROR
   *
   * This is the read_frame function which performs the actual read of the
   * selected frame type.
   *
   * No write takes place here!
   *
   */
    long Interface::hread_frame() {
        std::string function = "Archon::Interface::hread_frame";
        std::stringstream message;
        int retval;
        int bufready;
        char check[5], header[5];
        char *ptr_image;
        int bytesread, totalbytesread, toread;
        uint64_t bufaddr;
        unsigned int block, bufblocks=0;
        long error = ERROR;
        int num_detect = this->modemap[this->camera_info.current_observing_mode].geometry.num_detect;

        // Archon buffer number of the last frame read into memory
        // Archon frame index is 1 biased so add 1 here
        bufready = this->frame.index + 1;

        if (bufready < 1 || bufready > this->camera_info.activebufs) {
            message.str(""); message << "invalid Archon buffer " << bufready << " requested. Expected {1:" << this->camera_info.activebufs << "}";
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        message.str(""); message << "will read image data from Archon controller buffer " << bufready << " frame " << this->frame.frame;
        logwrite(function, message.str());

        // Lock the frame buffer before reading it
        //
        // if ( this->lock_buffer(bufready) == ERROR) {
        //    logwrite( function, "ERROR locking frame buffer" );
        //    return (ERROR);
        // }

        // Send the FETCH command to read the memory buffer from the Archon backplane.
        // Archon replies with one binary response per requested block. Each response
        // has a message header.

        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks =
                (unsigned int) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN );

        message.str(""); message << "will read " << std::dec << this->camera_info.image_memory << " bytes "
                                 << "0x" << std::uppercase << std::hex << bufblocks << " blocks from bufaddr=0x" << bufaddr;
        logwrite(function, message.str());

        if (!this->is_autofetch) {
          // send the FETCH command.
          // This will take the archon_busy semaphore, but not release it -- must release in this function!
          //
          error = this->fetch(bufaddr, bufblocks);
          if (error != NO_ERROR) {
            logwrite(function, "ERROR: fetching Archon buffer");
            return error;
          }
        }

        // Read the data from the connected socket into memory, one block at a time
        //
        ptr_image = this->image_data;
        totalbytesread = 0;
        std::cerr << "reading bytes: ";
        for (block=0; block<bufblocks; block++) {

            // Are there data to read?
            if ( (retval=this->archon.Poll()) <= 0) {
                if (retval==0) {
                    message.str("");
                    message << "Poll timeout waiting for Archon frame data";
                    error = ERROR;
                }  // TODO should error=TIMEOUT?

                if (retval<0)  {
                    message.str("");
                    message << "Poll error waiting for Archon frame data";
                    error = ERROR;
                }

                if ( error != NO_ERROR ) this->camera.log_error( function, message.str() );
                break;                         // breaks out of for loop
            }

            // Wait for a block+header Bytes to be available
            // (but don't wait more than 1 second -- this should be tens of microseconds or less)
            //
            auto start = std::chrono::steady_clock::now();               // start a timer now

            while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
                auto now = std::chrono::steady_clock::now();             // check the time again
                std::chrono::duration<double> diff = now-start;          // calculate the duration
                if (diff.count() > 1) {                                  // break while loop if duration > 1 second
                    std::cerr << "\n";
                    this->camera.log_error( function, "timeout waiting for data from Archon" );
                    error = ERROR;
                    break;                       // breaks out of while loop
                }
            }
            if ( error != NO_ERROR ) break;  // needed to also break out of for loop on error

            // Check message header
            //
            SNPRINTF(check, "<%02X:", this->msgref)
            if ( (retval=this->archon.Read(header, 4)) != 4 ) {
                message.str(""); message << "code " << retval << " reading Archon frame header";
                this->camera.log_error( function, message.str() );
                error = ERROR;
                break;                         // break out of for loop
            }
            if (header[0] == '?') {  // Archon retured an error
                message.str(""); message << "Archon returned \'?\' reading image data";
                this->camera.log_error( function, message.str() );
                this->fetchlog();      // check the Archon log for error messages
                error = ERROR;
                break;                         // break out of for loop

            } else if (strncmp(header, check, 4) != 0) {
                message.str(""); message << "Archon command-reply mismatch reading image data. header=" << header << " check=" << check;
                this->camera.log_error( function, message.str() );
                error = ERROR;
                break;                         // break out of for loop
            }

            // Read the frame contents
            //
            bytesread = 0;
            do {
                toread = BLOCK_LEN - bytesread;
                if ( (retval=this->archon.Read(ptr_image, (size_t)toread)) > 0 ) {
                    bytesread += retval;         // this will get zeroed after each block
                    totalbytesread += retval;    // this won't (used only for info purposes)
                    std::cerr << std::setw(10) << totalbytesread << "\b\b\b\b\b\b\b\b\b\b";
                    ptr_image += retval;         // advance pointer
                }
            } while (bytesread < BLOCK_LEN);

        } // end of loop: for (block=0; block<bufblocks; block++)

        // give back the archon_busy semaphore to allow other threads to access the Archon now
        //
        const std::unique_lock<std::mutex> lock(this->archon_mutex);
        this->archon_busy = false;
        this->archon_mutex.unlock();

        std::cerr << std::setw(10) << totalbytesread << " complete\n";   // display progress on same line of std err

        // If we broke out of the for loop for an error then report incomplete read
        //
        if ( error==ERROR || block < bufblocks) {
            message.str(""); message << "incomplete frame read " << std::dec
                                     << totalbytesread << " bytes: " << block << " of " << bufblocks << " 1024-byte blocks";
            logwrite( function, message.str() );
        }

        // Unlock the frame buffer
        //
        // if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

        // On success, write the value to the log and return
        //
        if (error == NO_ERROR) {
            message.str(""); message << "successfully read " << std::dec << totalbytesread
                    << " image bytes (0x" << std::uppercase << std::hex << bufblocks << " blocks) from Archon controller";
            logwrite(function, message.str());

        } else {
            // Throw an error for any other errors
            logwrite( function, "ERROR: reading Archon camera data to memory!" );
        }
        return error;
    }
    /**************** Archon::Interface::hread_frame *****************************/

  /**************** Archon::Interface::read_frame *****************************/
  /**
   * @fn     read_frame
   * @brief  read latest Archon frame buffer
   * @param  frame_type
   * @return ERROR or NO_ERROR
   *
   * This is the overloaded read_frame function which accepts the frame_type argument.
   * This is called only by this->read_frame() to perform the actual read of the
   * selected frame type.
   *
   * No write takes place here!
   *
   */
  long Interface::read_frame(Camera::frame_type_t frame_type) {
    std::string function = "Archon::Interface::read_frame";
    std::stringstream message;
    int retval;
    int bufready;
    char check[5], header[5];
    char *ptr_image;
    int bytesread, totalbytesread, toread;
    uint64_t bufaddr;
    unsigned int block, bufblocks=0;
    long error = ERROR;
    int num_detect = this->modemap[this->camera_info.current_observing_mode].geometry.num_detect;

    this->camera_info.frame_type = frame_type;

/***
    // Check that image buffer is prepared  //TODO should I call prepare_image_buffer() here, automatically?
    //
    if ( (this->image_data == nullptr) ||
         (this->image_data_bytes == 0) ) {
      this->camera.log_error( function, "image buffer not ready" );
//    return ERROR;
    }

    if ( this->image_data_allocated != this->image_data_bytes ) {
      message.str(""); message << "incorrect image buffer size: " 
                               << this->image_data_allocated << " bytes allocated but " << this->image_data_bytes << " needed";
      this->camera.log_error( function, message.str() );
//    return ERROR;
    }
***/

    error = this->prepare_image_buffer();
    if (error == ERROR) {
      logwrite( function, "ERROR: unable to allocate an image buffer" );
      return ERROR;
    }

// TODO removed 2021-Jun-09
// This shouldn't be needed since wait_for_readout() was called previously.
//  // Get the current frame buffer status
//  //
//  error = this->get_frame_status();
//
//  if (error != NO_ERROR) {
//    this->camera.log_error( function, "unable to get frame status");
//    return error;
//  }

    // Archon buffer number of the last frame read into memory
    //
    bufready = this->frame.index + 1;

    if (bufready < 1 || bufready > this->camera_info.activebufs) {
      message.str(""); message << "invalid Archon buffer " << bufready << " requested. Expected {1:" << this->camera_info.activebufs << "}";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    message.str(""); message << "will read " << (frame_type == Camera::FRAME_RAW ? "raw" : "image")
                             << " data from Archon controller buffer " << bufready << " frame " << this->frame.frame;
    logwrite(function, message.str());

    // don't lock frame buffer in autofetch mode
    if (!this->is_autofetch) {
      // Lock the frame buffer before reading it
      //
      if ( this->lock_buffer(bufready) == ERROR) {
        logwrite( function, "ERROR locking frame buffer" );
        return (ERROR);
      }
    }

    // Send the FETCH command to read the memory buffer from the Archon backplane.
    // Archon replies with one binary response per requested block. Each response
    // has a message header.
    //
    switch (frame_type) {
      case Camera::FRAME_RAW:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index] + this->frame.bufrawoffset[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks = (unsigned int) floor( (this->camera_info.image_memory + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      case Camera::FRAME_IMAGE:
        // Archon buffer base address
        bufaddr   = this->frame.bufbase[this->frame.index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks =
        (unsigned int) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      default:  // should be impossible
        message.str(""); message << "unknown frame type specified: " << frame_type << ": expected FRAME_RAW | FRAME_IMAGE";
        this->camera.log_error( function, message.str() );
        return ERROR;
        break;
    }

    message.str(""); message << "will read " << std::dec << this->camera_info.image_memory << " bytes "
                             << "0x" << std::uppercase << std::hex << bufblocks << " blocks from bufaddr=0x" << bufaddr;
    logwrite(function, message.str());

    // Dont't send fetch command in autofetch mode
    if (!this->is_autofetch) {
      // send the FETCH command.
      // This will take the archon_busy semaphore, but not release it -- must release in this function!
      //
      error = this->fetch(bufaddr, bufblocks);
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: fetching Archon buffer" );
        return error;
      }
    }

    // Read the data from the connected socket into memory, one block at a time
    //
    ptr_image = this->image_data;
    totalbytesread = 0;
    std::cerr << "reading bytes: ";
    for (block=0; block<bufblocks; block++) {
      // Disable polling in autofetch mode
      if (!this->is_autofetch) {
        // Are there data to read?
        if ( (retval=this->archon.Poll()) <= 0) {
          if (retval==0) {
            message.str("");
            message << "Poll timeout waiting for Archon frame data";
            error = ERROR;
          }  // TODO should error=TIMEOUT?

          if (retval<0)  {
            message.str("");
            message << "Poll error waiting for Archon frame data";
            error = ERROR;
          }

          if ( error != NO_ERROR ) this->camera.log_error( function, message.str() );
          break;                         // breaks out of for loop
        }
      }

      // Wait for a block+header Bytes to be available
      // (but don't wait more than 1 second -- this should be tens of microseconds or less)
      //
      auto start = std::chrono::steady_clock::now();             // start a timer now

      while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
        auto now = std::chrono::steady_clock::now();             // check the time again
        std::chrono::duration<double> diff = now-start;          // calculate the duration
        if (diff.count() > 1) {                                  // break while loop if duration > 1 second
          std::cerr << "\n";
          this->camera.log_error( function, "timeout waiting for data from Archon" );
          error = ERROR;
          break;                       // breaks out of while loop
        }
      }
      if ( error != NO_ERROR ) break;  // needed to also break out of for loop on error

      // Check message header
      if (this->is_autofetch) {
        sprintf(check, "<XF:");
      } else {
        SNPRINTF(check, "<%02X:", this->msgref);
      }

      if ( (retval=this->archon.Read(header, 4)) != 4 ) {
        message.str(""); message << "code " << retval << " reading Archon frame header";
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;                         // break out of for loop
      }

      // Read autofetch header
      if (this->is_autofetch) {
        if (strncmp(header, "<SFA", 4) == 0) {
          logwrite( function, "AUTOFETCH HEADER: FOUND" );
          std::string autofetch_header_str;
          retval = this->archon.Read(autofetch_header_str, '\n');

          // Read next header
          if ( (retval=this->archon.Read(header, 4)) != 4 ) {
            message.str(""); message << "code " << retval << " reading Archon frame header";
            this->camera.log_error( function, message.str() );
            error = ERROR;
            break;                         // break out of for loop
          }
        }
      }

      if (header[0] == '?') {  // Archon retured an error
        message.str(""); message << "Archon returned \'?\' reading " << (frame_type==Camera::FRAME_RAW?"raw ":"image ") << " data";
        this->camera.log_error( function, message.str() );
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;                         // break out of for loop

      } else if (strncmp(header, check, 4) != 0) {
        message.str(""); message << "Archon command-reply mismatch reading " << (frame_type==Camera::FRAME_RAW?"raw ":"image ")
                                 << " data. header=" << header << " check=" << check;
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;                         // break out of for loop
      }

      // Read the frame contents
      //
      bytesread = 0;
      do {
        toread = BLOCK_LEN - bytesread;
        if ( (retval=this->archon.Read(ptr_image, (size_t)toread)) > 0 ) {
          bytesread += retval;         // this will get zeroed after each block
          totalbytesread += retval;    // this won't (used only for info purposes)
          std::cerr << std::setw(10) << totalbytesread << "\b\b\b\b\b\b\b\b\b\b";
          ptr_image += retval;         // advance pointer
        }
      } while (bytesread < BLOCK_LEN);

    } // end of loop: for (block=0; block<bufblocks; block++)

    // give back the archon_busy semaphore to allow other threads to access the Archon now
    //
    const std::unique_lock<std::mutex> lock(this->archon_mutex);
    this->archon_busy = false;
    this->archon_mutex.unlock();

    std::cerr << std::setw(10) << totalbytesread << " complete\n";   // display progress on same line of std err

    // If we broke out of the for loop for an error then report incomplete read
    //
    if ( error==ERROR || block < bufblocks) {
      message.str(""); message << "incomplete frame read " << std::dec 
                               << totalbytesread << " bytes: " << block << " of " << bufblocks << " 1024-byte blocks";
      logwrite( function, message.str() );
    }

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->archon_cmd(UNLOCK);

    // On success, write the value to the log and return
    //
    if (error == NO_ERROR) {
      message.str(""); message << "successfully read " << std::dec << totalbytesread << (frame_type==Camera::FRAME_RAW?" raw":" image")
                               << " bytes (0x" << std::uppercase << std::hex << bufblocks << " blocks) from Archon controller";
      logwrite(function, message.str());

    } else {
        // Throw an error for any other errors
      logwrite( function, "ERROR: reading Archon camera data to memory!" );
    }
    return error;
  }
  /**************** Archon::Interface::read_frame *****************************/


  /**************** Archon::Interface::write_frame ****************************/
  /**
   * @fn     write_frame
   * @brief  creates a FITS_file object to write the image_data buffer to disk
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * A FITS_file object is created here to write the data. This object MUST remain
   * valid while any (all) threads are writing data, so the write_data function
   * will keep track of threads so that it doesn't terminate until all of its 
   * threads terminate.
   *
   * The camera_info class was copied into fits_info when the exposure was started,  //TODO I've un-done this.
   * so use fits_info from here on out.                                              //TODO Don't use fits_info right now.
   *                                                                                 //TODO Only using camera_info
   */
  long Interface::write_frame() {
    std::string function = "Archon::Interface::write_frame";
    std::stringstream message;
    uint32_t   *cbuf32;                  //!< used to cast char buf into 32 bit int
    uint16_t   *cbuf16;                  //!< used to cast char buf into 16 bit int
    int16_t    *cbuf16s;                 //!< used to cast char buf into 16 bit int
    long        error=NO_ERROR;

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

//  message.str(""); message << "writing " << this->fits_info.bitpix << "-bit data from memory to disk";  //TODO
    message.str(""); message << "writing " << this->camera_info.bitpix << "-bit data from memory to disk";
    logwrite(function, message.str());

    // The Archon sends four 8-bit numbers per pixel. To convert this into something usable,
    // cast the image buffer into integers. Handled differently depending on bits per pixel.
    //
    switch (this->camera_info.bitpix) {

      // convert four 8-bit values into a 32-bit value and scale by 2^16
      //
      case 32: {
        cbuf32 = (uint32_t *)this->image_data;                  // cast here to 32b

        // Write each amplifier as a separate extension
        //
        if ( this->camera.cubeamps() ) {
          float *fext = nullptr;

          for ( int ext=0; ext < (int)this->camera_info.amp_section.size(); ext++ ) {
            try {
              // get the coordinates of this amplifier extension
              //
              long x1 = this->camera_info.amp_section.at(ext).at(0);
              long x2 = this->camera_info.amp_section.at(ext).at(1);
              long y1 = this->camera_info.amp_section.at(ext).at(2);
              long y2 = this->camera_info.amp_section.at(ext).at(3);

              // assign this amplifier section as the region of interest
              //
              this->camera_info.region_of_interest[0] = x1;
              this->camera_info.region_of_interest[1] = x2;
              this->camera_info.region_of_interest[2] = y1;
              this->camera_info.region_of_interest[3] = y2;

              // This call to set_axes() is to set the axis_pixels, axes, and section_size,
              // needed for the FITS writer
              //
              error = this->camera_info.set_axes();

              #ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] x1=" << x1 << " x2=" << x2 << " y1=" << y1 << " y2=" << y2;
              logwrite( function, message.str() );
              message.str(""); message << "[DEBUG] axes[0]=" << this->camera_info.axes[0] << " axes[1]=" << this->camera_info.axes[1];
              logwrite( function, message.str() );
              #endif

              // allocate the number of pixels needed for this amplifier extension
              //
              long ext_size = (x2-x1+1) * (y2-y1+1);
              fext = new float[ ext_size ];

              #ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] allocated " << ext_size << " pixels for extension " << this->camera_info.extension+1;
              logwrite( function, message.str() );
              #endif

              // copy this amplifier from the main cbuf32,
              // at the same time right-shift the requested number of bits
              //
              long pix=0;
              long ncols=this->camera_info.detector_pixels[0];  // PIXELCOUNT
              for ( long row=y1-1; row<y2; row++ ) {
                for ( long col=x1-1; col<x2; col++ ) {
                  fext[pix++] = (float)( cbuf32[ row*ncols + col ] >> this->n_hdrshift );
                }
              }

              #ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] calling fits_file.write_image( ) for extension " << this->camera_info.extension+1;
              logwrite( function, message.str() );
              #endif

              error = this->fits_file.write_image(fext, this->camera_info); // write the image to disk
              this->camera_info.extension++;                                // increment extension for cubes
              if ( fext != nullptr ) { delete [] fext; fext=nullptr; }            // dynamic object not automatic so must be destroyed

            } catch( std::out_of_range & ) {
              message.str(""); message << "ERROR: " << ext << " is a bad extension number";
              logwrite( function, message.str() );
              if ( fext != nullptr ) { delete [] fext; fext=nullptr; }            // dynamic object not automatic so must be destroyed
              return ERROR;
            }
          }
            // end if this->camera.cubeamps()

        } else {
            // Write all amplifiers to the same extension
            //
            float *fbuf = nullptr;
//          fbuf = new float[ this->fits_info.section_size ];       // allocate a float buffer of same number of pixels for scaling  //TODO
            fbuf = new float[ this->camera_info.section_size ];     // allocate a float buffer of same number of pixels for scaling

//          for (long pix=0; pix < this->fits_info.section_size; pix++)   //TODO
            for (long pix=0; pix < this->camera_info.section_size; pix++) {
              fbuf[pix] = (float) ( cbuf32[pix] >> this->n_hdrshift ); // right shift the requested number of bits
            }

//          error = fits_file.write_image(fbuf, this->fits_info);   // write the image to disk //TODO
            error = this->fits_file.write_image(fbuf, this->camera_info); // write the image to disk
            if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 32-bit image to disk" ); }
            delete [] fbuf;
        }
        break;
      }

      // convert four 8-bit values into 16 bit values
      //
      case 16: {
        if (this->camera_info.datatype == USHORT_IMG) {                   // raw
          cbuf16 = (uint16_t *)this->image_data;                          // cast to 16b unsigned int
//        error = fits_file.write_image(cbuf16, this->fits_info);         // write the image to disk //TODO
          error = this->fits_file.write_image(cbuf16, this->camera_info); // write the image to disk
          if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 16-bit raw image to disk" ); }

        } else if (this->camera_info.datatype == SHORT_IMG) {
          cbuf16s = (int16_t *)this->image_data;                          // cast to 16b signed int
          int16_t *ibuf = nullptr;
          ibuf = new int16_t[ this->camera_info.section_size ];
          for (long pix=0; pix < this->camera_info.section_size; pix++) {
            ibuf[pix] = cbuf16s[pix] - 32768;                             // subtract 2^15 from every pixel
          }
          error = this->fits_file.write_image(ibuf, this->camera_info);   // write the image to disk
          if ( error != NO_ERROR ) { this->camera.log_error( function, "writing 16-bit image to disk" ); }
          delete [] ibuf;

        } else {
          message.str(""); message << "unsupported 16 bit datatype " << this->camera_info.datatype;
          this->camera.log_error( function, message.str() );
          error = ERROR;
        }
        break;
      }

      // shouldn't happen
      //
      default:
//      message.str(""); message << "unrecognized bits per pixel: " << this->fits_info.bitpix; //TODO 
        message.str(""); message << "unrecognized bits per pixel: " << this->camera_info.bitpix;
        this->camera.log_error( function, message.str() );
        error = ERROR;
        break;
    }

    // Things to do after successful write
    //
    if ( error == NO_ERROR ) {
      if ( this->camera.datacube() ) {
        this->camera_info.extension++;                                // increment extension for cubes
        message.str(""); message << "DATACUBE:" << this->camera_info.extension << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
        this->camera.async.enqueue( message.str() );
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }
      logwrite(function, "frame write complete");

    } else {
      logwrite( function, "ERROR: writing image" );
    }

    return error;
  }
  /**************** Archon::Interface::write_frame ****************************/


  /**************** Archon::Interface::write_raw ******************************/
  /**
   * @fn     write_raw
   * @brief  write raw 16 bit data to a FITS file
   * @param  none
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_raw() {
    std::string function = "Archon::Interface::write_raw";
    std::stringstream message;

    unsigned short *cbuf16;              //!< used to cast char buf into 16 bit int

    // Cast the image buffer of chars into integers to convert four 8-bit values 
    // into a 16-bit value
    //
    cbuf16 = (unsigned short *)this->image_data;

    fitsfile *FP       = nullptr;
    int       status   = 0;
    int       naxes    = 2;
    long      axes[2];
    long      firstele = 1;
    long      nelements;

    axes[0] = this->camera_info.axes[0];
    axes[1] = this->camera_info.axes[1];

    nelements = axes[0] * axes[1];

    // create fits file
    //
    if (this->camera_info.extension == 0) {
      #ifdef LOGLEVEL_DEBUG
      logwrite(function, "[DEBUG] creating fits file with cfitsio");
      #endif
      if (fits_create_file( &FP, this->camera_info.fits_name.c_str(), &status ) ) {
        message.str("");
        message << "cfitsio error " << status << " creating FITS file " << this->camera_info.fits_name;
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

    } else {
      #ifdef LOGLEVEL_DEBUG
      logwrite(function, "[DEBUG] opening fits file with cfitsio");
      message.str(""); message << "[DEBUG] file=" << this->camera_info.fits_name << " extension=" << this->camera_info.extension
                               << " bitpix=" << this->camera_info.bitpix;
      logwrite(function, message.str());
      #endif
      if (fits_open_file( &FP, this->camera_info.fits_name.c_str(), READWRITE, &status ) ) {
        message.str("");
        message << "cfitsio error " << status << " opening FITS file " << this->camera_info.fits_name;
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    // create image
    //
    logwrite(function, "create image");
    message.str(""); message << "axes=" << axes[0] << " " << axes[1];
    logwrite(function, message.str());
    if ( fits_create_img( FP, USHORT_IMG, naxes, axes, &status) ) {
      message.str("");
      message << "fitsio error " << status << " creating FITS image for " << this->camera_info.fits_name;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // supplemental header keywords
    //
    fits_write_key( FP, TSTRING,    "MODE", &this->camera_info.current_observing_mode, "observing mode", &status );

    // write HDU
    //
    logwrite(function, "write HDU");
    if ( fits_write_img( FP, TUSHORT, firstele, nelements, cbuf16, &status) ) {
      message.str("");
      message << "fitsio error " << status << " writing FITS image HDU to " << this->camera_info.fits_name;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // close file
    //
    logwrite(function, "close file");
    if ( fits_close_file( FP, &status ) ) {
      message.str("");
      message << "fitsio error " << status << " closing fits file " << this->camera_info.fits_name;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    return NO_ERROR;
  }
  /**************** Archon::Interface::write_raw ******************************/


  /**************** Archon::Interface::write_config_key ***********************/
  /**
   * @fn     write_config_key
   * @brief  write a configuration KEY=VALUE pair to the Archon controller
   * @param  key
   * @param  newvalue
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::write_config_key( const char *key, const char *newvalue, bool &changed ) {
    std::string function = "Archon::Interface::write_config_key";
    std::stringstream message, sscmd;
    long error=NO_ERROR;

    if ( key==nullptr || newvalue==nullptr ) {
      error = ERROR;
      this->camera.log_error( function, "key|value cannot have NULL" );

    } else if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "requested key " << key << " not found in configmap";
      this->camera.log_error( function, message.str() );

    } else if ( this->configmap[key].value == newvalue ) {
      // If no change in value then don't send the command
      error = NO_ERROR;
      message.str(""); message << "config key " << key << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());

    } else {
        /**
     * Format and send the Archon WCONFIG command
     * to write the KEY=VALUE pair to controller memory
     */
      sscmd << "WCONFIG"
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->configmap[key].line
            << key
            << "="
            << newvalue;
      message.str(""); message << "sending: archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error = this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;               // save newvalue in the STL map
        changed = true;

      } else {
        message.str(""); message << "ERROR: config key=value: " << key << "=" << newvalue << " not written";
        logwrite( function, message.str() );
      }
    }
    return error;
  }

  long Interface::write_config_key( const char *key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /**************** Archon::Interface::write_config_key ***********************/


  /**************** Archon::Interface::write_parameter ************************/
  /**
   * @fn     write_parameter
   * @brief  write a parameter to the Archon configuration memory
   * @param  paramname
   * @param  newvalue
   * @return NO_ERROR or ERROR
   *
   * After writing a parameter, requires an APPLYALL or LOADPARAMS command
   *
   */
  long Interface::write_parameter( const char *paramname, const char *newvalue, bool &changed ) {
    std::string function = "Archon::Interface::write_parameter";
    std::stringstream message, sscmd;
    long error=NO_ERROR;

    #ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] paramname=" << paramname << " value=" << newvalue;
    logwrite( function, message.str() );
    #endif

    if ( paramname==nullptr || newvalue==nullptr ) {
      error = ERROR;
      this->camera.log_error( function, "paramname|value cannot have NULL" );

    } else if ( this->parammap.find(paramname) == this->parammap.end() ) {
      error = ERROR;
      message.str(""); message << "parameter \"" << paramname << "\" not found in parammap";
      this->camera.log_error( function, message.str() );
    }

    /**
     * If no change in value then don't send the command
     */
    if ( error==NO_ERROR && this->parammap[paramname].value == newvalue ) {
      error = NO_ERROR;
      message.str(""); message << "parameter " << paramname << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());
    } else if (error==NO_ERROR) {
        /**
         * Format and send the Archon command WCONFIGxxxxttt...ttt
         * which writes the text ttt...ttt to configuration line xxx (hex)
         * to controller memory.
         */
      sscmd << "WCONFIG" 
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->parammap[paramname].line
            << this->parammap[paramname].key
            << "="
            << this->parammap[paramname].name
            << "="
            << newvalue;
      message.str(""); message << "sending archon_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error=this->archon_cmd((char *)sscmd.str().c_str());   // send the WCONFIG command here
      if ( error == NO_ERROR ) {
        this->parammap[paramname].value = newvalue;            // save newvalue in the STL map
        changed = true;

      } else logwrite( function, "ERROR: sending WCONFIG command" );
    } 
    
    return error;
  } 
  
  long Interface::write_parameter( const char *paramname, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return( write_parameter(paramname, newvaluestr.str().c_str(), changed) );
  }

  long Interface::write_parameter( const char *paramname, const char *newvalue ) {
    bool dontcare = false;
    return( write_parameter(paramname, newvalue, dontcare) );
  }

  long Interface::write_parameter( const char *paramname, int newvalue ) {
    bool dontcare = false;
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return( write_parameter(paramname, newvaluestr.str().c_str(), dontcare) );
  }
  /**************** Archon::Interface::write_parameter ************************/


  /**************** Archon::Interface::get_configmap_value ********************/
  /**
   * @fn     get_configmap_value
   * @brief  get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param  string key_in is the KEY
   * @param  T& value_out reference to variable to contain the VALUE
   * @return ERROR or NO_ERROR
   *
   * This is a template class function so the &value_out reference can be any type.
   * If the key_in KEY is not found then an error message is logged and ERROR is
   * returned, otherwise the VALUE associated with key_in is assigned to &value_out, 
   * and NO_ERROR is returned.
   *
   */
  template <class T>
  long Interface::get_configmap_value(std::string key_in, T& value_out) {
    std::string function = "Archon::Interface::get_configmap_value";
    std::stringstream message;

    if ( this->configmap.find(key_in) != this->configmap.end() ) {
      std::istringstream( this->configmap[key_in].value  ) >> value_out;
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] key=" << key_in << " value=" << value_out << " line=" << this->configmap[key_in].line;
      logwrite(function, message.str());
      #endif
      return NO_ERROR;

    } else {
      message.str("");
      message << "requested key: " << key_in << " not found in configmap";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }
  }
  /**************** Archon::Interface::get_configmap_value ********************/


  /**************** Archon::Interface::add_filename_key ***********************/
  /**
   * @fn     add_filename_key
   * @brief  adds the current filename to the systemkeys database
   * @param  none
   * @return none
   *
   */
  void Interface::add_filename_key() {
    std::stringstream keystr;
    int loc = this->camera_info.fits_name.find_last_of( '/' );
    std::string filename;
    filename = this->camera_info.fits_name.substr( loc+1 );
    keystr << "FILENAME=" << filename << "// this filename";
    this->systemkeys.addkey( keystr.str() );
  }
  /**************** Archon::Interface::add_filename_key ***********************/


  /***** Archon::Interface::get_status_key ************************************/
  /**
   * @brief      get value for the indicated key from the Archon "STATUS" string
   * @param[in]  key    key to extract from STATUS
   * @param[out] value  value of key
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::get_status_key( std::string key, std::string &value ) {
      std::string function = "Archon::Interface::get_status_key";
      std::stringstream message;
      std::string reply;

      long error = this->archon_cmd( STATUS, reply );  // first the whole reply in one string

      if ( error != NO_ERROR ) return error;

      std::vector<std::string> lines, tokens;
      Tokenize( reply, lines, " " );                   // then each line in a separate token "lines"

      for ( auto line : lines ) {
          Tokenize( line, tokens, "=" );                 // break each line into tokens to get KEY=value
          if ( tokens.size() != 2 ) continue;            // need 2 tokens
          try {
              if ( tokens.at(0) == key ) {                 // looking for the KEY
                  value = tokens.at(1);                      // found the KEY= status here
                  break;                                     // done looking
              } else continue;
          }
          catch (std::out_of_range &) {                  // should be impossible
              this->camera.log_error( function, "token out of range" );
              return(ERROR);
          }
      }

      return( NO_ERROR );
  }
  /***** Archon::Interface::get_status_key ************************************/


  /**************** Archon::Interface::expose *********************************/
  /**
   * @fn     expose
   * @brief  initiate an exposure
   * @param  nseq_in string, if set becomes the number of sequences
   * @return ERROR or NO_ERROR
   *
   * This function does the following before returning successful completion:
   *  1) trigger an Archon exposure by setting the EXPOSE parameter = nseq_in
   *  2) wait for exposure delay
   *  3) wait for readout into Archon frame buffer
   *  4) read frame buffer from Archon to host
   *  5) write frame to disk
   *
   * Note that this assumes that the Archon ACF has been programmed to automatically
   * read out the detector into the frame buffer after an exposure.
   *
   */
  long Interface::expose(std::string nseq_in) {
    std::string function = "Archon::Interface::expose";
    std::stringstream message;
    long error = NO_ERROR;
    std::string nseqstr;
    int nseq;

    std::string mode = this->camera_info.current_observing_mode;            // local copy for convenience

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

    // When switching from cubeamps=true to cubeamps=false,
    // simply reset the mode to the current mode in order to
    // reset the image size. 
    //
    // This will need to be revisited once ROI is implemented. // TODO
    //
    if ( !this->camera.cubeamps() && ( this->lastcubeamps != this->camera.cubeamps() ) ) {
      message.str(""); message << "detected change in cubeamps -- resetting camera mode to " << mode;
      logwrite( function, message.str() );
      this->set_camera_mode( mode );
    }

    // exposeparam is set by the configuration file
    // check to make sure it was set, or else expose won't work
    //
    if (this->exposeparam.empty()) {
      message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // If the exposure time or longexposure mode were never set then read them from the Archon.
    // This ensures that, if the client doesn't set these values then the server will have the
    // same default values that the ACF has, rather than hope that the ACF programmer picks
    // their defaults to match mine.
    //
    if ( ! this->camera_info.exposure_time.is_set() ) {
      logwrite( function, "NOTICE:exptime has not been set--will read from Archon" );
      this->camera.async.enqueue( "NOTICE:exptime has not been set--will read from Archon" );

      // read the Archon configuration memory
      //
      std::string etime;
      if ( read_parameter( "exptime", etime ) != NO_ERROR ) {
        logwrite( function, "ERROR: reading \"exptime\" parameter from Archon" );
        return ERROR;
      }

      // Tell the server these values
      //
      std::string retval;
      if ( this->exptime( etime, retval ) != NO_ERROR ) { logwrite( function, "ERROR: setting exptime" ); return ERROR; }
    }

    if ( ! this->is_longexposure_set && ! this->longexposeparam.empty() ) {
      logwrite( function, "NOTICE:longexposure has not been set--will read from Archon" );
      this->camera.async.enqueue( "NOTICE:longexposure has not been set--will read from Archon" );

      // read the Archon configuration memory
      //
      std::string lexp;
      if ( read_parameter( this->longexposeparam, lexp ) != NO_ERROR ) {
        logwrite( function, "ERROR reading \""+this->longexposeparam+"\" parameter from Archon" );
        return ERROR;
      }

      // Tell the server these values
      //
      std::string retval;
      if ( this->longexposure( lexp, retval ) != NO_ERROR ) {
        logwrite( function, "ERROR: setting longexposure" );
         return ERROR;
       }
    }
    // If nseq_in is not supplied then set nseq to 1.
    // Add any pre-exposures onto the number of sequences.
    //
    if ( nseq_in.empty() ) {
      nseq = 1 + this->camera_info.num_pre_exposures;
      nseqstr = std::to_string( nseq );

    } else {                                                          // sequence argument passed in
      try {
        nseq = std::stoi( nseq_in ) + this->camera_info.num_pre_exposures;      // test that nseq_in is an integer
        nseqstr = std::to_string( nseq );                           // before trying to use it

      } catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert sequences: " << nseq_in << " to integer";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch (std::out_of_range &) {
        message.str(""); message << "sequences " << nseq_in << " outside integer range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    // Always initialize the extension number because someone could
    // set datacube true and then send "expose" without a number.
    //
    this->camera_info.extension = 0;

    // Don't send get_frame_status in autofetch mode
    if (!this->is_autofetch) {
      error = this->get_frame_status();  // TODO is this needed here?
    }

    if (error != NO_ERROR) {
      logwrite( function, "ERROR: unable to get frame status" );
      return ERROR;
    }
    this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    // initiate the exposure here
    //
    error = this->prep_parameter(this->exposeparam, nseqstr);
    if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);
    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: could not initiate exposure" );
      return error;
    }

    // get system time and Archon's timer after exposure starts
    // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
    //
    this->camera_info.start_time = get_timestamp();                 // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
    if ( this->get_timer(&this->start_timer) != NO_ERROR ) {        // Archon internal timer (one tick=10 nsec)
      logwrite( function, "ERROR: could not get start time" );
      return ERROR;
    }
    this->camera.set_fitstime(this->camera_info.start_time);        // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
    error=this->camera.get_fitsname(this->camera_info.fits_name);   // assemble the FITS filename
    if ( error != NO_ERROR ) {
      logwrite( function, "ERROR: couldn't validate fits filename" );
      return error;
    }

    this->add_filename_key();                                       // add filename to system keys database

    logwrite(function, "exposure started");

    this->camera_info.systemkeys.keydb = this->systemkeys.keydb;    // copy the systemkeys database object into camera_info

    if (this->camera.writekeys_when=="before") this->copy_keydb();  // copy the ACF and userkeys database into camera_info

    // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension data cube,
    // one extension for the image and a separate extension for raw data.
    //
    if ( (mode != "RAW") && (this->modemap[mode].rawenable) ) {
      if ( !this->camera.datacube() ) {                                   // if datacube not already set then it must be overridden here
        this->camera.async.enqueue( "NOTICE:override datacube true" );  // let everyone know
        logwrite( function, "NOTICE:override datacube true" );
        this->camera.datacube(true);
      }
      this->camera_info.extension = 0;
    }

    // Save the datacube state in camera_info so that the FITS writer can know about it
    //
    this->camera_info.iscube = this->camera.datacube();

    // Open the FITS file now for cubes
    //
    if ( this->camera.datacube() && !this->camera.cubeamps() ) {
      #ifdef LOGLEVEL_DEBUG
      logwrite( function, "[DEBUG] opening fits file for multi-exposure sequence data cube" );
      #endif
      error = this->fits_file.open_file(this->camera.writekeys_when == "before", this->camera_info );
      if ( error != NO_ERROR ) {
        this->camera.log_error( function, "couldn't open fits file" );
        return error;
      }
    }

//  //TODO only use camera_info -- don't use fits_info -- is this OK? TO BE CONFIRMED
//  this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer  //TODO

//  this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

    if (nseq > 1) {
      message.str(""); message << "starting sequence of " << nseq << " frames. lastframe=" << this->lastframe;
      logwrite(function, message.str());
    }

    // If not RAW mode then wait for Archon frame buffer to be ready,
    // then read the latest ready frame buffer to the host. If this
    // is a squence, then loop over all expected frames.
    //
    if ( mode != "RAW" ) {                                          // If not raw mode then
      int expcount = 0;                                             // counter used only for tracking pre-exposures

      //
      // -- MAIN SEQUENCE LOOP --
      //
      while (nseq-- > 0) {

        // Wait for any pre-exposures, first the exposure delay then the readout,
        // but then continue to the next because pre-exposures are not read from
        // the Archon's buffer.
        //
        if ( ++expcount <= this->camera_info.num_pre_exposures ) {

          message.str(""); message << "pre-exposure " << expcount << " of " << this->camera_info.num_pre_exposures;
          logwrite( function, message.str() );

          if ( this->camera_info.exposure_time.value() != 0 ) {         // wait for the exposure delay to complete (if there is one)
            error = this->wait_for_exposure();
            if ( error != NO_ERROR ) {
              logwrite( function, "ERROR: waiting for pre-exposure" );
              return error;
            }
          }

          error = this->wait_for_readout();                             // Wait for the readout into frame buffer,
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: waiting for pre-exposure readout" );
            return error;
          }

          continue;
        }

        // Open a new FITS file for each frame when not using datacubes
        //
        #ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] datacube=" << this->camera.datacube() << " cubeamps=" << this->camera.cubeamps();
        logwrite( function, message.str() );
        #endif
        if ( !this->camera.datacube() || this->camera.cubeamps() ) {
          this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          if ( this->get_timer(&this->start_timer) != NO_ERROR ) {      // Archon internal timer (one tick=10 nsec)
            logwrite( function, "ERROR: could not get start time" );
            return ERROR;
          }
          this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
          error=this->camera.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: couldn't validate fits filename" );
            return error;
          }
          this->add_filename_key();                                     // add filename to system keys database

          #ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] reset extension=0 and opening new fits file" );
          #endif
          // reset the extension number and open the fits file
          //
          this->camera_info.extension = 0;
          error = this->fits_file.open_file(
                  this->camera.writekeys_when == "before", this->camera_info );
          if ( error != NO_ERROR ) {
            this->camera.log_error( function, "couldn't open fits file" );
            return error;
          }
        }

        if ( this->camera_info.exposure_time.value() != 0 ) {           // wait for the exposure delay to complete (if there is one)
          error = this->wait_for_exposure();
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: waiting for exposure" );
            return error;
          }
        }

        if (this->camera.writekeys_when=="after") this->copy_keydb();   // copy the ACF and userkeys database into camera_info

        error = this->wait_for_readout();                               // Wait for the readout into frame buffer,

        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: waiting for readout" );
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
          return error;
        }

        error = read_frame();                                           // then read the frame buffer to host (and write file) when frame ready.
        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: reading frame buffer" );
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
          return error;
        }

        // For non-sequence multiple exposures, including cubeamps, close the fits file here
        //
        if ( !this->camera.datacube() || this->camera.cubeamps() ) {    // Error or not, close the file.
          #ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] closing fits file (1)" );
          #endif
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info ); // close the file when not using datacubes
          this->camera.increment_imnum();                           // increment image_num when fitsnaming == "number"

          // ASYNC status message on completion of each file
          //
          message.str(""); message << "FILE:" << this->camera_info.fits_name << " COMPLETE";
          this->camera.async.enqueue( message.str() );
          logwrite( function, message.str() );
        }

        if (error != NO_ERROR) break;                               // should be impossible but don't try additional sequences if there were errors

      }  // end of sequence loop, while (nseq-- > 0)

    } else if ( mode == "RAW") {
      error = this->get_frame_status();                             // Get the current frame buffer status
      if (error != NO_ERROR) {
        logwrite( function, "ERROR: unable to get frame status" );
        return ERROR;
      }
      error = this->camera.get_fitsname( this->camera_info.fits_name ); // Assemble the FITS filename
      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: couldn't validate fits filename" );
        return error;
      }
      this->add_filename_key();                                     // add filename to system keys database

      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;  // copy the systemkeys database into camera_info

      this->copy_keydb();                                           // copy the ACF and userkeys databases into camera_info

      error = this->fits_file.open_file(this->camera.writekeys_when == "before", this->camera_info );
      if ( error != NO_ERROR ) {
        this->camera.log_error( function, "couldn't open fits file" );
        return error;
      }
      error = read_frame();                    // For raw mode just read immediately
      this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
      this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"
    }

    // for multi-exposure (non-cubeamp) cubes, close the FITS file now that they've all been written
    //
    if ( this->camera.datacube() && !this->camera.cubeamps() ) {
      #ifdef LOGLEVEL_DEBUG
      logwrite( function, "[DEBUG] closing fits file (2)" );
      #endif
      this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
      this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"

      // ASYNC status message on completion of each file
      //
      message.str(""); message << "FILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
      this->camera.async.enqueue( message.str() );
      error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
    }

    // remember the cubeamps setting used for the last completed exposure
    // TODO revisit once region-of-interest is implemented
    //
    this->lastcubeamps = this->camera.cubeamps();

    return (error);
  }
  /**************** Archon::Interface::expose *********************************/


  /**************** Archon::Interface::hsetup ********************************/
  /**
    * @fn     hsetup
    * @brief  setup archon for h2rg
    * @param  NONE
    * @return ERROR or NO_ERROR
    *
    * NOTE: this assumes LVDS is module 10
    * This function sets output to Pad B and HIGHOHM
    * The register reset of H2RGMainReset is already done
    * if you power on and then setp Start 1
    *
    */
    long Interface::hsetup() {
        std::string function = "Archon::Interface::hsetup";
        std::stringstream message;
        std::string reg;
        long error = NO_ERROR;

        // Enable output to Pad B and HIGHOHM
        error = this->inreg("10 1 16402");      // 0100 000000010010
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (error != NO_ERROR) {
            message.str(""); message << "enabling output to Pad B and HIGHOHM";
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        return (error);
    }
    /**************** Archon::Interface::hsetup *******************************/

    /**************** Archon::Interface::hroi ******************************/
    /**
      * @fn     hroi
      * @brief  set window limits for h2rg
      * @param  geom_in  string, with vstart vstop hstart hstop in pixels
      * @return ERROR or NO_ERROR
      *
      * NOTE: this assumes LVDS is module 10
      * This function does the following:
      *  1) sets limits of sub window using input params
      *
      */
    long Interface::hroi(std::string geom_in, std::string &retstring) {
        std::string function = "Archon::Interface::hroi";
        std::stringstream message;
        std::stringstream cmd;
        std::string dontcare;
        int hstart, hstop, vstart, vstop;
        long error = NO_ERROR;
        std::vector<std::string> tokens;

        // If geom_in is not supplied then set geometry to full frame.
        //
        if ( !geom_in.empty() ) {         // geometry arguments passed in
            Tokenize(geom_in, tokens, " ");

            if (tokens.size() != 4) {
                message.str(""); message << "param expected 4 arguments (vstart, vstop, hstart, hstop) but got " << tokens.size();
                this->camera.log_error( function, message.str() );
                return ERROR;
            }
            try {
                vstart = std::stoi( tokens[0] ); // test that inputs are integers
                vstop = std::stoi( tokens[1] );
                hstart = std::stoi( tokens[2] );
                hstop = std::stoi( tokens[3]);

            } catch (std::invalid_argument &) {
                message.str(""); message << "unable to convert geometry values: " << geom_in << " to integer";
                this->camera.log_error( function, message.str() );
                return ERROR;

            } catch (std::out_of_range &) {
                message.str(""); message << "geometry values " << geom_in << " outside integer range";
                this->camera.log_error( function, message.str() );
                return ERROR;
            }

            // Validate values are within detector
            if ( vstart < 0 || vstop > 2047 || hstart < 0 || hstop > 2047) {
                message.str(""); message << "geometry values " << geom_in << " outside pixel range";
                this->camera.log_error( function, message.str());
                return ERROR;
            }
            // Validate values have proper ordering
            if (vstart >= vstop || hstart >= hstop) {
                message.str(""); message << "geometry values " << geom_in << " are not correctly ordered";
                this->camera.log_error( function, message.str());
                return ERROR;
            }

            // Set detector registers and record limits
            // vstart 1000 000000000000 = 32768
            cmd.str("") ; cmd << "10 1 " << (32768 + vstart);
            error = this->inreg(cmd.str());
            if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
            if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0
            if (error == NO_ERROR) this->win_vstart = vstart; // set y lo lim
            // vstop 1001 000000000000 = 36864
            cmd.str("") ; cmd << "10 1 " << (36864 + vstop);
            if (error == NO_ERROR) error = this->inreg(cmd.str());
            if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
            if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0
            if (error == NO_ERROR) this->win_vstop = vstop; // set y hi lim
            // hstart 1010 000000000000 = 40960
            cmd.str("") ; cmd << "10 1 " << (40960 + hstart);
            if (error == NO_ERROR) error = this->inreg(cmd.str());
            if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
            if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0
            if (error == NO_ERROR) this->win_hstart = hstart; // set x lo lim
            // hstop 1011 000000000000 = 45056
            cmd.str("") ; cmd << "10 1 " << (45056 + hstop);
            if (error == NO_ERROR) error = this->inreg(cmd.str());
            if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
            if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0
            if (error == NO_ERROR) this->win_hstop = hstop; // set roi x hi lim

            // If we are in window mode, make adjustments to geometries
            if (this->is_window) {
                // Now set params
                int rows = (this->win_vstop - this->win_vstart) + 1;
                int cols = (this->win_hstop - this->win_hstart) + 1;
                cmd.str("");
                if (error == NO_ERROR) {
                    cmd << "H2RG_win_columns " << cols;
                    error = this->set_parameter(cmd.str());
                }
                cmd.str("");
                if (error == NO_ERROR) {
                    cmd << "H2RG_win_rows " << rows;
                    error = this->set_parameter(cmd.str());
                }

                // Now set CDS
                cmd.str("");
                cmd << "PIXELCOUNT " << cols;
                error = this->cds(cmd.str(), dontcare);
                cmd.str("");
                cmd << "LINECOUNT " << rows;
                error = this->cds(cmd.str(), dontcare);

                // update modemap, in case someone asks again
                std::string mode = this->camera_info.current_observing_mode;

                this->modemap[mode].geometry.linecount = rows;
                this->modemap[mode].geometry.pixelcount = cols;
                this->camera_info.region_of_interest[0] = this->win_hstart;
                this->camera_info.region_of_interest[1] = this->win_hstop;
                this->camera_info.region_of_interest[2] = this->win_vstart;
                this->camera_info.region_of_interest[3] = this->win_vstop;
                this->camera_info.detector_pixels[0] = cols;
                this->camera_info.detector_pixels[1] = rows;

                this->camera_info.set_axes();
            }

        }   // end if geom passed in

        // prepare the return value
        //
        message.str(""); message << this->win_vstart << " " << this->win_vstop << " " << this->win_hstart << " " << this->win_hstop;
        retstring = message.str();

        if (error != NO_ERROR) {
            message.str(""); message << "setting window geometry to " << retstring;
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        return (error);
    }
    /**************** Archon::Interface::hroi *********************************/

    /**************** Archon::Interface::hwindow ******************************/
    /**
      * @fn     hwindow
      * @brief  set into/out of window mode for h2rg
      * @param  state_in, string "TRUE, FALSE, 0, or 1"
      * @return ERROR or NO_ERROR
      *
      * NOTE: this assumes LVDS is module 10
      * This function does the following:
      *  1) puts h2rg into or out of window mode
      *
      */
    long Interface::hwindow(std::string state_in, std::string &state_out) {
        std::string function = "Archon::Interface::hwindow";
        std::stringstream message;
        std::string reg;
        std::string nowin_mode = "DEFAULT";
        std::string win_mode = "GUIDING";
        std::string dontcare;
        std::stringstream cmd;
        long error = NO_ERROR;

        // If something is passed then try to use it to set the window state
        //
        if ( !state_in.empty() ) {
            try {
                std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase

                if ( state_in == "FALSE" || state_in == "0" ) { // leave window mode
                    this->is_window = false;
                    // Set detector out of window mode
                    error = this->inreg("10 1 28684"); // 0111 000000001100
                    if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
                    if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0

                    // reset taplines
                    cmd.str("");
                    cmd << "TAPLINES " << this->taplines_store;
                    this->cds(cmd.str(), dontcare);
                    cmd.str("");
                    cmd << "TAPLINE0 " << this->tapline0_store;
                    this->cds(cmd.str(), dontcare);

                    // Set camera mode
                    // This resets all internal buffer geometries
                    this->set_camera_mode(nowin_mode);

                    // Now set CDS
                    cmd.str("");
                    cmd << "PIXELCOUNT " << this->modemap[nowin_mode].geometry.pixelcount;
                    error = this->cds( cmd.str(), dontcare );
                    cmd.str("");
                    cmd << "LINECOUNT " << this->modemap[nowin_mode].geometry.linecount;
                    error = this->cds( cmd.str(), dontcare );

                    // Issue Abort to complete window mode exit
                    cmd.str("");
                    if (error == NO_ERROR) {
                        cmd << "Abort 1 ";
                        error = this->set_parameter( cmd.str() );
                    }

                } else if ( state_in == "TRUE" || state_in == "1" ) {  // enter window mode
                    this->is_window = true;
                    // Set detector into window mode
                    error = this->inreg("10 1 28687"); // 0111 000000001111
                    if (error == NO_ERROR) error = this->inreg("10 0 1"); // send to detector
                    if (error == NO_ERROR) error = this->inreg("10 0 0"); // reset to 0

                    // Adjust taplines
                    std::string taplines_str;
                    this->cds("TAPLINES", taplines_str);
                    this->taplines_store = std::stoi(taplines_str);
                    this->cds("TAPLINES 1", dontcare);
                    this->taplines = 1;

                    std::string tapline0;
                    this->cds("TAPLINE0", tapline0);
                    this->tapline0_store = tapline0;
                    this->cds("TAPLINE0 AM33L,1,0", dontcare);

                    // Set camera mode to win_mode
                    this->set_camera_mode(win_mode);

                    // Now set params
                    int rows = (this->win_vstop - this->win_vstart) + 1;
                    int cols = (this->win_hstop - this->win_hstart) + 1;
                    if (error == NO_ERROR) {
                        cmd << "H2RG_win_columns " << cols;
                        error = this->set_parameter( cmd.str() );
                    }
                    cmd.str("");
                    if (error == NO_ERROR) {
                        cmd << "H2RG_win_rows " << rows;
                        error = this->set_parameter( cmd.str() );
                    }

                    // Now set CDS
                    cmd.str("");
                    cmd << "PIXELCOUNT " << cols;
                    error = this->cds( cmd.str(), dontcare );
                    cmd.str("");
                    cmd << "LINECOUNT " << rows;
                    error = this->cds( cmd.str(), dontcare );

                    // update modemap, in case someone asks again
                    std::string mode = this->camera_info.current_observing_mode;

                    // Adjust geometry parameters and camera_info
                    this->modemap[mode].geometry.linecount = rows;
                    this->modemap[mode].geometry.pixelcount = cols;
                    this->camera_info.region_of_interest[0] = this->win_hstart;
                    this->camera_info.region_of_interest[1] = this->win_hstop;
                    this->camera_info.region_of_interest[2] = this->win_vstart;
                    this->camera_info.region_of_interest[3] = this->win_vstop;
                    this->camera_info.detector_pixels[0] = cols;
                    this->camera_info.detector_pixels[1] = rows;

                    this->camera_info.set_axes();

                } else {
                    message.str(""); message << "window state " << state_in << " is invalid. Expecting {true,false,0,1}";
                    this->camera.log_error( function, message.str() );
                    return ERROR;
                }

            } catch (...) {
                message.str(""); message << "unknown exception converting window state " << state_in << " to uppercase";
                this->camera.log_error( function, message.str() );
                return ERROR;
            }
        }

        state_out = ( this->is_window ? "true" : "false" );

        if (error != NO_ERROR) {
            message.str(""); message << "setting window state to " << state_in;
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        return (error);
    }
    /**************** Archon::Interface::hwindow *******************************/

    /**************** Archon::Interface::autofetch ******************************/
    /**
      * @fn     autofetch
      * @brief  set into/out of autofetch mode for archon
      * @param  state_in, string "TRUE, FALSE, 0, or 1"
      * @return ERROR or NO_ERROR
      *
      * NOTE: this assumes LVDS is module 10
      * This function does the following:
      *  1) puts archon into or out of autofetch mode
      *
      */
    long Interface::autofetch(std::string state_in, std::string &state_out) {
        std::string function = "Archon::Interface::autofetch";
        std::stringstream message;
        std::string reg;
        std::string nowin_mode = "DEFAULT";
        std::string win_mode = "GUIDING";
        std::string dontcare;
        std::stringstream cmd;
        long error = NO_ERROR;

        // If something is passed then try to use it to set the autofetch state
        //
        if ( !state_in.empty() ) {
            try {
                std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase

                if ( state_in == "FALSE" || state_in == "0" ) { // leave autofetch mode
                    this->is_autofetch = false;
                    // Set detector out of autofetch mode
                    // Now send the AUTOFETCHx command
                    //
                    std::stringstream autofetchstr;
                    autofetchstr << "AUTOFETCH0";

                    if (error == NO_ERROR) error = this->archon_cmd(autofetchstr.str());

                    if (error != NO_ERROR) {
                        message << "unsetting autofetch mode: ";
                    } else {
                        message << "unset autofetch mode: ";
                    }

                    logwrite(function, message.str());

                } else if ( state_in == "TRUE" || state_in == "1" ) {  // enter window mode
                    this->is_autofetch = true;
                    // Set detector into autofetch mode
                    // Now send the AUTOFETCHx command
                    //
                    std::stringstream autofetchstr;
                    autofetchstr << "AUTOFETCH1";

                    if (error == NO_ERROR) error = this->archon_cmd(autofetchstr.str());

                    if (error != NO_ERROR) {
                        message << "setting autofetch mode: ";
                    } else {
                        message << "set autofetch mode: ";
                    }

                    logwrite(function, message.str());
                } else {
                    message.str(""); message << "autofetch state " << state_in << " is invalid. Expecting {true,false,0,1}";
                    this->camera.log_error( function, message.str() );
                    return ERROR;
                }

            } catch (...) {
                message.str(""); message << "unknown exception converting autofetch state " << state_in << " to uppercase";
                this->camera.log_error( function, message.str() );
                return ERROR;
            }
        }

        state_out = ( this->is_autofetch ? "true" : "false" );

        if (error != NO_ERROR) {
            message.str(""); message << "setting autofetch state to " << state_in;
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        return (error);
    }
    /**************** Archon::Interface::autofetch *******************************/

    /**************** Archon::Interface::hexpose ******************************/
    /**
     * @fn     hexpose
     * @brief  initiate an exposure for h2rg
     * @param  nseq_in string, if set becomes the number of sequences
     * @return ERROR or NO_ERROR
     *
     * This function does the following before returning successful completion:
     *  1) trigger an Archon exposure by setting the EXPOSE parameter = nseq_in
     *  2) wait for exposure delay
     *  3) wait for readout into Archon frame buffer
     *  4) read frame buffer from Archon to host
     *  5) Do NOT write frame to disk (eventually to shared memory)
     *
     * Note that this assumes that the Archon ACF has been programmed to automatically
     * read out the detector into the frame buffer after an exposure.
     *
     */
    long Interface::hexpose(std::string nseq_in) {
        std::string function = "Archon::Interface::hexpose";
        std::stringstream message;
        long error = NO_ERROR;
        std::string nseqstr;
        int nseq, finalframe, nread, currentindex;

        std::string mode = this->camera_info.current_observing_mode;            // local copy for convenience

        if ( ! this->modeselected ) {
            this->camera.log_error( function, "no mode selected" );
            return ERROR;
        }

        // exposeparam is set by the configuration file
        // check to make sure it was set, or else expose won't work
        if (this->exposeparam.empty()) {
            message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
            this->camera.log_error( function, message.str() );
            return ERROR;
        }

        // If the exposure time or longexposure mode were never set then read them from the Archon.
        // This ensures that, if the client doesn't set these values then the server will have the
        // same default values that the ACF has, rather than hope that the ACF programmer picks
        // their defaults to match mine.
        if ( ! this->camera_info.exposure_time.is_set() ) {
            logwrite( function, "NOTICE:exptime has not been set--will read from Archon" );
            this->camera.async.enqueue( "NOTICE:exptime has not been set--will read from Archon" );

            // read the Archon configuration memory
            //
            std::string etime;
            if ( read_parameter( "exptime", etime ) != NO_ERROR ) {
              logwrite( function, "ERROR: reading \"exptime\" parameter from Archon" );
               return ERROR;
            }

            // Tell the server these values
            //
            std::string retval;
            if ( this->exptime( etime, retval ) != NO_ERROR ) { logwrite( function, "ERROR: setting exptime" ); return ERROR; }
        }

        if ( ! this->is_longexposure_set && ! this->longexposeparam.empty() ) {
            logwrite( function, "NOTICE:longexposure has not been set--will read from Archon" );
            this->camera.async.enqueue( "NOTICE:longexposure has not been set--will read from Archon" );

            // read the Archon configuration memory
            //
            std::string lexp;
            if ( read_parameter( this->longexposeparam, lexp ) != NO_ERROR ) {
              logwrite( function, "ERROR: reading \"longexposure\" parameter from Archon" );
               return ERROR;
            }

            // Tell the server these values
            //
            std::string retval;
            if ( this->longexposure( lexp, retval ) != NO_ERROR ) {
              logwrite( function, "ERROR setting longexposure" );
              return ERROR;
            }
        }

        // If nseq_in is not supplied then set nseq to 1.
        if ( nseq_in.empty() ) {
            nseq = 1;
            nseqstr = std::to_string( nseq );

        } else {                    // sequence argument passed in
            try {
                nseq = std::stoi( nseq_in ) + this->camera_info.num_pre_exposures;      // test that nseq_in is an integer
                nseqstr = std::to_string( nseq );                           // before trying to use it

            } catch (std::invalid_argument &) {
                message.str(""); message << "unable to convert sequences: " << nseq_in << " to integer";
                this->camera.log_error( function, message.str() );
                return ERROR;

            } catch (std::out_of_range &) {
                message.str(""); message << "sequences " << nseq_in << " outside integer range";
                this->camera.log_error( function, message.str() );
                return ERROR;
            }
        }

        // Always initialize the extension number because someone could
        // set datacube true and then send "expose" without a number.
        this->camera_info.extension = 0;

        // initialize frame parameters (index, etc.)
        error = this->get_frame_status();
        currentindex = this->frame.index;

        if (error != NO_ERROR) {
            logwrite( function, "ERROR: unable to get frame status" );
            return ERROR;
        }
        // save the last frame number acquired (wait_for_readout will need this)
        this->lastframe = this->frame.bufframen[this->frame.index];

        // calculate the last frame to handle dropped frames correctly
        finalframe = this->lastframe + nseq;

        if (nseq > 1) {
            message.str(""); message << "starting sequence of " << nseq << " frames. lastframe=" << this->lastframe << " last buffer=" << currentindex+1;
            logwrite(function, message.str());
        }

        // Allocate image buffer once
        this->camera_info.frame_type = Camera::FRAME_IMAGE;
        error = this->prepare_image_buffer();
        if (error == ERROR) {
            logwrite( function, "ERROR: unable to allocate an image buffer" );
            return ERROR;
        }

        // initiate the exposure here
        logwrite(function, "exposure starting");
        error = this->prep_parameter(this->exposeparam, nseqstr);
        if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);
        if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: could not initiate exposure" );
            return error;
        }

        // get system time and Archon's timer after exposure starts
        // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
        // this->camera_info.start_time = get_timestamp();                 // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
        // if ( this->get_timer(&this->start_timer) != NO_ERROR ) {        // Archon internal timer (one tick=10 nsec)
        //     logwrite( function, "ERROR: could not get start time" );
        //     return ERROR;
        // }
        // this->add_filename_key();                                       // add filename to system keys database

        // Wait for Archon frame buffer to be ready,
        // then read the latest ready frame buffer to the host. If this
        // is a sequence, then loop over all expected frames.

        //
        // -- MAIN SEQUENCE LOOP --
        nread = 0;          // Keep track of how many we actually read
        int ns = nseq;      // Iterate with ns, to preserve original request
        while (ns-- > 0 && this->lastframe < finalframe) {

            // if ( !this->camera.datacube() || this->camera.cubeamps() ) {
            //    this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
            //    if ( this->get_timer(&this->start_timer) != NO_ERROR ) {      // Archon internal timer (one tick=10 nsec)
            //        logwrite( function, "ERROR: could not get start time" );
            //        return ERROR;
            //    }
                // this->add_filename_key();                                     // add filename to system keys database
            // }

            // wait for the exposure delay to complete (if there is one)
            if ( this->camera_info.exposure_time.value() != 0 ) {
                error = this->wait_for_exposure();
                if ( error != NO_ERROR ) {
                    logwrite( function, "ERROR: waiting for exposure" );
                    return error;
                }
            }

            // Wait for the readout into frame buffer,
            error = this->hwait_for_readout();
            if ( error != NO_ERROR ) {
                logwrite( function, "ERROR: waiting for readout" );
                return error;
            }

            // then read the frame buffer to host (and write file) when frame ready.
            error = hread_frame();
            if ( error != NO_ERROR ) {
                logwrite( function, "ERROR: reading frame buffer" );
                return error;
            }

            // ASYNC status message on completion of each readout
            nread++;
            message.str(""); message << "READOUT COMPLETE (" << nread << " of " << nseq << " read)";
            this->camera.async.enqueue( message.str() );
            logwrite( function, message.str() );

            if (error != NO_ERROR) break;                               // should be impossible but don't try additional sequences if there were errors

        }  // end of sequence loop, while (ns-- > 0 && this->lastframe < finalframe)

        // ASYNC status message on completion of each sequence
        message.str(""); message << "READOUT SEQUENCE " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" ) << " (" << nread << " of " << nseq << " read)";
        this->camera.async.enqueue( message.str() );
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );

        error = get_frame_status();
        if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: getting final frame status" );
            return error;
        }

        message.str(""); message << "Last frame read " << this->frame.frame << " from buffer " << this->frame.index + 1;
        logwrite( function, message.str());

        return (error);
    }
    /**************** Archon::Interface::hexpose *********************************/


    /**************** Archon::Interface::video *********************************/
    /**
     * @fn     video
     * @brief  initiate a video exposure
     * @param  nseq_in string, if set becomes the number of sequences
     * @return ERROR or NO_ERROR
     *
     * This function does the following before returning successful completion:
     *  1) trigger an Archon exposure by setting the EXPOSE parameter = nseq_in
     *  2) wait for exposure delay
     *  3) wait for readout into Archon frame buffer
     *  4) read frame buffer from Archon to host
     *  5) do NOT write frame to disk
     *
     * Note that this assumes that the Archon ACF has been programmed to automatically
     * read out the detector into the frame buffer after an exposure.
     *
     */

    long Interface::video() {
      std::string function = "Archon::Interface::video";
      std::stringstream message;
      long error = NO_ERROR;
      std::string nseqstr;
      int nseq;

      std::string mode = this->camera_info.current_observing_mode;            // local copy for convenience

      if ( ! this->modeselected ) {
          this->camera.log_error( function, "no mode selected" );
          return ERROR;
      }

      // When switching from cubeamps=true to cubeamps=false,
      // simply reset the mode to the current mode in order to
      // reset the image size.
      //
      // This will need to be revisited once ROI is implemented. // TODO
      //
      if ( !this->camera.cubeamps() && ( this->lastcubeamps != this->camera.cubeamps() ) ) {
          message.str(""); message << "detected change in cubeamps -- resetting camera mode to " << mode;
          logwrite( function, message.str() );
          this->set_camera_mode( mode );
      }

      // exposeparam is set by the configuration file
      // check to make sure it was set, or else expose won't work
      //
      if (this->exposeparam.empty()) {
          message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
          this->camera.log_error( function, message.str() );
          return ERROR;
      }

      // If the exposure time or longexposure mode were never set then read them from the Archon.
      // This ensures that, if the client doesn't set these values then the server will have the
      // same default values that the ACF has, rather than hope that the ACF programmer picks
      // their defaults to match mine.
      //
      if ( ! this->camera_info.exposure_time.is_set() ) {
          logwrite( function, "NOTICE:exptime has not been set--will read from Archon" );
          this->camera.async.enqueue( "NOTICE:exptime has not been set--will read from Archon" );

          // read the Archon configuration memory
          //
          std::string etime;
          if ( read_parameter( "exptime", etime ) != NO_ERROR ) { logwrite( function, "ERROR: reading \"exptime\" parameter from Archon" ); return ERROR; }

          // Tell the server these values
          //
          std::string retval;
          if ( this->exptime( etime, retval ) != NO_ERROR ) { logwrite( function, "ERROR: setting exptime" ); return ERROR; }
      }
      if ( ! this->is_longexposure_set && ! this->longexposeparam.empty() ) {
          logwrite( function, "NOTICE:longexposure has not been set--will read from Archon" );
          this->camera.async.enqueue( "NOTICE:longexposure has not been set--will read from Archon" );

          // read the Archon configuration memory
          //
          std::string lexp;
          if ( read_parameter( this->longexposeparam, lexp ) != NO_ERROR ) {
            logwrite( function, "ERROR: reading \"longexposure\" parameter from Archon" );
             return ERROR;
          }

          // Tell the server these values
          //
          std::string retval;
          if ( this->longexposure( lexp, retval ) != NO_ERROR ) {
            logwrite( function, "ERROR setting longexposure" );
            return ERROR;
          }
      }

      // If nseq_in is not supplied then set nseq to 1.
      // Add any pre-exposures onto the number of sequences.
      //

      nseq = 1 + this->camera_info.num_pre_exposures;

      // Always initialize the extension number because someone could
      // set datacube true and then send "expose" without a number.
      //
      this->camera_info.extension = 0;

      error = this->get_frame_status();  // TODO is this needed here?

      if (error != NO_ERROR) {
          logwrite( function, "ERROR: unable to get frame status" );
          return ERROR;
      }
      this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

      // initiate the exposure here
      //
      error = this->prep_parameter(this->exposeparam, nseqstr);
      if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);
      if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: could not initiate exposure" );
          return error;
      }

      // get system time and Archon's timer after exposure starts
      // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
      //
      this->camera_info.start_time = get_timestamp();                 // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      if ( this->get_timer(&this->start_timer) != NO_ERROR ) {        // Archon internal timer (one tick=10 nsec)
          logwrite( function, "ERROR: could not get start time" );
          return ERROR;
      }
      this->camera.set_fitstime(this->camera_info.start_time);        // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
      error=this->camera.get_fitsname(this->camera_info.fits_name);   // assemble the FITS filename
      if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: couldn't validate fits filename" );
          return error;
      }

      this->add_filename_key();                                       // add filename to system keys database

      logwrite(function, "exposure started");

      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;    // copy the systemkeys database object into camera_info

      if (this->camera.writekeys_when=="before") this->copy_keydb();  // copy the ACF and userkeys database into camera_info

      // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension data cube,
      // one extension for the image and a separate extension for raw data.
      //
      if ( (mode != "RAW") && (this->modemap[mode].rawenable) ) {
          if ( !this->camera.datacube() ) {                                   // if datacube not already set then it must be overridden here
              this->camera.async.enqueue( "NOTICE:override datacube true" );  // let everyone know
              logwrite( function, "NOTICE:override datacube true" );
              this->camera.datacube(true);
          }
          this->camera_info.extension = 0;
      }

      // Save the datacube state in camera_info so that the FITS writer can know about it
      //
      this->camera_info.iscube = this->camera.datacube();

      // Open the FITS file now for cubes
      //
      if ( this->camera.datacube() && !this->camera.cubeamps() ) {
          #ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] opening fits file for multi-exposure sequence data cube" );
          #endif
          error = this->fits_file.open_file(
                  this->camera.writekeys_when == "before", this->camera_info );
          if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't open fits file" );
              return error;
          }
      }

      //  //TODO only use camera_info -- don't use fits_info -- is this OK? TO BE CONFIRMED
      //  this->fits_info = this->camera_info;                            // copy the camera_info class, to be given to fits writer  //TODO

      //  this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

      if (nseq > 1) {
          message.str(""); message << "starting sequence of " << nseq << " frames. lastframe=" << this->lastframe;
          logwrite(function, message.str());
      }

      // If not RAW mode then wait for Archon frame buffer to be ready,
      // then read the latest ready frame buffer to the host. If this
      // is a squence, then loop over all expected frames.
      //
      if ( mode != "RAW" ) {                                          // If not raw mode then
          int expcount = 0;                                             // counter used only for tracking pre-exposures

          //
          // -- MAIN SEQUENCE LOOP --
          //
          while (nseq-- > 0) {

              // Wait for any pre-exposures, first the exposure delay then the readout,
              // but then continue to the next because pre-exposures are not read from
              // the Archon's buffer.
              //
              if ( ++expcount <= this->camera_info.num_pre_exposures ) {

                  message.str(""); message << "pre-exposure " << expcount << " of " << this->camera_info.num_pre_exposures;
                  logwrite( function, message.str() );

                  if ( this->camera_info.exposure_time.value() != 0 ) {         // wait for the exposure delay to complete (if there is one)
                      error = this->wait_for_exposure();
                      if ( error != NO_ERROR ) {
                          logwrite( function, "ERROR: waiting for pre-exposure" );
                          return error;
                      }
                  }

                  error = this->wait_for_readout();                             // Wait for the readout into frame buffer,
                  if ( error != NO_ERROR ) {
                      logwrite( function, "ERROR: waiting for pre-exposure readout" );
                      return error;
                  }

                  continue;
              }

              // Open a new FITS file for each frame when not using datacubes
              //
              #ifdef LOGLEVEL_DEBUG
              message.str(""); message << "[DEBUG] datacube=" << this->camera.datacube() << " cubeamps=" << this->camera.cubeamps();
                logwrite( function, message.str() );
              #endif
              if ( !this->camera.datacube() || this->camera.cubeamps() ) {
                  this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
                  if ( this->get_timer(&this->start_timer) != NO_ERROR ) {      // Archon internal timer (one tick=10 nsec)
                      logwrite( function, "ERROR: could not get start time" );
                      return ERROR;
                  }
                  this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
                  error=this->camera.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
                  if ( error != NO_ERROR ) {
                      logwrite( function, "ERROR: couldn't validate fits filename" );
                      return error;
                  }
                  this->add_filename_key();                                     // add filename to system keys database

                  #ifdef LOGLEVEL_DEBUG
                  logwrite( function, "[DEBUG] reset extension=0 and opening new fits file" );
                  #endif
                  // reset the extension number and open the fits file
                  //
                  this->camera_info.extension = 0;
                  error = this->fits_file.open_file(
                          this->camera.writekeys_when == "before", this->camera_info );
                  if ( error != NO_ERROR ) {
                      this->camera.log_error( function, "couldn't open fits file" );
                      return error;
                  }
              }

              if ( this->camera_info.exposure_time.value() != 0 ) {           // wait for the exposure delay to complete (if there is one)
                  error = this->wait_for_exposure();
                  if ( error != NO_ERROR ) {
                      logwrite( function, "ERROR: waiting for exposure" );
                      return error;
                  }
              }

              if (this->camera.writekeys_when=="after") this->copy_keydb();   // copy the ACF and userkeys database into camera_info

              error = this->wait_for_readout();                               // Wait for the readout into frame buffer,

              if ( error != NO_ERROR ) {
                  logwrite( function, "ERROR: waiting for readout" );
                  this->fits_file.close_file(
                          this->camera.writekeys_when == "after", this->camera_info );
                  return error;
              }

              error = read_frame();                                           // then read the frame buffer to host (and write file) when frame ready.
              if ( error != NO_ERROR ) {
                  logwrite( function, "ERROR: reading frame buffer" );
                  this->fits_file.close_file(
                          this->camera.writekeys_when == "after", this->camera_info );
                  return error;
              }

              // For non-sequence multiple exposures, including cubeamps, close the fits file here
              //
              if ( !this->camera.datacube() || this->camera.cubeamps() ) {    // Error or not, close the file.
                  #ifdef LOGLEVEL_DEBUG
                  logwrite( function, "[DEBUG] closing fits file (1)" );
                  #endif
                  this->fits_file.close_file(
                          this->camera.writekeys_when == "after", this->camera_info ); // close the file when not using datacubes
                  this->camera.increment_imnum();                           // increment image_num when fitsnaming == "number"

                  // ASYNC status message on completion of each file
                  //
                  message.str(""); message << "FILE:" << this->camera_info.fits_name << " COMPLETE";
                  this->camera.async.enqueue( message.str() );
                  logwrite( function, message.str() );
              }

              if (error != NO_ERROR) break;                               // should be impossible but don't try additional sequences if there were errors

          }  // end of sequence loop, while (nseq-- > 0)

      } else if ( mode == "RAW") {
          error = this->get_frame_status();                             // Get the current frame buffer status
          if (error != NO_ERROR) {
              logwrite( function, "ERROR: unable to get frame status" );
              return ERROR;
          }
          error = this->camera.get_fitsname( this->camera_info.fits_name ); // Assemble the FITS filename
          if ( error != NO_ERROR ) {
              logwrite( function, "ERROR: couldn't validate fits filename" );
              return error;
          }
          this->add_filename_key();                                     // add filename to system keys database

          this->camera_info.systemkeys.keydb = this->systemkeys.keydb;  // copy the systemkeys database into camera_info

          this->copy_keydb();                                           // copy the ACF and userkeys databases into camera_info

          error = this->fits_file.open_file(
                  this->camera.writekeys_when == "before", this->camera_info );
          if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't open fits file" );
              return error;
          }
          error = read_frame();                    // For raw mode just read immediately
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
          this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"
      }

      // for multi-exposure (non-cubeamp) cubes, close the FITS file now that they've all been written
      //
      if ( this->camera.datacube() && !this->camera.cubeamps() ) {
          #ifdef LOGLEVEL_DEBUG
          logwrite( function, "[DEBUG] closing fits file (2)" );
          #endif
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
          this->camera.increment_imnum();          // increment image_num when fitsnaming == "number"

          // ASYNC status message on completion of each file
          //
          message.str(""); message << "FILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
          this->camera.async.enqueue( message.str() );
          error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }

      // remember the cubeamps setting used for the last completed exposure
      // TODO revisit once region-of-interest is implemented
      //
      this->lastcubeamps = this->camera.cubeamps();

      return (error);
    }
    /**************** Archon::Interface::video *********************************/


  /***** Archon::Interface::wait_for_exposure *********************************/
  /**
   * @brief      creates a wait until the exposure delay has completed
   * @return     ERROR | NO_ERROR
   *
   * This is not the actual exposure delay, nor does it accurately time the exposure
   * delay. This function merely creates a reasonably accurate wait on the host to
   * allow time for the Archon to complete its exposure delay. This is done by using
   * the exposure time given to the Archon and by using the Archon's internal timer,
   * which is queried here. There is no sense in polling the Archon's timer for the
   * entire exposure time, so this function waits internally for about 90% of the
   * exposure time, then only starts polling the Archon for the remaining time.
   *
   * A prediction is made of what the Archon's timer will be at the end, in order
   * to provide an estimate of completion.
   *
   */
  long Interface::wait_for_exposure() {
    std::string function = "Archon::Interface::wait_for_exposure";
    std::stringstream message;
    long error = NO_ERROR;

    // Predicting when the exposure ends requires knowing when it started.
    // The Archon timer was read when the exposure was initiated but that
    // may generate multiple frames so each time through here requires
    // getting the start time. It's not accurate but good enough for this.
    //
    unsigned long int archon_start_time;
    error = this->get_timer(&archon_start_time);                  // Archon internal timer (one tick=10 nsec)

    // set a max timeout time of 1000 ms more than the exposure time
    //
    uint32_t exposure_timeout_time = this->camera_info.exposure_time.ms() + 1000;  // Time to wait in msec for the exposure delay to time out

    // Wait for 1000 ms less than the exposure time
    // unless the waittime works out to be less than 1000 ms then
    // don't wait at all, just start polling.
    //
    uint32_t waittime=0;
    if ( this->camera_info.exposure_time.ms() > 1000 ) {
      waittime = this->camera_info.exposure_time.ms() - 1000;
    }
    waittime = ( waittime < 1000 ? 0 : waittime );

    // Wait, (don't sleep) for the above waittime.
    // This is a period that could be aborted by setting the this->abort flag. //TODO not yet implemented?
    // All that is happening here is a wait -- there is no Archon polling going on here.
    //

    // Prediction is the predicted finish_timer, used to compute exposure time progress,
    // and is computed as start_time + exposure_time in Archon ticks.
    // Each Archon tick is 10 nsec (1e8 sec) and exposure_time.value() is in ms so multiply
    // exposure time by MSEC_TO_TICK.
    //
    unsigned long int prediction = archon_start_time + this->camera_info.exposure_time.ms() * MSEC_TO_TICK;

    {
    auto tstart  = std::chrono::steady_clock::now();
    while (true) {
      auto tnow    = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();
      if ( elapsed > waittime ) break;
      std::this_thread::sleep_for( std::chrono::microseconds( 10 ));
    }
    }

    // If the waittime is more than 1000 ms then publish exposure progress
    //
/*** TODO optionally broadcast progress
    {
    std::cerr << "exposure progress: ";
    auto tstart  = std::chrono::steady_clock::now();
    auto tend    = tstart + std::chrono::seconds( static_cast<uint32_t>(std::ceil( total_s )) );
    while ( !this->abort && std::chrono::steady_clock::now() < tend ) {
      auto elapsed = std::chrono::steady_clock::now() - tstart;
      double elapsed_s = std::chrono::duration<double>(elapsed).count();
      this->camera_info.exposure_progress = 100.0 * ( elapsed_s / total_s );
      std::cerr << std::setw(3) << static_cast<int>(std::round(this->camera_info.exposure_progress)) << "\b\b\b";

      // ASYNC status message reports the elapsed time in the chosen unit
      //
      message.str(""); message << "EXPOSURE:" << (int)(this->camera_info.exposure_time.value() - (this->camera_info.exposure_progress * this->camera_info.exposure_time.value()));
      this->camera.async.enqueue( message.str() );
//    auto tnow    = std::chrono::steady_clock::now();
//    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();
//    if ( elapsed > waittime ) break;
      std::this_thread::sleep_for( std::chrono::milliseconds( 1 ));
    }
    }
***/

    if (this->abort) {
      std::cerr << "\n";
      logwrite(function, "exposure aborted");
      return NO_ERROR;
    }

    // Now start polling the Archon for the last remaining portion of the exposure delay.
    // Record the start time to compare against current time and allow timing out.
    //
    bool done = false;
    std::chrono::steady_clock::time_point tstart = std::chrono::steady_clock::now();
    while (!done && !this->abort) {
      // Poll Archon's internal timer
      //
      unsigned long int archon_time_now;
      if ( (error=this->get_timer(&archon_time_now)) == ERROR ) {
        std::cerr << "\n";
        logwrite( function, "ERROR: could not get Archon timer" );
        break;
      }

      // update progress
      //
      this->camera_info.exposure_progress = (double)(archon_time_now - archon_start_time) / (double)(prediction - archon_start_time);
      if (this->camera_info.exposure_progress < 0 || this->camera_info.exposure_progress > 1) this->camera_info.exposure_progress=1;

      // ASYNC status message reports the elapsed time in the chosen unit
      //
      message.str(""); message << "EXPOSURE:" << (int)(this->camera_info.exposure_time.value() - (this->camera_info.exposure_progress * this->camera_info.exposure_time.value()));
      this->camera.async.enqueue( message.str() );

      std::cerr << std::setw(3) << (int)(this->camera_info.exposure_progress*100) << "\b\b\b";  // send to stderr in case anyone is watching

      // Archon timer ticks are in 10 nsec (1e-8) and exposure_time.value() is in msec
      // so multiply exposure_time.value() by MSEC_TO_TICK.
      //
      if ( (archon_time_now - archon_start_time) >= ( this->camera_info.exposure_time.ms() * MSEC_TO_TICK ) ) {
        this->finish_timer = archon_time_now;
        done  = true;
        break;
      }

      // a little pause to slow down the requests to Archon
      std::this_thread::sleep_for( std::chrono::microseconds( 100 ));

      // There's a limit to how long we let this polling loop last
      //
      std::chrono::steady_clock::time_point tnow = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();
      if ( elapsed > exposure_timeout_time ) {
        std::cerr << "\n";
        error = ERROR;
        this->camera.log_error( function, "timeout waiting for exposure" );
        break;
      }
    }  // end while (done == false && this->abort == false)
    logwrite( function, "exposure complete" );

    std::cerr << "\n";

    if (this->abort) {
      logwrite(function, "exposure aborted");
    }

    return error;
  }
  /***** Archon::Interface::wait_for_exposure *********************************/


  /**************** Archon::Interface::wait_for_readout ***********************/
  /**
   * @fn     wait_for_readout
   * @brief  creates a wait until the next frame buffer is ready
   * @param  none
   * @return ERROR or NO_ERROR
   *
   * This function polls the Archon frame status until a new frame is ready.
   *
   */
  long Interface::wait_for_readout() {
    std::string function = "Archon::Interface::wait_for_readout";
    std::stringstream message;
    long error = NO_ERROR;
    int currentframe=this->lastframe;
    bool done = false;

    message.str("");
    message << "waiting for new frame: current frame=" << this->lastframe << " current buffer=" << this->frame.index+1;
    logwrite(function, message.str());

    // waittime is 10% over the specified readout time
    // and will be used to keep track of timeout errors
    //
    double waittime;
    try {
      waittime = ceil( 1.1* this->camera.readout_time[0] );

    } catch(std::out_of_range &) {
      message.str(""); message << "readout time for Archon not found from config file";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    double clock_now     = get_clock_time();                   // get_clock_time returns seconds
    double clock_timeout = clock_now + waittime/1000.;         // must receive frame by this time

    // Poll frame status until current frame is not the last frame and the buffer is ready to read.
    // The last frame was recorded before the readout was triggered in get_frame().
    //
    while (!done && !this->abort) {
      if ( this->camera_info.exposure_time.is_longexposure() ) std::this_thread::sleep_for( std::chrono::microseconds( 10 ) );

      // Don't run get_frame status in autofetch mode
      if (this->is_autofetch) {
        // Check if data is ready on socket
        if (this->archon.Bytes_ready() > 0) {
          logwrite( function, "AUTOFETCH MODE: Bytes ready on socket: " + std::to_string(this->archon.Bytes_ready()));
          done = true;
          break;
        }
      } else {
        error = this->get_frame_status();
      }

      if (error == ERROR) {
        done = true;
        logwrite( function, "ERROR: unable to get frame status" );
        break;
      }

      // get current frame number and check the status of its buffer
      currentframe = this->frame.bufframen[this->frame.index];
      if ( (currentframe != this->lastframe) && (this->frame.bufcomplete[this->frame.index]==1) ) {
        done  = true;
        error = NO_ERROR;
        break;
      }

      // If the frame isn't done by the predicted time then
      // enough time has passed to trigger a timeout error.
      //
      if (clock_now > clock_timeout) {
        done = true;
        error = ERROR;
        message.str(""); message << "timeout waiting for new frame exceeded " << waittime << ". lastframe = " << this->lastframe;
        this->camera.log_error( function, message.str() );
        break;
      }
      clock_now = get_clock_time();

      // ASYNC status message reports the number of lines read so far,
      // which is buflines not from this->frame.index but from the NEXT index...
      //
      message.str(""); message << "LINECOUNT:" << this->frame.buflines[ this->frame.next_index ];
      #ifdef LOGLEVEL_DEBUG
      message << " [DEBUG] ";
      message << " index=" << this->frame.index << " next_index=" << this->frame.next_index << " | ";
      for ( int i=0; i < Archon::nbufs; i++ ) { message << " " << this->frame.buflines[ i ]; }
      #endif
      this->camera.async.enqueue( message.str() );

    } // end while (!done && !this->abort)

    // After exiting while loop, one update to ensure accurate ASYNC message
    // reporting of LINECOUNT.
    //

    // don't run get_frame_status() in autofetch mode
    if (!this->is_autofetch) {
      if ( error == NO_ERROR ) {
        error = this->get_frame_status();
        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: unable to get frame status" );
          return error;
        }
        message.str(""); message << "LINECOUNT:" << this->frame.buflines[ this->frame.index ];
        this->camera.async.enqueue( message.str() );
      }
    }

    if ( error != NO_ERROR ) {
      return error;
    }

    #ifdef LOGLEVEL_DEBUG
    message.str(""); 
    message << "[DEBUG] lastframe=" << this->lastframe 
            << " currentframe=" << currentframe 
            << " bufcomplete=" << this->frame.bufcomplete[this->frame.index];
    logwrite(function, message.str());
    #endif
    this->lastframe = currentframe;

    // On success, write the value to the log and return
    //
    if (!this->abort) {
      message.str("");
      message << "received currentframe: " << currentframe << " from buffer " << this->frame.index+1;
      logwrite(function, message.str());
      return NO_ERROR;

    } else if (this->abort) {
        // If the wait was stopped, log a message and return NO_ERROR
      logwrite(function, "wait for readout stopped by external signal");
      return NO_ERROR;

    } else {
        // Throw an error for any other errors (should be impossible)
      this->camera.log_error( function, "waiting for readout" );
      return error;
    }
  }
  /**************** Archon::Interface::wait_for_readout ***********************/


    /**************** Archon::Interface::hwait_for_readout ***********************/
    /**
     * @fn     hwait_for_readout
     * @brief  creates a wait until the next frame buffer is ready
     * @param  none
     * @return ERROR or NO_ERROR
     *
     * This function polls the Archon frame status until a new frame is ready.
     *
     */
    long Interface::hwait_for_readout() {
        std::string function = "Archon::Interface::hwait_for_readout";
        std::stringstream message;
        long error = NO_ERROR;
        int currentframe=this->lastframe + 1;

        message.str("");
        message << "waiting for new frame: current frame=" << this->lastframe << " current buffer=" << this->frame.index+1;
        logwrite(function, message.str());

        usleep( 700 );  // tune for size of window

        this->frame.index += 1;

        // Wrap frame.index
        if (this->frame.index >= (int)this->frame.bufframen.size()) {
            this->frame.index = 0;
        }

        this->frame.bufframen[ this->frame.index ] = currentframe;
        this->frame.frame = currentframe;

#ifdef LOGLEVEL_DEBUG
        message.str("");
    message << "[DEBUG] lastframe=" << this->lastframe
            << " currentframe=" << currentframe
            << " bufcomplete=" << this->frame.bufcomplete[this->frame.index];
    logwrite(function, message.str());
#endif
        this->lastframe = currentframe;

        // On success, write the value to the log and return
        //
        if (!this->abort) {
            message.str("");
            message << "received currentframe: " << currentframe << " from buffer " << this->frame.index+1;
            logwrite(function, message.str());
            return NO_ERROR;

        } else if (this->abort) {
            // If the wait was stopped, log a message and return NO_ERROR
            logwrite(function, "wait for readout stopped by external signal");
            return NO_ERROR;

        } else {
            // Throw an error for any other errors (should be impossible)
            this->camera.log_error( function, "waiting for readout" );
            return error;
        }
    }
    /**************** Archon::Interface::hwait_for_readout ***********************/


  /**************** Archon::Interface::get_parameter **************************/
  /**
   * @fn     get_parameter
   * @brief  get parameter using read_parameter()
   * @param  string
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::get_parameter(std::string parameter, std::string &retstring) {
    std::string function = "Archon::Interface::get_parameter";

    return this->read_parameter(parameter, retstring);
  }
  /**************** Archon::Interface::get_parameter **************************/


  /**************** Archon::Interface::set_parameter **************************/
  /**
   * @fn     set_parameter
   * @brief  set an Archon parameter
   * @param  string
   * @return ERROR or NO_ERROR
   *
   * This function calls "prep_parameter()" and "load_parameter()"
   *
   */
  long Interface::set_parameter( std::string parameter, long value ) {
    std::stringstream paramstring;
    paramstring << parameter << " " << value;
    return( set_parameter( paramstring.str() ) );
  }
  long Interface::set_parameter(std::string parameter) {
    std::string function = "Archon::Interface::set_parameter";
    std::stringstream message;
    long ret=ERROR;
    std::vector<std::string> tokens;

    Tokenize(parameter, tokens, " ");

    if (tokens.size() != 2) {
      message.str(""); message << "param expected 2 arguments (paramname and value) but got " << tokens.size();
      this->camera.log_error( function, message.str() );
      ret=ERROR;

    } else {
      ret = this->prep_parameter(tokens[0], tokens[1]);
      if (ret == NO_ERROR) ret = this->load_parameter(tokens[0], tokens[1]);
    }
    return ret;
  }
  /**************** Archon::Interface::set_parameter **************************/


  /***** Archon::Interface::exptime *******************************************/
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
  long Interface::exptime(std::string exptime_in, std::string &retstring) {
    std::string function = "Archon::Interface::exptime";
    std::stringstream message;
    long ret=NO_ERROR;

    if ( !exptime_in.empty() ) {
      if ( exptime_in.find(".") != std::string::npos ) {
      this->camera.log_error( function, "must be a whole number" );
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
          this->camera.log_error( function, "expected <exptime> [ s | ms ]" );
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
            this->camera.log_error( function, "expected <exptime> [ s | ms ]" );
            return ERROR;
          }
          // longexposure() could fail if there's an error talking to Archon
          // or if longexposure is not supported by the ACF.
          //
          if ( ret != NO_ERROR ) {
            this->camera.log_error( function, "unable to set selected unit. exptime not set." );
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
        this->camera.log_error( function, message.str() );
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
  }
  /***** Archon::Interface::exptime *******************************************/


  /** Camera::Camera::shutter *************************************************/
  /**
   * @fn     shutter
   * @brief  set or get the shutter enable state
   * @param  std::string shutter_in
   * @param  std::string& shutter_out
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::shutter(std::string shutter_in, std::string& shutter_out) {
    std::string function = "Archon::Interface::shutter";
    std::stringstream message;
    long error = NO_ERROR;
    int level=0, force=0;  // trigout level and force for activate

    if ( this->shutenableparam.empty() ) {
      this->camera.log_error( function, "SHUTENABLE_PARAM is not defined in configuration file" );
      return ERROR;
    }

    if ( !shutter_in.empty() ) {
      try {
        bool shutten;          // shutter enable state read from command
        bool ability=false;    // are we going to change the ability (enable/disable)?
        bool activate=false;   // are we going to activate the shutter (open/close)?
        std::string activate_str;
        bool dontcare;
        std::transform( shutter_in.begin(), shutter_in.end(), shutter_in.begin(), ::tolower );  // make lowercase
        if ( shutter_in == "disable" ) {
          ability = true;
          shutten = false;

        } else if ( shutter_in == "enable"  ) {
          ability = true;
          shutten = true;

        } else if ( shutter_in == "open"  ) {
          activate = true;
          force = 1;
          level = 1;
          activate_str = "open";

        } else if ( shutter_in == "close"  ) {
          activate = true;
          force = 1;
          level = 0;
          activate_str = "closed";

        } else if ( shutter_in == "reset"  ) {
          activate = true;
          force = 0;
          level = 0;
          activate_str = "";
          // shutter back to normal operation;
          // shutter force level keyword has no context so remove it from the system keys db
          //
          this->systemkeys.EraseKeys( "SHUTFORC" );

        } else {
          message.str(""); message << shutter_in << " is invalid. Expecting { enable | disable | open | close | reset }";
          this->camera.log_error( function, message.str() );
          error = ERROR;
        }
        // if changing the ability (enable/disable) then do that now
        if ( error == NO_ERROR && ability ) {
          std::stringstream cmd;
          cmd << this->shutenableparam << " " << ( shutten ? this->shutenable_enable : this->shutenable_disable );
          error = this->set_parameter( cmd.str() );

          // If parameter was set OK then save the new value
          if ( error == NO_ERROR ) this->camera_info.shutterenable = shutten;
        }
        // if changing the activation (open/close/reset) then do that now
        if ( error == NO_ERROR && activate ) {
          if ( this->configmap.find( "TRIGOUTFORCE" ) != this->configmap.end() ) {
            error = this->write_config_key( "TRIGOUTFORCE", force, dontcare );

          } else {
            this->camera.log_error( function, "TRIGOUTFORCE not found in configmap" );
            error = ERROR;
          }

          if ( this->configmap.find( "TRIGOUTLEVEL" ) != this->configmap.end() ) {
            if ( error == NO_ERROR) error = this->write_config_key( "TRIGOUTLEVEL", level, dontcare );

          } else {
            this->camera.log_error( function, "TRIGOUTLEVEL not found in configmap" );
            error = ERROR;
          }
          if ( error == NO_ERROR ) error = this->archon_cmd( APPLYSYSTEM );
          if ( error == NO_ERROR ) { this->camera_info.shutteractivate = activate_str; }
	    }

      } catch (...) {
        message.str(""); message << "converting requested shutter state: " << shutter_in << " to lowercase";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    // set the return value and report the state now, either setting or getting
    //
    shutter_out = this->camera_info.shutterenable ? "enabled" : "disabled";

    // if the shutteractivate string is not empty then use it for the return string, instead
    //
    if ( !this->camera_info.shutteractivate.empty() ) shutter_out = this->camera_info.shutteractivate;

    message.str("");
    message << "shutter is " << shutter_out;
    logwrite( function, message.str() );

    // If the shutter was forced open or closed then add that to the system keys db
    //
    if ( force ) {
      message.str(""); message << "SHUTFORC=" << level << "// shutter force level";
      this->systemkeys.addkey( message.str() );
    }

    // Add the shutter enable keyword to the system keys db
    //
    message.str(""); message << "SHUTTEN=" << ( this->camera_info.shutterenable ? "T" : "F" ) << "// shutter was enabled";
    this->systemkeys.addkey( message.str() );

    return error;
  }
  /** Camera::Camera::shutter *************************************************/


  /**************** Archon::Interface::hdrshift *******************************/
  /**
   * @fn     hdrshift
   * @brief  set/get number of hdrshift bits
   * @param  bits_in
   * @param  &bits_out
   * @return ERROR or NO_ERROR
   *
   * This function sets (or gets) this->n_hdrshift.
   *
   * In HDR mode (i.e. SAMPLEMODE=1, 32 bits per pixel) this->write_frame()
   * will right-shift the Archon data buffer by this->n_hdrshift.
   *
   */
  long Interface::hdrshift(std::string bits_in, std::string &bits_out) {
    std::string function = "Archon::Interface::hdrshift";
    std::stringstream message;
    int hdrshift_req=-1;

    // If something is passed then try to use it to set the number of hdrshifts
    //
    if ( !bits_in.empty() ) {
      try {
        hdrshift_req = std::stoi( bits_in );

      } catch ( std::invalid_argument & ) {
        message.str(""); message << "converting hdrshift: " << bits_in << " to integer";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch ( std::out_of_range & ) {
        message.str(""); message << "hdrshift: " << bits_in << " is outside integer range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    if ( hdrshift_req < 0 || hdrshift_req > 31 ) {
      this->camera.log_error( function, "hdrshift outside range {0:31}" );
      return ERROR;

    } else this->n_hdrshift = hdrshift_req;

    // error or not, the number of bits reported now will be whatever was last successfully set
    //
    bits_out = std::to_string( this->n_hdrshift );

    // add to system keyword database
    //
    std::stringstream keystr;
    keystr << "HDRSHIFT=" << this->n_hdrshift << "// number of HDR right-shift bits";
    this->systemkeys.addkey( keystr.str() );

    return NO_ERROR;
  }
  /**************** Archon::Interface::hdrshift *******************************/


  /**************** Archon::Interface::trigin *********************************/
  /**
   * @fn     trigin
   * @brief  setup Archon for external triggering of exposures
   * @param  state_in
   * @return ERROR or NO_ERROR
   *
   * This function sets three Archon parameters, as defined in
   * the configuration file for:
   *   TRIGIN_EXPOSE_PARAM
   *   TRIGIN_UNTIMED_PARAM
   *   TRIGIN_READOUT_PARAM
   *
   * Calling trigin() should be considered analagous to calling expose()
   * with the exception that the actual exposure is expected to be triggered
   * in the Archon ACF by a TRIGIN pulse.
   *
   * This trigin() function then assumes that the exposure was started and
   * then waits for the exposure delay, waits for readout, reads the frame 
   * buffer, then writes the frame to disk. In the case of "untimed" then
   * it doesn't wait for the exposure delay but returns immediately,
   * requiring a second call to trigin() with the argument "readout" in
   * order to enter the sequence of wait for readout, read frame buffer,
   * write frame to disk.
   *
   */
  long Interface::trigin( std::string state_in ) {
    std::string function = "Archon::Interface::trigin";
    std::string newstate;
    std::stringstream message;
    std::vector<std::string> tokens;
    long error = NO_ERROR;

    std::string mode = this->camera_info.current_observing_mode;            // local copy for convenience

    if ( ! this->modeselected ) {
      this->camera.log_error( function, "no mode selected" );
      return ERROR;
    }

    std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );  // make lowercase

    Tokenize( state_in, tokens, " " );                                                // break into tokens

    // Must have 1 or 2 arguments only
    //
    if ( tokens.empty() || tokens.size() > 2 ) {
      message.str(""); message << "requested trigin state: " << state_in << " is invalid. Expected { expose [N] | untimed | readout | disable }";
      this->camera.log_error( function, message.str() );
      error = ERROR;
    }

    // If the basic checks have failed then get out now, no point in continuing
    //
    if ( error != NO_ERROR ) return error;

    // Now look at the arguments, expecting { expose [N] | untimed | readout }
    // and set the Archon parameters appropriately. The assumption is that
    // after setting these parameters, the Archon will receive a TRIGIN which
    // resets the timing core so that the ACF will take appropriate action
    // based on the settings of these parameters.
    //
    try {

      // Ultimately, only will only write the parameter *IF* the parameter has been defined in the config file.
      // In other words, make no requirement on having all trigger parameters (TRIGIN_EXPOSE_PARAM,
      // TRIGIN_UNTIMED_PARAM, TRIGIN_READOUT_PARAM) defined. But if the "trigin expose" command is sent then
      // obviously the TRIGIN_EXPOSE_PARAM must be defined, and so on.
      //
      if ( tokens.at(0) == "expose" ) {     // timed exposures can take an optional argument
        // At minimum, expose requires that TRIGIN_EXPOSE_PARAM be defined
	//
        if ( this->trigin_exposeparam.empty() ) {
          message.str(""); message << "TRIGIN_EXPOSE_PARAM not defined in configuration file " << this->config.filename;
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        if ( tokens.size() == 1 ) {         // no size given so = 1
          this->trigin_expose  = 1;
          this->trigin_untimed = this->trigin_untimed_disable;
          this->trigin_readout = this->trigin_readout_disable;

        } else {                              // size given so try to set trigin_expose to requested value
          int expnum = std::stoi( tokens.at(1) );
          if ( expnum < 1 ) {
            this->camera.log_error( function, "number of timed exposures must be greater than zero" );
            error = ERROR;

          } else {
            this->trigin_expose  = expnum;
            this->trigin_untimed = this->trigin_untimed_disable;
            this->trigin_readout = this->trigin_readout_disable;
          }
        }

      } else if ( tokens.at(0) == "untimed"  ) {   // untimed exposures
        // At minimum, untimed requires that TRIGIN_UNTIMED_PARAM be defined
        if ( this->trigin_untimedparam.empty() ) {
          message.str(""); message << "TRIGIN_UNTIMED_PARAM not defined in configuration file " << this->config.filename;
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        this->trigin_expose  = 0;
        this->trigin_untimed = this->trigin_untimed_enable;
        this->trigin_readout = this->trigin_readout_disable;

      } else if ( tokens.at(0) == "readout"  ) {   // readout is used at the end of untimed exposures
        // At minimum, readout requires that TRIGIN_READOUT_PARAM be defined
        if ( this->trigin_readoutparam.empty() ) {
          message.str(""); message << "TRIGIN_READOUT_PARAM not defined in configuration file " << this->config.filename;
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        this->trigin_expose  = 0;
        this->trigin_untimed = this->trigin_untimed_disable;
        this->trigin_readout = this->trigin_readout_enable;

      } else if ( tokens.at(0) == "disable"  ) {   // used to disable untimed exposures
        this->trigin_expose  = 0;
        this->trigin_untimed = this->trigin_untimed_disable;
        this->trigin_readout = this->trigin_readout_disable;

      } else {
        message.str(""); message << "requested trigin state: " << state_in << " is invalid. Expected { expose [N] | untimed | readout | disable }";
        this->camera.log_error( function, message.str() );
        error = ERROR;
      }

      // The first token is taken to be the state name.
      // Only after the parameters have been successfully written will we set this->trigin_state = newstate
      //
      newstate = tokens.at(0);
    } catch ( std::invalid_argument & ) {
      message.str(""); message << "converting number of timed external-trigger exposures: " << state_in << " to integer";
      this->camera.log_error( function, message.str() );
      error = ERROR;

    } catch ( std::out_of_range & ) {
      message.str(""); message << "parsing trigin string: " << state_in;
      this->camera.log_error( function, message.str() );
      error = ERROR;

    } catch (...) {
      message.str(""); message << "unknown exception parsing trigin string: " << state_in;
      this->camera.log_error( function, message.str() );
      error = ERROR;
    }

    // Get out now if any internal errors from above
    //
    if ( error != NO_ERROR ) return error;

    // Build up a vector of the parameters and their values that have been defined.
    //
    typedef struct {
      std::string param;
      int value;
    } triginfo_t;

    std::vector<triginfo_t> trigger;

    // If the parameter has been defined then push that parameter and its value into the vector.
    // Check this for each of the three trigin parameters.
    //

    if ( !this->trigin_exposeparam.empty() ) {        // TRIGIN_EXPOSE
      triginfo_t expose;
      expose.param = this->trigin_exposeparam;
      expose.value = this->trigin_expose;
      trigger.push_back( expose );
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] EXPOSE param=" << this->trigin_exposeparam << " value=" << this->trigin_expose;
      logwrite( function, message.str() );
      #endif
    }

    if ( !this->trigin_untimedparam.empty() ) {       // TRIGIN_UNTIMED
      triginfo_t untimed;
      untimed.param = this->trigin_untimedparam;
      untimed.value = this->trigin_untimed;
      trigger.push_back( untimed );
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] UNTIMED param=" << this->trigin_untimedparam << " value=" << this->trigin_untimed;
      logwrite( function, message.str() );
      #endif
    }

    if ( !this->trigin_readoutparam.empty() ) {       // TRIGIN_READOUT
      triginfo_t readout;
      readout.param = this->trigin_readoutparam;
      readout.value = this->trigin_readout;
      trigger.push_back( readout );
      #ifdef LOGLEVEL_DEBUG
      message.str(""); message << "[DEBUG] READOUT param=" << this->trigin_readoutparam << " value=" << this->trigin_readout;
      logwrite( function, message.str() );
      #endif
    }

    // Now write those parameters to the Archon
    //
    for ( const auto &trig : trigger ) {
      if ( error == NO_ERROR ) error = this->set_parameter( trig.param, trig.value );
    }

    // Now that the parameters have been successfully written we are in the new state
    //
    if ( error == NO_ERROR ) this->trigin_state = newstate;

    // Save the last frame number acquired -- wait_for_readout() will need this later
    //
    if ( error == NO_ERROR) error = this->get_frame_status();
    if ( error == NO_ERROR) this->lastframe = this->frame.bufframen[this->frame.index];

    #ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] lastframe=" << this->lastframe;
    logwrite( function, message.str() );
    #endif

    // For "untimed" prepare camera_info and open the FITS file
    //
    if ( error == NO_ERROR && this->trigin_state == "untimed" ) {
      this->camera_info.start_time = get_timestamp();                // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      this->get_timer(&this->start_timer);                           // Archon internal timer (one tick=10 nsec)
      this->camera.set_fitstime(this->camera_info.start_time);       // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
      error=this->camera.get_fitsname(this->camera_info.fits_name);  // Assemble the FITS filename
      this->add_filename_key();                                      // add filename to system keys database
      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;   // copy the systemkeys database into camera_info
      if (this->camera.writekeys_when=="before") this->copy_keydb(); // copy the ACF and userkeys database into camera_info
      error = this->fits_file.open_file(this->camera.writekeys_when == "before", this->camera_info );
      logwrite( function, "untimed exposure trigger enabled" );
      return error;
    }

    // For "disable" there's nothing more to do right now (except close any open FITS file)
    //
    if ( error == NO_ERROR && this->trigin_state == "disable" ) {
      if ( this->fits_file.isopen() ) {                              // "untimed" could have opened a FITS file
        this->fits_file.close_file( false, this->camera_info );      // close the FITS file container
        std::remove( this->camera_info.fits_name.c_str() );          // remove the (empty) file it created
      }
      logwrite( function, "untimed exposure trigger disabled" );
      return error;
    }

    // Don't continue if Archon parameters were not written properly
    //
    if ( error != NO_ERROR ) return error;

    // ------------------------------------------------------------------------
    // Now the rest is somewhat like the "expose" function...
    // ------------------------------------------------------------------------

    // Always initialize the extension number because someone could
    // set datacube true and then send "expose" without a number.
    //
    this->camera_info.extension = 0;

    // ------------------------------------------------------------------------
    // "readout"
    // The assumption here is that the exposure has started and this command is
    // sent to prepare for the end of exposure. The exposure is nearly complete.
    // The next TRIGIN will end the exposure and begin the readout, so now we
    // begin waiting for the new frame to appear in the Archon frame buffer.
    // ------------------------------------------------------------------------
    //
    if ( this->trigin_state == "readout" ) {

      // Save the datacube state in camera_info so that the FITS writer can know about it
      //
      this->camera_info.iscube = this->camera.datacube();

      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;   // copy systemkeys database object into camera_info

      if (this->camera.writekeys_when=="after") this->copy_keydb();  // copy the ACF and userkeys database into camera_info

      if (error==NO_ERROR) error = this->wait_for_readout();         // Wait for the readout into frame buffer,
      if (error==NO_ERROR) error = read_frame();                     // then read the frame buffer to host (and write file) when frame ready.

      this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info ); // close the file when not using datacubes
      this->camera.increment_imnum();                                // increment image_num when fitsnaming == "number"

      // ASYNC status message on completion of each file
      //
      message.str(""); message << "FILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
      this->camera.async.enqueue( message.str() );
      error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );

    } else if ( this->trigin_state == "expose" ) {
        // ------------------------------------------------------------------------
        // "expose"
        // The assumption here is that the exposure hasn't started yet. The next
        // TRIGIN will initiate the complete exposure sequence in the Archon,
        // exposure delay followed by sensor readout, so now we are waiting for that
        // sequence to start.
        // ------------------------------------------------------------------------
        //

      // get system time and Archon's timer after exposure starts
      // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
      //
      this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
      error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)

      this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
      error=this->camera.get_fitsname(this->camera_info.fits_name); // assemble the FITS filename
      this->add_filename_key();                                     // add filename to system keys database

      this->camera_info.systemkeys.keydb = this->systemkeys.keydb;  // copy the systemkeys database into camera_info
      if (this->camera.writekeys_when=="before") this->copy_keydb();// copy the ACF and userkeys database into camera_info

      // If mode is not "RAW" but RAWENABLE is set then we're going to require a multi-extension data cube,
      // one extension for the image and a separate extension for raw data.
      //
      if ( (error == NO_ERROR) && (mode != "RAW") && (this->modemap[mode].rawenable) ) {
        if ( !this->camera.datacube() ) {                                   // if datacube not already set then it must be overridden here
          this->camera.async.enqueue( "NOTICE:override datacube true" );    // let everyone know
          logwrite( function, "NOTICE:override datacube true" );
          this->camera.datacube(true);
        }
        this->camera_info.extension = 0;
      }

      // Save the datacube state in camera_info so that the FITS writer can know about it
      //
      this->camera_info.iscube = this->camera.datacube();

      // Open the FITS file now for cubes
      //
      if ( this->camera.datacube() ) {
        error = this->fits_file.open_file(
                this->camera.writekeys_when == "before", this->camera_info );
        if ( error != NO_ERROR ) {
          this->camera.log_error( function, "couldn't open fits file" );
          return error;
        }
      }

      if (this->trigin_expose > 1) {
        message.str(""); message << "starting sequence of " << this->trigin_expose << " frames. lastframe=" << this->lastframe;
        logwrite(function, message.str());
      }

      // If not RAW mode then wait for Archon frame buffer to be ready,
      // then read the latest ready frame buffer to the host. If this
      // is a squence, then loop over all expected frames.
      //
      if ( (error == NO_ERROR) && (mode != "RAW") ) {                 // If not raw mode then
        while ( this->trigin_expose-- > 0 ) {

          // Open a new FITS file for each frame when not using datacubes
          //
          if ( !this->camera.datacube() ) {
            this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
            this->get_timer(&this->start_timer);                          // Archon internal timer (one tick=10 nsec)
            this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
            error=this->camera.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
            if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't validate fits filename" );
              return error;
            }
            this->add_filename_key();                                     // add filename to system keys database
            error = this->fits_file.open_file(
                    this->camera.writekeys_when == "before", this->camera_info );
            if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't open fits file" );
              return error;
            }
          }

          if (error==NO_ERROR && this->camera_info.exposure_time.value() != 0) {  // wait for the exposure delay to complete (if there is one)
            error = this->wait_for_exposure();
          }

          if (error==NO_ERROR) error = this->wait_for_readout();      // Wait for the readout into frame buffer,
          if (error==NO_ERROR) error = read_frame();                  // then read the frame buffer to host (and write file) when frame ready.

          if ( !this->camera.datacube() ) {                           // Error or not, close the file.
            this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info ); // close the file when not using datacubes
            this->camera.increment_imnum();                           // increment image_num when fitsnaming == "number"

            // ASYNC status message on completion of each file
            //
            message.str(""); message << "FILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
            this->camera.async.enqueue( message.str() );
            error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
          }

          if (error != NO_ERROR) break;                               // don't try additional sequences if there were errors
        }

      } else if ( (error == NO_ERROR) && (mode == "RAW") ) {
        error = this->get_frame_status();                             // Get the current frame buffer status
        if (error==NO_ERROR) error = this->camera.get_fitsname( this->camera_info.fits_name ); // Assemble the FITS filename
        this->add_filename_key();                                     // add filename to system keys database
        if (error==NO_ERROR) error = this->fits_file.open_file(
                    this->camera.writekeys_when == "before", this->camera_info );
        if (error==NO_ERROR) error = read_frame();                    // For raw mode just read immediately
        this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
        this->camera.increment_imnum();                               // increment image_num when fitsnaming == "number"
      }
      // for cubes, close the FITS file now that they've all been written
      //
      if ( this->camera.datacube() ) {
        this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
        this->camera.increment_imnum();                               // increment image_num when fitsnaming == "number"

        // ASYNC status message on completion of each file
        //
        message.str(""); message << "FILE:" << this->camera_info.fits_name << " " << ( error==NO_ERROR ? "COMPLETE" : "ERROR" );
        this->camera.async.enqueue( message.str() );
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }
        // end if ( this->trigin_state == "expose" )

    } else {
        // unknown (should be impossible)
      message.str(""); message << "unexpected error processing trigin state " << this->trigin_state;
      this->camera.log_error( function, message.str() );
      error = ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::trigin *********************************/


  /**************** Archon::Interface::copy_keydb *****************************/
  /**
   * @fn         copy_keydb
   * @brief      copy the ACF and user keyword databases into camera_info
   * @param[in]  none
   * @return     none
   *
   */
  void Interface::copy_keydb() {
    std::string function = "Archon::Interface::copy_keydb";

    // copy the userkeys database object into camera_info
    //
    this->camera_info.userkeys.keydb = this->userkeys.keydb;

    // add any keys from the ACF file (from modemap[mode].acfkeys) into the
    // camera_info.userkeys object
    //
    std::string mode = this->camera_info.current_observing_mode;
    Common::FitsKeys::fits_key_t::iterator keyit;
    for ( keyit  = this->modemap[mode].acfkeys.keydb.begin();
          keyit != this->modemap[mode].acfkeys.keydb.end();
          keyit++ ) {
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
      this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
    }

    #ifdef LOGLEVEL_DEBUG
    logwrite( function, "[DEBUG] copied userkeys db to camera_info" );
    #endif
  }
  /**************** Archon::Interface::copy_keydb *****************************/


  /***** Archon::Interface::longexposure **************************************/
  /**
   * @brief      set/get longexposure mode
   * @param[in]  string
   * @return     ERROR | NO_ERROR
   *
   */
  long Interface::longexposure(std::string state_in, std::string &retstring) {
    std::string function = "Archon::Interface::longexposure";
    std::stringstream message;
    long error = NO_ERROR;

    if ( this->longexposeparam.empty() ) {
      this->camera.log_error( function, "longexposure not supported" );
      retstring="not_supported";
      return ERROR;
    }

    // If something is passed then try to use it to set the longexposure state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
        if ( state_in == "FALSE" || state_in == "0" ) this->camera_info.exposure_time.longexposure( false );
        else
        if ( state_in == "TRUE" || state_in == "1" ) this->camera_info.exposure_time.longexposure( true );
        else {
          message.str(""); message << "longexposure state " << state_in << " is invalid. Expecting {true,false,0,1}";
          retstring="invalid_argument";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        this->is_longexposure_set = true;

        // Set the longexposure param and re-set the exposure time in the current unit
        //
        std::stringstream cmd;
        cmd << this->longexposeparam << " " << ( this->camera_info.exposure_time.is_longexposure() ? 1 : 0 );
        error |= this->set_parameter( cmd.str() );
        cmd.str(""); cmd << "exptime " << this->camera_info.exposure_time.value();
        error |= this->set_parameter( cmd.str() );

      } catch ( std::exception &e ) {
        message.str(""); message << "exception converting longexposure state: " << e.what();
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    // prepare the return value
    //
    message.str(""); message << ( this->camera_info.exposure_time.is_longexposure() ? "true " : "false " )
                             << this->camera_info.exposure_time.value() << " "
                             << this->camera_info.exposure_time.unit();
    retstring = message.str();

    return error;
  }
  /***** Archon::Interface::longexposure **************************************/


  /**************** Archon::Interface::heater *********************************/
  /**
   * @fn     heater
   * @brief  heater control, set/get state, target, PID, ramp
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   * valid args format,
   * to set or get the enable state and target for heater A or B on the specified module:
   *   <module> < A | B > [ <on | off> <target> ]
   *   possible args: 2 (get) or 3 or 4
   *
   * to set or get the PID parameters for heater A or B on the specified module:
   *   <module> < A | B > PID [ <p> <i> <d> ]
   *   possible args: 3 (get) or 6 (set)
   *
   * to set or get the ramp and ramprate for heater A or B on the specified module:
   *   <module> < A | B > RAMP [ <on | off> [ramprate] ]
   *   possible args: 3 (get) or 5 (set)
   *
   * to set or get the current limit for heater A or B on the specified module:
   *   <module> < A | B > ILIM [ <value> ]
   *   possible args: 3 (get) or 4 (set)
   *
   * to set or get the input sensor for heater A or B on the specified module:
   *   <module> < A | B > INPUT [ A | B | C ]
   *   possible args: 3 (get) or 4 (set)
   *
   */
  long Interface::heater(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::heater";
    std::stringstream message;
    std::vector<std::string> tokens;
    int module;
    std::string heaterid;                   //!< A|B
    bool readonly=false;
    float pid_p, pid_i, pid_d;              //!< requested PID values
    int ramprate;                           //!< requested ramp rate value
    float target;                           //!< requested heater target value
    std::vector<std::string> heaterconfig;  //!< vector of configuration lines to read or write
    std::vector<std::string> heatervalue;   //!< vector of values associated with config lines (for write)

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return ERROR;
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    // RAMP requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_RAMP );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_RAMP;

      } else {
        message << "requires backplane version " << REV_RAMP << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    Tokenize(args, tokens, " ");

    // At minimum there must be two tokens, <module> A|B
    //
    if ( tokens.size() < 2 ) {
      this->camera.log_error( function, "expected at least two arguments: <module> A|B" );
      return ERROR;
    }

    // As long as there are at least two tokens, get the module and heaterid
    // which will be common to everything.
    //
    try {
      module   = std::stoi( tokens[0] );
      heaterid = tokens[1];
      if ( heaterid != "A" && heaterid != "B" ) {
        message.str(""); message << "invalid heater " << heaterid << ": expected <module> A|B";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

    } catch (std::invalid_argument &) {
      message.str(""); message << "converting heater <module> " << tokens[0] << " to integer";
      this->camera.log_error( function, message.str() );
      return ERROR;

    } catch (std::out_of_range &) {
      message.str(""); message << "heater <module>: " << tokens[0] << " is outside integer range";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // check that requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return ERROR;
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "module " << module << " not a heater board";
        this->camera.log_error( function, message.str() );
        return ERROR;
    }

    // Now that we've passed the basic tests (firmware OK, basic syntax OK, and requested module is a heater)
    // go ahead and set up some needed variables.
    //

    // heater target min/max depends on backplane version
    //
    ret = compare_versions( this->backplaneversion, REV_HEATERTARGET );
    if ( ret == -999 ) {
      message.str(""); message << "comparing backplane version " << this->backplaneversion << " to " << REV_HEATERTARGET;
      this->camera.log_error( function, message.str() );
      return ERROR;

    } else if ( ret == -1 ) {
      this->heater_target_min = -150.0;
      this->heater_target_max =   50.0;

    } else {
      this->heater_target_min = -250.0;
      this->heater_target_max =   50.0;
    }

    heaterconfig.clear();
    heatervalue.clear();
    std::stringstream ss;

    // Any one heater command can require reading or writing multiple configuration lines.
    // The code will look at each case and push each heater configuration line that needs
    // to be read or written into a vector (heaterconfig). For those that need to be written,
    // the value to write will be pushed into a separate vector (heatervalue). After these
    // vectors have been built up, the code will loop through each element of the vectors,
    // reading and/or writing each.

    // Start looking at the heater arguments passed, which have been tokenized.
    // There can be only 2, 3, 4, 5, or 6 tokens allowed.

    // If there are exactly two (2) tokens then we have received only:
    // <module> A|B
    // which means we're reading the state and target.
    //
    if ( tokens.size() == 2 ) {        // no args reads ENABLE, TARGET
      readonly = true;
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
      ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );

    } else if ( tokens.size() == 3 ) {
        // If there are three (3) tokens then the 3rd must be one of the following:
        // ON | OFF (for ENABLE), <target>, PID, RAMP, ILIM
      if ( tokens[2] == "ON" ) {       // ON
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.emplace_back("1" );

      } else if ( tokens[2] == "OFF" ) {      // OFF
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.emplace_back("0" );

      } else if ( tokens[2] == "RAMP" ) {     // RAMP
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );

      } else if ( tokens[2] == "PID" ) {      // PID
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );

      } else if ( tokens[2] == "ILIM" ) {     // ILIM
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "IL"; heaterconfig.push_back( ss.str() );

      } else if ( tokens[2] == "INPUT" ) {    // INPUT
        readonly = true;
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "SENSOR"; heaterconfig.push_back( ss.str() );

      } else {                           // <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[2] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "requested heater target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "converting heater <target>=" << tokens[2] << " to float";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "heater <target>: " << tokens[2] << " outside range of float";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[2] );
      }

    } else if ( tokens.size() == 4 ) {
        // If there are four (4) tokens then the 4th must be one of the following:
        // <target>               in <module> < A | B > [ <on | off> <target> ]
        // <ramprate> | ON | OFF  in <module> < A | B > RAMP [ <on | off> [ramprate] ]
        // <value>                in <module> < A | B > ILIM [ <value> ]
        // A | B | C              in <module> < A | B > INPUT [ A | B | C ]

      if ( tokens[2] == "ON" ) {       // ON <target>
        // first check that the requested target is a valid number and within range...
        //
        try {
          target = std::stof( tokens[3] );
         if ( target < this->heater_target_min || target > this->heater_target_max ) {
           message.str(""); message << "requested heater target " << target << " outside range {" 
                                    << this->heater_target_min << ":" << this->heater_target_max << "}";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "converting heater <target> " << tokens[3] << " to float";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "heater <target>: " << tokens[3] << " outside range of float";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        // ...if target is OK then push into the config, value vectors
        //
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "ENABLE"; heaterconfig.push_back( ss.str() );
        heatervalue.emplace_back("1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "TARGET"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );

      } else if ( tokens[2] == "RAMP" ) {     // RAMP x

        if ( tokens[3] == "ON" || tokens[3] == "OFF" ) {   // RAMP ON|OFF
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
          std::string state = ( tokens[3]=="ON" ? "1" : "0" );
          heatervalue.push_back( state );

        } else {                                             // RAMP <ramprate>
          try {
            ramprate = std::stoi( tokens[3] );
            if ( ramprate < 1 || ramprate > 32767 ) {
              message.str(""); message << "heater ramprate " << ramprate << " outside range {1:32767}";
              this->camera.log_error( function, message.str() );
              return ERROR;
            }

          } catch (std::invalid_argument &) {
            message.str(""); message << "converting RAMP <ramprate> " << tokens[3] << " to integer";
            this->camera.log_error( function, message.str() );
            return ERROR;

          } catch (std::out_of_range &) {
            message.str(""); message << "RAMP <ramprate>: " << tokens[3] << " outside range of integer";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }
          ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
          heatervalue.push_back( tokens[3] );
        }

      } else if ( tokens[2] == "ILIM" ) {     // ILIM x
        int il_value=0;
        try {
          il_value = std::stoi( tokens[3] );
          if ( il_value < 0 || il_value > 10000 ) {
            message.str(""); message << "heater ilim " << il_value << " outside range {0:10000}";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "converting ILIM <value> " << tokens[3] << " to integer";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "ILIM <value>: " << tokens[3] << " outside range of integer";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "IL"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );

      } else if ( tokens[2] == "INPUT" ) {    // INPUT A|B|C
        std::string sensorid;
        if ( tokens[3] == "A" ) {
            sensorid = "0";

        } else if ( tokens[3] == "B" ) {
            sensorid = "1";

        } else if ( tokens[3] == "C" ) {
            sensorid = "2";
          // input C supported only on HeaterX cards
          if ( this->modtype[ module-1 ] != 11 ) {
            message.str(""); message << "sensor C not supported on module " << module << ": HeaterX module required";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } else {
          message.str(""); message << "invalid sensor " << tokens.at(3) << ": expected <module> A|B INPUT A|B|C";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "SENSOR"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( sensorid );

      } else {
        message.str(""); message << "expected heater <" << module << "> ON | RAMP for 3rd argument but got " << tokens[2];
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

    } else if ( tokens.size() == 5 ) {        // RAMP ON <ramprate>
        // If there are five (5) tokens then they must be
        // <module> A|B RAMP ON <ramprate>
      if ( tokens[2] != "RAMP" && tokens[3] != "ON" ) {
        message.str(""); message << "expected RAMP ON <ramprate> but got"; for (int i=2; i<5; i++) message << " " << tokens[i];
        this->camera.log_error( function, message.str() );
        return ERROR;

      } else {  // got "<module> A|B RAMP ON" now check that the last (5th) token is a number
        try {
          ramprate = std::stoi( tokens[4] );
          if ( ramprate < 1 || ramprate > 32767 ) {
            message.str(""); message << "heater ramprate " << ramprate << " outside range {1:32767}";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "expected RAMP ON <ramprate> but unable to convert <ramprate>=" << tokens[4] << " to integer";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "expected RAMP ON <ramprate> but <ramprate>=" << tokens[4] << " outside range of integer";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMP";     heaterconfig.push_back( ss.str() );
        heatervalue.emplace_back("1" );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "RAMPRATE"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
      }

    } else if ( tokens.size() == 6 ) {
        // If there are six (6) tokens then they must be
        // <module> A|B PID <p> <i> <d>
      if ( tokens[2] != "PID" ) {
        message.str(""); message << "expected PID <p> <i> <d> but got"; for (int i=2; i<6; i++) message << " " << tokens[i];
        this->camera.log_error( function, message.str() );
        return ERROR;

      } else {  // got "<module> A|B PID <p> <i> <d>" now check that the last 3 tokens are numbers
        // Fractional PID requires a minimum backplane version
        //
        bool fractionalpid_ok = false;
        ret = compare_versions( this->backplaneversion, REV_FRACTIONALPID );
        if ( ret == -999 ) {
          message.str(""); message << "comparing backplane version " << this->backplaneversion << " to " << REV_FRACTIONALPID;
          this->camera.log_error( function, message.str() );
          return ERROR;

        } else if ( ret == -1 ) {
            fractionalpid_ok = false;

        } else fractionalpid_ok = true;

        try {
          // If using backplane where fractional PID was not allowed, check for decimal points
          //
          if ( (!fractionalpid_ok) &&
               ( ( tokens[3].find('.') != std::string::npos ) ||
                 ( tokens[4].find('.') != std::string::npos ) ||
                 ( tokens[5].find('.') != std::string::npos ) ) ) {
            fesetround(FE_TONEAREST);  // round to value nearest X. halfway cases rounded away from zero
            tokens[3] = std::to_string( std::lrint( std::stof( tokens[3] ) ) ); // replace token with rounded integer
            tokens[4] = std::to_string( std::lrint( std::stof( tokens[4] ) ) ); // replace token with rounded integer
            tokens[5] = std::to_string( std::lrint( std::stof( tokens[5] ) ) ); // replace token with rounded integer

            message.str(""); message << "NOTICE:fractional heater PID requires backplane version " << REV_FRACTIONALPID << " or newer";
            logwrite( function, message.str() );
            this->camera.async.enqueue( message.str() );
            message.str(""); message << "NOTICE:backplane version " << this->backplaneversion << " detected";
            logwrite( function, message.str() );
            this->camera.async.enqueue( message.str() );
            message.str(""); message << "NOTICE:PIDs converted to: " << tokens[3] << " " << tokens[4] << " " << tokens[5];
            this->camera.async.enqueue( message.str() );
            logwrite( function, message.str() );

          }
          pid_p = std::stof( tokens[3] );
          pid_i = std::stof( tokens[4] );
          pid_d = std::stof( tokens[5] );
          if ( pid_p < 0 || pid_p > 10000 || pid_i < 0 || pid_i > 10000 || pid_d < 0 || pid_d > 10000 ) {
            message.str(""); message << "one or more heater PID values outside range {0:10000}";
            this->camera.log_error( function, message.str() );
            return ERROR;
          }

        } catch (std::invalid_argument &) {
          message.str(""); message << "converting one or more heater PID values to numbers:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch (std::out_of_range &) {
          message.str(""); message << "heater PID exception: one or more values outside range:";
          for (int i=3; i<6; i++) message << " " << tokens[i];
          this->camera.log_error( function, message.str() );
          return ERROR;
        }
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "P"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[3] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "I"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[4] );
        ss.str(""); ss << "MOD" << module << "/HEATER" << heaterid << "D"; heaterconfig.push_back( ss.str() );
        heatervalue.push_back( tokens[5] );
      }

    } else {
        // Otherwise we have an invalid number of tokens
      message.str(""); message << "received " << tokens.size() << " arguments but expected 2, 3, 4, 5, or 6";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    long error;

    if ( ! readonly ) {

      // For writing, loop through the heaterconfig and heatervalue vectors.
      // They MUST be the same size! This should be impossible.
      //
      if ( heaterconfig.size() != heatervalue.size() ) {
        message.str("");
        message << "BUG DETECTED: heaterconfig (" << heaterconfig.size() 
                << ") - heatervalue (" << heatervalue.size() << ") vector size mismatch";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      // Write the configuration lines
      //
      bool changed = false;
      size_t error_count = 0;
      for ( size_t i=0; i < heaterconfig.size(); i++ ) {
        error = this->write_config_key( heaterconfig[i].c_str(), heatervalue[i].c_str(), changed );
        message.str("");
        if ( error != NO_ERROR ) {
          message << "writing configuration " << heaterconfig[i] << "=" << heatervalue[i];
          error_count++;  // this counter will be checked before the APPLYMOD command

        } else if ( !changed ) {
          message << "heater configuration: " << heaterconfig[i] << "=" << heatervalue[i] << " unchanged";

        } else {
          message << "updated heater configuration: " << heaterconfig[i] << "=" << heatervalue[i];
        }
        error == NO_ERROR ? logwrite( function, message.str() ) : this->camera.log_error( function, message.str() );
      }

      // send the APPLMODxx command which parses and applies the configuration for module xx
      // The APPLYMOD is sent even if an error occured writing the config key(s) because
      // it's possible that one of the config keys was written.
      //
      // If error_count is the same as the number of configuration lines, then they all failed
      // to write, in which case do not send APPLYMOD. But if even one config key was written
      // then the APPLYMOD command must be sent.
      //
      if ( error_count == heaterconfig.size() ) {
        return ERROR;
      }

      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);

      error = this->archon_cmd( applystr.str() );

      if ( error != NO_ERROR ) {
        logwrite( function, "ERROR: applying heater configuration" );
      }
    }

    // Now read the configuration line(s).
    // For multiple lines, concatenate all values into one space-delimited string.

    // loop through the vector of heaterconfig keys,
    // getting the value for each, and putting them into retss
    //
    std::string value;
    std::stringstream retss;
    for ( const auto &key : heaterconfig ) {

      error = this->get_configmap_value( key, value );

      if ( error != NO_ERROR ) {
        message.str(""); message << "reading heater configuration " << key;
        logwrite( function, message.str() );
        return error;

      } else {
        // If key ends with "ENABLE" or "RAMP"
        // then convert the values 0,1 to OFF,ON, respectively
        //
        if ( key.substr( key.length()-6 ) == "ENABLE" ||
             key.substr( key.length()-4 ) == "RAMP" ) {
          if ( value == "0" ) value = "OFF";
          else if ( value == "1" ) value = "ON";
          else {
            message.str(""); message << "bad value " << value << " from configuration. expected 0 or 1";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

        } else if ( key.substr( key.length()-6 ) == "SENSOR" ) {
            // or if key ends with "SENSOR" then map the value (0,1,2) to the name (A,B,C)
          if ( value == "0" ) value = "A";
          else if ( value == "1" ) value = "B";
          else if ( value == "2" ) value = "C";
          else {
            message.str(""); message << "bad value " << value << " from configuration. expected 0,1,2";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }
        }
        retss << value << " ";
        message.str(""); message << key << "=" << value;
        logwrite( function, message.str() );
      }
    }
    retstring = retss.str();  // return value to calling function, passed by reference

    return ( error );
  }
  /**************** Archon::Interface::heater *********************************/


  /**************** Archon::Interface::sensor *********************************/
  /**
   * @fn     sensor
   * @brief  set or get temperature sensor current
   * @param  args contains various allowable strings (see full descsription)
   * @return ERROR or NO_ERROR
   *
   * sensor <module> < A | B | C > [ current ]
   *        possible args: 2 (get) or 3 (set)
   *
   * sensor <module> < A | B | C > AVG [ N ]
   *        possible args: 3 (get) or 4 (set)
   *
   * Sets or gets the temperature sensor current <current> for the specified
   * sensor <A|B|C> on the specified module <module>. <module> refers to the
   * (integer) module number. <current> is specified in nano-amps. 
   * This is used only for RTDs
   *
   * When the AVG arg is used then set or get digital averaging
   *
   */
  long Interface::sensor(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::sensor";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string sensorid;                   //!< A | B | C
    std::stringstream sensorconfig;         //!< configuration line to read or write
    std::string sensorvalue;                //!< configuration line value
    int module;                             //!< integer module number
    bool readonly=true;                     //!< true is reading, not writing current
    long error;

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return ERROR;
    }

    // requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_SENSORCURRENT );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_SENSORCURRENT;

      } else {
        message << "requires backplane version " << REV_SENSORCURRENT << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    Tokenize( args, tokens, " " );

    // At minimum there must be two tokens, <module> <sensorid>
    //
    if ( tokens.size() < 2 ) {
      this->camera.log_error( function, "expected at least two arguments: <module> A|B" );
      return ERROR;
    }

    // Get the module and sensorid
    //
    try {
      module   = std::stoi( tokens.at(0) );
      sensorid = tokens.at(1);

      if ( sensorid != "A" && sensorid != "B" && sensorid != "C" ) {
        message.str(""); message << "invalid sensor " << sensorid << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

    } catch ( std::invalid_argument & ) {
      message.str(""); message << "parsing argument: " << args << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
      this->camera.log_error( function, message.str() );
      return ERROR;

    } catch ( std::out_of_range & ) {
      message.str(""); message << "argument outside range in " << args << ": expected <module#> <A|B|C> [ current | AVG [N] ]";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // check that the requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return ERROR;
      case 5:  // Heater
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "module " << module << " is not a heater board";
        this->camera.log_error( function, message.str() );
        return ERROR;
    }

    // input C supported only on HeaterX cards
    if ( sensorid == "C" && this->modtype[ module-1 ] != 11 ) {
      message.str(""); message << "sensor C not supported on module " << module << ": HeaterX module required";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // Now check the number of tokens to decide how to next proceed...

    // If there are 2 tokens then must be to read the current,
    //  <module> < A | B | C > 
    //
    if ( tokens.size() == 2 ) {
      readonly = true;
      sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "CURRENT";

    } else if ( tokens.size() == 3 ) {
        // If there are 3 tokens then it is either to write the current,
        //  <module> < A | B | C > current
        // or to read the average,
        //  <module> < A | B | C > AVG
      // if the 3rd arg is AVG then read the average (MODmSENSORxFILTER)
      //
      if ( tokens[2] == "AVG" ) {
        readonly = true;
        sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "FILTER";
      } else {
        // if it's not AVG then assume it's a current value and try to convert it to int
        //
        int current_val=-1;
        try {
          current_val = std::stoi( tokens[2] );

        } catch ( std::invalid_argument & ) {
          message.str(""); message << "parsing \"" << args << "\" : expected \"AVG\" or integer for arg 3";
          this->camera.log_error( function, message.str() );
          return ERROR;

        } catch ( std::out_of_range & ) {
          message.str(""); message << "parsing \"" << args << "\" : arg 3 outside integer range";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }

        // successfully converted value so check the range
        //
        if ( current_val < 0 || current_val > 1600000 ) {
          message.str(""); message << "requested current " << current_val << " outside range {0:1600000}";
          this->camera.log_error( function, message.str() );
          return ERROR;
        }

        // prepare sensorconfig string for writing
        //
        readonly = false;
        sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "CURRENT";
        sensorvalue = tokens[2];
      }
    } else if ( tokens.size() == 4 ) {        // set avg
        // If there are 4 tokens thenn it must be to write the average,
        // <module> < A | B | C > AVG N
      // check the contents of the 3rd arg
      //
      if ( tokens[2] != "AVG" ) {
        message.str(""); message << "invalid syntax \"" << tokens[2] << "\". expected <module> A|B|C AVG N";
      }

      // convert the avg value N to int and check for proper value
      int filter_val=-1;
      try {
        filter_val = std::stoi( tokens[3] );

      } catch ( std::invalid_argument & ) {
        message.str(""); message << "parsing \"" << args << "\" : expected integer for arg 4";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch ( std::out_of_range & ) {
        message.str(""); message << "parsing \"" << args << "\" : arg 4 outside integer range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      // prepare sensorconfig string for writing
      //
      readonly = false;
      sensorconfig << "MOD" << module << "/SENSOR" << sensorid << "FILTER";

      switch ( filter_val ) {
        case   1: sensorvalue = "0"; break;
        case   2: sensorvalue = "1"; break;
        case   4: sensorvalue = "2"; break;
        case   8: sensorvalue = "3"; break;
        case  16: sensorvalue = "4"; break;
        case  32: sensorvalue = "5"; break;
        case  64: sensorvalue = "6"; break;
        case 128: sensorvalue = "7"; break;
        case 256: sensorvalue = "8"; break;
          break;
        default:
          message.str(""); message << "requested average " << filter_val << " outside range {1,2,4,8,16,32,64,128,256}";
          this->camera.log_error( function, message.str() );
          return ( ERROR );
      }
    } else {
        // Otherwise an invalid number of tokens
      message.str(""); message << "received " << tokens.size() << " arguments but expected 2, 3, or 4";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    #ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] module=" << module << " sensorid=" << sensorid
                             << " readonly=" << ( readonly ? "true" : "false" )
                             << " sensorconfig=" << sensorconfig.str()
                             << " sensorvalue=" << sensorvalue;
    logwrite( function, message.str() );
    #endif

    std::string sensorkey = sensorconfig.str();

    if ( ! readonly ) {

      // should be impossible but just make sure these aren't empty because they're needed
      //
      if ( sensorconfig.rdbuf()->in_avail() == 0 || sensorvalue.empty() ) {
        this->camera.log_error( function, "BUG DETECTED: sensorconfig and sensorvalue cannot be empty" );
        return ( ERROR );
      }

      // Write the config line to update the sensor current
      //
      bool changed = false;
      error = this->write_config_key( sensorkey.c_str(), sensorvalue.c_str(), changed );

      // Now send the APPLYMODx command
      //
      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);

      if ( error == NO_ERROR ) error = this->archon_cmd( applystr.str() );

      message.str("");

      if ( error != NO_ERROR ) {
        message << "writing sensor configuration: " << sensorkey << "=" << sensorvalue;

      } else if ( !changed ) {
        message << "sensor configuration: " << sensorkey << "=" << sensorvalue << " unchanged";

      } else {
        message << "updated sensor configuration: " << sensorkey << "=" << sensorvalue;
      }
      logwrite( function, message.str() );
    }

    // now read back the configuration line
    //
    std::string value;
    error = this->get_configmap_value( sensorkey, value );

    if ( error != NO_ERROR ) {
      message.str(""); message << "reading sensor configuration " << sensorkey;
      logwrite( function, message.str() );
      return error;
    } 

    // return value to calling function, passed by reference
    //
    retstring = value;

    // if key ends with "FILTER" 
    // then convert the return value {0,1,2,...} to {1,2,4,8,...}
    //
    if ( sensorkey.substr( sensorkey.length()-6 ) == "FILTER" ) {

      // array of filter values that humans use
      //
      std::array< std::string, 9 > filter = { "1", "2", "4", "8", "16", "32", "64", "128", "256" };

      // the value in the configuration is an index into the above array
      //
      int findex=0;

      try {
        findex = std::stoi( value );

      } catch ( std::invalid_argument & ) {
        message.str(""); message << "bad value: " << value << " read back from configuration. expected integer";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch ( std::out_of_range & ) {
        message.str(""); message << "value: " << value << " read back from configuration outside integer range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      try {
        retstring = filter.at( findex );         // return value to calling function, passed by reference

      } catch ( std::out_of_range & ) {
        message.str(""); message << "filter index " << findex << " outside range: {0:" << filter.size()-1 << "}";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
    }

    message.str(""); message << sensorkey << "=" << value << " (" << retstring << ")";
    logwrite( function, message.str() );

    return ( error );
  }
  /**************** Archon::Interface::sensor *********************************/


  /**************** Archon::Interface::bias ***********************************/
  /**
   * @fn     bias
   * @brief  set a bias
   * @param  args contains: module, channel, bias
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::bias(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::bias";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::stringstream biasconfig;
    int module;
    int channel;
    float voltage;
    float vmin, vmax;
    bool readonly=true;

    // must have loaded firmware
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return ERROR;
    }

    Tokenize(args, tokens, " ");

    if (tokens.size() == 2) {
      readonly = true;

    } else if (tokens.size() == 3) {
      readonly = false;

    } else {
      message.str(""); message << "incorrect number of arguments: " << args << ": expected module channel [voltage]";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    std::transform( args.begin(), args.end(), args.begin(), ::toupper );  // make uppercase

    try {
      module  = std::stoi( tokens[0] );
      channel = std::stoi( tokens[1] );
      if (!readonly) voltage = std::stof( tokens[2] );

    } catch (std::invalid_argument &) {
      message.str(""); message << "parsing bias arguments: " << args << ": expected <module> <channel> [ voltage ]";
      this->camera.log_error( function, message.str() );
      return ERROR;

    } catch (std::out_of_range &) {
      message.str(""); message << "argument range: " << args << ": expected <module> <channel> [ voltage ]";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // Check that the module number is valid
    //
    if ( (module < 0) || (module > nmods) ) {
      message.str(""); message << "module " << module << ": outside range {0:" << nmods << "}";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "module " << module << " not installed";
        this->camera.log_error( function, message.str() );
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
        this->camera.log_error( function, message.str() );
        return ERROR;
        break;
    }

    // Check that the channel number is valid
    // and add it to the bias configuration string.
    //
    if ( (channel < 1) || (channel > 30) ) {
      message.str(""); message << "bias channel " << module << ": outside range {1:30}";
      this->camera.log_error( function, message.str() );
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
      this->camera.log_error( function, message.str() );
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
    error = this->write_config_key(key.c_str(), value.c_str(), changed);

    // Now send the APPLYMODx command
    //
    std::stringstream applystr;
    applystr << "APPLYMOD" 
             << std::setfill('0')
             << std::setw(2)
             << std::hex
             << (module-1);

    if (error == NO_ERROR) error = this->archon_cmd(applystr.str());

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
  /**************** Archon::Interface::bias ***********************************/


  /***** Archon::Interface::preampgain ****************************************/
  /**
   * @brief    set/get the ADC preamp gain
   * @param    args contains: gain {low|high}
   * @return   ERROR or NO_ERROR
   *
   */
  long Interface::preampgain(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::preampgain");
    std::stringstream message;
    int gain;
    long error = NO_ERROR;

    // must have loaded firmware
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return ERROR;
    }

    // make a list of installed AD modules
    //
    std::vector<int> admodules;
    for ( int i=0; i<nmods; i++ ) {
      if ( this->modtype[i] == 2 ) admodules.push_back(i);
    }
    if (admodules.size()==0) {
      this->camera.log_error( function, "no AD modules found" );
      return ERROR;
    }

    // no args is read gain
    //
    if ( args.empty() ) {
      std::vector<int> gains;

      // read gain of all AD modules into gains vecctor
      for ( const auto &mod : admodules ) {
        std::string key ( "MOD" + std::to_string(mod) + "/PREAMPGAIN" );
        error |= this->get_configmap_value(key, gain);
        gains.push_back(gain);
      }

      if ( error != NO_ERROR || gains.size() < 1 ) {
        this->camera.log_error( function, "reading PREAMPGAIN or no AD modules found" );
        return ERROR;
      }

      // are they all the same?
      if ( ! std::equal(gains.begin()+1, gains.end(), gains.begin()) ) {
        logwrite( function, "NOTICE: AD gains not the same" );
        message.str(""); message << "gains =";
        for ( const auto &gain : gains ) message << " " << (gain==0?"low":"high");
        retstring = message.str();
      }
      else {
        retstring = gains.front()==0 ? "low" : "high";
      }
      return NO_ERROR;
    }

    // if not read-only then get the arg is the requested gain level
    //
    if ( caseCompareString(args, "low") )  gain=0;
    else
    if ( caseCompareString(args, "high") ) gain=1;
    else {
      this->camera.log_error( function, "bad arguments: expected {low|high}" );
      return ERROR;
    }

    // write preamp gain configuration to each AD module
    //
    for ( const auto &mod : admodules ) {
      bool changed = false;
      std::string key ( "MOD" + std::to_string(mod) + "/PREAMPGAIN" );
      std::string val ( gain==0 ? "0" : "1" );

      // write the config line
      error |= this->write_config_key(key.c_str(), val.c_str(), changed);

      // send the APPLYMODx command
      std::stringstream applystr;
      applystr << "APPLYMOD"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (mod-1);
      if (error==NO_ERROR) error |= this->archon_cmd(applystr.str());

      if (error==NO_ERROR) {
        logwrite( function, "applied preamp gain="+args+" to AD "+std::to_string(mod) );
      }
      else {
        this->camera.log_error( function, "applying preamp gain="+args+" to AD "+std::to_string(mod) );
      }
    }

    retstring = gain==0?"LOW":"HIGH";
    return error;
  }
  /***** Archon::Interface::preampgain ****************************************/


  /**************** Archon::Interface::cds ************************************/
  /**
   * @fn     cds
   * @brief  set / get CDS parameters
   * @param  
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::cds(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::cds";
    std::stringstream message;
    std::vector<std::string> tokens;
    std::string key, value;
    bool changed;
    long error = ERROR;

    if ( args.empty() ) {
      this->camera.log_error( function, "no argument: expected cds <configkey> [ value ]" );
      return ERROR;
    }

    try {
      Tokenize(args, tokens, " ");

      // One token --
      // get the configuration key value
      //
      if ( tokens.size() == 1 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        error = this->get_configmap_value(key, retstring);                         // read
      }
      else

      // Two tokens --
      // set the configuration key to value, send APPLYCDS, then read back the config key
      //
      if ( tokens.size() == 2 ) {
        key   = tokens.at(0);
        std::transform( key.begin(), key.end(), key.begin(), ::toupper );          // make uppercase
        value = tokens.at(1);
        error = this->write_config_key( key.c_str(), value.c_str(), changed );     // set
        if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);                 // apply
        if (error == NO_ERROR) error = this->get_configmap_value(key, retstring);  // read back

      } else {
          // More than two tokens is an error
        this->camera.log_error( function, "Too many arguments. Expected cds <configkey> [ value ]" );
        return ERROR;
      }

    } catch(std::out_of_range &) {
      message << "parsing cds arguments: " << args << ": Expected cds <configkey> [ value ]";
      this->camera.log_error( function, message.str() );
      return ERROR;

    } catch(...) {
      message << "unknown exception parsing cds arguments: " << args << ": Expected cds <configkey> [ value ]";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::cds ************************************/


  /**************** Archon::Interface::inreg **********************************/
  /**
   * @fn     inreg
   * @brief  write to a VCPU INREGi
   * @param  args contains: module inreg value
   * @return ERROR or NO_ERROR
   *
   */
  long Interface::inreg( std::string args ) {
    std::string function = "Archon::Interface::inreg";
    std::stringstream message;
    std::vector<std::string> tokens;
    int module, reg, value;

    // must have loaded firmware // TODO implement a command to read the configuration 
    //                           //      memory from Archon, in order to remove this restriction.
    //
    if ( ! this->firmwareloaded ) {
      this->camera.log_error( function, "firmware not loaded" );
      return ERROR;
    }

    // VCPU requires a minimum backplane version
    //
    int ret = compare_versions( this->backplaneversion, REV_VCPU );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_VCPU;

      } else {
        message << "requires backplane version " << REV_VCPU << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    Tokenize(args, tokens, " ");

    // There must be three tokens, <module> <reg> <value>
    //
    if ( tokens.size() != 3 ) {
      this->camera.log_error( function, "expected three arguments: <module> <reg> <value>" );
      return ERROR;
    }

    // Now get the module and 
    // which will be common to everything.
    //
    try {
      module = std::stoi( tokens[0] );
      reg    = std::stoi( tokens[1] );
      value  = std::stoi( tokens[2] );
    }
    catch (std::invalid_argument &) {
      message.str(""); message << "unable to convert one of \"" << args << "\" to integer";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }
    catch (std::out_of_range &) {
      message.str(""); message << "one of \"" << args << "\" is outside integer range";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // check that requested module is valid
    //
    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message.str(""); message << "requested module " << module << " not installed";
        this->camera.log_error( function, message.str() );
        return ERROR;
      case 3:  // LVBias
      case 5:  // Heater
      case 7:  // HS
      case 9:  // LVXBias
      case 10: // LVDS
      case 11: // HeaterX
        break;
      default:
        message.str(""); message << "requested module " << module << " does not contain a VCPU";
        this->camera.log_error( function, message.str() );
        return ERROR;
    }

    // check that register number is valid
    //
    if ( reg < 0 || reg > 15 ) {
      message.str(""); message << "requested register " << reg << " outside range {0:15}";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    // check that value is within range
    //
    if ( value < 0 || value > 65535 ) {
      message.str(""); message << "requested value " << value << " outside range {0:65535}";
      this->camera.log_error( function, message.str() );
      return ERROR;
    }

    std::stringstream inreg_key;
    bool changed = false;
    inreg_key << "MOD" << module << "/VCPU_INREG" << reg;
    long error = this->write_config_key( inreg_key.str().c_str(), value, changed );
    if ( error != NO_ERROR ) {
      message.str(""); message << "configuration " << inreg_key.str() << "=" << value;
      logwrite( function, message.str() );
      return ERROR;

    } else {
      std::stringstream applystr;
      applystr << "APPLYDIO"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);
      error = this->archon_cmd( applystr.str() );
      return error;
    }
  }
  /**************** Archon::Interface::inreg **********************************/


  /**************** Archon::Interface::test ***********************************/
  /**
   * @fn     test
   * @brief  test routines
   * @param  string args contains test name and arguments
   * @param  reference to retstring for any return values
   * @return ERROR or NO_ERROR
   *
   * This is the place to put various debugging and system testing tools.
   * It is placed here, rather than in camera, to allow for controller-
   * specific tests. This means some common tests may need to be duplicated
   * for each controller.
   *
   * These are kept out of server.cpp so that I don't need a separate,
   * potentially conflicting command for each test I come up with, and also
   * reduces the chance of someone accidentally, inadvertently entering one
   * of these test commands.
   *
   * The server command is "test", the next parameter is the test name,
   * and any parameters needed for the particular test are extracted as
   * tokens from the args string passed in.
   *
   * The input args string is tokenized and tests are separated by a simple
   * series of if..else.. conditionals.
   *
   * current tests are:
   *   ampinfo   - print what is known about the amplifiers
   *   busy      - Override the archon_busy flag
   *   fitsname  - show what the fitsname will look like
   *   builddate - log the build date
   *   async     - queue an asynchronous status message
   *   modules   - Log all installed modules and their types
   *   parammap  - Log all parammap entries found in the ACF file
   *   configmap - Log all configmap entries found in the ACF file
   *   bw        - tests the exposure sequence bandwidth by running a sequence of exposures
   *   timer     - test Archon time against system time
   */
  long Interface::test(std::string args, std::string &retstring) {
    std::string function = "Archon::Interface::test";
    std::stringstream message;
    std::vector<std::string> tokens;
    long error;

    Tokenize(args, tokens, " ");

    if (tokens.empty()) {
      this->camera.log_error( function, "no test name provided" );
      return ERROR;
    }

    std::string testname;
      /* the first token is the test name */
    try {
        testname = tokens.at(0);

    } catch ( std::out_of_range & ) {
        this->camera.log_error( function, "testname token out of range" );
        return ERROR;
    }

    // ----------------------------------------------------
    // ampinfo
    // ----------------------------------------------------
    // print what is known about the amplifiers
    //
    if (testname == "ampinfo") {

      std::string mode = this->camera_info.current_observing_mode;
      int framemode = this->modemap[mode].geometry.framemode;

      message.str(""); message << "[ampinfo] observing mode=" << mode;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] FRAMEMODE=" << this->modemap[mode].geometry.framemode;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] LINECOUNT=" << this->modemap[mode].geometry.linecount << " PIXELCOUNT=" << this->modemap[mode].geometry.pixelcount;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] num_taps=" << this->modemap[mode].tapinfo.num_taps;
      logwrite( function, message.str() );
      message.str(""); message << "[ampinfo] hori_amps=" << this->modemap[mode].geometry.amps[0] << " vert_amps=" << this->modemap[mode].geometry.amps[1];
      logwrite( function, message.str() );

      message.str(""); message << "[ampinfo] gains =";
      for ( auto gn : this->gain ) {
        message << " " << gn;
      }
      logwrite( function, message.str() );

      int rows = this->modemap[mode].geometry.linecount;
      int cols = this->modemap[mode].geometry.pixelcount;

      int hamps = this->modemap[mode].geometry.amps[0];
      int vamps = this->modemap[mode].geometry.amps[1];

      int x0=-1, x1, y0, y1;

      for ( int y=0; y<vamps; y++ ) {
        for ( int x=0; x<hamps; x++ ) {
          if ( framemode == 2 ) {
            x0 = x; x1=x+1;
            y0 = y; y1=y+1;

          } else {
            x0++;   x1=x0+1;
            y0 = 0; y1=1;
          }
          message.str(""); message << "[ampinfo] x0=" << x0 << " x1=" << x1 << " y0=" << y0 << " y1=" << y1 
                                   << " | amp section (xrange, yrange) " << (x0*cols + 1) << ":" << (x1)*cols << ", " << (y0*rows + 1) << ":" << (y1)*rows;
          logwrite( function, message.str() );
        }
      }
      error = NO_ERROR;

    } else if (testname == "busy") {
        // ----------------------------------------------------
        // busy
        // ----------------------------------------------------
        // Override the archon_busy flag to test system responsiveness
        // when the Archon is busy.
      if ( tokens.size() == 1 ) {
        message.str(""); message << "archon_busy=" << this->archon_busy;
        logwrite( function, message.str() );
        error = NO_ERROR;

      } else if ( tokens.size() == 2 ) {
        try {
            this->archon_busy = tokens.at(1) == "yes";

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "tokens out of range" );
            error=ERROR;
        }
        message.str(""); message << "archon_busy=" << this->archon_busy;
        logwrite( function, message.str() );
        error = NO_ERROR;

      } else {
        message.str(""); message << "expected one argument, yes or no";
        this->camera.log_error( function, message.str() );
        error = ERROR;
      }
      retstring = this->archon_busy ? "true" : "false";

    } else if (testname == "fitsname") {
        // ----------------------------------------------------
        // fitsname
        // ----------------------------------------------------
        // Show what the fitsname will look like.
        // This is a "test" rather than a regular command so that it doesn't get mistaken
        // for returning a real, usable filename. When using fitsnaming=time, the filename
        // has to be generated at the moment the file is opened.
      std::string msg;
      this->camera.set_fitstime( get_timestamp() );                  // must set camera.fitstime first
      error = this->camera.get_fitsname(msg);                        // get the fitsname (by reference)
      retstring = msg;
      message.str(""); message << "NOTICE:" << msg;
      this->camera.async.enqueue( message.str() );                   // queue the fitsname
      logwrite(function, msg);                                       // log the fitsname
      if (error!=NO_ERROR) {
          this->camera.log_error( function, "couldn't validate fits filename" );
      }
        // end if (testname == fitsname)

    } else if ( testname == "builddate" ) {
        // ----------------------------------------------------
        // builddate
        // ----------------------------------------------------
        // log the build date
      std::string build(BUILD_DATE); build.append(" "); build.append(BUILD_TIME);
      retstring = build;
      error = NO_ERROR;
      logwrite( function, build );
        // end if ( testname == builddate )

    } else if (testname == "async") {
        // ----------------------------------------------------
        // async [message]
        // ----------------------------------------------------
        // queue an asynchronous message
        // The [message] param is optional. If not provided then "test" is queued.
      if (tokens.size() > 1) {
        if (tokens.size() > 2) {
          logwrite(function, "NOTICE:received multiple strings -- only the first will be queued");
        }
        try { message.str(""); message << "NOTICE:" << tokens.at(1);
              logwrite( function, message.str() );
              this->camera.async.enqueue( message.str() );

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "tokens out of range" );
            error=ERROR;
        }

      } else {                                // if no string passed then queue a simple test message
        logwrite(function, "NOTICE:test");
        this->camera.async.enqueue("NOTICE:test");
      }
      error = NO_ERROR;
        // end if (testname == async)

    } else if (testname == "modules") {
        // ----------------------------------------------------
        // modules
        // ----------------------------------------------------
        // Log all installed modules
      logwrite( function, "installed module types: " );
      message.str("");
      for ( const auto &mod : this->modtype ) {
        message << mod << " ";
      }
      logwrite( function, message.str() );
      retstring = message.str();
      error = NO_ERROR;

    } else if (testname == "parammap") {
        // ----------------------------------------------------
        // parammap
        // ----------------------------------------------------
        // Log all parammap entries found in the ACF file

      // loop through the modes
      //
      logwrite(function, "parammap entries by mode section:");
      for (auto & mode_it : this->modemap) {
        std::string mode = mode_it.first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto param_it = this->modemap[mode].parammap.begin(); param_it != this->modemap[mode].parammap.end(); ++param_it) {
          message.str(""); message << "MODE_" << mode << ": " << param_it->first << "=" << param_it->second.value;
          logwrite(function, message.str());
        }
      }

      logwrite(function, "ALL parammap entries in ACF:");
      int keycount=0;
      for (auto & param_it : this->parammap) {
        keycount++;
        message.str(""); message << param_it.first << "=" << param_it.second.value;
        logwrite(function, message.str());
        this->camera.async.enqueue( "NOTICE:"+message.str() );
      }
      message.str(""); message << "found " << keycount << " parammap entries";
      logwrite(function, message.str());
      error = NO_ERROR;
        // end if (testname == parammap)

    } else if (testname == "configmap") {
        // ----------------------------------------------------
        // configmap
        // ----------------------------------------------------
        // Log all configmap entries found in the ACF file
      error = NO_ERROR;
      logwrite(function, "configmap entries by mode section:");
      for (auto & mode_it : this->modemap) {
        std::string mode = mode_it.first;
        message.str(""); message << "found mode section " << mode;
        logwrite(function, message.str());
        for (auto config_it = this->modemap[mode].configmap.begin(); config_it != this->modemap[mode].configmap.end(); ++config_it) {
          message.str(""); message << "MODE_" << mode << ": " << config_it->first << "=" << config_it->second.value;
          logwrite(function, message.str());
        }
      }

      // if a second argument was passed then this is a config key
      // try to read it
      //
      if ( tokens.size() == 2 ) {
        try {
          std::string configkey = tokens.at(1);
          error = this->get_configmap_value(configkey, retstring);

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "configkey token out of range" );
            error=ERROR;
        }
      }

      // if a third argument was passed then set this configkey
      //
      if ( tokens.size() == 3 ) {
        try { std::string key = tokens.at(1);
              std::string value = tokens.at(2);
              bool configchanged;
              error = this->write_config_key( key.c_str(), value.c_str(), configchanged );
              if (error == NO_ERROR) error = this->archon_cmd(APPLYCDS);

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "key,value tokens out of range" );
            error=ERROR;
        }
      }

      int keycount=0;
      for (auto config_it = this->configmap.begin(); config_it != this->configmap.end(); ++config_it) {
        keycount++;
      }
      message.str(""); message << "found " << keycount << " configmap entries";
      logwrite(function, message.str());
        // end if (testname == configmap)

    } else if (testname == "bw") {
        // ----------------------------------------------------
        // bw <nseq>
        // ----------------------------------------------------
        // Bandwidth test
        // This tests the exposure sequence bandwidth by running a sequence
        // of exposures, including reading the frame buffer -- everything except
        // for the fits file writing.

      if ( ! this->modeselected ) {
        this->camera.log_error( function, "no mode selected" );
        return ERROR;
      }

      std::string nseqstr;
      int nseq;
      bool ro=false;  // read only
      bool rw=false;  // read and write

      if (tokens.size() > 1) {
        try {
            nseqstr = tokens.at(1);

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "nseqstr token out of range" );
            return ERROR;
        }

      } else {
        this->camera.log_error( function, "usage: test bw <nseq> [ rw | ro ]");
        return ERROR;
      }

      if (tokens.size() > 2) {
        try { if (tokens.at(2) == "rw") rw=true; else rw=false;
              if (tokens.at(2) == "ro") ro=true; else ro=false;

        } catch ( std::out_of_range & ) {
            this->camera.log_error( function, "rw tokens out of range" );
            error=ERROR;
        }
      }

      try {
        nseq = std::stoi( nseqstr );                                // test that nseqstr is an integer before trying to use it

      } catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert sequences: " << nseqstr << " to integer";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch (std::out_of_range &) {
        message.str(""); message << "sequences " << nseqstr << " outside integer range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      // exposeparam is set by the configuration file
      // check to make sure it was set, or else expose won't work
      //
      if (this->exposeparam.empty()) {
        message.str(""); message << "EXPOSE_PARAM not defined in configuration file " << this->config.filename;
        this->camera.log_error( function, message.str() );
        return ERROR;
      }
      error = this->get_frame_status();  // TODO is this needed here?

      if (error != NO_ERROR) {
        logwrite( function, "ERROR: unable to get frame status" );
        return ERROR;
      }
      this->lastframe = this->frame.bufframen[this->frame.index];     // save the last frame number acquired (wait_for_readout will need this)

      // initiate the exposure here
      //
      error = this->prep_parameter(this->exposeparam, nseqstr);
      if (error == NO_ERROR) error = this->load_parameter(this->exposeparam, nseqstr);

      // get system time and Archon's timer after exposure starts
      // start_timer is used to determine when the exposure has ended, in wait_for_exposure()
      //
      if (error == NO_ERROR) {
        this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
        error = this->get_timer(&this->start_timer);                  // Archon internal timer (one tick=10 nsec)
        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR: couldn't get start time" );
          return error;
        }
        this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
        // If read-write selected then need to do some FITS stuff
        //
        if ( rw ) {
          this->camera_info.extension = 0;                            // always initialize extension
          error = this->camera.get_fitsname( this->camera_info.fits_name ); // assemble the FITS filename if rw selected
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: couldn't validate fits filename" );
            return error;
          }
          this->add_filename_key();                                   // add filename to system keys database
          Common::FitsKeys::fits_key_t::iterator keyit;               // add keys from the ACF file 
          for (keyit  = this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.begin();
               keyit != this->modemap[this->camera_info.current_observing_mode].acfkeys.keydb.end();
               keyit++) {
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyword    = keyit->second.keyword;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keytype    = keyit->second.keytype;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keyvalue   = keyit->second.keyvalue;
            this->camera_info.userkeys.keydb[keyit->second.keyword].keycomment = keyit->second.keycomment;
          }

          this->camera_info.iscube = this->camera.datacube();

          // open the file now for datacubes
          //
          if ( this->camera.datacube() ) {
            error = this->fits_file.open_file(
                    this->camera.writekeys_when == "before", this->camera_info );
            if ( error != NO_ERROR ) {
              this->camera.log_error( function, "couldn't open fits file" );
              return error;
            }
          }
        }
      }

      if (error == NO_ERROR) logwrite(function, "exposure started");

      long frames_read = 0;

      // Wait for Archon frame buffer to be ready, then read the latest ready frame buffer to the host.
      // Loop over all expected frames.
      //
      while (nseq-- > 0) {

        // If read-write selected,
        // Open a new FITS file for each frame when not using datacubes
        //
        if ( rw && !this->camera.datacube() ) {
          this->camera_info.start_time = get_timestamp();               // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
          if ( this->get_timer(&this->start_timer) != NO_ERROR ) {      // Archon internal timer (one tick=10 nsec)
            logwrite( function, "ERROR: couldn't get start time" );
            return error;
          }
          this->camera.set_fitstime(this->camera_info.start_time);      // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
          error=this->camera.get_fitsname(this->camera_info.fits_name); // Assemble the FITS filename
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR: couldn't validate fits filename" );
            return error;
          }
          this->add_filename_key();                                     // add filename to system keys database

          error = this->fits_file.open_file(
                  this->camera.writekeys_when == "before", this->camera_info );
          if ( error != NO_ERROR ) {
            this->camera.log_error( function, "couldn't open fits file" );
            return error;
          }
        }

        if (this->camera_info.exposure_time.value() != 0) {         // wait for the exposure delay to complete (if there is one)
          error = this->wait_for_exposure();
          if (error==ERROR) {
            logwrite( function, "ERROR: exposure delay error" );
            break;

          } else {
            logwrite(function, "exposure delay complete");
          }
        }

        if (error==NO_ERROR) error = this->wait_for_readout();                     // wait for the readout into frame buffer,
        if (error==NO_ERROR && ro) error = this->read_frame(Camera::FRAME_IMAGE);  // read image frame directly with no write
        if (error==NO_ERROR && rw) error = this->read_frame();                     // read image frame directly with no write
        if (error==NO_ERROR && rw && !this->camera.datacube()) {
          this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
          this->camera.increment_imnum();                                          // increment image_num when fitsnaming == "number"
        }
        if (error==NO_ERROR) frames_read++;
      }
      retstring = std::to_string( frames_read );

      // for cubes, close the FITS file now that they've all been written
      // (or any time there is an error)
      //
      if ( rw && ( this->camera.datacube() || (error==ERROR) ) ) {
        this->fits_file.close_file(this->camera.writekeys_when == "after", this->camera_info );
        this->camera.increment_imnum();                                            // increment image_num when fitsnaming == "number"
      }

      logwrite( function, (error==ERROR ? "ERROR" : "complete") );

      message.str(""); message << "frames read = " << frames_read;
      logwrite(function, message.str());
        // end if (testname==bw)

    } else if (testname == "timer") {
        // ----------------------------------------------------
        // timer
        // ----------------------------------------------------
        // test Archon time against system time

      int nseq;
      int sleepus;
      double systime1, systime2;
      unsigned long int archontime1, archontime2;
      std::vector<int> deltatime;
      int delta_archon, delta_system;

      if (tokens.size() < 3) {
        this->camera.log_error( function, "expected test timer <cycles> <sleepus>" );
        return ERROR;
      }

      try {
        nseq    = std::stoi( tokens.at(1) );
        sleepus = std::stoi( tokens.at(2) );

      } catch (std::invalid_argument &) {
        message.str(""); message << "unable to convert one or more args to an integer";
        this->camera.log_error( function, message.str() );
        return ERROR;

      } catch (std::out_of_range &) {
        message.str(""); message << "nseq, sleepus tokens outside range";
        this->camera.log_error( function, message.str() );
        return ERROR;
      }

      error = NO_ERROR;

      // turn off background polling while doing the timing test
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLOFF);

      // send the Archon TIMER command here, nseq times, with sleepus delay between each
      //
      int nseqsave = nseq;
      while ( error==NO_ERROR && nseq-- > 0 ) {
        // get Archon timer [ns] and system time [s], twice
        //
        error = get_timer(&archontime1);
        systime1 = get_clock_time();
        error = get_timer(&archontime2);
        systime2 = get_clock_time();

        // difference between two calls, converted to µsec
        //
        delta_archon = (int)((archontime2 - archontime1) / 100.);  // archon time was in 10 nsec
        delta_system = (int)((systime2 - systime1) * 1000000.);    // system time was in sec

        // enque each line to the async message port
        //
        message.str(""); message << "TEST_TIMER: " << nseqsave-nseq << ", " << delta_archon << ", " << delta_system;
        this->camera.async.enqueue( message.str() );

        // save the difference between 
        //
        deltatime.push_back( abs( delta_archon - delta_system ) );

        usleep( sleepus );
      }

      // background polling back on
      //
      if (error == NO_ERROR) error = this->archon_cmd(POLLON);

      // calculate the average and standard deviation of the difference
      // between system and archon
      //
      double sum = std::accumulate(std::begin(deltatime), std::end(deltatime), 0.0);
      double m =  sum / deltatime.size();

      double accum = 0.0;
      std::for_each (std::begin(deltatime), std::end(deltatime), [&](const double d) {
          accum += (d - m) * (d - m);
      });

      double stdev = sqrt(accum / (deltatime.size()-1));

      message.str(""); message << "average delta=" << m << " stddev=" << stdev;
      logwrite(function, message.str());

      retstring = "delta=" + std::to_string( m ) + " stddev=" + std::to_string( stdev );
        // end if (testname==timer)

    } else if (testname == "rconfigmap") {
        // ----------------------------------------------------
        // rconfigmap <filter>
        // reports the configmap (what should have been written)
        // ----------------------------------------------------
      std::string filter;
      if ( tokens.size() > 1 ) {
        if ( tokens[1] == "line" ) filter="LINE";
        else
        if ( tokens[1] == "mod" ) filter="MOD";
        else
        if ( tokens[1] == "param" ) filter="PARAMETER";
        else
        if ( tokens[1] == "state" ) filter="STATE";
        else
        if ( tokens[1] == "vcpu" ) filter="VCPU";
      }
      for ( const auto &[k,v] : this->configmap ) {
        if ( k.find(filter)!=std::string::npos ) {
          message.str(""); message << "RCONFIG"
                                   << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
                                   << v.line << ": " << k << "=" << v.value;
          logwrite( function, message.str() );
        }
      }
      error = NO_ERROR;
    } else if (testname == "rconfig") {
        // ----------------------------------------------------
        // rconfig <filter>
        // reads config directly from Archon
        // ----------------------------------------------------
      std::string filter;
      if ( tokens.size() > 1 ) {
        if ( tokens[1] == "line" ) filter="LINE";
        else
        if ( tokens[1] == "mod" ) filter="MOD";
        else
        if ( tokens[1] == "param" ) filter="PARAMETER";
        else
        if ( tokens[1] == "state" ) filter="STATE";
        else
        if ( tokens[1] == "vcpu" ) filter="VCPU";
      }
      for ( int line=0; line < this->configlines; line++ ) {
        // form the RCONFIG command to send to Archon (without logging each command)
        //
        std::stringstream cmd;
        cmd.str(""); cmd << QUIET
                         << "RCONFIG"
                         << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << line;
        std::string reply;
        error = this->archon_cmd(cmd.str(), reply);               // send RCONFIG command here
        if ( reply.find(filter)!=std::string::npos ) {
          message.str(""); message << "RCONFIG"
                                   << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << line
                                   << ": " << strip_newline(reply);
          logwrite( function, message.str() );
        }
      }
      error = NO_ERROR;
    } else if (testname == "logwconfig") {
        // ----------------------------------------------------
        // logwconfig [ <state> ]
        // set/get state of logwconfig, to optionally log WCONFIG commands
        // ----------------------------------------------------
      if ( tokens.size() > 1 ) {
        if ( tokens[1] == "true" ) this->logwconfig=true;
        else
        if ( tokens[1] == "false" ) this->logwconfig=false;
      }
      retstring = ( this->logwconfig ? "true" : "false" );
      message.str(""); message << "logwconfig " << retstring;
      logwrite( function, message.str() );
      error = NO_ERROR;
    } else {
        // ----------------------------------------------------
        // invalid test name
        // ----------------------------------------------------
      message.str(""); message << "unknown test: " << testname;
      this->camera.log_error( function, message.str() );
      error = ERROR;
    }

    return error;
  }
  /**************** Archon::Interface::test ***********************************/

}
