/**
 * @file     exposure_modes.h
 *
 * @brief    describes the supported exposure modes
 *
 * @details  This contains derived classes for each exposure mode, selected
 *           by Archon::Interface::select_expose_mode(). These inherit from
 *           the ExposureBase class and override the expose_for_mode function.
 *
 *           Each class is constructed with a pointer to the Archon::Interface
 *           class so that member functions have access to Interface, using the
 *           "this->interface->" pointer dereference.
 *
 */
#pragma once

#include "archon.h"

namespace Archon {

  /***** Archon::ExposureBase *************************************************/
  /**
   * @class      Archon::ExposureBase
   * @brief      exposure base class
   * @details    This class gets inherited by a derived Exposure_xxx class and
   *             contains a virtual declaration for expose_for_mode() which is
   *             overridden in each derived class and contains the exposure
   *             sequence for that exposure mode.
   *
   */
  class ExposureBase {
    protected:
      Archon::Interface* interface;  // pointer to the Archon::Interface class
      std::unique_ptr<DeInterlaceBase> deinterlacer;
      int nseq;

      // Each exposure gets its own copy of the Camera::Information class.
      // There is one each for processed and unprocessed images.
      //
      Camera::Information fits_info;  /// processed images
      Camera::Information unp_info;   /// un-processed images

    public:
      ExposureBase(Interface* interface) : interface(interface), deinterlacer(nullptr), nseq(1) { }

      virtual ~ExposureBase() = default;

      virtual long expose_for_mode() = 0;

      long expose( const int &nseq_in );

      template <typename T>
      void create_deinterlacer( const std::string &mode, const std::vector<T> &buf, const size_t imgsz ) {
        this->deinterlacer = deinterlacer_factory(mode, buf, imgsz);
      }
  };
  /***** Archon::ExposureBase *************************************************/


  /***** Archon::Expose_Raw ***************************************************/
  /**
   * @class      Archon::Expose_Raw
   * @brief      derived exposure mode class for raw sampling, oscilloscope mode
   *
   */
  class Expose_Raw : public ExposureBase {
    public:
      Expose_Raw(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_Raw::expose_for_mode";
        logwrite( "Archon::Expose_Raw::expose_for_mode", "Raw" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_Raw ***************************************************/


  /***** Archon::Expose_CCD ***************************************************/
  /**
   * @class      Archon::Expose_CCD
   * @brief      derived exposure mode class for CCDs
   *
   */
  class Expose_CCD : public ExposureBase {
    public:
      Expose_CCD(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_CCD::expose_for_mode";
        logwrite( "Archon::Expose_CCD::expose_for_mode", "CCD" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_CCD ***************************************************/


  /***** Archon::Expose_UTR ***************************************************/
  /**
   * @class      Archon::Expose_UTR
   * @brief      derived exposure mode class for Up The Ramp
   *
   */
  class Expose_UTR : public ExposureBase {
    public:
      Expose_UTR(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_UTR::expose_for_mode";
        logwrite( "Archon::Expose_UTR::expose_for_mode", "UTR" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_UTR ***************************************************/


  /***** Archon::Expose_Fowler ************************************************/
  /**
   * @class      Archon::Expose_Fowler
   * @brief      derived exposure mode class for Fowler sampling
   *
   */
  class Expose_Fowler : public ExposureBase {
    public:
      Expose_Fowler(Interface* interface) : ExposureBase(interface) { }

      long expose_for_mode() override {
        const std::string function="Archon::Expose_Fowler::expose_for_mode";
        logwrite( "Archon::Expose_Fowler::expose_for_mode", "Fowler" );
        return NO_ERROR;
      }
  };
  /***** Archon::Expose_Fowler ************************************************/


  /***** Archon::Expose_RXRV **************************************************/
  /**
   * @class      Archon::Expose_RXRV
   * @brief      derived exposure mode class for RXR video
   * @details    Each Archon frame buffer contains a pair of frames, a read and
   *             a reset frame. The reset frame belongs to the next read, so
   *             the read of the first pair and the reset of the last pair are
   *             not used.
   *
   */
  class Expose_RXRV : public ExposureBase {
    private:
      template <typename T>
      void process_frame() {
        std::vector<T> converted_buf = convert_archon_buffer<T,T>(interface->archon_buf, interface->camera_info.image_size);
        this->deinterlace(converted_buf, interface->camera_info.image_size);
      }

      template <typename T>
      void deinterlace( const std::vector<T> &converted_buf, const size_t imgsz ) {
        auto deinterlacer = deinterlace_factory<T>("rxrv", converted_buf, imgsz);
        if ( deinterlacer ) deinterlacer->deinterlace();
        else {
          throw std::runtime_error("Archon::Expose_RXRV::deinterlace failed to create deinterlacer");
        }
      }

    public:
      Expose_RXRV(Interface* interface) : ExposureBase(interface) { }

      /***** Archon::Expose_RXRV::expose_for_mode *****************************/
      /**
       * @brief      this is the exposure sequence for RXR Video
       *
       */
      long expose_for_mode() override {
        const std::string function="Archon::Expose_RXRV::expose_for_mode";
        std::stringstream message;
        long error = ERROR;

        // Create FITS_file pointers for the files used in this function,
        // for CDS and unprocessed images.
        //
        auto file_cds = std::make_unique<FITS_file<int32_t>>(interface->camera.datacube() ? true : false);
        auto file_unp = std::make_unique<FITS_file<uint16_t>>(interface->camera.datacube() ? true : false);

        logwrite( function, "RXRV" );
        logwrite( "Archon::Expose_RXRV::isconnected", std::to_string(interface->archon.isconnected()) );

        // ***************************************
        // ******** first frame pair here ********
        // ***************************************

        // Read the first frame buffer from Archon to host and decrement my local
        // frame counter.  This call to read_frame() reads into interface->archon_buf.
        //
        error = interface->read_frame();

        if ( error != NO_ERROR ) {
          logwrite( function, "ERROR reading frame buffer" );
          return ERROR;
        }

        // Processing the first frame pair will convert the Archon char* buffer
        // and deinterlace, producing a pair of properly typed and deinterlaced
        // frames in _sigbuf[i] and _resbuf[i].
        //
        switch (interface->camera_info.bitpix) {
          case ULONG_IMG:  process_frame<uint32_t>();
                           break;
          case USHORT_IMG: process_frame<uint16_t>();
                           break;
        }

        // Always deinterlace first frame.
        // Write here only if is_unp set to write unprocessed images.
        //
        if ( deinterlacer ) deinterlacer->deinterlace();
        else {
          logwrite( function, "ERROR no deinterlacer" );
          return ERROR;
        }

        //
        // -- MAIN SEQUENCE LOOP --
        //
        while ( nseq > 0 ) {

          // increment ring index
          //
          interface->ring_index_inc();

          if ( interface->camera_info.exposure_time.value() != 0 ) {           // wait for the exposure delay to complete (if there is one)
            error = interface->wait_for_exposure();
            if ( error != NO_ERROR ) {
              logwrite( function, "ERROR: waiting for exposure" );
              break;
            }
          }

          // Wait for detector readout into Archon internal frame buffer
          //
          error = interface->wait_for_readout();
          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR waiting for readout" );
            break;
          }

          // *********************************************
          // ******** subsequent frame pairs here ********
          // *********************************************

          // Read the next (and subsequent) frame buffers from Archon to host and
          // decrement my local frame counter.  This reads into interface->archon_buf.
          //
          error = interface->read_frame();
          nseq--;

          if ( error != NO_ERROR ) {
            logwrite( function, "ERROR reading frame buffer" );
            break;
          }

          // De-interlace each subsequent frame
          //

          // Write CDS frame
          //

          if ( error != NO_ERROR ) break;
        }

        // Complete the FITS file after processing all frames.
        // This closes the file(s) for any defined FITS_file object(s), and
        // shuts down the FITS engine, waiting for the queue to empty if needed.
        //
        if ( file_cds ) file_cds->complete();
        if ( file_unp ) file_unp->complete();

        return error;
      }
      /***** Archon::Expose_RXRV::expose_for_mode *****************************/
  };
  /***** Archon::Expose_RXRV **************************************************/

}
