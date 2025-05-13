/**
 * @file    archon_controller.cpp
 * @brief   implementation for Archon Interface Controller
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_controller.h"
#include "archon_interface.h"

namespace Camera {

  Controller::Controller() : 
    interface(nullptr), framebuf(nullptr), framebuf_bytes(0), is_connected(false), is_busy(false), is_firmwareloaded(false)
  {
  }

  Controller::~Controller() {
    delete[] framebuf;
  }

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


  /***** Camera::Controller::write_config_key *********************************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @param[in]  key
   * @param[in]  newvalue
   * @param[out] changed
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
   * @details    This function is overloaded.
   *             Use this version for integer values.
   * @param[in]  key       pointer to key
   * @param[in]  newvalue  integer value
   * @param[out] changed   true if changed
   * @return     ERROR | NO_ERROR
   *
   */
  long Controller::write_config_key( const char* key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /***** Camera::Controller::write_config_key *********************************/

}
