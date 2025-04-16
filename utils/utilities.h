/**
 * @file    utilities.h
 * @brief   some handy utilities to use anywhere
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <functional>
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
#include <set>
#include <limits>
#include <ctime>
#include "time.h"
#include <dirent.h>
#include <map>
#include <json.hpp>
#include <condition_variable>
#include <initializer_list>
#include <bitset>
#include <cstdlib>
#include <arpa/inet.h>
#include <netdb.h>

#define TO_DEGREES ( 360. / 24. )
#define TO_HOURS   ( 24. / 360. )

extern std::string tmzone_cfg; /// time zone if set in cfg file
extern std::mutex generate_tmpfile_mtx;

bool cmdOptionExists(char **begin, char **end, const std::string &option);

char *getCmdOption(char **begin, char **end, const std::string &option);

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


/***** NumberPool ***********************************************************/
/**
 * @class   NumberPool
 * @brief   manages a pool of numbers
 * @details Provides function of getting the lowest available number out
 *          of a pool of numbers, and the ability to return a number back
 *          to the pool. The pool grows as needed. The pool is meant to
 *          contain positive integers and if the number would exceed the
 *          max value for int, then -1 is returned.
 *
 */
class NumberPool {
  private:
    std::mutex pool_lock;        /// protects pool access from multiple threads
    int next_number;             /// next number available
    std::set<int> used_numbers;  /// The pool is a set (for automatic sorting) which
                                 /// represents the numbers that are being used, so
                                 /// missing numbers are the available numbers.
  public:

    /**
     * Constructed with the starting number of the pool
     */
    NumberPool( int starting_number ) : next_number(starting_number) { }


    /***** NumberPool::get_next_number ****************************************/
    /**
     * @brief      gets the lowest available number from the pool
     * @details    This returns the lowest missing value in the set and works
     *             because a std::set is auto-sorted.
     * @return     positive int or -1 if pool exceeds max allowed by an int
     *
     */
    int get_next_number() {
      std::lock_guard<std::mutex> lock( pool_lock );
      int number = next_number;
      used_numbers.insert( number );
      if ( next_number < std::numeric_limits<int>::max() ) ++next_number;
      else return -1;
      return number;
    }
    /***** NumberPool::get_next_number ****************************************/


    /***** NumberPool::release_number *****************************************/
    /**
     * @brief      returns indicated number back to the pool
     * @details    numbers are "returned" by removing them from the set of used numbers
     * @param[in]  number  number to put back into pool
     *
     */
    void release_number( int number ) {
      std::lock_guard<std::mutex> lock( pool_lock );
      used_numbers.erase( number );
      if ( number < next_number ) next_number = number;
      return;
    }
    /***** NumberPool::release_number *****************************************/
};
/***** NumberPool ***********************************************************/
