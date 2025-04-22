#include "network.h"

struct network_details {
    std::string hostname;
    int port;
};

namespace Camera {

    class ArchonInterface;

    /***** Camera::AstroCamInteface::Controller *********************************/
  /**
   * @class    Controller
   * @brief    contains information for each controller
   * @details  The Controller class is a sub-class of Interface and is
   *           here to contain the Camera::Information class and FITS_file
   *           class objects.  There will be a vector of Controller class
   *           objects which matches the vector of controller objects.
   *
   */
  class Controller {
    friend class ArchonInterface;

    public:
      Controller() = default;                 //!< class constructor
      ~Controller() = default;      //!< no deconstructor

    private:
      bool connected;                  //!< true if controller connected
      bool is_busy;
      int msgref;
      std::string backplaneversion;
      std::string modtype;
      std::vector<std::string> modversion;
      std::string offset;
      std::string gain;
      int n_hdrshift;
      std::mutex archon_mutex;
      Network::TcpSocket sock;
      network_details archon_network_details;
  };
  /***** Camera::AstroCamInteface::Controller *********************************/
}

