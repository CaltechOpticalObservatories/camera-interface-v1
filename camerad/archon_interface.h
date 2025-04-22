/**
 * @file    archon_interface.h
 * @brief   this defines the Archon implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"          //!< defines Camera::Interface base class

namespace Camera {

  class ArchonInterface : public Interface {
    public:
      ArchonInterface();
      ~ArchonInterface() override;

      // these virtual functions are inherited by the Camera::Interface class
      // and implemented in archon_interface.cpp
      //
      void myfunction() override;
      long connect_controller( const std::string args, std::string &retstring ) override;
      long disconnect_controller( const std::string args, std::string &retstring ) override;
      long exptime( const std::string args, std::string &retstring ) override;
  };

}

