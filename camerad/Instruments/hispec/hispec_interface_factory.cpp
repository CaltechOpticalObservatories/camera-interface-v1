/**
 * @file    Instruments/hispec/hispec_interface_factory.cpp
 * @brief   HISPEC Interface Factory
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include "hispec_instrument.h"
#include "camera_interface.h"

namespace Camera {

  /***** Camera::Interface::create ********************************************/
  /**
   * @brief      factory function to create pointer to HISPEC
   * @return     unique_ptr<Hispec>
   *
   */
  std::unique_ptr<Interface> Interface::create() {
    return std::make_unique<Hispec>();
  }
  /***** Camera::Interface::create ********************************************/

}
