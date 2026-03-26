/**
 * @file    timing_stats.h
 * @brief   lightweight utility for collecting and summarizing timing measurements
 *
 */

#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace Utils {

  class TimingStats {
    public:
      void clear() { samples_.clear(); }

      void add(double value_us) { samples_.push_back(value_us); }

      // Record elapsed time since a time_point, returns the duration in microseconds
      template<typename Clock = std::chrono::steady_clock>
      double record_since(std::chrono::time_point<Clock> start) {
        auto now = Clock::now();
        double us = std::chrono::duration<double, std::micro>(now - start).count();
        add(us);
        return us;
      }

      [[nodiscard]] size_t count() const { return samples_.size(); }
      [[nodiscard]] bool empty() const { return samples_.empty(); }

      [[nodiscard]] double mean() const {
        if (samples_.empty()) return 0.0;
        return std::accumulate(samples_.begin(), samples_.end(), 0.0) / static_cast<double>(samples_.size());
      }

      [[nodiscard]] double median() const {
        if (samples_.empty()) return 0.0;
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        const auto n = sorted.size();
        return (n % 2 == 0) ? (sorted[n/2 - 1] + sorted[n/2]) / 2.0 : sorted[n/2];
      }

      [[nodiscard]] double stddev() const {
        if (samples_.size() < 2) return 0.0;
        const double m = mean();
        double sum_sq = 0.0;
        for (const auto &s : samples_) {
          const double diff = s - m;
          sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / static_cast<double>(samples_.size() - 1));
      }

      // Difference between max and min sample
      [[nodiscard]] double jitter() const {
        if (samples_.size() < 2) return 0.0;
        auto [min_it, max_it] = std::minmax_element(samples_.begin(), samples_.end());
        return *max_it - *min_it;
      }

      // Mean frequency in Hz (assumes samples are in microseconds)
      [[nodiscard]] double hertz() const {
        const double m = mean();
        return (m > 0.0) ? 1.0e6 / m : 0.0;
      }

      // Frequency uncertainty in Hz
      [[nodiscard]] double hertz_stddev() const {
        const double m = mean();
        const double sd = stddev();
        if (m <= 0.0) return 0.0;
        return (1.0e6 * sd) / (m * m);
      }

      [[nodiscard]] std::string summary() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1)
           << "median=" << median() << " us, "
           << "mean=" << mean() << " us, "
           << "jitter=" << jitter() << " us, "
           << std::setprecision(2)
           << hertz() << " +/- " << hertz_stddev() << " Hz"
           << " (n=" << count() << ")";
        return ss.str();
      }

    private:
      std::vector<double> samples_;
  };

}
