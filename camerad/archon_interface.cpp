/**
 * @file    archon_interface.cpp
 * @brief   implementation of Archon Interface
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#include "archon_interface.h"
#include "archon_controller.h"

namespace Camera {

  /***** Camera::ArchonInterface::ArchonInterface *****************************/
  /**
   * @brief      ArchonInterface constructor
   *
   */
  ArchonInterface::ArchonInterface() {
    {
    auto ptr=std::make_unique<ArchonController>();    // create an Archon-specific Controller object
    this->controller=ptr.get();                       // store typed pointer
    Interface::controller = std::move(ptr);           // transfer ownership to base class
    }

    controller->set_interface(this);                  // provides Controller access to my Interface
  }
  /***** Camera::ArchonInterface::ArchonInterface *****************************/


  /***** Camera::ArchonInterface::controller_cmd ******************************/
  /**
   * @brief      dispatcher for Archon-specific commands
   * @details    This allows dispatching Archon controller specific commands by
   *             receiving the commands and args and calling the appropriate
   *             controller-specific function.
   * @param[in]  cmd        command
   * @param[in]  args       argument list
   * @param[out] retstring  return string
   * @return     ERROR|NO_ERROR|HELP
   *
   */
  long ArchonInterface::controller_cmd(const std::string &cmd,
                                       const std::string &args,
                                       std::string &retstring) {
    if ( cmd == CAMERAD_LOADTIMING ) {
      return this->load_timing(args, retstring);
    }
    else
    if ( cmd == CAMERAD_READACF ) {
      return this->read_acf(args);
    }
    else
    if ( cmd == "getp" ) {
      return this->get_parameter(args, retstring);
    }
    else
    if ( cmd == "setp" ) {
      return this->set_parameter(args, retstring);
    }
    else
    if ( cmd == CAMERAD_MODE ) {
      return this->set_camera_mode(args, retstring);
    }
    else {
      retstring="unrecognized command";
      return ERROR;
    }
  }
  /***** Camera::ArchonInterface::controller_cmd ******************************/


  /***** Camera::ArchonInterface::configure_interface *************************/
  /**
   * @brief      extract+apply interface-specific parameters from config file
   * @throws     std::runtime_error
   *
   */
  void ArchonInterface::configure_interface() {
    const std::string function("Camera::ArchonInterface::configure_interface");
    logwrite(function, "");
  }
  /***** Camera::ArchonInterface::configure_interface *************************/


  /***** Camera::ArchonInterface::abort ***************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::abort( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::abort");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::abort ***************************************/


  /***** Camera::ArchonInterface::autodir *************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::autodir( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::autodir");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::autodir *************************************/


  /***** Camera::ArchonInterface::basename ************************************/
  /**
   * @brief      set or get the image basename
   * @param[in]  args       requested base name
   * @param[out] retstring  current base name
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::basename( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::basename");
    long error=NO_ERROR;

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BASENAME;
      retstring.append( " [ <name> ]\n" );
      retstring.append( "  set or get image basename\n" );
      return HELP;
    }

    // Base name cannot contain a "/" because that would be a subdirectory,
    // and subdirectories are not checked here, only by imdir command.
    //
    if ( args.find('/') != std::string::npos ) {
      logwrite( function, "ERROR basename cannot contain '/' character" );
      error = ERROR;
    }
    // if name is supplied the set the image name
    else if ( !args.empty() ) {
      this->camera_info.base_name = args;
      error = NO_ERROR;
    }

    // In any case, log and return the current value.
    //
    logwrite(function, "base name is "+std::string(this->camera_info.base_name));
    retstring = this->camera_info.base_name;

    return error;
  }
  /***** Camera::ArchonInterface::basename ************************************/


  /***** Camera::ArchonInterface::bias ****************************************/
  /**
   * @brief      set a bias voltage
   * @param[in]  args       contains <mod> <chan> <volts>
   * @param[out] retstring  
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::bias( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bias");
    long error;

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_BIAS;
      retstring.append( " <mod> <chan> [ <volts> ]\n" );
      retstring.append( "  set or optionally get a bias voltage\n" );
      return HELP;
    }

    std::vector<std::string> tokens;
    Tokenize(args, tokens, " ");

    try {
      int mod, chan;
      float volts;
      size_t ntok = tokens.size();
      bool should_write=false;

      if (ntok != 2 && ntok != 3) {
        throw std::runtime_error("expected <mod> <chan> [ <volts> ]");
      }

      mod  = std::stoi(tokens.at(0));
      chan = std::stoi(tokens.at(1));

      if (ntok==3) {
        volts = std::stof(tokens.at(2));
        should_write = true;
      }

      this->controller->bias(mod, chan, volts, should_write);
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(3) << volts;
      retstring=oss.str();
      error=NO_ERROR;
    }
    catch (const std::exception &e) {
      retstring=std::string(e.what());
      error=ERROR;
    }

    logwrite(function, retstring);
    return error;
  }
  /***** Camera::ArchonInterface::bias ****************************************/


  /***** Camera::ArchonInterface::bin *****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::bin( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::bin");
    logwrite(function, "not yet implemented");
    return ERROR;
  }
  /***** Camera::ArchonInterface::bin *****************************************/


  /***** Camera::ArchonInterface::connect_controller **************************/
  /**
   * @brief      connect to Archon controller
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::connect_controller( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::connect_controller");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_OPEN;
      retstring.append( "\n" );
      retstring.append( "  open connection to Archon controller\n" );
      return HELP;
    }

    // connect to the controller
    try {
      this->controller->connect();
    }
    catch (const std::exception &e) {
      logwrite(function, "ERROR: "+std::string(e.what()));
      return ERROR;
    }

    logwrite(function, "connected");

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::connect_controller **************************/


  /***** Camera::ArchonInterface::disconnect_controller ***********************/
  /**
   * @brief      general description
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       not used
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     HELP|ERROR|NO_ERROR
   *
   */
  long ArchonInterface::disconnect_controller(const std::string args, std::string &retstring) {
    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_CLOSE;
      retstring.append( " \n" );
      retstring.append( "  Closes host connection to Archon controller.\n" );
      return HELP;
    }
    return disconnect_controller();
  }
  /***** Camera::ArchonInterface::disconnect_controller ***********************/
  /**
   * @brief      specialized version closes connection to Archon
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::disconnect_controller() {
    const std::string function("Camera::ArchonInterface::disconnect_controller");
    long error = controller->archon.Close();
    if (error == NO_ERROR) {
      logwrite(function, "Archon connection terminated");
    }
    else {
      logwrite( function, "ERROR disconnecting Archon" );
    }
    return error;
  }
  /***** Camera::ArchonInterface::disconnect_controller ***********************/


  /***** Camera::ArchonInterface::exptime *************************************/
  /**
   * @brief      set/get the exposure time
   * @details    exposure time is in the units set by longexposure() or the
   *             unit can be optionally set here
   * @param[in]  exptime_in  "<time> [ s | ms ]" exposure time in current units
   * @param[out] retstring   return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::exptime( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::exptime");
    std::stringstream message;
    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_EXPTIME;
      retstring.append( " [ <exptime> ]\n" );
      retstring.append( "  get or optionally set the exposure time in floating point units of sec\n" );
      return HELP;
    }

    // If an arg was supplied then use it to try to set the exptime
    if (!args.empty()) {
      try {
        this->set_exptime(std::stod(args));
      }
      catch (const std::exception &e) {
        retstring=std::string(e.what());
        logwrite(function, "ERROR: "+retstring);
        return ERROR;
      }
    }

    // read the exptime from the class
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << this->controller->exposure_time->get();
    retstring = oss.str();

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::exptime *************************************/


  /***** Camera::ArchonInterface::set_exptime *********************************/
  /**
   * @brief      set the exposure time
   * @details    This function is used internally and is what actually sets
   *             the exposure time.
   * @param[in]  exptime  double-precision exposure time in seconds
   * @throws     std::exception
   *
   */
  void ArchonInterface::set_exptime(double exptime) {
    try {
      this->controller->set_exptime(exptime);
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Camera::ArchonInterface::set_exptime *********************************/


  /***** Camera::ArchonInterface::expose **************************************/
  /**
   * @brief      initiates an exposure sequence
   * @details    
   * @param[in]  args       optionally contains number of repeats
   * @param[out] retstring  optional return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::expose( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::expose");

    // Help
    //
    if (args=="?" || args=="help") {
      retstring = CAMERAD_EXPOSE;
      retstring.append( " <tbd>\n" );
      retstring.append( "  TBD\n" );
      return HELP;
    }

    int nseq=1;

    if (!args.empty()) {
      try { nseq = std::stoi(args);
      }
      catch (const std::exception &e) {
        retstring="ERROR reading nseq: "+std::string(e.what());
        logwrite(function, retstring);
        return ERROR;
      }
    }

    int nseq_remaining = nseq;

    while (nseq_remaining-- > 0) {
      do_expose();
    }

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::expose **************************************/


  /***** Camera::ArchonInterface::exposure_mode *******************************/
  /**
   * @brief      interface function to set/get the exposure mode
   * @param[in]  args       requested mode
   * @param[out] retstring  contains current mode
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::exposure_mode( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::exposure_mode");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_EXPOSUREMODE;
      retstring.append( " [ <mode> ]\n" );
      retstring.append( "  Set or get current exposure mode.\n" );
      retstring.append( "  Valid modes are: {" );

      auto modes = this->get_exposure_modes();
      for (const auto &mode : modes) { retstring.append(" "); retstring.append(mode); }

      retstring.append( " }\n" );
      return HELP;
    }

    // not specified is request to return current exposure mode
    if (args.empty()) {
      if ( !this->is_exposuremode_set() ) {
        logwrite(function, "ERROR exposure mode not set!");
        retstring="undefined";
        return ERROR;
      }
      retstring=this->exposuremode->get_type();
      return NO_ERROR;
    }

    // otherwise something was specified so try to set exposure mode
    long error=set_exposure_mode(args);

    // always return current mode
    if ( !this->is_exposuremode_set() ) {
      logwrite(function, "ERROR exposure mode not set!");
      retstring="undefined";
      return ERROR;
    }
    retstring=this->exposuremode->get_type();

    logwrite(function, retstring);

    return error;
  }
  /***** Camera::ArchonInterface::exposure_mode *******************************/


  /***** Camera::ArchonInterface::get_exposure_modes **************************/
  /**
   * @brief      return a vector of strings of recognized exposure modes
   * @return     vector<string>
   *
   */
  std::vector<std::string> ArchonInterface::get_exposure_modes() {
    std::vector<std::string> modes;
    for (const auto &mode : Camera::ArchonExposureMode::ALLMODES) { modes.push_back(mode); }
    return modes;
  }
  /***** Camera::ArchonInterface::get_exposure_modes **************************/


  /***** Camera::ArchonInterface::set_exposure_mode ***************************/
  /**
   * @brief      actually sets the exposure mode
   * @details    This creates the appropriate exposure mode object for the
   *             requested exposure mode, providing access to that mode's functions.
   * @param[in]  modein  string representing the exposure mode
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_exposure_mode(const std::string &modein) {

    if (modein==ArchonExposureMode::RAW) {
      this->exposuremode = std::make_shared<ExposureModeRaw>(this);
    }
    else
    if (modein==ArchonExposureMode::SINGLE) {
      this->exposuremode = std::make_shared<ExposureModeSingle>(this);
    }
    else
    if (modein==ArchonExposureMode::RXRV) {
      this->exposuremode = std::make_shared<ExposureModeRXRV>(this);
    }
    else {
      logwrite("Camera::ArchonInterface::set_exposure_mode",
               "ERROR unrecognized exposure mode \""+modein+"\"");
      return ERROR;
    }
    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::set_exposure_mode ***************************/


  /***** Camera::ArchonInterface::get_parameter *******************************/
  /**
   * @brief      get an Archon parameter value from Configuration
   * @param[in]  args       expected string "<parametername>"
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::get_parameter(const std::string &args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::get_parameter");

    // Help
    if (args=="?" || args=="help") {
      retstring = "getp";
      retstring.append( " <name>\n" );
      retstring.append( "  Reads Archon parameter <name> from configuration memory.\n" );
      return HELP;
    }

    // args should contain only a single word, the parameter name
    if (args.find_first_of(" \t\n\r") != std::string::npos) {
      logwrite(function, "ERROR expected <name>");
      return ERROR;
    }

    // get the parameter value from the controller
    try {
      retstring = this->controller->get_parameter<std::string>(args);
      return NO_ERROR;
    }
    catch (const std::exception &e) {
      logwrite(function, std::string(e.what()));
      return ERROR;
    }
  }
  /***** Camera::ArchonInterface::get_parameter *******************************/


  /***** Camera::ArchonInterface::read_acf ************************************/
  /**
   * @brief      loads the ACF file into host only
   * @details    This is a wrapper to provide outside access to load the ACF
   *             file with write_to_archon = false.
   * @param[in]  filename  (optional) fully qualified path to ACF file. Pull from
   *                       config file if not supplied.
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::read_acf(const std::string &filename) {
    return( this->controller->load_acf(filename, false) );
  }
  /***** Camera::ArchonInterface::read_acf ************************************/


  /***** Camera::ArchonInterface::load_firmware *******************************/
  /**
   * @brief      general description of load firmware
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       fully qualified path of ACF
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::load_firmware(const std::string &args, std::string &retstring) {
    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_LOAD;
      retstring.append( " <acf-file>\n" );
      retstring.append( "  Loads the ACF file and applies the complete Archon configuration.\n" );
      retstring.append( "  Archon power will be off after this operation.\n" );
      return HELP;
    }
    // call the work function
    return load_firmware(args);
  }
  /***** Camera::ArchonInterface::load_firmware *******************************/
  /**
   * @brief      loads the ACF file and applies the complete Archon Configuration
   * @param[in]  args       fully qualified path of ACF
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::load_firmware(const std::string &acffile) {
    // load the ACF file and write to Archon configuration memory
    //
    long error = this->controller->load_acf(acffile);

    // Parse and apply the complete system configuration from configuration memory.
    // Detector power will be off after this.
    //
    if (error == NO_ERROR) error = this->controller->send_cmd(APPLYALL);

    // read/clear Archon's internal error log
    //
    if (error != NO_ERROR) this->controller->fetchlog();

    // set the mode to DEFAULT
    //
    if (error == NO_ERROR) error = this->set_camera_mode(std::string("DEFAULT"));

    // TODO set exptime
    // TODO other instrument-specific defaults?

    return error;
  }
  /***** Camera::ArchonInterface::load_firmware *******************************/


  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @details    This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param      acffile, specified ACF to load
   * @param      retstring, reference to string for return values
   * @return     ERROR | NO_ERROR | HELP
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing( const std::string args, std::string &retstring ) {
    // Help
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_LOADTIMING;
      retstring.append( " <timing.acf>\n" );
      retstring.append( "  Loads <timing.acf> file into Archon, then sends the LOADTIMING command\n" );
      retstring.append( "  which parses and compiles only the timing script and parameters.\n" );
      return HELP;
    }
    // call the work function
    return( this->load_timing(args) );
  }
  /***** Camera::ArchonInterface::load_timing *********************************/
  /**
   * @brief      loads the ACF file and applies the timing script and parameters only
   * @details    This version calls the controller class functions to do the load.
   * @param      acffile, specified ACF to load
   * @return     ERROR | NO_ERROR
   *
   * This function is overloaded.
   *
   * This function loads the ACF file then sends the LOADTIMING command
   * which parses and compiles only the timing script and parameters.
   *
   */
  long ArchonInterface::load_timing(const std::string &filename) {
    // load the ACF file into configuration memory
    //
    long error = this->controller->load_acf( filename );

    // parse timing script and parameters and apply them to the system
    //
    if (error == NO_ERROR) error = this->controller->send_cmd(LOADTIMING);

    return error;
  }
  /***** Camera::ArchonInterface::load_timing *********************************/


  /***** Camera::ArchonInterface::set_camera_mode *****************************/
  /**
   * @brief      set a camera "mode"
   * @details    Allowable camera modes are defined in the ACF file and specify
   *             a set of camera settings which can be used to change camera
   *             settings.
   *             This version fits the general description of all virtual functions
   *             inherited by the Camera::Interface base class and is overridden
   *             by an Archon-specific implementation.
   * @param[in]  args       requested mode name (case-insensitive)
   * @param[out  retstring  current mode name
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_camera_mode(std::string args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::set_camera_mode");
    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_MODE;
      retstring.append( " <name>\n" );
      retstring.append( "  Applies camera settings associated with MODE_<name> specified in the ACF file.\n");
      return HELP;
    }
    // call the work function
    return( this->set_camera_mode(args) );
  }
  /***** Camera::ArchonInterface::set_camera_mode *****************************/
  /**
   * @brief      set a camera "mode"
   * @details    This version is for internal use.
   * @param[in]  modeselect  requested mode name
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_camera_mode(std::string modeselect) {
    const std::string function("Camera::ArchonInterface::set_camera_mode");

    // cannot changes while exposure in progress

    // firmware must be loaded first
    if (!this->controller->is_firmwareloaded) {
      logwrite(function, "ERROR no firmware loaded");
      return ERROR;
    }

    to_uppercase(modeselect);

    // requested mode must have been read from the current ACF
    // and put into modemap
    if (this->controller->modemap.find(modeselect) == this->controller->modemap.end()) {
      logwrite(function, "ERROR undefined mode "+modeselect+" in ACF "+this->controller->firmware);
      return ERROR;
    }

    // load mode settings from .acf and apply to Archon
    if (this->controller->load_mode_settings(modeselect) != NO_ERROR) {
      return ERROR;
    }

    this->controller->set_image_geometry(modeselect);

    return ERROR;
  }
  /***** Camera::ArchonInterface::set_camera_mode *****************************/


  /***** Camera::ArchonInterface::set_parameter *******************************/
  /**
   * @brief      write Archon parameter to configuration memory
   * @param[in]  args       expected string "<parametername> <value>"
   * @param[out] retstring  contains help on request, otherwise not used
   * @return     ERROR|NO_ERROR
   *
   */
  long ArchonInterface::set_parameter(const std::string &args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::set_parameter");

    // Help
    if (args=="?" || args=="help") {
      retstring = "setp";
      retstring.append( " <name> <value>\n" );
      retstring.append( "  Sets Archon parameter <name> to <value>.\n" );
      retstring.append( "  <value> must be in range {0:1048575}.\n" );
      return HELP;
    }

    // tokenize args, expect two: <name> <value>
    std::vector<std::string> tokens;
    Tokenize(args, tokens, " ");

    if (tokens.size() != 2) {
      logwrite(function, "ERROR expected <name> <value>");
      return ERROR;
    }

    // set the parameter value on the controller
    try {
      long value = std::stol(tokens[1]);
      return( this->controller->set_parameter(tokens[0], value) );
    }
    catch (const std::exception &e) {
      logwrite(function, std::string(e.what()));
      return ERROR;
    }
  }
  /***** Camera::ArchonInterface::set_parameter *******************************/


  /***** Camera::ArchonInterface::native **************************************/
  /**
   * @brief      send native commands directly to Archon and log result
   * @details    sends the command directly to Archon but does not parse any
   *             reply. @TODO publish the reply?
   * @param[in]  args       command to send to Archon Controller
   * @param[out] retstring  reply from Archon
   * @return     ERROR|NO_ERROR|BUSY, return from controller->send_cmd() call
   *
   */
  long ArchonInterface::native( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::native");

    // Help
    if (args.empty() || args=="?" || args=="help") {
      retstring = CAMERAD_NATIVE;
      retstring.append( " <cmd>\n" );
      retstring.append( "  Sends <cmd> directly to Archon without parsing the reply, other than\n" );
      retstring.append( "  to confirm that it did reply.\n" );
      return HELP;
    }
    // call the work function
    long error = this->controller->send_cmd(args, retstring);

    if (!retstring.empty()) {
      logwrite(function, retstring);
      // TODO publish the reply?
    }

    return error;
  }
  /***** Camera::ArchonInterface::native **************************************/


  /***** Camera::ArchonInterface::power ***************************************/
  /**
   * @brief      turn on controller bias power supplies
   * @param[in]  args       requested state or help, on|off|?
   * @param[out] retstring  contains power_status string on success
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long ArchonInterface::power(const std::string args, std::string &retstring) {
    const std::string function("Camera::ArchonInterface::power");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_POWER;
      retstring.append( " [ on | off ]\n" );
      retstring.append( "  Turn on|off Archon bias power supplies. If no arg supplied then\n" );
      retstring.append( "  return current state.\n" );
      return HELP;
    }

    // parse the requested state
    if ( !args.empty() ) {
      int state=0;
      if ( caseCompareString(args, "on") )  state=1;
      else
      if ( caseCompareString(args, "off") ) state=0;
      else {
        logwrite(function, "ERROR expected {ON|OFF}");
        return ERROR;
      }
      // set the requested Archon power state
      if (this->controller->set_power(state) != NO_ERROR) {
        logwrite( function, "ERROR setting Archon power "+args);
        return ERROR;
      }
    }

    // read the power status from the controller
    long error = this->controller->get_power(retstring);

    logwrite(function, retstring);

    return error;
  }
  /***** Camera::ArchonInterface::power ***************************************/


  /***** Camera::ArchonInterface::test ****************************************/
  /**
   * @brief
   * @param[in]  args
   * @param[out] retstring
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::test( const std::string args, std::string &retstring ) {
    const std::string function("Camera::ArchonInterface::test");

    if (!this->exposuremode) {
      logwrite(function, "ERROR exposure mode undefined!");
      return ERROR;
    }

    logwrite(function, "calling exposuremode->expose() for mode"+this->exposuremode->get_type());

    this->exposuremode->test();
    this->exposuremode->expose();

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::test ****************************************/


  /***** Camera::ArchonInterface::allocate_framebuf ***************************/
  /**
   * @brief      allocate memory for frame buffer
   * @details    wraps Camera::Controller::allocate_framebuf
   * @param[in]  reqsz  requested frame buffer size in bytes
   * @return     ERROR | NO_ERROR
   *
   */
  long ArchonInterface::allocate_framebuf(uint32_t reqsz) {
    return controller->allocate_framebuf(reqsz);
  }
  /***** Camera::ArchonInterface::allocate_framebuf ***************************/


  /***** Camera::ArchonInterface::read_frame **********************************/
  /**
   * what was this for?
   */
//long ArchonInterface::read_frame() {
//  controller->read_frame(Camera::ArchonController::FRAME_IMAGE);
//  return NO_ERROR;
//}
  /***** Camera::ArchonInterface::read_frame **********************************/


  /***** Camera::ArchonInterface::do_expose ***********************************/
  /**
   *
   */
  long ArchonInterface::do_expose() {
    const std::string function("Camera::ArchonInterface::do_expose");
    long error=NO_ERROR;

    logwrite(function, "");

    if (!this->is_exposuremode_set()) {
      logwrite(function, "ERROR exposure mode not set!");
      return ERROR;
    }

    // spawn two threads, a producer and a consumer
    //
    // The producer triggers the exposure and collect images into a FIFO queue.
    // The consumer pops images out of the queue for processing.
    //
    std::thread producer(&ExposureMode::image_acquisition_thread, this->exposuremode.get());
    std::thread consumer(&ExposureMode::image_processing_thread, this->exposuremode.get());

    producer.join();
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      is_producer_finished=true;
      error |= (is_producer_error ? ERROR : NO_ERROR);
    }

    consumer.join();

    return NO_ERROR;
  }
  /***** Camera::ArchonInterface::do_expose ***********************************/


  /***** Camera::ArchonInterface::image_acquisition_thread ********************/
  /**
   * @brief      triggers exposure, collects and pushes frames into a queue
   * @details    This is run in the producer thread.
   * @param[in]  nexp
   *
   */
  void ArchonInterface::image_acquisition_thread() {
    /***
     * MOVE THIS TO EXPOSURE MODE
     */
  }
  /***** Camera::ArchonInterface::image_acquisition_thread ********************/


  /***** Camera::ArchonInterface::image_processing_thread *********************/
  /**
   *
   */
  void ArchonInterface::image_processing_thread() {
    /***
     * MOVE THIS TO EXPOSURE MODE
     */
  }
  /***** Camera::ArchonInterface::image_processing_thread *********************/

}
