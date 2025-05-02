/**
 * @file    camera_interface.h
 * @brief   this defines the Camera::Interface base class
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "common.h"
#include "camera_information.h"
#include "camerad_commands.h"
#include "exposure_modes.h"

namespace Camera {

  class Server;  // forward declaration for Interface class

  class Interface {
    protected:
      Camera::Server* server=nullptr;
      Camera::Information camera_info;
      Common::FitsKeys systemkeys;

      std::unique_ptr<ExposureMode> exposure_mode;

    public:
      virtual ~Interface() = default;

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
      virtual long connect_controller( std::string args, std::string &retstring ) = 0;
      virtual long disconnect_controller( std::string args, std::string &retstring ) = 0;
      virtual long exptime( std::string args, std::string &retstring ) = 0;
      virtual long expose( std::string args, std::string &retstring ) = 0;
      virtual long load_firmware( std::string args, std::string &retstring ) = 0;
      virtual long native( std::string args, std::string &retstring ) = 0;
      virtual long power( std::string args, std::string &retstring ) = 0;
      virtual long test( std::string args, std::string &retstring ) = 0;

  };

}
