/**
 * @file    archon_controller.h
 * @brief   defines the Archon Interface Controller
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <cinttypes>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include "common.h"
#include "network.h"
#include "camera_interface.h"
#include "camera_information.h"

/**
 * Archon constants
 */
constexpr int MAXADCCHANS =   16;              //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
constexpr int MAXADMCHANS =   72;              //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
constexpr int BLOCK_LEN   = 1024;              //!< Archon block size
constexpr int REPLY_LEN   =  100 * BLOCK_LEN;  //!< Reply buffer size (over-estimate)

/**
 * Archon Module Types
 */
constexpr int MODTYPE_NONE    =  0;
constexpr int MODTYPE_DRIVER  =  1;
constexpr int MODTYPE_ADC     =  2;
constexpr int MODTYPE_LVBIAS  =  3;
constexpr int MODTYPE_HVBIAS  =  4;
constexpr int MODTYPE_HEATER  =  5;
constexpr int MODTYPE_HS      =  7;
constexpr int MODTYPE_HVXBIAS =  8;
constexpr int MODTYPE_LVXBIAS =  9;
constexpr int MODTYPE_LVDS    = 10;
constexpr int MODTYPE_HEATERX = 11;
constexpr int MODTYPE_XVBIAS  = 12;
constexpr int MODTYPE_ADF     = 13;
constexpr int MODTYPE_ADX     = 14;
constexpr int MODTYPE_ADLN    = 15;
constexpr int MODTYPE_UNKNOWN = 16;
constexpr int MODTYPE_ADM     = 17;

/**
 * Archon commands
 */
const std::string  SYSTEM        = "SYSTEM";
const std::string  STATUS        = "STATUS";
const std::string  FRAME         = "FRAME";
const std::string  CLEARCONFIG   = "CLEARCONFIG";
const std::string  POLLOFF       = "POLLOFF";
const std::string  POLLON        = "POLLON";
const std::string  APPLYALL      = "APPLYALL";
const std::string  POWERON       = "POWERON";
const std::string  POWEROFF      = "POWEROFF";
const std::string  APPLYCDS      = "APPLYCDS";
const std::string  APPLYSYSTEM   = "APPLYSYSTEM";
const std::string  RESETTIMING   = "RESETTIMING";
const std::string  LOADTIMING    = "LOADTIMING";
const std::string  HOLDTIMING    = "HOLDTIMING";
const std::string  RELEASETIMING = "RELEASETIMING";
const std::string  LOADPARAMS    = "LOADPARAMS";
const std::string  TIMER         = "TIMER";
const std::string  FETCHLOG      = "FETCHLOG";
const std::string  UNLOCK        = "LOCK0";

/**
 * Minimum required backplane revisions for certain features
 */
const std::string REV_RAMP           = "1.0.548";
const std::string REV_SENSORCURRENT  = "1.0.758";
const std::string REV_HEATERTARGET   = "1.0.1087";
const std::string REV_FRACTIONALPID  = "1.0.1054";
const std::string REV_VCPU           = "1.0.784";

/**
 * Archon Power states
 */
const std::string POWER_UNKNOWN        = "UNKNOWN";
const std::string POWER_NOT_CONFIGURED = "NOT_CONFIGURED";
const std::string POWER_OFF            = "OFF";
const std::string POWER_INTERMEDIATE   = "INTERMEDIATE";
const std::string POWER_ON             = "ON";
const std::string POWER_STANDBY        = "STANDBY";

namespace Camera {

  class ArchonInterface;

  /***** Camera::ArchonInterface::ArchonExposureTime **************************/
  /**
   * @class    ArchonExposureTime
   * @brief    derived class inherits from ExposureTime base class
   * @details  Adds Archon-specific functions because the Archon separates
   *           seconds and milliseconds.
   *
   */
  class ArchonExposureTime : public ExposureTime {
    protected:
      uint32_t sec_part;
      uint32_t msec_part;
    public:
      ArchonExposureTime() : ExposureTime(), sec_part(0), msec_part(0) { }

      // override the base class
      void set(double value) override;

      // unique to the derived class
      std::pair<uint32_t,uint32_t> get_pair() const;
      std::pair<uint32_t,uint32_t> split(double value);
  };
  /***** Camera::ArchonInterface::ArchonExposureTime **************************/


  /***** Camera::ArchonInterface::Controller **********************************/
  /**
   * @class    Controller
   * @brief    contains information for each controller
   *
   */
  class ArchonController : public Controller {
    friend class ArchonInterface;      //!< forward declaration of parent interface

    // Archon hardware-based constants.
    // These shouldn't change unless there is a significant hardware change.
    //
    static constexpr int MAXNBUFS = 3;    //!< total number of frame buffers
    static constexpr int MAXNMODS = 12;   //!< number of modules per controller
    static constexpr int MAXNADCHAN = 4;  //!< number of channels per ADC module

    public:
      ArchonController();
      ~ArchonController();

      void configure_controller() override;
      long abort() override;

      /** @brief Archon frame type
       */
      typedef enum {
        FRAME_IMAGE,
        FRAME_RAW,
        NUM_FRAME_TYPES
      } frametype_t;

      const char *const frametype_str[NUM_FRAME_TYPES] = {
        "IMAGE",
        "RAW"
      };

      struct network_details {
          std::string hostname;
          int port;
      };

      /**
       * @var     struct bias_config_t
       * @details structure of bias configuration info
       */
      struct bias_config_t {
	std::string key;
	float vmin;
	float vmax;
      };

      bias_config_t get_bias_config(int mod, int chan) const;
      std::string make_applymod_command(int mod) const;

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int amps[2];     //!< number of amplifiers per detector for each axis, set in set_camera_mode
        int num_detect;  //!< number of detectors, set in set_camera_mode
        int linecount;   //!< number of lines per tap
        int pixelcount;  //!< number of pixels per tap
        int framemode;   //!< Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
      };

      /**
       * @var     struct tapinfo_t tapinfo[]
       * @details structure of tapinfo which is unique to each observing mode
       */
      struct tapinfo_t {
        int num_taps;
        int tap[16];
        float gain[16];
        float offset[16];
        std::string readoutdir[16];
      };

      /**
       * @var     struct frameinfo_t frame
       * @details structure to contain Archon results from "FRAME" command
       */
      struct frameinfo_t {
        std::atomic<int>      index;          // index of newest buffer data
        std::atomic<int>      currentframe;   // frame of newest buffer data
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
      } frameinfo;

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      struct config_line_t {
        int line;          // line number used for updating Archon
        std::string value; // used for configmap
      };

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      struct param_line_t {
        int line;          // line number used for updating Archon
        std::string key;   // PARAMETERn part
        std::string name;  // parametername part
        std::string value; // value part
      };

      typedef std::map<std::string, config_line_t> cfg_map_t;
      typedef std::map<std::string, param_line_t> param_map_t;

      cfg_map_t configmap;
      param_map_t parammap;

      struct rawinfo_t {
        int adchan;
        uint16_t rawsamples;
        uint16_t rawlines;
      } rawinfo;

      /**
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      struct modeinfo_t {
        int rawenable=-1;          //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t configmap;       //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;      //!< PARAMETERn=parametername=value map for mode sections
        Common::FitsKeys acfkeys;  //!< create a FitsKeys object to hold user keys read from ACF file for each mode
        geometry_t geometry;
        tapinfo_t tapinfo;
        long bigbuf=-1;
        long samplemode=-1;
      };

//  protected:
      ArchonInterface* interface;      //!< pointer back to the parent interface
      Camera::Information info;        //!< information for this controller
      Network::TcpSocket archon;       //!< this is how we talk to the Archon

      /** @var      exposure_time
       *  @details  non-owning pointer to ExposureTime object owned by Information.
       *            Valid as long as Information exists.
       */
      ArchonExposureTime* exposure_time;

      void set_interface(ArchonInterface* _interface);

      int activebufs;                  //!< number of active frame buffers
      char* framebuf;                  //!< local frame buffer read from Archon
      uint32_t framebuf_bytes;         //!< size of framebuf in bytes
      frametype_t frametype;           //!< Archon frame type (IMAGE|RAW)

      bool is_connected;               //!< true if controller connected
      bool is_powered;                 //!< power_status has 5 states. This is only true is power_status==ON
      std::atomic_flag archon_busy = ATOMIC_FLAG_INIT;  //!< indicates a thread is accessing Archon
      bool is_firmwareloaded;
      std::string firmware;
      bool is_camera_mode{false};      //!< has a camera mode been selected
      int msgref;
      std::string backplaneversion;
      std::vector<int> modtype;             //!< type of each module from SYSTEM command
      std::vector<std::string> modversion;  //!< version of each module from SYSTEM command
      std::string offset;
      std::string gain;
      int readout_time_msec;                //!< readout time in msec from config file
      int configlines;                      //!< number of configuration lines in ACF
      int n_hdrshift;
      uint64_t last_frame_timer;            //!< Archon timer of last frame
      std::string power_status;             //!< Archon power status
      std::mutex archon_mutex;
      network_details archon_network_details;

      std::string sec_param;                //!< parameter name for exposure time seconds
      std::string msec_param;               //!< parameter name for exposure time milliseconds
      std::string expose_param;             //!< parameter name to trigger exposure when set =1
      std::string abort_param;              //!< parameter name to abort when set =1 (optional)

      void connect();
      void bias(const int &mod, const int &chan, float &volts, const bool &should_write);
      long initiate_exposure(const int &nexp);
      long get_frame_status();
      template<typename T> T get_parameter(const std::string &parameter);
      long fetch(uint64_t bufferaddress, uint32_t bufferblocks);
      long get_timer(uint64_t &timer);
      void set_exptime(double exptime);
      long set_parameter(const std::string &parameter, const int &value);
      long prep_parameter(const std::string &parameter, const int &value);
      long load_parameter(const std::string &parameter, const int &value);
      long set_vcpu_inreg(const std::string &args);
      double get_exptime() const { return( this->exposure_time->get() ); }
      void print_frame_status();
      long send_cmd(const std::string &cmd, std::string &reply);
      long send_cmd(const std::string &cmd);
      long wait_for_readout();
      long fetchlog();
      long load_acf(const std::string &filename, bool write_to_archon=true);
      long load_mode_settings(modeinfo_t* mode);
      long lock_buffer(int buffernumber);
      long unlock_buffer();
      template <class T> void get_configmap_value(const std::string &key_in, T &value_out);
      long get_status_key(const std::string &key, std::string &value);
      std::string set_power(int state);
      std::string get_power();

      long allocate_framebuf(uint32_t reqsz);
      long read_frame(frametype_t type, char* &imagebufferptr);
      long write_config_key(const char* key, const char* newvalue, bool &changed);
      long write_config_key(const char* key, int newvalue, bool &changed);


      std::map<std::string, modeinfo_t> modemap;

      std::string selectedmode;    //!< currently selected mode

      /** @var      int lastframe
       *  @details  the last (I.E. previous) frame number acquired
       */
      int lastframe;
      uint64_t lasttimestamp;

      long parse_system_configuration(const std::string &message);
  };
  /***** Camera::ArchonInterface::Controller **********************************/
}
