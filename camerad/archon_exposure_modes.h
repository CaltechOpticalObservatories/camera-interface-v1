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

  namespace ArchonExposureMode {
    constexpr const char* RAW = "RAW";
    constexpr const char* CCD = "CCD";
    constexpr const char* RXRV = "RXRV";
    constexpr const char* ALLMODES[] = {RAW, CCD, RXRV};
    constexpr size_t NUM_MODES = sizeof(ALLMODES)/sizeof(ALLMODES[0]);
  };

  class ArchonInterface;     // forward declaration

  class ExposureModeRaw : public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeRaw(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=ArchonExposureMode::RAW;
        }

    long expose() override;
  };

  class ExposureModeCCD : public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeCCD(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=ArchonExposureMode::CCD;
        }

    void image_acquisition_thread(int nexp) override;

    void image_processing_thread() override;

    long expose() override;
  };

  class ExposureModeRXRV : public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeRXRV(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=ArchonExposureMode::RXRV;
        }

    long expose() override;
  };
}
