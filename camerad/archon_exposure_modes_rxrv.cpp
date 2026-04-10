/**
 * @file     archon_exposure_mode_rxrv.cpp
 * @brief    implements Archon-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"
#include "archon_interface.h"

namespace Camera {

  /***** Camera::ExposureMode_VIDEORXR ***************************************/
  /**
   * @brief  implementation of Archon-specific expose for RXR-Video
   *
   */
  long ExposureMode_VIDEORXR::expose() {
    std::string_view function("Camera::ExposureMode_VIDEORXR::expose");

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
    char* buffer=new char[1024]{};  // TODO temporary, for compilation only
    this->interface->controller->read_frame(ArchonController::FRAME_IMAGE, buffer);

    // process (deinterlace) first frame pair
//  processor->deinterlacer()->deinterlace(interface->get_framebuf(), sigbuf[0].data(), resbuf[0].data());

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

    delete [] buffer;

    return NO_ERROR;
  }
  /***** Camera::ExposureMode_VIDEORXR ****************************************/


  /***** Camera::ExposureMode_VIDEORXR::image_acquisition_thread **************/
  /**
   * @brief      producer thread for ExposureMode VIDEORXR
   * @details    Spawned by Camera::ArchonInterface::do_expose()
   *
   */
  void ExposureMode_VIDEORXR::image_acquisition_thread() {
    std::string_view function("Camera::ExposureMode_VIDEORXR::image_acquisition_thread");
    char message[256];

    logwrite(function, "started");

    // get value for 'Expose = <nexp>' from the class args for the exposure mode
    // default = 1
    int nexp=1;
    if (this->modeargs.size() > 0) {
      try { nexp = std::stoi(modeargs.at(0));
      }
      catch (const std::exception &e) {
        logwrite(function, "ERROR getting nexp from mode args: "+std::string(e.what()));
        return;
      }
    }

    auto info = &this->interface->camera_info;

    // record system time when exposure starts (YYYY-MM-DDTHH:MM:SS.sss)
    info->start_time = get_timestamp();

    // ---------- initiate the exposure --------------------
    //
    if ( this->interface->controller->initiate_exposure(nexp) != NO_ERROR ) {
      logwrite(function, "could not initiate exposure");
      return;
    }
    logwrite(function, "exposure initiated with "+std::to_string(nexp)+" frames");

    long error=NO_ERROR;

    uint64_t bufferbytes = (uint64_t)info->image_data_bytes * info->cubedepth;

    while (error==NO_ERROR && !this->interface->is_aborted() && nexp > 0) {

      // prepare an ImageBuffer object for the exposure
      auto imagebuffer = std::make_shared<ArchonImageBuffer>();
      try { imagebuffer->rawpixels.reset(new char[bufferbytes]);
      }
      catch (const std::exception &e) {
        SNPRINTF(message, "memory allocation failed: %s", e.what());
        logwrite(function, "ERROR "+std::string(message));
        error=ERROR;
        break;
      }

      imagebuffer->n_slices = 1;  // VIDEORXR uses extensions, not cubes

      imagebuffer->bufframen_slice.reserve(imagebuffer->n_slices);
      imagebuffer->buftimestamp_slice.reserve(imagebuffer->n_slices);

      // wait for frame readout into Archon buffer
      if ( (error=this->interface->controller->wait_for_readout()) == ERROR ) break;

      // ---------- read frame from Archon -----------------
      //
      char* base = imagebuffer->rawpixels.get();
      char* ptr  = base;
      error = this->interface->controller->read_frame( ArchonController::FRAME_IMAGE, ptr );

      // frame metadata
      auto index = this->interface->controller->frameinfo.index.load();
      imagebuffer->bufframen_slice.push_back( this->interface->controller->frameinfo.bufframen[index] );
      imagebuffer->buftimestamp_slice.push_back( this->interface->controller->frameinfo.buftimestamp[index] );

      // ---------- push frame into queue ------------------
      //
      this->image_queue.enqueue( std::move(imagebuffer) );
      logwrite("Camera::ExposureMode_VIDEORXR::image_acquisition_thread", "pushed frame");

      nexp--;

    }  // end loop over number of frames

    // ---------- end-of-queue marker ----------------------
    //
    this->image_queue.enqueue( nullptr );

    logwrite(function, "complete");
  }
  /***** Camera::ExposureMode_VIDEORXR::image_acquisition_thread **************/


  /***** Camera::ExposureMode_VIDEORXR::image_processing_thread ***************/
  /**
   * @brief      consumer thread for ExposureMode VIDEORXR
   * @details    Spawned by Camera::ArchonInterface::do_expose()
   *
   */
  void ExposureMode_VIDEORXR::image_processing_thread() {
    std::string_view function("Camera::ExposureMode_VIDEORXR::image_processing_thread");
    logwrite(function, "started");

//  output->open();                                        PLACEHOLDER

    const long bufcols = this->interface->camera_info.naxes[0] / 2;
    const long bufrows = this->interface->camera_info.naxes[1];
    const long imgcols = bufcols - 64;
    const long imgrows = bufrows;

    // two sig frames, curr and prev
    std::vector<uint32_t> sig[2] = { std::vector<uint32_t>(bufcols*bufrows),
                                     std::vector<uint32_t>(bufcols*bufrows) };

    // two res frames, curr and prev
    std::vector<uint32_t> res[2] = { std::vector<uint32_t>(bufcols*bufrows),
                                     std::vector<uint32_t>(bufcols*bufrows) };

    // one cds frame, sig[curr] - res[prev]
    std::vector<uint32_t> cds(imgcols*imgrows);

    int count=0;

    // ---------- pop image out of queue -------------------
    //
    while (!this->interface->is_aborted()) {

      auto image = this->image_queue.dequeue();  // what comes out of queue is curr frame

      if (!image) break;                         // end of queue is marked with a nullptr

      uint32_t* raw = reinterpret_cast<uint32_t*>(image->rawpixels.get());

      auto [curr, prev] = get_indices(count);    // indices of curr and prev frames based on count

      // ---------- deinterlace current image --------------
      //
      processor->deinterlacer()->deinterlace( raw,
                                              sig[curr].data(),
                                              res[curr].data(),
                                              bufcols,
                                              bufrows );

//    // ---------- write unprocessed frames ---------------
//    //
//    if (unp) {                                           PLACEHOLDER
//      output->write( sig[curr].data(), info_unp );
//      output->write( res[curr].data(), info_unp );
//    }
//
//    // ---------- write processed (CDS) frame ------------
//    //
//    if (count > 0 && processor->has_subtractor()) {      PLACEHOLDER
//
//      processor->subtractor()->subtract( sig[curr].data(),
//                                         res[prev].data(),
//                                         cds.data(),
//                                         bufcols, bufrows,
//                                         imgcols, imgrows );
//
//      output->write( cds.data(), info_cds );             PLACEHOLDER
//    }

      count++;
    }

//  output->close();                                       PLACEHOLDER

    logwrite(function, "complete");
  }
  /***** Camera::ExposureMode_VIDEORXR::image_processing_thread ***************/
}
