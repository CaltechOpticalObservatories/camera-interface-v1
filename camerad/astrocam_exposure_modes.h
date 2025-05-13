/**
 * @file     astrocam_exposure_modes.h
 * @brief    declares AstroCam-specific exposure mode classes
 * @details  Declares classes that implement exposure modes supported by
 *           AstroCam. These classes override virtual functions in the
 *           ExposureMode base class to provide mode-specific behavior.
 *
 */

#pragma once

#include "exposure_modes.h"  // ExposureMode base class

namespace Camera {

  class AstroCamInterface;   // forward declaration

  class Expose_CCD : public ExposureMode<Camera::AstroCamInterface> {
    using ExposureMode<Camera::AstroCamInterface>::ExposureMode;

    long expose() override;
  };

}
