/**
 * @file    archon.h
 * @brief   
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#pragma once

#include <CCfits/CCfits>           //!< needed here for types in set_axes()
#include <atomic>
#include <chrono>
#include <numeric>
#include <fenv.h>
#include <string_view>

#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"
#include <zmq.hpp>
#include <TimingStats.h>

#define MAXADCCHANS 16             //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
#define MAXADMCHANS 72             //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
#define BLOCK_LEN 236             //!< Archon block size 36 + 10*10*2
// #define BLOCK_LEN 198             //!< Archon block size 36 + 9*9*2
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

    constexpr std::string_view QUIET = "quiet";  // allows sending commands without logging
    constexpr uint32_t MSEC_TO_TICK = 100000;    // Archon clock ticks per millisecond
    constexpr uint32_t MAX_EXPTIME  = 0xFFFFF;   // Archon parameters are limited to 20 bits

    // Archon hardware-based constants.
    // These shouldn't change unless there is a significant hardware change.
    //
    const int nbufs = 3; //!< total number of frame buffers  //TODO rename to maxnbufs?
    const int nmods = 12; //!< number of modules per controller
    const int nadchan = 4; //!< number of channels per ADC module

    // Parameter defaults, unless overridden by the configuration file
    //
    const int DEF_TRIGIN_EXPOSE_ENABLE = 1;
    const int DEF_TRIGIN_EXPOSE_DISABLE = 0;
    const int DEF_TRIGIN_UNTIMED_ENABLE = 1;
    const int DEF_TRIGIN_UNTIMED_DISABLE = 0;
    const int DEF_TRIGIN_READOUT_ENABLE = 1;
    const int DEF_TRIGIN_READOUT_DISABLE = 0;
    const int DEF_SHUTENABLE_ENABLE = 1;
    const int DEF_SHUTENABLE_DISABLE = 0;

    class PushSocketClass {  // Ensure the class name is correct
      public:
        // Constructor with correct name (matches class name) and no return type
        PushSocketClass()
         : context(1),
           push_socket(context, zmq::socket_type::push)
        {}

        void send_data(zmq::mutable_buffer message) {
          push_socket.send(message, zmq::send_flags::none);  // Example usage of push_socket
        }

        void connect(std::string uri) {
          push_socket.connect(uri);
        }

      private:
        zmq::context_t context;       // zmq context as a class member
        zmq::socket_t push_socket;    // zmq socket as a class member

    };

    class Interface {
    private:
        unsigned long int start_timer, finish_timer; //!< Archon internal timer, start and end of exposure
        int n_hdrshift; //!< number of right-shift bits for Archon buffer in HDR mode

        // For ZMQ
        std::unique_ptr<zmq::context_t> context_;
        std::unique_ptr<zmq::socket_t> publisher_;
        std::thread serverThread_;
        std::mutex serverMutex_;

    public:
        Interface();

        ~Interface();

        // Class Objects
        //
        Network::TcpSocket archon;
        Camera::Information camera_info; /// this is the main camera_info object
        Camera::Information fits_info; /// used to copy the camera_info object to preserve info for FITS writing
        Camera::Camera camera; /// instantiate a Camera object
        Common::FitsKeys userkeys; //!< instantiate a Common object
        Common::FitsKeys systemkeys; //!< instantiate a Common object

        Config config;

        FITS_file fits_file; //!< instantiate a FITS container object

        // Timing
        mutable TimingStats fetch_stats;
        mutable TimingStats archon_ts_deltas;

        int msgref; //!< Archon message reference identifier, matches reply to command
        bool abort;
        int taplines;
        int configlines;  //!< number of configuration lines
        bool logwconfig;  //!< optionally log WCONFIG commands
        std::vector<int> gain; //!< digital CDS gain (from TAPLINE definition)
        std::vector<int> offset; //!< digital CDS offset (from TAPLINE definition)
        bool modeselected; //!< true if a valid mode has been selected, false otherwise
        bool firmwareloaded; //!< true if firmware is loaded, false otherwise
        bool is_longexposure_set; //!< true for long exposure mode (exptime in sec), false for exptime in msec
        bool is_window; //!< true if in window mode for h2rg, false if not
        bool is_autofetch;
        bool is_freerun;
        bool is_zmq;
        int win_hstart;
        int win_hstop;
        int win_vstart;
        int win_vstop;
        int taplines_store; //!< int number of original taplines
        std::string tapline0_store; //!< store tapline0 for window mode so can restore later

        bool lastcubeamps;

        std::string trigin_state; //!< for external triggering of exposures

        int trigin_expose; //!< current value of trigin expose
        int trigin_expose_enable; //!< value which enables trigin expose
        int trigin_expose_disable; //!< value which disables trigin expose

        int trigin_untimed; //!< current value of trigin_untimed
        int trigin_untimed_enable; //!< value which enables trigin untimed
        int trigin_untimed_disable; //!< value which disables trigin untimed

        int trigin_readout; //!< current value of trigin_readout
        int trigin_readout_enable; //!< value which enables trigin readout
        int trigin_readout_disable; //!< value which disables trigin readout

        std::string trigin_exposeparam; //!< parameter name to write for trigin_expose
        std::string trigin_untimedparam; //!< parameter name to write for trigin_untimed
        std::string trigin_readoutparam; //!< parameter name to write for trigin_readout

        float heater_target_min; //!< minimum heater target temperature
        float heater_target_max; //!< maximum heater target temperature

        char *image_data; //!< image data buffer
        uint32_t image_data_bytes; //!< requested number of bytes allocated for image_data rounded up to block size
        uint32_t image_data_allocated; //!< allocated number of bytes for image_data

        std::atomic<bool> archon_busy; //!< indicates a thread is accessing Archon
        std::mutex archon_mutex;
        //!< protects Archon from being accessed by multiple threads,
                                                    //!< use in conjunction with archon_busy flag
        std::string longexposeparam; //!< param name to control longexposure in ACF (empty=not supported)
        std::string exposeparam; //!< param name to trigger exposure when set =1

        std::string shutenableparam; //!< param name to enable shutter open on expose
        int shutenable_enable; //!< the value which enables shutter enable
        int shutenable_disable; //!< the value which disables shutter enable

        PushSocketClass push_socket_class;

        // Functions
        //
        static long interface(std::string &iface); //!< get interface type
        long configure_controller(); //!< get configuration parameters
        long prepare_image_buffer(); //!< prepare image_data, allocating memory as needed
        long connect_controller(const std::string &devices_in); //!< open connection to archon controller
        long disconnect_controller(); //!< disconnect from archon controller
        long load_timing(std::string acffile); //!< load specified ACF then LOADTIMING
        long load_timing(std::string acffile, std::string &retstring);

        long load_firmware(std::string acffile); //!< load specified acf then APPLYALL
        long load_firmware(std::string acffile, std::string &retstring);

        long load_acf(std::string acffile); //!< only load (WCONFIG) the specified ACF file
        long set_camera_mode(std::string mode_in);

        long load_mode_settings(std::string mode);

        long native(const std::string &cmd);

        long archon_cmd(std::string cmd);

        long archon_cmd(std::string cmd, std::string &reply);

        long read_parameter(const std::string &paramname, std::string &valstring);

        long prep_parameter(const std::string &paramname, std::string value);

        long load_parameter(std::string paramname, std::string value);

        long fetchlog();

        long get_frame_status();

        void print_frame_status();

        long lock_buffer(int buffer);

        long get_timer(unsigned long int *timer);

        long fetch(uint64_t bufaddr, uint32_t bufblocks);

        long read_frame(); //!< read Archon frame buffer into host memory
        long hread_frame();

        long read_frame(Camera::frame_type_t frame_type); /// read Archon frame buffer into host memory
        long write_frame(); //!< write (a previously read) Archon frame buffer to disk
        long write_raw(); //!< write raw 16 bit data to a FITS file
        long write_config_key(const char *key, const char *newvalue, bool &changed);

        long write_config_key(const char *key, int newvalue, bool &changed);

        long write_parameter(const char *paramname, const char *newvalue, bool &changed);

        long write_parameter(const char *paramname, int newvalue, bool &changed);

        long write_parameter(const char *paramname, const char *newvalue);

        long write_parameter(const char *paramname, int newvalue);

        template<class T>
        long get_configmap_value(std::string key_in, T &value_out);

        void add_filename_key();

        long get_status_key( std::string key, std::string &value );     /// get value for indicated key from STATUS string

        long power( std::string state_in, std::string &retstring );     /// wrapper for do_power
        long do_power( std::string state_in, std::string &retstring );  /// set/get Archon power state

        long expose(std::string nseq_in);

        long hexpose(std::string nseq_in);

        long hsetup();

        long hroi(std::string geom_in, std::string &retstring);

        long hwindow(std::string state_in, std::string &state_out);

        long autofetch(std::string state_in, std::string &state_out);

        long set_freerun(std::string set_state);

        long zmq(std::string state_in, std::string &state_out);

        long int write_to_zmq(const std::string& message);

        long video();

        long wait_for_exposure();

        long wait_for_readout();

        long hwait_for_readout();

        long get_parameter(std::string parameter, std::string &retstring);

        long set_parameter(std::string parameter, long value);

        long set_parameter(std::string parameter);

        long exptime(std::string exptime_in, std::string &retstring);

        void copy_keydb(); /// copy user keyword database into camera_info
        long longexposure(std::string state_in, std::string &state_out);

        long shutter(std::string shutter_in, std::string &shutter_out);

        long hdrshift(std::string bits_in, std::string &bits_out);

        long trigin(std::string state_in);

        long heater(std::string args, std::string &retstring);

        long sensor(std::string args, std::string &retstring);

        long bias(std::string args, std::string &retstring);

        long preampgain(std::string args, std::string &retstring);

        long cds(std::string args, std::string &retstring);

        long inreg(std::string args);

        long region_of_interest(std::string args, std::string &retstring);

        long test(std::string args, std::string &retstring);

        /**
         * @var     struct geometry_t geometry[]
         * @details structure of geometry which is unique to each observing mode
         */
        struct geometry_t {
            int amps[2]; // number of amplifiers per detector for each axis, set in set_camera_mode
            int num_detect; // number of detectors, set in set_camera_mode
            int linecount; // number of lines per tap
            int pixelcount; // number of pixels per tap
            int framemode; // Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
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
         * @var     struct frame_data_t frame
         * @details structure to contain Archon results from "FRAME" command
         */
        struct frame_data_t {
            int index; // index of newest buffer data
            int frame; // frame of newest buffer data
            int next_index; // index of next buffer
            std::string timer; // current hex 64 bit internal timer
            int rbuf; // current buffer locked for reading
            int wbuf; // current buffer locked for writing
            std::vector<int> bufsample; // sample mode 0=16 bit, 1=32 bit
            std::vector<int> bufcomplete; // buffer complete, 1=ready to read
            std::vector<int> bufmode; // buffer mode: 0=top 1=bottom 2=split
            std::vector<uint64_t> bufbase; // buffer base address for fetching
            std::vector<int> bufframen; // buffer frame number
            std::vector<int> bufwidth; // buffer width
            std::vector<int> bufheight; // buffer height
            std::vector<int> bufpixels; // buffer pixel progress
            std::vector<int> buflines; // buffer line progress
            std::vector<int> bufrawblocks; // buffer raw blocks per line
            std::vector<int> bufrawlines; // buffer raw lines
            std::vector<int> bufrawoffset; // buffer raw offset
            std::vector<uint64_t> buftimestamp; // buffer hex 64 bit timestamp
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
            int adchan; // selected A/D channels
            int rawsamples; // number of raw samples per line
            int rawlines; // number of raw lines
            int iteration; // iteration number
            int iterations; // number of iterations
        } rawinfo;

        /**
         * config_line_t is a struct for the configfile key=value map, used to
         * store the configuration line and its associated line number.
         */
        typedef struct {
            int line; // the line number, used for updating Archon
            std::string value; // used for configmap
        } config_line_t;

        /**
         * param_line_t is a struct for the PARAMETER name key=value map, used to
         * store parameters where the format is PARAMETERn=parametername=value
         */
        typedef struct {
            std::string key; // the PARAMETERn part
            std::string name; // the parametername part
            std::string value; // the value part
            int line; // the line number, used for updating Archon
        } param_line_t;

        typedef std::map<std::string, config_line_t> cfg_map_t;
        typedef std::map<std::string, param_line_t> param_map_t;

        cfg_map_t configmap;
        param_map_t parammap;

        /**
         * \var     modeinfo_t modeinfo
         * \details structure contains a configmap and parammap unique to each mode,
         *          specified in the [MODE_*] sections at the end of the .acf file.
         */
        typedef struct {
            int rawenable; //!< initialized to -1, then set according to RAWENABLE in .acf file
            cfg_map_t configmap; //!< key=value map for configuration lines set in mode sections
            param_map_t parammap; //!< PARAMETERn=parametername=value map for mode sections
            Common::FitsKeys acfkeys; //!< create a FitsKeys object to hold user keys read from ACF file for each mode
            geometry_t geometry;
            tapinfo_t tapinfo;
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
}
