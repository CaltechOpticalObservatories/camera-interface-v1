/**
 * @file   deinterlace_modes.h
 * @brief  template-based implementation for deinterlacing
 *
 */

#pragma once

#include "common.h"

namespace Camera {

  /**
   * @brief    tags for constructing appropriate deinterlacer object
   * @details  DeInterlaceMode tags are used to distinguish the type
   *           of deinterlacer the factory function will make. Empty
   *           structs take zero space.
   */
  struct ModeNone {};
  struct ModeRXRV {};
  struct ModeFowler {};
  struct ModeCCD {};
  struct ModeUTR {};

  /**
   * @brief    DeInterlace abstract base class
   * @details  Actual deinterlacing implementations are defined in
   *           template specifications.
   */
  class DeInterlaceBase {
    public:
      virtual ~DeInterlaceBase() = default;
      virtual void test() = 0;
  };

  /**
   * @brief    template for mode-specific deinterlacing
   * @details  Specialize this class for each ModeTag to implement the
   *           corresponding deinterlacer.
   */
  template <typename TIN, typename TOUT, typename ModeTag>
  class DeInterlaceMode : public DeInterlaceBase {
    public:
      void test() override;
  };

  // factory function creates appropriate deinterlacer object
  std::unique_ptr<DeInterlaceBase> make_deinterlacer(const std::string &mode);

}
