#pragma once

#include <map>
#include <memory>
#include "camera.h"
#include "camerad_commands.h"

#include "CArcBase.h"
#include "ArcDefs.h"
#include "CExpIFace.h"
#include "CConIFace.h"
#include "CArcPCI.h"
#include "CArcPCIe.h"
#include "CArcDevice.h"
#include "CArcBase.h"
#include "CooExpIFace.h"

namespace AstroCam {

  constexpr int NUM_EXPBUF = 3;  ///< number of exposure buffers

  class Callback;  // forward declaration

  class Controller {
    private:
      uint32_t bufsize;

    public:

      // These are detector image geometry values for each device,
      // unaffected by binning.
      //
      int detcols;                     //!< number of detector columns (unchanged by binning)
      int detrows;                     //!< number of detector rows (unchanged by binning)
      int oscols0;                     //!< requested number of overscan rows
      int osrows0;                     //!< requested number of overscan columns
      int oscols;                      //!< realized number of overscan rows (can be modified by binning)
      int osrows;                      //!< realized number of overscan columns (can be modified by binning)
      int skiprows;
      int skipcols;

      int cols;                        //!< total number of columns read (includes overscan)
      int rows;                        //!< total number of rows read (includes overscan)

      std::string channel;             //!< name of spectrographic channel
      bool connected;                  //!< true if controller connected (requires successful TDL command)
      std::string devname;             //!< comes from arc::gen3::CArcPCI::getDeviceStringList()
      int devnum;                      //!< this controller's devnum
      std::uint32_t retval;            //!< convenient place to hold return values for threaded commands to this controller

      bool inactive;                   //!< set true to skip future use of controllers when unable to connect

      arc::gen3::CArcDevice* pArcDev;  //!< arc::CController object pointer -- things pointed to by this are in the ARC API
      Callback* pCallback;
      Camera::Information info;

      bool have_ft;                    //!< Do I have (and am I using) frame transfer?
      std::string imsize_args;         ///< IMAGE_SIZE arguments read from config file, used to restore default
      std::atomic<bool> in_readout;    //!< Is the controller currently reading out/transmitting pixels?
      std::atomic<bool> in_frametransfer;  //!< Is the controller currently performing a frame transfer?

      inline uint32_t get_bufsize() { return this->bufsize; }; 
      inline uint32_t set_bufsize( uint32_t sz ) { this->bufsize=sz; return this->bufsize; };

      void test();
  };

  class Interface {
    private:
      zmqpp::context context;
      std::vector<int> configured_devnums;  //!< vector of configured Arc devices (from camerad.cfg file)
      std::vector<int> devnums;    //!< vector of all opened and connected devices
      int _expbuf;                 //!< points to next avail in exposure vector
      std::mutex _expbuf_mutex;    //!< mutex to protect expbuf operations
      std::mutex epend_mutex;
      std::vector<int> exposures_pending;  //!< vector of devnums that have a pending exposure (which needs to be stored)
      void retval_to_string( std::uint32_t check_retval, std::string& retstring );


    public:
      Interface()
        : context(),
          publish_enable(false),
          collect_enable(false),
          subscriber(std::make_unique<Common::PubSub>(context, Common::PubSub::Mode::SUB)),
          is_subscriber_thread_running(false),
          should_subscriber_thread_run(false) {
            topic_handlers = {
              { "_snapshot", std::function<void(const nlohmann::json&)>(
                         [this](const nlohmann::json &msg) { handletopic_snapshot(msg); } ) }
            };
        }

      std::mutex publish_mutex;
      std::mutex collect_mutex;
      std::condition_variable publish_condition;
      std::condition_variable collect_condition;

      std::atomic<bool> publish_enable;
      std::atomic<bool> collect_enable;

      std::unique_ptr<Common::PubSub> publisher;       ///< publisher object
      std::string publisher_address;                   ///< publish socket endpoint
      std::string publisher_topic;                     ///< my default topic for publishing
      std::unique_ptr<Common::PubSub> subscriber;      ///< subscriber object
      std::string subscriber_address;                  ///< subscribe socket endpoint
      std::vector<std::string> subscriber_topics;      ///< list of topics I subscribe to
      std::atomic<bool> is_subscriber_thread_running;  ///< is my subscriber thread running?
      std::atomic<bool> should_subscriber_thread_run;  ///< should my subscriber thread run?
      std::unordered_map<std::string,
                         std::function<void(const nlohmann::json&)>> topic_handlers;
                                                       ///< maps a handler function to each topic

      // publish/subscribe functions
      //
      long init_pubsub(const std::initializer_list<std::string> &topics={}) {
        return Common::PubSubHandler::init_pubsub(context, *this, topics);
      }
      void start_subscriber_thread() { Common::PubSubHandler::start_subscriber_thread(*this); }
      void stop_subscriber_thread()  { Common::PubSubHandler::stop_subscriber_thread(*this); }

      void handletopic_snapshot( const nlohmann::json &jmessage );

      void publish_snapshot();
      void publish_snapshot(std::string &retstring);

      int numdev;

      Camera::Camera camera;
      Camera::Information camera_info;
      std::map<int, Controller> controller;

      std::atomic<bool> state_monitor_thread_running;
      std::condition_variable state_monitor_condition;
      std::mutex state_lock;
      void state_monitor_thread();

      /**
       * @brief      is the specified camera idle?
       * @details    Asking if a camera is idle refers to its activity
       *             status -- actively reading out or in frame transfer.
       * @param[in]  dev  device number to check
       * @return     true|false
       *
       */
      inline bool is_camera_idle( int dev ) {
        int num=0;
        num += ( this->controller[dev].in_readout ? 1 : 0 );
        num += ( this->controller[dev].in_frametransfer ? 1 : 0 );
        std::lock_guard<std::mutex> lock( this->epend_mutex );
        num += this->exposures_pending.size();
        return ( num>0 ? false : true );
      }

      inline bool is_camera_idle() {
        int num=0;
        for ( auto dev : this->devnums ) {
          num += ( this->controller[dev].in_readout ? 1 : 0 );
          num += ( this->controller[dev].in_frametransfer ? 1 : 0 );
        }
        std::lock_guard<std::mutex> lock( this->epend_mutex );
        num += this->exposures_pending.size();
        return ( num>0 ? false : true );
      }

      inline bool in_readout() const {
        int num=0;
        for ( auto dev : this->devnums ) {
          num += ( this->controller.at(dev).in_readout ? 1 : 0 );
          num += ( this->controller.at(dev).in_frametransfer ? 1 : 0 );
        }
        return( num==0 ? false : true );
      }

      inline bool in_frametransfer() const {
        int num=0;
        for ( auto dev : this->devnums ) {
          num += ( this->controller.at(dev).in_frametransfer ? 1 : 0 );
        }
        return( num==0 ? false : true );
      }

      inline void inc_expbuf() {
        std::lock_guard<std::mutex> lock( _expbuf_mutex );
        _expbuf = ( ( ++_expbuf >= NUM_EXPBUF ) ? 0 : _expbuf );
        return;
      }

      inline int get_expbuf() {
        std::lock_guard<std::mutex> lock( _expbuf_mutex );
        return _expbuf;
      }

      long buffer(std::string size_in, std::string &retstring);
      long connect_controller( std::string args, std::string &retstring );
      void deinterlace();
      long disconnect_controller();
      long disconnect_controller(int dev);
      long extract_dev_chan( std::string args, int &dev, std::string &chan, std::string &retstring );
      long geometry(std::string args, std::string &retstring);
      long image_size( std::string args, std::string &retstring, const bool save_as_default=false );

      /**
       * send 3-letter command to ...
       */
      long native(std::string cmdstr);                          ///< selected or all open controllers
      long native(std::string cmdstr, std::string &retstring);  ///< selected or all open controllers, return reply
      long native(std::vector<uint32_t> selectdev, std::string cmdstr);    ///< specified by vector
      long native(std::vector<uint32_t> selectdev, std::string cmdstr, std::string &retstring);  ///< specified by vector
      long native(int dev, std::string cmdstr, std::string &retstring);  ///< specified by devnum

      static void dothread_native(Controller &con, std::vector<uint32_t> cmd);
  };

  class Callback {
    public:
      void frameCallback();
  };
}

