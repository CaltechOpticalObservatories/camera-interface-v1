/**
 * @file    camera.h
 * @brief   camera interface functions header file common to all camera interfaces (astrocam, archon)
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#ifndef CAMERA_H
#define CAMERA_H

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
      std::string fitstime;                  //!< "YYYYMMDDHHMMSS" uesd for filename, set by get_fitsname()
      mode_t dirmode;                        //!< user specified mode to OR with 0700 for imdir creation
      int image_num;
      bool is_coadd;                         /// set true to co-add any processed cds frames
      bool is_mex;                           /// set true for multi-extensions
      bool is_longerror;                     //!< set to return error message on command port
      bool is_mexamps;                       //!< should amplifiers be written as multi-extension?
      std::atomic<bool> abortstate;          /// set true to abort the current operation (exposure, readout, etc.)
      std::atomic<bool> exposing;            /// set when exposure in progress
      std::stringstream lasterrorstring;     //!< a place to preserve an error message

    public:
      Camera();
      ~Camera();

      bool          autodir_state;           //!< if true then images are saved in a date subdir below image_dir, i.e. image_dir/YYYYMMDD/
      std::string   writekeys_when;          //!< when to write fits keys "before" or "after" exposure
      Common::Queue async;                   /// message queue object

      inline void set_abort()   { this->abortstate.store( true, std::memory_order_seq_cst );  };
      inline void clear_abort() { this->abortstate.store( false, std::memory_order_seq_cst ); };
      inline bool is_aborted()  { return this->abortstate.load( std::memory_order_seq_cst );  };

      inline void set_exposing()   { this->exposing.store( true, std::memory_order_seq_cst );  };
      inline void clear_exposing() { this->exposing.store( false, std::memory_order_seq_cst ); };
      inline bool is_exposing()    { return this->exposing.load( std::memory_order_seq_cst );  };

      inline bool coadd()       { return this->is_coadd;    };
      inline void coadd(bool in){ this->is_coadd = in;      };

      void set_dirmode( mode_t mode_in ) { this->dirmode = mode_in; }

      std::map<int, std::string> firmware;   //!< firmware file for given controller device number, read from .cfg file
      std::map<int, int> readout_time;       //!< readout time in msec for given controller device number, read from .cfg file

      std::string default_sampmode;          //!< optional default samplemode can be set in .cfg file
      std::string default_exptime;           //!< optional default exptime can be set in .cfg file
      std::string default_roi;               //!< optional default roi can be set in .cfg file

      std::string power_status;              //!< archon power status

      void log_error( std::string function, std::string message );

      std::string get_longerror();

      long imdir(std::string dir_in);
      long imdir(std::string dir_in, std::string& dir_out);
      long autodir(std::string state_in, std::string& state_out);
      long basename(std::string name_in);
      long basename(std::string name_in, std::string& name_out);
      long imnum(std::string num_in, std::string& num_out);
      long writekeys(std::string writekeys_in, std::string &writekeys_out);
      long fitsnaming(std::string naming_in, std::string& naming_out);
      void increment_imnum() { if (this->fits_naming.compare("number")==0) this->image_num++; };
      void set_fitstime(std::string time_in);
      long get_fitsname(std::string &name_out);
      long get_fitsname(std::string controllerid, std::string &name_out);
      void mex(bool state_in);
      bool mex();
      long mex(std::string state_in, std::string &state_out);
      long coadd(std::string state_in, std::string &state_out);
      void longerror(bool state_in);
      bool longerror();
      long longerror(std::string state_in, std::string &state_out);
      void mexamps(bool state_in);
      bool mexamps();
      long mexamps(std::string state_in, std::string &state_out);
  };
  /**************** Camera::Camera ********************************************/

  typedef enum {
    FRAME_IMAGE,
    FRAME_RAW,
    NUM_FRAME_TYPES
  } frame_type_t;

  const char * const frame_type_str[NUM_FRAME_TYPES] = {
    "IMAGE",
    "RAW"
  };

  /**************** Camera::Information ***************************************/
  //
  class Information {
    private:
    public:
      void swap( Information &other ) noexcept;
      friend void swap( Information &first, Information &second ) noexcept { first.swap(second); }

    int det_id;            //!< ID value of detector
    int amp_id;            //!< ID value of amplifier
    int framenum;          //!< Archon buffer frame number

    int serial_prescan;                 //!< Serial prescan number
    int serial_overscan;                //!< Serial overscan number
    int parallel_overscan;      //!< Parallel overscan number
    int image_cols;                                     //!< Number of columns in the image
    int image_rows;                                     //!< Number of rows in the image
    std::string det_name;  //!< name of detector
    std::string amp_name;  //!< name of amplifier

    std::string detector;                                               //!< Detector name string
    std::string detector_software;      //!< Detector software version string
    std::string detector_firmware;      //!< Detector hardware version string

    float pixel_scale;  //!< Image pixel scale, like arcsec per pixel
    float det_gain;                     //!< Gain value for detector electronics
    float read_noise;           //!< Read noise value
    float dark_current; //!< Dark current value

    size_t image_size;     //!< pixels per CCD

    // the following 4-element arrays are the components of FITS header
    // keywords, [0:1,2:3], i.e CCDSEC[0:1,2:3] where 0,1,2,3 are the 4
    // elements of detsize[]
    int ccdsec[4];   //!< Detection section
    int ampsec[4];   //!< amplifier section
    int trimsec[4];  //!< trim section
    int datasec[4];  //!< data section
    int biassec[4];  //!< bias section (over-scan)
    int detsec[4];   //!< detector section
    int detsize[4];  //!< physical size of the CCD

    std::vector<std::string> detid;  //!< DETID = unique detector identifier
    int gain;                                                                           //!< Image gain value

    int fits_compression_code;  //!< cfitsio code for compression type
    std::string fits_compression_type;  //!< string representing FITS compression type
    int fits_noisebits;                                 //!< noisebits parameter used when compressing floating-point images
    float frame_exposure_time;  //!< Exposure time for individual frames

      std::string directory;                          //!< Data directory
      std::string image_name;                             //!< Name of the image, includes all info
      std::string basename;                               //!< Basic image name


      /** @var     bitpix
       *  @brief   FITS datatype (not literally bits per pixel)
       *  @details This is the datatype of the primary image. bitpix may be one
       *           of the following CFITSIO constants: BYTE_IMG, SHORT_IMG,
       *           LONG_IMG, FLOAT_IMG, DOUBLE_IMG, USHORT_IMG, ULONG_IMG,
       *           LONGLONG_IMG. Note that if you send in a bitpix of USHORT_IMG
       *           or ULONG_IMG, CCfits will set HDU::bitpix() to its signed
       *           equivalent (SHORT_IMG or LONG_IMG), and then set BZERO to 2^15
       *           or 2^31.
       */
      int         bitpix;

      std::vector<long> naxes;             //!< array of axis lengths where element 0=cols, 1=rows, 2=cubedepth
      frame_type_t frame_type;             //!< frame_type is IMAGE or RAW
      long        detector_pixels[2];      //!< number of physical pixels. element 0=cols (pixels), 1=rows (lines)
      uint64_t    section_size;            //!< pixels to write for this section (could be less than full sensor size but accounts for cubedepth)
      uint64_t    image_memory;            //!< bytes per image sensor
      std::string current_observing_mode;  //!< the current mode
      std::string readout_name;            //!< name of the readout source
      int         readout_type;            //!< type of the readout source is an enum
      long        naxis;                   //!< number of axes in the image (3 for data cube)
      long        axes[3];                 //!< array of axis lengths where element 0=cols, 1=rows, 2=cubedepth <-- here for old fits.h
      int         binning[2];              //!< pixel binning, each axis
      long        axis_pixels[2];          //!< number of physical pixels used, before binning
      long        region_of_interest[4];   //!< region of interest
      bool        abortexposure;

      int           activebufs;              //!< Archon controller number of active frame buffers
      int           datatype;                //!< FITS data type (corresponding to bitpix) used in set_axes()
      bool          type_set;                //!< set when FITS data type has been defined

      // these break the STL map paradigm but it's simpler, and all that NIRC2 needs
      //
      double pixel_time;                     //!< pixel time in usec
      double pixel_skip_time;                //!< pixel skip time in usec
      double row_overhead_time;              //!< row overhead time in usec
      double row_skip_time;                  //!< row skip time in usec
      double frame_start_time;               //!< frame start time in usec
      double fs_pulse_time;                  //!< fs pulse time in usec

      /**
       * @var     cubedepth
       * @brief   depth, or number of slices for 3D data cubes
       * @details cubedepth is used to calculate memory allocation and is not necessarily the
       *          same as fitscubed, which is used to force the fits writer to create a cube.
       *
       */
      uint32_t      cubedepth;

      /**
       * @var     fitscubed
       * @brief   depth, or number of slices for 3D data cubes
       * @details fitscubed is used to tell the fitswriter to create a file with a 3rd axis or not
       *
       */
      long          fitscubed;

      int           ncoadd;                  /// current count of completed number of coadds
      int           nslice;                  /// current count of completed number of slices
      int           image_center[2];
      int           imwidth;                 /// image width displayed (written to FITS)
      int           imheight;                /// image height displayed (written to FITS)
      int           imwidth_read;            /// image width read from controller
      int           imheight_read;           /// image height read from controller
      bool          exposure_aborted;        /// was this exposure aborted?
      bool          iscds;                   //!< is CDS subtraction requested?
      int           nmcds;                   ///  number of MCDS samples
      bool          ismex;                   //!< the info object given to the FITS writer will need to know multi-extension status
      std::atomic<int>  extension;           //!< extension number for multi-extension files
      bool          shutterenable;           //!< set true to allow the controller to open the shutter on expose, false to disable it
      std::string   shutteractivate;         //!< shutter activation state
      int32_t       exposure_time;           //!< requested exposure time in exposure_unit
      uint32_t      exposure_delay;          //!< exposure delay given to controller in exposure_unit
      int32_t       requested_exptime;       //!< user-requested exposure time
      int32_t       readouttime;             //!< readout time, or minimum exposure time
      std::string   exposure_unit;           //!< exposure time unit
      int           exposure_factor;         //!< multiplier for exposure_unit relative to 1 sec (=1 for sec, =1000 for msec, etc.)
      double        exposure_progress;       //!< exposure progress (fraction)
      int           num_pre_exposures;       //!< pre-exposures are exposures taken but not saved
      bool          is_cds;                  //!< is this a CDS exposure? (subtraction will be done)
      int           nseq;                    //!< number of exposure sequences
      int           nexp;                    //!< the number to be passed to do_expose (not all instruments use this)
      int           num_coadds;              //!< number of coadds
      int           sampmode;                //!< sample mode (NIRC2)
      int           sampmode_ext;            //!< sample mode number of extensions (NIRC2)
      int           sampmode_frames;         //!< sample mode number of frames (NIRC2)
      std::string   fits_name;               //!< contatenation of Camera's image_dir + image_name + image_num
      std::string   cmd_start_time;          //!< system time when the expose command arrived (YYYY-MM-DDTHH:MM:SS.sss)
      std::string   start_time;              //!< system time when the exposure started (YYYY-MM-DDTHH:MM:SS.sss)
      std::string   stop_time;               //!< system time when the exposure stopped (YYYY-MM-DDTHH:MM:SS.sss)

      std::vector< std::vector<long> > amp_section;

      bool writekeys_before;

      Common::FitsKeys userkeys;     /// create a FitsKeys object for FITS keys specified by the user
      Common::FitsKeys systemkeys;   /// create a FitsKeys object for FITS keys imposed by the software
      Common::FitsKeys extkeys;      /// create a FitsKeys object for FITS keys imposed by the software

      // Default Information class constructor and initializer list
      //
      Information() :
        fits_compression_code(0),          //!< cfitsio code for compression type
        fits_compression_type("none"),     //!< string representing FITS compression type
        naxes({0, 0, 0}),                  //!< array of axis lengths where element 0=cols, 1=rows, 2=cubedepth
        binning{1, 1},                     //!< pixel binning, each axis
        region_of_interest{1, 1, 1, 1},    //!< region of interest
        activebufs(3),
        datatype(0),                       //!< FITS data type (corresponding to bitpix) used in set_axes()
        type_set(false),                   //!< set when FITS data type has been defined
        pixel_time(0),                     //!< pixel time in usec
        pixel_skip_time(0),                //!< pixel skip time in usec
        row_overhead_time(0),              //!< row overhead time in usec
        row_skip_time(0),                  //!< row skip time in usec
        frame_start_time(0),               //!< frame start time in usec
        fs_pulse_time(0),                  //!< fs pulse time in usec
        cubedepth(1),                      //!< cubedepth
        fitscubed(1),                      //!< fitscubed
        ncoadd(0),                         //!< current count of completed number of coadds
        nslice(0),                         //!< current count of completed number of slices
        image_center{1, 1},                //!< image center
        exposure_aborted(false),           //!< was this exposure aborted?
        iscds(false),                      //!< is CDS subtraction requested?
        nmcds(0),                          //!< number of MCDS samples
        ismex(false),                      //!< the info object given to the FITS writer will need to know multi-extension status
        extension(0),
        shutteractivate(""),               //!< shutter activation state
        exposure_time(-1),                 //!< requested exposure time in exposure_unit
        requested_exptime(0),              //!< user-requested exposure time
        readouttime(-1),                   //!< readout time, or minimum exposure time
        exposure_unit(""),                 //!< exposure time unit
        exposure_factor(-1),               //!< multiplier for exposure_unit relative to 1 sec (=1 for sec, =1000 for msec, etc.)
        num_pre_exposures(0),              //!< pre-exposures are exposures taken but not saved
        is_cds(false),                     //!< is this a CDS exposure? (subtraction will be done)
        nseq(1),                           //!< number of exposure sequences
        nexp(-1),                          //!< the number to be passed to do_expose (not all instruments use this)
        num_coadds(1),                     //!< number of coadds
        sampmode(-1),                      //!< sample mode (NIRC2)
        sampmode_ext(-1),                  //!< sample mode number of extensions (NIRC2)
        sampmode_frames(-1),               //!< sample mode number of frames (NIRC2)
        writekeys_before(true)             //!< 
        { }

    // Copy constructor needed because the default copy constructor is deleted,
    // due to the atomic variable (which doesn't have a copy or copy assignment
    // constructor). So this manually loads and stores the atomic. All of the
    // other variables have to come along, too.
    //
    // Copy Constructor
    Information(const Information& other) :
          det_id(other.det_id),
          amp_id(other.amp_id),
          framenum(other.framenum),
          serial_prescan(other.serial_prescan),
          serial_overscan(other.serial_overscan),
          parallel_overscan(other.parallel_overscan),
          image_cols(other.image_cols),
          image_rows(other.image_rows),
          det_name(other.det_name),
          amp_name(other.amp_name),
          detector(other.detector),
          detector_software(other.detector_software),
          detector_firmware(other.detector_firmware),
          pixel_scale(other.pixel_scale),
          det_gain(other.det_gain),
          read_noise(other.read_noise),
          dark_current(other.dark_current),
          image_size(other.image_size),
          detid(other.detid),
          gain(other.gain),
          fits_compression_code(other.fits_compression_code),
          fits_compression_type(other.fits_compression_type),
          fits_noisebits(other.fits_noisebits),
          frame_exposure_time(other.frame_exposure_time),
          directory(other.directory),
          image_name(other.image_name),
          basename(other.basename),
          bitpix(other.bitpix),
          naxes(other.naxes),
          frame_type(other.frame_type),
          section_size(other.section_size),
          image_memory(other.image_memory),
          current_observing_mode(other.current_observing_mode),
          readout_name(other.readout_name),
          readout_type(other.readout_type),
          naxis(other.naxis),
          abortexposure(other.abortexposure),
          activebufs(other.activebufs),
          datatype(other.datatype),
          type_set(other.type_set),
          pixel_time(other.pixel_time),
          pixel_skip_time(other.pixel_skip_time),
          row_overhead_time(other.row_overhead_time),
          row_skip_time(other.row_skip_time),
          frame_start_time(other.frame_start_time),
          fs_pulse_time(other.fs_pulse_time),
          cubedepth(other.cubedepth),
          fitscubed(other.fitscubed),
          ncoadd(other.ncoadd),
          nslice(other.nslice),
          imwidth(other.imwidth),
          imheight(other.imheight),
          imwidth_read(other.imwidth_read),
          imheight_read(other.imheight_read),
          exposure_aborted(other.exposure_aborted),
          iscds(other.iscds),
          nmcds(other.nmcds),
          ismex(other.ismex),
          extension(other.extension.load()),
          shutterenable(other.shutterenable),
          shutteractivate(other.shutteractivate),
          exposure_time(other.exposure_time),
          exposure_delay(other.exposure_delay),
          requested_exptime(other.requested_exptime),
          readouttime(other.readouttime),
          exposure_unit(other.exposure_unit),
          exposure_factor(other.exposure_factor),
          exposure_progress(other.exposure_progress),
          num_pre_exposures(other.num_pre_exposures),
          is_cds(other.is_cds),
          nseq(other.nseq),
          nexp(other.nexp),
          num_coadds(other.num_coadds),
          sampmode(other.sampmode),
          sampmode_ext(other.sampmode_ext),
          sampmode_frames(other.sampmode_frames),
          fits_name(other.fits_name),
          cmd_start_time(other.cmd_start_time),
          start_time(other.start_time),
          stop_time(other.stop_time),
          amp_section(other.amp_section),
          writekeys_before(other.writekeys_before),
          userkeys(other.userkeys),
          systemkeys(other.systemkeys),
          extkeys(other.extkeys) {
        // Copy fixed-size arrays
        std::copy(std::begin(other.ccdsec), std::end(other.ccdsec), std::begin(ccdsec));
        std::copy(std::begin(other.ampsec), std::end(other.ampsec), std::begin(ampsec));
        std::copy(std::begin(other.trimsec), std::end(other.trimsec), std::begin(trimsec));
        std::copy(std::begin(other.datasec), std::end(other.datasec), std::begin(datasec));
        std::copy(std::begin(other.biassec), std::end(other.biassec), std::begin(biassec));
        std::copy(std::begin(other.detsec), std::end(other.detsec), std::begin(detsec));
        std::copy(std::begin(other.detsize), std::end(other.detsize), std::begin(detsize));
        std::copy(std::begin(other.detector_pixels), std::end(other.detector_pixels), std::begin(detector_pixels));
        std::copy(std::begin(other.axes), std::end(other.axes), std::begin(axes));
        std::copy(std::begin(other.binning), std::end(other.binning), std::begin(binning));
        std::copy(std::begin(other.axis_pixels), std::end(other.axis_pixels), std::begin(axis_pixels));
        std::copy(std::begin(other.region_of_interest), std::end(other.region_of_interest), std::begin(region_of_interest));
        std::copy(std::begin(other.image_center), std::end(other.image_center), std::begin(image_center));
// std::cout << "Information copy constructor extension=" << other.extension.load() << "\n";
    }

    // Copy assignment operator
    //
    Information& operator=(Information other) { // Pass-by-value enables copy-and-swap
      swap(other);
// std::cout << "Information copy assignment extension=" << other.extension.load() << "\n";
      return *this;
    }

    // Destructor
    //
    ~Information() = default;

      long pre_exposures( std::string num_in, std::string &num_out );

      long set_axes() {
        std::string function = "Camera::Information::set_axes";
        std::stringstream message;
        uint8_t bytes_per_pixel;

        if ( this->frame_type == FRAME_RAW ) {
          bytes_per_pixel = 2;
          this->datatype = USHORT_IMG;
        }
        else {
          switch ( this->bitpix ) {
            case 16:
              bytes_per_pixel = 2;
              this->datatype = USHORT_IMG;
              break;
            case 32:
              bytes_per_pixel = 4;
              this->datatype = FLOAT_IMG;
              break;
          default:
            message << "ERROR: unknown bitpix " << this->bitpix << ": expected {16,32}";
            logwrite( function, message.str() );
            return (ERROR);
          }
        }
        this->type_set = true;         // datatype has been set

        this->axis_pixels[0] = this->region_of_interest[1] -
                               this->region_of_interest[0] + 1;
        this->axis_pixels[1] = this->region_of_interest[3] -
                               this->region_of_interest[2] + 1;

        long num_axis = ( this->cubedepth > 1 ? 3 : 2 );

        if ( this->cubedepth > 1 ) {
          this->axes[0] = this->axis_pixels[0] / this->binning[0];  // cols
          this->axes[1] = this->axis_pixels[1] / this->binning[1];  // rows
          this->axes[2] = this->cubedepth;                          // cubedepth
          // Pixels to write for this image section, includes depth for 3D data cubes
          this->section_size = this->axes[0] * this->axes[1] * this->axes[2];
        }
        else {
          this->axes[0] = this->axis_pixels[0] / this->binning[0];  // cols
          this->axes[1] = this->axis_pixels[1] / this->binning[1];  // rows
          this->axes[2] = 0;                                        // (no cube)
          // Pixels to write for this image section, includes depth for 3D data cubes
          this->section_size = this->axes[0] * this->axes[1];
        }

        this->naxes.resize(num_axis);

        for ( int i=0; i<num_axis; i++ ) this->naxes[i]=this->axes[i];     // for Reed's FITS engine

        this->image_size   = this->section_size;

        this->image_memory = this->detector_pixels[0] 
                           * this->detector_pixels[1] * bytes_per_pixel;       // Bytes per detector, single frame read

#ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] region_of_interest[1]=" << this->region_of_interest[1]
                << " region_of_interest[0]=" << this->region_of_interest[0]
                << " region_of_interest[3]=" << this->region_of_interest[3]
                << " region_of_interest[2]=" << this->region_of_interest[2]
                << " num_axis=" << num_axis
                << " axes[0]=" << this->axes[0]
                << " axes[1]=" << this->axes[1]
                << " axes[2]=" << this->axes[2];
        logwrite( function, message.str() );
#endif

        return (NO_ERROR);
      }
  };
  /**************** Camera::Information ***************************************/

}
#endif
