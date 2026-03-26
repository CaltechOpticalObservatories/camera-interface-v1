/**
 * @file    archon_interface.h
 * @brief   this defines the Archon implementation of Camera::Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include "camera_interface.h"          //!< defines Camera::Interface base class
#include "archon_controller.h"
#include "archon_exposure_modes.h"
#include "camera_information.h"

namespace Camera {

  class Controller;

  class ArchonInterface : public Interface {
    friend ArchonController;

    public:
      ArchonInterface();

      // These are virtual functions inherited by the Camera::Interface base class
      // and have their own controller-specific implementations which are
      // implemented in archon_interface.cpp.
      //
      void configure_interface() override;
      long abort( const std::string args, std::string &retstring ) override;
      long autodir( const std::string args, std::string &retstring ) override;
      long basename( const std::string args, std::string &retstring ) override;
      long bias( const std::string args, std::string &retstring ) override;
      long bin( const std::string args, std::string &retstring ) override;
      long connect_controller( const std::string args, std::string &retstring ) override;
      long disconnect_controller( const std::string args, std::string &retstring ) override;
      long exptime( const std::string args, std::string &retstring ) override;
      void set_exptime(double exptime) override;
      long expose( const std::string args, std::string &retstring ) override;
      long exposure_mode( const std::string args, std::string &retstring ) override;
      std::vector<std::string> get_exposure_modes() override;
      long set_exposure_mode(const std::string &modein, const std::vector<std::string> &modeargs) override;
      long load_firmware( const std::string &args, std::string &retstring ) override;
      long native( const std::string args, std::string &retstring ) override;
      long power( const std::string args, std::string &retstring ) override;
      long test( const std::string args, std::string &retstring ) override;

      long do_expose() override;

      // Archon controller command dispatcher
      //
      long controller_cmd(const std::string &cmd,
                          const std::string &args,
                          std::string &retstring) override;

      // These functions are specific to the Archon Interface and are not
      // found in the base class.
      //
      long disconnect_controller();
      long load_firmware(const std::string &acffile);
      long allocate_framebuf(uint32_t reqsz);
      long get_parameter(const std::string &args, std::string &retstring);
      long load_timing(std::string cmd, std::string &reply);
      long read_acf(const std::string &filename);
      long set_parameter(const std::string &args, std::string &retstring);
      long set_camera_mode(std::string args, std::string &retstring);
      long set_camera_mode(std::string modeselect);
      long set_vcpu_inreg(const std::string &args, std::string &retstring);
      long autofetch_mode(const std::string &args, std::string &retstring);

      char* get_framebuf() { return controller->framebuf; }

      bool is_autofetch_mode{false};

      /** @var     controller
       *  @brief   for hardware operations with the Archon controller
       *  @details typed pointer to Archon-specific controller
       */
      ArchonController* controller;

      std::string_view QUIET = "quiet";  // allows sending commands without logging

      // These functions are specific to the Archon Interface and are not
      // found in the base class.
      //
    private:
      bool is_mode_defined(const std::string &modename) {
        return (this->controller &&
               (this->controller->modemap.find(modename) != this->controller->modemap.end())
            );
      }
      long connect_controller(const std::string& devices_in);
      long load_timing(const std::string &filename);
      long set_image_geometry(ArchonController::modeinfo_t* mode);

  };

}
