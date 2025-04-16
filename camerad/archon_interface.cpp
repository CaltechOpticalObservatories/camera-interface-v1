#include "archon_interface.h"
#include <iostream>

namespace Camera {

  ArchonInterface::~ArchonInterface() = default;

  void Camera::ArchonInterface::myfunction() {
    std::cerr << "Archon's implementation of Camera::myfunction\n";
  }

}
