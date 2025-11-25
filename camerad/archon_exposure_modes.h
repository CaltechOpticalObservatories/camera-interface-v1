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
#include "archon_interface.h"

namespace Camera {

  /**
   * @namespace  all recognized exposure modes
   */
  namespace ArchonExposureMode {
    constexpr const char* RAW = "RAW";
    constexpr const char* SINGLE = "SINGLE";
    constexpr const char* RXRV = "RXRV";
    constexpr const char* ALLMODES[] = {RAW, SINGLE, RXRV};
  };

  /** @struct   ArchonImageBuffer
   *  @brief    holds one or more frames and metadata from the controller
   *  @details  This inherits from Camera::ImageBuffer which contains
   *            int n_slices;
   *            std::shared_ptr<char[]> rawpixels;
   *            and includes additional metadata for Archon.
   *            Archon pixels are read as 8-bit <char> type.
   */
  struct ArchonImageBuffer : public ImageBuffer<char> {
    std::vector<int> bufframen_slice;          ///< Archon frame number(s) for all slices in this image
    std::vector<uint64_t> buftimestamp_slice;  ///< Archon timestamp(s) for all slices in this image
  };

  class ArchonInterface;     // forward declaration

  /**
   * @brief      class constructor
   * @param[in]  _interface  Pointer to Camera InterfaceType
   */
  class ExposureModeRaw : public ArchonImageBuffer, public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeRaw(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          this->type=ArchonExposureMode::RAW;
        }

    long expose() override;
  };

  /***** Camera::ExposureModeSingle *******************************************/
  /**
   * @class      Camera::ExposureModeSingle
   * @brief      derived class for Exposure Mode Single
   *
   */
  class ExposureModeSingle : public ArchonImageBuffer, public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeSingle(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=ArchonExposureMode::SINGLE;
        }

      /** @var imagebuf_queue
       *  @brief the FIFO queue to contain images from Archon
       */
      std::queue<std::shared_ptr<ArchonImageBuffer>> imagebuf_queue;

      void image_acquisition_thread() override;
      void image_processing_thread() override;
      long expose() override;
      void process_image(std::shared_ptr<ArchonImageBuffer> &imagebuffer);
  };
  /***** Camera::ExposureModeSingle *******************************************/


  class ExposureModeRXRV : public ArchonImageBuffer, public ExposureModeTemplate<Camera::ArchonInterface> {
    public:
      ExposureModeRXRV(Camera::ArchonInterface* iface)
        : ExposureModeTemplate<Camera::ArchonInterface>(iface) {
          type=ArchonExposureMode::RXRV;
        }

    long expose() override;
  };
}
