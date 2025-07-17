/** ---------------------------------------------------------------------------
* @file     logentry.h
 * @brief    include file for logging functions
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#include "utilities.h"

extern unsigned int nextday; /// number of seconds until the next day is a global

enum class LogLevel {
  ERROR,
  WARNING,
  INFO,
  DEBUG,
  VERBOSE
};

inline const char* log_level_to_string(LogLevel level) {
  switch (level) {
  case LogLevel::ERROR:   return "ERROR";
  case LogLevel::WARNING: return "WARNING";
  case LogLevel::INFO:    return "INFO";
  case LogLevel::DEBUG:   return "DEBUG";
  case LogLevel::VERBOSE: return "VERBOSE";
  default:                return "UNKNOWN";
  }
}

inline LogLevel string_to_log_level(std::string level_str) {
  std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::toupper);

  if (level_str == "ERROR")   return LogLevel::ERROR;
  if (level_str == "WARNING") return LogLevel::WARNING;
  if (level_str == "INFO")    return LogLevel::INFO;
  if (level_str == "DEBUG")   return LogLevel::DEBUG;
  if (level_str == "VERBOSE") return LogLevel::VERBOSE;

  return LogLevel::INFO;
}

long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone, LogLevel min_level = LogLevel::INFO);

/// initialize the logging system
void close_log(); /// close the log file stream
void logwrite(const std::string &function, const std::string &message, LogLevel level);
void logwrite(const std::string &function, const std::string &message);
void set_loglevel_filter(LogLevel new_level);

/// create a time-stamped log entry "message" from "function"
