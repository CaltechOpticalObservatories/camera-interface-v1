/**
 * @file    deimos.cpp
 * @brief   instrument specific code for DEIMOS
 *
 */

#include "archon.h"
#include "common.h"
#include "utilities.h"
#include "logentry.h"
#include "camerad_commands.h"

namespace Archon {

  /***** Archon::Interface::region_of_interest ********************************/
  /**
   * @brief      not supported
   * @param[in]  args
   * @param[out] restring
   * @return     ERROR
   *
   */
  long Interface::region_of_interest(std::string args, std::string &retstring) {
    camera.log_error("Archon::Interface::region_of_interest", "ROI not supported");
    retstring="not_supported";
    return ERROR;
  }
  /***** Archon::Interface::region_of_interest ********************************/


  /***** Archon::Interface::fcs_exptime ***************************************/
  /**
   * @brief      set/get FCS exposure time
   * @details    This function is used by the server.
   * @param[in]  args      requested exposure time, or empty or help
   * @param[out] restring  return string holds exposure time
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::fcs_exptime(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::fcs_exptime");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_FCS_EXPTIME;
      retstring.append(" [ <exptime> ]\n");
      retstring.append("  set/get FCS exposure time, in units of floating point seconds\n");
      return HELP;
    }

    // If an arg was supplied then use it to try to set the exptime
    if (!args.empty()) {
      try {
        this->set_fcs_exptime(std::stod(args));
      }
      catch (const std::exception &e) {
        retstring=std::string(e.what());
        camera.log_error(function, retstring);
        return ERROR;
      }
    }

    // read the exptime from the class
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << this->fcs_info.exposure_time.get();
    retstring = oss.str();

    return NO_ERROR;
  }
  /***** Archon::Interface::fcs_exptime ***************************************/


  /***** Archon::Interface::set_fcs_exptime ***********************************/
  /**
   * @brief      set FCS exposure time
   * @details    This function is used internally, and is what actually sets
   *             the exposure time.
   * @param[in]  exptime  double-precision exposure time in seconds
   * @throws     std::exception
   *
   */
  void Interface::set_fcs_exptime(double exptime) {
    try {
      this->set_exptime(exptime,
                        this->fcs_exptime_sec_param,
                        this->fcs_exptime_msec_param,
                        this->fcs_info);
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Archon::Interface::set_fcs_exptime ***********************************/


  /***** Archon::Interface::fcs_expose ****************************************/
  /**
   * @brief      start FCS exposure
   * @param[in]  args      args
   * @param[out] restring  return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::fcs_expose(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::fcs_expose");
    char message[256];
    long error=NO_ERROR;

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_FCS_EXPOSE;
      retstring.append("\n");
      retstring.append("  Perform a complete FCS exposure: trigger exposure, wait for exposure\n");
      retstring.append("  delay, clock the FCS detector into an Archon buffer, transmit that\n");
      retstring.append("  buffer to the host computer.\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    // FCS exposure trigger parameter must be defined
    if (fcs_start_param.empty()) {
      camera.log_error(function, "FCS exposure trigger parameter not defined");
      retstring="missing_config";
      return ERROR;
    }

    // set the mode if needed
    if (!caseCompareString(this->camera_info.current_observing_mode, mode_fcs)) {
      this->camera_info = this->fcs_info;
      if ( set_camera_mode(mode_fcs) != NO_ERROR ) {
        camera.log_error(function, "setting mode '"+mode_fcs+"'");
        return ERROR;
      }
    }

    this->clear_abortstate();

    // get system time and Archon's timer after exposure starts,
    // and assemble FITS filename
    this->camera_info.start_time = get_timestamp();                 // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss
    this->camera.set_fitstime(this->camera_info.start_time);        // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename

    if ( this->camera.get_fitsname(this->camera_info.fits_name) != NO_ERROR ) {
      camera.log_error(function, "validating FITS filename");
      return ERROR;
    }

    this->add_filename_key();                                       // add filename to system keys database

    this->camera_info.systemkeys.keydb = this->systemkeys.keydb;    // copy the systemkeys database object into camera_info
    if (this->camera.writekeys_when=="before") this->copy_keydb();  // copy the ACF and userkeys database into camera_info

    // open guarded FITS file (automatically closes on exit)
    FitsFileGuard guarded_fits(this->fits_file, this->camera_info, true);

    if (guarded_fits.open() != NO_ERROR) {
      camera.log_error(function, "opening FITS file");
      return ERROR;
    }

    // start FCS exposure by setting FCS exposure parameter = 1
    if ( (set_parameter(fcs_start_param, 1) != NO_ERROR) ) {
      camera.log_error(function, "setting Archon parameter");
      return ERROR;
    }

    // Archon internal timer (one tick=10 nsec)
    if (this->get_timer(&this->archon_timer_start) != NO_ERROR) {
      camera.log_error(function, "could not get start time");
      return ERROR;
    }

    logwrite(function, "exposure started");

    // wait for exposure delay
    if ( error==NO_ERROR && (error=this->wait_for_exposure(this->fcs_info.exposure_time.get())) != NO_ERROR ) {
      camera.log_error(function, "waiting for exposure");
    }

    // poll for an Archon frame buffer to be ready and record the time
    if ( error==NO_ERROR && (error=this->wait_for_readout())==ERROR ) {
      camera.log_error(function, "waiting for readout");
    }

    // read frame
    if ( (error=this->read_frame(Camera::FRAME_IMAGE)) != NO_ERROR ) {
      camera.log_error(function, "reading frame buffer");
    }

    // write frame
    if (error==NO_ERROR && (error = this->write_fcs()) != NO_ERROR) {
      camera.log_error(function, "writing image frame");
    }

    // ASYNC status message on completion of each file
    SNPRINTF(message, "FILE:%s %s", this->camera_info.fits_name.c_str(), (error==NO_ERROR ? "COMPLETE" : "ERROR"));
    this->camera.async.enqueue( std::string(message) );
    logwrite( function, std::string(message) );

    logwrite(function, "done");

    return error;
  }
  /***** Archon::Interface::fcs_expose ****************************************/


  /***** Archon::Interface::sci_exptime ***************************************/
  /**
   * @brief      set/get FCS exposure time
   * @param[in]  args      requested exposure time, or empty or help
   * @param[out] restring  return string holds exposure time
   * @return     ERROR | NO_ERROR | HELP
   * @throws     std::exception
   *
   */
  long Interface::sci_exptime(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sci_exptime");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_EXPTIME;
      retstring.append(" [ <exptime> ]\n");
      retstring.append("  set/get SCI exposure time, in units of floating point seconds.\n");
      retstring.append("  Used for lab testing with "+CAMERAD_SCI_EXPOSE+" command, not expected\n");
      retstring.append("  to be used for deployed instrument.\n");
      return HELP;
    }

    // If an arg was supplied then use it to try to set the exptime
    if (!args.empty()) {
      try {
        this->set_sci_exptime(std::stod(args));
      }
      catch (const std::exception &e) {
        retstring=std::string(e.what());
        camera.log_error(function, retstring);
        return ERROR;
      }
    }

    // read the exptime from the class
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << this->sci_info.exposure_time.get();
    retstring = oss.str();

    return NO_ERROR;
  }
  /***** Archon::Interface::sci_exptime ***************************************/


  /***** Archon::Interface::set_sci_exptime ***********************************/
  /**
   * @brief      set SCI exposure time
   * @details    This function is used internally, and is what actually sets
   *             the exposure time.
   * @param[in]  exptime  double-precision exposure time in seconds
   * @throws     std::exception
   *
   */
  void Interface::set_sci_exptime(double exptime) {
    try {
      this->set_exptime(exptime,
                        this->sci_exptime_sec_param,
                        this->sci_exptime_msec_param,
                        this->sci_info);
    }
    catch (const std::exception &e) {
      throw;
    }
  }
  /***** Archon::Interface::set_sci_exptime ***********************************/


  /***** Archon::Interface::sci_expose ****************************************/
  /**
   * @brief      performs a complete science exposure
   * @details    This is for lab use, not expected to be used by deployed
   *             instrument. This triggers a science exposure, waits for an
   *             exposure delay, clocks the detector into an Archon buffer and
   *             transmits that buffer to host.
   * @param[in]  args      optional number of exposures
   * @param[out] restring  return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::sci_expose(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sci_expose");
    long error=NO_ERROR;

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_EXPOSE;
      retstring.append("\n");
      retstring.append("  Triggers science exposure, waits for exposure delay, transmits\n");
      retstring.append("  image to host. Not expected to be used by deployed instrument.\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    // science exposure trigger parameter must be defined
    if (sci_start_param.empty()) {
      camera.log_error(function, "science exposure trigger parameter not defined");
      retstring="missing_config";
      return ERROR;
    }

    // set the mode if needed
    if (!caseCompareString(this->camera_info.current_observing_mode, mode_science)) {
      this->camera_info = this->sci_info;
      if ( set_camera_mode(mode_science) != NO_ERROR ) {
        camera.log_error(function, "could not set mode '"+mode_science+"'");
        return ERROR;
      }
    }

    this->clear_abortstate();

    // start SCI exposure by setting science exposure parameter = 1
    if ( (set_parameter(sci_start_param, 1) != NO_ERROR) ) {
      camera.log_error(function, "could not set Archon parameter");
      return ERROR;
    }

    // Archon internal timer (one tick=10 nsec)
    if (this->get_timer(&this->archon_timer_start) != NO_ERROR) {
      camera.log_error(function, "could not get start time");
      return ERROR;
    }

    logwrite(function, "exposure started");

    return error;
  }
  /***** Archon::Interface::sci_expose ****************************************/


  /***** Archon::Interface::sci_start *****************************************/
  /**
   * @brief      start science exposure
   * @details    Sets a parameter to trigger a branch in the ACF which stops
   *             the SCI detector flush/idle so that integration on the SCI
   *             detector effectively begins.
   * @param[in]  args      optional number of exposures
   * @param[out] restring  return string
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::sci_start(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sci_start");

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_START;
      retstring.append("\n");
      retstring.append("  stops SCI detector idle, which begins integration. This only starts\n");
      retstring.append("  an exposure, it does not stop or read out.\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    // science exposure trigger parameter must be defined
    if (sci_start_param.empty()) {
      camera.log_error(function, "science exposure trigger parameter not defined");
      retstring="missing_config";
      return ERROR;
    }

    // start SCI exposure by setting science exposure parameter = 1
    if ( (set_parameter(sci_start_param, 1) != NO_ERROR) ) {
      camera.log_error(function, "could not set Archon parameter");
      return ERROR;
    }

    // get system time and Archon's timer after exposure starts
    this->sci_info.start_time = get_timestamp();                    // current system time formatted as YYYY-MM-DDTHH:MM:SS.sss

    logwrite(function, "sci exposure started");

    return NO_ERROR;
  }
  /***** Archon::Interface::sci_start *****************************************/


  /***** Archon::Interface::sci_readout ***************************************/
  /**
   * @brief      readout science detector
   * @details    Configures taps and CDS parameters for the science detector,
   *             then sets a parameter to trigger a branch in the ACF which
   *             starts clocking the SCI detector into an Archon buffer,
   *             effectively ending the science integration, then transmits
   *             that buffer to the host computer.
   * @param[in]  args
   * @param[out] restring
   * @return     ERROR | NO_ERROR | HELP
   *
   */
  long Interface::sci_readout(std::string args, std::string &retstring) {
    const std::string function("Archon::Interface::sci_readout");
    char message[256];
    long error=NO_ERROR;

    // Help
    if (args=="?" || args=="help") {
      retstring = CAMERAD_SCI_READOUT;
      retstring.append("\n");
      retstring.append("  reads SCI detector into Archon buffer then transmits buffer to host.\n");
      retstring.append("  This ends a science exposure.\n");
      return HELP;
    }

    // Archon connection required
    if (!archon.isconnected()) {
      camera.log_error(function, "connection not open to controller");
      retstring="not_connected";
      return ERROR;
    }

    // set the mode if needed
    if (!caseCompareString(this->camera_info.current_observing_mode, mode_science)) {
      this->camera_info = this->sci_info;
      if ( set_camera_mode(mode_science) != NO_ERROR ) {
        camera.log_error(function, "could not set mode '"+mode_science+"'");
        return ERROR;
      }
    }

    this->camera.set_fitstime(this->sci_info.start_time);        // sets camera.fitstime (YYYYMMDDHHMMSS) used for filename

    if ( this->camera.get_fitsname(this->camera_info.fits_name) != NO_ERROR ) {
      camera.log_error(function, "validating FITS filename");
      return ERROR;
    }

    this->camera.datacube(true);
    this->camera_info.iscube = true;
    this->camera_info.extension=0;

    // open guarded FITS file (closes automatically on exit)
    FitsFileGuard guarded_fits(this->fits_file, this->camera_info, true);

    if (guarded_fits.open() != NO_ERROR) {
      camera.log_error(function, "opening FITS file");
      return ERROR;
    }

    // stop science exposure by setting parameter = 1
    if ( (set_parameter(sci_stop_param, 1) != NO_ERROR) ) {
      camera.log_error(function, "setting science stop parameter");
      return ERROR;
    }

    // poll for an Archon frame buffer to be ready and record the time
    if ( error==NO_ERROR && (error=this->wait_for_readout())==ERROR ) {
      camera.log_error(function, "waiting for exposure");
    }

    // read frame
    if ( (error=this->read_frame(Camera::FRAME_IMAGE)) != NO_ERROR ) {
      camera.log_error(function, "reading frame buffer");
    }

    this->write_science();

    // ASYNC status message on completion of each file
    SNPRINTF(message, "FILE:%s %s", this->camera_info.fits_name.c_str(), (error==NO_ERROR ? "COMPLETE" : "ERROR"));
    this->camera.async.enqueue( std::string(message) );
    logwrite( function, std::string(message) );

    logwrite(function, "done");

    return error;
  }
  /***** Archon::Interface::sci_readout ***************************************/


  long Interface::write_fcs() {
    const std::string function("Archon::Interface::write_fcs");
    long error=NO_ERROR;
    uint32_t *cbuf32 = (uint32_t *)this->image_data;

    if (!cbuf32) {
      logwrite(function, "ERROR invalid image_data buffer");
      return ERROR;
    }

/***
    float *fbuf = nullptr;
    fbuf = new float[this->camera_info.section_size];
    for (long pix=0; pix < this->camera_info.section_size; pix++) {
      fbuf[pix] = (float)(cbuf32[pix]/65536.0);
    }

    error = this->fits_file.write_image(fbuf, this->camera_info);

    if (error != NO_ERROR) {
      camera.log_error(function, "writing image to disk");
    }

    delete [] fbuf;
 ***/

    int num_detect = this->modemap[this->camera_info.current_observing_mode].geometry.num_detect;
    int num_cols   = this->camera_info.axes[0];
    int num_rows   = this->camera_info.axes[1];
    long buf_width = num_cols * num_detect/2;

    if (num_rows==0 || buf_width==0) {
      logwrite(function, "ERROR zero-dimension image");
      return ERROR;
    }
    if (num_detect != 2) {
      logwrite(function, "ERROR expected 2 CCDs");
      return ERROR;
    }

    {
    std::ostringstream oss;
    oss << "[DEBUG] section_size=" << this->camera_info.section_size
        << " image_memory=" << this->camera_info.image_memory
        << " num_detect=" << this->modemap[this->camera_info.current_observing_mode].geometry.num_detect
        << " num_cols=" << num_cols
        << " num_rows=" << num_rows
        << " buf_width=" << buf_width
        << " axes[0]=" << this->camera_info.axes[0]
        << " axes[1]=" << this->camera_info.axes[1];
    logwrite(function, oss.str());
    }

    return error;
  }


  long Interface::write_science() {
    const std::string function("Archon::Interface::write_science");
    long error=NO_ERROR;
    uint32_t *cbuf32 = (uint32_t *)this->image_data;

    if (!cbuf32) {
      logwrite(function, "ERROR invalid image_data buffer");
      return ERROR;
    }

    int num_detect = this->modemap[this->camera_info.current_observing_mode].geometry.num_detect;
    int num_cols   = this->camera_info.axes[0];
    int num_rows   = this->camera_info.axes[1];
    long buf_width = num_cols * num_detect/2;

    if (num_detect==0 || num_cols==0 || num_rows==0) {
      logwrite(function, "ERROR zero-dimension image");
      return ERROR;
    }
    if (num_detect % 2 != 0) {
      logwrite(function, "ERROR expected even number of CCDs");
      return ERROR;
    }

    {
    std::ostringstream oss;
    oss << "[DEBUG] section_size=" << this->camera_info.section_size
        << " image_memory=" << this->camera_info.image_memory
        << " num_detect=" << this->modemap[this->camera_info.current_observing_mode].geometry.num_detect
        << " num_cols=" << num_cols
        << " num_rows=" << num_rows
        << " buf_width=" << buf_width
        << " axes[0]=" << this->camera_info.axes[0]
        << " axes[1]=" << this->camera_info.axes[1];
    logwrite(function, oss.str());
    }

    for (int half=0, dir_half=2; half < 2; half++, dir_half--) {

      // loop through half of all CCDs
      for (long ccd_count=0; ccd_count < num_detect/2; ccd_count++) {  // 0,1,2,3

        // directional counter up, then down
        long dir_count = half == 0 ? ccd_count                         // 0,1,2,3
                                   : num_detect/2 - 1 - ccd_count;     // 3,2,1,0

        // temporary buffer for a single CCD
        auto ccdbuf = std::make_unique<float[]>(this->camera_info.section_size);

        for (long row=0; row < num_rows; row++) {
          for (long col=0; col < num_cols; col++) {

            // this indexes into each CCD
            long pix  = col + (row * num_cols);

            // this indexes into the Archon buffer
            long cbufpix = col + (dir_count * num_cols) + (row * buf_width) + (half * num_rows * buf_width);

            // copy each CCD out of main image buffer into temp buffer,
            // scaling and converting to float
            ccdbuf[pix] = (float)(cbuf32[cbufpix]/65536.0);
          }
        }

        this->camera_info.extension = ccd_count + half * (num_detect/2);  // 0-indexed

        long ccdnum = this->camera_info.extension+1;                      // 1-indexed

        std::ostringstream oss;
        oss << "DETSEC=[" << (dir_count*num_cols)+1 << ":" << (dir_count+1)*num_cols
                          << ","
                          << (dir_half-1)*num_rows+1 << ":" << (dir_half)*num_rows
                          << "]";
        this->systemkeys.addkey(oss.str());  // not getting to extensions!

        error = this->fits_file.write_image(ccdbuf.get(), this->camera_info);

        if (error!=NO_ERROR) {
          logwrite(function, "ERROR writing CCD "+std::to_string(ccdnum));
          break;
        }
      }
    }

    return error;
  }
}
