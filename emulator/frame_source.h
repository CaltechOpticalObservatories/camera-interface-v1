#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fitsio.h>

#include "utilities.h"

namespace Emulator {

  // Abstract interface for filling frame buffers with pixel data
  class FrameSource {
    public:
      virtual ~FrameSource() = default;

      // Fill buffer with one frame of pixel data (uint16 pixels)
      /// @param buffer  destination buffer, must be at least width*height*2 bytes
      /// @param width   frame width in pixels
      /// @param height  frame height in pixels
      /// @return true on success
      virtual bool fill_frame(char* buffer, int width, int height) = 0;
  };


  // Generate synthetic frames with bias + Gaussian noise
  // Mode-aware: RXR generates signal/reset halves with correlated noise
  class SyntheticSource : public FrameSource {
    private:
      std::mt19937 rng{std::random_device{}()};
      double noise_stddev = 50.0;
      std::string* active_mode;  // non-owning pointer to emulator's active mode
      int taplines = 0;

    public:
      // @param mode  pointer to the active mode string (owned by Interface)
      // @param taps  number of taplines for RXR half-width calculation
      SyntheticSource(std::string* mode = nullptr, int taps = 0)
        : active_mode(mode), taplines(taps) {}

      void set_taplines(int taps) { taplines = taps; }

      bool fill_frame(char* buffer, int width, int height) override {
        auto* pixels = reinterpret_cast<uint16_t*>(buffer);
        std::normal_distribution<double> noise(0.0, noise_stddev);
        size_t npixels = static_cast<size_t>(width) * height;

        // Check if active mode contains "RXR" (covers VideoRXR, RXRV, etc)
        bool rxr_mode = active_mode &&
                         active_mode->find("RXR") != std::string::npos;

        if (rxr_mode && taplines > 0) {
          // RXR: each tap has signal pixels (first half) then reset pixels (second half)
          // Generate correlated noise per pixel pair so CDS subtraction cancels it,
          // leaving bias difference (10000 - 5000 = 5000) + small readout noise
          int pixels_per_tap = width / taplines;
          int half = pixels_per_tap / 2;

          for (int row = 0; row < height; row++) {
            for (int tap = 0; tap < taplines; tap++) {
              int tap_offset = row * width + tap * pixels_per_tap;
              // Generate noise once per pixel pair, apply to both signal and reset
              for (int col = 0; col < half; col++) {
                double common_noise = noise(rng);
                double sig_val = 10000.0 + common_noise + noise(rng) * 0.3;
                double res_val =  5000.0 + common_noise + noise(rng) * 0.3;
                pixels[tap_offset + col]        = static_cast<uint16_t>(std::clamp(sig_val, 0.0, 65535.0));
                pixels[tap_offset + col + half] = static_cast<uint16_t>(std::clamp(res_val, 0.0, 65535.0));
              }
            }
          }
        }
        else {
          // Default: flat bias + noise + horizontal gradient
          for (size_t i = 0; i < npixels; i++) {
            int col = i % width;
            double val = 5000.0 + noise(rng) + static_cast<double>(col) * 100.0 / width;
            pixels[i] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
          }
        }
        return true;
      }
  };


  // Read FITS files from folder, serve them sequentially, cycling
  // Not mode-aware: FITS data is already in the correct format for its mode
  class FitsFileSource : public FrameSource {
    private:
      std::vector<std::string> files;
      size_t current_index = 0;

    public:
      explicit FitsFileSource(const std::string &datadir) {
        std::string function = "(Emulator::FitsFileSource) ";
        for (const auto &entry : std::filesystem::directory_iterator(datadir)) {
          auto path = entry.path().string();
          if (path.ends_with(".fits") || path.ends_with(".fits.gz") ||
              path.ends_with(".fit")  || path.ends_with(".fit.gz")) {
            files.push_back(path);
          }
        }
        std::sort(files.begin(), files.end());
        std::cout << get_timestamp() << function << files.size()
                  << " FITS files found in " << datadir << "\n";
      }

      bool fill_frame(char* buffer, int width, int height) override {
        std::string function = "(Emulator::FitsFileSource::fill_frame) ";
        if (files.empty()) {
          std::cerr << get_timestamp() << function << "ERROR: no FITS files\n";
          return false;
        }

        const auto &path = files[current_index % files.size()];
        current_index++;

        fitsfile* fptr = nullptr;
        int status = 0;

        fits_open_file(&fptr, path.c_str(), READONLY, &status);
        if (status) {
          std::cerr << get_timestamp() << function << "ERROR opening " << path << "\n";
          return false;
        }

        int naxis = 0;
        long naxes[2] = {0, 0};
        fits_get_img_dim(fptr, &naxis, &status);
        fits_get_img_size(fptr, 2, naxes, &status);

        if (status || naxis != 2) {
          std::cerr << get_timestamp() << function << "ERROR: " << path
                    << " is not a 2D image\n";
          fits_close_file(fptr, &status);
          return false;
        }

        if (naxes[0] != width || naxes[1] != height) {
          std::cerr << get_timestamp() << function << "ERROR: " << path
                    << " dimensions " << naxes[0] << "x" << naxes[1]
                    << " don't match expected " << width << "x" << height << "\n";
          fits_close_file(fptr, &status);
          return false;
        }

        long fpixel[2] = {1, 1};
        long nelements = static_cast<long>(width) * height;
        fits_read_pix(fptr, TUSHORT, fpixel, nelements, nullptr, buffer, nullptr, &status);
        fits_close_file(fptr, &status);

        if (status) {
          std::cerr << get_timestamp() << function << "ERROR reading pixels from " << path << "\n";
          return false;
        }

        std::cout << get_timestamp() << function << "loaded " << path << "\n";
        return true;
      }
  };


  // Create the appropriate FrameSource based on config
  inline std::unique_ptr<FrameSource> make_frame_source(
      const std::string &datadir,
      std::string* active_mode = nullptr,
      int taplines = 0) {
    if (!datadir.empty() && std::filesystem::is_directory(datadir)) {
      return std::make_unique<FitsFileSource>(datadir);
    }
    return std::make_unique<SyntheticSource>(active_mode, taplines);
  }

}
