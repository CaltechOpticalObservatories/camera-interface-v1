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


  /***** Camera::ExposureModeCCD::image_acquisition_thread *******************/
  /**
   * @brief  implementation of Archon-specific image_acquisition_thread for CCD
   *
   */
  void ExposureModeCCD::image_acquisition_thread() {
    const std::string function("Camera::ExposureModeCCD::image_acquisition_thread");
    char message[256];

    logwrite(function, "");

    // record system time when exposure starts (YYYY-MM-DDTHH:MM:SS.sss)
    this->interface->camera_info.start_time = get_timestamp();

/*****
 *  // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename
 *  this->interface->set_fitstime(this->camera_info.start_time);
 *
 *  // assemble the FITS filename
 *  get_fitsname(this->camera_info.fits_name);
 *
 *  // add filename to system keys database
 *  this->add_filename_key();
*****/

    // copy systemkeys databases into camera_info
    this->interface->camera_info.systemkeys.keydb = this->interface->systemkeys.keydb;

    auto nexp = this->interface->camera_info.nexp;

    if (nexp > 1) {
      SNPRINTF(message, "starting sequence of %d frames. lastframe=%d", nexp, this->interface->controller->lastframe);
      logwrite(function, std::string(message));
    }

    this->interface->controller->get_frame_status();

    //
    // *** initiate the exposure here ***
    //

    long error = this->interface->controller->expose(nexp);

    if (error != NO_ERROR) {
      logwrite(function, "could not initiate exposure");
      return;
    }
    logwrite(function, "exposure started");

  }
  /***** Camera::ExposureModeCCD::image_acquisition_thread *******************/


  /***** Camera::ExposureModeCCD::expose *************************************/
  /**
   * @brief  implementation of Archon-specific expose for CCD
   *
   */
  void ExposureModeCCD::image_processing_thread() {
    logwrite("Camera::ExposureModeCCD::image_processing_thread","");
  }
  /***** Camera::ExposureModeCCD::expose *************************************/


  /***** Camera::ExposureModeRaw::expose *************************************/
  /**
   * @brief  implementation of Archon-specific expose for Raw
   *
   */
  long ExposureModeRaw::expose() {
    logwrite("Camera::ExposureModeRaw::expose","");
    return 0;
  }
  /***** Camera::ExposureModeRaw::expose *************************************/


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
