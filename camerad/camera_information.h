
#pragma once

#include <CCfits/CCfits>
#include <vector>
#include <atomic>
#include "common.h"
#include "logentry.h"

namespace Camera {

  /***** Camera::ExposureTime *************************************************/
  /**
   * @class    ExposureTime
   * @brief    base class for exposure time handling
   * @details  Exposure time is always represented in seconds.
   *
   */
  class ExposureTime {
    protected:
      double exptime_sec;
    public:
      ExposureTime() : exptime_sec(0) { }

      virtual ~ExposureTime()=default;
      virtual void set(double value) { exptime_sec=value; }
      virtual double get() const { return exptime_sec; }
  };
  /***** Camera::ExposureTime *************************************************/


  /***** Camera::Information **************************************************/
  /**
   * @class    Information
   *
   */
  class Information {
    friend class CameraInterface;
    public:
      Information()
        : nexp(1)
      {
/** why does Information need exposure_time? why can't it stay in Controller?
 *      this->exposure_time = std::make_unique<ExposureTime>();
 **/
      }
//    std::unique_ptr<ExposureTime> exposure_time;
      Information(const Information&) = delete;
      Information& operator=(const Information&) = delete;

      Common::FitsKeys userkeys;            //!< FITS keys for command line use
      Common::FitsKeys systemkeys;          //!< FITS keys for system use

      std::vector<uint32_t> naxes;          //!< axis lengths element 0=cols, 1=rows, 2=cubedepth
      std::vector<uint32_t> binning;
      std::vector<uint32_t> region_of_interest;
      std::vector<uint32_t> detector_pixels;

      uint64_t section_size;                //!< pixels to write this section (accounts for depth)
      uint64_t image_memory;                //!< bytes per image sensor

      /** @var     bitpix
       *  @brief   FITS datatype (not literally bits per pixel)
       *  @details This is the datatype of the primary image. bitpix may be one
       *           of the following CFITSIO constants: BYTE_IMG, SHORT_IMG,
       *           LONG_IMG, FLOAT_IMG, DOUBLE_IMG, USHORT_IMG, ULONG_IMG,
       *           LONGLONG_IMG. Note that if you send in a bitpix of USHORT_IMG
       *           or ULONG_IMG, CCfits will set HDU::bitpix() to its signed
       *           equivalent (SHORT_IMG | LONG_IMG), and then set BZERO to 2^15 | 2^31.
       */
      int         bitpix;

      /** @var     cubedepth
       *  @brief   depth, or number of slices for 3D data cubes
       *  @details cubedepth is used to calculate memory allocation
       */
      uint32_t      cubedepth;

      std::atomic<int> extension;           //!< extension number for multi-extension files

      std::string base_name;                //!< base image name
      std::string fitstime;                 //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()
      int nexp;                             //!< number passed to do_expose
      std::string start_time;               //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)
      std::string modeselection;

//    long get_fitsname(std::string &name_out);
//    long get_fitsname(std::string controllerid, std::string &name_out);


      /***** Camera::Information::set_axes ************************************/
      /**
       * @brief      allocate
       * @param[in]  bits_per_pixel
       * @return     ERROR|NO_ERROR
       *
       */
      void set_axes(uint8_t bits_per_pixel) {
        const std::string function("Camera::Information::set_axes");
        std::ostringstream oss;

        uint8_t bytes_per_pixel = bits_per_pixel / 2;

        // array of axis lengths
        int number_of_axes = this->cubedepth > 1 ? 3 : 2;
        this->naxes.resize(number_of_axes);

        long cols = this->region_of_interest[1]
                  - this->region_of_interest[0]
                  + 1;
        long rows = this->region_of_interest[3]
                  - this->region_of_interest[2]
                  + 1;

        if (number_of_axes >= 2) {
          this->naxes[0] = cols / this->binning[0];
          this->naxes[1] = rows / this->binning[1];
        }
        if (number_of_axes == 3) {
          this->naxes[2] = this->cubedepth;
        }

        // section_size is pixels to write for this section.
        // can be less than full sensor. accounts for cubedepth.
        this->section_size = 1;
        for (const auto &axis : this->naxes) this->section_size *= axis;

        this->image_memory = this->detector_pixels[0]
                           * this->detector_pixels[1] * bytes_per_pixel;
      }
      /***** Camera::Information::set_axes ************************************/
  };
  /***** Camera::Information **************************************************/
}
