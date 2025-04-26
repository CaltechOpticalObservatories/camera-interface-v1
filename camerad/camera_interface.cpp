#include "camera_interface.h"

namespace Camera {

  // this is the code for shared functions common to all implementations

  void Interface::set_server(Camera::Server* s) {
    this->server=s;
  }

  void Interface::func_shared() {
    std::string function("Camera::Interface::func_shared");
    logwrite(function, "common implementation function");
  }

  /***** Camera::Interface::disconnect_controller *****************************/
  /**
   * @brief      disconnect camera controller
   * @details    use this to disconnect before exiting because it takes
   *             no arguments and returns nothing
   *
   */
  void Interface::disconnect_controller() {
    std::string retstring;
    this->disconnect_controller("", retstring);
  }
  /***** Camera::Interface::disconnect_controller *****************************/
}
