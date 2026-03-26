/**
 * @file    shared_memory_writer.cpp
 * @brief   FrameOutput implementation using boost shared memory with ring buffer
 *
 */

#include "shared_memory_writer.h"
#include "common.h"

#include <cstring>

namespace Camera {

  SharedMemoryWriter::SharedMemoryWriter(const std::string &segment_name,
                                         size_t max_frame_bytes,
                                         uint32_t num_frames)
    : segment_name_(segment_name),
      max_frame_bytes_(max_frame_bytes),
      num_frames_(num_frames) {
  }

  SharedMemoryWriter::~SharedMemoryWriter() {
    this->close();
  }

  long SharedMemoryWriter::open() {
    const std::string function("Camera::SharedMemoryWriter::open");

    const size_t slot_size = sizeof(SharedFrameHeader) + max_frame_bytes_;
    const size_t total_size = sizeof(RingBufferControl) + slot_size * num_frames_;

    try {
      // Remove any stale segment with the same name
      boost::interprocess::shared_memory_object::remove(segment_name_.c_str());

      shm_ = std::make_unique<boost::interprocess::shared_memory_object>(
        boost::interprocess::create_only,
        segment_name_.c_str(),
        boost::interprocess::read_write
      );
      shm_->truncate(static_cast<boost::interprocess::offset_t>(total_size));

      region_ = std::make_unique<boost::interprocess::mapped_region>(
        *shm_, boost::interprocess::read_write
      );

      // Zero the entire segment
      std::memset(region_->get_address(), 0, total_size);

      // Initialize the ring buffer control block
      control_ = static_cast<RingBufferControl*>(region_->get_address());
      control_->num_frames = num_frames_;
      control_->frame_slot_size = static_cast<uint32_t>(slot_size);
      control_->write_index.store(0);

      logwrite(function, "opened segment \"" + segment_name_ + "\" (" +
               std::to_string(total_size) + " bytes, " +
               std::to_string(num_frames_) + " slots)");

    } catch (const boost::interprocess::interprocess_exception &e) {
      logwrite(function, "ERROR creating shared memory: " + std::string(e.what()));
      return ERROR;
    }

    return NO_ERROR;
  }

  long SharedMemoryWriter::write(const char* data, size_t size, const FrameMetadata& meta) {
    const std::string function("Camera::SharedMemoryWriter::write");

    if (!control_) {
      logwrite(function, "ERROR shared memory not open");
      return ERROR;
    }

    if (size > max_frame_bytes_) {
      logwrite(function, "ERROR frame size " + std::to_string(size) +
               " exceeds max " + std::to_string(max_frame_bytes_));
      return ERROR;
    }

    // Advance write index (wraps around the ring buffer)
    const uint32_t idx = control_->write_index.fetch_add(1) % num_frames_;
    char* slot = this->slot_ptr(idx);

    // Write the header
    auto* header = reinterpret_cast<SharedFrameHeader*>(slot);
    header->ready.store(false);
    header->frame_number = meta.frame_number;
    header->timestamp = meta.timestamp;
    header->width = meta.width;
    header->height = meta.height;
    header->bytes_per_pixel = meta.bytes_per_pixel;
    header->sequence_number = meta.sequence_number;

    // Copy pixel data after the header
    std::memcpy(slot + sizeof(SharedFrameHeader), data, size);

    // Signal that this slot is ready for consumers
    header->ready.store(true);

    return NO_ERROR;
  }

  void SharedMemoryWriter::close() {
    const std::string function("Camera::SharedMemoryWriter::close");

    region_.reset();
    shm_.reset();
    control_ = nullptr;

    if (!segment_name_.empty()) {
      boost::interprocess::shared_memory_object::remove(segment_name_.c_str());
      logwrite(function, "closed segment \"" + segment_name_ + "\"");
    }
  }

  char* SharedMemoryWriter::slot_ptr(uint32_t index) {
    auto* base = static_cast<char*>(region_->get_address());
    const size_t slot_size = sizeof(SharedFrameHeader) + max_frame_bytes_;
    return base + sizeof(RingBufferControl) + slot_size * index;
  }

}
