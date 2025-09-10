/**
 * @file    image_info_base.h
 * @brief   header for ImageInfoBase class
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include "emulator-archon.h"

namespace Archon {

  /***** Archon::ImageInfoBase ************************************************/
  /**
   * @brief      base class for image information objects
   * @details    This is an abstract base class that defines the common
   *             interface and properties for different types of image
   *             information objects. It may contain shared data members and
   *             default implementations for certain functions, but primarily
   *             serves as a polymorphic interface to specializations of image
   *             information for particular instrument types
   *
   */
  class ImageInfoBase {
    private:
    public:
      uint32_t framen;
      int  activebufs;                //!< number of active frame buffers
      int  taplines;                  //!< from "TAPLINES=" in ACF file
      int  linecount;                 //!< from "LINECOUNT=" in ACF file
      int  pixelcount;                //!< from "PIXELCOUNT=" in ACF file
      int  readtime;                  //!< from "READOUT_TIME=" in configuration file
      int  readouttime;               //!< 
      int  imwidth;                   //!< 
      int  imheight;                  //!< 
      int  exptime;                   //!< requested exposure time in msec from WCONFIG
      int  exposure_factor;           //!< multiplier for exptime relative to 1 sec (=1 for sec, =1000 for msec, etc.)

      ImageInfoBase() : framen(0), activebufs(-1), taplines(-1),
                        linecount(-1), pixelcount(-1),
                        readtime(-1), readouttime(0),
                        imwidth(0), imheight(0),
                        exptime(0), exposure_factor(1000) { }

      virtual ~ImageInfoBase() = default;

      // These are pure virtual functions which must be overridden
      // in the derived classes.
      //
      virtual double calc_rowtime() = 0;
      virtual int get_readouttime() = 0;

      virtual std::string sample_info() const { return ""; };

      virtual long handle_key( const std::string &key, const int value ) {
        std::string function = " (Archon::ImageInfoBase::handle_key) ";
        if ( key == "nPixelsPair" ) {
          this->imwidth = 32 * value;
        }
        else if ( key == "nRowsQuad" ) {
          this->imheight = 8 * value;
        }
        else if ( key == "exptime" ) {
          int readouttime = get_readouttime();
          this->exptime = std::max( value, readouttime );
          std::cout << get_timestamp() << function << "exptime = " << this->exptime
                                                   << ( this->exposure_factor == 1 ? " sec" : " msec" ) << "\n";
        }
        else if ( key == "longexposure" ) {
          // exposure_factor=1000 when longexposure=0 (msec)
          // exposure_factor=1    when longexposure=1 (sec)
          this->exposure_factor = ( value == 1 ? 1 : 1000 );
          std::cout << get_timestamp() << function << "exptime = " << this->exptime
                                                   << ( this->exposure_factor == 1 ? " sec" : " msec" ) << "\n";
        }
        else {
          return ERROR;
        }
        return NO_ERROR;
      }

      virtual void set_config_parameter( const std::string& key, const std::string& val ) {
        if (key == "ACTIVE_BUFS")     { this->activebufs = std::stoi(val);      }
        else
        if (key == "TAPLINES")        { this->taplines = std::stoi(val);        }
        else
        if (key == "LINECOUNT")       { this->linecount = std::stoi(val);       }
        else
        if (key == "PIXELCOUNT")      { this->pixelcount = std::stoi(val);      }
        else
        if (key == "READOUT_TIME")    { this->readtime = std::stoi(val);        }
        else
        if (key == "IMWIDTH")         { this->imwidth = std::stoi(val);         }
        else
        if (key == "IMHEIGHT")        { this->imheight = std::stoi(val);        }
        else
        if (key == "EXPTIME")         { this->exptime = std::stoi(val);         }
        else
        if (key == "EXPOSURE_FACTOR") { this->exposure_factor = std::stoi(val); }
        return;
      }

      virtual int get_frames_per_exposure() const { return 1; }

      virtual double get_pixel_time() const {
        throw std::runtime_error("pixel_time not available in this class");
      }
  };
  /***** Archon::ImageInfoBase ************************************************/
}
