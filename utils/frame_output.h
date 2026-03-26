/**
 * @file    frame_output.h
 * @brief   abstract base class for frame output destinations
 *
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace Camera {

  struct FrameMetadata {
    uint64_t frame_number{0};
    uint64_t timestamp{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t bytes_per_pixel{0};
    uint64_t sequence_number{0};
  };

  class FrameOutput {
    public:
      virtual ~FrameOutput() = default;
      virtual long open() = 0;
      virtual long write(const char* data, size_t size, const FrameMetadata& meta) = 0;
      virtual void close() = 0;
  };

}
