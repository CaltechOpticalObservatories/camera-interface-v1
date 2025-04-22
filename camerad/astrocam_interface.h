/**
 * @file    astrocam_interface.h
 * @brief   this defines the AstroCam implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"
#include "astrocam_controller.h"
#include "camerad_commands.h"

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

      // these functions are inherited by the Camera::Interface class
      //
      void myfunction() override;
      long connect_controller( const std::string args, std::string &retstring ) override;
      long disconnect_controller( const std::string args, std::string &retstring ) override;

    private:
      std::map<int, Controller> controller;
      int numdev;                           //!< number of PCI devices detected in system
      std::vector<int> configured_devnums;  //!< configured PCI devices (from camerad.cfg file)
      std::vector<int> devnums;             //!< all opened and connected devices

      // these functions are specific to the AstroCam Interface
      //
      long disconnect_controller();
      long disconnect_controller(int dev);
      long exptime( const std::string args, std::string &retstring ) override;
  };
}
