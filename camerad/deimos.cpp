/**
 * @file    deimos.cpp
 * @brief   instrument specific code for DEIMOS
 *
 */

#include "archon.h"
#include "common.h"
#include "utilities.h"
#include "logentry.h"
#include "camerad_commands.h"

namespace Archon {

  /***** Archon::Interface::region_of_interest ********************************/
  /**
   * @brief      not supported
   * @param[in]  args
   * @param[out] restring
   * @return     ERROR
   *
   */
  long Interface::region_of_interest(std::string args, std::string &retstring) {
    camera.log_error("Archon::Interface::region_of_interest", "ROI not supported");
    retstring="not_supported";
    return ERROR;
  }
  /***** Archon::Interface::region_of_interest ********************************/


  /***** Archon::Interface::power *********************************************/
  /**
   * @brief      wrapper for Archon::Interface::do_power()
   * @param[in]  args      requested power state
   * @param[out] restring  return string holds power state
   * @return     ERROR or NO_ERROR
   *
   */
  long Interface::power(std::string args, std::string &retstring) {
      const std::string function("Archon::Interface::power");
      std::stringstream message;

      camera.log_error(function, "not yet implemented");
      return ERROR;

      // use Archon::Interface::do_power() to set/get the power
      //
      return( do_power(args, retstring) );
  }
  /***** Archon::Interface::power *********************************************/


  /***** Archon::Interface::fcs_exptime ***************************************/
  /**
   * @brief      set/get FCS exposure time
   * @details    This function is used by the server.
   * @param[in]  args      requested exposure time, or empty or help
   * @param[out] restring  return string holds exposure time
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::fcs_exptime(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::fcs_exptime");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_FCS_EXPTIME;
      retstring.append(" [ <exptime> ]\n");
      retstring.append("  set/get FCS exposure time, in units of floating point seconds\n");
      return HELP;
    }

    // If an arg was supplied then use it to try to set the exptime
    if (!args.empty()) {
      try {
        this->set_fcs_exptime(std::stod(args));
      }
      catch (const std::exception &e) {
        retstring=std::string(e.what());
        camera.log_error(function, retstring);
        return ERROR;
      }
    }

    // read the exptime from the class
    retstring = std::to_string(fcs_exposure_time.get());

    return NO_ERROR;
  }
  /***** Archon::Interface::fcs_exptime ***************************************/


  /***** Archon::Interface::set_fcs_exptime ***********************************/
  /**
   * @brief      set FCS exposure time
   * @details    This function is used internally, and is what actually sets
   *             the exposure time.
   * @param[in]  exptime  double-precision exposure time in seconds
   * @throws     std::exception
   *
   */
  void Interface::set_fcs_exptime(double exptime) {
    const std::string function("Archon::Interface::set_fcs_exptime");
    // Archon connection required to set/get exptime
    if (!archon.isconnected()) {
      throw std::runtime_error("connection not open to controller");
    }
    // exposure time parameters must be defined
    if (fcs_exptime_sec_param.empty() || fcs_exptime_msec_param.empty()) {
      throw std::runtime_error("FCS expsure time parameters not defined");
    }
    try {
      // split the requested exposure time into seconds and milliseconds
      auto [sec, msec] = fcs_exposure_time.split(exptime);

      // set the sec and msec parameters on the controller
      // and store the exptime in the class on success
      if ( (set_parameter(fcs_exptime_sec_param, sec)   == NO_ERROR) &&
           (set_parameter(fcs_exptime_msec_param, msec) == NO_ERROR) ) {
        fcs_info.exposure_time.set(exptime);
        fcs_exposure_time.set(exptime);
      }
      else throw std::runtime_error("could not set FCS exposure time parameters");
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Archon::Interface::set_fcs_exptime ***********************************/


  /***** Archon::Interface::fcs_expose ****************************************/
  /**
   * @brief      start FCS exposure
   * @param[in]  args      args
   * @param[out] restring  return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::fcs_expose(std::string args, std::string &retstring) {
    return NO_ERROR;
  }
  /***** Archon::Interface::fcs_expose ****************************************/


  /***** Archon::Interface::sci_exptime ***************************************/
  /**
   * @brief      set/get FCS exposure time
   * @param[in]  args      requested exposure time, or empty or help
   * @param[out] restring  return string holds exposure time
   * @return     ERROR | NO_ERROR | HELP
   * @throws     std::exception
   *
   */
  long Interface::sci_exptime(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sci_exptime");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_EXPTIME;
      retstring.append(" [ <exptime> ]\n");
      retstring.append("  set/get SCI exposure time, in units of floating point seconds\n");
      return HELP;
    }

    // If an arg was supplied then use it to try to set the exptime
    if (!args.empty()) {
      try {
        this->set_sci_exptime(std::stod(args));
      }
      catch (const std::exception &e) {
        retstring=std::string(e.what());
        camera.log_error(function, retstring);
        return ERROR;
      }
    }

    // read the exptime from the class
    retstring = std::to_string(sci_exposure_time.get());

    return NO_ERROR;
  }
  /***** Archon::Interface::sci_exptime ***************************************/


  /***** Archon::Interface::set_sci_exptime ***********************************/
  /**
   * @brief      set SCI exposure time  // TODO THIS COULD BE COMBINED WITH set_fcs_exptime <------------
   * @details    This function is used internally, and is what actually sets
   *             the exposure time.
   * @param[in]  exptime  double-precision exposure time in seconds
   * @throws     std::exception
   *
   */
  void Interface::set_sci_exptime(double exptime) {
    const std::string function("Archon::Interface::set_sci_exptime");
    // Archon connection required to set/get exptime
    if (!archon.isconnected()) {
      throw std::runtime_error("connection not open to controller");
    }
    // exposure time parameters must be defined
    if (sci_exptime_sec_param.empty() || sci_exptime_msec_param.empty()) {
      throw std::runtime_error("SCI expsure time parameters not defined");
    }
    try {
      // split the requested exposure time into seconds and milliseconds
      auto [sec, msec] = sci_exposure_time.split(exptime);

      // set the sec and msec parameters on the controller
      // and store the exptime in the class on success
      if ( (set_parameter(sci_exptime_sec_param, sec)   == NO_ERROR) &&
           (set_parameter(sci_exptime_msec_param, msec) == NO_ERROR) ) {
        sci_info.exposure_time.set(exptime);
        sci_exposure_time.set(exptime);
      }
      else throw std::runtime_error("could not set SCI exposure time parameters");
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Archon::Interface::set_sci_exptime ***********************************/


  /***** Archon::Interface::start_sci_expose **********************************/
  /**
   * @brief      start science exposure
   * @details    Sets a parameter to trigger a branch in the ACF which stops
   *             the SCI detector flush/idle so that integration on the SCI
   *             detector effectively begins.
   * @param[in]  args      optional number of exposures
   * @param[out] restring  return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::start_sci_expose(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::start_sci_expose");
    long error=NO_ERROR;

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_START;
      retstring.append("\n");
      retstring.append("  stops SCI detector idle, which begins integration\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    // science exposure trigger parameter must be defined
    if (sci_start_param.empty()) {
      camera.log_error(function, "science exposure trigger parameter not defined");
      retstring="missing_config";
      return ERROR;
    }

    this->camera_info = this->sci_info;

    error = set_camera_mode("SCIENCE");

    if (error!=NO_ERROR) {
      logwrite(function, "error");
      return error;
    }

    // start SCI exposure by setting science exposure parameter = 1
    if ( (set_parameter(sci_start_param, 1) != NO_ERROR) ) {
      camera.log_error(function, "could not set Archon parameter");
      return ERROR;
    }

    logwrite(function, "sci exposure started");

    error = this->wait_for_exposure();

    logwrite(function, "done");

    return error;
  }
  /***** Archon::Interface::start_sci_expose **********************************/


  /***** Archon::Interface::readout_sci ***************************************/
  /**
   * @brief      readout science detector
   * @details    Configures taps and CDS parameters for the science detector,
   *             then sets a parameter to trigger a branch in the ACF which
   *             starts clocking the SCI detector into an Archon buffer,
   *             effectively ending the science integration, then transmits
   *             that buffer to the host computer.
   * @param[in]  args
   * @param[out] restring
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::readout_sci(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::readout_sci");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_READOUT;
      retstring.append("\n");
      retstring.append("  clocks SCI detector into Archon buffer then transmits buffer to host\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    camera.log_error(function, "not yet implemented");
    retstring="not_implemented";
    return ERROR;
  }
  /***** Archon::Interface::readout_sci ***************************************/
}
