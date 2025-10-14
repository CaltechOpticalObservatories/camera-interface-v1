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
#include "camera_interface.h"

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
  class ArchonController : public Controller {
    friend class ArchonInterface;      //!< forward declaration of parent interface

    // Archon hardware-based constants.
    // These shouldn't change unless there is a significant hardware change.
    //
    const int NBUFS = 3; //!< total number of frame buffers  //TODO rename to maxnbufs?
    const int NMODS = 12; //!< number of modules per controller
    const int NADCHAN = 4; //!< number of channels per ADC module

    public:
      ArchonController();
      ~ArchonController();

      void configure_controller() override;

      typedef enum {
        FRAME_IMAGE,
        FRAME_RAW,
        NUM_FRAME_TYPES
      } frametype_t;

      const char *const frametype_str[NUM_FRAME_TYPES] = {
        "IMAGE",
        "RAW"
      };

    private:
      ArchonInterface* interface;      //!< pointer back to the parent interface
      Camera::Information info;        //!< information for this controller
      Network::TcpSocket archon;       //!< this is how we talk to the Archon

      void set_interface(ArchonInterface* _interface);

      char* framebuf;                  //!< local frame buffer read from Archon
      uint32_t framebuf_bytes;         //!< size of framebuf in bytes
      frametype_t frametype;

      bool is_connected;               //!< true if controller connected
      std::atomic_flag archon_busy = ATOMIC_FLAG_INIT;  //!< indicates a thread is accessing Archon
      bool is_firmwareloaded;
      std::string firmware;
      bool is_camera_mode;             //!< has a camera mode been selected
      int msgref;
      std::string backplaneversion;
      std::vector<int> modtype;             //!< type of each module from SYSTEM command
      std::vector<std::string> modversion;  //!< version of each module from SYSTEM command
      std::string offset;
      std::string gain;
      int configlines;                      //!< number of configuration lines in ACF
      int n_hdrshift;
      std::string power_status;             //!< Archon power status
      std::mutex archon_mutex;
      network_details archon_network_details;

      long send_cmd(const std::string &cmd, std::string &reply);
      long send_cmd(const std::string &cmd);
      long fetchlog();
      long load_acf(const std::string &filename, bool write_to_archon=true);
      long get_status_key(const std::string &key, std::string &value);
      long set_power(int state);
      long get_power();
      long get_power(std::string &power);

      long allocate_framebuf(uint32_t reqsz);
      long read_frame(frametype_t type);
      long write_config_key(const char* key, const char* newvalue, bool &changed);
      long write_config_key(const char* key, int newvalue, bool &changed);

      /**
       * @var     struct geometry_t geometry[]
       * @details structure of geometry which is unique to each observing mode
       */
      struct geometry_t {
        int amps[2];     //!< number of amplifiers per detector for each axis, set in set_camera_mode
        int num_detect;  //!< number of detectors, set in set_camera_mode
        int linecount;   //!< number of lines per tap
        int pixelcount;  //!< number of pixels per tap
        int framemode;   //!< Archon deinterlacing mode, 0=topfirst, 1=bottomfirst, 2=split
      };

      /**
       * @var     struct tapinfo_t tapinfo[]
       * @details structure of tapinfo which is unique to each observing mode
       */
      struct tapinfo_t {
        int num_taps;
        int tap[16];
        float gain[16];
        float offset[16];
        std::string readoutdir[16];
      };

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

      /**
       * \var     modeinfo_t modeinfo
       * \details structure contains a configmap and parammap unique to each mode,
       *          specified in the [MODE_*] sections at the end of the .acf file.
       */
      typedef struct {
        int rawenable;             //!< initialized to -1, then set according to RAWENABLE in .acf file
        cfg_map_t configmap;       //!< key=value map for configuration lines set in mode sections
        param_map_t parammap;      //!< PARAMETERn=parametername=value map for mode sections
        Common::FitsKeys acfkeys;  //!< create a FitsKeys object to hold user keys read from ACF file for each mode
        geometry_t geometry;
        tapinfo_t tapinfo;
      } modeinfo_t;

      std::map<std::string, modeinfo_t> modemap;

      long parse_system_configuration(const std::string &message);
  };
  /***** Camera::ArchonInterface::Controller **********************************/
}
