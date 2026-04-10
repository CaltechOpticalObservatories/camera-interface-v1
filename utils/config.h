/**
 * @file    config.h
 * @brief   include file for config
 * @details 
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#pragma once

#include <fstream>
#include <vector>
#include <map>
#include "utilities.h"  // for rtrim

/***** Config ***************************************************************/
/**
 * @class Config
 * @brief class provides functions for accessing configuration files
 */
class Config {
  public:
    Config(std::string filename_in): n_rows(), param(), arg() {
      filename = filename_in;
    };

    Config(): n_rows(), param(), arg() {
    };

    std::string filename;
    int n_rows;
    std::vector<std::string> param;
    std::vector<std::string> arg;
    std::map<std::string, std::string> configmap;


    /***** Config::get ******************************************************/
    /**
     * @brief      return configuration value for provided key
     * @details    When is_required is true then this will throw an
     *             exception if the key is not found. When false, silently
     *             return an empty string.
     * @param[in]  key          configuration key
     * @param[in]  is_required  true|false must I return a value?
     * @return     value  configuration value
     * @throws     runtime_error
     *
     */
    std::string get(const std::string &key, bool is_required=false) {
      if (configmap.find(key) != configmap.end()) {
        return configmap.at(key);
      }
      if (is_required) {
        throw std::runtime_error("'"+key+"' not in configuration");
      }
      else return "";
    }
    /***** Config::get ******************************************************/

    /** @brief calls Config::get() with is_requred=true
     */
    std::string required(const std::string &key) { return get(key, true);  }

    /** @brief calls Config::get() with is_requred=false
     */
    std::string optional(const std::string &key) { return get(key, false); }


    /***** Config::read_config **********************************************/
    /**
     * @brief      read the configuration file
     * @param[in]  config  reference to Config class object
     * @return     0 (no error) or 1 (error)
     * @throws     runtime_error
     *
     */
    long read_config() {
      std::fstream filestream; // I/O stream class
      std::string line; // Temporary string to read file lines into
      size_t index1, index2; // String indexing variables
      int linesread = 0; // Counts the number of parameter lines read from file

      // Try to open the config file
      //
      if (!this->filename.empty()) {
        try {
          filestream.exceptions(std::ios_base::failbit);
          // set the fail bit to catch exceptions opening config file
          filestream.open(this->filename, std::ios::in);
        }
        catch (const std::ios_base::failure &e) {
          throw std::runtime_error( filename + ": " + std::string(e.what()) );
        }
      }
      else {
        throw std::runtime_error("no filename specified");
      }

      // Clear the vectors to load the new information from the config file
      //
      this->param.clear();
      this->arg.clear();
      this->configmap.clear();

      // Read the config file
      //
      try {
        // setting the fail bit above requires catching exceptions here
        while (getline(filestream, line)) {
          // Get a line from the file as long as they are available

          std::string key="", value="";

          if (line.length() > 2) {
            // valid line is at least 3 characters, ie. X=Y

            if (line.at(0) == '#') continue; // If the first character is a #, ignore it.

            // At this point, all that is left to check are PARAM=ARG pairs, plus
            // possibly comments tagged on the end of the line.
            //
            else {
              line = line.substr(0, line.find_first_of("#"));
              // If # appears elsewhere then take only the line up until that
              rtrim(line); // remove trailing whitespace
              index1 = line.find_first_of("="); // Find the = delimiter in the line
              this->param.push_back(line.substr(0, index1));
              key = line.substr(0, index1);

              // Put the variable name into the vector holding the names

              // Look for configuration parameters in a vector format (i.e. surrounded by parentheses).
              //
              if ((index2 = line.find_last_of("(")) == std::string::npos) {
                // Look for quotes around the variable value.  Quotes are required for
                // variables that have spaces in them (name strings for example).
                //
                if ((index2 = line.find_last_of("\"")) == std::string::npos) {
                  // No quote strings, check for a comment at the end of the line
                  //
                  if ((index2 = line.find_first_of("#")) == std::string::npos) {
                    // No comment, so get the index of the end of the comment and set the value into the vector.
                    //
                    index2 = line.find_first_of("\t\0");
                    this->arg.push_back(line.substr(index1 + 1, index2 - index1));
                    value = line.substr(index1 + 1, index2 - index1);
                  }

                  // There is a comment, get the index and put the value (note the
                  // different index value for the line substring).
                  //
                  else {
                    index2 = line.find_first_of(" \t#");
                    this->arg.push_back(line.substr(index1 + 1, index2 - index1 - 1));
                    value = line.substr(index1 + 1, index2 - index1 - 1);
                  }
                }

                // Quotes change the substring index again, so find the position of the
                // two quote characters in the string and get the value.
                //
                else {
                  index1 = line.find_first_of("\"") + 1;
                  index2 = line.find_last_of("\"");
                  this->arg.push_back(line.substr(index1, index2 - index1));
                  value = line.substr(index1, index2 - index1);
                }
              }

              // A vector was found, so strip the part between the parentheses out and save it
              //
              else {
                index1 = line.find_first_of("(") + 1;
                index2 = line.find_last_of(")");
                this->arg.push_back(line.substr(index1, index2 - index1));
                value = line.substr(index1, index2 - index1);
              }
            }

            configmap[key] = value;

          } else continue; // For lines of less than 2 characters, we just loop to the next line

          linesread++; // Increment the number of values read successfully
        }
      }
      catch (const std::ios_base::failure &e) {
        // even an EOF will set the fail bit, which we don't care about
        ; // so just ignore it here
      }

      this->n_rows = linesread; // Set the number of elements to the number of lines read

      if (filestream.is_open() == true) {
        // Close the file stream
        filestream.flush();
        filestream.close();
      }

      return 0;
    }
    /***** Config::read_config **********************************************/
};
/***** Config ***************************************************************/
