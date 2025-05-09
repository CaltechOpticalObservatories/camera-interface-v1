/**
 * @file   deinterlace_modes.cpp
 * @brief  implementation of deinterlacing factory and specializations
 *
 */

#include "deinterlace_modes.h"

namespace Camera {

  /***** Camera::make_deinterlacer ********************************************/
  /**
   * @brief      factory function creates appropriate deinterlacer object
   * @param[in]  mode
   * @return     unique_ptr to DeInterlaceBase derived object
   * @throws     std::invalid_argument
   *
   */
  std::unique_ptr<DeInterlaceBase> make_deinterlacer(const std::string &mode) {
    if (mode=="none") {
      logwrite("Camera::make_deinterlacer", "factory made deinterlacer for \'none\'");
      return std::make_unique<DeInterlaceMode<uint16_t, uint16_t, ModeNone>>();
    }
    else
    if (mode=="rxrv") {
      logwrite("Camera::make_deinterlacer", "factory made deinterlacer for \'rxrv\'");
      return std::make_unique<DeInterlaceMode<uint16_t, uint16_t, ModeRXRV>>();
    }
    else {
      logwrite("Camera::make_deinterlacer", "ERROR factory got unknown mode \'"+mode+"\'");
      throw std::invalid_argument("unknown mode "+mode);
    }
  }
  /***** Camera::make_deinterlacer ********************************************/


  /***** Camera::DeInterlaceMode::test ****************************************/
  /**
   * @brief      specialization for ModeNone
   *
   */
  template <>
  void DeInterlaceMode<uint16_t, uint16_t, ModeNone>::test() {
    const std::string function("Camera::DeInterlaceMode::ModeNone::test");
    logwrite(function, "here");
  }
  /***** Camera::DeInterlaceMode::test ****************************************/


  /***** Camera::DeInterlaceMode::test ****************************************/
  /**
   * @brief      specialization for ModeRXRV
   *
   */
  template <>
  void DeInterlaceMode<uint16_t, uint16_t, ModeRXRV>::test() {
    const std::string function("Camera::DeInterlaceMode::ModeRXRV::test");
    logwrite(function, "here");
  }
  /***** Camera::DeInterlaceMode::test ****************************************/

}
