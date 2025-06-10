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

std::mutex loglock;           /// mutex to protect from multiple access
std::ofstream filestream;     /// IO stream class
unsigned int nextday = 86410; /// number of seconds until a new day
bool to_stderr = true;        /// write to stderr by default
std::string tmzone_log;       /// optional time zone for logging
std::queue<std::string> log_queue;
std::condition_variable log_cv;
std::atomic<bool> logger_running{true};
std::thread logger_thread;

/***** logger_worker **********************************************************/
/**
 * @brief      Internal thread function that processes queued log messages.
 *
 * This function runs in a separate thread and waits for new log messages
 * to be pushed into the queue. It writes messages to the log file (if open)
 * and optionally to stderr, depending on configuration.
 *
 * The thread continues to run as long as `logger_running` is true or there
 * are messages left in the `log_queue`. Once done, it flushes the stream.
 *
 */
void logger_worker()
{
    while (logger_running || !log_queue.empty())
    {
        std::unique_lock<std::mutex> lock(loglock);
        log_cv.wait(lock, []
                    { return !log_queue.empty() || !logger_running; });

        while (!log_queue.empty())
        {
            std::string msg = std::move(log_queue.front());
            log_queue.pop();
            lock.unlock();

            bool write_failed = false;

            if (filestream.is_open())
            {
                filestream << msg;
                if (filestream.fail())
                {
                    std::cerr << "ERROR: Failed to write to log file (disk full or I/O error)" << std::endl;
                    write_failed = true;
                }
            }

            if (to_stderr || write_failed)
            {
                std::cerr << msg;
            }

            lock.lock();
        }

        if (filestream.is_open())
        {
            filestream.flush();
            if (filestream.fail())
            {
                std::cerr << "ERROR: Failed to flush log file (disk full or I/O error)" << std::endl;
                logger_running = false;
            }
        }
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
long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone)
{
    const std::string function = "init_log";
    std::stringstream filename;
    std::stringstream message;
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
 * Signals the logger thread to stop by setting `logger_running` to false
 * and notifying the condition variable. Waits for the logger thread to
 * finish processing any remaining messages. Closes the log file if open.
 *
 */
void close_log()
{
    logger_running = false;
    log_cv.notify_one();
    if (logger_thread.joinable())
        logger_thread.join();
    if (filestream.is_open())
        filestream.close();
}

/***** close_log **************************************************************/

/***** logwrite ***************************************************************/
/**
 * @brief      Queues a formatted log message with a timestamp and function name.
 * @param[in]  function   Name of the calling function or context.
 * @param[in]  message    Log message to record.
 *
 * Formats the message as: "TIMESTAMP  (function) message\n" using the
 * configured time zone. Pushes the message into the logging queue in a
 * thread-safe manner and notifies the logger thread for processing.
 *
 * Create a time-stamped entry in the log file in the form of:
 * YYYY-MM-DDTHH:MM:SS.ssssss (function) message\n
 *
 */
void logwrite(const std::string &function, const std::string &message)
{
    char buffer[512];
    std::string timestamp = get_timestamp(tmzone_log);

    // snprintf for speed
    int len = snprintf(buffer, sizeof(buffer), "%s  (%s) %s\n",
                       timestamp.c_str(), function.c_str(), message.c_str());

    // but fallback to std::string for long messages
    std::string logmsg = (len>0 && len < static_cast<int>(sizeof(buffer)))
                       ? std::string(buffer, len)
                       : (timestamp + "  (" + function + ") " + message + "\n");

    // push message into the queue
    {
        std::lock_guard<std::mutex> lock(loglock);
        log_queue.push(logmsg);
    }
    log_cv.notify_one();
}

/***** logwrite ***************************************************************/
