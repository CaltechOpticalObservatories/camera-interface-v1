/**
 * @file    Instruments/hispec/hispec_instrument.h
 * @brief   contains properties unique to the HISPEC instrument
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include "archon_interface.h"  /// HISPEC uses ArchonInteface

namespace Camera {

  /***** Camera::Hispec *******************************************************/
  /**
   * @class    Hispec
   * @brief    derived class inherits from ArchonInterface
   * @details  This class describes HISPEC-specific functionality.
   *
   */
  class Hispec : public ArchonInterface {
    public:
      // instrument command dispatcher
      long instrument_cmd(const std::string &cmd,
                          const std::string &args,
                          std::string &retstring) override;

      void configure_instrument() override;

      std::vector<std::string> get_exposure_modes() override;
      long set_exposure_mode(const std::string &modein) override;

    private:
      // these are HISPEC-specific functions
      long hispec_this(const std::string &args, std::string &retstring);
      long hispec_that(const std::string &args, std::string &retstring);
  };
  /***** Camera::Hispec *******************************************************/
}
