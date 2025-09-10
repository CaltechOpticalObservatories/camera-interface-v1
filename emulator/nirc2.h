/**
 * @file    nirc.h
 * @brief   header for Nirc2Image inherited class
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once
#include "image_info_base.h"

namespace Archon {

  /***** Archon::Nirc2Image ***************************************************/
  /**
   * @brief      derived class inherits from ImageInfoBase
   * @details    This derived class provides the specialization of the base
   *             class for the NIRC2 instrument. This contains specific
   *             implementations for NIRC2.
   *
   */
  class Nirc2Image : public ImageInfoBase {
    private:
      double frame_start_time;        //!< FRAME_START_TIME
    public:
      double pixel_time;              //!< PIXEL_TIME
      double pixel_skip_time;         //!< PIXEL_SKIP_TIME
      double row_overhead_time;       //!< ROW_OVERHEAD_TIME
      double row_skip_time;           //!< ROW_SKIP_TIME
      double fs_pulse_time;           //!< FS_PULSE_TIME
      bool iscds;                     //!< is this a CDS exposure? (NIRC2)
      int utr_samples;                //!< number of UTR samples (NIRC2)
      int mcds_samples;               //!< number of MCDS samples (NIRC2)
      int numsamples;                 //!< number of samples, larger of utr,mcds (NIRC2)

      Nirc2Image() : frame_start_time(-1), pixel_time(-1), pixel_skip_time(-1),
                     row_overhead_time(-1), row_skip_time(-1), fs_pulse_time(-1),
                     iscds(false),
                     utr_samples(1),
                     mcds_samples(1),
                     numsamples(1) { }


      std::string sample_info() const override {
        std::stringstream retstring;
        retstring << " mcds_samples=" << this->mcds_samples
                  << " utr_samples=" << this->utr_samples
                  << " numsamples=" << this->numsamples;
        return retstring.str();
      }

      /***** Nirc2Image::calc_rowtime *****************************************/
      /**
       * @brief      returns the calculated row time for NIRC2
       * @return     rowtime in usec
       *
       */
      virtual double calc_rowtime() override {
        double rowtime = ( this->imwidth/32. ) * this->pixel_time +
                         ( 1024/32. - this->imwidth/32. ) * this->pixel_skip_time + this->row_overhead_time;
        return rowtime;
      }

      /***** Nirc2Image::calc_rowtime *****************************************/


      /***** Nirc2Image::handle_key *******************************************/
      /**
       * @brief      set class variables using input key, value
       * @param[in]  key    keyword
       * @param[in]  value  integer value
       * @return     ERROR | NO_ERROR
       *
       */
      virtual long handle_key( const std::string &key, const int value ) override {
        std::string function = " (Archon::Nirc2Image::handle_key) ";
        if ( ImageInfoBase::handle_key( key, value ) == NO_ERROR ) {  // key handled by base class
          return NO_ERROR;
        }
        else if ( key == "mode_MCDS" ) {
          this->iscds = ( value == 1 ? true : false );
          std::cout << get_timestamp() << function << "CDS mode " << ( this->iscds ? "en" : "dis" ) << "abled\n";
        }
        else if ( key == "UTR_sample" ) {
          this->utr_samples = ( value == 0 ? 1 : value );
          this->numsamples = std::max( this->mcds_samples, this->utr_samples );
        }
        else if ( key == "MCDS_sample" ) {
          this->mcds_samples = ( value == 0 ? 1 : value );
          this->numsamples = std::max( this->mcds_samples, this->utr_samples );
        }
        else {
          std::cout << get_timestamp() << function << "ERROR unknown key: " << key << "\n";
          return ERROR;
        }
        return NO_ERROR;
      }
      /***** Nirc2Image::handle_key *******************************************/


      /***** Nirc2Image::set_config_parameter *********************************/
      /**
       * @brief      
       * @param[in]  key    keyword
       * @param[in]  value  value
       *
       */
      void set_config_parameter( const std::string &key, const std::string &val ) {
        ImageInfoBase::set_config_parameter( key, val ); // Call base class method for common parameters
        if ( key == "PIXEL_TIME" )        { this->pixel_time = std::stod( val );        }
        else
        if ( key == "PIXEL_SKIP_TIME" )   { this->pixel_skip_time = std::stod( val );   }
        else
        if ( key == "ROW_OVERHEAD_TIME" ) { this->row_overhead_time = std::stod( val ); }
        else
        if ( key == "ROW_SKIP_TIME" )     { this->row_skip_time = std::stod( val );     }
        else
        if ( key == "FRAME_START_TIME" )  { this->frame_start_time = std::stod( val );  }
        else
        if ( key == "FS_PULSE_TIME" )     { this->fs_pulse_time = std::stod( val );     }
        return;
      }
      /***** Nirc2Image::set_config_parameter *********************************/


      /***** Nirc2Image::get_readouttime **************************************/
      /**
       * @brief      calculate readout time for NIRC2
       * @return     readout time in msec
       *
       */
      virtual int get_readouttime() override {
        std::string function = " (Archon::Nirc2Image::get_readouttime) ";
        double frame_ohead = frame_start_time + fs_pulse_time;
        double row_ohead   = row_overhead_time;
        double pixeltime   = pixel_time;
        double pixelskip   = pixel_skip_time;
        double rowskip     = row_skip_time;
        int cols           = imwidth;
        int rows           = imheight;
        double rowtime     = (cols/32.) * pixeltime + ( 1024/32. - cols/32. ) * pixelskip + row_ohead;

        readouttime  = std::lround( ( frame_ohead + (4 + rows/2.)*rowtime + rowskip * ( 516 - rows/2 - 4 ) ) / 1000. );
        std::cout << get_timestamp() << function << "readouttime = " << readouttime << "\n";

        return readouttime;
      }
      /***** Nirc2Image::get_readouttime **************************************/

      inline int get_frames_per_exposure() { return numsamples * ( iscds ? 2 : 1 ); }
  };
  /***** Archon::Nirc2Image ***************************************************/
}
