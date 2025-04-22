/**
 * @file    archon_controller.h
 * @brief   defines the Archon Interface Controller
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include "common.h"
#include "network.h"

struct network_details {
    std::string hostname;
    int port;
};

namespace Camera {

  class ArchonInterface;

  /***** Camera::ArchonInterface::Controller **********************************/
  /**
   * @class    Controller
   * @brief    contains information for each controller
   *
   */
  class Controller {
    friend class ArchonInterface;

    public:
      Controller();
      ~Controller() = default;

    private:
      ArchonInterface* interface;      //!< pointer back to the parent interface

      void set_interface(ArchonInterface* _interface);

      bool is_connected;               //!< true if controller connected
      bool is_busy;
      bool is_firmwareloaded;
      int msgref;
      std::string backplaneversion;
      std::vector<int> modtype;             //!< type of each module from SYSTEM command
      std::vector<std::string> modversion;  //!< version of each module from SYSTEM command
      std::string offset;
      std::string gain;
      int n_hdrshift;
      std::mutex archon_mutex;
      Network::TcpSocket sock;
      network_details archon_network_details;

      long write_config_key(const char* key, const char* newvalue, bool &changed);
      long write_config_key(const char* key, int newvalue, bool &changed);

      /**
       * config_line_t is a struct for the configfile key=value map, used to
       * store the configuration line and its associated line number.
       */
      typedef struct {
        int line;          // line number used for updating Archon
        std::string value; // used for configmap
      } config_line_t;

      /**
       * param_line_t is a struct for the PARAMETER name key=value map, used to
       * store parameters where the format is PARAMETERn=parametername=value
       */
      typedef struct {
        int line;          // line number used for updating Archon
        std::string key;   // PARAMETERn part
        std::string name;  // parametername part
        std::string value; // value part
      } param_line_t;

      typedef std::map<std::string, config_line_t> cfg_map_t;
      typedef std::map<std::string, param_line_t> param_map_t;

      cfg_map_t configmap;
      param_map_t parammap;

  };
  /***** Camera::ArchonInterface::Controller **********************************/
}
