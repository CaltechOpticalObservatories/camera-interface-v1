/**
 * @file     exposure_modes.h
 * @brief    defines camera exposure mode infrastructure
 * @details  Declares abstract base classes for camera exposure modes.
 *           Each derived exposure mode implements the specific exposure
 *           logic for a given Interface type. The appropriate mode is
 *           selected by Camera::Interface::select_expose_mode().
 *
 */
#pragma once

#include "common.h"
#include "camera_information.h"
#include "deinterlace_modes.h"

namespace Camera {

  class Interface;  // forward declaration for ExposureMode class

  /***** Camera::ExposureModeBase *********************************************/
  /**
   * @brief      non-templated base class for polymorphic exposure mode access
   * @details    Provides a common interface for exposure mode implementations,
   *             used to hold ExposureMode instances via polymorphic pointers.
   *
   */
  class ExposureModeBase {
    public:
      virtual ~ExposureModeBase() = default;
      virtual long expose() = 0;
  };
  /***** Camera::ExposureModeBase *********************************************/


  /***** Camera::ExposureMode *************************************************/
  /**
   * @class      Camera::ExposureMode
   * @brief      templated abstract base class for exposure mode implementations
   * @details    Defines Interface and common member functions for all exposure
   *             modes. Each mode inherits from this class and implements the
   *             expose() function with logic specific to that mode. The template
   *             parameter InterfaceType provides access to the appropriate
   *             Camera Interface.
   *
   */
  template <typename InterfaceType>
  class ExposureMode : public ExposureModeBase {
    protected:
      InterfaceType* interface;   //!< pointer to the specific Camera Interface instance

      // Pointer to the deinterlacer for this mode. This is a pointer
      // to the base class -- each exposure mode will have to initialize
      // this to an appropriate deinterlacer using a factory function.
      //
      std::unique_ptr<DeInterlaceBase> deinterlacer;

      // Each exposure gets its own copy of the Camera::Information class.
      // There is one each for processed and unprocessed images.
      //
      Camera::Information fits_info;  //!< processed images
      Camera::Information unp_info;   //!< un-processed images

    public:
      /**
       * @brief      class constructor
       * @param[in]  _interface  Pointer to Camera InterfaceType
       */
      ExposureMode(InterfaceType* _interface) : interface(_interface) { }

      virtual ~ExposureMode() = default;

      virtual long expose() = 0;

  };
  /***** Camera::ExposureMode *************************************************/

}
