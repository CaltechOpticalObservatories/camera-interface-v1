/**
 * @file     common.h
 * @brief    Common namespace header file
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <sstream>
#include <queue>
#include <string>
#include <mutex>
#include <map>
#include <condition_variable>
#include <zmqpp/zmqpp.hpp>
#include <nlohmann/json.hpp>

#include "logentry.h"

constexpr long NOTHING = -1;
constexpr long NO_ERROR = 0;
constexpr long ERROR = 1;
constexpr long BUSY = 2;
constexpr long TIMEOUT = 3;
constexpr long HELP = 4;
constexpr long JSON = 5;
constexpr long ABORT = 6;
constexpr long EXIT = 999;

const std::string JEOF = "EOF\n";              ///< used to terminate JSON messages
const std::string TELEMREQUEST = "sendtelem";  ///< common daemon command used to request telemetry
const std::string SNAPSHOT = "snapshot";       ///< common daemon command forces publish of telemetry

constexpr bool EXT = true;   ///< constant for use_extension arg of Common::Header::add_key()
constexpr bool PRI = !EXT;   ///< constant for use_extension arg of Common::Header::add_key()

namespace Common {

    /**************** Common::FitsKeys ******************************************/
    /*
     * @class  FitsKeys
     * @brief  provides a user-defined keyword database
     *
     */
    class FitsKeys {
    private:
    public:
        FitsKeys() = default;

        std::string get_keytype(std::string keyvalue); /// return type of keyword based on value
        long listkeys(); /// list FITS keys in the internal database
        long addkey(std::string arg); /// add FITS key to the internal database
        long delkey(std::string arg); /// delete FITS key from the internal database
        void erasedb() { this->keydb.clear(); }; /// erase the entire contents of the internal database

        /***** Common::FitsKeys::addkey *****************************************/
        /**
         * @brief      template function adds FITS keyword to internal database
         * @details    no parsing is done here
         * @param[in]  keyword  keyword name <string>
         * @param[in]  value    value for keyword, any type <T>
         * @param[in]  comment  comment <string>
         * @param[in]  prec     decimal places of precision
         * @return     ERROR or NO_ERROR
         *
         * This function is overloaded.
         *
         * This version adds the keyword=value//comment directly into the
         * database with no parsing. The database stores only strings so
         * the type variable preserves the type and informs the FITS writer.
         *
         * The template input "tval" here allows passing in any type of value,
         * and the type will be set by the actual input type. However, if
         * the input type is a string then an attempt will be made to infer the
         * type from the string (this allows passing in a string "12.345" and
         * have it be converted to a double.
         *
         * The prec argument sets the displayed precision of double or float
         * types if passed in as a double or float. Default is 8 if not supplied.
         *
         * The overloading for type bool is because for the template class,
         * since T can be evaluated as string, it can't be treated as bool,
         * so this is the easiest way to extract the bool value.
         *
         */
        inline long addkey(const std::string &key, bool bval, const std::string &comment) {
            return addkey(key, (bval ? "T" : "F"), comment, 0);
        }

        template<class T>
        long addkey(const std::string &key, T tval, const std::string &comment) {
            return addkey(key, tval, comment, 8);
        }

        template<class T>
        long addkey(const std::string &key, T tval, const std::string &comment, int prec) {
            std::stringstream val;
            std::string type;

            // "convert" the type <T> value into a string via the appropriate stream
            // and set the type string
            //
            if (std::is_same<T, double>::value) {
                val << std::fixed << std::setprecision(prec) << tval;
                type = "DOUBLE";
            } else if (std::is_same<T, float>::value) {
                val << std::fixed << std::setprecision(prec) << tval;
                type = "FLOAT";
            } else if (std::is_same<T, int>::value) {
                val << tval;
                type = "INT";
            } else if (std::is_same<T, long>::value) {
                val << tval;
                type = "LONG";
            } else {
                val << tval;
                type = this->get_keytype(val.str()); // try to infer the type from any string
            }

            // insert new entry into the database
            //
            this->keydb[key].keyword = key;
            this->keydb[key].keytype = type;
            this->keydb[key].keyvalue = val.str();
            this->keydb[key].keycomment = comment;

#ifdef LOGLEVEL_DEBUG
        std::string function = "Common::FitsKeys::addkey";
        std::stringstream message;
        message << "[DEBUG] added key: " << key << "=" << tval << " (" << this->keydb[key].keytype << ") // " << comment;
        logwrite( function, message.str() );
#endif
            return (NO_ERROR);
        }
        /***** Common::FitsKeys::addkey *****************************************/


        typedef struct {
            /// structure of FITS keyword internal database
            std::string keyword;
            std::string keytype;
            std::string keyvalue;
            std::string keycomment;
        } user_key_t;

        typedef std::map<std::string, user_key_t> fits_key_t; /// STL map for the actual keyword database

        fits_key_t keydb; /// keyword database

        // Find all entries in the keyword database which start with the search_for string,
        // return a vector of iterators.
        //
        std::vector<fits_key_t::const_iterator> FindKeys(std::string search_for) {
            std::vector<fits_key_t::const_iterator> vec;
            for (auto it = this->keydb.lower_bound(search_for);
                 it != std::end(this->keydb) && it->first.compare(0, search_for.size(), search_for) == 0;
                 ++it) {
                vec.push_back(it);
            }
            return (vec);
        }

        // Find and remove all entries in the keyword database which match the search_for string.
        //
        void EraseKeys(std::string search_for) {
            std::string function = "FitsKeys::EraseKeys";
            std::stringstream message;
            std::vector<fits_key_t::const_iterator> erasevec;

            // get a vector of iterators for all the keys matching the search string
            //
            erasevec = this->FindKeys(search_for);
#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] found " << erasevec.size() << " entries matching \"" << search_for << "*\"";
        logwrite( function, message.str() );
#endif

            // loop through that vector and erase the selected iterators from the database
            //
            for (auto vec: erasevec) {
#ifdef LOGLEVEL_DEBUG
          message.str(""); message << "[DEBUG] erasing " << vec->first; logwrite( function, message.str() );
#endif
                this->keydb.erase(vec);
            }
            return;
        }
    };
    /**************** Common::FitsKeys ******************************************/


    /**************** Common::Queue *********************************************/
    /**
     * @class  Queue
     * @brief  provides a thread-safe messaging queue
     *
     */
    class Queue {
    private:
        std::queue<std::string> message_queue;
        mutable std::mutex queue_mutex;
        std::condition_variable notifier;
        bool is_running;

    public:
        Queue(void) : is_running(false) {
        }

        void service_running(bool state) { this->is_running = state; }; /// set service running
        bool service_running() { return this->is_running; }; /// is the service running?
        void enqueue_and_log(std::string function, std::string message);
        void enqueue_and_log(std::string tag, std::string function, std::string message);
        void enqueue(std::string message); /// push an element into the queue.
        std::string dequeue(void); /// pop an element from the queue
    };
    /**************** Common::Queue *********************************************/

}

