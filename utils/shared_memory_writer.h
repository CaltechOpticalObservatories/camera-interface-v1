/**
 * @file    shared_memory_writer.h
 * @brief   FrameOutput implementation using boost shared memory with ring buffer
 *
 */
#pragma once

#include "frame_output.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace Camera {

  /**
   * Shared memory layout per frame slot:
   *   FrameHeader (fixed size) followed by pixel data (variable size)
   */
  struct SharedFrameHeader {
    uint64_t frame_number;
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_pixel;
    uint64_t sequence_number;
    std::atomic<bool> ready;
  };

  /**
   * Ring buffer control block at the start of the shared memory segment.
   */
  struct RingBufferControl {
    uint32_t num_frames;
    uint32_t frame_slot_size;
    std::atomic<uint32_t> write_index;
  };

  class SharedMemoryWriter : public FrameOutput {
    public:
      SharedMemoryWriter(const std::string &segment_name,
                         size_t max_frame_bytes,
                         uint32_t num_frames = 4);
      ~SharedMemoryWriter() override;

      long open() override;
      long write(const char* data, size_t size, const FrameMetadata& meta) override;
      void close() override;

    private:
      std::string segment_name_;
      size_t max_frame_bytes_;
      uint32_t num_frames_;

      std::unique_ptr<boost::interprocess::shared_memory_object> shm_;
      std::unique_ptr<boost::interprocess::mapped_region> region_;

      RingBufferControl* control_{nullptr};

      // Returns pointer to the start of frame slot at given index
      char* slot_ptr(uint32_t index);
  };

}
