/**
 * @file    camera.h
 * @brief   camera interface functions header file common to all camera interfaces (astrocam, archon)
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include <CCfits/CCfits>           /// needed here for types in set_axes()
#include <dirent.h>                /// for imdir()
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

#include "common.h"
#include "logentry.h"
#include "utilities.h"

// handy snprintf shortcut
#define SNPRINTF(VAR, ...) { snprintf(VAR, sizeof(VAR), __VA_ARGS__); }

namespace Camera {
    /**************** Camera::Camera ********************************************/
    //
    class Camera {
    private:
        std::string image_dir;
        std::string base_name;
        std::string fits_naming;

        std::string fitstime; //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()

        mode_t dirmode; //!< user specified mode to OR with 0700 for imdir creation
        int image_num;
        bool is_datacube;
        bool is_longerror; //!< set to return error message on command port
        bool is_cubeamps; //!< should amplifiers be written as multi-extension data cubes?
        std::atomic<bool> abortstate;

        std::mutex abort_mutex;
        std::stringstream lasterrorstring; //!< a place to preserve an error message

    public:
        Camera() : image_dir("/tmp"), base_name("image"), fits_naming("time"),
                   dirmode(0), image_num(0), is_datacube(false), is_longerror(false), is_cubeamps(false),
                   abortstate(false), autodir_state(true), writekeys_when("before") {
        }


        bool autodir_state;
        //!< if true then images are saved in a date subdir below image_dir, i.e. image_dir/YYYYMMDD/

        std::string writekeys_when; //!< when to write fits keys "before" or "after" exposure
        Common::Queue async; /// message queue object

        inline void set_abort()   { this->abortstate.store(true, std::memory_order_seq_cst); };
        inline void clear_abort() { this->abortstate.store(false, std::memory_order_seq_cst); };
        inline bool is_aborted()  { return this->abortstate.load(std::memory_order_seq_cst); };

        void set_dirmode(mode_t mode_in) { this->dirmode = mode_in; }

        std::map<int, std::string> firmware; //!< firmware file for given controller device number, read from .cfg file
        std::map<int, int> readout_time;
        //!< readout time in msec for given controller device number, read from .cfg file

        void log_error(std::string function, std::string message);

        std::string get_longerror();

        long imdir(std::string dir_in);

        long imdir(std::string dir_in, std::string &dir_out);

        long autodir(std::string state_in, std::string &state_out);

        long basename(std::string name_in);

        long basename(std::string name_in, std::string &name_out);

        long imnum(std::string num_in, std::string &num_out);

        long writekeys(std::string writekeys_in, std::string &writekeys_out);

        long fitsnaming(std::string naming_in, std::string &naming_out);

        void increment_imnum() { if (this->fits_naming == "number") this->image_num++; };

        void set_fitstime(std::string time_in);

        long get_fitsname(std::string &name_out);

        long get_fitsname(std::string controllerid, std::string &name_out);

        void abort();

        void datacube(bool state_in);

        bool datacube();

        long datacube(std::string state_in, std::string &state_out);

        void longerror(bool state_in);

        bool longerror();

        long longerror(std::string state_in, std::string &state_out);

        void cubeamps(bool state_in);

        bool cubeamps();

        long cubeamps(std::string state_in, std::string &state_out);
    };

    /**************** Camera::Camera ********************************************/

    typedef enum {
        FRAME_IMAGE,
        FRAME_RAW,
        NUM_FRAME_TYPES
    } frame_type_t;

    const char *const frame_type_str[NUM_FRAME_TYPES] = {
        "IMAGE",
        "RAW"
    };


    /***** Camera::ExposureTime ***********************************************/
    /**
     * @class  ExposureTime
     * @brief  creates object that encapsulates exposure time with a function
     *         to split it into sec+msec which are needed by Archon.
     *
     */
    class ExposureTime {
      private:
        double   _exptime_sec;  //!< total exposure time
        uint32_t _sec_part;     //!< whole number seconds
        uint32_t _msec_part;    //!< whole number milliseconds

      public:
        ExposureTime() : _exptime_sec(0), _sec_part(0), _msec_part(0) { }

        /***** Camera::ExposureTime:split *************************************/
        /**
         * @brief      split the exposure time into sec+msec
         * @details    Exposure time is represented as double precision seconds
         *             but the Archon needs whole number seconds and/or milliseconds.
         *             Archon parameters are limited to 20-bits. When it fits,
         *             assign the exposure time to msec only, otherwise overflow
         *             into sec.
         *             This is only a tool and does not set any class variables.
         * @param[in]  value  double precision seconds
         * @return     pair{sec,msec}
         * @throws     std::runtime_error
         */
        std::pair<uint32_t,uint32_t> split(double value) {
          if (std::isnan(value)) throw std::runtime_error("value not a number");
          if (value < 0) throw std::runtime_error("value can't be negative");

          uint32_t totmsec;  // total exposure time in msec
          uint32_t sec;      // whole number of seconds
          uint32_t msec;     // whole number of msec

          // total exposure time in msec
          totmsec = static_cast<uint32_t>(std::round(value*1000.0));

          // if it fits in 20 bits then assign it all to msec
          if (totmsec <= 0xFFFFF) {
            sec  = 0;
            msec = totmsec;
          }
          else
          // otherwise assign it to seconds
          if (totmsec/1000 <= 0xFFFFF) {
            sec  = totmsec / 1000;
            msec = totmsec % 1000;
          }
          else throw std::runtime_error("value exceeds 2^20 sec");

          // return the pair
          return {sec,msec};
        }
        /***** Camera::ExposureTime:split *************************************/

        /***** Camera::ExposureTime:set ***************************************/
        /**
         * @brief      set the exposure time
         * @details    This uses the class split() tool and sets class variables.
         * @param[in]  value  double precision seconds
         * @throws     std::runtime_error
         */
        void set(double value) {
          try {
            auto [sec,msec] = split(value);
            _sec_part       = sec;
            _msec_part      = msec;
            _exptime_sec    = sec + msec/1000.0;  // store value rounded to nearest msec
          }
          catch (const std::exception &e) { throw; }
        }
        /***** Camera::ExposureTime:set ***************************************/

        std::pair<uint32_t,uint32_t> get_pair() { return {_sec_part, _msec_part}; }
        double get()  { return _exptime_sec;  }
    };
    /***** Camera::ExposureTime ***********************************************/


    /***** Camera::XxposureTime ***********************************************/
    /**
     * @class  XxposureTime
     * @brief  creates object that encapsulates exposure time with its unit
     *
     * This class allows setting and getting an exposure time together with
     * its unit. The default unit and value is msec and 0. Internally, the
     * class stores the exposure time as an unsigned 32 bit value in the
     * units of _unit but a specific unit can be requested with .s() and .ms()
     * functions. Standard operators + - * / > < >= <= == are overloaded here
     * to operate on scalars only.
     *
     */
    class XxposureTime {
      private:
        uint32_t    _value;            // exposure time in the current units
        bool        _is_set;           // has it been set using the class value() function?
        std::string _unit;             // "s" for seconds, "ms" for milliseconds
        bool        _is_longexposure;  // true for s, false for ms

      public:
        /**
         * @brief      class constructor optionally sets exposure time and unit
         * @param[in]  time  exposure time, defaults to 0 if not provided
         * @param[in]  u     unit, "s" or "ms", defaults to ms if not provided
         *
         * Since this is a generic class, there is no range check on the
         * exposure time value; that must be done by whatever instantiates
         * an ExposureTime object.
         *
         */
        explicit XxposureTime( uint32_t time=0, const std::string &u="ms" )
                   : _value(0), _is_set(false), _unit(u), _is_longexposure(u=="s") {
          if ( u != "ms" && u != "s" ) throw std::invalid_argument("invalid unit, expected \"s\" or \"ms\"");
          this->value(time);
        }

        /**
         * @brief      set the unit
         * @details    this will modify the value as necessary
         * @param[in]  u  "ms" or "s" for milliseconds or seconds
         */
        void unit( const std::string &newunit ) {
          if ( newunit == "s" || newunit == "ms" ) {
            if ( _unit != newunit ) {
              _value = ( newunit == "ms" ? _value*1000 : _value/1000 );
              _unit = newunit;
              _is_longexposure = ( newunit == "s" );
            }
          }
          else throw std::invalid_argument("invalid unit, expected \"s\" or \"ms\"");
        }

        /**
         * @brief      return the current unit
         * @return     "s" | "ms"
         */
        std::string unit() const { return _unit; }

        /**
         * @brief      set the longexposure state
         * @details    uses the class unit() function to both set the unit
         *             and modify the value as necessary.
         * @param[in]  true | false
         */
        void longexposure( bool state ) { this->unit( state ? "s" : "ms" ); }

        /**
         * @brief      return the longexposure state
         * @return     true | false
         */
        bool is_longexposure() const { return _is_longexposure; }

        /**
         * @brief      return the exposure time in units of milliseconds
         * @return     exposure time
         */
        uint32_t ms() const {
          return ( _unit == "ms" ? _value : _value * 1000 );
        }

        /**
         * @brief      return the exposure time in units of seconds
         * @return     exposure time
         */
        uint32_t s() const {
          return ( _unit == "s" ? _value : _value / 1000 );
        }

        /**
         * @brief      return the exposure time in the current units
         * @return     exposure time
         */
        uint32_t value() const {
          return _value;
        }

        /**
         * @brief      set the exposure time
         * @details    There is no range check on the value; that must be done
         *             by whatever instantiates an ExposureTime object.
         * @param[in]  time  exposure time in the current unit
         */
        void value( uint32_t time ) {
          _value = time;
          _is_set = true;
        }

        /**
         * @brief      return _is_set flag
         * @details    allows checking if a user has explicitly set the exposure time.
         * @return     true | false
         */
        bool is_set() const { return _is_set; }

        /**
         * @brief      overload operators to perform in the correct units
         * @details    Use .s() or .ms() to operate on seconds or milliseconds.
         *             These only work on scalars (not other ExposureTime objects).
         *
         * E.G. .ms() +/- 1000 will add/subtract 1000 msec to/from the current value
         *       .s() +/- 1    will add/subtract 1 sec to/from the current value
         *       .s() == xxx   will compare value in seconds to xxx
         */
        XxposureTime operator+( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() );
          return XxposureTime( val + scalar, _unit );
        }
        XxposureTime operator-( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() );
          return XxposureTime( val - scalar, _unit );
        }
        XxposureTime operator*( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return XxposureTime( val * scalar );
        }
        XxposureTime operator/( uint32_t scalar ) const {
          if ( scalar == 0 ) throw std::invalid_argument("division by zero");
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return XxposureTime( val / scalar );
        }
        bool operator<( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val < scalar;
        }
        bool operator>( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val > scalar;
        }
        bool operator==( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val == scalar;
        }
        bool operator<=( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val <= scalar;
        }
        bool operator>=( uint32_t scalar ) const {
          uint32_t val = ( _unit == "ms" ? ms() : s() * 1000 );
          return val >= scalar;
        }
    };
    /***** Camera::XxposureTime ***********************************************/


    /**************** Camera::Information ***************************************/
    //
    class Information {
    private:
    public:
        std::string hostname; //!< Archon controller hostname
        int port; //!< Archon controller TPC/IP port number
        int activebufs; //!< Archon controller number of active frame buffers
        int bitpix; //!< Archon bits per pixel based on SAMPLEMODE
        int datatype; //!< FITS data type (corresponding to bitpix) used in set_axes()
        bool type_set; //!< set when FITS data type has been defined
        frame_type_t frame_type; //!< frame_type is IMAGE or RAW
        long detector_pixels[2]; //!< element 0=cols (pixels), 1=rows (lines)
        long section_size; //!< pixels to write for this section (could be less than full sensor size)
        long image_memory; //!< bytes per image sensor
        std::string current_observing_mode; //!< the current mode
        std::string readout_name; //!< name of the readout source
        int readout_type; //!< type of the readout source is an enum
        long naxis;
        long axes[2];
        int binning[2];
        long axis_pixels[2];
        long region_of_interest[4];
        long image_center[2];
        bool abortexposure;
        bool iscube; //!< the info object given to the FITS writer will need to know cube status
        int extension; //!< extension number for data cubes
        bool shutterenable; //!< set true to allow the controller to open the shutter on expose, false to disable it
        std::string shutteractivate; //!< shutter activation state
        double exposure_progress; //!< exposure progress (fraction)
        int num_pre_exposures; //!< pre-exposures are exposures taken but not saved
        std::string fits_name; //!< contatenation of Camera's image_dir + image_name + image_num
        std::string start_time; //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)

        std::vector<std::vector<long> > amp_section;

        ExposureTime exposure_time;

        Common::FitsKeys userkeys; /// create a FitsKeys object for FITS keys specified by the user
        Common::FitsKeys systemkeys; /// create a FitsKeys object for FITS keys imposed by the software


        Information() {
            this->axes[0] = 1;
            this->axes[1] = 1;
            this->binning[0] = 1;
            this->binning[1] = 1;
            this->region_of_interest[0] = 1;
            this->region_of_interest[1] = 1;
            this->region_of_interest[2] = 1;
            this->region_of_interest[3] = 1;
            this->image_center[0] = 1;
            this->image_center[1] = 1;
            this->iscube = false;
            this->datatype = -1;
            this->type_set = false; //!< set true when datatype has been defined
            this->shutteractivate = "";
            this->num_pre_exposures = 0; //!< default is no pre-exposures
        }

        long pre_exposures(std::string num_in, std::string &num_out);

        long set_axes() {
            std::string function = "Camera::Information::set_axes";
            std::stringstream message;
            long bytes_per_pixel;

            if (this->frame_type == FRAME_RAW) {
                bytes_per_pixel = 2;
                this->datatype = USHORT_IMG;
            } else {
                switch (this->bitpix) {
                    case 16:
                        bytes_per_pixel = 2;
                        this->datatype = SHORT_IMG;
                        break;
                    case 32:
                        bytes_per_pixel = 4;
                        this->datatype = FLOAT_IMG;
                        break;
                    default:
                        message << "ERROR: unknown bitpix " << this->bitpix << ": expected {16,32}";
                        logwrite(function, message.str());
                        return (ERROR);
                }
            }
            this->type_set = true; // datatype has been set

            this->naxis = 2;

            this->axis_pixels[0] = this->region_of_interest[1] -
                                   this->region_of_interest[0] + 1;
            this->axis_pixels[1] = this->region_of_interest[3] -
                                   this->region_of_interest[2] + 1;

            this->axes[0] = this->axis_pixels[0] / this->binning[0];
            this->axes[1] = this->axis_pixels[1] / this->binning[1];

            this->section_size = this->axes[0] * this->axes[1]; // Pixels to write for this image section
            this->image_memory = this->detector_pixels[0]
                                 * this->detector_pixels[1] * bytes_per_pixel; // Bytes per detector

#ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] region_of_interest[1]=" << this->region_of_interest[1]
                << " region_of_interest[0]=" << this->region_of_interest[0]
                << " region_of_interest[3]=" << this->region_of_interest[3]
                << " region_of_interest[2]=" << this->region_of_interest[2]
                << " axes[0]=" << this->axes[0]
                << " axes[1]=" << this->axes[1];
        logwrite( function, message.str() );
#endif

            return (NO_ERROR);
        }
    };

    /**************** Camera::Information ***************************************/
}
