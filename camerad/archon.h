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
#include <variant>
#include <memory>

#include "opencv2/opencv.hpp"
#include "utilities.h"
#include "common.h"
#include "camera.h"
#include "config.h"
#include "logentry.h"
#include "network.h"
#include "fits.h"       // old
#include "fits_file.h"  // new

#define REPLY_LEN 100 * BLOCK_LEN  //!< Reply buffer size (over-estimate)

namespace Archon {

    constexpr int MAXADCCHANS =   16;            //!< max number of ADC channels per controller (4 mod * 4 ch/mod)
    constexpr int MAXADMCHANS =   72;            //!< max number of ADM channels per controller (4 mod * 18 ch/mod)
    constexpr int BLOCK_LEN   = 1024;            //!< Archon block size

    // Archon commands
    //
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

    // Minimum required backplane revisions for certain features
    //
    const std::string REV_RAMP          = "1.0.548";
    const std::string REV_SENSORCURRENT = "1.0.758";
    const std::string REV_HEATERTARGET  = "1.0.1087";
    const std::string REV_FRACTIONALPID = "1.0.1054";
    const std::string REV_VCPU          = "1.0.784";

    const std::string_view QUIET = "quiet";      // allows sending commands without logging
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

    /**
     * @brief   deinterlacing modes
     */
    enum class DeInterlaceMode {
      NONE,
      RXRVIDEO
    };

    /***** Archon::PostProcess ***********************************************/
    template <typename T>
    class PostProcess {
      protected:
        DeInterlaceMode _mode;
        std::vector<std::vector<T>> _sigbuf;
        std::vector<std::vector<T>> _resbuf;
        std::vector<int32_t> _cdsbuf;

        // naxes = axis lengths element 0=cols 1=rows
        // This is for the pair of frames in a single Archon buffer, so cols will be 2*2112=4224
        // but rows can be <= 2048.
        //
        std::vector<long> _naxes;

        long _cols;
        long _rows;

      public:

        PostProcess( DeInterlaceMode mode, std::vector<long> naxes ) : _mode(mode), _naxes(naxes) {
          const std::string function="Archon::PostProcess::PostProcess";
          std::stringstream message;

          // rows and cols in an image (not the buffer)
          // half as many cols because the buffer contains two frames
          //
          _cols = _naxes[0] / 2;
          _rows = _naxes[1];

          long single_image_size = _cols * _rows;

          _sigbuf.resize( 2, std::vector<T>(single_image_size) );
          _resbuf.resize( 2, std::vector<T>(single_image_size) );
          _cdsbuf.resize(single_image_size);
        }

        void deinterlace(const T* typed_image, size_t idx) {
          std::stringstream message;
          message << "[DEBUG] datatype=" << demangle(typeid(T).name()) << " storing pair for idx=" << idx;
          logwrite( "PostProcess::deinterlace", message.str() );
          T* psignal = _sigbuf[idx].data();
          T* preset  = _resbuf[idx].data();
          for ( long row=0; row < _rows; ++row ) {
            for ( long col=0; col < _cols; col+=64 ) {
              long chan = col / 64;
              // even-number channels are a straight copy
              if (chan % 2 == 0) {
                std::memcpy( &psignal[row*_cols + col], &typed_image[row*_cols*2 + col*2],      64*sizeof(T) );
                std::memcpy(  &preset[row*_cols + col], &typed_image[row*_cols*2 + col*2 + 64], 64*sizeof(T) );
              }
              else {
              // odd-number channels read out in the opposite direction so copy each element
                const T* signal_source = &typed_image[row * _cols * 2 + col*2 + 64];  // signal is the 2nd block
                const T* reset_source  = &typed_image[row * _cols * 2 + col*2];       // reset is the 1st block
                T* signal_dest = &psignal[row * _cols + col];
                T* reset_dest  = &preset[row * _cols + col];
                for (long i=0; i<64; ++i) {
                  signal_dest[i] = signal_source[63-i];
                  reset_dest[i]  = reset_source[63-i];
                }
              }
            }
          }
        }

        /***** Archon::PostProcess::cds_subtract ******************************/
        /**
         * @brief
         * @param
         * @param
         */
        void cds_subtract(int sigidx, int residx) {
          std::string function="PostProcess::cds_subtract";
          std::stringstream message;
          message << "[DEBUG] subtracting signal[" << sigidx << "] - reset[" << residx << "]";
          logwrite( "PostProcess::cds_subtract", message.str() );

// force some pixels for testing
//_sigbuf[sigidx][0]=0;    _sigbuf[sigidx][1]=8675; _sigbuf[sigidx][2]=8675; _sigbuf[sigidx][3]=999; _sigbuf[sigidx][4]=666;
//_resbuf[residx][0]=8675; _resbuf[residx][1]=0;    _resbuf[residx][2]=8675; _resbuf[residx][3]=666; _resbuf[residx][4]=999;

          T* psignal = _sigbuf[sigidx].data();
          T* preset  = _resbuf[residx].data();

          if (psignal==nullptr) { logwrite(function, "[DEBUG] ERROR psignal is null" ); return; }
          if (preset==nullptr)  { logwrite(function, "[DEBUG] ERROR preset is null" );  return; }

          // this will hold the subtracted frame
          //
          std::unique_ptr<cv::Mat> cdsframe( new cv::Mat( cv::Mat::zeros( _rows, 2048, CV_32S ) ) );

          try {
            // create (pointers to) Mat objects to hold the sig and res frames
            // This instantiation of cv::Mat sets the dimensions to 2048 and
            // the distance between the start of subsequent rows, 2112*sizeof(T)
            //
            std::shared_ptr<cv::Mat> sigframe = std::make_shared<cv::Mat>( _rows, 2048, CV_16U, psignal, 2112 * sizeof(uint16_t) );
            std::shared_ptr<cv::Mat> resframe = std::make_shared<cv::Mat>( _rows, 2048, CV_16U, preset,  2112 * sizeof(uint16_t) );
//          sigframe.reset( new cv::Mat( 2048, 2048, CV_16U, psignal ) );
//          resframe.reset( new cv::Mat( 2048, 2048, CV_16U, preset  ) );

            // perform the subtraction, storing the result in a 32-bit signed object
            //
            cv::subtract(*sigframe, *resframe, *cdsframe, cv::noArray(), CV_32S);

logwrite(function,"[DEBUG] cv::subtract(*sigframe, *resframe, *cdswork, cv::noArray(), CV_16S)");
for (int i=0; i<5; i++) {
  message.str(""); message << "[DEBUG] pix " << cdsframe->at<int32_t>(i);
  logwrite(function, message.str());
}
/*
            cv::subtract(*sigframe, *resframe, *cdswork);
logwrite(function,"[DEBUG] cv::subtract(*sigframe, *resframe, *cdswork)");
for (int i=0; i<5; i++) {
  message.str(""); message << "[DEBUG] pix " << cdswork->at<uint16_t>(i);
  logwrite(function, message.str());
}
*/
            std::copy(cdsframe->begin<int32_t>(), cdsframe->end<int32_t>(), _cdsbuf.begin());
          }
          catch ( const cv::Exception &e ) {
            message.str(""); message << "ERROR OpenCV exception: " << e.what();
            logwrite( function, message.str() );
            return;
          }
          catch ( const std::exception &e ) {
            message.str(""); message << "ERROR std exception: " << e.what();
            logwrite( function, message.str() );
            return;
          }
        }
        /***** Archon::PostProcess::cds_subtract ******************************/


        int32_t* get_cdsbuf() { return _cdsbuf.data(); }

        /***** Archon::PostProcess::write_unp *********************************/
        /**
         * @param[in]  camera_info  Camera::Information structure
         * @param[in]  fits_file    FITS_file object
         * @param[in]  idx          index into ring buffer
         *
         */
        void write_unp( Camera::Information &camera_info, FITS_file<T> &fits_file, int idx ) {

          std::stringstream message;
          message << "[DEBUG] file=" << camera_info.fits_name
                  << " extension=" << camera_info.extension
                  << " datatype=" << demangle(typeid(T).name());
          logwrite( "PostProcess::write_unp", message.str() );


          camera_info.region_of_interest[0]=1;
          camera_info.region_of_interest[1]=_cols;
          camera_info.region_of_interest[2]=1;
          camera_info.region_of_interest[3]=_rows;

          camera_info.set_axes(USHORT_IMG);

          T* sigbuf = _sigbuf[idx].data();
          fits_file.write_image( sigbuf, get_timestamp(), ++camera_info.extension-1, camera_info );

          T* resbuf = _resbuf[idx].data();
          fits_file.write_image( resbuf, get_timestamp(), ++camera_info.extension-1, camera_info );
        }
        /***** Archon::PostProcess::write_unp *********************************/
    };
    /***** Archon::PostProcess ***********************************************/


    class Interface {
      private:
        unsigned long int start_timer, finish_timer; //!< Archon internal timer, start and end of exposure
        int n_hdrshift; //!< number of right-shift bits for Archon buffer in HDR mode

      public:
        Interface();

        ~Interface();

        // Class Objects
        //
        Network::TcpSocket archon;
        Camera::Information camera_info; /// this is the main camera_info object
        Camera::Information fits_info;   /// used to copy the camera_info object to preserve info for FITS writing
        Camera::Camera camera;           /// instantiate a Camera object
        Common::FitsKeys userkeys;       //!< instantiate a Common object
        Common::FitsKeys systemkeys;     //!< instantiate a Common object

/********
        // Create a deinterlacer using a variant of acceptable data types
        //
        std::variant<LostProcess<int16_t>,
                     LostProcess<uint16_t>,
                     LostProcess<uint32_t>> lostprocessor;

        void set_frame_dimensions( uint16_t r, uint16_t c ) {
          auto SetDimensions = [rows=r, cols=c](auto &lostprocessor) { lostprocessor.set_dimensions(rows,cols); };
          std::visit( SetDimensions, lostprocessor );
          return;
        }
********/

        template<typename Tin, typename Tout>
        Tout* typed_convert_buffer( Tin* buffer_in ) {
          // create a buffer of the correct type and space
          Tout* buffer_out = new Tout[this->camera_info.image_size];
          for (size_t pix=0; pix<this->camera_info.image_size; ++pix) {
            // for 32b scale by hdrshift
            if constexpr (std::is_same<Tin, uint32_t>::value) {
              buffer_out[pix] = static_cast<Tout>(buffer_in[pix] >> this->n_hdrshift);
            }
            else
            // for signed 16b subtract 2^15 from every pixel
            if constexpr (std::is_same<Tin, int16_t>::value) {
              buffer_out[pix] = buffer_in[pix] - 32768;  // subtract 2^15 from every pixel
            }
            else {
            // for all others (unsigned int) its a straight copy
              buffer_out[pix] = buffer_in[pix];
            }
          }
          return buffer_out;  // caller must release this!
        }


        // write the image data based on the type T
        //
        template<typename T>
        void typed_write_frame( T* buffer, FITS_file<T> &fits_file ) {
          ++this->camera_info.extension;
          Camera::Information fits_info( this->camera_info );

          #ifdef LOGLEVEL_DEBUG
          std::stringstream message;
          std::string function="Archon::Interface::typed_write_frame";
          message.str(""); message << "[DEBUG] before override:"
                                   << " fits_info.region_of_interest[0]=" << fits_info.region_of_interest[0]
                                   << " [1]=" << fits_info.region_of_interest[1]
                                   << " [2]=" << fits_info.region_of_interest[2]
                                   << " [3]=" << fits_info.region_of_interest[3];
          logwrite( function, message.str() );
          #endif

          // must override the width
          //
          fits_info.region_of_interest[0]=1;
          fits_info.region_of_interest[1]=2048;
//        fits_info.region_of_interest[2]=1;
//        fits_info.region_of_interest[3]=2048;

          fits_info.set_axes(LONG_IMG);

          #ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG]  after override:"
                                   << " fits_info.region_of_interest[0]=" << fits_info.region_of_interest[0]
                                   << " [1]=" << fits_info.region_of_interest[1]
                                   << " [2]=" << fits_info.region_of_interest[2]
                                   << " [3]=" << fits_info.region_of_interest[3];
          logwrite( function, message.str() );
          message.str(""); message << "[DEBUG] extension=" << fits_info.extension << " datatype=" << demangle(typeid(T).name())
                                                           << " compression=" << fits_info.fits_compression_type;
          logwrite( function, message.str() );
          #endif

          fits_file.write_image( buffer,
                                 get_timestamp(),
                                 fits_info.extension-1,
                                 fits_info,
                                 fits_info.fits_compression_code );
        }

        Config config;

        bITS_file fits_file; //!< instantiate a FITS container object

        int activebufs; //!< Archon controller number of active frame buffers
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
        bool is_unp;               //!< should I write unp images?
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

        int ring_index;                     //!< index into ring buffer, counts 0,1,0,1,...
        std::vector<char*> signal_buf;      //!< signal frame data ring buffer
        std::vector<char*> reset_buf;       //!< reset frame data ring buffer
        char *archon_buf;                   //!< image data buffer
        uint32_t archon_buf_bytes;          //!< requested number of bytes allocated for archon_buf rounded up to block size
        uint32_t archon_buf_allocated;      //!< allocated number of bytes for archon_buf

        std::atomic<bool> archon_busy; //!< indicates a thread is accessing Archon
        std::mutex archon_mutex;
        //!< protects Archon from being accessed by multiple threads,
                                                    //!< use in conjunction with archon_busy flag
        std::string longexposeparam; //!< param name to control longexposure in ACF (empty=not supported)
        std::string exposeparam; //!< param name to trigger exposure when set =1

        std::string shutenableparam; //!< param name to enable shutter open on expose
        int shutenable_enable; //!< the value which enables shutter enable
        int shutenable_disable; //!< the value which disables shutter enable

        // Functions
        //
        void ring_index_inc() { if (++this->ring_index==2) this->ring_index=0; }
        int  prev_ring_index() { int i=this->ring_index-1; return( i<0 ? 1 : i ); }
        long save_unp(std::string args, std::string &retstring);
        long fits_compression(std::string args, std::string &retstring);
        static long interface(std::string &iface); //!< get interface type
        long configure_controller(); //!< get configuration parameters
        long prepare_archon_buffer(); //!< prepare archon_buf, allocating memory as needed
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

        long cds(std::string args, std::string &retstring);

        long inreg(std::string args);

        long do_boi(std::string args, std::string &retstring);
        long band_of_interest(std::string args, std::string &retstring);

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
