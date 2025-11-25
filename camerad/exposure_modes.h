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
#include "image_process.h"

namespace Camera {

  class Interface;  // forward declaration for ExposureMode class

  /***** Camera::ExposureMode *************************************************/
  /**
   * @brief      non-templated base class for polymorphic exposure mode access
   * @details    Provides a common interface for exposure mode implementations,
   *             used to hold ExposureMode instances via polymorphic pointers.
   *
   */
  class ExposureMode {
    protected:
      std::string type;               ///< what type of exposure mode is this?
      std::vector<std::string> args;  ///< optional mode-specific args

    public:
      std::mutex queue_mutex;            ///< mutex protects access to the queue
      std::condition_variable queue_cv;  ///< notify when the queue has new data

      std::atomic<bool> is_producer_finished;
      std::atomic<bool> is_producer_error;
      std::atomic<bool> is_consumer_error;

      std::string get_type() { return this->type; }
      std::vector<std::string> get_args() { return this->args; }

      /** brief  return the args as a space-delimited string */
      std::string get_args_string() {
        std::ostringstream oss;
        for (const auto &arg : this->args) { if (!arg.empty()) { oss << " " << arg; } }
        return oss.str();
      }

      virtual ~ExposureMode() = default;
      virtual long expose() = 0;
      virtual void image_acquisition_thread() { };
      virtual void image_processing_thread() { };

      virtual void test() { logwrite("Camera::ExposureMode","not implemented"); }
  };
  /***** Camera::ExposureMode *************************************************/


  /***** Camera::ExposureModeTemplate *****************************************/
  /**
   * @class      Camera::ExposureModeTemplate
   * @brief      templated abstract base class for exposure mode implementations
   * @details    Defines Interface and common member functions for all exposure
   *             modes. Each mode inherits from this class and implements the
   *             expose() function with logic specific to that mode. The template
   *             parameter InterfaceType provides access to the appropriate
   *             Camera Interface.
   *
   */
  template <typename InterfaceType>
  class ExposureModeTemplate : public ExposureMode {
    protected:
      InterfaceType* interface;   //!< pointer to the specific Camera Interface instance

      // Pointer to the deinterlacer for this mode. This is a pointer
      // to the base class -- each exposure mode will have to initialize
      // this to an appropriate deinterlacer using a factory function.
      //
      std::unique_ptr<ImageProcessor> processor;

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
      ExposureModeTemplate(InterfaceType* _interface) : interface(_interface) { }

      virtual ~ExposureModeTemplate() = default;
  };
  /***** Camera::ExposureModeTemplate *****************************************/

}
