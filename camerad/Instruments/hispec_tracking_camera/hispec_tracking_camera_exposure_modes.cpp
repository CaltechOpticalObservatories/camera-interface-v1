/**
 * @file     Instruments/hispec_tracking_camera/hispec_tracking_camera_exposure_modes.cpp
 * @brief    implements HISPEC Tracking Camera-specific exposure modes
 * @author   Michael Langmayr <langmayr@astro.caltech.edu>
 *
 */

#include "archon_exposure_modes.h"
#include "hispec_tracking_camera_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::ExposureModeTracking::expose *********************************/
  /**
   * @brief  implementation of hispec tracking camera-specific expose for Tracking mode
   *
   */
  long ExposureModeTracking::expose() {
    const std::string function("Camera::ExposureModeTracking::expose");
    logwrite(function, "tracking exposure stub");
    return NO_ERROR;
  }
  /***** Camera::ExposureModeTracking::expose *********************************/

}
