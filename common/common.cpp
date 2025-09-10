/**
 * @file     common.cpp
 * @brief    Common namespace functions
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#include "common.h"

namespace Common {
    /** Common::Queue::enqueue **************************************************/
    /**
     * @fn     enqueue
     * @brief  puts a message into the queue
     * @param  std::string message
     * @return none
     *
     */
    void Queue::enqueue(std::string message) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        message_queue.push(message);
        notifier.notify_one();
        return;
    }

    /** Common::Queue::enqueue **************************************************/


    /** Common::Queue::dequeue **************************************************/
    /**
     * @fn     dequeue
     * @brief  pops the first message off the queue
     * @param  none
     * @return std::string message
     *
     * Get the "front"-element.
     * If the queue is empty, wait untill an element is avaiable.
     *
     */
    std::string Queue::dequeue(void) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        while (message_queue.empty()) {
            notifier.wait(lock); // release lock as long as the wait and reaquire it afterwards.
        }
        std::string message = message_queue.front();
        message_queue.pop();
        return message;
    }

    /** Common::Queue::dequeue **************************************************/


    /** Common::FitsKeys::get_keytype *******************************************/
    /**
     * @brief  return the keyword type based on the keyvalue
     * @param  std::string value
     * @return string { "BOOL", "STRING", "DOUBLE", "FLOAT", "INT", "LONG" }
     *
     * This function looks at the contents of the value string to determine if
     * it contains an INT, DOUBLE, FLOAT, BOOL or STRING, and returns a string
     * identifying the type.  That type is used in FITS_file::add_user_key()
     * for adding keywords to the header.
     *
     * To differentiate between double and float, the keyvalue string must end
     * with an "f", as in "3.14f" which returns "FLOAT" or "3.14" which returns
     * "DOUBLE". Decimal point is required ("1f" is not a float but "1.0f" is)
     *
     * To differentiate between int and long, keyvalue string must end with "l"
     * as in "100l" which returns "LONG" or "100" which returns "INT". Since a
     * long can't have a decimal, using 'l' with a decimal point is a string.
     *
     */
    std::string FitsKeys::get_keytype(std::string keyvalue) {
        std::size_t pos(0);

        // If it's empty then what else can it be but string
        if ( keyvalue.empty() ) return std::string("STRING");

        // if the entire string is either (exactly) T or F then it's a boolean
        if (keyvalue == "T" || keyvalue == "F") {
            return std::string("BOOL");
        }

        // skip the whitespaces
        if (keyvalue.find_first_not_of(' ') != std::string::npos) {
          pos = keyvalue.find_first_not_of(' ');
        }
        if (pos == keyvalue.size()) return std::string("STRING"); // all spaces, so it's a string

        // remove sign if it exists
        if (keyvalue[pos] == '+' || keyvalue[pos] == '-') keyvalue.erase(0,1);

        // count the different types of characters
        //
        // number of decimal points
        auto n_pt = std::count(keyvalue.begin(), keyvalue.end(), '.');
        // number of numerics
        auto n_nm = std::count_if(keyvalue.begin(), keyvalue.end(), [](unsigned char c) {
                                                                    return std::isdigit(c);
                                                                    } );
        // number of string chars, non-numerics, non-decimals
        auto n_ch = std::count_if(keyvalue.begin(), keyvalue.end(), [](unsigned char c) {
                                                                    return (!std::isdigit(c) && c!='.');
                                                                    } );

        // number of decimals points, numeric and non-numeric chars determines the type
        // verify by trying to convert to that type and return string on conversion failure
        //
        try {
          // no numbers or more than one decimal point can't be any type of number, so string
          if ( n_nm==0 || n_pt > 1 ) {
            return std::string("STRING");
          }
          else
          // at least one digit, exactly one decimal point and only and last character is "f" is a float
          if ( n_nm > 0 && n_pt==1 && n_ch==1 && keyvalue.back()=='f' ) {
            std::stof(keyvalue);
            return std::string("FLOAT");
          }
          else
          // at least one digit, exactly one decimal point and no chars is double
          if ( n_nm > 0 && n_pt==1 && n_ch==0 ) {
            std::stod(keyvalue);
            return std::string("DOUBLE");
          }
          else
          // at least one digit, no decimal point and no chars is int
          if ( n_nm > 0 && n_pt==0 && n_ch==0 ) {
            std::stoi(keyvalue);
            return std::string("INT");
          }
          else
          // at least one digit, no decimal point and only and last char is "l" is long
          if ( n_nm > 0 && n_pt==0 && n_ch==1 && keyvalue.back()=='l' ) {
            std::stol(keyvalue);
            return std::string("LONG");
          }
          else
            // what else can it be but string
            return std::string("STRING");
        }
        catch (const std::exception &) {
          // any numeric conversion failure sets type as string
          return std::string("STRING");
        }
    }
    /** Common::FitsKeys::get_keytype *******************************************/


    /** Common::FitsKeys::listkeys **********************************************/
    /**
     * @fn     listkeys
     * @brief  list FITS keywords in internal database
     * @param  none
     * @return NO_ERROR
     *
     */
    long FitsKeys::listkeys() {
        std::string function = "Common::FitsKeys::listkeys";
        std::stringstream message;
        fits_key_t::iterator keyit;
        for (keyit = this->keydb.begin();
             keyit != this->keydb.end();
             keyit++) {
            message.str("");
            message << keyit->second.keyword << " = " << keyit->second.keyvalue;
            if (!keyit->second.keycomment.empty()) message << " // " << keyit->second.keycomment;
            message << " (" << keyit->second.keytype << ")";
            logwrite(function, message.str());
        }
        return NO_ERROR;
    }

    /** Common::FitsKeys::listkeys **********************************************/


    /** Common::FitsKeys::addkey ************************************************/
    /**
     * @fn     addkey
     * @brief  add FITS keyword to internal database
     * @param  std::string arg
     * @return ERROR for improper input arg, otherwise NO_ERROR
     *
     * Expected format of input arg is KEYWORD=VALUE//COMMENT
     * where COMMENT is optional. KEYWORDs are automatically converted to uppercase.
     *
     * Internal database is Common::FitsKeys::keydb
     *
     */
    long FitsKeys::addkey(std::string arg) {
        std::string function = "Common::FitsKeys::addkey";
        std::stringstream message;
        std::vector<std::string> tokens;
        std::string keyword, keystring, keyvalue, keytype, keycomment;
        std::string comment_separator = "//";

        // There must be one equal '=' sign in the incoming string, so that will make two tokens here
        //
        Tokenize(arg, tokens, "=");
        if (tokens.size() != 2) {
            logwrite(function, "missing or too many '=': expected KEYWORD=VALUE//COMMENT (optional comment)");
            return ERROR;
        }

        keyword = tokens[0].substr(0, 8); // truncate keyword to 8 characters
        keyword = keyword.erase(keyword.find_last_not_of(" ") + 1); // remove trailing spaces from keyword
        std::locale loc;
        for (std::string::size_type ii = 0; ii < keyword.length(); ++ii) {
            // Convert keyword to upper case:
            keyword[ii] = std::toupper(keyword[ii], loc); // prevents duplications in STL map
        }
        keystring = tokens[1]; // tokenize the rest in a moment

        size_t pos = keystring.find(comment_separator); // location of the comment separator
        keyvalue = keystring.substr(0, pos); // keyvalue is everything up to comment
        keyvalue = keyvalue.erase(0, keyvalue.find_first_not_of(" ")); // remove leading spaces from keyvalue
        keyvalue = keyvalue.erase(keyvalue.find_last_not_of(" ") + 1); // remove trailing spaces from keyvalue
        if (pos != std::string::npos) {
            keycomment = keystring.erase(0, pos + comment_separator.length());
            keycomment = keycomment.erase(0, keycomment.find_first_not_of(" "));
            // remove leading spaces from keycomment
        }

        // Delete the keydb entry for associated keyword if the keyvalue is a sole period '.'
        //
        if (keyvalue == ".") {
            fits_key_t::iterator ii = this->keydb.find(keyword);
            if (ii == this->keydb.end()) {
                message.str("");
                message << "keyword " << keyword << " not found";
                logwrite(function, message.str());
            } else {
                this->keydb.erase(ii);
                message.str("");
                message << "keyword " << keyword << " erased";
                logwrite(function, message.str());
            }
            return NO_ERROR;
        }

        // check for instances of the comment separator in keycomment
        //
        if (keycomment.find(comment_separator) != std::string::npos) {
            message.str("");
            message << "ERROR: FITS comment delimiter: found too many instancces of " << comment_separator <<
                    " in keycomment";
            logwrite(function, message.str());
            return NO_ERROR;
        }

        // insert new entry into the database
        //
        this->keydb[keyword].keyword = keyword;
        this->keydb[keyword].keytype = this->get_keytype(keyvalue);
        this->keydb[keyword].keyvalue = keyvalue;
        this->keydb[keyword].keycomment = keycomment;

        // long and float are designated by a modifier char which must be removed
        //
        if (this->keydb[keyword].keytype=="LONG") {
          if (!this->keydb[keyword].keyvalue.empty() && this->keydb[keyword].keyvalue.back()=='l') {
            this->keydb[keyword].keyvalue.pop_back();
          }
        }
        if (this->keydb[keyword].keytype=="FLOAT") {
          if (!this->keydb[keyword].keyvalue.empty() && this->keydb[keyword].keyvalue.back()=='f') {
            this->keydb[keyword].keyvalue.pop_back();
          }
        }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] added key: " << keyword << "=" << keyvalue << " (" << this->keydb[keyword].keytype << ") // " << keycomment;
    logwrite( function, message.str() );
#endif

        return NO_ERROR;
    }

    /** Common::FitsKeys::addkey ************************************************/


    /***** Common::FitsKeys::delkey *********************************************/
    /**
     * @brief      delete FITS keyword from internal database
     * @param[in]  keyword
     * @return     ERROR for improper input arg, otherwise NO_ERROR
     *
     */
    long FitsKeys::delkey(std::string keyword) {
        std::string function = "Common::FitsKeys::delkey";
        std::stringstream message;

        try {
            std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::toupper); // make uppercase
        } catch (...) {
            message.str("");
            message << "converting keyword " << keyword << " to uppercase";
            logwrite(function, message.str());
            return ERROR;
        }

        // Delete the keydb entry for associated keyword (if found).
        // Don't report anything if keyword is not found.
        //
        fits_key_t::iterator keyit = this->keydb.find(keyword);
        if (keyit != this->keydb.end()) {
            this->keydb.erase(keyit);
            message.str("");
            message << "keyword " << keyword << " erased";
            logwrite(function, message.str());
        }
        return NO_ERROR;
    }

    /***** Common::FitsKeys::delkey *********************************************/
}
