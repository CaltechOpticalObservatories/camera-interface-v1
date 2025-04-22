/**
 * @file    utilities.cpp
 * @brief   some handy utilities to use anywhere
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "utilities.h"

std::string tmzone_cfg; ///< time zone if set in cfg file
std::mutex generate_tmpfile_mtx;

/***** cmdOptionExists ******************************************************/
/**
 * @brief      returns true if option is found in argv
 * @param[in]  begin       this is the beginning, should be argv
 * @param[in]  end         this is the end, should be argv+argc
 * @param[out] option      string to search for
 * @return     true|false  if string was found
 *
 * Intended to be called as cmdOptionExists( argv, argv+argc, "-X" )
 * to search for "-X" option in argv.
 *
 * Pair with getCmdOption() function.
 *
 */
bool cmdOptionExists(char **begin, char **end, const std::string &option) {
    return std::find(begin, end, option) != end;
}

/***** cmdOptionExists ******************************************************/


/***** getCmdOption *********************************************************/
/**
 * @brief      returns pointer to command line option specified with "-X option"
 * @param[in]  begin   this is the beginning, should be argv
 * @param[in]  end     this is the end, should be argv+argc
 * @param[out] option  string to search for
 * @return     char*   pointer to the option found
 *
 * Intended to be called as char* option = getCmdOption( argv, argv+argc, "-X" );
 * to get option associated with "-X option" in argv.
 *
 * Pair with cmdOptionExists()
 *
 */
char *getCmdOption(char **begin, char **end, const std::string &option) {
    char **itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) {
        return *itr;
    }
    return nullptr;
}

/***** getCmdOption *********************************************************/


/***** my_hardware_concurrency **********************************************/
/**
 * @brief      return number of concurrent threads supported by the implementation
 * @return     integer number of processors listed in /proc/cpuinfo
 *
 * Counts the number of processors listed in /proc/cpuinfo
 *
 */
int my_hardware_concurrency() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    return static_cast<int>(std::count(std::istream_iterator<std::string>(cpuinfo),
                                       std::istream_iterator<std::string>(),
                                       std::string("processor")));
}

/***** my_hardware_concurrency **********************************************/


/***** cores_available ******************************************************/
/**
 * @brief      return number of concurrent threads supported by the implementation
 * @return     integer number of cores
 *
 * If the value is not known then check /proc/cpuinfo
 *
 */
int cores_available() {
    int cores = static_cast<int>(std::thread::hardware_concurrency());
    return cores ? cores : my_hardware_concurrency();
}

/***** cores_available ******************************************************/


/***** parse_val ************************************************************/
/**
 * @brief      returns an unsigned int from a string
 * @param[in]  str  string to convert to uint
 * @return     unsigned int form of string
 *
 */
unsigned int parse_val(const std::string &str) {
    std::stringstream v;
    unsigned int u = 0;
    if ((str.find("0x") != std::string::npos) || (str.find("0X") != std::string::npos))
        v << std::hex << str;
    else
        v << std::dec << str;
    v >> u;
    return u;
}

/***** parse_val ************************************************************/


/***** Tokenize *************************************************************/
/**
 * @brief      break a string into a vector
 * @param[in]  str         input string
 * @param[out] tokens      vector of tokens
 * @param[in]  delimiters  delimiters
 * @return     number of tokens
 *
 */
int Tokenize(const std::string &str, std::vector<std::string> &tokens, const std::string &delimiters) {
    // Clear the tokens vector, we only want to putput new tokens
    tokens.clear();

    // If the string is zero length, return now with no tokens
    if (str.empty()) { return (0); }

    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // Find first "non-delimiter".
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

    std::string quote("\"");
    unsigned int quote_start = str.find(quote); //finds first quote mark
    bool quotes_found = false;

    if (quote_start <= str.length()) {
    } else {
        quote_start = -1;
    }

    while (std::string::npos != pos || std::string::npos != lastPos) {
        if (quotes_found) {
            tokens.push_back(str.substr(lastPos + 1, pos - lastPos - 2));
            pos++;
            lastPos = str.find_first_not_of(delimiters, pos);
            quotes_found = false;
        } else {
            // Found a token, add it to the vector.
            tokens.push_back(str.substr(lastPos, pos - lastPos));

            // Skip delimiters.  Note the "not_of"
            lastPos = str.find_first_not_of(delimiters, pos);
        }

        // If the next character is a quote, grab between the quotes
        if (std::string::npos != lastPos && lastPos == quote_start) {
            pos = str.find_first_of('\"', lastPos + 1) + 1;
            quotes_found = true;
        }
        // Otherwise, find next "non-delimiter"
        else {
            pos = str.find_first_of(delimiters, lastPos);
        }
    }
    return static_cast<int>(tokens.size());
}

/***** Tokenize *************************************************************/


/***** Tokenize *************************************************************/
/**
 * @brief      break a string into device list and arg list vectors
 * @param[in]  str      reference to input string
 * @param[in]  devlist  reference to vector for device list
 * @param[in]  ndev     reference to number of devices
 * @param[out] arglist  reference to vector for arg list
 * @param[in]  narg     reference to number of args
 *
 * This is a special version of Tokenize.
 *
 * The expected format is a comma-delimited device list, followed by a colon,
 * followed by a space-delimited argument list.
 *
 * On error, ndev is set to -1
 *
 */
void Tokenize(const std::string &str, std::vector<uint32_t> &devlist, int &ndev, std::vector<std::string> &arglist,
              int &narg) {
    devlist.clear();
    ndev = 0; // empty the dev and arg list vectors
    arglist.clear();
    narg = 0;

    std::size_t devdelim = str.find(':'); // position of device delimiter

    // If there is a device delimiter then build a vector of the device numbers
    //
    if (devdelim != std::string::npos) {
        std::string dev_str = str.substr(0, str.find(':'));
        std::vector<std::string> tokens;
        Tokenize(dev_str, tokens, ","); // Tokenize the dev string on the comma ","
        for (const auto &tok: tokens) {
            try {
                devlist.push_back(std::stoi(tok));
            } catch (std::invalid_argument &) {
                ndev = -1;
                return;
            }
            catch (std::out_of_range &) {
                ndev = -1;
                return;
            }
        }
        ndev = static_cast<int>(devlist.size());
    }

    // Anything left, look for space-delimited tokens for the arg list
    //
    std::string arg_str = str.substr(devdelim + 1);
    std::vector<std::string> tokens;
    Tokenize(arg_str, tokens, " "); // Tokenize the arg string on the space " "
    for (const auto &tok: tokens) {
        arglist.push_back(tok);
    }
    narg = static_cast<int>(arglist.size());
}

/***** Tokenize *************************************************************/


/***** chrrep ***************************************************************/
/**
 * @brief      replace one character within a string with a new character
 * @param[out] str     pointer to string
 * @param[in]  oldchr  the old character to replace in the string
 * @param[in]  newchr  the replacement value
 *
 * This function modifies the original string pointed to by *str.
 *
 */
void chrrep(char *str, char oldchr, char newchr) {
    char *p = str;
    int i = 0;
    while (*p) {
        if (*p == oldchr) {
            // special case for DEL character. move string over by one char
            //
            if (newchr == 127) {
                // if newchr==DEL copy memory after that chr
                memmove(&str[i], &str[i + 1], strlen(str) - i);
            } else {
                // otherwise just replace chr
                *p = newchr;
            }
        }
        ++p;
        i++; // increment pointer and byte counter
    }
}

/***** chrrep ***************************************************************/


/***** string_replace_char **************************************************/
/**
 * @brief      replace one character within a std::string with a new character
 * @param[out] str      reference to string to modify
 * @param[in]  oldchar  the char to replace
 * @param[in]  newchar  the replacement value
 *
 */
void string_replace_char(std::string &str, const char *oldchar, const char *newchar) {
    while (str.find(oldchar) != std::string::npos) {
        if (str.find(oldchar) != std::string::npos) str.replace(str.find(oldchar), 1, newchar);
    }
}

/***** string_replace_char **************************************************/


/***** get_time *************************************************************/
/**
 * @brief      gets the current time and returns values by reference
 * @details    call get_time with tmzone_cfg, which is the optional time zone
 *             set by config file. If not set then UTC is used.
 * @param[out] year  int reference for year
 * @param[out] mon   int reference for month
 * @param[out] mday  int reference for day
 * @param[out] hour  int reference for hour
 * @param[out] min   int reference for minute
 * @param[out] sec   int reference for second
 * @param[out] usec  int reference for microsecond
 * @return     1=error, 0=okay
 *
 * This function is overloaded
 *
 */
long get_time(int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec) {
    return get_time(tmzone_cfg, year, mon, mday, hour, min, sec, usec);
}

/***** get_time *************************************************************/


/***** get_time *************************************************************/
/**
 * @brief      gets the current time and returns values by reference
 * @param[in]  tmzone_in  optional time zone {local|UTC|<empty>}
 * @param[out] year       reference to year
 * @param[out] mon        reference to month
 * @param[out] mday       reference to day
 * @param[out] hour       reference to hour
 * @param[out] min        reference to minute
 * @param[out] sec        reference to second
 * @param[out] usec       reference to microsecond
 * @return     1=error, 0=okay
 *
 * This function is overloaded
 *
 */
long get_time(const std::string &tmzone_in, int &year, int &mon, int &mday, int &hour, int &min, int &sec, int &usec) {
    std::stringstream current_time; // String to contain the time
    std::time_t t = std::time(nullptr); // Container for system time
    timespec timenow{}; // Time of day container
    tm mytime{}; // GMT time container
    long error = 0;

    // Get the system time, return a bad timestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &timenow) != 0) error = 1;

    // Convert the time of day to local or GMT
    //
    if (!error) {
        t = timenow.tv_sec;
        if (tmzone_in == "local") { if (localtime_r(&t, &mytime) == nullptr) error = 1; } else {
            if (gmtime_r(&t, &mytime) == nullptr) error = 1;
        }
    }

    // prepare the return values
    //
    if (error == 0) {
        year = mytime.tm_year + 1900;
        mon = mytime.tm_mon + 1;
        mday = mytime.tm_mday;
        hour = mytime.tm_hour;
        min = mytime.tm_min;
        sec = mytime.tm_sec;
        usec = (int) (timenow.tv_nsec / 1000);
    } else {
        year = 9999;
        mon = 99;
        mday = 99;
        hour = 99;
        min = 99;
        sec = 99;
        usec = 999999;
    }

    return (error);
}

/***** get_time *************************************************************/


/***** timestamp_from *******************************************************/
/**
 * @brief      get a human-readable timestamp from a timespec struct
 * @details    call timestamp_from with tmzone_cfg, which is the optional time zone
 *             set by config file. If not set then UTC is used.
 *             The timespec struct must have been filled before calling
 *             this function. This function only gets the time from it. This
 *             is used when multiple functions might need to do things all
 *             with the same time and you don't want the time to differ by
 *             even a fraction of a second between those operations.
 * @param[in]  time_in  reference to a filled timespec struct
 * @return     string   YYYY-MM-DDTHH:MM:SS.sss
 *
 * This function is overloaded
 *
 */
std::string timestamp_from(struct timespec &time_in) {
    return timestamp_from(tmzone_cfg, time_in);
}

/***** timestamp_from *******************************************************/


/***** timestamp_from *******************************************************/
/**
 * @brief      get a human-readable timestamp from a timespec struct
 * @details    The timespec struct must have been filled before calling
 *             this function. This function only gets the time from it. This
 *             is used when multiple functions might need to do things all
 *             with the same time and you don't want the time to differ by
 *             even a fraction of a second between those operations.
 * @param[in]  tmzone_in  optional time zone {local|UTC|<empty>}
 * @param[in]  time_in    reference to a filled timespec struct
 * @return     string     YYYY-MM-DDTHH:MM:SS.sss
 *
 * This function is overloaded
 *
 */
std::string timestamp_from(const std::string &tmzone_in, struct timespec &time_in) {
    std::stringstream current_time; // String to contain the time
    std::time_t t = std::time(nullptr); // Container for system time
    tm time{}; // time container

    // Convert the input time to local or GMT
    //
    t = time_in.tv_sec;
    if (tmzone_in == "local") { if (localtime_r(&t, &time) == nullptr) return ("9999-99-99T99:99:99.999999"); } else {
        if (gmtime_r(&t, &time) == nullptr) return ("9999-99-99T99:99:99.999999");
    }

    current_time.setf(std::ios_base::right);
    current_time << std::setfill('0') << std::setprecision(0)
            << std::setw(4) << time.tm_year + 1900 << "-"
            << std::setw(2) << time.tm_mon + 1 << "-"
            << std::setw(2) << time.tm_mday << "T"
            << std::setw(2) << time.tm_hour << ":"
            << std::setw(2) << time.tm_min << ":"
            << std::setw(2) << time.tm_sec << "."
            << std::setw(6) << time_in.tv_nsec / 1000;

    return (current_time.str());
}

/***** timestamp_from *******************************************************/


/***** get_system_date ******************************************************/
/**
 * @brief      return current date in formatted string "YYYYMMDD"
 * @details    call get_system_date with tmzone_cfg, which is the optional time zone
 *             set by config file. If not set then UTC is used.
 * @return     string  YYYYMMDD
 *
 * This function is overloaded
 *
 */
std::string get_system_date() {
    return get_system_date(tmzone_cfg);
}

/***** get_system_date ******************************************************/


/***** get_system_date ******************************************************/
/**
 * @brief      return current date in formatted string "YYYYMMDD"
 * @param[in]  tmzone_in  optional time zone {local|UTC|<empty>}
 * @return     string  YYYYMMDD
 *
 * This function is overloaded
 *
 */
std::string get_system_date(const std::string &tmzone_in) {
    std::stringstream current_date; // String to contain the return value
    std::time_t t = std::time(nullptr); // Container for system time
    timespec timenow{}; // Time of day container
    tm mytime{}; // time container

    // Get the system time, return a bad datestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &timenow) != 0) return ("99999999");

    // Convert the time of day to local or GMT
    //
    t = timenow.tv_sec;
    if (tmzone_in == "local") { if (localtime_r(&t, &mytime) == nullptr) return ("9999-99-99T99:99:99.999999"); } else {
        if (gmtime_r(&t, &mytime) == nullptr) return ("9999-99-99T99:99:99.999999");
    }

    current_date.setf(std::ios_base::right);
    current_date << std::setfill('0') << std::setprecision(0)
            << std::setw(4) << mytime.tm_year + 1900
            << std::setw(2) << mytime.tm_mon + 1
            << std::setw(2) << mytime.tm_mday;

    return (current_date.str());
}

/***** get_system_date ******************************************************/


/***** get_file_time ********************************************************/
/**
 * @brief      return current time in formatted string "YYYYMMDDHHMMSS"
 * @return     string  YYYYMMDDHHMMSS
 *
 * Used for filenames
 *
 */
std::string get_file_time() {
    return get_file_time(tmzone_cfg);
}

/***** get_file_time ********************************************************/


/***** get_file_time ********************************************************/
/**
 * @brief      return current time in formatted string "YYYYMMDDHHMMSS"
 * @param[in]  tmzone_in  optional time zone {local|UTC|<empty>}
 * @return     string  YYYYMMDDHHMMSS
 *
 * Used for filenames
 *
 */
std::string get_file_time(const std::string &tmzone_in) {
    std::stringstream current_time; // String to contain the time
    std::time_t t = std::time(nullptr); // Container for system time
    timespec timenow{}; // Time of day container
    tm mytime{}; // time container

    // Get the system time, return a bad timestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &timenow) != 0) return ("99999999999999");

    // Convert the time of day to local or GMT
    //
    t = timenow.tv_sec;
    if (tmzone_in == "local") { if (localtime_r(&t, &mytime) == nullptr) return ("99999999999999"); } else {
        if (gmtime_r(&t, &mytime) == nullptr) return ("99999999999999");
    }

    current_time.setf(std::ios_base::right);
    current_time << std::setfill('0') << std::setprecision(0)
            << std::setw(4) << mytime.tm_year + 1900
            << std::setw(2) << mytime.tm_mon + 1
            << std::setw(2) << mytime.tm_mday
            << std::setw(2) << mytime.tm_hour
            << std::setw(2) << mytime.tm_min
            << std::setw(2) << mytime.tm_sec;

    return (current_time.str());
}

/***** get_file_time ********************************************************/


/***** get_clock_time *******************************************************/
/**
 * @brief      get the current clock time using REALTIME flag from the processor
 * @return     time in seconds
 *
 */
double get_clock_time() {
    timespec data{}; // Container for the current time
    if (clock_gettime(CLOCK_REALTIME, &data) != 0) return 0;
    return (static_cast<double>(data.tv_sec) + (static_cast<double>(data.tv_nsec) / 1000000000.0));
}

/***** get_clock_time *******************************************************/


/***** timeout **************************************************************/
/**
 * @brief      sleeps integral number of minutes or seconds
 * @param[in]  wholesec  an optional number of integral seconds to sleep first
 * @param[in]  next      string "sec" or "min" to return on integral sec or min
 * @return     0 on success, 1 on error
 *
 */
long timeout(int wholesec, const std::string &next) {
    std::time_t t = std::time(nullptr); // Container for system time
    timespec timenow{}; // Time of day container
    tm mytime{}; // GMT time container
    long error = 0;
    int nsec = 0;
    int sec = 0;

    // sleep for any requested whole number of seconds
    //
    if (wholesec > 0) std::this_thread::sleep_for(std::chrono::seconds(wholesec));

    // Get the system time, return a bad timestamp on error
    //
    if (clock_gettime(CLOCK_REALTIME, &timenow) != 0) error = 1;

    // Convert the time of day to local or GMT
    //
    if (!error) {
        t = timenow.tv_sec;
        if (gmtime_r(&t, &mytime) == nullptr) error = 1;
        sec = mytime.tm_sec; // current second
        nsec = static_cast<int>(timenow.tv_nsec); // current nanosecond
    }

    // sleep for the required fraction to get to the next whole number
    // of sec or min, as requested
    //
    if (!error && next == "sec") {
        if (nsec < 999999999) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(999999999 - nsec));
        }
    } else if (!error && next == "min") {
        if (sec < 59) {
            std::this_thread::sleep_for(std::chrono::seconds(59 - sec));
        }
        if (nsec < 999999999) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(999999999 - nsec));
        }
    }
    return error;
}

/***** timeout **************************************************************/


/***** mjd_from *************************************************************/
/**
 * @brief      return Modified Julian Date for time in a timespec struct
 * @details    The input timespec struct must have been filled before calling
 *             this function. This function only calculates the MJD from it.
 *
 *             I got this from ZTF's astronomy.h which says it came from
 *             Meeus' Astronomical Formulae for Calculators.  The two JD
 *             conversion routines routines were replaced 1998 November 29
 *             to avoid inclusion of copyrighted "Numerical Recipes" code.
 *             A test of 1 million random JDs between 1585 and 3200 AD gave
 *             the same conversions as the NR routines.
 * @param[in]  time_in  reference to timespec struct filled by clock_gettime()
 * @return     double, 0 on error
 *
 */
double mjd_from(timespec &time_in) {
    std::time_t t = std::time(nullptr); // Container for system time
    tm time{}; // GMT time container

    // Convert the input time to GMT
    //
    t = time_in.tv_sec;
    if (gmtime_r(&t, &time) == nullptr) return 0.;

    double a = std::floor((14 - (time.tm_mon + 1)) / 12);
    double y = (time.tm_year + 1900) + 4800 - a;
    double m = (time.tm_mon + 1) + 12 * a - 3;
    double jdn = time.tm_mday
                 + std::floor((153 * m + 2) / 5)
                 + 365 * y
                 + std::floor(y / 4)
                 - std::floor(y / 100)
                 + std::floor(y / 400)
                 - 32045;

    double jd = jdn + (time.tm_hour - 12) / 24.
                + time.tm_min / 1440.
                + time.tm_sec / 86400.
                + (static_cast<double>(time_in.tv_nsec) / 1000000000.) / 86400.;

    return (jd - 2400000.5);
}

/***** mjd_from *************************************************************/


/***** compare_versions *****************************************************/
/**
 * @brief      compares two version numbers represented as strings
 * @param[in]  v1  the first version number string to be compared
 * @param[in]  v2  the second version number string to be compared
 * @return     0,1,-1,-999
 *
 * This function compares version or revision numbers which are represented
 * as strings which contain decimals. Each portion of a version number is
 * compared.
 *
 * Returns 1 if v2 is smaller, -1 if v1 is smaller, 0 if equal
 * Returns -999 on error
 *
 * Revision numbers can be like n.n.n etc with any number of numbers n,
 * but must be numbers.
 *
 */
int compare_versions(const std::string &v1, const std::string &v2) {
    std::vector<std::string> tokens1;
    std::vector<std::string> tokens2;

    // Tokenize the version numbers, using the decimal point "." as the separator
    //
    Tokenize(v1, tokens1, ".");
    Tokenize(v2, tokens2, ".");

    // Compare each token.
    // As soon as one is greater than the other, then return.
    //
    for (size_t i = 0, j = 0; (i < tokens1.size() && j < tokens2.size()); i++, j++) {
        try {
            if (std::stoi(tokens1.at(i)) > std::stoi(tokens2.at(j))) return 1;
            if (std::stoi(tokens2.at(j)) > std::stoi(tokens1.at(i))) return -1;
        } catch (std::invalid_argument &) {
            return (-999);
        }
        catch (std::out_of_range &) {
            return (-999);
        }
    }

    // If we finished the loop then either they were all equal or
    // one version had more tokens than the other (E.G., 1.1.123 vs 1.1).
    // The one with more tokens has to be greater.
    //
    if (tokens1.size() > tokens2.size()) return 1;
    if (tokens2.size() > tokens1.size()) return -1;

    return 0; // or they are equal
}

/***** compare_versions *****************************************************/


/***** md5_file *************************************************************/
/**
 * @brief      compute the md5sum of a file
 * @details    This makes use of an external source, md5.h and md5.cpp
 * @param[in]  filename  const reference to filename to process
 * @param[out] hash      reference to a string to contain result
 *
 */
long md5_file(const std::string &filename, std::string &hash) {
    MD5_CTX ctx;
    md5_init(&ctx);

    try {
        // open the input file stream
        //
        std::ifstream instream(filename, std::ios::binary);
        if (!instream.is_open()) {
            std::cerr << "ERROR opening file: " << filename << "\n";
            return 1;
        }

        // process a byte at a time so the entire file never lives in memory
        // use a vector for better cleanup
        //
        std::vector<BYTE> buffer(1, 0);
        while (instream.read(reinterpret_cast<char *>(buffer.data()), 1)) {
            md5_update(&ctx, buffer.data(), instream.gcount());
        }

        instream.close();
    } catch (std::ifstream::failure &e) {
        std::cerr << "md5_file( " << filename << " ): " << e.what() << "\n";
        hash = "ERROR";
        return 1;
    }
    catch (std::exception &e) {
        std::cerr << "md5_file( " << filename << " ): " << e.what() << "\n";
        hash = "ERROR";
        return 1;
    }

    BYTE result[MD5_BLOCK_SIZE];
    md5_final(&ctx, result);

    // convert result to a string
    //
    std::stringstream str;
    for (unsigned char i: result) {
        str << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(i);
    }

    hash = str.str();

    return 0;
}

/***** md5_file *************************************************************/


/***** is_owner *************************************************************/
/**
 * @brief      does the user own the specified file?
 * @details    Check if the effective user ID matches the file's owner ID
 * @param[in]  filename  const reference to filename to check
 * @return     true or false
 *
 */
bool is_owner(const std::filesystem::path &filename) {
    struct stat fstat{};
    if (stat(filename.c_str(), &fstat) == 0) {
        // Check if the effective user ID matches the file's owner ID
        return geteuid() == fstat.st_uid;
    }
    return false; // Error handling: Unable to get file information
}

/***** is_owner *************************************************************/


/***** has_write_permission *************************************************/
/**
 * @brief      does the user have write permission on the specified file?
 * @details    Check if the effective user ID has write permission
 * @param[in]  filename  const reference to filename to check
 * @return     true or false
 *
 */
bool has_write_permission(const std::filesystem::path &filename) {
    struct stat fstat{};
    if (stat(filename.c_str(), &fstat) == 0) {
        // Check if the effective user ID has write permission
        return fstat.st_mode & S_IWUSR;
    }
    return false; // Error handling: Unable to get file information
}

/***** has_write_permission *************************************************/


/***** tchar ****************************************************************/
/**
 * @brief      return a printable string of a non-printable terminating char
 * @param[in]  str  reference to input string
 * @return     string (static references are valid for the lifetime of the program)
 *
 */
const std::string &tchar(const std::string &str) {
    static const std::string unknown = "??";
    static const std::string newline = "\\n";
    static const std::string carreturn = "\\r";
    static const std::string nullchar = "\\0";

    if (str.empty()) return unknown;

    switch (str.back()) {
        case '\n': return newline;
        case '\r': return carreturn;
        case '\0': return nullchar;
        default: return unknown;
    }
}

/***** tchar ****************************************************************/


/***** strip_newline **********************************************************/
/**
 * @brief      strip newline and cr chars from a string
 * @details    this makes a local copy of the original string which is not changed
 * @param[in]  str_in  reference to input string
 * @return     string
 *
 */
const std::string strip_newline( const std::string &str_in ) {
  std::string str = str_in;
  str.erase( std::remove( str.begin(), str.end(), '\n' ), str.end() );
  str.erase( std::remove( str.begin(), str.end(), '\r' ), str.end() );
  return str;
}
/***** strip_newline **********************************************************/


/***** strip_control_characters *********************************************/
/**
 * @brief      strip all leading and trailing control chars from a string
 * @param[in]  str  input string
 * @return     input string without control chars
 *
 * str is cast to uchar for safety because iscntrl() requires uchar
 *
 */
std::string strip_control_characters(const std::string &str) {
    // locate leading control characters
    //
    size_t start = 0;
    while (start < str.length() && std::iscntrl(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    // locate trailing control characters
    //
    size_t end = str.length();
    while (end > start && std::iscntrl(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    // return the substring without leading and trailing control characters
    //
    return str.substr(start, end - start);
}

/***** strip_control_characters *********************************************/


/***** starts_with **********************************************************/
/**
 * @brief      check if a string starts with a string literal
 * @details    this is here in case c++20 is not available
 * @param[in]  str     reference to input string
 * @param[in]  prefix  a read-only prefix string
 * @return     true or false
 *
 */
bool starts_with(const std::string &str, std::string_view prefix) {
    return (str.compare(0, prefix.length(), prefix) == 0);
}

/***** starts_with **********************************************************/


/***** ends_with ************************************************************/
/**
 * @brief      check if a string end with a string literal
 * @details    this is here in case c++20 is not available
 * @param[in]  str     reference to input string
 * @param[in]  suffix  a read-only suffix string
 * @return     true or false
 *
 */
bool ends_with(const std::string &str, std::string_view suffix) {
    if (str.length() < suffix.length()) return false;
    return str.substr(str.length() - suffix.length()) == suffix;
}

/***** ends_with ************************************************************/


/***** generate_temp_filename ***********************************************/
/**
 * @brief      generates a temporary filename only, not a file
 * @details    mimmics tmpnam to more safely create a temporary filename
 * @param[in]  prefix  prefix for the filename, /tmp/prefix.XXXXXX
 * @return     generated temporary filename
 *
 */
std::string generate_temp_filename(const std::string &prefix) {
    std::string pattern = "/tmp/" + prefix + "XXXXXX";
    char *filename = strdup(pattern.c_str()); // mkstemp requires a non-const pointer

    generate_tmpfile_mtx.lock();
    int fd = mkstemp(filename); // create and open the file
    generate_tmpfile_mtx.unlock();

    if (fd != -1) {
        close(fd); // close the file immediately
        std::string temp_filename = filename; // create a std::string
        free(filename); // cleanup and
        unlink(temp_filename.c_str()); // delete the file.
        return temp_filename;
    } else {
        free(filename);
        return "";
    }
}

/***** generate_temp_filename ***********************************************/


/***** rtrim ***********************************************/
/**
 * @brief      trim trailing white space from a string
 * @details    The input string will be modified.
 * @param[in]  s  reference to string
 *
 */
void rtrim(std::string &s) {
    /// trim off trailing whitespace from a string
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}
/***** rtrim ***********************************************/
