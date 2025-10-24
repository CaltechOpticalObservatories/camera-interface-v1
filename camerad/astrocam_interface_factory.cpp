/**
 * @file    astrocam_interface_factory.cpp
 * @brief   AstroCam Interface Factory
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "astrocam_interface.h"
#include "camera_interface.h"

namespace Camera {
  /***** Camera::Interface::create ********************************************/
  /**
   * @brief      factory function to create pointer to AstroCamInterface
   * @return     unique_ptr<AstroCamInterface>
   *
   */
  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<AstroCamInterface>();
  }
  /***** Camera::Interface::create ********************************************/
}
