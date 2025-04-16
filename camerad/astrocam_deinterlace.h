#pragma once

namespace Camera {

  template <class T>
  class DeInterlace {
    public:
      DeInterlace();

      void do_deinterlace(int row_start, int row_stop, int index, int index_flip);
  };
}
