/**
 * @file    archon_controller.cpp
 * @brief   implementation for Archon Interface Controller
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_controller.h"
#include "archon_interface.h"

namespace Camera {


  /***** Camera::ArchonController::ArchonController ***************************/
  /**
   * @brief      ArchonController constructor
   */
  ArchonController::ArchonController() :
    interface(nullptr),
    framebuf(nullptr),
    framebuf_bytes(0),
    is_connected(false),
    is_firmwareloaded(false)
  {
    // pre-size the modtype and modversion vectors to hold the max number of modules
    modtype.resize(NMODS);
    modversion.resize(NMODS);

    {
    auto ptr=std::make_unique<ArchonExposureTime>();  // create pointer to ArchonExposureTime object
    this->exposure_time=ptr.get();
    this->info.exposure_time=std::move(ptr);          // transfer ownership to info
    }

  }
  /***** Camera::ArchonController::ArchonController ***************************/


  /***** Camera::ArchonController::~ArchonController **************************/
  /**
   * @brief      ArchonController destructor
   */
  ArchonController::~ArchonController() {
    delete[] framebuf;
  }
  /***** Camera::ArchonController::~ArchonController **************************/


  /***** Camera::ArchonController::set_interface ******************************/
  /**
   * @brief      initialize pointer to parent interface
   * @details    This allows Controller members to call back into ArchonInterface
   * @param[in]  _interface  pointer to parent Camera::ArchonInterface object
   *
   */
  void ArchonController::set_interface(ArchonInterface* _interface) {
    this->interface = _interface;
  }
  /***** Camera::ArchonController::set_interface ******************************/


  /***** Camera::ArchonController::configure_controller ***********************/
  /**
   * @brief      parse the configuration file for controller-related parameters
   * @details    The config file has already been read into the Config class.
   * @throws     std::runtime_error
   *
   */
  void ArchonController::configure_controller() {
    const std::string function("Camera::ArchonController::configure_controller");
    logwrite(function, "");

    if (this->interface->configfile.n_rows < 1) throw std::runtime_error("empty configuration");

    // iterate through each row in config file
    for (int row=0; row < this->interface->configfile.n_rows; row++) {

      // ARCHON_IP
      if (this->interface->configfile.param[row]=="ARCHON_IP") {
        this->archon.sethost( this->interface->configfile.arg[row] );
      }
      else
      // ARCHON_PORT
      if (this->interface->configfile.param[row]=="ARCHON_PORT") {
        try {
          this->archon.setport( std::stoi(this->interface->configfile.arg[row]) );
        }
        catch (const std::exception &e) {
          std::ostringstream oss;
          oss << "parsing " << this->interface->configfile.param[row]
                            << "=" << this->interface->configfile.arg[row] << ": " << e.what();
          throw std::runtime_error(oss.str());
        }
      }
      else
      // DEFAULT_FIRMWARE
      if (this->interface->configfile.param[row]=="DEFAULT_FIRMWARE") {
        this->firmware = this->interface->configfile.arg[row];
      }
    }
  }
  /***** Camera::ArchonController::configure_controller ***********************/


  /***** Camera::ArchonController::connect ************************************/
  /**
   * @brief      parse the configuration file for controller-related parameters
   * @details    The config file has already been read into the Config class.
   * @throws     std::runtime_error
   *
   */
  void ArchonController::connect() {
    const std::string function("Camera::ArchonController::connect");

    // nothing to do if already connected
    if ( this->archon.isconnected() ) {
      logwrite(function, "camera connection already open");
      return;
    }

    // initialize camera connection
    try {
      this->archon.Connect();
    }
    catch (const std::exception &e) {
      throw;
    }

    std::stringstream message;
    message << "socket connection to host " << this->archon.gethost()
            << " port " << this->archon.getport()
            << " established on fd " << this->archon.getfd();
    logwrite(function, message.str());

    // get the Archon system information for installed modules
    std::string reply;
    if (this->send_cmd(SYSTEM, reply) != NO_ERROR) {   // first the whole reply in one string
      throw std::runtime_error("getting SYSTEM information");
    }

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );              // then each line in a separate token "lines"

    for ( const auto &line : lines ) {
      Tokenize( line, tokens, "_=" );           // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;       // need 3 tokens

      std::string version;
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( (tokens[0].compare( 0, 3, "MOD" ) == 0) && (tokens[1] == "TYPE") ) {
        try {
          module = std::stoi(tokens[0].substr(3));
          type   = std::stoi(tokens[2]);
        }
        catch (const std::exception &e) {
          logwrite(function, "ERROR parsing module/type from \""+line+"\": "+std::string(e.what()));
          throw std::runtime_error("unexpected SYSTEM information");
        }
      }
      else continue;

      // validate module number
      //
      if (module<1 || module>NMODS) {
        message.str(""); message << "module " << module << " outside range {1:" << NMODS << "}";
        logwrite( function, message.str() );
        throw std::runtime_error("invalid module number");
      }

      // get the module version
      //
      if (tokens[1] == "VERSION") version = tokens[2]; else version = "";

      // now store it permanently
      //
      try {
        this->modtype.at(module-1)    = type;      // store the type in a vector indexed by module
        this->modversion.at(module-1) = version;   // store the type in a vector indexed by module
      }
      catch (const std::exception &e) {
        message.str(""); message << "requested module " << module << " out of range {1:" << NMODS << "}";
        logwrite( function, message.str() );
        throw std::runtime_error("invalid module number");
      }

      // Use the module type to resize the gain and offset vectors,
      // but always use the largest possible value allowed.
      //
      int adchans=0;
      if (type == MODTYPE_ADC) adchans = ( adchans < MAXADCCHANS ? MAXADCCHANS : adchans );
      if (type == MODTYPE_ADM) adchans = ( adchans < MAXADMCHANS ? MAXADMCHANS : adchans );
      this->gain.resize( adchans );
      this->offset.resize( adchans );

      // Check that the AD modules are installed in the correct slot
      //
      if ( (type == MODTYPE_ADC || type == MODTYPE_ADM) && (module < 5 || module > 8) ) {
        message.str(""); message << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
        logwrite( function, message.str() );
        throw std::runtime_error(message.str());
      }
    } // end for ( auto line : lines )

    // empty the Archon log
    //
    this->fetchlog();
  }
  /***** Camera::ArchonController::connect ************************************/


  /***** Camera::ArchonController::get_bias_config ****************************/
  /**
   * @brief      create the bias configuration key and validate mod and chan
   * @details    This creates the configuration line needed as a key for both
   *             reading and writing a bias from and to the Archon configuration.
   * @return     bias_config_t structure
   * @throws     std::runtime_error
   *
   */
  ArchonController::bias_config_t ArchonController::get_bias_config(int mod, int chan) const {
    std::ostringstream oss;

    // Check that the module number is valid
    if ( (mod < 0) || (mod > NMODS) ) {
      oss << "module " << mod << ": outside range {0:" << NMODS << "}";
      throw std::runtime_error(oss.str());
    }

    // Check that the channel number is valid
    if ( (chan < 1) || (chan > 30) ) {
      oss << "bias channel " << mod << ": outside range {1:30}";
      throw std::runtime_error(oss.str());
    }

    bias_config_t info;
    std::ostringstream biasconfig;

    // Use the module type to get LV or HV Bias
    // and start building the bias configuration string.
    float vmin, vmax;
    switch ( this->modtype[ mod-1 ] ) {
      case MODTYPE_NONE:
        oss << "module " << mod << " not installed";
        throw std::runtime_error(oss.str());
      case MODTYPE_LVBIAS:
      case MODTYPE_LVXBIAS:
        biasconfig << "MOD" << mod << "/LV";
        info.vmin = -14.0;
        info.vmax = +14.0;
        break;
      case MODTYPE_HVBIAS:
      case MODTYPE_HVXBIAS:
        biasconfig << "MOD" << mod << "/HV";
        info.vmin =   0.0;
        info.vmax = +31.0;
        break;
      default:
        oss << "module " << mod << " not a bias board";
        throw std::runtime_error(oss.str());
    }

    // append the channel to the bias configuration string
    if (chan < 25) {
      biasconfig << "LC_V" << chan;
    }
    else {
      biasconfig << "HC_V" << (chan-24);
    }

    info.key = biasconfig.str();
    return info;
  }
  /***** Camera::ArchonController::get_bias_config ****************************/


  /***** Camera::ArchonController::make_applymod_command **********************/
  /**
   * @brief      creates an APPLYMODx string
   * @returns    std::string
   *
   */
  std::string ArchonController::make_applymod_command(int mod) const {
    std::ostringstream oss;
    oss << "APPLYMOD"
        << std::setfill('0')
        << std::setw(2)
        << std::hex
        << (mod-1);
    return oss.str();
  }
  /***** Camera::ArchonController::make_applymod_command **********************/


  /***** Camera::ArchonController::bias ***************************************/
  /**
   * @brief      parse the configuration file for controller-related parameters
   * @details    The config file has already been read into the Config class.
   * @throws     std::runtime_error
   *
   */
  void ArchonController::bias(const int &mod, const int &chan, float &volts, const bool &should_write) {
    const std::string function("Camera::ArchonController::bias");
    std::ostringstream oss;
    std::ostringstream biasconfig;

    // nothing to do if no connection open to controller
    if (!this->archon.isconnected()) {
      throw std::runtime_error("connection not open to controller");
    }

    // creating the bias configuration key also validates mod and chan
    auto info = get_bias_config(mod, chan);

    // write the bias configuration line if needed
    if (should_write) {
      bool changed=false;
      // check requested voltage is within range
      if ( (volts < info.vmin) || (volts > info.vmax) ) {
        oss << volts << " outside range {" << info.vmin << ":" << info.vmax << "}";
        throw std::runtime_error(oss.str());
      }

      // write the configuration line to update the bias voltage
      std::string val = std::to_string(volts);
      this->write_config_key(info.key.c_str(), val.c_str(), changed);

      // send the APPLYMODx command
      long error = this->send_cmd(make_applymod_command(mod));

      if (error != NO_ERROR) {
        oss << "writing bias configuration " << info.key << "=" << val;
        throw std::runtime_error(oss.str());
      }
      else if (!changed) {
        oss << "bias configuration " << info.key << "=" << val << " unchanged";
        logwrite(function, oss.str());
        return;
      }
      else {
        oss << "updated bias configuration " << info.key << "=" << val;
        logwrite(function, oss.str());
        return;
      }
    }

    // read the configuration
    this->get_configmap_value(info.key, volts);
  }
  /***** Camera::ArchonController::bias ***************************************/


  /***** Camera::ArchonController::set_exptime ********************************/
  /**
   * @brief      set the exposure time on the controller
   * @details    This takes a floating point exposure time (sec) and writes it
   *             to the sec and/or msec parameters on the Archon, as appropriate.
   * @param[in]  exptime        exposure time in seconds
   * @throws     std::runtime_error
   * @throws     std::exception
   *
   */
  void ArchonController::set_exptime(double exptime) {
    // nothing to do if no connection open to controller
    if (!this->archon.isconnected()) {
      throw std::runtime_error("connection not open to controller");
    }

    // exposure time parameters must be defined
    if (this->sec_param.empty() || this->msec_param.empty()) {
      throw std::runtime_error("exposure time parameters not defined");
    }

    try {
      // split the requested exposure time into seconds and milliseconds
      auto [sec, msec] = this->exposure_time->split(exptime);

      // Set the sec and msec parameters on the controller,
      // store the exptime in the class on success.
//    if ( (set_parameter(sec_param, sec)   == NO_ERROR) &&     //TODO haven't created these functions yet!
//         (set_parameter(msec_param, msec) == NO_ERROR) ) {
//      this->exposure_time->set(exptime);
//    }
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Camera::ArchonController::set_exptime ********************************/


  /***** Camera::ArchonController::send_cmd ***********************************/
  /**
   * @brief      send a command to Archon
   * @details    This function is overloaded.
   *             Use this form when the calling function doesn't need a reply.
   * @param[in]  cmd    command to send
   * @return     ERROR | BUSY | NO_ERROR
   *
   */
  long ArchonController::send_cmd(const std::string &cmd) {
    std::string reply;
    return( this->send_cmd(cmd, reply) );
  }
  /***** Camera::ArchonController::send_cmd ***********************************/
  /**
   * @brief      send a command to Archon
   * @details    This function is overloaded. This version returns a reply.
   * @param[in]  cmd    command to send
   * @param[out] reply  string contains reply
   * @return     ERROR | BUSY | NO_ERROR
   *
   */
  long ArchonController::send_cmd(const std::string &cmd, std::string &reply) {
    std::string function = "Camera::ArchonController::send_cmd";
    char message[256];
    char check[4];
    int     retval;
    int     error = NO_ERROR;

    // nothing to do if no connection open to controller
    if (!this->archon.isconnected()) {
      logwrite( function, "ERROR connection not open to controller" );
      return ERROR;
    }

    // Blocks to protect against simultaneous access, automatically
    // unlocks on return.
    //
    std::lock_guard<std::mutex> lock(archon_mutex);

    // The archon busy atomic flag is also needed because FETCH can keep
    // Archon busy for longer than the duration of this function.
    //
    if ( archon_busy.test_and_set() ) {
      SNPRINTF(message, "ERROR Archon busy: ignored command \"%s\"", cmd.c_str());
      logwrite(function, std::string(message));
      return BUSY;
    }

    // build command: ">xxCOMMAND\n" where xx=hex msgref and COMMAND=command
    //
    char buf[256];
    msgref = (msgref + 1) % 256;       // increment msgref for each new command sent
    int len = std::snprintf(buf, sizeof(buf), ">%02X%s\n", msgref, cmd.c_str());
    std::string scmd(buf, len);

    // build the command checksum: msgref used to check that reply matches command
    //
    SNPRINTF(check, "<%02X", msgref);

    // send the command
    //
    if ( (this->archon.Write(scmd)) == -1) {
      logwrite( function, "ERROR writing to camera socket");
      return ERROR;
    }

    // For the FETCH command we don't wait for a reply, but return immediately.
    // FETCH results in a binary response which is handled elsewhere (in read_frame).
    // Must also distinguish this from the FETCHLOG command, for which we do wait
    // for a normal reply.
    //
    // Do not clear the archon_busy flag because Archon is still busy!
    // The read_frame() function will have to clear this flag when it is
    // done reading the data.
    //
    if (cmd.size() >= 5 && memcmp(cmd.data(), "FETCH", 5)==0 &&
       (cmd.size() < 8 || memcmp(cmd.data(), "FETCHLOG", 8) != 0)) return NO_ERROR;

    // For all other commands, receive the reply
    //
    char* buffer = new char[64*1024+1]{};                // temporary buffer for holding Archon replies
    reply.clear();                                       // zero reply buffer
    do {
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { SNPRINTF(message, "Poll timeout waiting for response from Archon command (maybe unrecognized command?)"); error=TIMEOUT; }
        if (retval<0)  { SNPRINTF(message, "Poll error waiting for response from Archon command"); error=ERROR; }
        if ( error != NO_ERROR ) logwrite( function, std::string(message) );
        break;
      }
      retval = this->archon.Read(buffer, 64*1024);       // read into temp buffer
      if (retval <= 0) {
        logwrite( function, "ERROR reading Archon" );
        break;
      }
      buffer[retval] = '\0';                             // null-terminate bytes read
      reply.append(buffer);                              // append read buffer into the reply string
      if (strchr(buffer, '\n') != nullptr) break;        // exit on newline
    } while(retval>0);

    delete [] buffer;

    // If there was an Archon error then clear the busy flag and get out now
    //
    if ( error != NO_ERROR ) {
      archon_busy.clear();
      return error;
    }

    // The first three bytes of the reply should contain the msgref of the
    // command, which can be used as a check that the received reply belongs
    // to the command which was sent.
    //
    // Error processing command (no other information is provided by Archon)
    //
    if (!reply.empty() && reply[0]=='?') {
      error = ERROR;
      SNPRINTF(message, "Archon controller returned ERROR processing command: %s", cmd.c_str());
      logwrite( function, std::string(message) );
    }
    else
    // First 3 bytes of reply must equal checksum else reply doesn't belong to command
    if (reply.size()<3 || std::memcmp(reply.data(), check, 3) != 0) {
      error = ERROR;
      std::string hdr = reply;
      try { scmd.erase(scmd.find("\n"), 1); } catch(...) { }
      SNPRINTF(message, "ERROR command-reply mismatch for command: %s: expected %s but received %s", scmd.c_str(), check, reply.c_str());
      logwrite( function, std::string(message) );
    }
    else {
      // command and reply are a matched pair
      error = NO_ERROR;
      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the busy flag
    archon_busy.clear();

    return error;
  }
  /***** Camera::ArchonController::send_cmd ***********************************/


  /***** Camera::ArchonController::fetchlog ***********************************/
  /**
   * @brief  fetch the archon log entry and log the response
   * @return NO_ERROR or ERROR,  return value from send_cmd call
   *
   * Send the FETCHLOG command to, then read the reply from Archon.
   * Fetch until the log is empty. Log the response.
   *
   */
  long ArchonController::fetchlog() {
    const std::string function("Camera::ArchonController::fetchlog");
    std::string reply;
    std::stringstream message;
    long  retval;

    // send FETCHLOG command while reply is not (null)
    //
    do {
      if ( (retval=this->send_cmd(FETCHLOG, reply)) != NO_ERROR ) {  // send command here
        logwrite( function, "ERROR calling FETCHLOG" );
        return retval;
      }
      if (reply != "(null)") {
        try {
            reply.erase(reply.find('\n'), 1);
        } catch(...) { }             // strip newline
        logwrite(function, reply);                                   // log reply here
      }
    } while (reply != "(null)");                                     // stop when reply is (null)

    return retval;
  }
  /***** Camera::ArchonController::fetchlog ***********************************/


  /***** Camera::ArchonController::load_acf ***********************************/
  /**
   * @brief      loads the ACF file into configuration memory (no APPLY!)
   * @details    This is an internal-use function which performs the detailed
   *             steps of loading and parsing an ACF file, from disk, into
   *             the class and Archon configuration memory. Essentially, it
   *             performs the WCONFIG but nothing else, nothing is applied
   *             and the timing cores are not reset. This function will be
   *             called by load_timing() and load_firmware().
   * @param[in]  acffile          (optional) fully qualified path to ACF file
   * @param[in]  write_to_archon  (optional) true=write to Archon, false=host only
   * @return     ERROR | NO_ERROR
   *
   * This function loads the optionally specified file into configuration memory
   * (if not specified, then the file specified by DEFAULT_FIRMWARE will be used).
   * While the ACF is being read, an internal database (STL map) is being
   * created to allow lookup access to the ACF file or parameters.
   *
   * The [MODE_XXX] sections are also parsed and parameters as a function
   * of mode are saved in their own database.
   *
   * This function only loads (WCONFIGxxx) the configuration memory; it does
   * not apply it to the system. Therefore, this function must be followed
   * with a LOADTIMING or APPLYALL command, for example.
   *
   * If write_to_archon is false then the Archon is not written to at all.
   * The ACF is read only into host memory. This is useful in case camerad
   * is stopped and restarted, it can use this to know about the previously
   * loaded ACF.
   *
   */
  long ArchonController::load_acf(const std::string &filename_in, bool write_to_archon) {
    const std::string function("Camera::ArchonController::load_acf");
    std::stringstream message;
    std::fstream filestream;  // I/O stream class
    std::string line;         // the line read from the acffile
    std::string mode;
    std::string keyword, keystring, keyvalue, keytype, keycomment;
    std::stringstream sscmd;
    std::string key, value;

    int      linecount;  // the Archon configuration line number is required for writing back to config memory
    long     error=NO_ERROR;
    bool     parse_config=false;

    std::string filename(filename_in);

    if (filename_in.empty()) filename = this->firmware;

    // try to open the file
    //
    try {
      filestream.open(filename, std::ios_base::in);
    }
    catch(const std::exception &e) {
      logwrite( function, "ERROR opening \""+filename+"\": "+std::string(e.what()));
      return ERROR;
    }

    if ( ! filestream.is_open() || ! filestream.good() ) {
      logwrite(function, "ERROR opening \""+filename+"\"");
      return ERROR;
    }

    logwrite(function, filename);

    if ( write_to_archon ) {
      logwrite( function, "will write ACF to Archon" );
    }
    else {
      logwrite( function, "reading ACF into host memory only" );
    }

    // The CPU in Archon is single threaded, so it checks for a network
    // command, then does some background polling (reading bias voltages etc.),
    // then checks again for a network command.  "POLLOFF" disables this
    // background checking, so network command responses are very fast.
    // The downside is that bias voltages, temperatures, etc. are not updated
    // until you give a "POLLON".
    //
    if (write_to_archon) error = this->send_cmd(POLLOFF);

    // clear configuration memory for this controller
    //
    if (error == NO_ERROR && write_to_archon) error = this->send_cmd(CLEARCONFIG);

    if ( error != NO_ERROR  && write_to_archon) {
        logwrite( function, "ERROR: could not prepare Archon for new ACF" );
        return error;
    }

    // Any failure after clearing the configuration memory will mean
    // no firmware is loaded.
    //
    this->is_firmwareloaded=false;

    this->modemap.clear();                       // file is open, clear all modes

    linecount = 0;                               // init Archon configuration line number

    while ( getline(filestream, line) ) {        // note that getline discards the newline "\n" character

      // don't start parsing until [CONFIG] and stop on a newline or [SYSTEM]
      //
      if (line == "[CONFIG]") { parse_config=true;  continue; }
      if (line == "\n"      ) { parse_config=false; continue; }
      if (line == "[SYSTEM]") { parse_config=false; continue; }

      std::string savedline = line;              // save un-edited line for error reporting

      // parse mode sections, looking for "[MODE_xxxxx]"
      //
      if (line.substr(0,6)=="[MODE_") {          // this is a mode section
        try {
          line.erase(line.find('['), 1);         // erase the opening square bracket
          line.erase(line.find(']'), 1);         // erase the closing square bracket
        }
        catch(const std::exception &e) {
          logwrite(function, "ERROR parsing \""+savedline+"\": "+std::string(e.what())+": expected [MODE_xxxx]");
          filestream.close();
          return ERROR;
        }
        if ( ! line.empty() ) {                  // What's remaining should be MODE_xxxxx
          mode = line.substr(5);                 // everything after "MODE_" is the mode name
          std::transform( mode.begin(), mode.end(), mode.begin(), ::toupper );    // make uppercase

          // got a mode, now check if one of this name has already been located
          // and put into the modemap
          //
          if ( this->modemap.find(mode) != this->modemap.end() ) {
            logwrite(function, "ERROR duplicate definition of mode "+mode+": load aborted");
            filestream.close();
            return ERROR;
          }
          else {
            parse_config = true;
            message.str(""); message << "detected mode: " << mode; logwrite(function, message.str());
            this->modemap[mode].rawenable=-1;    // initialize to -1, require it be set somewhere in the ACF
                                                 // this also ensures something is saved in the modemap for this mode
          }

        }
        else {                                   // somehow there's no xxx left after removing "[MODE_" and "]"
          logwrite(function, "ERROR malformed mode section \""+savedline+"\": expected [MODE_xxxx]");
          filestream.close();
          return ERROR;
        }
      }

      // Everything else is for parsing configuration lines so if we didn't get [CONFIG] then
      // skip to the next line.
      //
      if (!parse_config) continue;

      // replace any TAB characters with a space
      //
      string_replace_char(line, "\t", " ");

      // replace any backslash characters with a forward slash
      //
      string_replace_char(line, "\\", "/");

      // erase all quotes
      try {
          line.erase( std::remove(line.begin(), line.end(), '"'), line.end() );
      } catch(...) { }

      // Initialize key, value strings used to form WCONFIG KEY=VALUE command.
      // As long as key stays empty then the WCONFIG command is not written to the Archon.
      // This is what keeps TAGS: in the [MODE_xxxx] sections from being written to Archon,
      // because these do not populate key.
      //
      key="";
      value="";

      //  ************************************************************
      // Store actual Archon parameters in their own STL map IN ADDITION to the map
      // in which all other keywords are store, so that they can be accessed in
      // a different way.  Archon PARAMETER KEY=VALUE paris are formatted as:
      // PARAMETERn=ParameterName=value
      // where "PARAMETERn" is the key and "ParameterName=value" is the value.
      // However, it is logical to access them by ParameterName only. That is what the
      // parammap is for, hence the need for this STL map indexed on only the "ParameterName"
      // portion of the value. Conversely, the configmap is indexed by the key.
      //
      // parammap stores ONLY the parameters, which are identified as PARAMETERxx="paramname=value"
      // configmap stores every configuration line sent to Archon (which includes parameters)
      //
      // In order to modify these keywords in Archon, the entire above phrase
      // (KEY=VALUE pair) must be preserved along with the line number on which it
      // occurs in the config file.
      // ************************************************************

      // Look for TAGS: in the .acf file mode sections
      //
      // If tag is "ACF:" then it's a .acf line (could be a parameter or configuration)
      //
      if (line.compare(0,4,"ACF:")==0) {
        std::vector<std::string> tokens;
        line = line.substr(4);              // strip off the "ACF:" portion
        std::string acf_key, acf_value;

        try {
          Tokenize(line, tokens, "="); // separate into tokens by "="

          if (tokens.size() == 1) {                    // KEY=, the VALUE is empty
            acf_key   = tokens[0];
            acf_value = "";

          } else if (tokens.size() == 2) {             // KEY=VALUE
                acf_key   = tokens[0];
                acf_value = tokens[1];

          } else {
                logwrite(function, "ERROR malformed ACF line \""+savedline+": expected KEY=VALUE");
                filestream.close();
                return ERROR;
          }

          bool keymatch = false;

          // If this key is in the main parammap then store it in the modemap's parammap for this mode
          if (this->parammap.find( acf_key ) != this->parammap.end()) {
            this->modemap[mode].parammap[ acf_key ].name  = acf_key;
            this->modemap[mode].parammap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is in the main configmap, then store it in the modemap's configmap for this mode
          //
          if (this->configmap.find( acf_key ) != this->configmap.end()) {
            this->modemap[mode].configmap[ acf_key ].value = acf_value;
            keymatch = true;
          }

          // If this key is neither in the parammap nor in the configmap then return an error
          //
          if ( ! keymatch ) {
            message.str("");
            message << "[MODE_" << mode << "] ACF directive: " << acf_key << "=" << acf_value << " is not a valid parameter or configuration key";
            logwrite(function, message.str());
            filestream.close();
            return ERROR;
          }

        } catch (const std::exception &e) {
          logwrite(function, "ERROR extracting KEY=VAUE pair from \""+savedline+"\": "+std::string(e.what()));
          filestream.close();
          return ERROR;
        }
        // end if (line.compare(0,4,"ACF:")==0)

      } else if (line.compare(0,5,"ARCH:")==0) {
          // The "ARCH:" tag is for internal (Archon_interface) variables
          // using the KEY=VALUE format.
          //
          std::vector<std::string> tokens;
          line = line.substr(5);                                            // strip off the "ARCH:" portion
          Tokenize(line, tokens, "=");                                      // separate into KEY, VALUE tokens
          if (tokens.size() != 2) {
            logwrite(function, "ERROR malformed ARCH line \""+savedline+"\": expected ARCH:KEY=VALUE");
            filestream.close();
            return ERROR;
          }
          if ( tokens[0] == "NUM_DETECT" ) {
            this->modemap[mode].geometry.num_detect = std::stoi( tokens[1] );

          } else if ( tokens[0] == "HORI_AMPS" ) {
            this->modemap[mode].geometry.amps[0] = std::stoi( tokens[1] );

          } else if ( tokens[0] == "VERT_AMPS" ) {
            this->modemap[mode].geometry.amps[1] = std::stoi( tokens[1] );

          } else {
            logwrite(function, "ERROR unrecognized internal parameter "+tokens[0]);
            filestream.close();
            return ERROR;
          }
          // end else if (line.compare(0,5,"ARCH:")==0)

      } else if (line.compare(0,5,"FITS:")==0) {
          // the "FITS:" tag is used to write custom keyword entries of the form "FITS:KEYWORD=VALUE/COMMENT"
          //
          std::vector<std::string> tokens;
          line = line.substr(5);           // strip off the "FITS:" portion

          // First, tokenize on the equal sign "=".
          // The token left of "=" is the keyword. Immediate right is the value
          Tokenize(line, tokens, "=");
          if (tokens.size() != 2) {    // need at least two tokens at this point
            logwrite(function, "ERROR malformed FITS command "+savedline+": expected KEYWORD=VALUE/COMMENT");
            filestream.close();
            return ERROR;
          }
          keyword   = tokens[0].substr(0,8); // truncate keyword to 8 characters
          keystring = tokens[1];                    // tokenize the rest in a moment
          keycomment = "";                          // initialize comment, assumed empty unless specified below

          // Next, tokenize on the slash "/".
          // The token left of "/" is the value. Anything to the right is a comment.
          //
          Tokenize(keystring, tokens, "/");

          if (tokens.empty()) {          // no tokens found means no "/" characeter which means no comment
            keyvalue = keystring;        // therefore the keyvalue is the entire string
          }

          if (not tokens.empty()) {         // at least one token
            keyvalue = tokens[0];
          }

          if (tokens.size() == 2) {      // If there are two tokens here then the second is a comment
            keycomment = tokens[1];
          }

          if (tokens.size() > 2) {       // everything below this has been covered
            logwrite(function, "ERROR malformed FITS command "+savedline+": expected KEYWORD=VALUE/COMMENT");
            logwrite(function, "ERROR too many \"/\" in comment string? "+keystring);
            filestream.close();
            return ERROR;
          }

          // Save all the user keyword information in a map for later
          this->modemap[mode].acfkeys.keydb[keyword].keyword    = keyword;
          this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->info.userkeys.get_keytype(keyvalue);
          this->modemap[mode].acfkeys.keydb[keyword].keyvalue   = keyvalue;
          this->modemap[mode].acfkeys.keydb[keyword].keycomment = keycomment;
          // end if (line.compare(0,5,"FITS:")==0)
          //
          // ----- all done looking for "TAGS:" -----
          //

      } else if ( (line.compare(0,11,"PARAMETERS=")!=0) &&   // not the "PARAMETERS=xx line
            (line.compare(0, 9,"PARAMETER"  )==0) ) {  // but must start with "PARAMETER"
          // If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...
          //

          std::vector<std::string> tokens;
          Tokenize(line, tokens, "=");                  // separate into PARAMETERn, ParameterName, value tokens

          if (tokens.size() != 3) {
            logwrite(function, "ERROR malformed parameter line "+savedline+": expected PARAMETERn=Param=value");
            filestream.close();
            return ERROR;
          }

          // Tokenize broke everything up at the "=" and
          // we need all three parts, but we also need a version containing the last
          // two parts together, "ParameterName=value" so re-assemble them here.
          //
          std::stringstream paramnamevalue;
          paramnamevalue << tokens[1] << "=" << tokens[2];             // reassemble ParameterName=value string

          // build an STL map "configmap" indexed on PARAMETERn, the part before the first "=" sign
          //
          this->configmap[ tokens[0] ].line  = linecount;              // configuration line number
          this->configmap[ tokens[0] ].value = paramnamevalue.str();   // configuration value for PARAMETERn

          // build an STL map "parammap" indexed on ParameterName so that we can look up by the actual name
          //
          this->parammap[ tokens[1] ].key   = tokens[0];    // PARAMETERn
          this->parammap[ tokens[1] ].name  = tokens[1];    // ParameterName
          this->parammap[ tokens[1] ].value = tokens[2];    // value
          this->parammap[ tokens[1] ].line  = linecount;    // line number

          // assemble a KEY=VALUE pair used to form the WCONFIG command
          key   = tokens[0];                                      // PARAMETERn
          value = paramnamevalue.str();                           // ParameterName=value
          // end If this is a PARAMETERn=ParameterName=value KEY=VALUE pair...

      } else {
          // ...otherwise, for all other KEY=VALUE pairs, there is only the value and line number
          // to be indexed by the key. Some lines may be equal to blank, e.g. "CONSTANTx=" so that
          // only one token is made
          //
          std::vector<std::string> tokens;
          // Tokenize will return a size=1 even if there are no delimiters,
          // so work around this by first checking for delimiters
          // before calling Tokenize.
          //
          if (line.find_first_of('=', 0) == std::string::npos) {
            continue;
          }

          Tokenize(line, tokens, "=");                            // separate into KEY, VALUE tokens
          if (tokens.empty()) {
            continue;                                             // nothing to do here if no tokens (ie no "=")
          }

          key = tokens[0];                                        // not empty so at least one token is the KEY
          value.clear();                                          // VALUE can be empty (e.g. labels not required)

          if ( tokens.size() > 1 ) {                              // at least one more token is the value
            value = tokens[1];                                    // VALUE (there is a second token)
          }

          if ( tokens.size() > 2 ) {                              // more tokens are possible
            for ( size_t i=2; i<tokens.size(); ++i ) {            // loop through remaining tokens
              value += "=" + tokens[i];                           // and put them back together as the VALUE
            }
          }

          this->configmap[ tokens[0] ].line  = linecount;
          this->configmap[ tokens[0] ].value = value;
      } // end else

      // Form the WCONFIG command to Archon and
      // write the config line to the controller memory (if key is not empty).
      //
      if ( !key.empty() ) {                                     // value can be empty but key cannot
        sscmd.str("");
        sscmd << "WCONFIG"
              << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
              << linecount
              << key << "=" << value << "\n";
        // send the WCONFIG command here
        if (error == NO_ERROR && write_to_archon) error = this->send_cmd(sscmd.str());
        linecount++;
      } // end if ( !key.empty() && !value.empty() )
    } // end while ( getline(filestream, line) )

    this->configlines = linecount;  // save the number of configuration lines

    // re-enable background polling
    //
    if (error == NO_ERROR && write_to_archon) error = this->send_cmd(POLLON);

    filestream.close();
    if (error == NO_ERROR) {
      logwrite(function, "loaded Archon Config File OK");
      this->is_firmwareloaded = true;

      // add to systemkeys keyword database
      //
      std::stringstream keystr;
      keystr << "FIRMWARE=" << filename << "// controller firmware";
      this->info.systemkeys.addkey( keystr.str() );
    }

    // on success, the class variable is what was just loaded
    //
    if (error==NO_ERROR) {
      this->firmware = filename;
    }
    else error=this->fetchlog();

    this->is_camera_mode = false;         // require that a mode be selected after loading new firmware

    return error;
  }
  /***** Camera::ArchonController::load_acf ***********************************/


  /***** Camera::ArchonController::get_configmap_value ************************/
  /**
   * @brief      get the VALUE from configmap for a givenn KEY and assign to a variable
   * @param[in]  key_in is the KEY
   * @param[out] value_out reference to variable to contain the VALUE
   * @throws     std::runtime_error
   *
   * This is a template class function so the &value_out reference can be any type.
   * Throws runtime_error if the key_in KEY is not found, otherwise the VALUE
   * associated with key_in is assigned to &value_out.
   *
   */
  template <class T>
  void ArchonController::get_configmap_value(const std::string &key_in, T &value_out) {
    if ( this->configmap.find(key_in) != this->configmap.end() ) {
      std::istringstream( this->configmap[key_in].value  ) >> value_out;
    }
    else {
      std::ostringstream oss;
      oss << "key \"" << key_in << "\" not in configuration";
      throw std::runtime_error(oss.str());
    }
  }
  /***** Camera::ArchonController::get_configmap_value ************************/


  /***** Camera::ArchonController::get_status_key *****************************/
  /**
   * @brief      get value for the indicated key from the Archon "STATUS" string
   * @param[in]  key    key to extract from STATUS
   * @param[out] value  value of key
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::get_status_key(const std::string &key, std::string &value) {
    const std::string function("Camera::ArchonController::get_status_key");
    std::stringstream message;
    std::string reply;

    long error = this->send_cmd( STATUS, reply );  // first the whole reply in one string

    if ( error != NO_ERROR ) return error;

    std::vector<std::string> lines, tokens;
    Tokenize( reply, lines, " " );                 // then each line in a separate token "lines"

    for ( auto line : lines ) {
      Tokenize( line, tokens, "=" );               // break each line into tokens to get KEY=value
      if ( tokens.size() != 2 ) continue;          // need 2 tokens
      try {
        if ( tokens.at(0) == key ) {               // looking for the KEY
          value = tokens.at(1);                    // found the KEY= status here
          break;                                   // done looking
        } else continue;
      }
      catch (const std::exception &e) {            // should be impossible
        logwrite(function, "ERROR: "+std::string(e.what()));
        return ERROR;
      }
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonController::get_status_key *****************************/


  /***** Camera::ArchonController::set_power **********************************/
  /**
   * @brief      turn on|off controller bias power supplies
   * @param[in]  state  0=off, 1=on
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::set_power(int state) {
    const std::string function("Camera::ArchonController::power");
    long error=NO_ERROR;

    // must be connected
    if (!this->archon.isconnected()) {
      logwrite(function, "ERROR connection not open to controller");
      return ERROR;
    }

    // set power according to state
    switch( state ) {
      case 0:   // send POWEROFF command to Archon and wait 200ms to ensure off
                if ( (error=this->send_cmd(POWEROFF)) == NO_ERROR ) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                break;
      case 1:   // send POWERON command to Archon and wait 2s to ensure stable
                if ( (error=this->send_cmd(POWERON)) == NO_ERROR ) {
                  std::this_thread::sleep_for(std::chrono::seconds(2));
                }
                break;
      default:  logwrite(function, "ERROR expected 0|1");
                return ERROR;
    }

    // get_power on return sets the class power_status variable
    return( this->get_power() );
  }
  /***** Camera::ArchonController::set_power **********************************/


  /***** Camera::ArchonController::get_power **********************************/
  /**
   * @brief      get Archon power status
   * @details    This version only sets the class variable power_status.
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::get_power() {
    std::string dontcare;
    return( this->get_power(dontcare) );
  }
  /***** Camera::ArchonController::get_power **********************************/
  /**
   * @brief      get Archon power status
   * @details    This version returns the power_status.
   * @param[in]  power  reference to string to return the power_status
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::get_power(std::string &power) {
    const std::string function("Camera::ArchonController::get_power");

    // Read the Archon power state directly from Archon,
    // which will be a string representation of an integer.
    std::string power_status_key;
    if (this->get_status_key("POWER", power_status_key) != NO_ERROR) {
      logwrite(function, "ERROR getting status key: POWER");
      return ERROR;
    }

    // convert that string into a real integer
    int status=-1;
    try { status = std::stoi( power_status_key ); }
    catch (const std::exception &e) {
      logwrite(function, "ERROR parsing status key \""+power_status_key+"\": "+std::string(e.what()));
      return ERROR;
    }

    // convert that integer into a human readable string
    // set the power status (or not) depending on the value extracted from the STATUS message
    switch( status ) {
      case -1:                                  // no POWER token found in status message
        logwrite(function, "ERROR finding power in Archon status message" );
        return ERROR;
      case  0:                                  // usually an internal error
        this->power_status = "UNKNOWN";
        break;
      case  1:                                  // no configuration applied
        this->power_status = "NOT_CONFIGURED";
        break;
      case  2:                                  // power is off
        this->power_status = "OFF";
        break;
      case  3:                                  // some modules powered, some not
        this->power_status = "INTERMEDIATE";
        break;
      case  4:                                  // power is on
        this->power_status = "ON";
        break;
      case  5:                                  // system is in standby
        this->power_status = "STANDBY";
        break;
      default:                                  // should be impossible
        logwrite(function, "ERROR unknown power status: "+power_status_key);
        return ERROR;
    }

    // return variable is the class power status
    power = this->power_status;

    return NO_ERROR;
  }
  /***** Camera::ArchonController::power **************************************/


  /***** Camera::ArchonController::allocate_framebuf **************************/
  /**
   * @brief      allocate memory for frame buffer
   * @param[in]  reqsz  size in bytes of Archon frame buffer
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::allocate_framebuf(uint32_t reqsz) {
    const std::string function("Camera::ArchonController::allocate_framebuf");
    try {
      if (reqsz>0) {
        delete[] this->framebuf;
        this->framebuf = new char[reqsz];
        this->framebuf_bytes = reqsz;
      }
      else throw std::runtime_error("invalid requested size");
    }
    catch(const std::exception &e) {
      logwrite(function, "ERROR allocating framebuf: "+std::string(e.what()));
      this->framebuf_bytes = 0;
    }
    return (this->framebuf_bytes>0 ? NO_ERROR : ERROR);
  }
  /***** Camera::ArchonController::allocate_framebuf **************************/


  long ArchonController::read_frame(frametype_t type) {
    const std::string function("Camera::ArchonController::read_frame");
    logwrite(function, "reading frame from Archon");
    for (int i=0; i<100; i++) framebuf[i]=i+1;
    return NO_ERROR;
  }


  /***** Camera::ArchonController::write_config_key ***************************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @details    This function is overloaded. Use this version for char values.
   * @param[in]  key       char pointer to key
   * @param[in]  newvalue  char pointer to value
   * @param[out] changed   set true if changed
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::write_config_key( const char* key, const char* newvalue, bool &changed ) {
    const std::string function("Camera::ArchonController::write_config_key");
    std::stringstream message, sscmd;
    long error=NO_ERROR;

    if ( key==nullptr || newvalue==nullptr ) {
      error = ERROR;
      logwrite( function, "key|value cannot have NULL" );

    } else if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      message.str(""); message << "requested key " << key << " not found in configmap";
      logwrite( function, message.str() );

    } else if ( this->configmap[key].value == newvalue ) {
      // If no change in value then don't send the command
      error = NO_ERROR;
      message.str(""); message << "config key " << key << "=" << newvalue << " not written: no change in value";
      logwrite(function, message.str());

    } else {
        /**
     * Format and send the Archon WCONFIG command
     * to write the KEY=VALUE pair to controller memory
     */
      sscmd << "WCONFIG"
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->configmap[key].line
            << key
            << "="
            << newvalue;
      message.str(""); message << "sending: send_cmd(" << sscmd.str() << ")";
      logwrite(function, message.str());
      error = this->send_cmd((char*)sscmd.str().c_str());             // send the WCONFIG command here
      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;                        // save newvalue in the STL map
        changed = true;

      } else {
        message.str(""); message << "ERROR: config key=value: " << key << "=" << newvalue << " not written";
        logwrite( function, message.str() );
      }
    }
    return error;
  }
  /***** Camera::ArchonController::write_config_key ***************************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @details    This function is overloaded. Use this version for integer values.
   * @param[in]  key       pointer to key
   * @param[in]  newvalue  integer value
   * @param[out] changed   set true if changed
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::write_config_key( const char* key, int newvalue, bool &changed ) {
    std::stringstream newvaluestr;
    newvaluestr << newvalue;
    return ( write_config_key(key, newvaluestr.str().c_str(), changed) );
  }
  /***** Camera::ArchonController::write_config_key ***************************/


  /***** Camera::ArchonController::parse_system_configuration *****************/
  /**
   * @brief      write a configuration KEY=VALUE pair to the Archon controller
   * @brief
   * @param[in]  message
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::parse_system_configuration(const std::string &message) {
    const std::string function("Camera::ArchonController::get_system_configuration");
    if ( !is_connected ) {
      logwrite(function, "ERROR Archon connection not open");
      return ERROR;
    }

    std::vector<std::string> lines, tokens;
    Tokenize( message, lines, " " );                  // then each line in a separate token "lines"

    for ( const auto &line : lines ) {
      Tokenize( line, tokens, "_=" );                 // finally break each line into tokens to get module, type and version
      if ( tokens.size() != 3 ) continue;             // need 3 tokens

      std::string version;
      int module=0;
      int type=0;

      // get the module number
      //
      if ( tokens[0].compare( 0, 9, "BACKPLANE" ) == 0 ) {
        if ( tokens[1] == "VERSION" ) this->backplaneversion = tokens[2];
        continue;
      }

      // get the module and type of each module from MODn_TYPE
      //
      if ( ( tokens[0].compare( 0, 3, "MOD" ) == 0 ) && ( tokens[1] == "TYPE" ) ) {
        try {
          module = std::stoi( tokens[0].substr(3) );
          type   = std::stoi( tokens[2] );
        }
        catch (const std::exception &e) {
          std::stringstream err;
          err << "ERROR parsing module or type from " << tokens[0] << "=" << tokens[1] << ": " << e.what();
          logwrite( function, err.str() );
          return ERROR;
        }

      } else continue;

      // get the module version
      //
      if ( tokens[1] == "VERSION" ) version = tokens[2]; else version = "";

      // now store it permanently
      //
      if ( (module > 0) && (module <= NMODS) ) {
        try {
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module

        }
        catch (const std::out_of_range &e) {
          std::stringstream err;
          err << "ERROR module " << module << " out of range {1:" << NMODS << "}";
          logwrite( function, err.str() );
        }
      }
      else {                                          // else should never happen
        std::stringstream err;
        err << "ERROR module " << module << " outside range {1:" << NMODS << "}";
        logwrite( function, err.str() );
        return ERROR;
      }

      // Use the module type to resize the gain and offset vectors,
      // ADC module is type 2
      // ADM module is type 17
      //
      if (type==2 || type==17) {
        const auto size = (type==2) ? MAXADCCHANS : MAXADMCHANS;
        this->gain.resize( size );
        this->offset.resize( size );

        // Check that the AD modules are installed in the correct slot
        if ( module < 5 || module > 8 ) {
          std::stringstream err;
          err << "AD module (type=" << type << ") cannot be in slot " << module << ". Use slots 5-8";
          logwrite( function, err.str() );
          return ERROR;
        }
      }

    } // end for ( auto line : lines )

    return NO_ERROR;
  }
  /***** Camera::ArchonController::parse_system_configuration *****************/


  /***** Camera::ArchonExposureTime::split ************************************/
  /**
   * @brief      split the exposure time into sec+msec
   * @details    Exposure time is represented as double precision seconds
   *             but the Archon needs whole number seconds and/or milliseconds.
   *             Archon parameters are limited to 20-bits. When it fits,
   *             assign the exposure time to msec only, otherwise overflow
   *             into sec.
   *             This is only a tool and does not set any class variables.
   * @param[in]  value  double precision seconds
   * @return     pair{sec,msec}
   * @throws     std::runtime_error
   */
  std::pair<uint32_t,uint32_t> ArchonExposureTime::split(double value) {
    if (std::isnan(value)) throw std::runtime_error("value not a number");
    if (value < 0) throw std::runtime_error("value can't be negative");

    uint32_t totmsec;  // total exposure time in msec
    uint32_t sec;      // whole number of seconds
    uint32_t msec;     // whole number of msec

    // total exposure time in msec
    totmsec = static_cast<uint32_t>(std::round(value*1000.0));

    // if it fits in 20 bits then assign it all to msec
    if (totmsec <= 0xFFFFF) {
      sec  = 0;
      msec = totmsec;
    }
    else
    // otherwise assign it to seconds
    if (totmsec/1000 <= 0xFFFFF) {
      sec  = totmsec / 1000;
      msec = totmsec % 1000;
    }
    else throw std::runtime_error("value exceeds 2^20 sec");

    // return the pair
    return {sec,msec};
  }
  /***** Camera::ArchonExposureTime::split ************************************/

  /***** Camera::ArchonExposureTime::set **************************************/
  /**
   * @brief      set the exposure time
   * @details    This uses the class split() tool and sets class variables.
   * @param[in]  value  double precision seconds
   * @throws     std::runtime_error
   */
  void ArchonExposureTime::set(double value) {
    try {
      auto [sec,msec]   = split(value);
      this->sec_part    = sec;
      this->msec_part   = msec;
      this->exptime_sec = sec + msec/1000.0;  // store value rounded to nearest msec
    }
    catch (const std::exception &e) { throw; }
  }
  /***** Camera::ArchonExposureTime::set **************************************/

  /***** Camera::ArchonExposureTime::get_pair *********************************/
  /**
   * @brief      return the sec,msec parts as a pair
   * @return     sec,msec
   */
  std::pair<uint32_t,uint32_t> ArchonExposureTime::get_pair() const {
    return {this->sec_part, this->msec_part};
  }
  /***** Camera::ArchonExposureTime::get_pair *********************************/

}
