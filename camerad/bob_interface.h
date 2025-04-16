
#pragma once

#include "camera_interface.h"

namespace Camera {

  class BobInterface : public Interface {
    public:
      ~BobInterface() override;

      // functions common to all interfaces
      //
      void myfunction() override;

      // interface-specific functions
      //
      void bob_only();
  };

}
