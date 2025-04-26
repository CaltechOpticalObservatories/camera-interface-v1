/**
 * @file    astrocam_interface.h
 * @brief   this defines the AstroCam implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"
#include "astrocam_controller.h"

#include <map>
#include <memory>

#include "CArcBase.h"
#include "ArcDefs.h"
#include "CExpIFace.h"
#include "CConIFace.h"
#include "CArcPCI.h"
#include "CArcPCIe.h"
#include "CArcDevice.h"
#include "CArcBase.h"
#include "CooExpIFace.h"

namespace Camera {

  class AstroCamInterface : public Interface {
    public:
      AstroCamInterface();
      ~AstroCamInterface() override;

      // These are virtual functions inherited by the Camera::Interface base class
      // and have their own controller-specific implementations which are
      // implemented in astrocam_interface.cpp
      //
      long abort( const std::string args, std::string &retstring ) override;
      long autodir( const std::string args, std::string &retstring ) override;
      long basename( const std::string args, std::string &retstring ) override;
      long bias( const std::string args, std::string &retstring ) override;
      long bin( const std::string args, std::string &retstring ) override;
      long connect_controller( const std::string args, std::string &retstring ) override;
      long disconnect_controller( const std::string args, std::string &retstring ) override;
      long power( const std::string args, std::string &retstring ) override;
      long test( const std::string args, std::string &retstring ) override;

    private:
      std::map<int, Controller> controller;
      int numdev;                           //!< number of PCI devices detected in system
      std::vector<int> configured_devnums;  //!< configured PCI devices (from camerad.cfg file)
      std::vector<int> devnums;             //!< all opened and connected devices

      // These functions are specific to the AstroCam Interface and are not
      // found in the base class.
      //
      long disconnect_controller();
      long disconnect_controller(int dev);
      long exptime( const std::string args, std::string &retstring ) override;
  };
}
