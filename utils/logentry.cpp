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

#include <pthread.h>

std::mutex loglock; /// mutex to protect from multiple access
std::ofstream filestream; /// IO stream class
unsigned int nextday = 86410; /// number of seconds until a new day
bool to_stderr = true; /// write to stderr by default
std::string tmzone_log; /// optional time zone for logging

// Non-blocking log queue — logwrite() pushes here, logger_worker drains to file/stderr
std::queue<std::string> log_queue;
std::condition_variable log_cv;
std::atomic<bool> logger_running{false};
std::thread logger_thread;


/***** logger_worker **********************************************************/
/**
 * @brief      background thread that drains the log queue to file and stderr
 *
 */
static void logger_worker() {
#ifdef __linux__
    pthread_setname_np(pthread_self(), "camerad:log");
#elif defined(__APPLE__)
    pthread_setname_np("camerad:log");
#endif

    std::unique_lock<std::mutex> lock(loglock);
    while (logger_running || !log_queue.empty()) {
        log_cv.wait(lock, [] { return !log_queue.empty() || !logger_running; });

        while (!log_queue.empty()) {
            std::string msg = std::move(log_queue.front());
            log_queue.pop();
            lock.unlock();

            if (filestream.is_open()) filestream << msg;
            if (to_stderr) std::cerr << msg;

            lock.lock();
        }
        if (filestream.is_open()) filestream.flush();
    }
}
/***** logger_worker **********************************************************/


/***** init_log ***************************************************************/
/**
 * @brief      initializes the logging
 * @param[in]  name       name of the log file in logpath
 * @param[in]  logpath    path for log file
 * @param[in]  logstderr  should logwrite also print to stderr?
 * @param[in]  logtmzone  optional time zone for logging
 * @return     0 on success, 1 on error
 *
 * Opens an ofstream to the specified logfile, "logpath/name_YYYYMMDD.log"
 * where logpath and name are passed in as parameters.
 *
 */
long init_log(std::string name, std::string logpath, std::string logstderr, std::string logtmzone) {
    const std::string function = "init_log";
    std::stringstream filename;
    std::stringstream message;
    int year, mon, mday, hour, min, sec, usec;
    long error = 0;

    to_stderr = (logstderr == "false" ? false : true); // should logwrite also print to stderr?

    tmzone_log = logtmzone;

    if ((error = get_time(tmzone_log, year, mon, mday, hour, min, sec, usec))) return error;

    // assemble log file name from the passed-in arguments and current date
    //
    filename << logpath << "/" << name << "_" << std::setfill('0')
            << std::setw(4) << year
            << std::setw(2) << mon
            << std::setw(2) << mday << ".log";

    // number of seconds until a new day
    //
    nextday = (unsigned int) (86410 - hour * 3600 - min * 60 - sec);

    // open the log file stream for append
    //
    try {
        filestream.open(filename.str(), std::ios_base::app);
    } catch (const std::filesystem::filesystem_error &e) {
        message.str("");
        message << "ERROR " << filename.str() << ": " << e.what() << ": " << e.code().value();
        logwrite(function, message.str());
        return 1;
    }
    catch (...) {
        message.str("");
        message << "ERROR: opening log file " << filename.str() << ": " << std::strerror(errno);
        logwrite(function, message.str());
        return 1;
    }

    // If I am the owner then make sure the permissions are set correctly.
    // Only the owner can change permissions. Even if I have write access,
    // if I'm not the owner then I can't change the permissions.
    //
    if (is_owner(filename.str())) {
        try {
            // remove all permissions
            //
            std::filesystem::permissions(filename.str(),
                                         std::filesystem::perms::all,
                                         std::filesystem::perm_options::remove
            );

            // add back permissions rw-rw-r-- (664)
            //
            std::filesystem::permissions(filename.str(),
                                         std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write |
                                         std::filesystem::perms::group_read |
                                         std::filesystem::perms::group_write |
                                         std::filesystem::perms::others_read,
                                         std::filesystem::perm_options::add
            );
        } catch (const std::filesystem::filesystem_error &e) {
            message.str("");
            message << "ERROR " << filename.str() << ": " << e.what() << ": " << e.code().value();
            logwrite(function, message.str());
            return 1;
        }
    }

    // If I do not have write permission then that is a fatal error
    //
    if (!has_write_permission(filename.str())) {
        message.str("");
        message << "ERROR: no write permission for log file " << filename.str();
        logwrite(function, message.str());
        return 1;
    }

    if (!filestream.is_open()) {
        message.str("");
        message << "ERROR: log file " << filename.str() << " not open";
        logwrite(function, message.str());
        return 1;
    }

    // Start the background log writer thread
    logger_running = true;
    logger_thread = std::thread(logger_worker);

    return 0;
}

/***** init_log ***************************************************************/


/***** close_log **************************************************************/
/**
 * @brief      closes the logfile stream
 * @details    Call this from the main class deconstructor to clean up the log file.
 *
 */
void close_log() {
    logger_running = false;
    log_cv.notify_one();
    if (logger_thread.joinable()) logger_thread.join();
    if (filestream.is_open()) filestream.close();
}

/***** close_log **************************************************************/


/***** logwrite ***************************************************************/
/**
 * @brief      create a log file entry
 * @param[in]  function  string containing the Namespace::Class::function
 * @param[in]  message   string to log
 *
 * Create a time-stamped entry in the log file in the form of:
 * YYYY-MM-DDTHH:MM:SS.ssssss (function) message\n
 *
 * This function can also be used for logging to stderr, even if the
 * log filestream isn't open.
 *
 */
void logwrite(const std::string &function, const std::string &message) {
    std::stringstream logmsg;
    std::string timestamp = get_timestamp(tmzone_log); // get the current time (defined in utilities.h)
    logmsg << timestamp << "  (" << function << ") " << message << "\n";

    {
        std::lock_guard<std::mutex> lock(loglock);
        log_queue.push(logmsg.str());
    }
    log_cv.notify_one();
}

/***** logwrite ***************************************************************/
