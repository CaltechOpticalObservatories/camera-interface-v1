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

}
