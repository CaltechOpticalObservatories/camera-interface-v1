/**
 * @file    archon_interface_factory.cpp
 * @brief   Archon Interface Factory
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "archon_interface.h"
#include "camera_interface.h"

namespace Camera {
  /***** Camera::Interface::create ********************************************/
  /**
   * @brief      factory function to create pointer to ArchonInterface
   * @return     unique_ptr<ArchonInterface>
   *
   */
  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<ArchonInterface>();
  }
  /***** Camera::Interface::create ********************************************/
}
