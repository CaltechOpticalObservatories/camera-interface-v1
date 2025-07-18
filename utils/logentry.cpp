/**
 * @file    logentry.cpp
 * @brief   logentry functions
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Write time-stamped entries to log file using logwrite(function, message)
 * where function is a string representing the calling function and
 * message is an arbitrary string.
 *
 */

#include "logentry.h"

std::ofstream filestream;     /// IO stream class
unsigned int nextday = 86410; /// number of seconds until a new day
bool to_stderr = false;        /// write to stderr by default
std::string tmzone_log;       /// optional time zone for logging
struct LogItem {
    std::string* msg;
    LogLevel level;
};
boost::lockfree::queue<LogItem*> log_queue(1024);
std::atomic<bool> logger_running{true};
std::thread logger_thread;
std::atomic<LogLevel> global_loglevel = LogLevel::INFO;

/***** logger_worker **********************************************************/
/**
 * @brief      Internal thread function that processes queued log messages.
 *
 * This function runs in a dedicated thread and continuously attempts to
 * pop messages from a lock-free Boost queue (`log_queue`). Each message
 * is written to the log file if open, and optionally to stderr depending
 * on configuration or write failure.
 *
 * The thread continues running as long as `logger_running` is true or
 * there are pending messages in the queue. After processing all messages,
 * it flushes the output stream.
 *
 * Messages are heap-allocated by the producer and must be freed after
 * processing to avoid memory leaks.
 *
 * This implementation is fully non-blocking on the producer side and avoids
 * the use of mutexes or condition variables for queue access.
 *
 */
void logger_worker() {
    LogItem* item;
    while (logger_running || !log_queue.empty()) {
        while (log_queue.pop(item)) {
            bool write_failed = false;

            if (filestream.is_open()) {
                filestream << *(item->msg);
                if (filestream.fail()) {
                    std::cerr << "ERROR: Failed to write to log file (disk full or I/O error)" << std::endl;
                    write_failed = true;
                }
            }

            if (item->level == LogLevel::ERROR || write_failed) {
                std::cerr << *(item->msg);
            } else {
                std::cout << *(item->msg);
            }

            delete item->msg;
            delete item;
        }

        if (filestream.is_open()) {
            filestream.flush();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // slight delay to reduce CPU usage
    }
}
/***** logger_worker ***************************************************************/

/***** init_log ***************************************************************/
/**
 * @brief      Initializes the logging system and starts the logger thread.
 * @param[in]  name        Base name of the log file.
 * @param[in]  logpath     Directory path where the log file will be created.
 * @param[in]  logstderr   If not "false", enables logging to stderr.
 * @param[in]  logtmzone   Time zone to use for timestamps in the log.
 * @return     0 on success, 1 on error.
 *
 * Constructs a filename of the form "logpath/name_YYYYMMDD.log" using the
 * current date from the specified time zone. Opens an appendable ofstream
 * to this file and ensures proper file permissions if the process owns it.
 * Verifies that the file is writable. If all checks succeed, starts the
 * background logger thread that processes queued log messages.
 *
 */
long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone, LogLevel min_level)
{
    const std::string function = "init_log";
    std::stringstream filename;
    std::stringstream message;
    global_loglevel = min_level;
    int year, mon, mday, hour, min, sec, usec;
    long error = 0;

    to_stderr = (logstderr != "false");
    tmzone_log = logtmzone;

    if ((error = get_time(tmzone_log, year, mon, mday, hour, min, sec, usec)))
        return error;

    filename << logpath << "/" << name << "_" << std::setfill('0')
             << std::setw(4) << year << std::setw(2) << mon << std::setw(2) << mday << ".log";

    nextday = static_cast<unsigned int>(86410 - hour * 3600 - min * 60 - sec);

    try
    {
        filestream.open(filename.str(), std::ios_base::app);
    }
    catch (...)
    {
        std::cerr << "ERROR: opening log file failed\n";
        return 1;
    }

    if (is_owner(filename.str()))
    {
        try
        {
            std::filesystem::permissions(filename.str(), std::filesystem::perms::all,
                                         std::filesystem::perm_options::remove);
            std::filesystem::permissions(filename.str(),
                                         std::filesystem::perms::owner_read |
                                             std::filesystem::perms::owner_write |
                                             std::filesystem::perms::group_read |
                                             std::filesystem::perms::group_write |
                                             std::filesystem::perms::others_read,
                                         std::filesystem::perm_options::add);
        }
        catch (...)
        {
            std::cerr << "ERROR: setting permissions failed\n";
            return 1;
        }
    }

    if (!has_write_permission(filename.str()) || !filestream.is_open())
    {
        std::cerr << "ERROR: no write permission or file not open\n";
        return 1;
    }

    logger_running = true;
    logger_thread = std::thread(logger_worker);
    return 0;
}

/***** init_log ***************************************************************/

/***** close_log **************************************************************/
/**
 * @brief      Shuts down the logging system and closes the log file.
 *
 * Sets `logger_running` to false to signal the logger thread to stop.
 * Waits for the thread to finish processing any remaining messages in
 * the log queue. Once the logger thread has exited, the log file stream
 * is closed if it was open.
 * All remaining log messages in the queue are flushed before shutdown.
 *
 */
void close_log() {
    logger_running = false;
    if (logger_thread.joinable())
        logger_thread.join();
    if (filestream.is_open())
        filestream.close();
}


/***** close_log **************************************************************/

/***** set_loglevel ************************************************************/
/**
 * @brief      Sets the global log level threshold.
 * @param[in]  new_level   The minimum LogLevel to allow logging.
 *
 * This function updates the global logging threshold used by `logwrite()`
 * to decide whether a message should be queued for logging. Any log message
 * with a level higher (less severe) than this threshold will be ignored.
 *
 * Example usage:
 *   set_loglevel(LogLevel::WARNING);  // Log only WARNING and ERROR
 *
 * Thread-safe due to use of std::atomic.
 */
void set_loglevel_filter(LogLevel new_level) {
    global_loglevel.store(new_level);
}
/***** set_loglevel ************************************************************/

/***** logwrite ***************************************************************/
/**
 * @brief      Queues a formatted log message with a timestamp and function name.
 * @param[in]  function   Name of the calling function or context.
 * @param[in]  message    Log message to record.
 * @param[in]  level      LogLevel (e.g. ERROR, INFO, DEBUG).
 *
 * Formats the message as:
 *   "YYYY-MM-DDTHH:MM:SS.ssssss  (function) message\n"
 * using the configured time zone.
 *
 * Allocates the formatted string on the heap and pushes a pointer to it
 * into a Boost lock-free queue (`log_queue`). If the queue is full,
 * the function spin-waits briefly until the message is accepted.
 *
 */
void logwrite(const std::string &function, const std::string &message, LogLevel level) {
    if (level > global_loglevel.load()) return;  // filter based on global threshold

    char buffer[512];
    std::string timestamp = get_timestamp(tmzone_log);
    const char* level_str = log_level_to_string(level);

    int len = snprintf(buffer, sizeof(buffer), "%s  [%s] (%s) %s\n",
                       timestamp.c_str(), level_str, function.c_str(), message.c_str());

    std::string* logmsg = new std::string(
    (len > 0 && len < static_cast<int>(sizeof(buffer)))
        ? std::string(buffer, len)
        : timestamp + "  [" + level_str + "] (" + function + ") " + message + "\n");

    LogItem* item = new LogItem{ logmsg, level };

    while (!log_queue.push(item)) {
        std::this_thread::yield();  // brief spin-wait if queue is full
    }
}
void logwrite(const std::string &function, const std::string &message) {
    logwrite(function, message, LogLevel::INFO);
}
/***** logwrite ***************************************************************/
