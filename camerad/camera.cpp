/**
 * @file    camera.cpp
 * @brief   camera interface functions common to all camera interfaces
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */
#include <iostream>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>
#include <fstream>
#include <algorithm>  //!< vector iterators, find, count
#include <functional> //!< pass by reference to threads

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camera.h"

namespace Camera {

  Camera::Camera() {
    this->is_mexamps = false;            // don't force amplifiers to be written as multi-extension cubes
    this->is_longerror = false;
    this->is_coadd = false;
    this->is_mex = false;
    this->image_dir = "/tmp";
    this->dirmode = 0;                   // user specified mode to OR with 0700 for imdir creation
    this->base_name = "image";
    this->image_num = 0;
    this->fits_naming = "time";
    this->fitstime = "";
    this->abortstate.store(false);
    this->exposing.store(false);
    this->writekeys_when = "before";
    this->autodir_state = true;
    this->default_roi = "1024 1024";
    this->default_sampmode = "2 2 1";
    this->default_exptime = "0";
  }

  Camera::~Camera() {
  }

  /** Camera::Camera::log_error ***********************************************/
  /**
   * @fn     log_error
   * @brief  logs the error and saves the message to be returned on the command port
   * @param  std::string function name
   * @param  std::string message (error)
   * @return ERROR or NO_ERROR
   *
   */
  void Camera::log_error( std::string function, std::string message ) {
    std::stringstream err;

    // Save this message in class variable
    this->lasterrorstring.str("");
    this->lasterrorstring << message;

    // Form an error string as "ERROR: <message>"
    err << "ERROR: " << this->lasterrorstring.str();

    // Log and send to async port in the usual ways
    //
    logwrite( function, err.str() );
    this->async.enqueue( err.str() );
  }
  /** Camera::Camera::log_error ***********************************************/


  /** Camera::Camera::get_longerror *******************************************/
  /**
   * @fn     get_longerror
   * @brief  return the saved error message
   * @param  none
   * @return std::string message
   *
   * If is_longerror is set (true) then return the last saved error message
   * in lasterrorstring, then erase that string.
   *
   * If is_longerror is clear (false) then return an empty string.
   *
   */
  std::string Camera::get_longerror() {
    std::string err = ( this->is_longerror ? ( " " + this->lasterrorstring.str() ) : "" );
    this->lasterrorstring.str("");
    return ( err );
  }
  /** Camera::Camera::get_longerror *******************************************/


  /** Camera::Camera::writekeys ***********************************************/
  /**
   * @fn     writekeys
   * @brief  set or get the writekeys_when value
   * @param  std::string writekeys_in
   * @param  std::string& writekeys_out
   * @return ERROR or NO_ERROR
   *
   */
  long Camera::writekeys(std::string writekeys_in, std::string &writekeys_out) {
    std::string function = "Camera::Camera::writekeys";
    std::stringstream message;
    long error = NO_ERROR;

    if ( !writekeys_in.empty() ) {
      try {
        std::transform( writekeys_in.begin(), writekeys_in.end(), writekeys_in.begin(), ::tolower );  // make lowercase
        if ( writekeys_in == "before" || writekeys_in == "after" ) this->writekeys_when = writekeys_in;
        else {
          message.str(""); message << writekeys_in << " is invalid. Expecting before or after";
          this->log_error( function, message.str() );
          error = ERROR;
        }
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << writekeys_in;
        this->log_error( function, message.str() );
        return( ERROR );
      }
    }

    writekeys_out = this->writekeys_when;
    return error;
  }
  /** Camera::Camera::writekeys ***********************************************/


  /** Camera::Camera::fitsnaming **********************************************/
  /**
   * @fn     fitsnaming
   * @brief  set or get the fits naming type
   * @param  std::string naming_in
   * @param  std::string& naming_out
   * @return ERROR or NO_ERROR
   *
   */
  long Camera::fitsnaming(std::string naming_in, std::string& naming_out) {
    std::string function = "Camera::Camera::fitsnaming";
    std::stringstream message;
    long error;

    message.str(""); message << "fits naming: " << this->fits_naming;

    if (naming_in.empty()) {           // no string passed so this is a request. do nothing but will return the current value
      error = NO_ERROR;
    }
    else
    if ( (naming_in.compare("time")==0) || (naming_in.compare("number")==0) ) {
      this->fits_naming = naming_in;   // set new value
      error = NO_ERROR;
    }
    else {
      message.str(""); message << "invalid naming type: " << naming_in << ". Must be \"time\" or \"number\".";
      error = ERROR;
    }

    error == NO_ERROR ? logwrite(function, message.str()) : this->log_error( function, message.str() );
    naming_out = this->fits_naming;    // return the current value
    return error;
  }
  /** Camera::Camera::fitsnaming **********************************************/


  /** Camera::Camera::imnum ***************************************************/
  /**
   * @fn     imnum
   * @brief  set or get the image_num member
   * @param  std::string num_in
   * @param  std::string& num_out
   * @return ERROR or NO_ERROR
   *
   */
  long Camera::imnum(std::string num_in, std::string& num_out) {
    std::string function = "Camera::Camera::imnum";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if (num_in.empty()) {
      message.str(""); message << "image number: " << this->image_num;
      logwrite(function, message.str());
      num_out = std::to_string(this->image_num);
      return(NO_ERROR);
    }

    else {                             // Otherwise check the incoming value
      int num;
      try {
        num = std::stoi(num_in);
      }
      catch (std::invalid_argument &) {
        this->log_error( function, "invalid number: unable to convert to integer" );
        return(ERROR);
      }
      catch (std::out_of_range &) {
        this->log_error( function, "imnum out of integer range" );
        return(ERROR);
      }
      if (num < 0) {                   // can't be negative
        message.str(""); message << "requested image number " << num << " must be >= 0";
        this->log_error( function, message.str() );
        return(ERROR);
      }
      else {                           // value is OK
        this->image_num = num;
        num_out = num_in;
        return(NO_ERROR);
      }
    }
  }
  /** Camera::Camera::imnum ***************************************************/


  /** Camera::Camera::basename ************************************************/
  /**
   * @fn     basename
   * @brief  set or get the base_name member
   * @param  std::string name_in
   * @param  std::string& name_out
   * @return NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value.
   * The only restriction on base name is that it can't contain a '/' character.
   *
   */
  long Camera::basename(std::string name_in) {
    std::string dontcare;
    return( basename(name_in, dontcare) );
  }
  long Camera::basename(std::string name_in, std::string& name_out) {
    std::string function = "Camera::Camera::basename";
    std::stringstream message;
    long error=NO_ERROR;

    // Base name cannot contain a "/" because that would be a subdirectory,
    // and subdirectories are not checked here, only by imdir command.
    //
    if (  name_in.find('/') != std::string::npos ) {
      this->log_error( function, "basename cannot contain a '/' character" );
      error = ERROR;
    }
    else if ( !name_in.empty() ) {     // If a name is supplied
      this->base_name = name_in;       // then set the image name.
      error = NO_ERROR;
    }

    // In any case, log and return the current value.
    //
    message.str(""); message << "base name is " << this->base_name;
    logwrite(function, message.str());
    name_out = this->base_name;

    return(error);
  }
  /** Camera::Camera::basename ************************************************/


  /** Camera::Camera::imdir ***************************************************/
  /**
   * @fn     imdir
   * @brief  set or get the image_dir base directory
   * @param  std::string dir_in
   * @param  std::string& dir_out (pass reference for return value)
   * @return ERROR or NO_ERROR
   *
   * This function is overloaded with a form that doesn't use a return value reference.
   *
   * The base directory for images is this->image_dir. It is set (or read) here. It
   * may contain any number of subdirectories. This function will try to create any
   * needed subdirectories if they don't already exist.  If autodir is set then a 
   * UTC date subdirectory is added later, in the get_fitsname() function.
   *
   */
  long Camera::imdir(std::string dir_in) {
    std::string dontcare;
    return( imdir(dir_in, dontcare) );
  }
  long Camera::imdir(std::string dir_in, std::string& dir_out) {
    std::string function = "Camera::Camera::imdir";
    std::stringstream message;
    std::vector<std::string> tokens;
    long error = NO_ERROR;

    // Tokenize the input string on the '/' character to get each requested
    // subdirectory as a separate token.
    //
    Tokenize(dir_in, tokens, "/");

    std::stringstream nextdir;  // the next subdirectory to check and/or create

    // Loop through each requested subdirectory to check if they exist.
    // Try to create them if they don't exist.
    //
    for ( auto tok : tokens ) {

      // The next directory to create --
      // start from the bottom and append each successive token.
      //
      nextdir << "/" << tok;

      // Check if each directory exists
      //
      DIR *dirp;                                             // pointer to the directory
      if ( (dirp = opendir(nextdir.str().c_str())) == NULL ) {
        // If directory doesn't exist then try to create it.
        //
        if ( ( mkdir( nextdir.str().c_str(), (S_IRWXU | this->dirmode) ) ) == 0 ) {
          message.str(""); message << "created directory " << nextdir.str();
          logwrite(function, message.str());
        }
        else {                                               // error creating date subdirectory
          message.str("");
          message << "creating directory " << nextdir.str() << ": " << strerror(errno);
          this->log_error( function, message.str() );
          error = ERROR;
          break;
        }
      }
      else {
        closedir(dirp);                                      // directory already existed so close it
      }
    }

    // Make sure the directory can be written to by writing a test file.
    //
    if ( error == NO_ERROR && !dir_in.empty() ) {
      try {
        std::string testfile;
        testfile = dir_in + "/.tmp";
        FILE* fp = std::fopen(testfile.c_str(), "w");    // create the test file
        if (!fp) {
          message.str(""); message << "cannot write to requested image directory " << dir_in;
          this->log_error( function, message.str() );
          error = ERROR;
        }
        else {                                           // remove the test file
          std::fclose(fp);
          if (std::remove(testfile.c_str()) != 0) {
            message.str(""); message << "removing temporary file " << testfile;
            this->log_error( function, message.str() );
            error = ERROR;
          }
        }
      }
      catch(...) {
        message.str(""); message << "writing to " << dir_in;
        this->log_error( function, message.str() );
        error = ERROR;
      }
      if ( error == NO_ERROR) this->image_dir = dir_in;    // passed all tests so set the image_dir
    }

    // In any case, return the current value.
    //
    message.str(""); message << "image directory: " << this->image_dir;
    logwrite(function, message.str());
    dir_out = this->image_dir;
    return( error );
  }
  /** Camera::Camera::imdir ***************************************************/


  /** Camera::Camera::autodir *************************************************/
  /**
   * @fn     autodir
   * @brief  set or get autodir_state used for creating UTC date subdirectory
   * @param  std::string dir_in
   * @param  std::string& dir_out (pass reference for return value)
   * @return ERROR or NO_ERROR
   *
   * The base directory for images is this->image_dir. It is set (or read) here. It
   * is not created; it must already exist. The date subdirectory is added later, in
   * the get_fitsname() function.
   *
   */
  long Camera::autodir(std::string state_in, std::string& state_out) {
    std::string function = "Camera::Camera::autodir";
    std::stringstream message;
    long error = NO_ERROR;

    if ( !state_in.empty() ) {
      try {
        bool verifiedstate;
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );  // make lowercase
        if ( state_in == "no"  ) verifiedstate = false;
        else
        if ( state_in == "yes" ) verifiedstate = true;
        else {
          message.str(""); message << state_in << " is invalid.  Expecting yes or no";
          this->log_error( function, message.str() );
          error = ERROR;
        }
        if ( error == NO_ERROR ) this->autodir_state = verifiedstate;
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << state_in;
        this->log_error( function, message.str() );
        return( ERROR );
      }
    }

    // set the return value and report the state now, either setting or getting
    //
    state_out = this->autodir_state ? "yes" : "no";
    message.str("");
    message << "autodir is " << ( this->autodir_state ? "ON" : "OFF" );
    logwrite( function, message.str() );

    return error;

  }
  /** Camera::Camera::autodir *************************************************/



  /***** Camera::Camera:set_fitstime ******************************************/
  /**
   * @brief      set the "fitstime" variable used for the filename
   * @param[in]  string formatted as "YYYY-MM-DDTHH:MM:SS.sss"
   * @return     std::string formatted as "YYYYMMDDHHMMSS"
   *
   * The Camera class has a public string variable "fitstime" which is
   * to be used for the FITS filename, when the time-format is selected.
   * This time should be the whole-second of the time that the exposure
   * was started, so that time is passed in here. This function strips
   * that string down to just the numerals for use in the filename.
   *
   */
  void Camera::set_fitstime(std::string time_in) {
    std::string function = "Camera::Camera::set_fitstime";
    std::stringstream message;

    if ( time_in.length() != 23 ) {  // wrong number of characters, input can't be formatted correctly
      message.str(""); message << "ERROR: bad input time \"" << time_in << "\""
                               << " has " << time_in.length() << " chars but expected 23";
      logwrite(function, message.str());
      this->fitstime = "99999999999999";
      return;
    }

    this->fitstime = time_in.substr(0,4)     // YYYY
                   + time_in.substr(5,2)     // MM
                   + time_in.substr(8,2)     // DD
                   + time_in.substr(11,2)    // HH
                   + time_in.substr(14,2)    // MM
                   + time_in.substr(17,2);   // SS

    return;
  }
  /***** Camera::Camera:set_fitstime ******************************************/


  /** Camera::Camera:get_fitsname *********************************************/
  /**
   * @fn     get_fitsname
   * @brief  assemble the FITS filename
   * @param  std::string controllerid (optional, due to overloading)
   * @param  std::string &name_out reference for name
   * @return ERROR or NO_ERROR
   *
   * This function assembles the fully qualified path to the output FITS filename
   * using the parts (dir, basename, time or number) stored in the Camera::Camera class.
   * If the filename already exists then a -number is inserted, incrementing that
   * number until a unique name is achieved.
   *
   * This function is overloaded, to allow passing a controller id to include in the filename.
   *
   */
  long Camera::get_fitsname(std::string &name_out) {
    return ( this->get_fitsname("", name_out) );
  }
  long Camera::get_fitsname(std::string controllerid, std::string &name_out) {
    std::string function = "Camera::Camera::get_fitsname";
    std::stringstream message;
    std::stringstream fn, fitsname;

    // image_dir is the requested base directory and now optionaly add on the date directory
    //
    std::stringstream basedir;
    if ( this->autodir_state ) basedir << this->image_dir << "/" << get_system_date();
    else                       basedir << this->image_dir;

    // Make sure the directory exists
    //
    DIR *dirp;                                             // pointer to the directory
    if ( (dirp = opendir(basedir.str().c_str())) == NULL ) {
      // If directory doesn't exist then try to create it.
      // Note that this only creates the bottom-level directory, the added date part.
      // The base directory has to exist.
      //
      if ( ( mkdir( basedir.str().c_str(), (S_IRWXU | this->dirmode ) ) ) == 0 ) {
        message.str(""); message << "created directory " << basedir.str();
        logwrite(function, message.str());
      }
      else {                                               // error creating date subdirectory
        message.str("");
        message << "code " << errno << " creating directory " << basedir.str() << ": " << strerror(errno);
        this->log_error( function, message.str() );
        // a common error might be that the base directory doesn't exist
        //
        if (errno==ENOENT) {
          message.str("");
          message << "requested base directory " << basedir.str() << " does not exist";
          this->log_error( function, message.str() );
        }
        return(ERROR);
      }
    }
    else {
      closedir(dirp);                                      // directory already existed so close it
    }

    // start building the filename with directory/basename_
    // where "basedir" was just assembled above
    //
    fitsname.str("");
    fitsname << basedir.str() << "/" << this->base_name;

    // add the controllerid if one is given
    //
    if ( ! controllerid.empty() ) {
      fitsname << controllerid << "_";
    }

    // add the time or number suffix
    //
    if (this->fits_naming.compare("time")==0) {
      fitsname << this->fitstime;
    }
    else
    if (this->fits_naming.compare("number")==0) {
      // width of image_num portion of the filename is at least 4 digits, and grows as needed
      //
      int width = (this->image_num < 10000 ? 4 :   
                  (this->image_num < 100000 ? 5 :   
                  (this->image_num < 1000000 ? 6 :   
                  (this->image_num < 10000000 ? 7 :  
                  (this->image_num < 100000000 ? 8 :  
                  (this->image_num < 1000000000 ? 9 : 10)))))); 
      fitsname << std::setfill('0') << std::setw(width) << this->image_num;
    }

    // Check if file exists and include a -# to set apart duplicates.
    //
    struct stat st;
    int dupnumber=1;
    fn.str(""); fn << fitsname.str();
    fn << ".fits";
    while (stat(fn.str().c_str(), &st) == 0) {
      fn.str(""); fn << fitsname.str();
      fn << "-" << dupnumber << ".fits";
      dupnumber++;  // keep incrementing until we have a unique filename
    }

#ifdef LOGLEVEL_DEBUG
//  message.str(""); message << "[DEBUG] fits_naming=" << this->fits_naming 
//                           << " controllerid=" << controllerid
//                           << " will write to file: " << fn.str();
//  logwrite(function, message.str());
#endif

    name_out = fn.str();
    return(NO_ERROR);
  }
  /** Camera::Camera:get_fitsname *********************************************/


  /***** Camera::Camera::coadd ************************************************/
  /**
   * @brief      set or get the coadd state
   * @param[in]  state_in
   * @param[out] state_out
   * @return     true or false
   *
   * The state_in string should be "True" or "False", case-insensitive.
   * This function is here to handle a coadd command from the user interface;
   * internally, use the inline function instead.
   *
   */
  long Camera::coadd( std::string state_in, std::string &state_out ) {
    std::string function = "Camera::Camera::coadd";
    std::stringstream message;
    int error = NO_ERROR;

    // If something is passed then try to use it to set the multi-extension state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );    // make lowercase
        if (state_in == "false" ) this->is_coadd = false;
        else
        if (state_in == "true"  ) this->is_coadd = true;
        else {
          message.str(""); message << state_in << " is invalid. Expecting true or false";
          this->log_error( function, message.str() );
          error = ERROR;
        }
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << state_in;
        this->log_error( function, message.str() );
        error = ERROR;
      }
    }

    // error or not, the state reported is whatever was last successfully set
    //
    state_out = (this->is_coadd ? "true" : "false");
    logwrite( function, state_out );
    message.str(""); message << "NOTICE:coadd=" << state_out;
    this->async.enqueue( message.str() );

    // and this lets the server know if it was set or not
    //
    return( error );
  }
  /***** Camera::Camera::coadd ************************************************/


  /** Camera::Camera::mex *****************************************************/
  /**
   * @brief  set or get the multi-extension state
   * @param  std::string state_in
   * @return true or false
   *
   * The state_in string should be "True" or "False", case-insensitive.
   *
   * This function is overloaded.
   *
   */
  void Camera::mex(bool state_in) {                                       // write-only boolean
    std::string dontcare;
    this->mex( (state_in ? "true" : "false"), dontcare );
  }
  bool Camera::mex() {                                                    // read-only boolean
    return ( this->is_mex );
  }
  long Camera::mex(std::string state_in, std::string &state_out) {        // read-write string, called from server
    std::string function = "Camera::Camera::mex";
    std::stringstream message;
    int error = NO_ERROR;

    // If something is passed then try to use it to set the multi-extension state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );    // make lowercase
        if (state_in == "false" ) this->is_mex = false;
        else
        if (state_in == "true"  ) this->is_mex = true;
        else {
          message.str(""); message << state_in << " is invalid. Expecting true or false";
          this->log_error( function, message.str() );
          error = ERROR;
        }
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << state_in;
        this->log_error( function, message.str() );
        error = ERROR;
      }
    }

    // error or not, the state reported is whatever was last successfully set
    //
    state_out = (this->is_mex ? "true" : "false");
    logwrite( function, state_out );
    message.str(""); message << "NOTICE:mex=" << state_out;
    this->async.enqueue( message.str() );

    // and this lets the server know if it was set or not
    //
    return( error );
  }
  /** Camera::Camera::mex *****************************************************/



  /** Camera::Camera::longerror ***********************************************/
  /**
   * @fn     longerror
   * @brief  set or get the longerror state
   * @param  std::string state_in
   * @return true or false
   *
   * The state_in string should be "True" or "False", case-insensitive.
   *
   * This function is overloaded.
   *
   */
  void Camera::longerror(bool state_in) {                                 // write-only boolean
    std::string dontcare;
    this->longerror( (state_in ? "true" : "false"), dontcare );
  }
  bool Camera::longerror() {                                              // read-only boolean
    return ( this->is_longerror );
  }
  long Camera::longerror(std::string state_in, std::string &state_out) {  // read-write string, called from server
    std::string function = "Camera::Camera::longerror";
    std::stringstream message;
    int error = NO_ERROR;

    // If something is passed then try to use it to set the longerror state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );    // make lowercase
        if (state_in == "false" ) this->is_longerror = false;
        else
        if (state_in == "true"  ) this->is_longerror = true;
        else {
          message.str(""); message << state_in << " is invalid. Expecting true or false";
          this->log_error( function, message.str() );
          error = ERROR;
        }
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << state_in;
        this->log_error( function, message.str() );
        error = ERROR;
      }
    }

    // error or not, the state reported is whatever was last successfully set
    //
    state_out = (this->is_longerror ? "true" : "false");
    logwrite( function, state_out );
    message.str(""); message << "NOTICE:longerror=" << state_out;
    this->async.enqueue( message.str() );

    // and this lets the server know if it was set or not
    //
    return( error );
  }
  /** Camera::Camera::longerror ***********************************************/


  /***** Camera::Camera::mexamps **********************************************/
  /**
   * @brief      set or get the mexamps state
   * @param[in]  std::string state_in
   * @return     true or false
   *
   * The state_in string should be "True" or "False", case-insensitive.
   *
   * This function is overloaded.
   *
   * mex also gets enabled/disabled along with mexamps. If mex (multi-extension)
   * is needed after disabling mexamps then it must be separately enabled.
   *
   */
  void Camera::mexamps(bool state_in) {                                  // write-only boolean
    std::string dontcare;
    this->mexamps( (state_in ? "true" : "false"), dontcare );
  }
  /***** Camera::Camera::mexamps **********************************************/


  /***** Camera::Camera::mexamps **********************************************/
  /**
   * @brief      set or get the mexamps state
   * @details    this version accepts no inputs and returns only the state
   * @return     true or false
   *
   * This function is overloaded.
   *
   */
  bool Camera::mexamps() {                                               // read-only boolean
    return ( this->is_mexamps );
  }
  /***** Camera::Camera::mexamps **********************************************/


  /***** Camera::Camera::mexamps **********************************************/
  /**
   * @brief      set or get the mexamps state
   * @details    this function is overloaded
   * @param[in]  state_in   string containing the requested state
   * @param[out] state_out  reference to string containing the current state
   * @return     ERROR or NO_ERROR
   *
   * The state_in string should be "True" or "False", case-insensitive.
   * update: can't allow this to be true for NIRC2 but Keck still wants
   *         to be able to send the command.
   *
   */
  long Camera::mexamps(std::string state_in, std::string &state_out) {   // read-write string, called from server
    std::string function = "Camera::Camera::mexamps";
    std::stringstream message;
    int error = NO_ERROR;

    // If something is passed then try to use it to set the mexamps state
    //
    if ( !state_in.empty() ) {
      try {
        std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::tolower );    // make lowercase
        if (state_in == "false" ) {
          this->is_mexamps = false;
          this->is_mex      = false;
        }
/** can't allow this to be true for NIRC2
        else
        if (state_in == "true"  ) {
          this->is_mexamps = true;
          this->is_mex      = true;
        }
**/
        else {
          message.str(""); message << state_in << " is invalid. NIRC2 requires this to be false.";
          this->log_error( function, message.str() );
          error = ERROR;
        }
      }
      catch (...) {
        message.str(""); message << "unknown exception parsing argument: " << state_in;
        this->log_error( function, message.str() );
        error = ERROR;
      }
    }

    // error or not, the state reported is whatever was last successfully set
    //
    state_out = (this->is_mexamps ? "true" : "false");
    logwrite( function, state_out );
    message.str(""); message << "NOTICE:mexamps=" << state_out;
    this->async.enqueue( message.str() );

    // and this lets the server know if it was set or not
    //
    return( error );
  }
  /***** Camera::Camera::mexamps **********************************************/


  /**************** Camera::Information::pre_exposures ************************/
  /**
   * @fn     pre_exposures
   * @brief  set/get pre-exposures
   * @param  string num_in   incoming value
   * @param  string &num_out return value
   * @return ERROR or NO_ERROR
   *
   * Get / set number of pre-exposures, which are exposures taken by the
   * controller but are not saved. This number is stored in the
   * Camera:Information class and will show up in the camera_info object.
   *
   */
  long Information::pre_exposures( std::string num_in, std::string &num_out ) {
    std::string function = "Camera::Information::pre_exposures";
    std::stringstream message;

    // If no string is passed then this is a request; return the current value.
    //
    if ( num_in.empty() ) {
      message.str(""); message << "pre-exposures: " << this->num_pre_exposures;
      logwrite( function, message.str() );
      num_out = std::to_string( this->num_pre_exposures );
      return( NO_ERROR );
    }

    else {                             // Otherwise check the incoming value
      int num;
      try {
        num = std::stoi( num_in );     // convert incoming string to integer
      }
      catch ( std::invalid_argument & ) {
        message.str(""); message << "ERROR: invalid number: unable to convert " << num_in << " to integer";
        logwrite( function, message.str() );
        return( ERROR );
      }
      catch ( std::out_of_range & ) {
        message.str(""); message << "ERROR: " << num_in << " out of integer range";
        logwrite( function, message.str() );
        return( ERROR );
      }
      if ( num < 0 ) {                 // can't be negative
        message.str(""); message << "ERROR: requested pre-exposures " << num << " must be >= 0";
        logwrite( function, message.str() );
        return( ERROR );
      }
      else {                           // incoming value is OK
        this->num_pre_exposures = num; // set the class variable
        num_out = num_in;              // set the return string value
        return( NO_ERROR );
      }
    }
  }
  /**************** Camera::Information::pre_exposures ************************/


  void Information::swap(Information& other) noexcept {
    std::swap(det_id, other.det_id);
    std::swap(amp_id, other.amp_id);
    std::swap(framenum, other.framenum);
    std::swap(serial_prescan, other.serial_prescan);
    std::swap(serial_overscan, other.serial_overscan);
    std::swap(parallel_overscan, other.parallel_overscan);
    std::swap(image_cols, other.image_cols);
    std::swap(image_rows, other.image_rows);
    std::swap(det_name, other.det_name);
    std::swap(amp_name, other.amp_name);
    std::swap(detector, other.detector);
    std::swap(detector_software, other.detector_software);
    std::swap(detector_firmware, other.detector_firmware);
    std::swap(pixel_scale, other.pixel_scale);
    std::swap(det_gain, other.det_gain);
    std::swap(read_noise, other.read_noise);
    std::swap(dark_current, other.dark_current);
    std::swap(image_size, other.image_size);
    std::swap(ccdsec, other.ccdsec);
    std::swap(ampsec, other.ampsec);
    std::swap(trimsec, other.trimsec);
    std::swap(datasec, other.datasec);
    std::swap(biassec, other.biassec);
    std::swap(detsec, other.detsec);
    std::swap(detsize, other.detsize);
    std::swap(detid, other.detid);
    std::swap(gain, other.gain);
    std::swap(fits_compression_code, other.fits_compression_code);
    std::swap(fits_compression_type, other.fits_compression_type);
    std::swap(fits_noisebits, other.fits_noisebits);
    std::swap(frame_exposure_time, other.frame_exposure_time);
    std::swap(directory, other.directory);
    std::swap(image_name, other.image_name);
    std::swap(basename, other.basename);
    std::swap(bitpix, other.bitpix);
    std::swap(naxes, other.naxes);
    std::swap(frame_type, other.frame_type);
    std::swap(detector_pixels, other.detector_pixels);
    std::swap(section_size, other.section_size);
    std::swap(image_memory, other.image_memory);
    std::swap(current_observing_mode, other.current_observing_mode);
    std::swap(readout_name, other.readout_name);
    std::swap(readout_type, other.readout_type);
    std::swap(naxis, other.naxis);
    std::swap(axes, other.axes);
    std::swap(binning, other.binning);
    std::swap(axis_pixels, other.axis_pixels);
    std::swap(region_of_interest, other.region_of_interest);
    std::swap(abortexposure, other.abortexposure);
    std::swap(activebufs, other.activebufs);
    std::swap(datatype, other.datatype);
    std::swap(type_set, other.type_set);
    std::swap(pixel_time, other.pixel_time);
    std::swap(pixel_skip_time, other.pixel_skip_time);
    std::swap(row_overhead_time, other.row_overhead_time);
    std::swap(row_skip_time, other.row_skip_time);
    std::swap(frame_start_time, other.frame_start_time);
    std::swap(fs_pulse_time, other.fs_pulse_time);
    std::swap(cubedepth, other.cubedepth);
    std::swap(fitscubed, other.fitscubed);
    std::swap(ncoadd, other.ncoadd);
    std::swap(nslice, other.nslice);
    std::swap(image_center, other.image_center);
    std::swap(imwidth, other.imwidth);
    std::swap(imheight, other.imheight);
    std::swap(imwidth_read, other.imwidth_read);
    std::swap(imheight_read, other.imheight_read);
    std::swap(exposure_aborted, other.exposure_aborted);
    std::swap(iscds, other.iscds);
    std::swap(nmcds, other.nmcds);
    std::swap(ismex, other.ismex);
//  std::swap(extension, other.extension);
    int temp = extension.load();
    extension.store(other.extension.load());
    other.extension.store(temp);
    std::swap(shutterenable, other.shutterenable);
    std::swap(shutteractivate, other.shutteractivate);
    std::swap(exposure_time, other.exposure_time);
    std::swap(exposure_delay, other.exposure_delay);
    std::swap(requested_exptime, other.requested_exptime);
    std::swap(readouttime, other.readouttime);
    std::swap(exposure_unit, other.exposure_unit);
    std::swap(exposure_factor, other.exposure_factor);
    std::swap(exposure_progress, other.exposure_progress);
    std::swap(num_pre_exposures, other.num_pre_exposures);
    std::swap(is_cds, other.is_cds);
    std::swap(nseq, other.nseq);
    std::swap(nexp, other.nexp);
    std::swap(num_coadds, other.num_coadds);
    std::swap(sampmode, other.sampmode);
    std::swap(sampmode_ext, other.sampmode_ext);
    std::swap(sampmode_frames, other.sampmode_frames);
    std::swap(fits_name, other.fits_name);
    std::swap(cmd_start_time, other.cmd_start_time);
    std::swap(start_time, other.start_time);
    std::swap(stop_time, other.stop_time);
    std::swap(amp_section, other.amp_section);
    std::swap(userkeys, other.userkeys);
    std::swap(systemkeys, other.systemkeys);
    std::swap(extkeys, other.extkeys);
  }

}
