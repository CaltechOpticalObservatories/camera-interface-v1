/**
 * @file    nirc2.cpp
 * @brief   instrument-specific class definitions for NIRC2
 * @details This file defines functions specific to NIRC2
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 * Any functions defined here must have (generic) function definitions in archon.h.
 * This file contains the specific code for those function definitions which are
 * unique to the particular instrument.
 *
 */

#include <string>
#include <sstream>
#include <vector>

#include "archon.h"
#include "common.h"
#include "utilities.h"
#include "logentry.h"

namespace Archon {

      /***** PythonProc::PythonProc *******************************************/
      /**
       * @brief      
       *
       */
/***
      PythonProc::PythonProc() {
        this->pName   = PyUnicode_FromString( "calcmcds" );
        this->pModule = PyImport_Import( this->pName );
        logwrite( "PythonProc::PythonProc", "constructed" );
      }
***/
      /***** PythonProc::PythonProc *******************************************/


      /***** PythonProc::~PythonProc ******************************************/
      /**
       * @brief      
       *
       */
/***
      PythonProc::~PythonProc() {
        logwrite( "PythonProc::~PythonProc", "de-constructed" );
      }
***/
      /***** PythonProc::~PythonProc ******************************************/


//    PythonProc pp;


      /***** Archon::Interface::power *****************************************/
      /**
       * @brief      wrapper for Archon::Interface::do_power()
       * @details    NIRC2 requires setting a parameter to start the clocks after
       *             turning on the power and that is done here.
       * @param[in]  state_in  requested power state
       * @param[out] restring  return string holds power state
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::power( std::string state_in, std::string &retstring ) {
        std::string function = "Archon::Instrument::power";
        std::stringstream message;

        // First use Archon::Interface::do_power() to set/get the power
        //
        long error = this->do_power( state_in, retstring );

        // After turning on the power NIRC2 needs to set this parameter to start the clocks,
        // which we'll do only if the power was turned on successfully.
        //
        if ( error==NO_ERROR && !state_in.empty() ) {               // received something
          std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
          if ( state_in == "ON" ) {
            error = this->set_parameter( "Start", 1 );              // needed to start NIRC2 firmware clocks
            if ( error != NO_ERROR ) this->camera.log_error( function, "starting clocks" );
            else
            logwrite( function, "clocks started" );
          }
        }

        return( error );
      }
      /***** Archon::Interface::power *****************************************/


      /***** Archon::Interface::expose ****************************************/
      /**
       * @brief      wrapper for Archon::Interface::do_expose()
       * @param[in]  nseq_in  string which can (optionally) contain number of sequences
       * @return     ERROR or NO_ERROR
       *
       */
      long Interface::expose( std::string nseq_in ) {
        std::string function = "Archon::Instrument::expose";
        std::stringstream message;

        // Cannot start another exposure while currently exposing
        //
        if ( this->camera.is_exposing() ) {
          this->camera.log_error( function, "cannot start another exposure while exposure in progress" );
          return ERROR;
        }

        // must have specified sampmode first (set to -1 in constructor)
        //
        if ( this->camera_info.sampmode == -1 ) {
          this->camera.log_error( function, "sampmode has not been set" );
          return ERROR;
        }

        // must have specified sampmode first (set to -1 in constructor)
        //
        if ( this->camera_info.nexp == -1 ) {
          this->camera.log_error( function, "nexp undefined (error in sampmode?)" );
          return ERROR;
        }

        int nseq = 1;  // default number of sequences if not otherwise specified by nseq_in

        // If nseq_in is supplied then try to use that to set nseq, number of exposures (files)
        //
        if ( not nseq_in.empty() ) {
          try {
            nseq = std::stoi( nseq_in );    // test that nseq_in is an integer
          }
          catch (std::invalid_argument &) {
            message.str(""); message << "unable to convert requested number of sequences: " << nseq_in << " to integer";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
          catch (std::out_of_range &) {
            message.str(""); message << "requested number of sequences  " << nseq_in << " outside integer range";
            this->camera.log_error( function, message.str() );
            return(ERROR);
          }
        }

        // Everything okay, tell the world we're exposing now
        //
        this->camera.set_exposing();
        this->camera.async.enqueue( "EXPOSING: true" );

        // Clear the abort state
        //
        this->camera.clear_abort();
        this->camera_info.exposure_aborted = false;

        // Add NIRC2 system headers
        //
        this->make_camera_header();

        message.str(""); message << "beginning " << nseq << " sequence" << ( nseq > 1 ? "s" : "" );
        logwrite( function, message.str() );

        // Loop over nseq.
        // This is like sending the "expose" command nseq times,
        // so each of these generates a separate FITS file.
        //
        int totseq = nseq;
        long ret;
        while( not this->camera.is_aborted() && ( nseq-- > 0 ) ) {
          ret = this->do_expose( std::to_string( this->camera_info.nexp ) );
          message.str(""); message << "NSEQ:" << (totseq-nseq);
          this->camera.async.enqueue( message.str() );
          if ( ret != NO_ERROR ) break;
          message.str(""); message << nseq << " sequence" << ( nseq > 1 ? "s" : "" ) << " remaining";
          logwrite( function, message.str() );
        }

        this->camera.clear_exposing();
        this->camera.async.enqueue( "EXPOSING: false" );

        return( ret );
      }
      /***** Archon::Interface::expose ****************************************/


      /***** Archon::Interface::make_camera_header ****************************/
      /**
       * @brief      adds header keywords to the systemkeys database
       *
       */
      void Interface::make_camera_header( ) {
        std::stringstream keystr;

        keystr.str("");
        keystr << "ITIME=" << std::fixed << std::showpoint << std::setprecision(3)
                           << this->camera_info.exposure_time/1000.0
                           << " // integration time per coadd in sec";
        this->systemkeys.addkey( keystr.str() );

        keystr.str("");
        keystr << "SAMPMODE=" << this->camera_info.sampmode << " //";
        for ( int i=0; i < (int)SAMPMODE_NAME.size(); i++ ) keystr << " " << i+1 << ":" << SAMPMODE_NAME[i];
        this->systemkeys.addkey( keystr.str() );

        switch ( this->camera_info.sampmode ) {

          case SAMPMODE_MCDS:
            keystr.str("");
            keystr << "MULTISAM=" << this->camera_info.sampmode_frames/2 << " // number of MCDS pairs";
            this->systemkeys.addkey( keystr.str() );
            break;

          case SAMPMODE_CDS:
            keystr.str("");
            keystr << "MULTISAM=" << this->camera_info.sampmode_frames/2 << " // number of pairs";
            this->systemkeys.addkey( keystr.str() );
            break;

          case SAMPMODE_UTR:
            keystr.str("");
            keystr << "MULTISAM=" << this->camera_info.sampmode_frames << " // number of UTR samples";
            this->systemkeys.addkey( keystr.str() );
            break;

          default:
            this->systemkeys.delkey( "MULTISAM" );                        // remove the MULTISAM keyword
            break;
        }

        keystr.str("");
        keystr << "COADDS=" << this->camera_info.sampmode_ext << " // number of coadds";
        this->systemkeys.addkey( keystr.str() );

        return;
      }
      /***** Archon::Interface::make_camera_header ****************************/


      /***** Archon::Interface::recalc_geometry *******************************/
      /**
       * @brief      recalculate geometry
       * @returns    ERROR or NO_ERROR
       *
       */
      long Interface::recalc_geometry( ) {
        std::string function = "Archon::Instrument::recalc_geometry";
        std::stringstream message;
        long error = NO_ERROR;
        std::string mode = this->camera_info.current_observing_mode;

        // Prior to this, detector_pixels[0] = PIXELCOUNT
	//                detector_pixels[1] = LINECOUNT
	// multiply them here by the number of amplifiers to get the detector pixel geometry.
	//
        this->camera_info.detector_pixels[0] *= this->modemap[mode].geometry.amps[0];
        this->camera_info.detector_pixels[1] *= this->modemap[mode].geometry.amps[1];
        this->camera_info.frame_type = Camera::FRAME_IMAGE;

        // ROI is the full detector
	//
        this->camera_info.region_of_interest[0] = 1;
        this->camera_info.region_of_interest[1] = this->camera_info.detector_pixels[0];
        this->camera_info.region_of_interest[2] = 1;
        this->camera_info.region_of_interest[3] = this->camera_info.detector_pixels[1];

        // Binning factor (no binning)
	//
        this->camera_info.binning[0] = 1;
        this->camera_info.binning[1] = 1;

        // The current imwidth and imheight is based on these two parameters
        //
        long check_npp=0;
        long check_nrq=0;

        if (error==NO_ERROR) error = get_parammap_value( "nPixelsPair", check_npp );
        if (error==NO_ERROR) error = get_parammap_value( "nRowsQuad", check_nrq );

        this->camera_info.imwidth_read  = 32 * static_cast<int>(check_npp);
        this->camera_info.imheight_read =  8 * static_cast<int>(check_nrq);
        this->camera_info.imwidth       = this->camera_info.imwidth_read;
        this->camera_info.imheight      = this->camera_info.imheight_read - 8;

        error = this->camera_info.set_axes();                                                 // 16 bit raw is unsigned short int
        this->camera_info.section_size = this->camera_info.imwidth
                                       * this->camera_info.imheight
                                       * ( this->camera_info.fitscubed>1 ? this->camera_info.axes[2] : 1 );

        // allocate image_data in blocks because the controller outputs data in units of blocks
        //
        int num_detect = this->modemap[mode].geometry.num_detect;
        this->image_data_bytes = (uint32_t) floor( ((this->camera_info.image_memory * num_detect) + BLOCK_LEN - 1 ) / BLOCK_LEN ) * BLOCK_LEN;

        if (this->image_data_bytes == 0) {
          this->camera.log_error( function, "image data size is zero! check NUM_DETECT, HORI_AMPS, VERT_AMPS in .acf file" );
          error = ERROR;
        }

        this->camera_info.current_observing_mode = mode;       // identify the newly selected mode in the camera_info class object
        this->modeselected = true;                             // a valid mode has been selected

        message.str(""); message << "new mode: " << mode << " will use " << this->camera_info.bitpix << " bits per pixel";
        logwrite(function, message.str());

        // Calculate amplifier sections
        //
        int rows = this->modemap[mode].geometry.linecount;     // rows per section
        int cols = this->modemap[mode].geometry.pixelcount;    // cols per section

        int hamps = this->modemap[mode].geometry.amps[0];      // horizontal amplifiers
        int vamps = this->modemap[mode].geometry.amps[1];      // vertical amplifiers

        int x0=-1, x1, y0, y1;                                 // for indexing
        std::vector<long> coords;                              // vector of coordinates, convention is x0,x1,y0,y1
        int framemode = this->modemap[mode].geometry.framemode;

        this->camera_info.amp_section.clear();                 // vector of coords vectors, one set of coords per extension

        for ( int y=0; y<vamps; y++ ) {
          for ( int x=0; x<hamps; x++ ) {
            if ( framemode == 2 ) {
              x0 = x; x1=x+1;
              y0 = y; y1=y+1;
            }
            else {
              x0++;   x1=x0+1;
              y0 = 0; y1=1;
            }
            coords.clear();
            coords.push_back( (x0*cols + 1) );                 // x0 is xstart
            coords.push_back( (x1)*cols );                     // x1 is xstop, xrange = x0:x1
            coords.push_back( (y0*rows + 1) );                 // y0 is ystart
            coords.push_back( (y1)*rows );                     // y1 is ystop, yrange = y0:y1

            this->camera_info.amp_section.push_back( coords ); // (x0,x1,y0,y1) for this extension

          }
        }
        message.str(""); message << "identified " << this->camera_info.amp_section.size() << " amplifier sections";
        logwrite( function, message.str() );

        return( error );
      }
      /***** Archon::Interface::recalc_geometry *******************************/


      /***** Archon::Interface::calc_readouttime ******************************/
      /**
       * @brief      calculate the readout time
       * @returns    ERROR or NO_ERROR
       *
       * The readout time depends on the ROI geometry and the sampling mode, so 
       * this function should be called whenever either of those change.
       *
       */
      long Interface::calc_readouttime( ) {
        std::string function = "Archon::Instrument::calc_readouttime";
        std::stringstream message;

        double frame_ohead = this->camera_info.frame_start_time + this->camera_info.fs_pulse_time;
        double row_ohead   = this->camera_info.row_overhead_time;
        double pixeltime   = this->camera_info.pixel_time;
        double pixelskip   = this->camera_info.pixel_skip_time;
        double rowskip     = this->camera_info.row_skip_time;

        int cols           = this->camera_info.imwidth;
        int rows           = this->camera_info.imheight;

        double rowtime     = (cols/32.) * pixeltime + ( 1024/32. - cols/32. ) * pixelskip + row_ohead;

        long readouttime   = std::lround( ( frame_ohead + (4 + rows/2.)*rowtime + rowskip * ( 516 - rows/2 - 4 ) ) / 1000. );

        // The class stores the total readout time, which is this readouttime (per frame)
        // multiplied by the number of MCDS pairs, or by 1, if not in MCDS mode.
        //
        this->camera_info.readouttime = readouttime * ( ( this->camera_info.sampmode == SAMPMODE_MCDS ) ? this->camera_info.nmcds/2 : 1 );

        // Check if the exposure time needs to be updated
        //
        long retval = this->exptime( this->camera_info.requested_exptime );

        message.str(""); message << "frame_overhead=" << frame_ohead << " pixel=" << pixeltime
                                 << " pixel_skip=" << pixelskip << " row_overhead=" << row_ohead
                                 << " row_skip=" << rowskip << " total row time=" << rowtime << " usec."
                                 << " frame readouttime=" << readouttime
                                 << " total readouttime=" << this->camera_info.readouttime << " msec";
        logwrite( function, message.str(), LogLevel::DEBUG );

        return( retval );
      }
      /***** Archon::Interface::calc_readouttime ******************************/


      /***** Archon::Interface::region_of_interest ****************************/
      /**
       * @brief      define a region of interest for NIRC2
       * @param[in]  args string which must contain width and height
       * @return     ERROR or NO_ERROR
       *
       * This function is overloaded. This version has no return value.
       * See the overloaded function for full details.
       *
       */
      long Interface::region_of_interest( std::string args ) {
        std::string dontcare;
        return this->region_of_interest( args, dontcare );
      }
      /***** Archon::Interface::region_of_interest ****************************/


      /***** Archon::Interface::region_of_interest ****************************/
      /**
       * @brief      define a region of interest for NIRC2
       * @param[in]  args string which must contain width and height
       * @param[out] retstring, reference to string which contains the width and height
       * @return     ERROR or NO_ERROR
       *
       * This function is overloaded with a version that provides no return value.
       *
       * Specify width and height only.
       * Width must be 32 <= cols <= 1024 and multiple of 32
       * Height must be 8 <= rows <= 1024 and multiple of 8
       * The NIRC2 region of interest is always centered on the detector.
       *
       */
      long Interface::region_of_interest( std::string args, std::string &retstring ) {
        std::string function = "Archon::Instrument::region_of_interest";
        std::stringstream message;
        std::vector<std::string> tokens;
        long error = NO_ERROR;
        int trywidth=0, tryheight=0;

        message.str(""); message << "args=" << args; logwrite( function, message.str(), LogLevel::DEBUG );

        // Cannot change ROI while exposing
        //
        if ( this->camera.is_exposing() ) {
          this->camera.log_error( function, "cannot change ROI while exposure in progress" );
          return ERROR;
        }

        // Firmware must be loaded and a mode must have been selected
        //
        if ( ! this->firmwareloaded ) {
          this->camera.log_error( function, "no firmware loaded" );
          return ERROR;
        }

        if ( ! this->modeselected ) {
          this->camera.log_error( function, "no mode selected" );
          return ERROR;
        }

        // process args only if not empty
        //
        if ( not args.empty() ) {

          Tokenize( args, tokens, " " );

          // Extract and convert the width and height tokens.
          // If only one value is specified then force a square ROI (width=height).
          //
          try {
            if ( tokens.size() > 0 ) trywidth  = std::stoi( tokens.at(0) );
            if ( tokens.size() > 1 ) tryheight = std::stoi( tokens.at(1) ); else tryheight = trywidth;
          }
          catch( std::invalid_argument const &e ) {
            message << "invalid argument parsing " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
          catch( std::out_of_range const &e ) {
            message << "out of range parsing " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // check that width and height are in range and valid values
          //
          if ( trywidth < 32 || trywidth > 1024 ) {
            message.str(""); message << "width " << trywidth << " out of range {32:1024}";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( trywidth % 32 != 0 ) {
            message.str(""); message << "width " << trywidth << " not a multiple of 32";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( tryheight < 8 || tryheight > 1024 ) {
            message.str(""); message << "height " << tryheight << " out of range {8:1024}";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          if ( tryheight % 8 != 0 ) {
            message.str(""); message << "height " << tryheight << " not a multiple of 8";
            this->camera.log_error( function, message.str() );
            error = ERROR;
          }

          // compute the parameters required for the ACF to realize this width and height
          //
          int NRQ = (tryheight / 8) + 1;   /// nRowsQuad, add +1 for Aladdin III
          int SRQ = (128 - NRQ) + 1;       /// SkippedRowsQuad, add +1 for Aladdin III
          int LC  = NRQ * 4;               /// LINECOUNT **overwritten below! depends on sampmode

          int NPP = trywidth / 32;         /// nPixelsPair
          int SCQ = 32 - NPP;              /// SkippedColumnsQuad
          int PC  = NPP * 2;               /// PIXELCOUNT

          // write the parameters to Archon
          //
          std::stringstream cmd;

          cmd.str(""); cmd << "nRowsQuad " << NRQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "SkippedRowsQuad " << SRQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "nPixelsPair " << NPP;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          cmd.str(""); cmd << "SkippedColumnsQuad " << SCQ;
          if (error==NO_ERROR) error = this->set_parameter( cmd.str() );

          std::string dontcare;

          // LINECOUNT must be doubled for CDS Video mode
          //
          if ( this->camera_info.sampmode == SAMPMODE_RXRV ) {
            LC = NRQ * 8;
          }
          else {
            LC = NRQ * 4;
          }
          cmd.str(""); cmd << "LINECOUNT " << LC;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          cmd.str(""); cmd << "PIXELCOUNT " << PC;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          // get out now if any errors
          //
          if ( error != NO_ERROR ) {
            this->camera.log_error( function, "writing ROI" );
            return error;
          }

          // update the modemap, in case someone asks again
          //
          std::string mode = this->camera_info.current_observing_mode;

          this->modemap[mode].geometry.linecount  = LC;
          this->modemap[mode].geometry.pixelcount = PC;

          if (error==NO_ERROR) error = get_configmap_value( "PIXELCOUNT", this->camera_info.detector_pixels[0] );
          if (error==NO_ERROR) error = get_configmap_value( "LINECOUNT", this->camera_info.detector_pixels[1] );

          if (error==NO_ERROR) error = this->recalc_geometry();  // this sets camera_info.imwidth, .imheight

        } // end if args not empty

/***
        // regardless of args empty or not, check and return the current width and height
        //
        long check_npp=0;
        long check_nrq=0;

        if (error==NO_ERROR) error = get_parammap_value( "nPixelsPair", check_npp );
        if (error==NO_ERROR) error = get_parammap_value( "nRowsQuad", check_nrq );

        this->camera_info.imwidth_read  = 32 * check_npp;
        this->camera_info.imheight_read =  8 * check_nrq;
        this->camera_info.imwidth       = this->camera_info.imwidth_read;
        this->camera_info.imheight      = this->camera_info.imheight_read - 8;
        this->camera_info.set_axes();
        this->camera_info.section_size  = this->camera_info.imwidth * this->camera_info.imheight * this->camera_info.axes[2];
***/
        message.str(""); message << this->camera_info.imwidth << " " << this->camera_info.imheight;

        retstring = message.str();

        return( error );
      }
      /***** Archon::Interface::region_of_interest ****************************/


      /***** Archon::Interface::sample_mode ***********************************/
      /**
       * @brief      set the sample mode
       * @param[in]  args
       * @return     ERROR or NO_ERROR
       *
       * This function is overloaded. This version has no return value.
       * See the overloaded function for full details.
       *
       */
      long Interface::sample_mode( std::string args ) {
        std::string dontcare;
        return this->sample_mode( args, dontcare );
      }
      /***** Archon::Interface::sample_mode ***********************************/


      /***** Archon::Interface::sample_mode ***********************************/
      /**
       * @brief      set the sample mode
       * @param[in]  args
       * @param[out] retstring
       * @return     ERROR or NO_ERROR
       *
       * This function is overloaded with a version that has no return value.
       *
       * General format in NIRC2-terminolgy is: <mode> <j> <k>
       * where j and k have various meanings.  Use the guide below:
       *
       * Input args string contains sample mode and a count for that mode.
       * valid strings for each mode are as follows:
       * ~~~
       *  SINGLE:  1 1 1
       *  CDS:     2 1 <ext>
       *  MCDS:    3 <pairs> <ext>
       *  UTR:     4 <samples> <ramps>
       *  RXV:     5 1 <frames>
       *  RXRV:    6 1 <frames>
       * ~~~
       *
       */
      long Interface::sample_mode( std::string args, std::string &retstring ) {
        std::string function = "Archon::Interface::sample_mode";
        std::stringstream message;
        std::vector<std::string> tokens;
        long error = NO_ERROR;

        message.str(""); message << "args=" << args; logwrite( function, message.str(), LogLevel::DEBUG );

        // Cannot change while exposure in progress
        //
        if ( this->camera.is_exposing() ) {
          this->camera.log_error( function, "cannot change sampmode while exposure in progress" );
          return ERROR;
        }

        // Firmware must be loaded before selecting a mode because this will write to the controller
        //
        if ( ! this->firmwareloaded ) {
          this->camera.log_error( function, "no firmware loaded" );
          return ERROR;
        }

        int mode_in=-1;
        uint32_t multisamp=0;
        int coadds=-1;

        // process args only if not empty
        //
        if ( ! args.empty() ) {

          Tokenize( args, tokens, " " );

          if ( tokens.size() != 3 ) {
            message.str(""); message << "received " << tokens.size() << " but expected 3 arguments: <mode> <j> <k>";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // extract and convert the mode, multisamp and coadds from the input arg string
          //
          try {
            mode_in   = std::stoi( tokens.at(0) );
            unsigned long value = std::stoul( tokens.at(1) );
            if ( value > std::numeric_limits<uint32_t>::max() ) throw std::out_of_range("value exceeds uint32_t range");
            multisamp = static_cast<uint32_t>(value);
            coadds    = std::stoi( tokens.at(2) );
          }
          catch( std::invalid_argument const &e ) {
            message << "invalid argument parsing args " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
          catch( std::out_of_range const &e ) {
            message << "out of range parsing args " << args << ": " << e.what();
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }

          // Check the values of the additional args for the requested mode.
          // Each mode will have its own requirements on the accompanying values.
          //
          switch( mode_in ) {

            // For SINGLE, everything is setup just like for RXV but with the <frames> parameter = 1 (aka coadds).
            //
            case SAMPMODE_SINGLE:
              if ( multisamp != 1 ) {
                message.str(""); message << "multisamp " << multisamp << " invalid. must equal 1";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( coadds != 1 ) {
                message.str(""); message << "coadds " << coadds << " invalid. must equal 1";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }

              // write the parameters to Archon to set RX and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 1;
              this->camera_info.fitscubed = 1;
              this->camera_info.nmcds = 0;
              this->camera_info.iscds = false;
              break;

            // For UTR, the first argument <multisamp> is the number of frames (cubedepth),
            // and the second argument <coadds> is the number of ramps.
            //
            case SAMPMODE_UTR:
              if ( multisamp < 1 ) {
                message.str(""); message << "requested UTR samples " << multisamp << " must be > 0";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( coadds < 1 ) {
                message.str(""); message << "requested UTR ramps " << coadds << " must be > 0";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set UTR and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, multisamp );
              this->camera_info.cubedepth = multisamp;
              this->camera_info.fitscubed = multisamp;
              this->camera_info.nmcds = 0;
              this->camera_info.iscds = false;
              break;

            // For CDS the first argument <multisamp> must = 1 and the second arg <coadds>
            // is the number of extensions. The NIRC2 user wants to enter a "1" here, but
            // then change it to a "2".
            //
            case SAMPMODE_CDS:
              if ( multisamp != 1 ) {
                message.str(""); message << "multisamp " << multisamp << " invalid. must equal 1";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              multisamp = 2;  // convert back to common sense
              if ( coadds < 1 ) {
                message.str(""); message << "coadds " << coadds << " invalid. must specify a non-zero number of extensions";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set CDS (a special form of MCDS) and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 2;
              this->camera_info.fitscubed = 2;
              this->camera_info.nmcds = 0;
              this->camera_info.iscds = true;
              break;

            // For MCDS, multisamp will be the total number of frames per extension (=cubedepth)
            // but only multisamp/2 MCDS pairs. The NIRC2 user wants to enter number of pairs.
            // Accept pairs, but then multiply by 2 to remain consistent with everything else which
            // is in number of frames.
            //
            case SAMPMODE_MCDS:
              if ( coadds < 1 ) {
                message.str(""); message << "coadds " << coadds << " invalid. must specify non-zero number of extensions";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( multisamp < 1 ) {
                message.str(""); message << "requested MCDS pairs " << multisamp << " must be a non-zero";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              multisamp *= 2;  // convert back to common sense

              // write the parameters to Archon to set MCDS and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, multisamp/2 );  // number of pairs is multisamp/2
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = multisamp;
              this->camera_info.fitscubed = multisamp;
              this->camera_info.nmcds = multisamp;
              this->camera_info.iscds = true;
              break;

            case SAMPMODE_RXV:
              //
              // For non-CDS video (Rx mode) the first argument must be = 1 and the
              // second argument specifies the number of extensions. There will always
              // be 1 frame per extension with N extensions, so this N will be passed 
              // to do_expose( coadds ).
              //
              if ( multisamp != 1 ) {
                message.str(""); message << "multisamp " << multisamp << " invalid. must equal 1";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( coadds < 1 ) {
                message.str(""); message << "coadds " << coadds << " invalid. must specify a non-zero number of frames";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set RX and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 1;
              this->camera_info.fitscubed = 1;
              this->camera_info.nmcds = 0;
              this->camera_info.iscds = false;
              break;

            case SAMPMODE_RXRV:
              //
              // For CDS video (RxR mode) the first argument must be = 1 and the
              // second argument specifies the number of extensions. There will always 
              // be 1 frame per extension with N extensions, so this N will be passed 
              // to do_expose( coadds ).
              //
              // This differs from Rx mode in that each frame is 2x the size.
              //
              if ( multisamp != 1 ) {
                message.str(""); message << "multisamp " << multisamp << " invalid. must equal 1";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              if ( coadds < 1 ) {
                message.str(""); message << "coadds " << coadds << " invalid. must specify a non-zero number of frames";
                this->camera.log_error( function, message.str() );
                return( ERROR );
              }
              // write the parameters to Archon to set RX and clear the other modes
              //
              if (error==NO_ERROR) error = this->set_parameter( this->mcdspairs_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->mcdsmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->rxrmode_param, 1 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrmode_param, 0 );
              if (error==NO_ERROR) error = this->set_parameter( this->utrsamples_param, 0 );
              this->camera_info.cubedepth = 1;  // don't alter memory allocation
              this->camera_info.fitscubed = 2;  // but force the fits writer to write a cube
              this->camera_info.nmcds = 0;
              this->camera_info.iscds = false;
              break;

            default:
              message.str(""); message << "unrecognized sample mode: " << mode_in;
              this->camera.log_error( function, message.str() );
              return( ERROR );
          }

          // Enable multi-extensions always, for consistency
          //
          this->camera.mex( true );

          // Enable co-adding
          //
          this->camera.coadd( true );

          // One last error check.
          // Do not allow camera_info to set a value less than 1 for either frames or extensions.
          //
          if ( multisamp < 1 || coadds < 1 ) error = ERROR;

          if ( error == NO_ERROR ) {
            this->camera_info.sampmode        = mode_in;
            this->camera_info.sampmode_ext    = coadds;
            this->camera_info.sampmode_frames = multisamp;
            this->camera_info.nexp            = coadds;
// these are done in recalc_geometry
//          this->camera_info.set_axes();
//          this->camera_info.section_size = this->camera_info.imwidth * this->camera_info.imheight * this->camera_info.axes[2];
          }
          else {
            message.str(""); message << "frames, extensions can't be <1: multisamp=" << multisamp << " coadds=" << coadds;
            this->camera.log_error( function, message.str() );
          }

          // Now LINECOUNT must be set because RXRV is x2 the size.
          // It will always be a multiple of nRowsQuad.
          // Also set the readout mode here, either NIRC2 or NIRC2VIDEO,
          // which is required for descrambling.
          //
          long nRowsQuad=0;
          long LINECOUNT=0;
          std::string dontcare;
          std::stringstream cmd;

          if (error==NO_ERROR) error = get_parammap_value( "nRowsQuad", nRowsQuad );

          switch( mode_in ) {
            case SAMPMODE_RXRV:
              this->readout( "NIRC2VIDEO", dontcare );
              LINECOUNT = nRowsQuad * 8;
              break;
            case SAMPMODE_SINGLE:
            case SAMPMODE_UTR:
            case SAMPMODE_CDS:
            case SAMPMODE_MCDS:
            case SAMPMODE_RXV:
              this->readout( "NIRC2", dontcare );
              LINECOUNT = nRowsQuad * 4;
              break;
            default:
              message.str(""); message << "unrecognized sample mode: " << mode_in;
              this->camera.log_error( function, message.str() );
              error = ERROR;
              break;
          }

          cmd << "LINECOUNT " << LINECOUNT;
          if (error==NO_ERROR) error = this->cds( cmd.str(), dontcare );

          // update the modemap, in case someone asks again
          //
          this->modemap[ this->camera_info.current_observing_mode ].geometry.linecount  = LINECOUNT;

          if (error==NO_ERROR) error = get_configmap_value( "PIXELCOUNT", this->camera_info.detector_pixels[0] );
          if (error==NO_ERROR) error = get_configmap_value( "LINECOUNT", this->camera_info.detector_pixels[1] );

          if (error==NO_ERROR) error = this->recalc_geometry();
        }

        message.str(""); message << "sampmode=" << this->camera_info.sampmode
                                 << " sampmode_ext=" << this->camera_info.sampmode_ext
                                 << " sampmode_frames=" << this->camera_info.sampmode_frames
                                 << " nexp=" << this->camera_info.nexp
                                 << " mex=" << ( this->camera.mex() ? true : false );
        logwrite( function, message.str(), LogLevel::DEBUG );

        // The return message has to be manipulated because the NIRC2 user wants to know frames
        // for some modes, pairs for others. So for CDS||MCDS return pairs, all else return frames.
        //
        int jj = ( ( this->camera_info.sampmode==SAMPMODE_CDS || this->camera_info.sampmode==SAMPMODE_MCDS ) ? 
                 this->camera_info.sampmode_frames/2 : this->camera_info.sampmode_frames );

        message.str(""); message << this->camera_info.sampmode
                                 << " " << jj
                                 << " " << this->camera_info.sampmode_ext;
        retstring = message.str();

        message.str(""); message << "sample mode = " << retstring;
        logwrite( function, message.str() );

        return( error );
      }
      /***** Archon::Interface::sample_mode ***********************************/


      /**************** Archon::Interface::longexposure ***********************/
      /**
       * @brief      set/get longexposure mode
       * @details    NIRC2 doesn't support longexposure but Keck wants to be able to send the command
       * @param[in]  state_in   string to set long exposure state, can be {0,1,true,false}
       * @param[out] state_out  reference to string containing the long exposure state
       *
       */
      long Interface::longexposure( std::string state_in, std::string &state_out ) {
        std::string function = "Archon::Instrument::longexposure";
        std::stringstream message;

	// If something is passed then make sure it's "0" or "false"
        //
        if ( !state_in.empty() ) {
          try {
            std::transform( state_in.begin(), state_in.end(), state_in.begin(), ::toupper );  // make uppercase
            if ( state_in != "FALSE" && state_in != "0" ) {
              message.str(""); message << "longexposure state " << state_in << " is invalid. NIRC2 supports only {false|0}";
              this->camera.log_error( function, message.str() );
              return( ERROR );
            }
          }
          catch (...) {
            message.str(""); message << "unknown exception converting longexposure state " << state_in << " to uppercase";
            this->camera.log_error( function, message.str() );
            return( ERROR );
          }
        }

        // error or not, the state reported now will be whatever was last successfully set
        //
        this->camera_info.exposure_unit   = "msec";
        this->camera_info.exposure_factor = 1000;
        state_out = "false";

        return( NO_ERROR );
      }
      /***** Archon::Interface::longexposure **********************************/

}
