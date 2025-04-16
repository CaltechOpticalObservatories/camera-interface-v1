/**
 * @file    camera_interface.h
 * @brief   this defines the Camera::Interface base class
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "common.h"

namespace Camera {

  class Server;

  class Interface {
    protected:
      Camera::Server* server=nullptr;

    public:
      virtual ~Interface() = default;

      // shared functions by all interfaces with common implementations
      //
      void set_server(Camera::Server* s);
      void func_shared();

      // virtual functions with interface-specific implementations
      // must be implemented by derived classes
      //
      virtual void myfunction() = 0;
      virtual long connect_controller( std::string args, std::string &retstring ) = 0;
      virtual long disconnect_controller( std::string args, std::string &retstring ) = 0;

  };

}

