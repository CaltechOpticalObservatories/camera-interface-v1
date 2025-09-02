/**
 * @file     archon_exposure_modes.h
 * @brief    implements Archon-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::ExposureModeCCD *********************************************/
  /**
   * @brief  implementation of Archon-specific expose for CCD
   *
   */
  long ExposureModeCCD::expose() {
    const std::string function("Camera::ExposureModeCCD::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }
  /***** Camera::ExposureModeCCD *********************************************/


  /***** Camera::ExposureModeRXRV ********************************************/
  /**
   * @brief  implementation of Archon-specific expose for RXR-Video
   *
   */
  long ExposureModeRXRV::expose() {
    const std::string function("Camera::ExposureModeRXRV::expose");

    size_t sz=100;

    // Two each of signal and reset buffers, current and previous, since
    // we need to pair the reset from the previous frame with signal from
    // the current frame. These will hold deinterlaced frames.
    //
    std::vector<std::vector<uint16_t>> sigbuf(2, std::vector<uint16_t>(sz));
    std::vector<std::vector<uint16_t>> resbuf(2, std::vector<uint16_t>(sz));

    // allocate memory for frame buffer read from Archon
    interface->allocate_framebuf(sz);

    // create an appropriate deinterlacer object
    try { processor = make_image_processor("rxrv");
    }
    catch(const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    // read first frame pair into my frame buffer
    interface->read_frame();

    // process (deinterlace) first frame pair
    processor->deinterlacer()->deinterlace(interface->get_framebuf(), sigbuf[0].data(), resbuf[0].data());

    // sample calls to other processor functions
    uint16_t a, b;
    int16_t c;
    processor->subtractor()->subtract(&a, &b, &c);
    processor->coadder()->coadd(&a, &b);

    // show contents
    std::stringstream message;
    message << "sig:";
    for (int i=0; i<10; i++) message << " " << sigbuf[0][i];
    logwrite(function, message.str());
    message.str(""); message << "res:";
    for (int i=0; i<10; i++) message << " " << resbuf[0][i];
    logwrite(function, message.str());

    // loop:
    // subsequent frame pairs, read, deinterlace, write

    return NO_ERROR;
  }
  /***** Camera::ExposureModeRXRV ********************************************/

}
