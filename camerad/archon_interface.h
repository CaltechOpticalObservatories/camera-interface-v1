
#pragma once

#include "camera_interface.h"

namespace Camera {

  class ArchonInterface : public Interface {
    public:
      ~ArchonInterface() override;

      void myfunction() override;
  };

}

