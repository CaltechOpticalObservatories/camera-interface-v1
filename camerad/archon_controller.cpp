/**
 * @file    archon_controller.cpp
 * @brief   implementation for Archon Interface Controller
 * @details These are hardware specific functions that the interface will use,
 *          not intended for direct-access by the user.
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
    activebufs(3),
    framebuf(nullptr),
    framebuf_bytes(0),
    is_connected(false),
    is_powered(false),
    is_firmwareloaded(false),
    readout_time_msec(0),
    frameinfo{
      .index{0},
      .currentframe{0}
    }
  {
    // pre-size the modtype and modversion vectors to hold the max number of modules
    this->modtype.resize(MAXNMODS);
    this->modversion.resize(MAXNMODS);

    // pre-size frameinfo_t vectors to hold max number of buffers
    this->frameinfo.bufsample.resize(MAXNBUFS);
    this->frameinfo.bufcomplete.resize(MAXNBUFS);
    this->frameinfo.bufmode.resize(MAXNBUFS);
    this->frameinfo.bufbase.resize(MAXNBUFS);
    this->frameinfo.bufframen.resize(MAXNBUFS);
    this->frameinfo.bufwidth.resize(MAXNBUFS);
    this->frameinfo.bufheight.resize(MAXNBUFS);
    this->frameinfo.bufpixels.resize(MAXNBUFS);
    this->frameinfo.buflines.resize(MAXNBUFS);
    this->frameinfo.bufrawblocks.resize(MAXNBUFS);
    this->frameinfo.bufrawlines.resize(MAXNBUFS);
    this->frameinfo.bufrawoffset.resize(MAXNBUFS);
    this->frameinfo.buftimestamp.resize(MAXNBUFS);
    this->frameinfo.bufretimestamp.resize(MAXNBUFS);
    this->frameinfo.buffetimestamp.resize(MAXNBUFS);

    {
    auto ptr=std::make_unique<ArchonExposureTime>();  // create ArchonExposureTime object
    this->exposure_time=ptr.get();                    // store non-owning pointer to derived type
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
    int numapplied=0, lastapplied=0;

    if (this->interface->configfile.n_rows < 1) throw std::runtime_error("empty configuration");

    // iterate through each row in config file
    for (int row=0; row < this->interface->configfile.n_rows; row++) {

      lastapplied=numapplied;

      // ARCHON_IP
      if (this->interface->configfile.param[row]=="ARCHON_IP") {
        this->archon.sethost( this->interface->configfile.arg[row] );
        numapplied++;
      }
      else
      // ARCHON_PORT
      if (this->interface->configfile.param[row]=="ARCHON_PORT") {
        try {
          this->archon.setport( std::stoi(this->interface->configfile.arg[row]) );
          numapplied++;
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
        numapplied++;
      }
      else
      // EXPTIME_SEC_PARAM
      if (this->interface->configfile.param[row]=="EXPTIME_SEC_PARAM") {
        this->sec_param = this->interface->configfile.arg[row];
        numapplied++;
      }
      else
      // EXPTIME_MSEC_PARAM
      if (this->interface->configfile.param[row]=="EXPTIME_MSEC_PARAM") {
        this->msec_param = this->interface->configfile.arg[row];
        numapplied++;
      }
      else
      // ABORT_PARAM
      if (this->interface->configfile.param[row]=="ABORT_PARAM") {
        this->abort_param = this->interface->configfile.arg[row];
        numapplied++;
      }
      else
      // EXPOSE_PARAM
      if (this->interface->configfile.param[row]=="EXPOSE_PARAM") {
        this->expose_param = this->interface->configfile.arg[row];
        numapplied++;
      }

      // publish and/or log applied configuration
      if (numapplied > lastapplied) {
        std::ostringstream oss;
        oss << "config:" << this->interface->configfile.param[row]
            << "=" << this->interface->configfile.arg[row];
        logwrite(function, oss.str());  // TODO publish?
      }
    }
  }
  /***** Camera::ArchonController::configure_controller ***********************/


  /***** Camera::ArchonController::abort **************************************/
  /**
   * @brief      set the abort parameter = 1
   * @details    This accomodates an ACF file which needs an abort parameter to
   *             be set in order to abort an operation and return to a previous
   *             state. This feature is optional. If no abort parameter is defined
   *             in the config file then no action takes place here, otherwise
   *             set that parameter to 1.
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::abort() {

    // if an abort parameter was not configured then nothing to do
    if (this->abort_param.empty()) return NO_ERROR;

    // otherwise set that parameter=1
    return this->set_parameter(this->abort_param, 1);
  }
  /***** Camera::ArchonController::abort **************************************/


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
      this->is_connected = this->archon.isconnected();
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
      if (module<1 || module>MAXNMODS) {
        message.str(""); message << "module " << module << " outside range {1:" << MAXNMODS << "}";
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
        message.str(""); message << "requested module " << module << " out of range {1:" << MAXNMODS << "}";
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

    // Make sure the following systemkeys are added.
    // They can be changed at any time by a command but since they have defaults
    // they don't require a command so this ensures they get into the systemkeys db.
    //
    std::stringstream keystr;
    keystr << "HDRSHIFT=" << this->n_hdrshift << "// number of HDR right-shift bits";
    this->interface->camera_info.systemkeys.addkey( keystr.str() );

    // Ensures these values are set on startup
    //
    this->get_frame_status();

    if (this->lastframe==0) {
      for (int i=0; i<MAXNBUFS; ++i) {
        if (this->frameinfo.bufframen[i] > this->lastframe) {
          this->lastframe = this->frameinfo.bufframen[i];
          this->lasttimestamp = this->frameinfo.buftimestamp[i];
          this->frameinfo.index.store(i);
        }
      }
      if (this->lastframe==0) {
        this->frameinfo.index.store(0);
        this->lasttimestamp=0;
      }
    }
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
    if ( (mod < 0) || (mod > MAXNMODS) ) {
      oss << "module " << mod << ": outside range {0:" << MAXNMODS << "}";
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


  /***** Camera::ArchonController::initiate_exposure **************************/
  /**
   * @brief      initiate an exposure by setting the parameter given by expose_param = nexp
   * @param[in]  nexp
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::initiate_exposure(const int &nexp) {
    logwrite("Camera::ArchonController::initiate_exposure", std::to_string(nexp));
    return( this->set_parameter(this->expose_param, nexp) );
  }
  /***** Camera::ArchonController::initiate_exposure **************************/


  /***** Camera::ArchonController::get_frame_status ***************************/
  /**
   * @brief      get Archon frame buffer status from FRAME command
   * @details    Sends the "FRAME" command to Archon, reads and parses the reply
   *             into framestatus structure.
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::get_frame_status() {
    const std::string function("Camera::ArchonController::get_frame_status");
    std::string reply;
    char message[512];
    long  error=NO_ERROR;

    // send FRAME command to get frame buffer status
    //
    if ( (error = this->send_cmd(FRAME, reply)) ) {
      if (error==ERROR) logwrite(function, "ERROR sending FRAME command");  // don't log here if BUSY
      return error;
    }

    // use direct pointer indexing for speed
    //
    const char* reply_ptr = reply.c_str();
    const char* reply_end = reply_ptr + reply.length();

    // reply is a continuous string of key=value pairs "TIMER=xxxx RBUF=xxxx " ... etc.
    // Find the key by reading chars up to the equal sign.
    //
    while (reply_ptr < reply_end) {

      // skip whitespace
      while (reply_ptr < reply_end && *reply_ptr==' ') reply_ptr++;
      if (reply_ptr > reply_end) break;

      // find key
      const char* key_start = reply_ptr;
      while (reply_ptr < reply_end && *reply_ptr != '=' && *reply_ptr != ' ') reply_ptr++;
      if (reply_ptr >= reply_end || *reply_ptr != '=') break;

      size_t key_len = reply_ptr - key_start;
      reply_ptr++;  // skip "="

      // find value
      const char* value_start = reply_ptr;
      while (reply_ptr < reply_end && *reply_ptr != ' ') reply_ptr++;
      size_t valuelen = reply_ptr - value_start;

      // TIMER=XXXX pattern
      if (key_len==5 && key_start[0]=='T' && std::strncmp(key_start, "TIMER", 5)==0) {
        this->frameinfo.timer.assign(value_start, valuelen);
      }
      else
      if (key_len==4) {
        // RBUF=XXXX pattern
        if (key_start[0]=='R' && std::strncmp(key_start, "RBUF", 4)==0) {
          this->frameinfo.rbuf = std::atoi(value_start);
        }
        else
        // WBUF=XXXX pattern
        if (key_start[0]=='W' && std::strncmp(key_start, "WBUF", 4)==0) {
          this->frameinfo.wbuf = std::atoi(value_start);
        }
      }
      else
      // BUFnXXXX=XXXX pattern...
      if (key_len>3 && key_start[0]=='B' && key_start[1]=='U' && key_start[2]=='F') {
        int bufnum = key_start[3]-'1';  // convert to 0-based

        // match suffix
        const char* suffix = key_start+4;
        size_t suffix_len = key_len-4;

        switch (suffix_len) {
          case 4:  // BUFnBASE, MODE
            if (std::strncmp(suffix, "BASE", 4)==0) this->frameinfo.bufbase[bufnum] = std::strtoul(value_start, nullptr, 10);
            else
            if (std::strncmp(suffix, "MODE", 4)==0) this->frameinfo.bufmode[bufnum] = std::atoi(value_start);
            break;
          case 5:  // BUFnFRAME, WIDTH, LINES
            if (std::strncmp(suffix, "FRAME", 5)==0) this->frameinfo.bufframen[bufnum] = std::atoi(value_start);
            else
            if (std::strncmp(suffix, "WIDTH", 5)==0) this->frameinfo.bufwidth[bufnum] = std::atoi(value_start);
            else
            if (std::strncmp(suffix, "LINES", 5)==0) this->frameinfo.buflines[bufnum] = std::atoi(value_start);
            break;
          case 6:  // BUFnSAMPLE, PIXELS, HEIGHT
            if (std::strncmp(suffix, "SAMPLE", 6)==0) this->frameinfo.bufsample[bufnum] = std::atoi(value_start);
            else
            if (std::strncmp(suffix, "PIXELS", 6)==0) this->frameinfo.bufpixels[bufnum] = std::atoi(value_start);
            else
            if (std::strncmp(suffix, "HEIGHT", 6)==0) this->frameinfo.bufheight[bufnum] = std::atoi(value_start);
            break;
          case 8:  // BUFnCOMPLETE
            if (std::strncmp(suffix, "COMPLETE", 8)==0) this->frameinfo.bufcomplete[bufnum] = std::atoi(value_start);
            break;
          case 9:  // BUFnTIMESTAMP
            if (std::strncmp(suffix, "TIMESTAMP", 9)==0) this->frameinfo.buftimestamp[bufnum] = std::strtoull(value_start, nullptr, 16);
            break;
          case 11: // BUFnRETIMESTAMP, FETIMESTAMP
            if (std::strncmp(suffix, "RETIMESTAMP", 11)==0) this->frameinfo.bufretimestamp[bufnum] = std::strtoull(value_start, nullptr, 16);
            else
            if (std::strncmp(suffix, "FETIMESTAMP", 11)==0) this->frameinfo.buffetimestamp[bufnum] = std::strtoull(value_start, nullptr, 16);
            break;
        } // end switch(suffix_len)
      } // end if BUFnXXXX pattern
    } // end looping through reply

    int completed_index = -1;
    int newestframe=0, newestbuf;

    for (int i = 0; i < MAXNBUFS; ++i) {
      if (this->frameinfo.bufframen[i] > newestframe) {
        this->frameinfo.currentframe.store(this->frameinfo.bufframen[i]);
        if (this->frameinfo.bufcomplete[i]) {
          newestframe = this->frameinfo.bufframen[i];
          completed_index = i;
        }
      }
    }

    this->lastframe = newestframe;

    if (completed_index != -1) {
      this->lasttimestamp = this->frameinfo.buftimestamp[completed_index];
      this->frameinfo.index.store(completed_index);
      this->frameinfo.next_index = (completed_index + 1) % this->activebufs;
    }

    return error;
  }
  /***** Camera::ArchonController::get_frame_status ***************************/


  /***** Camera::ArchonController::get_parameter ******************************/
  /**
   * @brief      gets parameter value from Archon configuration memory
   * @details    This template function sends an RCONFIG command to Archon
   *             and returns a string. Another template version returns int.
   * @param[in]  parameter
   * @return     string value of parameter
   * @throws     std::runtime_error
   *
   */
  template<> std::string ArchonController::get_parameter<std::string>(const std::string &parameter) {
    const std::string function("Archon::Interface::read_parameter");

    // requested parameter must be in the parammap
    //
    if (this->parammap.find(parameter.c_str()) == this->parammap.end()) {
      throw std::runtime_error("parameter \""+parameter+"\" not found in ACF");
    }

    // form the RCONFIG command to send to Archon
    //
    std::ostringstream cmd;
    cmd << "RCONFIG"
        << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
        << this->parammap[parameter.c_str()].line;

    std::string reply;

    // send RCONFIG command to Archon
    //
    if (this->send_cmd(cmd.str(), reply) != NO_ERROR) {
      throw std::runtime_error("sending "+cmd.str());
    }
    strip_newline(reply);

    // reply should now be of the form PARAMETERn=PARAMNAME=VALUE
    // check for that format by making sure it starts with PARAMETER
    // and contains two equal '=' signs
    //
    if ( (std::count(reply.begin(), reply.end(), '=') != 2) ||
         (reply.substr(0,9) != "PARAMETER") ) {
      throw std::runtime_error("malformed reply \""+reply+"\": expected PARAMETERn=NAME=VALUE");
    }

    // return just the VALUE here (find from the right)
    //
    size_t loc = reply.rfind('=');
    return reply.substr(++loc);
  }
  /***** Camera::ArchonController::get_parameter ******************************/
  /**
   * @brief      gets parameter value from Archon configuration memory
   * @details    This template version uses the string version and returns an integer.
   * @param[in]  parameter
   * @return     int value of parameter
   * @throws     std::runtime_error|std::exception
   *
   */
  template<> int ArchonController::get_parameter<int>(const std::string &parameter) {
    try {
      return std::stoi( this->get_parameter<std::string>(parameter) );
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Camera::ArchonController::get_parameter ******************************/


  /***** Camera::ArchonController::get_timer **********************************/
  /**
   * @brief      read the 64 bit interal timer from the Archon controller
   * @details    Sends the "TIMER" command to Archon, reads back the reply, and
   *             stores the value as (unsigned long int) in the reference param.
   *             This is an internal 64 bit timer/counter. One tick of the counter
   *             is 10 ns.
   * @param[out] timer  reference to timer value
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::get_timer(uint64_t &timer) {
    const std::string function("Camera::ArchonController::get_timer");
    std::string reply;

    // send TIMER command
    //
    long error = this->send_cmd(TIMER, reply);
    if (error != NO_ERROR) {
      logwrite(function, "could not read Archon TIMER");
      return error;
    }

    std::vector<std::string> tokens;
    Tokenize(reply, tokens, "=");                   // Tokenize the reply

    // Response should be "TIMER=xxxx\n" so there needs
    // to be two tokens
    //
    if (tokens.size() != 2) {
      logwrite(function, "unrecognized response \""+reply+"\": expected TIMER=xxxx");
      return ERROR;
    }

    // Second token must be a hexidecimal string
    //
    strip_newline(tokens[1]);
    if (!std::all_of(tokens[1].begin(), tokens[1].end(), ::isxdigit)) {
      logwrite(function, "value \""+tokens[1]+"\" not a hexadecimal string");
      return ERROR;
    }

    // convert from hex string to integer and save return value
    //
    try { timer = std::stoul(tokens[1], nullptr, 16);
    }
    catch (const std::exception &e) {
      logwrite(function, std::string(e.what()));
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonController::get_timer **********************************/


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
      throw std::runtime_error("exposure time parameters not in configuration");
    }

    try {
      // split the requested exposure time into seconds and milliseconds
      auto [sec, msec] = this->exposure_time->split(exptime);

      // Set the sec and msec parameters on the controller,
      // store the exptime in the class on success.
      if ( (set_parameter(sec_param, sec)   == NO_ERROR) &&
           (set_parameter(msec_param, msec) == NO_ERROR) ) {
        this->exposure_time->set(exptime);
      }
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Camera::ArchonController::set_exptime ********************************/


  /***** Camera::ArchonController::set_parameter ******************************/
  /**
   * @brief      set a parameter using FASTPREP, FASTLOADPARAM
   * @param[in]  parameter  reference to string parameter name
   * @param[in]  value      reference to int value
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::set_parameter(const std::string &parameter, const int &value) {
    const std::string function("Camera::ArchonController::set_parameter");
    long error=NO_ERROR;
    try {
      error |= this->prep_parameter(parameter, value);
      error |= this->load_parameter(parameter, value);
    }
    catch (const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    std::ostringstream oss;
    oss << (error==ERROR ? "ERROR setting " : "set ") << parameter;
    logwrite(function, oss.str());

    return error;
  }
  /***** Camera::ArchonController::set_parameter ******************************/


  /***** Camera::ArchonController::prep_parameter *****************************/
  /**
   * @brief      sends Archon command FASTPREPPARAM <parameter> <value>
   * @param[in]  parameter  reference to string parameter name (64 chars)
   * @param[in]  value      reference to int value in range {0:1048575}
   * @throws     std::runtime_error
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::prep_parameter(const std::string &parameter, const int &value) {
    if (value < 0 || value > 0xFFFFF ) {
      throw std::runtime_error("prep_parameter \""+std::to_string(value)+"\" outside range {0:1048575}");
    }
    char cmd[88];
    SNPRINTF(cmd, "FASTPREPPARAM %s %d", parameter.c_str(), value);
    return ( this->send_cmd(cmd) );
  }
  /***** Camera::ArchonController::prep_parameter *****************************/


  /***** Camera::ArchonController::load_parameter *****************************/
  /**
   * @brief      sends Archon command FASTLOADPARAM <parameter> <value>
   * @param[in]  parameter  reference to string parameter name (64 chars)
   * @param[in]  value      reference to int value in range {0:1048575}
   * @throws     std::runtime_error
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::load_parameter(const std::string &parameter, const int &value) {
    if (value < 0 || value > 0xFFFFF ) {
      throw std::runtime_error("load_parameter \""+std::to_string(value)+"\" outside range {0:1048575}");
    }
    char cmd[88];
    SNPRINTF(cmd, "FASTLOADPARAM %s %d", parameter.c_str(), value);
    return ( this->send_cmd(cmd) );
  }
  /***** Camera::ArchonController::load_parameter *****************************/


  /***** Camera::ArchonController::print_frame_status *************************/
  /**
   * @brief      prints Archon frame buffer status to the log
   * @details    Call get_frame_status() first, then formats and prints.
   *
   */
  void ArchonController::print_frame_status() {
    const std::string function("Camera::ArchonController::print_frame_status");

    this->get_frame_status();

    std::ostringstream message;
    int bufn;
    char statestr[ArchonController::MAXNBUFS][4];
    auto index = this->frameinfo.index.load();

    // write as log message
    //
    message.str(""); message << "    buf base       rawoff     frame ready lines rawlines rblks width height state";
    logwrite(function, message.str());
    message.str(""); message << "    --- ---------- ---------- ----- ----- ----- -------- ----- ----- ------ -----";
    logwrite(function, message.str());
    message.str("");
    for (bufn=0; bufn < ArchonController::MAXNBUFS; bufn++) {
      memset(statestr[bufn], '\0', 4);
      if ( (this->frameinfo.rbuf-1) == bufn)   strcat(statestr[bufn], "R");
      if ( (this->frameinfo.wbuf-1) == bufn)   strcat(statestr[bufn], "W");
      if ( this->frameinfo.bufcomplete[bufn] ) strcat(statestr[bufn], "C");
    }
    for (bufn=0; bufn < ArchonController::MAXNBUFS; bufn++) {
      message << std::setw(3) << (bufn==index ? "-->" : "") << " ";                       // buf
      message << std::setw(3) << bufn+1 << " ";
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frameinfo.bufbase[bufn] << " ";                                                    // base
      message << "0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex
              << this->frameinfo.bufrawoffset[bufn] << " ";                                               // rawoff
      message << std::setfill(' ') << std::setw(5) << std::dec << this->frameinfo.bufframen[bufn] << " "; // frame
      message << std::setw(5) << this->frameinfo.bufcomplete[bufn] << " ";                                // ready
      message << std::setw(5) << this->frameinfo.buflines[bufn] << " ";                                   // lines
      message << std::setw(8) << this->frameinfo.bufrawlines[bufn] << " ";                                // rawlines
      message << std::setw(5) << this->frameinfo.bufrawblocks[bufn] << " ";                               // rblks
      message << std::setw(5) << this->frameinfo.bufwidth[bufn] << " ";                                   // width
      message << std::setw(6) << this->frameinfo.bufheight[bufn] << " ";                                  // height
      message << std::setw(5) << statestr[bufn];                                                          // state
      logwrite(function, message.str());
      message.str("");
    }
  }
  /***** Camera::ArchonController::print_frame_status *************************/


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
    char check[4];
    int  retval;
    int  error = NO_ERROR;

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
    if ( this->archon_busy.test_and_set() ) {
      logwrite(function, "ERROR Archon busy: ignored command \""+cmd+"\"");
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
      this->archon_busy.clear();
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

    // For all other commands, receive the reply.
    // In autofetch mode, the Archon may interleave unsolicited <QF frame data
    // on the socket. Discard any such data and keep reading until the expected
    // command response (<XX) arrives.
    //
    constexpr size_t BUFSZ = 64*1024;
    auto buffer = std::make_unique<char[]>(BUFSZ+1);
    reply.clear();
    do {
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { logwrite(function, "Poll timeout waiting for response from Archon command (maybe unrecognized command?)"); error=TIMEOUT; }
        if (retval<0)  { logwrite(function, "Poll error waiting for response from Archon command"); error=ERROR; }
        break;
      }
      retval = this->archon.Read(buffer.get(), BUFSZ);
      if (retval <= 0) {
        logwrite( function, "ERROR reading Archon" );
        break;
      }
      buffer[retval] = '\0';

      // In autofetch mode, discard unsolicited autofetch frame data
      if (this->interface->is_autofetch_mode &&
          retval >= 3 && std::memcmp(buffer.get(), "<QF", 3) == 0) {
        continue;
      }

      reply.append(buffer.get(), retval);
      if (std::memchr(buffer.get(), '\n', retval) != nullptr) break;
    } while(retval>0);

    // If there was an Archon error then clear the busy flag and get out now
    //
    if ( error != NO_ERROR ) {
      this->archon_busy.clear();
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
      logwrite(function, "ERROR from Archon processing \""+cmd+"\"");
    }
    else
    // First 3 bytes of reply must equal checksum else reply doesn't belong to command
    if (reply.size()<3 || std::memcmp(reply.data(), check, 3) != 0) {
      error = ERROR;
      std::string hdr = reply;
      try { scmd.erase(scmd.find("\n"), 1); } catch(...) { }
      logwrite(function, "ERROR command-reply mismatch for \""+cmd+"\": expected "+check+" but received "+reply);
    }
    else {
      // command and reply are a matched pair
      error = NO_ERROR;
      reply.erase(0, 3);                             // strip off the msgref from the reply
    }

    // clear the busy flag
    this->archon_busy.clear();

    return error;
  }
  /***** Camera::ArchonController::send_cmd ***********************************/


  /***** Camera::ArchonController::fetch **************************************/
  /**
   * @brief      fetch an Archon frame buffer
   * @param[in]  bufferaddress
   * @param[in]  bufferblocks
   * @return     NO_ERROR | ERROR
   *
   */
  long ArchonController::fetch(uint64_t bufferaddress, uint32_t bufferblocks) {
    const std::string function("Camera::ArchonController::fetch");
    char message[256];
    auto index = this->frameinfo.index.load();
    uint32_t maxblocks  = (uint32_t)(1.5E9 / this->activebufs / 1024 );
    uint64_t maxoffset  = this->frameinfo.bufbase[index];
    uint64_t maxaddress = maxoffset + maxblocks;

    if (bufferaddress > maxaddress) {
      SNPRINTF(message, "fetch Archon buffer requested address 0x%0lX exceeds 0x%0lX", bufferaddress, maxaddress);
      logwrite(function, std::string(message));
      return ERROR;
    }
    if ( bufferblocks > maxblocks ) {
      SNPRINTF(message, "fetch Archon buffer requested blocks 0x%0X exceeds 0x%0X", bufferblocks, maxblocks);
      logwrite(function, std::string(message));
      return ERROR;
    }

    char buf[32];
    SNPRINTF(buf, "FETCH%08" PRIX64 "%08" PRIX32, bufferaddress, bufferblocks);

    // Sending send_cmd( FETCH ) will set the archon busy flag and not clear it.
    // If there's an error then send_cmd() probably cleared it but it's OK to
    // make sure that it's cleared here.
    //
    if ( this->send_cmd( std::string(buf) ) == ERROR ) {
      logwrite(function, "ERROR: sending FETCH command. Aborting read.");
      this->archon_busy.clear();
      this->unlock_buffer();
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonController::fetch **************************************/


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
          this->modemap[mode].acfkeys.keydb[keyword].keytype    = this->interface->camera_info.userkeys.get_keytype(keyvalue);
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
      this->interface->camera_info.systemkeys.addkey( keystr.str() );
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


  /***** Camera::ArchonController::load_mode_settings *************************/
  /**
   * @brief      loads parameters and keywords for a given mode
   * @details    The ACF may contain optional sections tagged as [MODE_xxxx]
   *             which may contain parameters, keywords, etc. This function
   *             applies those configuration parameters to the Archon.
   * @param[in]  mode  pointer to modeinfo_t struct from modemap
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::load_mode_settings(modeinfo_t* mode) {
    const std::string function("Camera::ArchonController::load_mode_settings");

    try {
      this->get_configmap_value("SAMPLEMODE", mode->samplemode);
      this->get_configmap_value("BIGBUF", mode->bigbuf);
      this->get_configmap_value("FRAMEMODE", mode->geometry.framemode);
      this->get_configmap_value("LINECOUNT", mode->geometry.linecount);
      this->get_configmap_value("PIXELCOUNT", mode->geometry.pixelcount);
      this->get_configmap_value("RAWENABLE", mode->rawenable);
      this->get_configmap_value("RAWSEL", this->rawinfo.adchan);
      this->get_configmap_value("RAWSAMPLES", this->rawinfo.rawsamples);
      this->get_configmap_value("RAWENDLINE", this->rawinfo.rawlines);
    }
    catch (const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonController::load_mode_settings *************************/


  /***** Camera::ArchonController::lock_buffer ********************************/
  /**
   * @brief      lock Archon frame buffer
   * @param[in]  buffernumber
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::lock_buffer(int buffernumber) {
    char cmd[8];
    SNPRINTF(cmd, "LOCK%d", buffernumber);
    if (this->send_cmd(std::string(cmd)) != NO_ERROR) {
      logwrite("Camera::ArchonController::lock_buffer", "ERROR sending "+std::string(cmd));
      return ERROR;
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonController::lock_buffer ********************************/


  /***** Camera::ArchonController::unlock_buffer ******************************/
  /**
   * @brief      unlock Archon frame buffer
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::unlock_buffer() {
    if (this->send_cmd(UNLOCK) != NO_ERROR) {
      logwrite("Camera::ArchonController::unlock_buffer", "ERROR sending UNLOCK");
      return ERROR;
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonController::unlock_buffer ******************************/


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
   * @return     string power state
   * @throws     std::runtime_error
   *
   */
  std::string ArchonController::set_power(int state) {
    // must be connected
    if (!this->archon.isconnected()) {
      throw std::runtime_error("connection not open to controller");
    }

    // set power according to state
    switch( state ) {
      case 0:   // send POWEROFF command to Archon and wait 200ms to ensure off
                if ( this->send_cmd(POWEROFF) == NO_ERROR ) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                break;
      case 1:   // send POWERON command to Archon and wait 2s to ensure stable
                if ( this->send_cmd(POWERON) == NO_ERROR ) {
                  std::this_thread::sleep_for(std::chrono::seconds(2));
                }
                break;
      default:  throw std::runtime_error("invalid state: expected 0|1");
    }

    // get_power on return sets the class power_status variable
    return( this->get_power() );
  }
  /***** Camera::ArchonController::set_power **********************************/


  /***** Camera::ArchonController::get_power **********************************/
  /**
   * @brief      get Archon power status
   * @param[in]  power  reference to string to return the power_status
   * @return     string  Archon power status
   * @throws     std::runtime_error
   *
   */
  std::string ArchonController::get_power() {

    // Read the Archon power state directly from Archon,
    // which will be a string representation of an integer.
    std::string power_status_key;
    if (this->get_status_key("POWER", power_status_key) != NO_ERROR) {
      throw std::runtime_error("reading status key: POWER");
    }

    // convert that string into a real integer
    int status=-1;
    try { status = std::stoi( power_status_key ); }
    catch (const std::exception &e) {
      throw std::runtime_error("parsing status key \""+power_status_key+"\": "+std::string(e.what()));
    }

    // power_status has 5 different states. this is only true if state is POWER_ON
    this->is_powered=false;

    // convert that integer into a human readable string
    // set the power status (or not) depending on the value extracted from the STATUS message
    switch( status ) {
      case -1:                                  // no POWER token found in status message
        throw std::runtime_error("no POWER token found in status message");
      case  0:                                  // usually an internal error
        this->power_status = POWER_UNKNOWN;
        break;
      case  1:                                  // no configuration applied
        this->power_status = POWER_NOT_CONFIGURED;
        break;
      case  2:                                  // power is off
        this->power_status = POWER_OFF;
        break;
      case  3:                                  // some modules powered, some not
        this->power_status = POWER_INTERMEDIATE;
        break;
      case  4:                                  // power is on
        this->power_status = POWER_ON;
        this->is_powered=true;
        break;
      case  5:                                  // system is in standby
        this->power_status = POWER_STANDBY;
        break;
      default:                                  // should be impossible
        throw std::runtime_error("unknown power status: "+power_status_key);
    }

    // return variable is the class power status
    return this->power_status;
  }
  /***** Camera::ArchonController::get_power **********************************/


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


  long ArchonController::read_frame(frametype_t type, char* &imagebufferptr) {
    const std::string function("Camera::ArchonController::read_frame");
    char message[256];
    int retval;
    int bufready;
    char check[5], header[5];
    int bytesread, totalbytesread, toread;
    uint64_t bufaddr;
    unsigned int block, bufblocks=0;
    long error = ERROR;
    int num_detect = this->modemap[this->selectedmode].geometry.num_detect;
    auto index = this->frameinfo.index.load();

    this->frametype = type;

    // Archon buffer number of the last frame read into memory
    //
    bufready = index + 1;

    if (bufready < 1 || bufready > this->activebufs) {
      SNPRINTF(message, "invalid Archon buffer %d requested. Expected {1:%d}", bufready, this->activebufs);
      logwrite(function, std::string(message));
      return ERROR;
    }

    // Lock the frame buffer before reading it
    //
    if ( this->lock_buffer(bufready) == ERROR) { logwrite(function, "ERROR locking frame buffer"); return ERROR; }

    // Send the FETCH command to read the memory buffer from the Archon backplane.
    // Archon replies with one binary response per requested block. Each response
    // has a message header.
    //
    switch (this->frametype) {
      case Camera::ArchonController::FRAME_RAW:
        // Archon buffer base address
        bufaddr   = this->frameinfo.bufbase[index] + this->frameinfo.bufrawoffset[index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks = (unsigned int) floor( (this->interface->camera_info.image_memory + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      case Camera::ArchonController::FRAME_IMAGE:
        // Archon buffer base address
        bufaddr   = this->frameinfo.bufbase[index];

        // Calculate the number of blocks expected. image_memory is bytes per detector
        bufblocks =
        (unsigned int) floor( ((this->interface->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN );
        break;

      default:  // should be impossible
        SNPRINTF(message, "unknown frame type specified: %d: expected FRAME_RAW | FRAME_IMAGE", this->frametype);
        logwrite(function, std::string(message));
        return(ERROR);
        break;
    }

    // send the FETCH command.
    // This will set the archon_busy flag, but not clear it (except on error).
    //
    error = this->fetch(bufaddr, bufblocks);

    if ( error != NO_ERROR ) {
      logwrite(function, "ERROR fetching Archon buffer");
      return error;
    }

    // Read the data from the connected socket into memory, one block at a time
    //
    totalbytesread = 0;
    for (block=0; block<bufblocks; block++) {

      // Are there data to read?
      if ( (retval=this->archon.Poll()) <= 0) {
        if (retval==0) { SNPRINTF(message, "Poll timeout waiting for Archon frame data"); error = ERROR;   }  // TODO should error=TIMEOUT?
        if (retval<0)  { SNPRINTF(message, "Poll error waiting for Archon frame data");   error = ERROR;   }
        if ( error != NO_ERROR ) logwrite(function, std::string(message));
        break;                         // breaks out of for loop
      }

      // Wait for a block+header Bytes to be available
      // (but don't wait more than 1 second -- this should be tens of microseconds or less)
      //
      auto start = std::chrono::steady_clock::now();             // start a timer now

      while ( this->archon.Bytes_ready() < (BLOCK_LEN+4) ) {
        auto now = std::chrono::steady_clock::now();             // check the time again
        std::chrono::duration<double> diff = now-start;          // calculate the duration
        if (diff.count() > 1) {                                  // break while loop if duration > 1 second
          logwrite(function, "timeout waiting for data from Archon");
          error = ERROR;
          break;                       // breaks out of while loop
        }
      }
      if ( error != NO_ERROR ) break;  // needed to also break out of for loop on error

      // Check message header
      //
      SNPRINTF(check, "<%02X:", this->msgref);
      if ( (retval=this->archon.Read(header, 4)) != 4 ) {
        SNPRINTF(message, "code %d reading Archon frame header", retval);
        logwrite(function, std::string(message));
        error = ERROR;
        break;                         // break out of for loop
      }

      if (header[0] == '?') {  // Archon retured an error
        SNPRINTF(message, "Archon returned \'?\' reading %s data", (this->frametype==Camera::ArchonController::FRAME_RAW?"raw ":"image "));
        logwrite(function, std::string(message));
        this->fetchlog();      // check the Archon log for error messages
        error = ERROR;
        break;                         // break out of for loop
      }
      else if (strncmp(header, check, 4) != 0) {
        SNPRINTF(message, "Archon command-reply mismatch reading %s data. header=%s check=%s",
                          (this->frametype==Camera::ArchonController::FRAME_RAW?"raw ":"image "), header, check);
        logwrite(function, std::string(message));
        error = ERROR;
        break;                         // break out of for loop
      }

      // Read the frame contents
      //
      bytesread = 0;
      do {
        toread = BLOCK_LEN - bytesread;
        if ( (retval=this->archon.Read(imagebufferptr, (size_t)toread)) > 0 ) {
          bytesread += retval;         // this will get zeroed after each block
          totalbytesread += retval;    // this won't (used only for info purposes)
          imagebufferptr += retval;    // advance pointer
        }
      } while (bytesread < BLOCK_LEN);

    } // end of loop: for (block=0; block<bufblocks; block++)

    // Archon has sent its data so clear the archon busy flag to
    // allow other threads to access the Archon now.
    //
    this->archon_busy.clear();

    // If we broke out of the for loop for an error then report incomplete read
    //
    if ( error==ERROR || block < bufblocks) {
      SNPRINTF(message, "incomplete frame read %d bytes: %d of %d 1024-byte blocks",
                        totalbytesread, block, bufblocks);
      logwrite(function, std::string(message));
      this->print_frame_status();
    }

    // Unlock the frame buffer
    //
    if (error == NO_ERROR) error = this->unlock_buffer();

    return error;
  }


  /***** Camera::ArchonController::wait_for_readout ***************************/
  /**
   * @brief      creates a wait until the next completed frame buffer is ready
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonController::wait_for_readout() {
    const std::string function("Camera::ArchonController::wait_for_readout");
    char message[256];
    long error = NO_ERROR;
    bool done = false;

    // fills frameinfo structure
    get_frame_status();

    // local copies
    int index                  = this->frameinfo.index.load();
    int latest_completed_frame = this->lastframe;
    int newframe               = this->frameinfo.currentframe.load();

    SNPRINTF(message, "waiting for new frame: lastframe=%d frameinfo.index=%d", this->lastframe, index);
    logwrite(function, std::string(message));

    // waittime is 10% over the specified readout time
    // and will be used to keep track of timeout errors
    //
    double waittime_ms = this->readout_time_msec * 1.1;      // this is in msec

    // if readout_time_msec was not defined or defined=0
    // then do not use a timeout timer
    //
    bool timeout_timer_enabled = (waittime_ms <= 0) ? false : true;

    uint64_t start_ns   = get_clock_time_nsec();             // returns nanoseconds
    uint64_t timeout_ns = (uint64_t)(waittime_ms * 1e6);     // convert waittime msec to nsec
    uint32_t pollcount  = 0;
    uint32_t busycount  = 0;
    int previous_frame  = this->lastframe;                   // initial frame number set once

    // Poll frame status until current frame is not the last frame and the buffer is ready to read.
    // The last frame was recorded before the readout was triggered in get_frame().
    //
    while ( !done && !this->interface->is_aborted() ) {

      error = this->get_frame_status();

      latest_completed_frame = this->lastframe;
      newframe               = this->frameinfo.currentframe.load();

      if (error == ERROR) {
        done = true;
        logwrite(function, "ERROR unable to get frame status");
        break;
      }
      else
      // If Archon is busy then ignore it, keep trying for up to ~ 3 second
      // (300000 attempts, ~10us between attempts)
      //
      if (error == BUSY) {
        if ( ++busycount > 30000 ) {
          done = true;
          logwrite(function, "ERROR received BUSY from Archon too many times trying to get frame status");
          break;
        }
        else {
          usleep(10); // reduces polling frequency
          continue;
        }
      }
      else busycount=0;

//    SNPRINTF(message, "previous_frame=%d latest_completed_frame=%d newframe=%d bufcomplete[%d]=%s",
//             previous_frame, latest_completed_frame, newframe, index, frameinfo.bufcomplete[index]?"T":"F");
//    logwrite(function, std::string(message));

      // latest completed frame number +1 above frame number coming in here,
      // then a new frame has arrived.
      //
      int frame_arrived = latest_completed_frame - (previous_frame+1);
      if (frame_arrived==0) {
        done  = true;
        error = NO_ERROR;
        break;
      }
      else
      // latest completed frame number more than +1 above frame number coming in here,
      // then at least one frame has been skipped.
      //
      if ( frame_arrived > 0 ) {
        SNPRINTF(message, "ERROR missed %d frame%s", frame_arrived, (frame_arrived>1?"s":""));
        logwrite(function, std::string(message));
        this->abort();
        done = true;
        error = ERROR;
        break;
      }

      // If the frame isn't done by the predicted time then
      // enough time has passed to trigger a timeout error.
      //
      if (timeout_timer_enabled &&
          ++pollcount >= 1000   &&
          (get_clock_time_nsec()-start_ns) > timeout_ns) {
        pollcount=0;
        done = true;
        error = ERROR;
        SNPRINTF(message, "timeout waiting for new frame exceeded %lf msec. lastframe=%d", waittime_ms, this->lastframe);
        logwrite(function, std::string(message));
        break;
      }

      usleep(10);  // reduces polling frequency
    } // end while (done == false && not this->camera.is_aborted)

    if ( error != NO_ERROR ) {
      logwrite(function, "ERROR waiting for readout");
      return error;
    }

    if ( this->interface->is_aborted() ) {
      logwrite(function, "wait for readout stopped by external signal");
      this->abort();
    }
#ifdef LOGLEVEL_DEBUG
    else {
      logwrite(function, "received currentframe: "+std::to_string(latest_completed_frame)+
                         " at TS "+std::to_string(this->lasttimestamp));
    }
#endif

    return NO_ERROR;
  }
  /***** Camera::ArchonController::wait_for_readout ***************************/


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
    long error=NO_ERROR;

    if ( key==nullptr || newvalue==nullptr ) {
      error = ERROR;
      logwrite( function, "ERROR key|value cannot have NULL" );
    }
    else
    if ( this->configmap.find(key) == this->configmap.end() ) {
      error = ERROR;
      logwrite(function, "ERROR key '"+std::string(key)+"' not in configuration");
    }
    else
    if ( this->configmap[key].value == newvalue ) {
      // If no change in value then don't send the command
      error = NO_ERROR;
      logwrite(function, "key '"+std::string(key)+"' not written. no change in value");
    }
    else {
    /**
     * Format and send the Archon WCONFIG command
     * to write the KEY=VALUE pair to controller memory
     */
      std::ostringstream sscmd;
      sscmd << "WCONFIG"
            << std::uppercase << std::setfill('0') << std::setw(4) << std::hex
            << this->configmap[key].line
            << key
            << "="
            << newvalue;

      error = this->send_cmd((char*)sscmd.str().c_str());             // send the WCONFIG command here

      sscmd.str(""); sscmd << key << "=" << newvalue << (error==ERROR ? "":"not") << " written";

      if (error==NO_ERROR) {
        this->configmap[key].value = newvalue;                        // save newvalue in the STL map
        changed = true;
      }
      logwrite(function, sscmd.str());
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
    if ( this->archon.isconnected() ) {
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
      if ( (module > 0) && (module <= MAXNMODS) ) {
        try {
          this->modtype.at(module-1)    = type;       // store the type in a vector indexed by module
          this->modversion.at(module-1) = version;    // store the type in a vector indexed by module

        }
        catch (const std::out_of_range &e) {
          std::stringstream err;
          err << "ERROR module " << module << " out of range {1:" << MAXNMODS << "}";
          logwrite( function, err.str() );
        }
      }
      else {                                          // else should never happen
        std::stringstream err;
        err << "ERROR module " << module << " outside range {1:" << MAXNMODS << "}";
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


  /***** Camera::ArchonController::set_vcpu_inreg *****************************/
  /**
   * @brief      write to a VCPU input register
   * @param[in]  args  reference to string expects <module> <inreg> <value>
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonController::set_vcpu_inreg(const std::string &args) {
    const std::string function("Camera::ArchonController::set_vcpu_inreg");

    // VCPU requires a minimum backplane version
    //
    std::ostringstream message;
    int ret = compare_versions( this->backplaneversion, REV_VCPU );
    if ( ret < 0 ) {
      if ( ret == -999 ) {
        message << "comparing backplane version " << this->backplaneversion << " to " << REV_VCPU;
      }
      else {
        message << "requires backplane version " << REV_VCPU << " or newer. ("
                << this->backplaneversion << " detected)";
      }
      logwrite(function, message.str());
      return ERROR;
    }

    // parse the args
    //
    std::istringstream iss(args);
    int module, reg, value;
    if (!(iss >> module >> reg >> value)) {
      logwrite(function, "ERROR expected (integers): <module> <reg> <value>");
      return ERROR;
    }

    // check that requested module is valid
    //
    if (module < 1 || module > this->modtype.size()) {
      logwrite(function, "ERROR invalid module '"+std::to_string(module)+"'");
      return ERROR;
    }

    switch ( this->modtype[ module-1 ] ) {
      case 0:
        message << "ERROR requested module " << module << " not installed";
        logwrite(function, message.str());
        return ERROR;
      case 3:  // LVBias
      case 5:  // Heater
      case 7:  // HS
      case 9:  // LVXBias
      case 10: // LVDS
      case 11: // HeaterX
        break;
      default:
        message << "ERROR requested module " << module << " does not contain a VCPU";
        logwrite(function, message.str());
        return ERROR;
    }

    // check that register number is valid
    //
    if ( reg < 0 || reg > 15 ) {
      message << "ERROR requested register " << reg << " outside range {0:15}";
      logwrite(function, message.str());
      return ERROR;
    }

    // check that value is within range
    //
    if ( value < 0 || value > 65535 ) {
      message << "ERROR requested value " << value << " outside range {0:65535}";
      logwrite(function, message.str());
      return ERROR;
    }

    std::ostringstream inreg_key;
    bool changed = false;
    inreg_key << "MOD" << module << "/VCPU_INREG" << reg;
    long error = this->write_config_key( inreg_key.str().c_str(), value, changed );

    if (error==NO_ERROR) {
      std::ostringstream applystr;
      applystr << "APPLYDIO"
               << std::setfill('0')
               << std::setw(2)
               << std::hex
               << (module-1);
      error = this->send_cmd(applystr.str());
    }
    return error;
  }
  /***** Camera::ArchonController::set_vcpu_inreg *****************************/


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
