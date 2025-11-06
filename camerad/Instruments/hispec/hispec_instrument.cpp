/**
 * @file    Instruments/hispec/hispec_instrument.cpp
 * @brief   implementation for HISPEC-specific properties
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "hispec_instrument.h"
#include "hispec_exposure_modes.h"

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
    if ( cmd == "hispec_this" ) {
      return this->hispec_this(args, retstring);
    }
    else
    if ( cmd == "hispec_that" ) {
      return this->hispec_that(args, retstring);
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


  /***** Camera::Hispec::get_exposure_modes ***********************************/
  /**
   * @brief      return a vector of strings of recognized exposure modes
   * @details    This adds Hispec exposure modes to the base exposure modes.
   * @return     vector<string>
   *
   */
  std::vector<std::string> Hispec::get_exposure_modes() {
    // base exposure modes
    auto modes = this->ArchonInterface::get_exposure_modes();

    // add hispec exposure modes
    for (const auto &mode : Camera::HispecExposureMode::ALLMODES) { modes.push_back(mode); }

    return modes;
  }
  /***** Camera::Hispec::get_exposure_modes ***********************************/


  /***** Camera::Hispec::set_exposure_mode ************************************/
  /**
   * @brief      actually sets the exposure mode
   * @details    This creates the appropriate exposure mode object for the
   *             requested exposure mode, providing access to that mode's functions.
   *             This is hispec-specific but gets called by ArchonInterface because
   *             it overrides. If the requested mode is not a hispec mode then
   *             this will call the set_exposure_mode in the base class.
   * @return     ERROR|NO_ERROR
   *
   */
  long Hispec::set_exposure_mode(const std::string &modein) {

    if (modein==HispecExposureMode::FASTREADOUT) {
      this->exposuremode = std::make_unique<ExposureModeFastReadout>(this);
    }
    else
    if (modein==HispecExposureMode::SLOWREADOUT) {
      this->exposuremode = std::make_unique<ExposureModeSlowReadout>(this);
    }
    else {
      return this->ArchonInterface::set_exposure_mode(modein);
    }

    return NO_ERROR;
  }
  /***** Camera::Hispec::set_exposure_mode ************************************/


  /***** Camera::Hispec::hispec_this ******************************************/
  /**
   * @brief      placeholder for example HISPEC-only function
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long Hispec::hispec_this(const std::string &args, std::string &retstring) {
    const std::string function("Camera::Hispec::hispec_this");
    logwrite(function, "hello, world: "+args);
    retstring="hello";
    return NO_ERROR;
  }
  long Hispec::hispec_that(const std::string &args, std::string &retstring) {
    const std::string function("Camera::Hispec::hispec_that");
    logwrite(function, "goodbye, world: "+args);
    retstring="goodbye";
    return NO_ERROR;
  }
  /***** Camera::Hispec::hispec_that ******************************************/
}
