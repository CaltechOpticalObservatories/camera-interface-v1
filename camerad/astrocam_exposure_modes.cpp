/**
 * @file     astrocam_exposure_modes.h
 * @brief    implements AstroCam-specific exposure modes
 *
 */

#include "astrocam_exposure_modes.h"

namespace Camera {

  long Expose_Single::expose() {
    const std::string function("Camera::Expose_Single::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }

}
