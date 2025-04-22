#include "archon_interface.h"
#include <iostream>

namespace Camera {

  ArchonInterface::ArchonInterface() = default;
  ArchonInterface::~ArchonInterface() = default;


  void Camera::ArchonInterface::myfunction() {
    const std::string function("Camera::ArchonInterface::myfunction");
    logwrite(function, "Archon's implementation of Camera::myfunction");
  }

  long Camera::ArchonInterface::connect_controller( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::connect_controller");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }

  long Camera::ArchonInterface::disconnect_controller( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::disconnect_controller");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }

  long Camera::ArchonInterface::exptime( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::exptime");
    logwrite(function, "not yet implemented");
    return NO_ERROR;
  }


}
