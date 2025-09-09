/**
 * @file    archon_controller.cpp
 * @brief   implementation for Archon Interface Controller
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_controller.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::Controller::Controller ***************************************/
  /**
   * @brief      Controller constructor
   */
  Controller::Controller() : 
    interface(nullptr),
    framebuf(nullptr),
    framebuf_bytes(0),
    is_connected(false),
    is_firmwareloaded(false)
  {
  }
  /***** Camera::Controller::Controller ***************************************/


  /***** Camera::Controller::~Controller **************************************/
  /**
   * @brief      Controller destructor
   */
  Controller::~Controller() {
    delete[] framebuf;
  }
  /***** Camera::Controller::~Controller **************************************/


  /***** Camera::Controller::set_interface ************************************/
  /**
   * @brief      initialize pointer to parent interface
   * @details    This allows Controller members to call back into ArchonInterface
   * @param[in]  _interface  pointer to parent Camera::ArchonInterface object
   *
   */
  void Controller::set_interface(ArchonInterface* _interface) {
    this->interface = _interface;
  }
  /***** Camera::Controller::set_interface ************************************/


  /***** Camera::Controller::allocate_framebuf ********************************/
  /**
   * @brief      allocate memory for frame buffer
   * @param[in]  reqsz  size in bytes of Archon frame buffer
   * @return     ERROR | NO_ERROR
   *
   */
  long Controller::allocate_framebuf(uint32_t reqsz) {
    const std::string function("Camera::Controller::allocate_framebuf");
    try {
      if (reqsz>0) {
        delete[] this->framebuf;
        this->framebuf = new char[reqsz];
        this->framebuf_bytes = reqsz;
      }
      else throw std::runtime_error("invalid requested size");
    }
    catch(const std::exception &e) {
      logwrite(function, "ERROR allocating framebuf: "+std::string(e.what()));
      this->framebuf_bytes = 0;
    }
    return (this->framebuf_bytes>0 ? NO_ERROR : ERROR);
  }
  /***** Camera::Controller::allocate_framebuf ********************************/


  long Controller::read_frame(frametype_t type) {
    const std::string function("Camera::Controller::read_frame");
    logwrite(function, "reading frame from Archon");
    for (int i=0; i<100; i++) framebuf[i]=i+1;
    return NO_ERROR;
  }


  /***** Camera::Controller::write_config_key *********************************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @details    This function is overloaded. Use this version for char values.
   * @param[in]  key       char pointer to key
   * @param[in]  newvalue  char pointer to value
   * @param[out] changed   set true if changed
   * @return     ERROR | NO_ERROR
   *
   */
  long Controller::write_config_key( const char* key, const char* newvalue, bool &changed ) {
    const std::string function("Camera::Controller::write_config_key");
    std::stringstream message, sscmd;
    long error=NO_ERROR;

    if ( key==nullptr || newvalue==nullptr ) {
      error = ERROR;
      logwrite( function, "key|value cannot have NULL" );

    } else if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "requested key " << key << " not found in configmap";
      logwrite( function, message.str() );

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
      error = this->interface->send_cmd((char*)sscmd.str().c_str());  // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;                        // save newvalue in the STL map
        changed = true;

      } else {
        message.str(""); message << "ERROR: config key=value: " << key << "=" << newvalue << " not written";
        logwrite( function, message.str() );
      }
    }
    return error;
  }
  /***** Camera::Controller::write_config_key *********************************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @details    This function is overloaded. Use this version for integer values.
   * @param[in]  key       pointer to key
   * @param[in]  newvalue  integer value
   * @param[out] changed   set true if changed
   * @return     ERROR | NO_ERROR
   *
   */
  long Controller::write_config_key( const char* key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /***** Camera::Controller::write_config_key *********************************/


  /***** Camera::Controller::parse_system_configuration ***********************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @brief
   * @param[in]  message
   * @return     ERROR | NO_ERROR
   *
   */
  long Controller::parse_system_configuration(const std::string &message) {
    const std::string function("Camera::Controller::get_system_configuration");
    if ( !is_connected ) {
      logwrite(function, "ERROR Archon connection not open");
      return ERROR;
    }

    std::vector<std::string> lines, tokens;
    Tokenize( message, lines, " " );                  // then each line in a separate token "lines"

    for ( const auto &line : lines ) {
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
        }
        catch (const std::exception &e) {
          std::stringstream err;
          err << "ERROR parsing module or type from " << tokens[0] << "=" << tokens[1] << ": " << e.what();
          logwrite( function, err.str() );
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
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module

        }
        catch (const std::out_of_range &e) {
          std::stringstream err;
          err << "ERROR module " << module << " out of range {1:" << NMODS << "}";
          logwrite( function, err.str() );
        }
      }
      else {                                          // else should never happen
        std::stringstream err;
        err << "ERROR module " << module << " outside range {1:" << NMODS << "}";
        logwrite( function, err.str() );
        return ERROR;
      }

      // Use the module type to resize the gain and offset vectors,
      // ADC module is type 2
      // ADM module is type 17
      //
      if (type==2 || type==17) {
        const auto size = (type==2) ? MAXADCCHANS : MAXADMCHANS;
        this->gain.resize( size );
        this->offset.resize( size );

        // Check that the AD modules are installed in the correct slot
        if ( module < 5 || module > 8 ) {
          std::stringstream err;
          err << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
          logwrite( function, err.str() );
          return ERROR;
        }
      }

    } // end for ( auto line : lines )

    return NO_ERROR;
  }
  /***** Camera::Controller::parse_system_configuration ***********************/

}
