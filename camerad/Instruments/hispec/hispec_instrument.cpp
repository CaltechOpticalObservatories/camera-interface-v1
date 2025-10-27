/**
 * @file    Instruments/hispec/hispec_instrument.cpp
 * @brief   implementation for HISPEC-specific properties
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "hispec_instrument.h"

namespace Camera {

  /***** Camera::Hispec::instrument_cmd ***************************************/
  /**
   * @brief      dispatcher for HISPEC-specific instrument commands
   * @details    This allows dispatching instrument specific commands by receiving
   *             the command and args and calling the appropriate instrument
   *             specific function.
   * @param[in]  cmd        command
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long Hispec::instrument_cmd(const std::string &cmd,
                              const std::string &args,
			      std::string &retstring) {
    if ( cmd == "hispec_expose" ) {
      return this->hispec_expose(args, retstring);
    }
    else {
      retstring = "unrecognized command";
      return ERROR;
    }
  }
  /***** Camera::Hispec::instrument_cmd ***************************************/


  /***** Camera::Hispec::configure_instrument *********************************/
  /**
   * @brief      extract+apply instrument-specific parameters from config file
   * @throws     std::runtime_error
   *
   */
  void Hispec::configure_instrument() {
    const std::string function("Camera::Hispec::configure_instrument");
    logwrite(function, "");
  }
  /***** Camera::Hispec::configure_instrument *********************************/


  /***** Camera::Hispec::hispec_expose ****************************************/
  /**
   * @brief      placeholder for example HISPEC-only function
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long Hispec::hispec_expose(const std::string &args, std::string &retstring) {
    const std::string function("Camera::Hispec::hispec_expose");
    logwrite(function, "hello, world: "+args);
    retstring="OK";
    return NO_ERROR;
  }
  /***** Camera::Hispec::hispec_expose ****************************************/
}
