#include "bob_interface.h"

namespace Camera {

  BobInterface::BobInterface() = default;
  BobInterface::~BobInterface() = default;

  void Camera::BobInterface::myfunction() {
    std::string function("Camera::BobInterface::myfunction");
    logwrite(function, "Bob's implementation of Camera::myfunction");
  }

  void Camera::BobInterface::bob_only() {
    std::cerr << "Bob's unique Camera::bob_only\n";
  }
}
