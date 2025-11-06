/**
 * @file     Instruments/hispec/hispec_exposure_modes.h
 * @brief    delcares HISPEC-specific exposure mode classes
 * @details  Declares classes that implement exposure modes supported by
 *           HISPEC. These classes override virtual functions in the
 *           ExposureMode base class to provide mode-specific behavior.
 *
 */

#pragma once

#include "exposure_modes.h"  // ExposureMode base class

namespace Camera {

  /**
   * @namespace  all recognized exposure modes for Hispec
   */
  namespace HispecExposureMode {
    constexpr const char* FASTREADOUT = "FASTREADOUT";
    constexpr const char* SLOWREADOUT = "SLOWREADOUT";
    constexpr const char* ALLMODES[] = {FASTREADOUT, SLOWREADOUT};
  }

  class Hispec;

  class ExposureModeFastReadout : public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeFastReadout(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=HispecExposureMode::FASTREADOUT;
        }

    long expose() override;
  };

  class ExposureModeSlowReadout : public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeSlowReadout(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=HispecExposureMode::SLOWREADOUT;
        }

    long expose() override;
  };

}
