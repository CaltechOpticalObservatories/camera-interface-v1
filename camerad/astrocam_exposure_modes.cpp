/**
 * @file     astrocam_exposure_modes.h
 * @brief    implements AstroCam-specific exposure modes
 *
 */

#include "astrocam_exposure_modes.h"

namespace Camera {

  long Expose_CCD::expose() {
    const std::string function("Camera::Expose_CCD::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }

}
