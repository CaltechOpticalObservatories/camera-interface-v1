/**
 * @file     common.cpp
 * @brief    Common namespace functions
 * @author   David Hale <dhale@astro.caltech.edu>
 *
 */

#include "common.h"

namespace Common {

  /** Common::Queue::~Queue *****************************************************/
  /**
  * @fn     ~Queue
  * @brief  Destructor that cleans up any remaining messages in the queue.
  *
  * Pops and deletes all remaining heap-allocated messages from the internal
  * Boost lock-free queue to prevent memory leaks. Should be called only once
  * when the queue is no longer in use.
  */
  Queue::~Queue() {
    std::string* msg = nullptr;
    while (message_queue.pop(msg)) {
      delete msg;
    }
  }
  /**************** Common::Queue::~Queue *************************************/

  /** Common::Queue::enqueue **************************************************/
  /**
   * @fn     enqueue
   * @brief  Adds a message to the queue in a non-blocking way.
   * @param  std::string message
   * @return none
   *
   */
  void Queue::enqueue(const std::string& message) {
    std::string* msg = new std::string(message);
    while (!message_queue.push(msg)) {
      std::this_thread::yield(); // back off if queue is full
    }
  }
  /** Common::Queue::enqueue **************************************************/


  /** Common::Queue::dequeue **************************************************/
  /**
   * @fn     dequeue
   * @brief  Retrieves and removes the next message from the queue.
   * @param  none
   * @return std::string message
   *
   * Pops a pointer to a message from the queue in a non-blocking way.
   * If the queue is empty, an empty string is returned.
   *
   */
  std::string Queue::dequeue() {
    std::string* msg = nullptr;
    if (message_queue.pop(msg)) {
      std::string result = *msg;
      delete msg;
      return result;
    } else {
      return "";
    }
  }
  /** Common::Queue::dequeue **************************************************/


  /** Common::FitsKeys::get_keytype *******************************************/
  /**
   * @fn     get_keytype
   * @brief  return the keyword type based on the keyvalue
   * @param  std::string value
   * @return std::string type: "BOOL", "STRING", "DOUBLE", "INT"
   *
   * This function looks at the contents of the value string to determine if it
   * contains an INT, DOUBLE, BOOL or STRING, and returns a string identifying the type.
   * That type is used in FITS_file::add_user_key() for adding keywords to the header.
   *
   */
  std::string FitsKeys::get_keytype(std::string keyvalue) {
    std::size_t pos(0);

    // if the entire string is either (exactly) T or F then it's a boolean
    if ( keyvalue == "T" || keyvalue == "F" ) {
      return std::string( "BOOL" );
    }

    // skip the whitespaces
    pos = keyvalue.find_first_not_of(' ');
    if (pos == keyvalue.size()) return std::string("STRING");   // all spaces, so it's a string

    // check the significand
    if (keyvalue[pos] == '+' || keyvalue[pos] == '-') ++pos;    // skip the sign if exist

    // count the number of digits and number of decimal points
    int n_nm, n_pt;
    for (n_nm = 0, n_pt = 0; std::isdigit(keyvalue[pos]) || keyvalue[pos] == '.'; ++pos) {
        keyvalue[pos] == '.' ? ++n_pt : ++n_nm;
    }

    if (n_pt>1 || n_nm<1 || pos<keyvalue.size()){ // no more than one point, no numbers, or a non-digit character
      return std::string("STRING");               // then it's a string
    }

    // skip the trailing whitespaces
    while (keyvalue[pos] == ' ') {
        ++pos;
    }

    std::string check_type;

    if (pos == keyvalue.size()) {
      // If it's an INT or DOUBLE, don't return that type until it has been checked, below
      //
      if (keyvalue.find(".") == std::string::npos)    // all numbers and no decimals, it's an integer
        check_type = "INT";
      else                                            // otherwise numbers with a decimal, it's a float
        check_type = "DOUBLE";
    }
    else return std::string("STRING");                // lastly, must be a string

    // If it's an INT or a DOUBLE then try to convert the value to INT or DOUBLE.
    // If that conversion fails then set the type to STRING.
    //
    try {
      if ( check_type == "INT"    ) std::stoi( keyvalue );
      if ( check_type == "DOUBLE" ) std::stod( keyvalue );
    }
    catch ( std::invalid_argument & ) {
      return std::string( "STRING" );
    }
    catch ( std::out_of_range & ) {
      return std::string( "STRING" );
    }
    catch ( std::exception &e ) {
      logwrite( "Common::FitsKeys::get_keytype", "ERROR: "+std::string(e.what()) );
      return std::string( "ERROR" );
    }
    return( check_type );

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
    for (keyit  = this->keydb.begin();
         keyit != this->keydb.end();
         keyit++) {
      message.str("");
      message << keyit->second.keyword << " = " << keyit->second.keyvalue;
      if ( ! keyit->second.keycomment.empty() ) message << " // " << keyit->second.keycomment;
      message << " (" << keyit->second.keytype << ")";
      logwrite(function, message.str());
    }
    return(NO_ERROR);
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
      logwrite( function, "ERROR missing or too many '=': expected KEYWORD=VALUE//COMMENT (optional comment)" );
      return(ERROR);
    }

    keyword   = tokens[0].substr(0,8);                                     // truncate keyword to 8 characters
    keyword   = keyword.erase(keyword.find_last_not_of(" ")+1);            // remove trailing spaces from keyword

    std::locale loc;
    for (std::string::size_type ii=0; ii<keyword.length(); ++ii) {         // Convert keyword to upper case
      keyword[ii] = std::toupper(keyword[ii],loc);
    }

    // Apply keyword rules one at a time so that the error can identify the problem
    //

    // no embedded spaces allowed in keyword
    //
    if ( keyword.find( " " ) != std::string::npos ) {
      message.str(""); message << "ERROR embedded spaces not allowed in keyword \"" << keyword << "\"";
      logwrite( function, message.str() );
      return( ERROR );
    }

    // keyword must start with a letter {A:Z}
    //
    static const std::regex start( "[A-Z].*" );  // start with [A-Z] followed by 0 or more of anything
    if ( !regex_match( keyword, start ) ) {
      message.str(""); message << "ERROR first character of keyword \"" << keyword << "\" must start with A:Z";
      logwrite( function, message.str() );
      return( ERROR );
    }

    // keyword must contain only A-Z, 0-9, underscore and dash
    //
    static const std::regex contents( "[A-Z][A-Z0-9_-]*" );  // start with A-Z followed by zero or more of A-Z, 0-9, _, -
    if ( !regex_match( keyword, contents ) ) {
      message.str(""); message << "ERROR keyword \"" << keyword << "\" must contain only A-Z, 0-9, underscore or hyphen";
      logwrite( function, message.str() );
      return( ERROR );
    }

    // keyword cannot conntain 0-padded number sequences
    //
    static const std::regex zeropadding( "[A-Z_-]*[0][0-9]+[A-Z_-]*" );  // zero or more chars and 0 followed by one or more numbers
    if ( regex_match( keyword, zeropadding ) ) {
      message.str(""); message << "ERROR keyword \"" << keyword << "\" cannot contain 0-padded numbers";
      logwrite( function, message.str() );
      return( ERROR );
    }

    keystring = tokens[1];                                                 // tokenize the rest in a moment

    size_t pos = keystring.find(comment_separator);                        // location of the comment separator
    keyvalue = keystring.substr(0, pos);                                   // keyvalue is everything up to comment
    keyvalue = keyvalue.erase(0, keyvalue.find_first_not_of(" "));         // remove leading spaces from keyvalue
    keyvalue = keyvalue.erase(keyvalue.find_last_not_of(" ")+1);           // remove trailing spaces from keyvalue

    if (pos != std::string::npos) {
      keycomment = keystring.erase(0, pos + comment_separator.length());
      keycomment = keycomment.erase(0, keycomment.find_first_not_of(" ")); // remove leading spaces from keycomment
    }

    // Delete the keydb entry for associated keyword if the keyvalue is a sole period '.'
    //
    if (keyvalue == ".") {
      fits_key_t::iterator ii = this->keydb.find(keyword);
      if (ii==this->keydb.end()) {
        message.str(""); message << "keyword " << keyword << " not found";
        logwrite(function, message.str());
      }
      else {
        this->keydb.erase(ii);
        message.str(""); message << "keyword " << keyword << " erased";
        logwrite(function, message.str());
      }
      return(NO_ERROR);
    }

    // keyvalue limited to 68 bytes
    //
    if ( keyvalue.length() > 68 ) {
      message.str(""); message << "ERROR keyvalue \"" << keyvalue << "\" cannot exceed 68 bytes";
      logwrite( function, message.str() );
      return( ERROR );
    }

    keytype = this->get_keytype(keyvalue);

    if ( keytype=="ERROR" ) {
      message.str(""); message << "ERROR determining type for keyvalue \"" << keyvalue << "\"";
      logwrite( function, message.str() );
      return( ERROR );
    }

    if ( keytype=="INT" || keytype=="LONG" || keytype=="FLOAT" || keytype=="DOUBLE" ) {
      if ( keyvalue.length() > 20 ) {
        message.str(""); message << "ERROR numerical keyvalue \"" << keyvalue << "\" cannot exceed 20 bytes";
        logwrite( function, message.str() );
        return( ERROR );
      }
    }

    if ( keyvalue.length() + keycomment.length() > 68 ) {
      int limit = 68 - keyvalue.length();
      keycomment = keycomment.substr(0, limit);
      message.str(""); message << "NOTICE comment truncated to fit record limit: \"" << keycomment << "\"";
      logwrite( function, message.str() );
    }

    // check for instances of the comment separator in keycomment
    //
    if (keycomment.find(comment_separator) != std::string::npos) {
      message.str(""); message << "ERROR: FITS comment delimiter: found too many instancces of " << comment_separator << " in keycomment";
      logwrite( function, message.str() );
      return(NO_ERROR);
    }

    // insert new entry into the database
    //
    this->keydb[keyword].keyword    = keyword;
    this->keydb[keyword].keytype    = keytype;
    this->keydb[keyword].keyvalue   = keyvalue;
    this->keydb[keyword].keycomment = keycomment;

//#ifdef LOGLEVEL_DEBUG
//    message.str(""); message << "[DEBUG] added key: " << keyword << "=" << keyvalue << " (" << this->keydb[keyword].keytype << ") // " << keycomment;
//    logwrite( function, message.str() );
//#endif

    return(NO_ERROR);
  }
  /** Common::FitsKeys::addkey ************************************************/


  /***** Common::FitsKeys::delkey *********************************************/
  /**
   * @brief      delete FITS keyword from internal database
   * @param[in]  keyword
   * @return     ERROR for improper input arg, otherwise NO_ERROR
   *
   */
  long FitsKeys::delkey( std::string keyword ) {
    std::string function = "Common::FitsKeys::delkey";
    std::stringstream message;

    try {
      std::transform( keyword.begin(), keyword.end(), keyword.begin(), ::toupper );    // make uppercase
    }
    catch (...) {
      message.str(""); message << "converting keyword " << keyword << " to uppercase";
      logwrite( function, message.str() );
      return(ERROR);
    }

    // Delete the keydb entry for associated keyword (if found).
    // Don't report anything if keyword is not found.
    //
    fits_key_t::iterator keyit = this->keydb.find( keyword );
    if ( keyit != this->keydb.end() ) {
      this->keydb.erase( keyit );
      message.str(""); message << "keyword " << keyword << " erased";
      logwrite( function, message.str() );
    }
    return(NO_ERROR);
  }
  /***** Common::FitsKeys::delkey *********************************************/

}
