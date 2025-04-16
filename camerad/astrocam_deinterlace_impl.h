#pragma once

#include "astrocam_deinterlace.h"

namespace Camera {

  template <class T>
    DeInterlace(T* buf1, T* buf2, long bufsz, int cols, int rows, int readout_type) {
      this->imbuf = buf1;
      this->workbuf = buf2;
      this->bufsize = bufsz;
      this->cols = cols;
      this->rows = rows;
      this->readout_type = readout_type;
    }

