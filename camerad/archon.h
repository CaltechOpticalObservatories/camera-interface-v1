/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef ARCHON_H
#define ARCHON_H

#include <queue>
#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>
#include <cmath>
#include <fenv.h>
#include <memory>                  // for std::unique and friends (c++17)
#include <sstream>                 // for std::stringstream
#include <iomanip>                 // for setfil, setw, etc.
#include <iostream>                // for hex, uppercase, etc.
#include <algorithm>
#include <cctype>
#include <string>
#include <fstream>
#include <inttypes.h>

// #include <Python.h>

// #include "pyhelper.h"
#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "build_date.h"
#include "fits_file.h"
#include "nirc2.h"
#include "opencv2/opencv.hpp"

#define MAXADCCHANS 16             //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
#define MAXADMCHANS 72             //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
#define BLOCK_LEN 1024             //!< Archon block size
#define REPLY_LEN 100 * BLOCK_LEN  //!< Reply buffer size (over-estimate)

// Archon commands
//
#define  SYSTEM        std::string("SYSTEM")
#define  STATUS        std::string("STATUS")
#define  FRAME         std::string("FRAME")
#define  CLEARCONFIG   std::string("CLEARCONFIG")
#define  POLLOFF       std::string("POLLOFF")
#define  POLLON        std::string("POLLON")
#define  APPLYALL      std::string("APPLYALL")
#define  POWERON       std::string("POWERON")
#define  POWEROFF      std::string("POWEROFF")
#define  APPLYCDS      std::string("APPLYCDS")
#define  APPLYSYSTEM   std::string("APPLYSYSTEM")
#define  RESETTIMING   std::string("RESETTIMING")
#define  LOADTIMING    std::string("LOADTIMING")
#define  HOLDTIMING    std::string("HOLDTIMING")
#define  RELEASETIMING std::string("RELEASETIMING")
#define  LOADPARAMS    std::string("LOADPARAMS")
#define  TIMER         std::string("TIMER")
#define  FETCHLOG      std::string("FETCHLOG")
#define  UNLOCK        std::string("LOCK0")

// Minimum required backplane revisions for certain features
//
#define REV_RAMP           std::string("1.0.548")
#define REV_SENSORCURRENT  std::string("1.0.758")
#define REV_HEATERTARGET   std::string("1.0.1087")
#define REV_FRACTIONALPID  std::string("1.0.1054")
#define REV_VCPU           std::string("1.0.784")

namespace Archon {

  // Archon hardware-based constants.
  // These shouldn't change unless there is a significant hardware change.
  //
  const int nbufs = 3;             //!< total number of frame buffers  //TODO rename to maxnbufs?
  const int nmods = 12;            //!< number of modules per controller
  const int nadchan = 4;           //!< number of channels per ADC module

  // Parameter defaults, unless overridden by the configuration file
  //
  const int DEF_TRIGIN_EXPOSE_ENABLE   = 1;
  const int DEF_TRIGIN_EXPOSE_DISABLE  = 0;
  const int DEF_TRIGIN_UNTIMED_ENABLE  = 1;
  const int DEF_TRIGIN_UNTIMED_DISABLE = 0;
  const int DEF_TRIGIN_READOUT_ENABLE  = 1;
  const int DEF_TRIGIN_READOUT_DISABLE = 0;
  const int DEF_SHUTENABLE_ENABLE      = 1;
  const int DEF_SHUTENABLE_DISABLE     = 0;

  // ENUM list for each readout type
  //
  const int READOUT_NONE       = 0;
  const int READOUT_NIRC2      = 1;
  const int READOUT_NIRC2VIDEO = 2;
  const int READOUT_TEST       = 3;

  const int CDS_OFFS =   0;  /// offset to add to read frame for cds images before subtraction


  /***** Archon::mat_add_will_overflow ****************************************/
  /**
   * @brief      check if adding two cv::Mat arrays could result in an overflow
   * @details    Include the explicit template argument specification.
   * @param[in]  dest  destination Mat array
   * @param[in]  src   source Mat array, added to dest
   * @return     true | false
   *
   * Returns true if adding src onto dest could result in an overflow of
   * the type <T> in either the positive (max) or negative (min) direction.
   *
   */
  template<typename T>
  bool mat_add_will_overflow( const cv::Mat &dest, const cv::Mat &src ) {
    // Find the min/max values in dest and src Mat arrays
    //
    double max_dest, max_src, min_dest, min_src;
    cv::minMaxLoc( dest, &min_dest, &max_dest );
    cv::minMaxLoc( src,  &min_src,  &max_src  );

    // Calculate the potential maximum value after adding src to dest
    //
    T potential_max, potential_min;
    try {
      potential_max = cv::saturate_cast<T>(max_dest + max_src);
      potential_min = cv::saturate_cast<T>(min_dest + min_src);
    } catch ( const std::exception &e ) {
      std::stringstream message;
      message << "ERROR calculating potential min/max values: " << e.what();
      logwrite( "Archon::mat_add_will_overflow", message.str() );
      return true;
    }

    // Check against the numeric limits for type <T>
    //
    return( potential_max > std::numeric_limits<T>::max() ||
            potential_min < std::numeric_limits<T>::min() );
  }
  /***** Archon::mat_add_will_overflow ****************************************/


  /***** Archon::PythonProc ***************************************************/
  /**
   * @class  PythonProc
   * @brief  This is the class for external Python processors
   *
   */
/***
  class PythonProc {
    private:
    public:
      PythonProc();
      ~PythonProc();

      CPyInstance hInstance;
      CPyObject pName;
      CPyObject pModule;

  };
***/
  /***** Archon::PythonProc ***************************************************/


  /***** Archon::DeInterlace **************************************************/
  /**
   * @class  DeInterlace
   * @brief  This is the deinterlacing class
   *
   */
  template <typename T>
  class DeInterlace {
    public:
    private:
      T* cdsbuf;
      T* workbuf;
      T* imbuf;
      cv::Mat resetframe;
      cv::Mat readframe;
      int32_t* coaddbuf;    /// final coadd buffer written to FITS
      int32_t* mcdsbuf_0;   /// first group of MCDS coadds (baseline)
      int32_t* mcdsbuf_1;   /// second group of MCDS coadds (signal)
      bool iscds;
      int nmcds;            /// number of MCDS samples
      int cols;
      int rows;
      int readout_type;
      int frame_rows;
      int frame_cols;
      int depth;


      /***** Archon::DeInterlace::test ****************************************/
      /**
       * @brief     test
       * @details 
       *
       */
      void test() {

//      int bufrows = this->rows * this->depth;

        for (long pix=0; pix < 256*256; pix++) {
          this->imbuf[pix] = static_cast<T>(pix % 65535);
        }

        cv::Mat image ( 256, 256, CV_16U, this->imbuf );

        return;
      }
      /***** Archon::DeInterlace::test ****************************************/


      /***** Archon::DeInterlace::nirc2_video *********************************/
      /**
       * @brief      deinterlacing for video modes
       * @details    Video modes contain two frames per image buffer,
       *             which are split here into two separate buffers,
       *             then the regular deinterlacing function can be called
       *             on each of those buffers.
       *
       */
      void nirc2_video() {
        int bufrows = this->rows * this->depth;
#ifdef LOGLEVEL_DEBUG
        char message[256];
        SNPRINTF(message, "[DEBUG] bufrows=%d this->rows=%d this->cols=%d", bufrows, this->rows, this->cols);
        logwrite( "Archon::DeInterlace::nirc2", std::string(message) );
#endif

        // These Mat objects are the input frames
        //
        cv::Mat raw    = cv::Mat( bufrows,   this->cols, CV_16U, this->imbuf   );
        cv::Mat signal = cv::Mat( bufrows/2, this->cols, CV_16U, cv::Scalar(0) );
        cv::Mat reset  = cv::Mat( bufrows/2, this->cols, CV_16U, cv::Scalar(0) );

        // These Mat objects hold the deinterlaced frames
        //
        cv::Mat deinter_reset  = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );
        cv::Mat deinter_signal = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );


        // Copy pairs of rows from the raw to the reset and signal frame Mat objects
        //
        for ( int rawrow=0, newrow=0; rawrow < bufrows-4; rawrow+=4, newrow+=2 ) {
          raw.row( rawrow+0 ).copyTo(  reset.row(newrow+0) );
          raw.row( rawrow+1 ).copyTo(  reset.row(newrow+1) );
          raw.row( rawrow+2 ).copyTo( signal.row(newrow+0) );
          raw.row( rawrow+3 ).copyTo( signal.row(newrow+1) );
        }

#ifdef LOGLEVEL_DEBUG
        logwrite( "Archon::DeInterlace::nirc2", "[DEBUG] extracted reset/read pairs from video frame" );
        SNPRINTF(message, "[DEBUG] deinter_reset.rows=%d deinter_reset.cols=%d deinter_signal.rows=%d deinter_signal.cols=%d",
                 deinter_reset.rows, deinter_reset.cols, deinter_signal.rows, deinter_signal.cols);
        logwrite( "Archon::DeInterlace::nirc2", std::string(message) );
#endif

        // Now that the video frame has been split into separate buffers (for reset and signal frames)
        // the regular deinterlacing function can be used on each, individually.
        //
        int workindex=0;
        this->nirc2( workindex, reset, deinter_reset );    // deinterlace here, reset --> deinter_reset
        this->nirc2( workindex, signal, deinter_signal );  // deinterlace here, signal --> deinter_signal

/**
        // Display live video of subtracted image
        //
        cv::Mat diff = deinter_reset - deinter_signal;
        cv::namedWindow( "image", cv::WINDOW_AUTOSIZE );
        cv::imshow( "image", diff );
        cv::waitKey(30);

#ifdef LOGLEVEL_DEBUG
        for ( int row=0; row<10; row++ ) {
          message.str(""); message << "[DEBUG] reset:" << reset.at<uint16_t>(row,0) 
                                   << " signal:" << signal.at<uint16_t>(row,0) 
                                   << " diff:" << diff.at<uint16_t>(row,0);
          logwrite( "Archon::DeInterlace::nirc2_video", message.str() );
        }
#endif
**/

        return;
      }
      /***** Archon::DeInterlace::nirc2_video *********************************/


      /***** Archon::DeInterlace::nirc2 ***************************************/
      /**
       * @brief      NIRC2 deinterlacing
       * @details    This function is overloaded.
       *
       * ~~~
       *                                               Q1 +---------+---------+ Q2
       * +---------+---------+---------+---------+        | <------ | ------> |
       * |         |         |         |         |        |       ^ | ^       |
       * |   Q1    |   Q2    |   Q3    |   Q4    | ===>   +---------+---------+
       * |         |         |         |         |        |       v | v       |
       * +---------+---------+---------+---------+        | <------ | ------> |
       * :         :         :         :         :     Q3 +---------+---------+ Q4
       * :         :         :         :         :
       * (for multiple frames, this raw buf is   
       *  extended in this dimension for each frame)
       *                                         
       * ~~~
       *
       */
      void nirc2() {
        char message[256];
#ifdef LOGLEVEL_DEBUG
        SNPRINTF(message, "[DEBUG] this->rows=%d this->cols=%d this->frame_rows=%d this->frame_cols=%d",
                 this->rows, this->cols, this->frame_rows, this->frame_cols);
        logwrite( "Archon::DeInterlace::nirc2", std::string(message) );
#endif
        // Create openCV image to hold entire imbuf (all cubes)
        //
        cv::Mat image = cv::Mat( (this->rows * this->depth), this->cols, CV_16U, this->imbuf );

        // Create an empty openCV image for performing the deinterlacing work.
        // Note that this "work" Mat image is a single frame!
        // If this->imbuf is a datacube then "work" will only hold the last frame by the time
        // it gets back here. So if you wait until this->nirc2() returns, this work image
        // may not be what you want. Consider it a temporary workspace only.
        //
        cv::Mat work  = cv::Mat( this->frame_rows, this->frame_cols, CV_16U, cv::Scalar(0) );




        int workindex=0;
        SNPRINTF(message, "workindex=%d prior to calling nirc2(workindex, image, work)", workindex);
        logwrite( "Archon::DeInterlace::nirc2", std::string(message) );
        this->nirc2( workindex, image, work );  // this is where the actual deinterlacing takes place

        return;
      }
      /***** Archon::DeInterlace::nirc2 ***************************************/


      /***** Archon::DeInterlace::nirc2 ***************************************/
      /**
       * @brief      NIRC2 deinterlacing
       * @param[in]  workindex  reference to index counter (where in deinterlacing)
       * @param[in]  image      reference to cv::Mat array of raw image
       * @param[out] work       reference to cv::Mat array for performing deinterlacing
       * @details 
       *
       * This function is overloaded.
       * This format is used for processing NIRC2 video. The deinterlacing is the
       * same but it operates on a cv::Mat image and returns a deinterlaced cv::Mat image.
       *
       * Note that the "work" Mat image for deinterlacing is a single frame!
       * If the image buffer is a datacube then "work" will only hold the last frame.
       * So if you want to save a deinterlaced image then you might need to get it
       * before this function finishes. Consider "work" a temporary workspace only.
       *
       * ~~~
       *                                               Q2 +---------+---------+ Q1
       * +---------+---------+---------+---------+        | <------ | ------> |
       * |         |         |         |         |        |       ^ | ^       |
       * |   Q1    |   Q2    |   Q3    |   Q4    | ===>   +---------+---------+
       * |         |         |         |         |        |       v | v       |
       * +---------+---------+---------+---------+        | <------ | ------> |
       * :         :         :         :         :     Q4 +---------+---------+ Q3
       * :         :         :         :         :
       * (for multiple frames, this raw buf is   
       *  extended in this dimension for each frame)
       * ~~~
       *
       */
      void nirc2( int &workindex, cv::Mat &image, cv::Mat &work ) {
        std::stringstream message;
        const std::string function("Archon::DeInterlace::nirc2");
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] this->rows=" << this->rows << " this->cols=" << this->cols
                                 << " this->frame_rows=" << this->frame_rows << " this->frame_cols=" << this->frame_cols
                                 << " workindex=" << workindex;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif
message.str(""); message << "[DEBUG] incoming image=";
for (int i=0; i<10; i++) message << " " << image.ptr<uint16_t>()[i];
logwrite(function,message.str());

        int taps=8;

        int quad_rows  = (this->frame_rows / 2) + 4;
        int quad_cols  =  this->frame_cols / 2;

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] quad_rows=" << quad_rows << " quad_cols=" << quad_cols;
        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif

        // Define images for each quadrant
        //
        cv::Mat Q1, Q2, Q3, Q4;

        // Define images for the de-interlaced quadrants
        //
        cv::Mat Q1d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q2d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q3d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );
        cv::Mat Q4d = cv::Mat::zeros( quad_rows, quad_cols, CV_16U );

        // Define images for cropped quadrants
        //
        cv::Mat Q1c, Q2c, Q3c, Q4c;

        // Define images for the quadrants that need to be flipped
        //
        cv::Mat Q1f, Q3f, Q4f;

        // Loop through each cube,
        // cutting each from the main image buffer, deinterlacing and
        // performing needed flips before copying into the buffer that is passed to the FITS writer.
        //
        int slicen;
        try {
        for ( slicen=0; slicen < this->depth; slicen++ ) {
          // Cut the quadrants out of the image
          //
          Q1 = image( cv::Range( slicen * quad_rows, (slicen+1) * quad_rows), cv::Range( 0 * quad_cols, 1 * quad_cols ) );
          Q2 = image( cv::Range( slicen * quad_rows, (slicen+1) * quad_rows), cv::Range( 1 * quad_cols, 2 * quad_cols ) );
          Q3 = image( cv::Range( slicen * quad_rows, (slicen+1) * quad_rows), cv::Range( 2 * quad_cols, 3 * quad_cols ) );
          Q4 = image( cv::Range( slicen * quad_rows, (slicen+1) * quad_rows), cv::Range( 3 * quad_cols, 4 * quad_cols ) );

          // Perform the deinterlacing here
          //
          for ( int row=0, idx=0; row < quad_rows; row++ ) {
            for ( int tap=0; tap < taps; tap++ ) {
              for ( int col=0; col < quad_cols; col+=8 ) {
                int loc = row*quad_cols + col + tap;
                int c   = (int) (loc%quad_cols);
                int C   = (int) (idx%quad_cols);
                idx++;
                Q1d.at<uint16_t>(row,c) = Q1.at<uint16_t>(row,C);
                Q2d.at<uint16_t>(row,c) = Q2.at<uint16_t>(row,C);
                Q3d.at<uint16_t>(row,c) = Q3.at<uint16_t>(row,C);
                Q4d.at<uint16_t>(row,c) = Q4.at<uint16_t>(row,C);
              }
            }
          }

#ifdef LOGLEVEL_DEBUG
//        logwrite( "Archon::DeInterlace::nirc2", "[DEBUG] removing rows from quadrants" );
//        message.str(""); message << "[DEBUG] before: Q2d.rows=" << Q2d.rows << " Q4d.rows=" << Q4d.rows;
//        logwrite( "Archon::DeInterlace::nirc2", message.str() );
//
//        logwrite( "", "BEFORE CROP" );
//        for ( int r=0; r< 10; r++ ) {
//          message.str(""); message << "\t\t\t   Q1d.row " << r << ": " << Q1d.at<uint16_t>(r,0) << "\t\t Q3d.row " << r << ": " << Q3d.at<uint16_t>(r,0);
//          logwrite( "", message.str() );
//        }
//        for ( int r=10; r>0; r-- ) {
//          message.str(""); message << "\t\t\t   Q1d.row " << Q1d.rows-r << ": " << Q1d.at<uint16_t>(Q1d.rows-r,0)
//                                   << "\t\t Q3d.row " << Q3d.rows-r << ": " << Q3d.at<uint16_t>(Q3d.rows-r,0);
//          logwrite( "", message.str() );
//        }
#endif
          // Remove first 3 rows (0,1,2) from Q1, Q2
	  // by copying from row 3 (0-based) to the bottom.
	  // (first arg of rowRange is inclusive, second arg is exclusive)
          //
          Q1c = Q1d.rowRange( 3, Q1d.rows );
          Q2c = Q2d.rowRange( 3, Q2d.rows );

          // Copy one of the good rows up into the dead space,
          // row 3 to row 5 (0-based).
          //
          Q3d.row( 3 ).copyTo( Q3d.row( 5 ) );
          Q4d.row( 3 ).copyTo( Q4d.row( 5 ) );

#ifdef LOGLEVEL_DEBUG
//        logwrite( "", "AFTER COPY" );
//        for ( int r=0; r< 10; r++ ) {
//          message.str(""); message << "\t\t\t   Q1c.row " << r << ": " << Q1c.at<uint16_t>(r,0) << "\t\t Q3d.row " << r << ": " << Q3d.at<uint16_t>(r,0);
//          logwrite( "", message.str() );
//        }
//        for ( int r=10; r>0; r-- ) {
//          message.str(""); message << "\t\t\t   Q1c.row " << Q1c.rows-r << ": " << Q1c.at<uint16_t>(Q1c.rows-r,0)
//                                   << "\t\t Q3d.row " << Q3d.rows-r << ": " << Q3d.at<uint16_t>(Q3d.rows-r,0);
//          logwrite( "", message.str() );
//        }
#endif

          // then remove first 5 rows (0,1,2,3,4) from Q3, Q4 by copying from row 5 to the bottom.
          //
          Q3c = Q3d.rowRange( 5, Q3d.rows );
          Q4c = Q4d.rowRange( 5, Q4d.rows );

#ifdef LOGLEVEL_DEBUG
//        logwrite( "", "AFTER CROP" );
//        for ( int r=0; r< 10; r++ ) {
//          message.str(""); message << "\t\t\t   Q1c.row " << r << ": " << Q1c.at<uint16_t>(r,0) << "\t\t Q3c.row " << r << ": " << Q3c.at<uint16_t>(r,0);
//          logwrite( "", message.str() );
//        }
//        for ( int r=10; r>0; r-- ) {
//          message.str(""); message << "\t\t\t   Q1c.row " << Q1c.rows-r << ": " << Q1c.at<uint16_t>(Q1c.rows-r,0)
//                                   << "\t\t Q3c.row " << Q3c.rows-r << ": " << Q3c.at<uint16_t>(Q3c.rows-r,0);
//          logwrite( "", message.str() );
//        }
#endif

          // Perform the quadrant flips here (Q2c is not changed)
          //
          cv::flip( Q1c, Q1f,  1 );  // flip horizontally
          cv::flip( Q3c, Q3f, -1 );  // flip vertically and vertically
          cv::flip( Q4c, Q4f,  0 );  // flip horizontally

          {
          cv::Mat uppers;
          cv::Mat lowers;
          cv::hconcat( Q2c, Q1f, uppers );      // concatenate the two upper quadrants together, horizontally
          cv::hconcat( Q4f, Q3f, lowers );      // concatenate the two lower quadrants together, horizontally
          cv::vconcat( lowers, uppers, work );  // concatenate the uppers and lowers together, vertically
message.str(""); message << "[DEBUG] before subtracting from 65535 Q2c=";
for (int i=0; i<10; i++) message << " " << Q2c.ptr<uint16_t>()[i];
logwrite(function,message.str());
message.str(""); message << "[DEBUG] before subtracting from 65535 Q1f=";
for (int i=0; i<10; i++) message << " " << Q1f.ptr<uint16_t>()[i];
logwrite(function,message.str());
message.str(""); message << "[DEBUG] before subtracting from 65535 Q4f=";
for (int i=0; i<10; i++) message << " " << Q4f.ptr<uint16_t>()[i];
logwrite(function,message.str());
message.str(""); message << "[DEBUG] before subtracting from 65535 Q3f=";
for (int i=0; i<10; i++) message << " " << Q3f.ptr<uint16_t>()[i];
logwrite(function,message.str());
          }

          // Subtract the image from 65535 because for NIRC2 the counts decrease
          // with increasing signal.
          //
{
message.str(""); message << "[DEBUG] before subtracting from 65535 work=";
for (int i=0; i<10; i++) message << " " << work.ptr<uint16_t>()[i];
logwrite(function,message.str());
}
          cv::subtract( 65535, work, work );

          // Copy assembled image into the FITS buffer, this->workbuf
          //
#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG] copying " << this->frame_rows << " from work to fits buffer";
          logwrite( "Archon::DeInterlace::nirc2", message.str() );
          message.str(""); message << "[DEBUG] workbuf=" << std::hex << static_cast<void*>(this->workbuf) << " workindex=" << workindex;
          logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif
          for ( int row=0; row<this->frame_rows; row++ ) {
            for ( int col=0; col<this->frame_cols; col++ ) {
              *( this->workbuf + workindex++ ) = (uint16_t)(work.at<uint16_t>(row,col));
            }
          }
#ifdef LOGLEVEL_DEBUG
//        message.str(""); message << "[DEBUG] work.rows=" << work.rows << " work.cols=" << work.cols 
//                                 << " resetframe.rows=" << this->resetframe.rows
//                                 << " resetframe.cols=" << this->resetframe.cols
//                                 << " readframe.rows=" << this->readframe.rows << " readframe.cols=" << this->readframe.cols;
//        logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif

          // For CDS mode, copy the work buffer to the appropriate frame buffer
          //
          if ( this->iscds && this->nmcds==0 && slicen==0 ) work.copyTo( this->resetframe );  // this is the reset frame
          if ( this->iscds && this->nmcds==0 && slicen==1 ) work.copyTo( this->readframe  );  // this is the read frame

          // For MCDS mode, copy the work buffer to the appropriate frame buffer
          //
          if ( this->nmcds > 0 ) {
            // For the first half of num MCDS point to buffer0, second half point to buffer1
            //
            int32_t* ptr = ( (slicen < this->nmcds/2) ? this->mcdsbuf_0 : this->mcdsbuf_1 );
#ifdef LOGLEVEL_DEBUG
//          message.str(""); message << "[DEBUG] " << ( (slicen < this->nmcds/2) ? "first" : "second" ) << " half of MCDS";
//          logwrite( "Archon::DeInterlace::nirc2", message.str() );
#endif
            // Create openCV image from the coadd buffer pointed above
            // and add the work buffer to it.
            //
            cv::Mat coadd = cv::Mat( this->frame_rows, this->frame_cols, CV_32S, ptr );
            cv::add( coadd, work, coadd, cv::noArray(), coadd.type() );
          }

        } // end of loop over cubes
        }
        catch ( const std::exception &ex ) {
          message.str(""); message << "ERROR deinterlacing slicen=" << slicen << " nmcds=" << this->nmcds << ": " << ex.what();
          logwrite( function, message.str() );
          return;
        }
        catch ( ... ) {
          logwrite( function, "unknown exception in deinterlacing" );
          return;
        }

        if ( !this->iscds ) return;

        // For CDS there will be two frames available here, the readframe and the resetframe.
        // Subtract the reset from the read frame (first add an offset to the readframe,
        // so that it can't be negative). Then add that result to the coadd buffer.

        if ( this->cdsbuf == NULL || this->coaddbuf == NULL ) {
          logwrite( "Archon::DeInterlace::nirc2", "ERROR: no memory allocated for cds buffers" );
          return;
        }

        // Perform the CDS subtraction, read-reset into a 32b type Mat array,
        // which will take care of any cases where reset > read.
        //
        cv::Mat diff;
        try {
          cv::subtract( this->readframe, this->resetframe, diff, cv::noArray(), CV_32S );
        }
        catch ( const std::exception& ex ) {
          message.str(""); message << "ERROR subtracting readframe-resetframe: " << ex.what();
          logwrite( function, message.str() );
          message.str(""); message << "ERROR readframe.rows=" << this->readframe.rows << " .cols=" << this->readframe.cols
                                   << " resetframe.rows=" << this->resetframe.rows << " .cols=" << this->resetframe.cols
                                   << " iscds=" << this->iscds << " nmcds=" << this->nmcds << " workbuf=" << std::hex << this->workbuf;
          logwrite( function, message.str() );
          return;
        }
        catch ( ... ) {
          logwrite( function, "unknown exception subtracting readframe-resetframe" );
          return;
        }

        // Create openCV image from the coadd buffer
        //
        cv::Mat coadd = cv::Mat( this->frame_rows, this->frame_cols, CV_32S, this->coaddbuf );

#ifdef LOGLEVEL_DEBUG
        double minVal, maxVal;
        int minIdx, maxIdx;
        cv::minMaxIdx( coadd, &minVal, &maxVal, &minIdx, &maxIdx, cv::noArray() );
        message.str(""); message << "[DEBUG] coadd minVal=" << minVal << " maxVal=" << maxVal;
        logwrite( function, message.str() );
#endif

        // Perform the coadd, this current pair plus whatever is already there.
        // This form of the cv::add() function will properly convert the type.
        //
        try {
          if ( mat_add_will_overflow<int32_t>( coadd, diff ) ) {
            logwrite( function, "ERROR coadd would overflow datatype" );
            return;
          }
          cv::add( coadd, diff, coadd, cv::noArray(), coadd.type() );
        }
        catch ( const std::exception& ex ) {
          message.str(""); message << "ERROR coadding CDS frame: " << ex.what();
          logwrite( function, message.str() );
          return;
        }
        catch ( ... ) {
          logwrite( function, "unknown exception coadding CDS frame" );
          return;
        }

        // Copy assembled image into the FITS buffer, this->cdsbuf
        //
        unsigned long index=0;
        for ( int row=0; row<this->frame_rows; row++ ) {
          for ( int col=0; col<this->frame_cols; col++ ) {
            *( this->cdsbuf   + index ) = static_cast<int32_t>(diff.at<int32_t>(row,col));
            *( this->coaddbuf + index ) = static_cast<int32_t>(coadd.at<int32_t>(row,col));
            index++;
          }
        }

        return;
      }
      /***** Archon::DeInterlace::nirc2 ***************************************/


      /***** Archon::DeInterlace::none ****************************************/
      /**
       * @brief      no deinterlacing -- copy imbuf to workbuf
       * @details 
       *
       * ~~~
       *    +-------------------+
       *    |                   |
       *    |                   |
       *    |         0         |
       *    | <---------------- |
       * L1 +-------------------+
       * ~~~
       *
       */
      void none( ) {
        const std::string function("Archon::DeInterlace::none");
        std::stringstream message;

        int bufrows = this->rows * this->depth;

        for ( int r=0, index=0; r<bufrows; r++ ) {
          for ( int c=0; c<this->cols; c++ ) {
            int loc = (r * this->cols) + c ;
            *( this->workbuf + loc ) = *( this->imbuf + (index++) );
/***
            // Store some special pixels for debugging
            //
            if ( loc == (2048*511) +    0          ) *( this->workbuf + loc ) = 20111;
            if ( loc == (2048*511) +  512          ) *( this->workbuf + loc ) = 20222;
            if ( loc == (2048*511) + 1024          ) *( this->workbuf + loc ) = 20333;
            if ( loc == (2048*511) + 1536          ) *( this->workbuf + loc ) = 20444;
            if ( loc == (2048*511) +    0 +2048*512) *( this->workbuf + loc ) = 30111;
            if ( loc == (2048*511) +  512 +2048*512) *( this->workbuf + loc ) = 30222;
            if ( loc == (2048*511) + 1024 +2048*512) *( this->workbuf + loc ) = 30333;
            if ( loc == (2048*511) + 1536 +2048*512) *( this->workbuf + loc ) = 30444;
***/
          }
        }
        return;
      }
      /***** Archon::DeInterlace::none ****************************************/


    public:

      /***** Archon::DeInterlace::DeInterlace *********************************/
      /**
       * @brief      class constructor
       * @param[in]  imbuf_in      T* pointer to image buffer (this will be image_data)
       * @param[in]  workbuf_in    T* pointer to work buffer (this will be where the deinterlacing is performed)
       * @param[in]  cols          
       * @param[in]  rows          
       * @param[in]  readout_type  
       *
       */
      DeInterlace( T* _imbuf, T* _workbuf, T* _cdsbuf, int32_t* _coaddbuf, int32_t* _mcdsbuf_0, int32_t* _mcdsbuf_1,
                   bool _iscds, int _nmcds, int _cols, int _rows, int _readout_type, int _height, int _width, int _depth ) {
        this->imbuf = _imbuf;
        this->workbuf = _workbuf;
        this->cdsbuf = _cdsbuf;
        this->coaddbuf = _coaddbuf;
        this->mcdsbuf_0 = _mcdsbuf_0;
        this->mcdsbuf_1 = _mcdsbuf_1;
        this->iscds = _iscds;
        this->nmcds = _nmcds;
        this->cols = _cols;
        this->rows = _rows;
        this->readout_type = _readout_type;
        this->frame_rows = _height;
        this->frame_cols = _width;
        this->depth = _depth;
      }
      /***** Archon::DeInterlace::DeInterlace *********************************/


      ~DeInterlace() {
        this->resetframe.release();
        this->readframe.release();
      }


      /***** Archon::DeInterlace::info ****************************************/
      /**
       * @brief returns some info, just for debugging
       *
       */
      std::string info() {
        std::stringstream ret;
        ret << " imbuf=" << std::hex << this->imbuf << " this->workbuf=" << std::hex << this->workbuf
            << " cols=" << this->cols << " rows=" << this->rows
            << " readout_type=" << this->readout_type;
        return ( ret.str() );
      }
      /***** Archon::DeInterlace::info ****************************************/


      /***** Archon::DeInterlace::do_deinterlace ******************************/
      /**
       * @brief
       *
       * This calls the appropriate deinterlacing function based on the readout
       * type, which is an enum that was given to the class constructor when
       * this DeInterlace object was constructed.
       *
       */
      void do_deinterlace() {

        const std::string function("Archon::DeInterlace::do_deinterlace");
        std::stringstream message;
  
        message.str(""); message << "[DEBUG] workbuf=" << std::hex << static_cast<void*>(this->workbuf)
                                 << " cdsbuf=" << std::hex << static_cast<void*>(this->cdsbuf)
                                 << " imbuf=" << std::hex << static_cast<void*>(this->imbuf);
        logwrite( "Archon::DeInterlace::do_deinterlace", message.str() );


        switch( this->readout_type ) {
          case Archon::READOUT_NONE:
            this->none();
            break;
          case Archon::READOUT_NIRC2:
            this->resetframe = cv::Mat::zeros( this->frame_rows, this->frame_cols, CV_16U );
            this->readframe  = cv::Mat::zeros( this->frame_rows, this->frame_cols, CV_16U );
            this->nirc2();
            this->resetframe.release();
            this->readframe.release();
            break;
          case Archon::READOUT_NIRC2VIDEO:
            this->nirc2_video();
            break;
          case Archon::READOUT_TEST:
            this->test();
            break;
          default:
            logwrite( "Archon::DeInterlace::do_deinterlace",
                      "ERROR unknown readout type: " +std::to_string(this->readout_type) );
        }

        return;
      }
      /***** Archon::DeInterlace::do_deinterlace ******************************/


  };
  /***** Archon::DeInterlace **************************************************/


  /** @brief    holds one or more frames and metadata from the Archon
   *  @details  A single ImageBuffer object can contain multiple frames,
   *            or slices, as would be the case for a datacube.
   */
  struct ImageBuffer {
    int ncoadd;
    int n_slices;                              // number of slices in this image
    std::vector<int> bufframen_slice;          // Archon frame number(s) for all slices in this image
    std::vector<uint64_t> buftimestamp_slice;  // Archon timestamp(s) for all slices in this image
    std::shared_ptr<char[]> rawpixels;         // Archon frame buffer(s)
  };


  template<typename T>
  struct ProcessingBuffers {
    std::shared_ptr<T[]> workbuf;
    std::shared_ptr<T[]> cdsbuf;

    ProcessingBuffers(size_t work_section_size, size_t cds_section_size) {
      workbuf = std::shared_ptr<T[]>(new T[work_section_size]());
      cdsbuf  = std::shared_ptr<T[]>(new T[cds_section_size]());

      if (!workbuf || !cdsbuf) throw std::bad_alloc();
    }
  };

  /***** Archon::Interface ****************************************************/
  /**
   * @class  Interface
   * @brief  This class defines the interface to the Archon controller
   *
   */
  class Interface {
    private:
      uint64_t start_timer, finish_timer;  //!< Archon internal timer, start and end of exposure
      uint64_t last_frame_timer;           //!< Archon timer of last frame
      int n_hdrshift;                      //!< number of right-shift bits for Archon buffer in HDR mode
      struct timespec cal_systime;
      uint64_t cal_archontime;

      std::unique_ptr<ProcessingBuffers<uint16_t>> buffers_16;
      std::unique_ptr<ProcessingBuffers<uint32_t>> buffers_32;
      std::unique_ptr<ProcessingBuffers<int16_t>>  buffers_16s;

      /** @brief FIFO queue to contain images from Archon */
      std::queue<std::shared_ptr<ImageBuffer>> imagebuf_queue;  ///< the queue itself
      std::mutex queue_mutex;                                   ///< mutex protects access to the queue
      std::condition_variable queue_cv;

      std::atomic<bool> is_producer_finished;
      std::atomic<bool> is_producer_error;

      long initialize_processing_buffers();
      void image_acquisition_loop(int nseq);
      void image_processing_loop();
      void process_image(std::shared_ptr<ImageBuffer> &imagebuf);
      template<typename T> void deinterlace_queue(std::shared_ptr<ImageBuffer> imagebuf, ProcessingBuffers<T> &buffers);
      void runcds();
      void make_simulated_data(char* buffer, uint16_t extra);

      // Add any pre-exposures onto the requested number of sequences,
      // using 1 if not supplied.
      //
      int parse_sequence_count(const std::string &nseq_in) {
        if (nseq_in.empty()) {
          return 1 + this->camera_info.num_pre_exposures;
        }
        try {
          int nseq = std::stoi(nseq_in);
          if ( nseq <= 0) return -1;
          else return nseq + this->camera_info.num_pre_exposures;
        }
        catch (const std::exception&) { return -1; }
      }

    public:
      Interface();
      ~Interface();

      // Class Objects
      //
      Network::TcpSocket archon;             /// this is how we talk to the Archon
      Camera::Information camera_info;       /// this is the main camera_info object
      Camera::Information cds_info;          /// this is the main camera_info object
//    Camera::Information fits_info;         /// used to copy the camera_info object to preserve info for FITS writing  //TODO @todo obsolete?
      Camera::Camera camera;                 /// instantiate a Camera object
      Common::FitsKeys userkeys;             /// instantiate a FitsKeys object for user-defined keywords
      Common::FitsKeys systemkeys;           /// instantiate a FitsKeys object for system-defined keywords
      Common::FitsKeys extkeys;              /// instantiate a FitsKeys object for extension-only keywords

      Config config;

//    xxxx_file xfits_file;                  //!< instantiate a FITS container object *** old method to be removed
//    xxxx_file xcds_file;                   //!< instantiate a FITS container object *** old method to be removed

      std::unique_ptr<FITS_file<uint16_t>> __fits_file; /*** for new FITS engine ***/
      std::unique_ptr<FITS_file<int32_t>>  __file_cds;  /*** for new FITS engine ***/

      typedef struct {
        int readout_type;                    //!< enum for readout type
        uint32_t    readout_arg;             //!< future use?
      } readout_info_t;

      std::map< std::string, readout_info_t > readout_source;  //!< STL map of readout sources indexed by readout name

      std::atomic<int> deinterlace_count;     /// number of times deinterlace has been called when mex=true
      std::atomic<int> write_frame_count;     /// number of times write_frame() has been called when mex=true

      std::mutex deinter_mtx;                 /// deinterlacing mutex
      std::condition_variable deinter_cv;     /// deinterlacing condition variable

      int  msgref;                           //!< Archon message reference identifier, matches reply to command
      int  taplines;
      std::vector<int> gain;                 //!< digital CDS gain (from TAPLINE definition)
      std::vector<int> offset;               //!< digital CDS offset (from TAPLINE definition)
      bool modeselected;                     //!< true if a valid mode has been selected, false otherwise
      bool firmwareloaded;                   //!< true if firmware is loaded, false otherwise
      bool is_longexposure;                  //!< true for long exposure mode (exptime in sec), false for exptime in msec
      uint32_t readout_arg;

      bool lastmexamps;

      bool write_tapinfo_to_fits;            //!< set to write tapinfo (gain, offset) to FITS headers

      float heater_target_min;               //!< minimum heater target temperature
      float heater_target_max;               //!< maximum heater target temperature

      char *image_data;                      //!< image data buffer //TODO @todo obsolete?

      std::unique_ptr<cv::Mat> coadd_img;    ///< persistent coadd image
      std::unique_ptr<cv::Mat> diff_img;     ///< persistent diff image

      int32_t* coaddbuf;                     /// final coadd buffer written to FITS
      int32_t* mcdsbuf_0;                    /// first group of MCDS coadds (baseline)
      int32_t* mcdsbuf_1;                    /// second group of MCDS coadds (signal)

      void *workbuf;                         //!< pointer to workspace for performing deinterlacing //TODO @todo obsolete?
      long workbuf_size;
      long cdsbuf_size;
      uint32_t image_data_bytes;             //!< requested number of bytes allocated for image_data rounded up to block size
      uint32_t image_data_allocated;         //!< allocated number of bytes for image_data

      std::atomic<bool> openfits_error;      //!< indicates the openfits thread had an error (or not)
      std::atomic_flag archon_busy = ATOMIC_FLAG_INIT;  //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads
      std::string exposeparam;               //!< param name to trigger exposure when set =1
      std::string abortparam;                //!< param name to trigger an abort when set =1
      std::string mcdspairs_param;           //!< param name to set MCDS samples
      std::string mcdsmode_param;            //!< param name to set MCDS mode
      std::string rxmode_param;              //!< param name to set RX mode (read-reset video)
      std::string rxrmode_param;             //!< param name to set RXR mode (read-reset-read video)
      std::string videosamples_param;        //!< param name to set video samples for RX, RXR modes
      std::string utrmode_param;             //!< param name to set UTR mode
      std::string utrsamples_param;          //!< param name to set UTR samples

      std::string shutenableparam;           //!< param name to enable shutter open on expose
      int shutenable_enable;                 //!< the value which enables shutter enable
      int shutenable_disable;                //!< the value which disables shutter enable

      // Functions
      //
      long abort();                          ///  abort an exposure
      long abort_archon();                   ///  set abort parameter(s) on Archon
      long caltimer();                       //!< calibrate Archon timer
      long interface(std::string &iface);    //!< get interface type
      long configure_controller();           //!< get configuration parameters
      long prepare_ring_buffer();            //!< prepare image_data, allocating memory as needed
      long connect_controller(std::string devices_in);  //!< open connection to archon controller
      long disconnect_controller();          //!< disconnect from archon controller
      long load_timing(std::string acffile); //!< load specified ACF then LOADTIMING
      long load_timing(std::string acffile, std::string &retstring);
      long load_firmware(std::string acffile); //!< load specified acf then APPLYALL
      long load_firmware(std::string acffile, std::string &retstring);
      long load_acf( std::string acffile, bool write_to_archon );    //!< only load (WCONFIG) the specified ACF file
      long set_camera_mode(std::string mode_in);
      long load_mode_settings(std::string mode);
      long native(std::string cmd);
      long archon_cmd(std::string cmd);
      long archon_cmd(std::string cmd, std::string &reply);
      long read_parameter(std::string paramname, std::string &valstring);
      long prep_parameter(std::string paramname, std::string value);
      long load_parameter(std::string paramname, std::string value);
      long fetchlog();
      long get_frame_status();
      void print_frame_status();
      long lock_buffer(int buffer);
      long get_timer(unsigned long int *timer);
      long fetch(uint64_t bufaddr, uint32_t bufblocks);
      long read_frame_cache(char* buffer, int slice);
      long read_frame( Camera::frame_type_t frame_type, char* &ptr_in );
      long write_config_key( const char *key, const char *newvalue, bool &changed );
      long write_config_key( const char *key, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue, bool &changed );
      long write_parameter( const char *paramname, int newvalue, bool &changed );
      long write_parameter( const char *paramname, const char *newvalue );
      long write_parameter( const char *paramname, int newvalue );
      template <class T> long get_configmap_value(std::string key_in, T& value_out);
      void add_filename_key();
      void add_filename_key( Camera::Information &info );  /// adds the current filename to the systemkeys database
      long get_status_key( std::string key, std::string &value );     /// get value for indicated key from STATUS string
      long temp( std::string &retstring );                            /// get Archon backplane temp
      long fan( std::string &retstring );                             /// get Archon fan tach
      long overheat( std::string &retstring );                        /// get Archon overheat state
      long tempinfo( std::string &retstring );                        /// get Archon temperature info string (all three)
      long power( std::string state_in, std::string &retstring );     /// wrapper for do_power
      long do_power( std::string state_in, std::string &retstring );  /// set/get Archon power state
      long expose( std::string nseq_in );
      long do_expose(std::string nseq_in);
      long wait_for_exposure();
      long wait_for_readout();
      long get_parameter(std::string parameter, std::string &retstring);
      long get_parammap_value( std::string param_in, long& value_out );
      long set_parameter( std::string parameter, long value );
      long set_parameter(std::string parameter);
      long exptime(std::string exptime_in, std::string &retstring);
      long exptime(std::string exptime_in);
      long exptime(int32_t exptime_in);
      void copy_keydb();                                                /// copy user keyword database into camera_info
      long longexposure(std::string state_in, std::string &state_out);
      long shutter(std::string shutter_in, std::string& shutter_out);
      long hdrshift(std::string bits_in, std::string &bits_out);
      long heater(std::string args, std::string &retstring);
      long sensor(std::string args, std::string &retstring);
      long bias(std::string args, std::string &retstring);
      long cds(std::string args, std::string &retstring);
      long inreg( std::string args );
      void make_camera_header();
      long recalc_geometry();
      long region_of_interest( std::string args );
      long region_of_interest( std::string args, std::string &retstring );
      long calc_readouttime();
      long sample_mode( std::string args );
      long sample_mode( std::string args, std::string &retstring );
      long readout( std::string readout_in, std::string &readout_out );
      long test(std::string args, std::string &retstring);

      static void dothread_openfits( Interface *self );                             /// this is run in a thread to open a fits file for flat (non-mex) files only

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int  amps[2];         // number of amplifiers per detector for each axis, set in set_camera_mode
        int  num_detect;      // number of detectors, set in set_camera_mode
        int  linecount;       // number of lines per tap
        int  pixelcount;      // number of pixels per tap
        int  framemode;       // Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
      };

      /**
       * @var     struct tapinfo_t tapinfo[]
       * @details structure of tapinfo which is unique to each observing mode
       */
      struct tapinfo_t {
        int   num_taps;
        int   tap[16];
        float gain[16];
        float offset[16];
        std::string readoutdir[16];
      };

      /**
       * @var     struct frame_data_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frame_data_t {
        int      index;                       // index of newest buffer data
        int      frame;                       // frame of newest buffer data
        int      next_index;                  // index of next buffer
        std::string timer;                    // current hex 64 bit internal timer
        int      rbuf;                        // current buffer locked for reading
        int      wbuf;                        // current buffer locked for writing
        std::vector<int>      bufsample;      // sample mode 0=16 bit, 1=32 bit
        std::vector<int>      bufcomplete;    // buffer complete, 1=ready to read
        std::vector<int>      bufmode;        // buffer mode: 0=top 1=bottom 2=split
        std::vector<uint64_t> bufbase;        // buffer base address for fetching
        std::vector<int>      bufframen;      // buffer frame number
        std::vector<int>      bufwidth;       // buffer width
        std::vector<int>      bufheight;      // buffer height
        std::vector<int>      bufpixels;      // buffer pixel progress
        std::vector<int>      buflines;       // buffer line progress
        std::vector<int>      bufrawblocks;   // buffer raw blocks per line
        std::vector<int>      bufrawlines;    // buffer raw lines
        std::vector<int>      bufrawoffset;   // buffer raw offset
        std::vector<uint64_t> buftimestamp;   // buffer hex 64 bit timestamp
        std::vector<uint64_t> bufretimestamp; // buf trigger rising edge time stamp
        std::vector<uint64_t> buffetimestamp; // buf trigger falling edge time stamp
      } frame;

      /** @var      vector modtype
       *  @details  stores the type of each module from the SYSTEM command
       */
      std::vector<int> modtype;

      /** @var      vector modversion
       *  @details  stores the version of each module from the SYSTEM command
       */
      std::vector<std::string> modversion;

      std::string backplaneversion;

      /** @var      int lastframe
       *  @details  the last (I.E. previous) frame number acquired
       */
      int lastframe;

      /**
       * rawinfo_t is a struct which contains variables specific to raw data functions
       */
      struct rawinfo_t {
        int adchan;          // selected A/D channels
        uint16_t rawsamples; // number of raw samples per line
        uint16_t rawlines;   // number of raw lines
        int iteration;       // iteration number
        int iterations;      // number of iterations
      } rawinfo;

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        int    line;           // the line number, used for updating Archon
        std::string value;     // used for configmap
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        std::string key;       // the PARAMETERn part
        std::string name;      // the parametername part
        std::string value;     // the value part
        int    line;           // the line number, used for updating Archon
      } param_line_t;

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;

      cfg_map_t   configmap;
      param_map_t parammap;

      /** 
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      typedef struct {
        int         rawenable;     //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t   configmap;     //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;      //!< PARAMETERn=parametername=value map for mode sections
        Common::FitsKeys acfkeys;  //!< create a FitsKeys object to hold user keys read from ACF file for each mode
        geometry_t  geometry;
        tapinfo_t   tapinfo;
      } modeinfo_t;

      std::map<std::string, modeinfo_t> modemap;

      /**
       * generic key=value STL map for Archon commands
       */
      typedef std::map<std::string, std::string> map_t;

      /**
       * \var     map_t systemmap
       * \details key=value map for Archon SYSTEM command
       */
      map_t systemmap;

      /**
       * \var     map_t statusmap
       * \details key=value map for Archon STATUS command
       */
      map_t statusmap;

  };
  /***** Archon::Interface ****************************************************/


}
#endif
