/**
 * @file     exposure_modes.h
 * @brief    defines supported camera exposure modes
 * @details  Contains derived classes for each exposure mode, selected by
 *           Camera::Interface::select_expose_mode(). These classes inherit
 *           from Camera::ExposureMode and override the expose function with
 *           mode-specific behavior. There can be an implementation for each
 *           controller.
 *
 */
#pragma once

#include "common.h"
#include "camera_information.h"
#include "deinterlace_modes.h"

namespace Camera {

  class Interface;  // forward declaration for ExposureMode class

  /***** Camera::ExposureMode *************************************************/
  /**
   * @class      Camera::ExposureMode
   * @brief      abstract base class for camera exposure modes
   * @details    This base class is inherited by mode-specific classes. Each
   *             derived class must implement the expose() function which
   *             defines the exposure sequence for that exposure mode.
   *
   */
  class ExposureMode {
    protected:
      Camera::Interface* interface;   //!< pointer to the Camera::Interface class

      std::unique_ptr<DeInterlaceBase> deinterlacer;

      // Each exposure gets its own copy of the Camera::Information class.
      // There is one each for processed and unprocessed images.
      //
      Camera::Information fits_info;  //!< processed images
      Camera::Information unp_info;   //!< un-processed images

    public:
      ExposureMode(Camera::Interface* _interface) : interface(_interface) { }

      virtual ~ExposureMode() = default;

      virtual long expose() = 0;

      template<typename T> void create_deinterlacer(const std::string &mode,
                                                    const std::vector<T> &buf,
                                                    const size_t bufsz) {
        this->deinterlacer = deinterface_factory(mode, buf, bufsz);
      }
  };
  /***** Camera::ExposureMode *************************************************/

}
