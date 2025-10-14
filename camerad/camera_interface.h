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

  class Controller {
    public:
      virtual ~Controller() = default;
      virtual void configure_controller() = 0;
  };

  class Interface {
    protected:
      Camera::Server* server=nullptr;
      Camera::Information camera_info;
      Common::FitsKeys systemkeys;

      std::unique_ptr<ExposureModeBase> exposure_mode;
      std::unique_ptr<Controller> controller;

      /***** Camera::Interface::set_controller ********************************/
      /**
       * @brief      Sets the controller and returns a typed reference to it
       * @details    This template function takes ownership of a controller via
       *             unique_ptr and stores it in the base class, while returning
       *             a reference to the derived controller type. This allows the
       *             base class to manage lifetime while derived classes maintain
       *             typed access without repeated downcasts.
       * @tparam     T            the derived controller type
       * @param[in]  _controller  unique_ptr to the controller instance
       * @return     reference to the controller with its derived type
       *
       */
      template <typename T>
      T &set_controller(std::unique_ptr<T> _controller) {
        T &controller_ref = *_controller;
        this->controller = std::move(_controller);
        return controller_ref;
      }
      /***** Camera::Interface::set_controller ********************************/

      std::atomic<bool> is_producer_finished;
      std::atomic<bool> is_producer_error;
      std::atomic<bool> is_consumer_error;

    public:
      virtual ~Interface() = default;

      Config configfile;

      // These functions are shared by all interfaces with common implementations,
      // and are implemented in camera_interface.cpp
      //
      void set_server(Camera::Server* s);
      void func_shared();
      void disconnect_controller();

      // These virtual functions have interface-specific implementations
      // and must be implemented by derived classes, implemented in xxxx_interface.cpp
      //
      virtual long abort( std::string args, std::string &retstring ) = 0;
      virtual long autodir( std::string args, std::string &retstring ) = 0;
      virtual long basename( std::string args, std::string &retstring ) = 0;
      virtual long bias( std::string args, std::string &retstring ) = 0;
      virtual long bin( std::string args, std::string &retstring ) = 0;
      virtual void configure_controller() { if (this->controller) this->controller->configure_controller(); }
      virtual long connect_controller( std::string args, std::string &retstring ) = 0;
      virtual long disconnect_controller( std::string args, std::string &retstring ) = 0;
      virtual long exptime( std::string args, std::string &retstring ) = 0;
      virtual long expose( std::string args, std::string &retstring ) = 0;
      virtual long load_firmware( const std::string &args, std::string &retstring ) = 0;
      virtual long native( std::string args, std::string &retstring ) = 0;
      virtual long power( std::string args, std::string &retstring ) = 0;
      virtual long test( std::string args, std::string &retstring ) = 0;

      virtual long do_expose(int nexp) = 0;
      virtual void image_acquisition_thread() = 0;
      virtual void image_processing_thread() = 0;

  };

}
