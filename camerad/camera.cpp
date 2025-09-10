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
    /** Camera::Camera::abort ***************************************************/
    /**
     * @fn     abort
     * @brief  abort the current operation (exposure, readout, etc.)
     * @param  none
     * @return none
     *
     */
    void Camera::abort() {
        std::string function = "Camera::Camera::abort";
        std::stringstream message;
        this->abortstate = true;
        logwrite(function, "received abort");
        return;
    }

    /** Camera::Camera::abort ***************************************************/

    void Camera::set_abortstate(bool state) {
        this->abort_mutex.lock();
        this->abortstate = state;
        this->_abortstate = state;
        this->abort_mutex.unlock();
    }

    bool Camera::get_abortstate() {
        bool state;
        this->abort_mutex.lock();
        state = this->abortstate;
        this->abort_mutex.unlock();
        return state;
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
    void Camera::log_error(std::string function, std::string message) {
        std::stringstream err;

        // Save this message in class variable
        this->lasterrorstring.str("");
        this->lasterrorstring << message;

        // Form an error string as "ERROR: <message>"
        err << "ERROR: " << this->lasterrorstring.str();

        // Log and send to async port in the usual ways
        //
        logwrite(function, err.str());
        this->async.enqueue(err.str());
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
        std::string err = (this->is_longerror ? (" " + this->lasterrorstring.str()) : "");
        this->lasterrorstring.str("");
        return (err);
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

        if (!writekeys_in.empty()) {
            try {
                std::transform(writekeys_in.begin(), writekeys_in.end(), writekeys_in.begin(), ::tolower);
                // make lowercase
                if (writekeys_in == "before" || writekeys_in == "after") this->writekeys_when = writekeys_in;
                else {
                    message.str("");
                    message << writekeys_in << " is invalid. Expecting before or after";
                    this->log_error(function, message.str());
                    error = ERROR;
                }
            } catch (...) {
                message.str("");
                message << "unknown exception parsing argument: " << writekeys_in;
                this->log_error(function, message.str());
                return ERROR;
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
    long Camera::fitsnaming(std::string naming_in, std::string &naming_out) {
        std::string function = "Camera::Camera::fitsnaming";
        std::stringstream message;
        long error;

        message.str("");
        message << "fits naming: " << this->fits_naming;

        if (naming_in.empty()) {
            // no string passed so this is a request. do nothing but will return the current value
            error = NO_ERROR;
        } else if ((naming_in.compare("time") == 0) || (naming_in.compare("number") == 0)) {
            this->fits_naming = naming_in; // set new value
            error = NO_ERROR;
        } else {
            message.str("");
            message << "invalid naming type: " << naming_in << ". Must be \"time\" or \"number\".";
            error = ERROR;
        }

        error == NO_ERROR ? logwrite(function, message.str()) : this->log_error(function, message.str());
        naming_out = this->fits_naming; // return the current value
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
    long Camera::imnum(std::string num_in, std::string &num_out) {
        std::string function = "Camera::Camera::imnum";
        std::stringstream message;

        // If no string is passed then this is a request; return the current value.
        //
        if (num_in.empty()) {
            message.str("");
            message << "image number: " << this->image_num;
            logwrite(function, message.str());
            num_out = std::to_string(this->image_num);
            return NO_ERROR;
        } else {
            // Otherwise check the incoming value
            int num;
            try {
                num = std::stoi(num_in);
            } catch (std::invalid_argument &) {
                this->log_error(function, "invalid number: unable to convert to integer");
                return ERROR;
            }
            catch (std::out_of_range &) {
                this->log_error(function, "imnum out of integer range");
                return ERROR;
            }
            if (num < 0) {
                // can't be negative
                message.str("");
                message << "requested image number " << num << " must be >= 0";
                this->log_error(function, message.str());
                return ERROR;
            } else {
                // value is OK
                this->image_num = num;
                num_out = num_in;
                return NO_ERROR;
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
        return basename(name_in, dontcare);
    }

    long Camera::basename(std::string name_in, std::string &name_out) {
        std::string function = "Camera::Camera::basename";
        std::stringstream message;
        long error = NO_ERROR;

        // Base name cannot contain a "/" because that would be a subdirectory,
        // and subdirectories are not checked here, only by imdir command.
        //
        if (name_in.find('/') != std::string::npos) {
            this->log_error(function, "basename cannot contain a '/' character");
            error = ERROR;
        } else if (!name_in.empty()) {
            // If a name is supplied
            this->base_name = name_in; // then set the image name.
            error = NO_ERROR;
        }

        // In any case, log and return the current value.
        //
        message.str("");
        message << "base name is " << this->base_name;
        logwrite(function, message.str());
        name_out = this->base_name;

        return error;
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
        return imdir(dir_in, dontcare);
    }

    long Camera::imdir(std::string dir_in, std::string &dir_out) {
        std::string function = "Camera::Camera::imdir";
        std::stringstream message;
        std::vector<std::string> tokens;
        long error = NO_ERROR;

        // Tokenize the input string on the '/' character to get each requested
        // subdirectory as a separate token.
        //
        Tokenize(dir_in, tokens, "/");

        std::stringstream nextdir; // the next subdirectory to check and/or create

        // Loop through each requested subdirectory to check if they exist.
        // Try to create them if they don't exist.
        //
        for (auto tok: tokens) {
            // The next directory to create --
            // start from the bottom and append each successive token.
            //
            nextdir << "/" << tok;

            // Check if each directory exists
            //
            DIR *dirp; // pointer to the directory
            if ((dirp = opendir(nextdir.str().c_str())) == NULL) {
                // If directory doesn't exist then try to create it.
                //
                if ((mkdir(nextdir.str().c_str(), (S_IRWXU | this->dirmode))) == 0) {
                    message.str("");
                    message << "created directory " << nextdir.str();
                    logwrite(function, message.str());
                } else {
                    // error creating date subdirectory
                    message.str("");
                    message << "creating directory " << nextdir.str() << ": " << strerror(errno);
                    this->log_error(function, message.str());
                    error = ERROR;
                    break;
                }
            } else {
                closedir(dirp); // directory already existed so close it
            }
        }

        // Make sure the directory can be written to by writing a test file.
        //
        if (error == NO_ERROR && !dir_in.empty()) {
            try {
                std::string testfile;
                testfile = dir_in + "/.tmp";
                FILE *fp = std::fopen(testfile.c_str(), "w"); // create the test file
                if (!fp) {
                    message.str("");
                    message << "cannot write to requested image directory " << dir_in;
                    this->log_error(function, message.str());
                    error = ERROR;
                } else {
                    // remove the test file
                    std::fclose(fp);
                    if (std::remove(testfile.c_str()) != 0) {
                        message.str("");
                        message << "removing temporary file " << testfile;
                        this->log_error(function, message.str());
                        error = ERROR;
                    }
                }
            } catch (...) {
                message.str("");
                message << "writing to " << dir_in;
                this->log_error(function, message.str());
                error = ERROR;
            }
            if (error == NO_ERROR) this->image_dir = dir_in; // passed all tests so set the image_dir
        }

        // In any case, return the current value.
        //
        message.str("");
        message << "image directory: " << this->image_dir;
        logwrite(function, message.str());
        dir_out = this->image_dir;
        return error;
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
    long Camera::autodir(std::string state_in, std::string &state_out) {
        std::string function = "Camera::Camera::autodir";
        std::stringstream message;
        long error = NO_ERROR;

        if (!state_in.empty()) {
            try {
                bool verifiedstate;
                std::transform(state_in.begin(), state_in.end(), state_in.begin(), ::tolower); // make lowercase
                if (state_in == "no") verifiedstate = false;
                else if (state_in == "yes") verifiedstate = true;
                else {
                    message.str("");
                    message << state_in << " is invalid.  Expecting yes or no";
                    this->log_error(function, message.str());
                    error = ERROR;
                }
                if (error == NO_ERROR) this->autodir_state = verifiedstate;
            } catch (...) {
                message.str("");
                message << "unknown exception parsing argument: " << state_in;
                this->log_error(function, message.str());
                return ERROR;
            }
        }

        // set the return value and report the state now, either setting or getting
        //
        state_out = this->autodir_state ? "yes" : "no";
        message.str("");
        message << "autodir is " << (this->autodir_state ? "ON" : "OFF");
        logwrite(function, message.str());

        return error;
    }

    /** Camera::Camera::autodir *************************************************/


    /** Camera::Camera:set_fitstime *********************************************/
    /**
     * @fn     set_fitstime
     * @brief  set the "fitstime" variable used for the filename
     * @param  string formatted as "YYYY-MM-DDTHH:MM:SS.sssss"
     * @return std::string
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

        if (time_in.length() != 26) {
            // wrong number of characters, input can't be formatted correctly
            message.str("");
            message << "ERROR: bad input time: " << time_in;
            logwrite(function, message.str());
            this->fitstime = "99999999999999";
            return;
        }

        this->fitstime = time_in.substr(0, 4) // YYYY
                         + time_in.substr(5, 2) // MM
                         + time_in.substr(8, 2) // DD
                         + time_in.substr(11, 2) // HH
                         + time_in.substr(14, 2) // MM
                         + time_in.substr(17, 2); // SS

        return;
    }

    /** Camera::Camera:set_fitstime *********************************************/


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
        return (this->get_fitsname("", name_out));
    }

    long Camera::get_fitsname(std::string controllerid, std::string &name_out) {
        std::string function = "Camera::Camera::get_fitsname";
        std::stringstream message;
        std::stringstream fn, fitsname;

        // image_dir is the requested base directory and now optionaly add on the date directory
        //
        std::stringstream basedir;
        if (this->autodir_state) basedir << this->image_dir << "/" << get_system_date();
        else basedir << this->image_dir;

        // Make sure the directory exists
        //
        DIR *dirp; // pointer to the directory
        if ((dirp = opendir(basedir.str().c_str())) == NULL) {
            // If directory doesn't exist then try to create it.
            // Note that this only creates the bottom-level directory, the added date part.
            // The base directory has to exist.
            //
            if ((mkdir(basedir.str().c_str(), (S_IRWXU | this->dirmode))) == 0) {
                message.str("");
                message << "created directory " << basedir.str();
                logwrite(function, message.str());
            } else {
                // error creating date subdirectory
                message.str("");
                message << "code " << errno << " creating directory " << basedir.str() << ": " << strerror(errno);
                this->log_error(function, message.str());
                // a common error might be that the base directory doesn't exist
                //
                if (errno == ENOENT) {
                    message.str("");
                    message << "requested base directory " << basedir.str() << " does not exist";
                    this->log_error(function, message.str());
                }
                return ERROR;
            }
        } else {
            closedir(dirp); // directory already existed so close it
        }

        // start building the filename with directory/basename_
        // where "basedir" was just assembled above
        //
        fitsname.str("");
        fitsname << basedir.str() << "/" << this->base_name << "_";

        // add the controllerid if one is given
        //
        if (!controllerid.empty()) {
            fitsname << controllerid << "_";
        }

        // add the time or number suffix
        //
        if (this->fits_naming.compare("time") == 0) {
            fitsname << this->fitstime;
        } else if (this->fits_naming.compare("number") == 0) {
            // width of image_num portion of the filename is at least 4 digits, and grows as needed
            //
            int width = (this->image_num < 10000
                             ? 4
                             : (this->image_num < 100000
                                    ? 5
                                    : (this->image_num < 1000000
                                           ? 6
                                           : (this->image_num < 10000000
                                                  ? 7
                                                  : (this->image_num < 100000000
                                                         ? 8
                                                         : (this->image_num < 1000000000 ? 9 : 10))))));
            fitsname << std::setfill('0') << std::setw(width) << this->image_num;
        }

        // Check if file exists and include a -# to set apart duplicates.
        //
        struct stat st;
        int dupnumber = 1;
        fn.str("");
        fn << fitsname.str();
        fn << ".fits";
        while (stat(fn.str().c_str(), &st) == 0) {
            fn.str("");
            fn << fitsname.str();
            fn << "-" << dupnumber << ".fits";
            dupnumber++; // keep incrementing until we have a unique filename
        }

#ifdef LOGLEVEL_DEBUG
    message.str(""); message << "[DEBUG] fits_naming=" << this->fits_naming
                             << " controllerid=" << controllerid
                             << " will write to file: " << fn.str();
    logwrite(function, message.str());
#endif

        name_out = fn.str();
        return NO_ERROR;
    }

    /** Camera::Camera:get_fitsname *********************************************/


    /** Camera::Camera::datacube ************************************************/
    /**
     * @fn     datacube
     * @brief  set or get the datacube state
     * @param  std::string state_in
     * @return true or false
     *
     * The state_in string should be "True" or "False", case-insensitive.
     *
     * This function is overloaded.
     *
     */
    void Camera::datacube(bool state_in) {
        // write-only boolean
        std::string dontcare;
        this->datacube((state_in ? "true" : "false"), dontcare);
    }

    bool Camera::datacube() {
        // read-only boolean
        return (this->is_datacube);
    }

    long Camera::datacube(std::string state_in, std::string &state_out) {
        // read-write string, called from server
        std::string function = "Camera::Camera::datacube";
        std::stringstream message;
        int error = NO_ERROR;

        // If something is passed then try to use it to set the datacube state
        //
        if (!state_in.empty()) {
            try {
                std::transform(state_in.begin(), state_in.end(), state_in.begin(), ::tolower); // make lowercase
                if (state_in == "false") this->is_datacube = false;
                else if (state_in == "true") this->is_datacube = true;
                else {
                    message.str("");
                    message << state_in << " is invalid. Expecting true or false";
                    this->log_error(function, message.str());
                    error = ERROR;
                }
            } catch (...) {
                message.str("");
                message << "unknown exception parsing argument: " << state_in;
                this->log_error(function, message.str());
                error = ERROR;
            }
        }

        // error or not, the state reported is whatever was last successfully set
        //
        state_out = (this->is_datacube ? "true" : "false");
        logwrite(function, state_out);
        message.str("");
        message << "NOTICE:datacube=" << state_out;
        this->async.enqueue(message.str());

        // and this lets the server know if it was set or not
        //
        return error;
    }

    /** Camera::Camera::datacube ************************************************/


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
    void Camera::longerror(bool state_in) {
        // write-only boolean
        std::string dontcare;
        this->longerror((state_in ? "true" : "false"), dontcare);
    }

    bool Camera::longerror() {
        // read-only boolean
        return (this->is_longerror);
    }

    long Camera::longerror(std::string state_in, std::string &state_out) {
        // read-write string, called from server
        std::string function = "Camera::Camera::longerror";
        std::stringstream message;
        int error = NO_ERROR;

        // If something is passed then try to use it to set the longerror state
        //
        if (!state_in.empty()) {
            try {
                std::transform(state_in.begin(), state_in.end(), state_in.begin(), ::tolower); // make lowercase
                if (state_in == "false") this->is_longerror = false;
                else if (state_in == "true") this->is_longerror = true;
                else {
                    message.str("");
                    message << state_in << " is invalid. Expecting true or false";
                    this->log_error(function, message.str());
                    error = ERROR;
                }
            } catch (...) {
                message.str("");
                message << "unknown exception parsing argument: " << state_in;
                this->log_error(function, message.str());
                error = ERROR;
            }
        }

        // error or not, the state reported is whatever was last successfully set
        //
        state_out = (this->is_longerror ? "true" : "false");
        logwrite(function, state_out);
        message.str("");
        message << "NOTICE:longerror=" << state_out;
        this->async.enqueue(message.str());

        // and this lets the server know if it was set or not
        //
        return error;
    }

    /** Camera::Camera::longerror ***********************************************/


    /** Camera::Camera::cubeamps ************************************************/
    /**
     * @fn     cubeamps
     * @brief  set or get the cubeamps state
     * @param  std::string state_in
     * @return true or false
     *
     * The state_in string should be "True" or "False", case-insensitive.
     *
     * This function is overloaded.
     *
     * datacube also gets enabled/disabled along with cubeamps. If datacube
     * is needed after disabling cubeamps then it must be separately enabled.
     *
     */
    void Camera::cubeamps(bool state_in) {
        // write-only boolean
        std::string dontcare;
        this->cubeamps((state_in ? "true" : "false"), dontcare);
    }

    bool Camera::cubeamps() {
        // read-only boolean
        return (this->is_cubeamps);
    }

    long Camera::cubeamps(std::string state_in, std::string &state_out) {
        // read-write string, called from server
        std::string function = "Camera::Camera::cubeamps";
        std::stringstream message;
        int error = NO_ERROR;

        // If something is passed then try to use it to set the cubeamps state
        //
        if (!state_in.empty()) {
            try {
                std::transform(state_in.begin(), state_in.end(), state_in.begin(), ::tolower); // make lowercase
                if (state_in == "false") {
                    this->is_cubeamps = false;
                    this->is_datacube = false;
                } else if (state_in == "true") {
                    this->is_cubeamps = true;
                    this->is_datacube = true;
                } else {
                    message.str("");
                    message << state_in << " is invalid. Expecting true or false";
                    this->log_error(function, message.str());
                    error = ERROR;
                }
            } catch (...) {
                message.str("");
                message << "unknown exception parsing argument: " << state_in;
                this->log_error(function, message.str());
                error = ERROR;
            }
        }

        // error or not, the state reported is whatever was last successfully set
        //
        state_out = (this->is_cubeamps ? "true" : "false");
        logwrite(function, state_out);
        message.str("");
        message << "NOTICE:cubeamps=" << state_out;
        this->async.enqueue(message.str());

        // and this lets the server know if it was set or not
        //
        return error;
    }

    /** Camera::Camera::cubeamps ************************************************/


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
    long Information::pre_exposures(std::string num_in, std::string &num_out) {
        std::string function = "Camera::Information::pre_exposures";
        std::stringstream message;

        // If no string is passed then this is a request; return the current value.
        //
        if (num_in.empty()) {
            message.str("");
            message << "pre-exposures: " << this->num_pre_exposures;
            logwrite(function, message.str());
            num_out = std::to_string(this->num_pre_exposures);
            return NO_ERROR;
        } else {
            // Otherwise check the incoming value
            int num;
            try {
                num = std::stoi(num_in); // convert incoming string to integer
            } catch (std::invalid_argument &) {
                message.str("");
                message << "ERROR: invalid number: unable to convert " << num_in << " to integer";
                logwrite(function, message.str());
                return ERROR;
            }
            catch (std::out_of_range &) {
                message.str("");
                message << "ERROR: " << num_in << " out of integer range";
                logwrite(function, message.str());
                return ERROR;
            }
            if (num < 0) {
                // can't be negative
                message.str("");
                message << "ERROR: requested pre-exposures " << num << " must be >= 0";
                logwrite(function, message.str());
                return ERROR;
            } else {
                // incoming value is OK
                this->num_pre_exposures = num; // set the class variable
                num_out = num_in; // set the return string value
                return NO_ERROR;
            }
        }
    }

    /**************** Camera::Information::pre_exposures ************************/
}
