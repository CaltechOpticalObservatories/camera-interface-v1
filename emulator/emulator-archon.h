/**
 * @file    emulator-archon.h
 * @brief   header for interface to Archon emulator
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include <atomic>
#include <chrono>
#include <numeric>
#include <functional>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <fenv.h>

#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"

#include "generic.h"
#include "nirc2.h"

namespace Archon {

  // Archon hardware-based constants.
  // These shouldn't change unless there is a significant hardware change.
  //
  constexpr const int NBUFS = 3;        //!< total number of frame buffers  //TODO rename to maxnbufs?
  constexpr const int NMODS = 12;       //!< number of modules per controller
  constexpr const int NADCHAN = 4;      //!< number of channels per ADC module
  constexpr const int BLOCKLEN = 1024;  //!< Archon block size in bytes

  class Interface {
    private:
      std::string instr;
      std::atomic<bool> abort{false};
      std::atomic<bool> exposing{false};
      unsigned long int start_timer, finish_timer;  //!< Archon internal timer, start and end of exposure

      // Declare a map to contain image types for each recognized instrument.
      //
      static std::unordered_map<std::string, std::function<std::unique_ptr<ImageInfoBase>()>> ImageInfoMap;

      static void initialize_image_info_map();

    public:

      std::unique_ptr<ImageInfoBase> image;   ///!< smart pointer to the base class

      Interface( const std::string &instr );

      // Class Objects
      //
      Config config;

      std::string systemfile;

      unsigned long long init_time;
      bool poweron;                //!< is the power on?
      bool bigbuf;                 //!< is BIGBUF==1 in ACF file?
      std::string exposeparam;               //!< param name to trigger exposure when set =1

      struct image_t {
        uint32_t framen;
        int  activebufs;             //!< number of active frame buffers
        int  taplines;               //!< from "TAPLINES=" in ACF file
        int  linecount;              //!< from "LINECOUNT=" in ACF file
        int  pixelcount;             //!< from "PIXELCOUNT=" in ACF file
        int  readtime;               //!< from "READOUT_TIME=" in configuration file
        double pixel_time;           //!< PIXEL_TIME
        double pixel_skip_time;      //!< PIXEL_SKIP_TIME
        double row_overhead_time;    //!< ROW_OVERHEAD_TIME
        double row_skip_time;        //!< ROW_SKIP_TIME
        double frame_start_time;     //!< FRAME_START_TIME
        double fs_pulse_time;        //!< FS_PULSE_TIME
        int  imwidth;                //!< 
        int  imheight;               //!< 
        int  readouttime;            //!< 
        int  exptime;                //!< requested exposure time in msec from WCONFIG
        int  exposure_factor;        //!< multiplier for exptime relative to 1 sec (=1 for sec, =1000 for msec, etc.)
        bool iscds;                  //!< is this a CDS exposure? (NIRC2)
        int utr_samples;             //!< number of UTR samples (NIRC2)
        int mcds_samples;            //!< number of MCDS samples (NIRC2)
        int numsamples;              //!< number of samples, larger of utr,mcds (NIRC2)
      } fimage;

      /**
       * @var     struct frame_data_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frame_data_t {
        int      index;                       // index of newest buffer data
        int      frame;                       // index of newest buffer data
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

      // Functions
      //
      long configure_controller();           //!< get configuration parameters from .cfg file
      long system_report(std::string buf, std::string &retstring);         
      long status_report(std::string &retstring);
      long timer_report(std::string &retstring);
      unsigned long  get_timer();
      long frame_report(std::string &retstring);
      long fetch_data( const std::string &ref, const std::string &cmd, Network::TcpSocket &sock );      //!< 
      long wconfig(std::string buf);         //!< 
      long rconfig(std::string buf, std::string &retstring);         
      long write_parameter(std::string buf);
      static void dothread_expose( Archon::Interface &iface, int numexpose );

// TODO ***** below here need to check what's needed **********************************************************

      int  msgref;                           //!< Archon message reference identifier, matches reply to command
      std::vector<int> gain;                 //!< digital CDS gain (from TAPLINE definition)
      std::vector<int> offset;               //!< digital CDS offset (from TAPLINE definition)
      bool modeselected;                     //!< true if a valid mode has been selected, false otherwise
      bool firmwareloaded;                   //!< true if firmware is loaded, false otherwise

      float heater_target_min;               //!< minimum heater target temperature
      float heater_target_max;               //!< maximum heater target temperature

      char *image_data;                      //!< image data buffer
      uint32_t image_data_bytes;             //!< requested number of bytes allocated for image_data rounded up to block size
      uint32_t image_data_allocated;         //!< allocated number of bytes for image_data

      std::atomic<bool> archon_busy;         //!< indicates a thread is accessing Archon
      std::mutex archon_mutex;               //!< protects Archon from being accessed by multiple threads,
                                             //!< use in conjunction with archon_busy flag

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int  amps[2];              // number of amplifiers per detector for each axis, set in set_camera_mode
        int  num_detect;           // number of detectors, set in set_camera_mode
        int  linecount;            // number of lines per tap
        int  pixelcount;           // number of pixels per tap
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
        int rawsamples;      // number of raw samples per line
        int rawlines;        // number of raw lines
        int iteration;       // iteration number
        int iterations;      // number of iterations
      } rawinfo;

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        std::string line;      // the line number, used for updating Archon
        std::string key;       // the part before the '='
        std::string value;     // the part after the '='
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        std::string key;       // the PARAMETERn part
        std::string name;      // the parametername part
        std::string value;     // the value part
        std::string line;      // the line number
      } param_line_t;

      typedef std::map<std::string, config_line_t>  cfg_map_t;
      typedef std::map<std::string, param_line_t>   param_map_t;

      cfg_map_t   configmap;
      param_map_t parammap;

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

}
