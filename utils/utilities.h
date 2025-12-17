/**
 * @file    utilities.h
 * @brief   some handy utilities to use anywhere
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <iomanip>
#include <vector>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <iostream>  // for istream
#include <fstream>   // for ifstream
#include <iterator>  // for istream_iterator
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <cmath>
#include <sys/stat.h>
#include "md5.h"
#include <string>
#include <string_view>
#include <cctype>

extern std::string tmzone_cfg; /// time zone if set in cfg file
extern std::mutex generate_tmpfile_mtx;

std::string getOptionArg(int argc, char** argv, const std::string &option);

bool hasOption(int argc, char** argv, const std::string &option);

int my_hardware_concurrency();

int cores_available();

inline int mod(int k, int n) { return ((k %= n) < 0) ? k + n : k; }

unsigned int parse_val(const std::string &str); /// returns an unsigned int from a string

int Tokenize(const std::string &str,
             std::vector<std::string> &tokens,
             const std::string &delimiters); /// break a string into a vector

void Tokenize(const std::string &str,
              std::vector<uint32_t> &devlist,
              int &ndev,
              std::vector<std::string> &arglist,
              int &narg);

void chrrep(char *str, char oldchr, char newchr); /// replace one character within a string with a new character
void string_replace_char(std::string &str, const char *oldchar, const char *newchar);

long get_time(int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec);

long get_time(const std::string &tmzone_in, int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec);

std::string timestamp_from(struct timespec &time_n);

/// return time from input timespec struct in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
std::string timestamp_from(const std::string &tmzone_in, struct timespec &time_in);


inline std::string get_timestamp(const std::string &tz) {
  /// return current time in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
  struct timespec timenow{};
  clock_gettime(CLOCK_REALTIME, &timenow);
  return timestamp_from(tz, timenow);
}

inline std::string get_timestamp() {
  /// return current time in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
  return get_timestamp(tmzone_cfg);
}

std::string get_system_date(); /// return current date in formatted string "YYYYMMDD"
std::string get_system_date(const std::string &tmzone_in);

std::string get_file_time(); /// return current time in formatted string "YYYYMMDDHHMMSS" used for filenames
std::string get_file_time(const std::string &tmzone_in);


double get_clock_time();

inline uint64_t clock_time_nsec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

long timeout(int wholesec = 0, const std::string &next = ""); /// wait until next integral second or minute

double mjd_from(struct timespec &time_n); /// modified Julian date from input timespec struct

inline double mjd_now() {
  /// modified Julian date now
  timespec timenow{};
  clock_gettime(CLOCK_REALTIME, &timenow);
  return (mjd_from(timenow));
}

int compare_versions(const std::string &v1, const std::string &v2);

long md5_file(const std::string &filename, std::string &hash); /// compute md5 checksum of file

bool is_owner(const std::filesystem::path &filename);

bool has_write_permission(const std::filesystem::path &filename);

const std::string &tchar(const std::string &str);

const std::string strip_newline( const std::string &str_in );

std::string strip_control_characters(const std::string &str);

void strip_spaces(std::string &str);

bool starts_with(const std::string &str, std::string_view prefix);

bool ends_with(const std::string &str, std::string_view suffix);

std::string generate_temp_filename(const std::string &prefix);


void rtrim(std::string &s);

inline bool caseCompareChar(char a, char b) { return (std::toupper(a) == std::toupper(b)); }

inline bool caseCompareString(const std::string &s1, const std::string &s2) {
  return ((s1.size() == s2.size()) && std::equal(s1.begin(), s1.end(), s2.begin(), caseCompareChar));
}


/***** to_string_prec *******************************************************/
/**
 * @brief      convert a numeric value to a string with specified precision
 * @details    Since std::to_string doesn't allow changing the precision,
 *             I wrote my own equivalent. Probably don't want to use this
 *             in a tight loop.
 * @param[in]  value_in  numeric value in of type <T>
 * @param[in]  prec      desired precision, default=6
 * @return     string
 *
 */
template<typename T>
std::string to_string_prec(const T value_in, const int prec = 6) {
  std::ostringstream out;
  out.precision(prec);
  out << std::fixed << value_in;
  return std::move(out).str();
}

/***** to_string_prec *******************************************************/


/***** InterruptableSleepTimer***********************************************/
/**
 * @class   InterruptableSleepTimer
 * @brief   creates a sleep timer that can be interrupted.
 * @details This class uses try_lock_for in order to put a thread to sleep,
 *          while allowing it to be woken up early.
 *
 */
class InterruptableSleepTimer {
private:
  std::timed_mutex _mut;
  std::atomic<bool> _locked{}; // track whether the mutex is locked
  std::atomic<bool> _run;

  void _lock() {
    _mut.lock();
    _locked = true;
  } // lock mutex

  void _unlock() {
    _locked = false;
    _mut.unlock();
  } // unlock mutex

public:
  // lock on creation
  //
  InterruptableSleepTimer() : _run(true) {
    _lock();
  }

  // unlock on destruction, if wake was never called
  //
  ~InterruptableSleepTimer() {
    if (_locked) {
      _unlock();
      _run = false;
    }
  }

  bool running() { return _run; }

  // called by any thread except the creator
  // waits until wake is called or the specified time passes
  //
  template<class Rep, class Period>
  void sleepFor(const std::chrono::duration<Rep, Period> &timeout_duration) {
    if (_run && _mut.try_lock_for(timeout_duration)) {
      // if successfully locked, remove the lock
      //
      _mut.unlock();
    }
  }

  // unblock any waiting threads, handling a situation
  // where wake has already been called.
  // should only be called by the creating thread
  //
  void stop() {
    if (_locked) {
      _run = false;
      _unlock();
    }
  }

  void start() {
    if (!_locked) {
      _lock();
      _run = true;
    }
  }
};

/***** InterruptableSleepTimer **********************************************/


/***** Time *****************************************************************/
/**
 * @class   Time
 * @brief   encapsulates the logic of getting current time into timespec struct
 * @details static function getTimeNow allows calling without creating an
 *          instance of the class.
 *
 */
class Time {
public:
  static timespec getTimeNow() {
    timespec timenow{};
    clock_gettime(CLOCK_REALTIME, &timenow);
    return timenow;
  }
};

/***** Time *****************************************************************/
