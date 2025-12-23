/**
 * @file    Instruments/hispec_tracking_camera/hispec_tracking_camera_instrument.h
 * @brief   contains properties unique to the HISPEC Tracking Camera instrument
 * @author  Michael Langmayr <langmayr@astro.caltech.edu>
 *
 */
#pragma once

#include "archon_interface.h"  /// HISPEC Tracking Camera uses ArchonInterface

namespace Camera {

  /***** Camera::HispecTrackingCamera *******************************************/
  /**
   * @class    HispecTrackingCamera
   * @brief    derived class inherits from ArchonInterface
   * @details  This class describes HISPEC Tracking Camera-specific functionality.
   *
   */
  class HispecTrackingCamera : public ArchonInterface {
    public:
      // instrument command dispatcher
      long instrument_cmd(const std::string &cmd,
                          const std::string &args,
                          std::string &retstring) override;

      void configure_instrument() override;

      std::vector<std::string> get_exposure_modes() override;
      long set_exposure_mode(const std::string &modein) override;

    private:
      // these are HISPEC Tracking Camera-specific functions
      long readout(const std::string &args, std::string &retstring);
      long expose(const std::string &args, std::string &retstring);
  };
  /***** Camera::HispecTrackingCamera *******************************************/
}
