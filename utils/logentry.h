/** ---------------------------------------------------------------------------
 * @file     logentry.h
 * @brief    include file for logging functions
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <chrono>
#include <thread>
#include <ctime>
#include "utilities.h"

extern unsigned int nextday; /// number of seconds until the next day is a global

long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone);

/// initialize the logging system
void close_log(); /// close the log file stream
void logwrite(std::string_view function, std::string_view message);

/// create a time-stamped log entry "message" from "function"
