/**
 * @file     Instruments/hispec/hispec_exposure_modes.h
 * @brief    implements HISPEC-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "hispec_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::ExposureModeFastReadout::expose *****************************/
  /**
   * @brief  implementation of hispec-specific expose for FastReadout
   *
   */
  long ExposureModeFastReadout::expose() {
    const std::string function("Camera::ExposureModeFastReadout::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }
  /***** Camera::ExposureModeFastReadout::expose *****************************/

}
