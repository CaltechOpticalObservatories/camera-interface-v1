/**
 * @file     archon_exposure_modes.h
 * @brief    delcares Archon-specific exposure mode classes
 * @details  Declares classes that implement exposure modes supported by
 *           Archon. These classes override virtual functions in the
 *           ExposureMode base class to provide mode-specific behavior.
 *
 */

#pragma once

#include "exposure_modes.h"  // ExposureMode base class

namespace Camera {

  class ArchonInterface;     // forward declaration

  class ExposureModeRaw : public ExposureMode<Camera::ArchonInterface> {
    using ExposureMode<Camera::ArchonInterface>::ExposureMode;

    long expose() override;
  };

  class ExposureModeCCD : public ExposureMode<Camera::ArchonInterface> {
    using ExposureMode<Camera::ArchonInterface>::ExposureMode;

    long expose() override;
  };

  class ExposureModeRXRV : public ExposureMode<Camera::ArchonInterface> {
    using ExposureMode<Camera::ArchonInterface>::ExposureMode;

    long expose() override;
  };
}
