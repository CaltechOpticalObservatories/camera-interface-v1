/**
 * @file    Instruments/hispec_tracking_camera/hispec_tracking_camera_instrument.cpp
 * @brief   implementation for HISPEC Tracking Camera-specific properties
 * @author  Michael Langmayr <langmayr@astro.caltech.edu>
 *
 */

#include "hispec_tracking_camera_instrument.h"
#include "hispec_tracking_camera_exposure_modes.h"

namespace Camera {

  /***** Camera::HispecTrackingCamera::instrument_cmd *************************/
  /**
   * @brief      dispatcher for HISPEC Tracking Camera-specific instrument commands
   * @details    This allows dispatching instrument specific commands by receiving
   *             the command and args and calling the appropriate instrument
   *             specific function.
   * @param[in]  cmd        command
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long HispecTrackingCamera::instrument_cmd(const std::string &cmd,
                              const std::string &args,
			      std::string &retstring) {
    if ( cmd == "readout" ) {
      return this->readout(args, retstring);
    }
    else
    if ( cmd == "expose" ) {
      return this->expose(args, retstring);
    }
    else {
      retstring = "unrecognized command";
      return ERROR;
    }
  }
  /***** Camera::HispecTrackingCamera::instrument_cmd *************************/


  /***** Camera::HispecTrackingCamera::configure_instrument *******************/
  /**
   * @brief      extract+apply instrument-specific parameters from config file
   * @throws     std::runtime_error
   *
   */
  void HispecTrackingCamera::configure_instrument() {
    const std::string function("Camera::HispecTrackingCamera::configure_instrument");
    logwrite(function, "");
  }
  /***** Camera::HispecTrackingCamera::configure_instrument *******************/


  /***** Camera::HispecTrackingCamera::get_exposure_modes *********************/
  /**
   * @brief      return a vector of strings of recognized exposure modes
   * @details    This adds HispecTrackingCamera exposure modes to the base exposure modes.
   * @return     vector<string>
   *
   */
  std::vector<std::string> HispecTrackingCamera::get_exposure_modes() {
    // base exposure modes
    auto modes = this->ArchonInterface::get_exposure_modes();

    // add hispec tracking camera exposure modes
    for (const auto &mode : Camera::HispecTrackingCameraExposureMode::ALLMODES) { modes.push_back(mode); }

    return modes;
  }
  /***** Camera::HispecTrackingCamera::get_exposure_modes *********************/


  /***** Camera::HispecTrackingCamera::set_exposure_mode **********************/
  /**
   * @brief      actually sets the exposure mode
   * @details    This creates the appropriate exposure mode object for the
   *             requested exposure mode, providing access to that mode's functions.
   *             This is hispec tracking camera-specific but gets called by
   *             ArchonInterface because it overrides. If the requested mode is
   *             not a hispec tracking camera mode then this will call the
   *             set_exposure_mode in the base class.
   * @return     ERROR|NO_ERROR
   *
   */
  long HispecTrackingCamera::set_exposure_mode(const std::string &modein) {

    if (modein==HispecTrackingCameraExposureMode::TRACKING) {
      this->exposuremode = std::make_unique<ExposureModeTracking>(this);
    }
    else {
      return this->ArchonInterface::set_exposure_mode(modein);
    }

    return NO_ERROR;
  }
  /***** Camera::HispecTrackingCamera::set_exposure_mode **********************/


  /***** Camera::HispecTrackingCamera::readout ********************************/
  /**
   * @brief      stub for HISPEC Tracking Camera readout command
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long HispecTrackingCamera::readout(const std::string &args, std::string &retstring) {
    const std::string function("Camera::HispecTrackingCamera::readout");
    logwrite(function, "readout stub called with args: "+args);
    retstring="readout stub";
    return NO_ERROR;
  }
  /***** Camera::HispecTrackingCamera::readout ********************************/


  /***** Camera::HispecTrackingCamera::expose *********************************/
  /**
   * @brief      stub for HISPEC Tracking Camera expose command
   * @param[in]  args       any number of arguments
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long HispecTrackingCamera::expose(const std::string &args, std::string &retstring) {
    const std::string function("Camera::HispecTrackingCamera::expose");
    logwrite(function, "expose stub called with args: "+args);
    retstring="expose stub";
    return NO_ERROR;
  }
  /***** Camera::HispecTrackingCamera::expose *********************************/
}
