/**
 * @file    camera_interface.h
 * @brief   this defines the Camera::Interface base class
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "config.h"
#include "common.h"
#include "camera_information.h"
#include "camerad_commands.h"
#include "exposure_modes.h"

namespace Camera {

  class Server;  // forward declaration for Interface class

  /** @struct   ImageBuffer
   *  @brief    holds one or more frames of type T from the controller
   *  @details  A single ImageBuffer object can contain multiple frames,
   *            or slices, as would be the case for a datacube.
   */
  template <typename T>
  struct ImageBuffer {
    int n_slices;                    ///< number of slices in this image
    std::shared_ptr<T[]> rawpixels;  ///< frame buffer(s)
  };

  class Controller {
    public:
      virtual ~Controller() = default;
      virtual void configure_controller() = 0;
      virtual long abort() = 0;
  };

  class Interface {
    protected:
      Camera::Server* server=nullptr;

      std::shared_ptr<ExposureMode> exposuremode;
      std::unique_ptr<Controller> controller;

      std::atomic<bool> abortstate{false};

    public:
      virtual ~Interface() = default;

      // Every interface will have this factory function to create a pointer
      // to the appropriate derived interface class.
      //
      static std::unique_ptr<Interface> create();

      Config configfile;
      Camera::Information camera_info;
//    Common::FitsKeys systemkeys;  move to Camera::Information?

      // These functions are shared by all interfaces with common implementations,
      // and are implemented in camera_interface.cpp
      //
      void set_server(Camera::Server* s);
      void func_shared();
      void disconnect_controller();
      bool is_exposuremode_set() { return ( this->exposuremode && !this->exposuremode->get_type().empty() ); }

      void set_abortstate()   { this->abortstate.store(true, std::memory_order_seq_cst); }
      void clear_abortstate() { this->abortstate.store(false, std::memory_order_seq_cst); }
      bool is_aborted()       { return this->abortstate.load(std::memory_order_seq_cst); }

      // These virtual functions have interface-specific implementations
      // and must be implemented by derived classes, implemented in xxxx_interface.cpp
      //
      virtual void configure_interface() = 0;
      virtual void configure_instrument() { }
      virtual long abort( std::string args, std::string &retstring ) = 0;
      virtual long autodir( std::string args, std::string &retstring ) = 0;
      virtual long basename( std::string args, std::string &retstring ) = 0;
      virtual long bias( std::string args, std::string &retstring ) = 0;
      virtual long bin( std::string args, std::string &retstring ) = 0;
      virtual void configure_controller() { if (this->controller) this->controller->configure_controller(); }
      virtual long connect_controller( std::string args, std::string &retstring ) = 0;
      virtual long disconnect_controller( std::string args, std::string &retstring ) = 0;
      virtual long exptime( std::string args, std::string &retstring ) = 0;
      virtual void set_exptime(double exptime) = 0;
      virtual long expose( std::string args, std::string &retstring ) = 0;
      virtual long exposure_mode( std::string args, std::string &retstring ) = 0;
      virtual std::vector<std::string> get_exposure_modes() = 0;
      virtual long set_exposure_mode(const std::string &modein, const std::vector<std::string> &modeargs) = 0;
      virtual long load_firmware( const std::string &args, std::string &retstring ) = 0;
      virtual long native( std::string args, std::string &retstring ) = 0;
      virtual long power( std::string args, std::string &retstring ) = 0;
      virtual long test( std::string args, std::string &retstring ) = 0;

      virtual long do_expose() = 0;

      /** @brief  returns error if not overridden
       */
      virtual long instrument_cmd(const std::string &cmd,
                                  const std::string &args,
                                  std::string &retstring) {
	retstring="not_supported";
	return ERROR;
      }

      /** @brief  returns error if not overridden
       */
      virtual long controller_cmd(const std::string &cmd,
                                  const std::string &args,
                                  std::string &retstring) {
	retstring="not_supported";
	return ERROR;
      }
  };

}
