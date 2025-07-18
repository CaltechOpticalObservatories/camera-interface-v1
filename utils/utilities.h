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
#include <cxxabi.h>
#include <condition_variable>

extern std::string tmzone_cfg;                      /// time zone if set in cfg file
extern std::mutex generate_tmpfile_mtx;

bool cmdOptionExists( char** begin, char** end, const std::string &option );
char* getCmdOption( char** begin, char** end, const std::string &option );
int my_hardware_concurrency();
int cores_available();

inline int mod( int k, int n ) { return ( (k %= n) < 0 ) ? k+n : k; }

unsigned int parse_val(const std::string& str);     /// returns an unsigned int from a string

int Tokenize(const std::string& str, 
             std::vector<std::string>& tokens, 
             const std::string& delimiters);        /// break a string into a vector

void Tokenize(const std::string &str, 
              std::vector<uint32_t> &devlist, 
              int &ndev, 
              std::vector<std::string> &arglist, 
              int &narg );

void chrrep(char *str, char oldchr, char newchr);   /// replace one character within a string with a new character
void string_replace_char(std::string &str, const char *oldchar, const char *newchar);

long get_time( int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec );
long get_time( std::string tmzone_in, int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec );

std::string timestamp_from( struct timespec &time_n );  /// return time from input timespec struct in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
std::string timestamp_from( const std::string &tmzone_in, struct timespec &time_n );  /// return time from input timespec struct in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
long timestamp_to_timespec( const std::string &timestamp_in, struct timespec &ts_out );
std::string timestamp_delta( struct timespec time, uint64_t buftimestamp );
std::string timestamp_delta( struct timespec time, uint64_t buftimestamp, int32_t offset );


inline std::string get_timestamp(std::string tz) {  /// return current time in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
  struct timespec timenow;
  clock_gettime( CLOCK_REALTIME, &timenow );
  return timestamp_from( tz, timenow );
}
inline std::string get_timestamp() {                /// return current time in formatted string "YYYY-MM-DDTHH:MM:SS.sss"
  return get_timestamp(tmzone_cfg);
}

std::string get_system_date();                      /// return current date in formatted string "YYYYMMDD"
std::string get_system_date(std::string tmzone_in); /// return current date in formatted string "YYYYMMDD"
std::string get_file_time();                        /// return current time in formatted string "YYYYMMDDHHMMSS" used for filenames
std::string get_file_time(std::string tmzone_in);   /// return current time in formatted string "YYYYMMDDHHMMSS" used for filenames

double get_clock_time();

inline uint64_t clock_time_nsec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

long timeout( int wholesec=0, std::string next="" );  /// wait until next integral second or minute

double mjd_from( struct timespec &time_n );         /// modified Julian date from input timespec struct

inline double mjd_now() {                           /// modified Julian date now
  struct timespec timenow;
  clock_gettime( CLOCK_REALTIME, &timenow );
  return( mjd_from( timenow ) );
}

int compare_versions(std::string v1, std::string v2);

long md5_file( const std::string &filename, std::string &hash );  /// compute md5 checksum of file

bool is_owner( const std::filesystem::path &filename );
bool has_write_permission( const std::filesystem::path &filename );
std::string_view tchar( std::string_view str );
std::string_view strip_control_characters( const std::string &str );
bool starts_with( const std::string &str, std::string_view prefix );
bool ends_with( const std::string &str, std::string_view suffix );
std::string generate_temp_filename( const std::string &prefix );


static inline void rtrim(std::string &s) {          /// trim off trailing whitespace from a string
  s.erase( std::find_if( s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); } ).base(), s.end() );
}

std::string demangle(const char* name);

inline bool caseCompareChar( char a, char b ) { return ( std::toupper(a) == std::toupper(b) ); }

inline bool caseCompareString( const std::string &s1, const std::string &s2 ) {
  return( (s1.size()==s2.size() ) && std::equal( s1.begin(), s1.end(), s2.begin(), caseCompareChar) ); }

inline double timespec_to_us(const timespec &ts) {
  return static_cast<double>(ts.tv_sec)*1000000.0+static_cast<double>(ts.tv_nsec)/1000.0;
}

inline timespec us_to_timespec(double usec) {
  timespec ts;
  ts.tv_sec  = static_cast<time_t>(usec / 1000000.0);
  double fractional_us = usec - static_cast<double>(ts.tv_sec)*1000000.0;
  ts.tv_nsec = static_cast<long>(fractional_us * 1000.0);
  return ts;
}

inline timespec timespec_avg(const timespec &t1, const timespec &t2) {
  double t1_us = timespec_to_us(t1);
  double t2_us = timespec_to_us(t2);
  double avg_us = 0.5 * (t1_us+t2_us);
  return us_to_timespec(avg_us);
}

inline timespec timespec_diff(const timespec &t1, const timespec &t2) {
  double t1_us = timespec_to_us(t1);
  double t2_us = timespec_to_us(t2);
  return us_to_timespec(t1_us-t2_us);
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
template <typename T>
std::string to_string_prec( const T value_in, const int prec = 6 ) {
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
    std::atomic<bool> _locked;        // track whether the mutex is locked
    std::atomic<bool> _run;

    inline void _lock() { _mut.lock(); _locked = true; }       // lock mutex

    inline void _unlock() { _locked = false; _mut.unlock(); }  // unlock mutex

  public:
    // lock on creation
    //
    InterruptableSleepTimer() {
      _lock();
      _run = true;
    }

    // unlock on destruction, if wake was never called
    //
    ~InterruptableSleepTimer() {
      if ( _locked ) {
        _unlock();
        _run = false;
      }
    }

    inline bool running() { return _run; }

    // called by any thread except the creator
    // waits until wake is called or the specified time passes
    //
    template< class Rep, class Period >
    void sleepFor( const std::chrono::duration<Rep,Period> &timeout_duration ) {
      if ( _run && _mut.try_lock_for( timeout_duration ) ) {
        // if successfully locked, remove the lock
        //
        _mut.unlock();
      }
    }

    // unblock any waiting threads, handling a situation
    // where wake has already been called.
    // should only be called by the creating thread
    //
    inline void stop() {
      if ( _locked ) {
        _run = false;
        _unlock();
      }
    }
    inline void start() {
      if ( ! _locked ) {
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
      struct timespec timenow;
      clock_gettime( CLOCK_REALTIME, &timenow );
      return timenow;
    }
};
/***** Time *****************************************************************/


/***** PreciseTimer ***********************************************************/
/**
 * @class   PreciseTimer
 * @brief   creates an interruptable, precise sleep timer object
 * @details This is a utility class to create a reasonably accurate
 *          interruptable sleep timer (precision approx 100 microseconds).
 *          This is done by making repeated calls to precise_sleep(),
 *          while checking for a cancel flag. The timer can also be
 *          modified.
 *
 *          Units are in microseconds except for the inputs, delay(ms) and
 *          modify(ms), and outputs, get_remaining() and stop(&ms), which
 *          are in milliseconds.
 */
class PreciseTimer {
  private:
    static const long max_short_sleep = 3000000;         // units are microseconds
    static const long busy_wait_threshold = 100;
    static const long overhead_compensation = 5;

    std::atomic<bool> should_hold;
    std::atomic<bool> on_hold;
    std::atomic<bool> should_stop;
    std::atomic<bool> running;
    std::atomic<long> delay_time;
    std::atomic<long> remaining_time;
    mutable std::mutex mtx;
    std::condition_variable cv;

    /***** PreciseTimer::busy_wait ********************************************/
    /** @brief  This is a flat-out loop of polling clock_gettime until        */
    /**         elapsed has occured. Use only for the very end!               */
    void busy_wait(long microseconds) {
      struct timespec start, current;
      clock_gettime(CLOCK_MONOTONIC, &start);
      while (true) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        long elapsed = (current.tv_sec - start.tv_sec) * 1000000 +
                       (current.tv_nsec - start.tv_nsec) / 1000;
        if (elapsed >= microseconds) break;
      }
    }

    /***** PreciseTimer::precise_sleep ****************************************/
    /** @brief  This is the inner loop which sleeps for a short time,         */
    /**         specified in microseconds. It cannot be interrupted.          */
    void precise_sleep(long microseconds) {
      struct timespec start_time;
      struct timespec current_time;
      struct timespec ts;

      // precise-loop remaining time starts as requested time
      //
      long _remaining_time = microseconds - overhead_compensation;

      // Loop until the total elapsed time reaches the requested sleep time
      //
      clock_gettime(CLOCK_MONOTONIC, &start_time);  // start time
      while ( _remaining_time > 0 ) {

        // Calculate remaining time
        //
        ts.tv_sec = _remaining_time / 1000000;
        ts.tv_nsec = (_remaining_time % 1000000) * 1000;

        if ( _remaining_time > busy_wait_threshold ) {

          // Sleep for the remaining time
          //
          clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);

          // check current time and calculate elapsed and remaining time
          //
          clock_gettime(CLOCK_MONOTONIC, &current_time);
          long elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                              (current_time.tv_nsec - start_time.tv_nsec) / 1000;
          _remaining_time = microseconds - elapsed_time;
        }
        else busy_wait(_remaining_time);
      }
    }

    /***** PreciseTimer::long_precise_sleep ***********************************/
    /** @brief  This is the main delay loop which can be interrupted.         */
    void long_precise_sleep() {
      // max_short_sleep is the largest sleep time for precise_sleep()
      // This should be kept small, a few seconds or less.
      //
      struct timespec start_time;
      struct timespec current_time;
      struct timespec hold_start, hold_stop;
      long hold_time=0;

      running.store(true, std::memory_order_release);

      // total remaining time starts as requested time
      //
      remaining_time.store( delay_time.load(std::memory_order_acquire), std::memory_order_release );

      // loop forever until broken
      //
      clock_gettime(CLOCK_MONOTONIC, &start_time);  // start time
      while ( true ) {
        // sleep for the shorter of max_short_sleep or remaining time
        //
        long to_sleep = std::min( remaining_time.load(std::memory_order_acquire), max_short_sleep );
        precise_sleep(to_sleep);

        // Calculate elapsed time and time remaining.
        // Elapsed time is difference between time now and start time,
        // less any time that the timer was on hold.
        //
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                            (current_time.tv_nsec - start_time.tv_nsec) / 1000  -
                            hold_time;

        remaining_time.store( (delay_time.load(std::memory_order_acquire) - elapsed_time),
                              std::memory_order_release );

        if ( remaining_time.load(std::memory_order_acquire) <= 0 ) break;    // break when time is up

        if (should_stop.load(std::memory_order_acquire)) break;              // break if stop requested

        // When hold is requested, store the hold start time, send
        // notification that loop is on hold, and wait for request
        // to either release hold or stop.
        //
        if (should_hold.load(std::memory_order_acquire)) {
          {
          std::unique_lock<std::mutex> lock(mtx);
          on_hold.store(true, std::memory_order_release);
          cv.notify_all();
          }
          clock_gettime(CLOCK_MONOTONIC, &hold_start);
          {
          std::unique_lock<std::mutex> lock(mtx);
          cv.wait( lock, [this]() { return ( should_stop.load(std::memory_order_acquire) ||
                                            !should_hold.load(std::memory_order_acquire) ); } );
          }

          if (should_stop.load(std::memory_order_acquire)) break;            // break if stop requested

          // how long was loop on hold, in microseconds
          clock_gettime(CLOCK_MONOTONIC, &hold_stop);
          hold_time = (hold_stop.tv_sec-hold_start.tv_sec)*1000000 +
                      (hold_stop.tv_nsec-hold_stop.tv_nsec)/1000;
          on_hold.store(false, std::memory_order_release);
        }
      }
      running.store(false, std::memory_order_release);
      cv.notify_all();
    }

  public:
    PreciseTimer() { reset(); }

    /***** PreciseTimer::delay ************************************************/
    /** @brief  This is the entry point to create a blocking delay, which     */
    /**         won't return until milliseconds have elapsed unless stopped   */
    /**         or modified.                                                  */
    long delay(long milliseconds) {
      if (running.load(std::memory_order_acquire)) {
        std::cerr << get_timestamp() << "  (PreciseTimer::delay) cannot start new timer while another is running\n";
        return 1;
      }
      reset();
      delay_time.store(1000*milliseconds);
      long_precise_sleep();
      return 0;
    }

    /***** PreciseTimer::delay_until ******************************************/
    /** @brief  Delay for milliseconds elapsed since provided reference time. */
    long delay_until(const struct timespec &reftime, long milliseconds_since_ref) {
      struct timespec timenow;
      clock_gettime(CLOCK_MONOTONIC, &timenow);

      // calculate elapsed time since reference
      long elapsed_since_ref = (timenow.tv_sec - reftime.tv_sec) * 1000 +
                               (timenow.tv_nsec - reftime.tv_nsec) / 1000000;

      // calculate remaining time to wait
      long waitms = milliseconds_since_ref - elapsed_since_ref;

      return delay(waitms);
    }

    /***** PreciseTimer::get_remaining ****************************************/
    /** @brief  Returns the time remaining in milliseconds                    */
    long get_remaining() { return remaining_time.load(std::memory_order_acquire)/1000; }

    /***** PreciseTimer::modify ***********************************************/
    /** @brief  Modifies the delay time to new value in milliseconds          */
    void modify(long milliseconds) { delay_time.store(milliseconds*1000, std::memory_order_release); }

    /***** PreciseTimer::progress *********************************************/
    /** @brief  Returns by reference remaining and delay times in msec        */
    void progress(long &remaintime, long &delaytime) {
      remaintime = ( !running.load(std::memory_order_acquire) ? 0 :
                     remaining_time.load(std::memory_order_acquire)/1000 );
      delaytime  = delay_time.load(std::memory_order_acquire)/1000;
    }

    /***** PreciseTimer::hold *************************************************/
    /** @brief  Hold/pause the delay timer at the next short-sleep boundary   */
    void hold() {
      std::unique_lock<std::mutex> lock(mtx);
      // request hold
      should_hold.store(true, std::memory_order_release);
      // wait until the delay loop says it is on hold
      if (on_hold.load(std::memory_order_acquire)) return;
      cv.wait( lock, [this]() { return on_hold.load(std::memory_order_acquire); } );
    }

    /***** PreciseTimer::resume ***********************************************/
    /** @brief  Removes the hold and resumes the delay after a hold           */
    void resume() {
      std::unique_lock<std::mutex> lock(mtx);
      should_hold.store(false, std::memory_order_release);
      cv.notify_all();
    }

    /***** PreciseTimer::stop *************************************************/
    /** @brief  Stops the delay at the next short-sleep boundary              */
    void stop() {
     long ms;
     stop(ms);
    }

    /***** PreciseTimer::stop *************************************************/
    /** @brief  Stops the delay at the next short-sleep boundary, returns     */
    /**         remaining time in milliseconds by reference.                  */
    void stop(long &milliseconds) {
      {
      std::unique_lock<std::mutex> lock(mtx);
      should_stop.store(true, std::memory_order_release);  // tells the main loop to stop
      cv.notify_all();
      }
      if ( running.load(std::memory_order_acquire) ) {
        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, std::chrono::microseconds(2*max_short_sleep),
            [this]() { return !running.load(std::memory_order_acquire); })) {
          std::cerr << get_timestamp() << "  (PreciseTimer::stop) *** timed out waiting for loop to stop ***\n";
        }
      }
      milliseconds=remaining_time.load(std::memory_order_acquire)/1000;
      reset();
    }

    /***** PreciseTimer::reset ************************************************/
    /** @brief  For internal use only, resets class variables                 */
    void reset() {
      should_hold.store(false);
      on_hold.store(false);
      should_stop.store(false);
      running.store(false);
      delay_time.store(0);
      remaining_time.store(0);
    }
};
/***** PreciseTimer ***********************************************************/
