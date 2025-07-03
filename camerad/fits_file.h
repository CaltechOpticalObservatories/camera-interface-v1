/**
 \file fits_file.h
 \brief FITS file handling engine
 \details This header file contains the functions and variables used to create
 and manage FITS files in the ROBO software package.  The FITS files are
 created with a threaded system that allows multiple files to be written
 rapidly enough to be useful for telemetry of the fastest CCD cameras in the
 instrument.  A standard FITS header is also created, using both parameters
 passed to the functions and files written into the status area of the disk
 with information from subsystems that are necessary for inclusion in the FITS
 headers.  Supports writing single FITS images and data cubes, and compressed
 images.
 \author Dr. Reed L. Riddle  riddle@caltech.edu
 \note This is a slimmed down version of the FITS data management system from
 the Robo-AO project.  It is a method to write FITS data files from a system,
 such as an AO system, that generates files at a high frequency.  This package
 has removed many features from the software that aren't necessary if you just
 want to write a FITS file to keep this focused on only that operation.

 Modified (slightly) to fit into camerad (D.Hale)
 */

#pragma once

// System include files
# include <iostream>
# include <boost/thread.hpp>
# include <boost/chrono.hpp>
# include <fstream>
# include <sstream>
# include <CCfits/CCfits>
# include <dirent.h>
# include <deque>

// Local include files
#include "utilities.h"
#include "camera.h"

// Value flags for the supported FITS compression modes
constexpr int FITS_COMPRESSION_NONE=0;
constexpr int FITS_COMPRESSION_RICE=RICE_1;
constexpr int FITS_COMPRESSION_GZIP=GZIP_1;
constexpr int FITS_COMPRESSION_PLIO=PLIO_1;

/// Maximum file size supported for image data cubes = 1GB
const unsigned long MAX_IMAGE_DATA_SIZE = 1073741824;

/** \class FITS_mex_frame
 \brief FITS mex frame object
 \details This class contains the information that goes into a frame of a data
 cube.  These are put into a vector and written out by the FITS_file class in
 its mex writing thread.
 */
template <class T>
class FITS_mex_frame
{
public:

  /** \var std::valarray<T> array
   \details Array containing FITS frame data */
  std::valarray<T> array;

  /** \var std::string timestamp
   \details Timestamp when the frame was created */
  std::string timestamp;

  /** \var int sequence
   \details Sequence number for the frame */
  int sequence;

  /** \var Camera::Information camera_info
   \details Camera information for the frame */
  Camera::Information camera_info;


  /**************** FITS_file::FITS_mex_frame ****************/
  /**
   This function constructs the FITS_mex_frame class.  Note that the array is
   automatically filled by the valarray constructor when this class is
   constructed.
   */
  FITS_mex_frame(T * data, int size, std::string timestamp_in, int sequence_in,
                  Camera::Information camera_info_in): array(data, size)
  {
    logwrite("FITS_file::FITS_mex_frame", "[DEBUG] constructed with type "+demangle(typeid(T).name()));
// std::cout << "(FITS_mex_frame) constructing FITS_mex_frame. camera_info_in.extension=" << camera_info_in.extension.load() << "\n";
    // Copy the camera info from the input
    this->camera_info = camera_info_in;
// std::cout << "(FITS_mex_frame) -> camera_info.extension=" << this->camera_info.extension.load() << "\n";

    // Copy the timestamp from the input
    this->timestamp = timestamp_in;

    // Copy the sequence number from the input
    this->sequence = sequence_in;
  };
  /**************** FITS_file::FITS_mex_frame ****************/

};


/** \class FITS_file
 \brief FITS image file container
 \details This class handles the interactions with data that are written into
 the standard FITS format images.  It handles single images and data cubes.  Note
 that it does not read data, only writes...instruments don't read data, but it
 should be asimple matter to add reading to this class at some point.
 \note This is a template class, based on the bit type of the data (int, float,
 etc).
 */
template <class T>
class FITS_file
{
private:

  /** \var mutable boost::mutex cache_mutex
   \details Mutex to block the variables in threads */
  boost::timed_mutex cache_mutex;

  // Note that older C++ libraries use auto_ptr, so uncommend that line and
  // comment out the unique_ptr line if your library doesn't support unique_ptr
  /** \var std::auto_ptr<CCfits::FITS> pFits
   \details Pointer to FITS data container. */
//  std::auto_ptr<CCfits::FITS> pFits; // used with older C++ versions
  std::unique_ptr<CCfits::FITS> pFits;

  /** \var CCfits::ExtHDU* imageExt
   \details FITS image extension header unit */
  CCfits::ExtHDU* imageExt;

  /** \var std::string fits_name
   \details Name for the FITS file */
  std::string fits_name;

  /** \var bool file_open
   \details Flag that the FITS file is open for writing */
  bool file_open;

  /** \var int num_frames
   \details Number of frames in data cube */
  int num_frames;

  /** \var int compression
   \details Flag for compresion type used in writing files */
  int compression;

  /** \var Camera::Information open_info
   \details CAmera information object used to open a new file */
  Camera::Information open_info;

  /** \var int open_sequence
   \details Sequence number submitted when opening a new file */
  int open_sequence;

  /** \var std::string open_timestamp
   \details Timestamp submitted when opening a new file */
  std::string open_timestamp;

  /** \var float telrad
   \details Reported telescope RA (in degrees), needed for WCS entry */
  float telrad;

  /** \var float teldecd
   \details Reported telescope dec (in degrees), needed for WCS entry */
  float teldecd;

  /** \var boost::thread fits_mex_thread
   \details Boost thread variable for the mex writing thread */
  boost::thread fits_mex_thread;

  /** \var bool run_mex_thread
   \details Flag that the FITS file is open for writing */
  bool run_mex_thread;

  /** \var bool ismex
   \details Flag that this FITS object writes a multi-extension file */
  bool ismex;

  /** \var std::vector<FITS_mex_frame <T> > mex_frames
   \details Queue containing the information for each frame of the data cube */
  std::deque<FITS_mex_frame <T> > mex_frames;

  /** \var std::vector<FITS_mex_frame <T> > mex_cache
   \details Cache of incoming frames that will be written to a data cube */
  std::deque<FITS_mex_frame <T> > mex_cache;

  /** \var bool mex_thread_running
   \details Flag the the thread writing the cubes is running */
  bool mex_thread_running;

  /** \var long mex_size
   \details The size of the data cube in bytes (more or less) */
  long mex_size;

  /** \var bool last_image
   \details Flag that the last image has been delivered to the FITS cube */
  bool last_image;

  /** \var long total_frames
   \details Number of frames written for all data cubes processed */
  long total_frames;

  /** \var int max_size
   \details This is the max allowed size for any single data cube */
  int max_size;

  /** \var int max_mex_frames
   \details This is the max allowed number of frames that can be written to any
   single data cube */
  int max_mex_frames;

  inline static const std::string in_process = ".writing";  /// add this extension while fits writing is in process, remove on close


  /**************** FITS_file::swap ****************/
  /**
   This is used to swap between two FITS_file class objects.  This is used when
   constructing class objects with assignment or copy construction.
   \param [first] The first object to swap (swap into this)
   \param [second] The second object to swap (swap from this)
   */
  friend void swap(FITS_file & first, FITS_file & second)
  {
//    // Enable ADL (not necessary in our case, but good practice)
//    using std::swap;

    // By swapping the members of two classes, the two classes are effectively
    // swapped.
    std::swap(first.log, second.log);
    std::swap(first.pFits, second.pFits);
    std::swap(first.imageExt, second.imageExt);
    std::swap(first.fits_name, second.fits_name);
    std::swap(first.file_open, second.file_open);
    std::swap(first.num_frames, second.num_frames);
    std::swap(first.compression, second.compression);
    std::swap(first.open_info, second.open_info);
    std::swap(first.open_sequence, second.open_sequence);
    std::swap(first.open_timestamp, second.open_timestamp);
    std::swap(first.telrad, second.telrad);
    std::swap(first.teldecd, second.teldecd);
    std::swap(first.fits_mex_thread, second.fits_mex_thread);
    std::swap(first.run_mex_thread, second.run_mex_thread);
    std::swap(first.ismex, second.ismex);
    std::swap(first.mex_frames, second.mex_frames);
    std::swap(first.mex_cache, second.mex_cache);
    std::swap(first.mex_thread_running, second.mex_thread_running);
    std::swap(first.mex_size, second.mex_size);
    std::swap(first.last_image, second.last_image);
    std::swap(first.total_frames, second.total_frames);
    std::swap(first.max_size, second.max_size);
    std::swap(first.max_mex_frames, second.max_mex_frames);
  }
  /**************** FITS_file::swap ****************/

 
  /**************** FITS_file::print_compression ****************/
  /**
   Prints a message to show what compression is being used
   */
  std::string print_compression(int _compression)
  {
    std::stringstream message;  // Temporary string for compression message

    if (_compression == FITS_COMPRESSION_NONE){
      message << "(" << FITS_COMPRESSION_NONE << ":FITS_COMPRESSION_NONE)";
    }
    else if (_compression == FITS_COMPRESSION_RICE){
      message << "(" << FITS_COMPRESSION_RICE << ":FITS_COMPRESSION_RICE)";
    }
    else if (_compression == FITS_COMPRESSION_GZIP){
      message << "(" << FITS_COMPRESSION_GZIP << ":FITS_COMPRESSION_GZIP)";
    }
    else if (_compression == FITS_COMPRESSION_PLIO){
      message << "(" << FITS_COMPRESSION_PLIO << ":FITS_COMPRESSION_PLIO)";
    }
    else {
      message << "(" << 9999 << ":FITS_COMPRESSION_UNKNOWN)";
    }
    return(message.str());
  }
  /**************** FITS_file::print_compression ****************/


  /**************** FITS_file::initialize_class ****************/
  /**
   Initializes values for the class variables
   */
  void initialize_class()
  {
    this->file_open = false;
    this->num_frames = 0;
    this->compression = FITS_COMPRESSION_NONE;
    this->telrad = 9999;
    this->teldecd = 9999;
    this->ismex = false;
    this->run_mex_thread = false;
    this->mex_thread_running = false;
    this->mex_size = 0;
    this->last_image = false;
    this->total_frames = 0;
    this->max_size = MAX_IMAGE_DATA_SIZE;
    this->max_mex_frames = 10000;
  }
  /**************** FITS_file::initialize_class ****************/


  /**************** FITS_file::open_file ****************/
  /**
   Open a FITS file to write data into it.  This function is used for data cubes
   but could be used for writing single files if you want to do it the hard way.
   \param [camera_info] The camera_info class holds image parameters such as size,
                        pixel scales, detector information
   \param [timestamp] The timestamp when the image was taken
   \param [sequence] A sequence number if the image is one in a sequence
   \param [compress] Compression to use on the image (NONE is default)
   \note None.
   */
  int open_file(Camera::Information & _camera_info,
                std::string & timestamp, int sequence,
                int compress = FITS_COMPRESSION_NONE)
  {
    // Set the function information for logging
    std::string function("FITS_file::open_file");
    std::stringstream message;

    message << "opening FITS file image for " << _camera_info.fits_name+this->in_process;
    logwrite(function, message.str());
    message.str("");

    message << "[DEBUG] cubedepth=" << _camera_info.cubedepth << " fitscubed=" << _camera_info.fitscubed;
    logwrite(function, message.str());
    message.str("");
//  int num_axis = ( _camera_info.cubedepth > 1 ? 3 : 2 );  // local variable for number of axes   CHECK
    int num_axis = ( _camera_info.fitscubed > 1 ? 3 : 2 );  // local variable for number of axes
    long _axes[num_axis];                                   // local variable of image axes size

    try {

      // Set the class compression flag
      this->compression = FITS_COMPRESSION_NONE;
      if (compress != FITS_COMPRESSION_NONE){
        if (compress == FITS_COMPRESSION_RICE){
          this->compression = FITS_COMPRESSION_RICE;
        }
        else if (compress == FITS_COMPRESSION_GZIP){
          this->compression = FITS_COMPRESSION_GZIP;
        }
        else if (compress == FITS_COMPRESSION_PLIO){
          this->compression = FITS_COMPRESSION_PLIO;
        }
        else {
          message << "ERROR unknown compression type: " << compress;
          logwrite(function, message.str());
          message.str("");
        }
      }

      // If the image is multi-extension, we don't want to write anything into the
      // primary image, so set the axes to 0 to avoid allocating the space. If
      // this is a single frame image, then we need to allocate the space,
      // unless it is compressed in which case we also set the primary image
      // to a null image.

      if (this->ismex == true){
        // multi-extension has no data associated with primary header
        for ( int i=0; i<num_axis; i++ ) _axes[i]=0;
        num_axis = 0;
      }
      else {
        for ( int i=0; i<num_axis; i++ ) _axes[i]=_camera_info.naxes[i];
      }


      this->fits_name = _camera_info.fits_name + this->in_process;
//    auto it = _camera_info.fits_name.find_last_of("/");
//    this->fits_name=_camera_info.fits_name.substr(0,it)+"/__"+_camera_info.fits_name.substr(it+1);

      // Check that we can write the file, because CCFits will crash if not
      std::ofstream checkfile (this->fits_name.c_str());
      if (checkfile.is_open()) {
        checkfile.close();
        std::remove(this->fits_name.c_str());
      }
      else {
        message << "ERROR unable to create file " << this->fits_name;
        logwrite(function, message.str());
        return(ERROR);
      }

      // Allocate the FITS file container, which holds all of the information
      // used by CCfits to write a file
      this->pFits.reset( new CCfits::FITS(this->fits_name, _camera_info.bitpix,
                                          num_axis, _axes) );

//    // Create the primary image header
      this->make_header(this->fits_name.substr(_camera_info.directory.length()+1),
                          timestamp, sequence, _camera_info);
  
      for ( const auto &[key,val] : _camera_info.systemkeys.keydb ) {
        this->add_primary_key( val.keyword, val.keytype, val.keyvalue, val.keycomment );
      }

      // I think this is wrong but it's what NIRC2 wants. Should be == USHORT_IMG
      if (_camera_info.datatype == SHORT_IMG) {
        this->pFits->pHDU().addKey( "BZERO", 32768, "offset for signed short int" );
        this->pFits->pHDU().addKey( "BSCALE", 1, "scaling factor" );
      }
      else {
        this->pFits->pHDU().addKey( "BZERO", 0.0, "offset" );
        this->pFits->pHDU().addKey( "BSCALE", 1, "scaling factor" );
      }

      // Set the compression. You have to do this after allocating the FITS file
      if (this->compression == FITS_COMPRESSION_RICE){
        this->pFits->setCompressionType(RICE_1);
      }
      else if (this->compression == FITS_COMPRESSION_GZIP){
        this->pFits->setCompressionType(GZIP_1);
      }
      else if (this->compression == FITS_COMPRESSION_PLIO){
        this->pFits->setCompressionType(PLIO_1);
      }
    }
    // Catch any errors from creating the FITS file and log them
    catch (CCfits::FitsException&){
      message << "ERROR unable to open FITS file " << this->fits_name;
      logwrite(function, message.str());
      return(ERROR);
    }

    // Set the flags for an open file without frames written to it
    this->file_open = true;
    this->num_frames = 0;

    // If this is a multi-extension, start the cube writing thread and set initial
    // cube parameters
    if (this->ismex == true){
      this->mex_size = 0;
      if (this->mex_thread_running == false){
        this->open_info = _camera_info;
        this->open_sequence = sequence;
        this->open_timestamp = timestamp;
        this->run_mex_thread = true;
        this->fits_mex_thread = boost::thread(&FITS_file::write_mex_thread,
                                               this);
      }
    }
    // Write a log message and return a success value
    message << "opened FITS file " << this->fits_name << " with compression "
            << print_compression(this->compression)
            << " section_size=" << _camera_info.section_size;
    return(NO_ERROR);
  }
  /**************** FITS_file::open_file ****************/


  void final_words(Camera::Information& info) {
    // Get the date- and time-only out of info.start_time into separate keywords
    //
    std::vector<std::string> datevec;
    std::string dateobs, timeobs;
    Tokenize( info.start_time, datevec, "T" );      // format is "YYYY-MM-DDTHH:MM:SS.s"
    if ( datevec.size() == 2 ) {
      dateobs = datevec.at(0);                      // date-only is the first of two tokens
      timeobs = datevec.at(1);                      // time-only is the second of two tokens
    }

    this->pFits->pHDU().addKey("DATE-BEG", info.start_time, "exposure start time" );
    this->pFits->pHDU().addKey("DATE-END", info.stop_time, "exposure stop time" );
    this->pFits->pHDU().addKey("DATE", get_timestamp(), "FITS file write time");
    this->pFits->pHDU().addKey("COMPSTAT", ( info.exposure_aborted ? "aborted" : "completed" ) , "exposure completion status");
    this->pFits->pHDU().addKey("DATE-CMD", info.cmd_start_time, "time of expose command" );
    this->pFits->pHDU().addKey("DATE-OBS", dateobs, "exposure start date" );
    this->pFits->pHDU().addKey("TIME-OBS", timeobs, "exposure start time" );
  }


  /**************** FITS_file::close_file ****************/
  /**
   Closes a FITS image file; this version is meant to close a single frame file.
   */
  int close_file(Camera::Information& _camera_info)
  {
    // Set the function information for logging
    std::string function("FITS_file::close_file");
    std::stringstream message;

    // Log what we're doing
    message << "closing FITS file " << this->fits_name;
    logwrite(function, message.str());
    message.str("");

    // Add a header keyword for the time the file was written (right now!)
    this->pFits->pHDU().addKey("DATE", get_timestamp(), "FITS file write date");

    final_words(_camera_info);

    // Write the checksum
    this->pFits->pHDU().writeChecksum();

    // Deallocate the CCfits object and close the FITS file
    this->pFits->destroy();

    // Reset flags to prepare for the next file
    this->file_open = false;

    // Log that the file closed successfully
    message << "successfully closed FITS file " << this->fits_name;
    logwrite(function, message.str());

    // Rename the file to remove the in_process extension
    //
    std::string finished_file { this->fits_name, 0, this->fits_name.rfind( this->in_process ) };  // remove extension

    std::filesystem::path in_process_path( this->fits_name );
    std::filesystem::path finished_path( finished_file );

    try {
      std::filesystem::rename( in_process_path, finished_path );
      message.str(""); message << "renamed " << this->fits_name << " to " << finished_file;
      logwrite(function, message.str());
    }
    catch ( const std::filesystem::filesystem_error &e ) {
      message.str(""); message << "ERROR renaming " << this->fits_name << " to " << finished_file << ": " << e.what();
      logwrite(function, message.str());
    }

    return(NO_ERROR);
  }
  /**************** FITS_file::close_file ****************/


  /**************** FITS_file::close_mex ****************/
  /**
   Close a FITS mex file.
   */
  int close_mex(Camera::Information& _camera_info)
  {
    // Set the function information for logging
    std::string function("FITS_file::close_mex");
    std::stringstream message;

    if (this->file_open == false){
      logwrite(function, "FITS mex file already closed");
      return(NO_ERROR);
    }

    // Log what we're doing
    message << "closing FITS data cube " << this->fits_name;
    logwrite(function, message.str());

message.str(""); message << "[DEBUG] last_image=" << (this->last_image?"T":"F")
                         << " mex_frames.size=" << this->mex_frames.size()
                         << " mex_cache.size=" << this->mex_cache.size();
logwrite(function, message.str());
    // Stop the mex writing thread only after the final image is received and
    // processed
    if (this->last_image == true && this->mex_frames.size() == 0 &&
        this->mex_cache.size() == 0){
      logwrite(function, "closing the last mex file...");
      this->run_mex_thread = false;
    }

    // Add a header keyword for the time the file was written (right now!), the
    // number of frames written into the cube, and the time the system stopped
    // taking cube data
//  this->pFits->pHDU().addKey("NFRAMES", this->num_frames,
//                             "number of frames in FITS file");
    this->pFits->pHDU().addKey("DATE", get_timestamp(), "FITS file write date");

    final_words(_camera_info);

    // Write the checksum
    this->pFits->pHDU().writeChecksum();

    // Deallocate the CCfits object and close the FITS file
    this->pFits->destroy();

    // Reset flags to prepare for the next file
    this->file_open = false;

    // Log that the file closed successfully
    message.str("");
    message << "successfully closed FITS data cube " << this->fits_name
            << ", wrote " << this->num_frames << " mex frames and "
            << this->mex_size << " image bytes, frames waiting: "
            << this->mex_frames.size() << " " << this->mex_cache.size();
    logwrite(function, message.str());

    // Rename the file to remove the in_process extension
    //
    std::string finished_file { this->fits_name, 0, this->fits_name.rfind( this->in_process ) };  // remove extension

    std::filesystem::path in_process_path( this->fits_name );
    std::filesystem::path finished_path( finished_file );

    try {
      std::filesystem::rename( in_process_path, finished_path );
      message.str(""); message << "renamed " << this->fits_name << " to " << finished_file;
      logwrite(function, message.str());
    }
    catch ( const std::filesystem::filesystem_error &e ) {
      message.str(""); message << "ERROR renaming " << this->fits_name << " to " << finished_file << ": " << e.what();
      logwrite(function, message.str());
    }

    return(NO_ERROR);
  }
  /**************** FITS_file::close_mex ****************/


  /**************** FITS_file::write_single_image ****************/
  /** Writes the FITS image to a file container.  This writes a single frame
   FITS file.
   \param [data] The image data array, a one dimensional array
   \param [timestamp] The timestamp when the image was taken
   \param [sequence] A sequence number if the image is one in a sequence
   \param [camera_info] The camera_info class holds image parameters such as
      size, pixel scales, detector information
   \param [compress] Compression to use on the image (NONE is default)
   */
  int write_single_image(T * data, std::string timestamp, int sequence,
                         Camera::Information _camera_info,
                         int compress = FITS_COMPRESSION_NONE)
  {
    // Set the function information for logging
    std::string function("FITS_file::write_single_image");
    std::stringstream message;
    logwrite(function, "[DEBUG] writing type "+demangle(typeid(T).name()));

    // Set the FITS system to verbose mode so it writes error messages
    CCfits::FITS::setVerboseMode(true);

    // Open the FITS file
    if (this->open_file(_camera_info, timestamp, sequence, compress) != NO_ERROR){
      message << "ERROR failed to open FITS file \"" << _camera_info.fits_name << "\", aborting";
      logwrite(function, message.str());
      return(ERROR);
    }

    try {
      // Move the data into a valarray, necessary to wite it using CCFITS
      std::valarray<T> array(data, _camera_info.section_size);

      // Set the first pixel value to 1, this tells the FITS system where it
      // starts writing (we don't start at anything but 1)
      long fpixel = 1;

      // Write the primary image into the FITS file if compression is off
      if (this->compression == FITS_COMPRESSION_NONE){
        this->pFits->pHDU().write(fpixel, _camera_info.section_size, array);
      }
      // With compression, write to an extension
      else {
        CCfits::ExtHDU& pseudo_primary = this->pFits->extension(1);
        pseudo_primary.write(fpixel, _camera_info.section_size, array);
      }

      // Flush the FITS container to make sure the image is written to disk
      this->pFits->flush();
    }
    // Catch any errors from the FITS system and log them
    catch (CCfits::FitsError & error){
      message << "ERROR FITS file error thrown: " << error.message();
      logwrite(function, message.str());
      return(ERROR);
    }
    catch( std::exception &e ) {
      message << "ERROR other exception thrown: " << e.what();
      logwrite(function, message.str());
      return(ERROR);
    }

    // Close the FITS file
    if (this->close_file(_camera_info) != NO_ERROR){
      message << "ERROR failed to close FITS file properly: " << this->fits_name;
      logwrite(function, message.str());
      return(ERROR);
    }

    return(NO_ERROR);
  }
  /**************** FITS_file::write_single_image ****************/


  /**************** FITS_file::write_mex_thread ****************/
  /** Writes FITS data cubes.  This is run in a thread, started when the FITS
   file is opened, and runs until the final image is received.  If the data
   cube goes over the maximum allowed size, the thread closed the cube and
   creates a new one with the same name.
   */
  void write_mex_thread()
  {
    std::string function("FITS_file::write_mex_thread");
    std::stringstream message;
    int error;

    // local copy of camera_info from the current mex_frames[0] deque element
    Camera::Information _camera_info;

    // If the thread is already flagged as running, stop here
    if (this->mex_thread_running == true){
      logwrite(function, "thread is already running, stopping!");
      return;
    }

    // Set the thread running flag
    this->mex_thread_running = true;

    // Log that the thread is starting
    logwrite(function, "starting thread to write mex frames...");

    // Set some initial values for parameters used in writing the thread
    bool finished = false;
    this->mex_size = 0;
    this->last_image = false;
    this->total_frames = 0;

    // Loop until the flag to finish processing is set
logwrite(function, "[DEBUG] enter Loop until the flag to finish processing is set");
    while (this->run_mex_thread == true && finished == false){

      // Pull data out of the cube cache if there is something in it.  This is
      // done here to avoid the writing process slowing down the process putting
      // images in the cache.
      if (this->mex_cache.size() > 0) {

        // Pull a maximum of 5 images over.  This limits how much the system is
        // accessing the cache to avoid slowing the writing process.
        int size = this->mex_cache.size();
        if (size > 5) size = 5;

        // Get the front image out of the queue, then clear it.  The mutex is
        // locked while clearing.  There are two options here, do it one image
        // at a time, or shift all five and then delete all at once.  This does
        // the former to lock the mutex for as short of a time as possible for
        // each image.
        try {
          for (int i = 0; i < size; i++) {
            if (this->mex_cache.empty() == false) {
              this->mex_frames.push_back(this->mex_cache[0]);
              if (this->cache_mutex.try_lock_for(boost::chrono::milliseconds(1))) {
                boost::lock_guard<boost::timed_mutex> lock(this->cache_mutex, boost::adopt_lock_t());
                this->mex_cache.pop_front();
              }
              size = this->mex_cache.size();
              if (size > 5) size = 5;
            }
          }
        }
        catch ( std::exception &e ) {
          message << "ERROR exception occured: " << e.what();
          logwrite( function, message.str() );
          message.str("");
        }
      }

      // Don't do anything if there are no frames available
      if (this->mex_frames.size() == 0){
        // Time out to save resources, this should be fast enough for data
        // acquisition expected by this system
        timeout(0.00001);
        // Check flags and set an exit flag if the image process is complete and
        // all images have been written out.
        if (this->last_image == true && this->mex_frames.size() == 0 &&
            this->mex_cache.size() == 0){
          finished = true;
        }
        continue;
      }

      // Open the mex file if it's not already open
      if (this->file_open == false){
        logwrite(function, "opening a new mex file...");
        error = this->open_file(this->open_info, this->open_timestamp,
                                this->open_sequence, this->compression);
        if (error != NO_ERROR){
          message << "ERROR failed to open FITS file \"" << this->fits_name
                  << "\", error code: " << error;
          logwrite(function, message.str());
          message.str("");
          continue;
        }
      }


      // Write the image to the mex file
      try {
        // copy camera_info from the current element, preserved for call to close_mex()
        _camera_info = this->mex_frames[0].camera_info;

        // Set the FITS system to verbose mode so it writes error messages
        CCfits::FITS::setVerboseMode(true);

        // Set the first pixel value to 1, this tells the FITS system where it
        // starts writing (we don't start at anything but 1)
        long fpixel = 1;

        // We set an image key to the frame number in the cube, just for tracking
        std::stringstream image_key;
        image_key << this->num_frames + 1;

        // Add the new image to the image extension
        this->imageExt = this->pFits->addImage(image_key.str(),
                                        this->mex_frames[0].camera_info.datatype,  /* was bitpix */
                                        this->mex_frames[0].camera_info.naxes);
//      // Make the image mex header
        this->make_mex_header(this->mex_frames[0].timestamp,
                               this->mex_frames[0].camera_info);
  
// moved to make_mex_header
//        for ( const auto &[key,val] : this->mex_frames[0].camera_info.systemkeys.keydb ) {
//message.str(""); message << "[MICKY] SYSTEM: " << val.keyword << "=" << val.keyvalue;
//logwrite(function,message.str()); message.str("");
//        this->add_extension_key( val.keyword, val.keytype, val.keyvalue, val.keycomment );
//        }

        // Write the image extension into the mex
        this->imageExt->write(fpixel, this->mex_frames[0].camera_info.section_size,
                              this->mex_frames[0].array);

        // Flush the FITS container to make sure the image is written to disk
        this->pFits->flush();

        #ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] wrote extension " << this->mex_frames[0].camera_info.extension;
        logwrite( function, message.str() ); message.str("");
        #endif

        // Increment the frame counter
        this->num_frames++;
        this->total_frames++;

        // Add size of the new frame to the size of the cube
        this->mex_size += this->mex_frames[0].camera_info.image_memory;

        this->mex_frames.pop_front();
      }
      // Catch any errors from the FITS system and log them
      catch (const CCfits::FitsError &err){
        message << "ERROR FITS file error thrown: " << err.message();
        logwrite(function, message.str());
        message.str("");
        break;
      }
      catch (const std::exception &err) {
        message << "ERROR std exception: " << err.what();
        logwrite(function, message.str());
        message.str("");
        break;
      }
      catch (...) {
        logwrite(function, "ERROR unknown exception");
        break;
      }

      // For cubes with lots of files, log the progress for writing the cube
      if (this->num_frames % 1000 == 0){
        message << "number of frames written: " << this->num_frames << " size: "
                << this->mex_size << " bytes, frames waiting: "
                << this->mex_frames.size() << " " << this->mex_cache.size();
        logwrite(function, message.str());
        message.str("");
      }

/******** temporarily (?) disable size and frame limit

      // If the cube has grown too large, close it
      if (this->mex_size >= this->max_size ||
          this->num_frames >= this->max_mex_frames){
        logwrite(function, "closing the mex file...");
        #ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] mex_size=" << this->mex_size << " max_size=" << this->max_size
                << " num_frames=" << this->num_frames << " max_frames=" << this->max_mex_frames;
        logwrite( function,  message.str() ); message.str("");
        #endif
        error = this->close_mex();
        if (error != NO_ERROR){
          message << "ERROR there was a problem closing the FITS mex, error code: "
                  << error;
          logwrite(function, message.str());
          message.str("");
        }
      }
********/

      // If the flag for a complete observation is set, and if the cache and
      // cube queues are empty, flag that the system can finish.
      if (this->last_image == true && this->mex_frames.size() == 0 &&
          this->mex_cache.size() == 0){
        finished = true;
        logwrite(function, "flag set to finish writing the data mex");
      }
    }
logwrite(function, "[DEBUG] exit Loop until the flag to finish processing is set");

    // Close the final cube
    error = this->close_mex(_camera_info);
    if (error != NO_ERROR){
      message << "ERROR there was a problem closing the FITS mex, error code: "
              << error;
      logwrite(function, message.str());
      message.str("");
    }

    // Log that the thread is complete and set the flag
    message << "stopping mex frame writing thread, total frames written: "
            << this->total_frames;
    logwrite(function, message.str());
    this->mex_thread_running = false;
  }
  /**************** FITS_file::write_mex_thread ****************/


  /**************** FITS_file::make_header ****************/
  /**
   Creates the primary FITS file header, which should have an extensive amount
   of information about the file.  The function looks for header file stubs that
   are created in the common_info.status_dir directory by the various systems
   and uses them to create the header file.
   \param [filename] The base name of the image
   \param [timestamp] The timestamp when the image was taken
   \param [sequence] A sequence number if the image is one in a sequence
   \param [camera_info] The camera_info class holds image parameters such as size,
                        pixel scales, detector information
   \note None.
   */
  void make_header(std::string filename, std::string timestamp, int sequence,
                   Camera::Information & _camera_info)
  {
    // Set the function information for logging
    std::string function("FITS_file::make_header");
    std::stringstream message;

    // Set these to the bad value, that'a default that will be changed if the
    // telescope information is passed in when reading the header stub files
    this->telrad = 9999;
    this->teldecd = 9999;

    // Write the header keys to the FITS header
    try {
      // Iterate through the system-defined FITS keyword databases and add them to the primary header.
      //
      Common::FitsKeys::fits_key_t::iterator keyit;
      for (keyit  = _camera_info.systemkeys.keydb.begin();
           keyit != _camera_info.systemkeys.keydb.end();
           keyit++) {
        this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
      }

      // If specified, iterate through the user-defined FITS keyword databases and add them to the primary header.
      //
      if ( _camera_info.writekeys_before ) {
        logwrite( function, "writing user-defined keys before exposure" );
        for (keyit  = _camera_info.userkeys.keydb.begin();
             keyit != _camera_info.userkeys.keydb.end();
             keyit++) {
          this->add_key( true, keyit->second.keyword, keyit->second.keytype, keyit->second.keyvalue, keyit->second.keycomment );
        }
      }

/**********
      // Add timestamp for the image
      this->pFits->pHDU().addKey("DATE-OBS", timestamp, "Time of observation");

      // Put the sequence into the header if this is one of multiple FITS files
      // that are part of the same observation
      if (sequence >= 0){
        this->pFits->pHDU().addKey("SEQUENCE", sequence, "Sequence number");
      }

      // Write information from camera_info structure into header
      this->pFits->pHDU().addKey("DETECTOR", camera_info.detector,
                                 "Detector controller");
      this->pFits->pHDU().addKey("DETSOFT", camera_info.detector_software,
                                 "Detector software version");
      this->pFits->pHDU().addKey("DETFIRM", camera_info.detector_firmware,
                                 "Detector firmware version");
      this->pFits->pHDU().addKey("EXPTIME", camera_info.exposure_time,
                                 "Exposure Time");
      this->pFits->pHDU().addKey("MODE_NUM", camera_info.current_observing_mode,
                                 "Mode identifying key");
      message << camera_info.binning[0] << " " << camera_info.binning[1];
      this->pFits->pHDU().addKey("DETSUM", message.str(), "DET binning");
      message.str("");
      this->pFits->pHDU().addKey("DET_ID", camera_info.det_id,
                                 "ID value of detector");
      this->pFits->pHDU().addKey("DETNAME", camera_info.det_name,
                                 "Detector name or serial number");
      this->pFits->pHDU().addKey("PIXSCALE", camera_info.pixel_scale,
                                 "Pixel scale, in arcsec per pixel");
      this->pFits->pHDU().addKey("FILENAME", filename, "File name");
      this->pFits->pHDU().addKey("ORIGNAME", filename, "Original file name");
      this->pFits->pHDU().addKey("FRAMENUM", camera_info.framenum,
                                 "Detector frame number");
**********/
    }
    // Catch any errors from the FITS system and log them
    catch (CCfits::FitsError & err) {
      message << "ERROR creating FITS header: " << err.message();
      logwrite(function, message.str());
    }
  }
  /**************** FITS_file::make_header ****************/


  /**************** FITS_file::make_mex_header ****************/
  /**
   Writes the FITS header for image extensions in a data cube.  Each cube should
   have a header, which is limited to just the information required for the
   individual mex images; information common to all of the images (such as
   telescope coordinates or sensor data) should be written in the primary
   header.
   \param [imageExt] The FITS image extension being written
   \param [timestamp] The timestamp when the image was taken
   \param [camera_info] The camera_info class holds image parameters such as
      size, pixel scales, detector information
   \note None.
   */
  void make_mex_header(std::string & timestamp, Camera::Information & _camera_info)
  {
    // Set the function information for logging
    std::string function("FITS_file::make_mex_header");
    std::stringstream message;
    std::stringstream temp;

    try {
      // I think this is wrong but it's what NIRC2 wants. Should be == USHORT_IMG
      if ( _camera_info.datatype == SHORT_IMG ) {
        this->imageExt->addKey("BZERO", 32768, "offset for signed short int");
        this->imageExt->addKey("BSCALE", 1, "scaling factor");
      }
      else {
        this->imageExt->addKey("BZERO", 0.0, "offset");
        this->imageExt->addKey("BSCALE", 1, "scaling factor");
      }

//      for ( const auto &[key,val] : _camera_info.systemkeys.keydb ) {
//        this->add_extension_key( val.keyword, val.keytype, val.keyvalue, val.keycomment );
//message.str(""); message << "[RICKY] SYSTEM: " << val.keyword << "=" << val.keyvalue;
//logwrite(function,message.str());
//      }

      for ( const auto &[key,val] : _camera_info.extkeys.keydb ) {
        this->add_extension_key( val.keyword, val.keytype, val.keyvalue, val.keycomment );
//message.str(""); message << "[RICKY] EXT: " << val.keyword << "=" << val.keyvalue;
//logwrite(function,message.str());
      }

/**********
      // Write the timestamp of observation for the image extension.
      this->imageExt->addKey("UTC", timestamp, "Time of observation");

      // Write information about the detector and amplifier
      this->imageExt->addKey("DET_ID", camera_info.det_id,
                             "ID value of detector");
      this->imageExt->addKey("DETNAME", camera_info.det_name,
                             "Detector name or serial number");
      this->imageExt->addKey("AMP_ID", camera_info.amp_id,
                             "ID value of amplifier");
      this->imageExt->addKey("AMP_NAME", camera_info.amp_name,
                             "Name of amplifier");
      this->imageExt->addKey("GAIN", camera_info.det_gain, "Gain e-/adu");
      this->imageExt->addKey("READNOI", camera_info.read_noise,
                             "Read noise e-");
      this->imageExt->addKey("DARKCUR", camera_info.dark_current,
                             "Dark current e-/s @ 150 K");

      // Write the detector information
      temp << "[" << camera_info.detsize[0] << ":"
           << camera_info.detsize[1] << ","
           << camera_info.detsize[2] << ":"
           << camera_info.detsize[3] << "]";
      this->imageExt->addKey("DETSIZE", temp.str(), "detector size (pixels)");
      temp.str("");

      temp << "[" << camera_info.ccdsec[0] << ":"
           << camera_info.ccdsec[1] << ","
           << camera_info.ccdsec[2] << ":"
           << camera_info.ccdsec[3] << "]";
      this->imageExt->addKey("CCDSEC", temp.str(), "Detector section");
      temp.str("");

      temp << "[" << camera_info.region_of_interest[0] << ":"
           << camera_info.region_of_interest[1] << ","
           << camera_info.region_of_interest[2] << ":"
           << camera_info.region_of_interest[3] << "]";
      this->imageExt->addKey("ROISEC", temp.str(), "Region of interest");
      temp.str("");

      temp << "[" << camera_info.ampsec[0] << ":"
           << camera_info.ampsec[1] << ","
           << camera_info.ampsec[2] << ":"
           << camera_info.ampsec[3] << "]";
      this->imageExt->addKey("AMPSEC", temp.str(), "Amplifier section");
      temp.str("");

      temp << "[" << camera_info.trimsec[0] << ":"
           << camera_info.trimsec[1] << ","
           << camera_info.trimsec[2] << ":"
           << camera_info.trimsec[3] << "]";
      this->imageExt->addKey("TRIMSEC", temp.str(), "Trim section");
      temp.str("");

      temp << "[" << camera_info.datasec[0] << ":"
           << camera_info.datasec[1] << ","
           << camera_info.datasec[2] << ":"
           << camera_info.datasec[3] << "]";
      this->imageExt->addKey("DATASEC", temp.str(), "Data section");
      temp.str("");

      temp << "[" << camera_info.biassec[0] << ":"
           << camera_info.biassec[1] << ","
           << camera_info.biassec[2] << ":"
           << camera_info.biassec[3] << "]";
      this->imageExt->addKey("BIASSEC", temp.str(), "Bias section");
      temp.str("");

      temp << "[" << camera_info.detsec[0] << ":"
           << camera_info.detsec[1] << ","
           << camera_info.detsec[2] << ":"
           << camera_info.detsec[3] << "]";
      this->imageExt->addKey("DETSEC", temp.str(), "Detector section");
      temp.str("");

      // Add the time that the FITS frame was added to the cube
      this->imageExt->addKey("DATE", get_timestamp(), "FITS frame write date");
**********/
    }
    // Catch any errors from the FITS system and log them
    catch (CCfits::FitsError & err) {
      message << "ERROR creating FITS header: " << err.message();
      logwrite(function, message.str());
    }
  }
  /**************** FITS_file::make_mex_header ****************/


  void add_primary_key( std::string keyword, std::string type, std::string value, std::string comment ) {
    add_key( true, keyword, type, value, comment );
  }
  void add_extension_key( std::string keyword, std::string type, std::string value, std::string comment ) {
    add_key( false, keyword, type, value, comment );
  }
  void add_key( bool isprimary, std::string keyword, std::string type, std::string value, std::string comment ) {
    const std::string function("FITS_file::add_key");
    std::stringstream message;

    try {
      if (type.compare("BOOL") == 0) {
        bool boolvalue = ( value == "T" ? true : false );
        ( isprimary ? this->pFits->pHDU().addKey( keyword, boolvalue, comment )
                    : this->imageExt->addKey( keyword, boolvalue, comment ) );
      }
      else if (type.compare("INT") == 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, std::stoi(value), comment)
                    : this->imageExt->addKey( keyword, std::stoi(value), comment ) );
      }
      else if (type.compare("LONG") == 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, std::stol(value), comment)
                    : this->imageExt->addKey( keyword, std::stol(value), comment ) );
      }
      else if (type.compare("FLOAT") == 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, std::stof(value), comment)
                    : this->imageExt->addKey( keyword, std::stof(value), comment ) );
      }
      else if (type.compare("DOUBLE") == 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, std::stod(value), comment)
                    : this->imageExt->addKey( keyword, std::stod(value), comment ) );
      }
      else if (type.compare("STRING") == 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
      }
      else {
        message.str(""); message << "ERROR unknown type: " << type << " for user keyword: " << keyword << "=" << value
                                 << ": expected {INT,LONG,FLOAT,DOUBLE,STRING,BOOL}";
        logwrite(function, message.str());
      }
    }
    // There could be an error converting a value to INT or FLOAT with stoi or stof,
    // in which case save the keyword as a STRING.
    //
    catch ( std::invalid_argument & ) {
      message.str(""); message << "ERROR: unable to convert value " << value;
      logwrite( function, message.str() );
      if (type.compare("STRING") != 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
      }
    }
    catch ( std::out_of_range & ) {
      message.str(""); message << "ERROR: value " << value << " out of range";
      logwrite( function, message.str() );
      if (type.compare("STRING") != 0) {
        ( isprimary ? this->pFits->pHDU().addKey(keyword, value, comment)
                    : this->imageExt->addKey( keyword, value, comment ) );
      }
    }
    catch (CCfits::FitsError & err) {
      message.str(""); message << "ERROR adding key " << keyword << "=" << value << " / " << comment << " (" << type << ")"
                               << " to " << ( isprimary ? "primary" : "extension" ) << " :"
                               << err.message();
      logwrite(function, message.str());
    }
//message.str(""); message << "[TESTTEST] added " << (isprimary?"pri":"ext") << " key " << keyword << "=" << value << " // " << comment;
//logwrite(function, message.str());
  }


public:


  /**************** FITS_file::FITS_file ****************/
  /**
   This function constructs the FITS file class
   */
  FITS_file(bool mex_state_in = false)
  {
    // Initialize the class
    this->initialize_class();

    // Set flag to make this FITS object output cubes
    this->ismex = mex_state_in;
  };
  /**************** FITS_file::FITS_file ****************/


  /**************** FITS_file::~FITS_file ****************/
  /**
   This is the destructor for the FITS file class.
   */
  ~FITS_file(){
  };
  /**************** FITS_file::~FITS_file ****************/


  /**************** FITS_file::FITS_file ****************/
  /**
   This is the copy constructor for the FITS file class.
   */
  FITS_file(const FITS_file & in_fitsfile)
  {
    this->swap(*this, in_fitsfile);
  }
  /**************** FITS_file::FITS_file ****************/


  /**************** FITS_file::FITS_file ****************/
  /**
   This is the assignment constructor for the FITS file class.
   */
  FITS_file & operator = (FITS_file & in_fitsfile)
  {
    this->swap(*this, in_fitsfile);

    return *this;
  }
  /**************** FITS_file::FITS_file ****************/


  /**************** FITS_file::add_user_key ***************/
  /**
   Add a user-defined keyword to the primary header of an open FITS file.
   This is invoked by a "FITS:" entry in the .acf file. All parameters
   are passed in as strings.
   \param [keyword] the keyword
   \param [type]    type of FITS key (INT, REAL, STRING)
   \param [value]   value of FITS key
   \param [comment] comment
   */
  void add_user_key(std::string keyword, std::string type, std::string value,
                    std::string comment) {
    std::string function("FITS_file::add_user_key");
    std::stringstream message;

    // Add the type based on the kind of keyword sent, integer, float, string
    if (type.compare("INT") == 0) {
      this->pFits->pHDU().addKey(keyword, atoi(value.c_str()), comment);
    }
    else if (type.compare("REAL") == 0) {
      this->pFits->pHDU().addKey(keyword, atof(value.c_str()), comment);
    }
    else if (type.compare("STRING") == 0) {
      this->pFits->pHDU().addKey(keyword, value, comment);
    }
    // If the type us unknown, log an error and don't write the keyword
    else {
      message << "ERROR unknown type: " << type << " for user keyword: " << keyword
              << "=" << value << " / " << comment;
      logwrite(function, message.str());
    }
  }
  /**************** FITS_file::add_user_key ***************/


  /**************** FITS_file::write_image ****************/
  /**
   Main image writing function.  This gets called anytime that a FITS image is
   written into the FITS file.  It has options to write a full image, or to
   add a frame to a data cube.
   \param [data] The image data array (images are one dimensional)
   \param [timestamp] The timestamp when the image was taken
   \param [sequence] A sequence number if the image is one in a sequence
   \param [camera_info] The camera_info class holds image parameters such as size,
                        pixel scales, detector information
   \param [compress] FITS compression to use on the data file
   \return ERROR if there is a failure, NO_ERROR on success
   */
  int write_image(T * data, std::string timestamp, int sequence,
                  Camera::Information _camera_info,
                  int compress = FITS_COMPRESSION_NONE)
  {
    std::string function("FITS_file::write_image");
    std::stringstream message;
    int error;
// std::cout << "*** sequence="<<sequence << " *** extension=" << camera_info.extension << "\n";
    // Write into the data cube
    logwrite(function, "[DEBUG] writing type "+demangle(typeid(T).name()));
    if (this->ismex == true){

      // Open the file if it's not already open
      if (this->file_open == false  && this->mex_thread_running == false){
        logwrite(function, "opening the mex file for writing...");
        error = this->open_file(_camera_info, timestamp, sequence, compress);
        if (error != NO_ERROR){
          message << "ERROR failed to open FITS file \"" << this->fits_name <<"\"";
          logwrite(function, message.str());
          return(ERROR);
        }
      }

      // Create the FITS frame object from the input data
      FITS_mex_frame <T> frame(data, _camera_info.section_size, timestamp,
                                sequence, _camera_info);

      boost::unique_lock<boost::timed_mutex> lock(this->cache_mutex);
      this->mex_cache.push_back(frame);
//    boost::timed_mutex *m = lock.release();
//    m->unlock();
    }

    // Write a single frame FITS image
    else {
      error = this->write_single_image(data, timestamp, sequence, _camera_info,
                                       compress);
      if (error != NO_ERROR){
        message << "ERROR failed to write FITS file " << this->fits_name;
        logwrite(function, message.str());
        return(ERROR);
      }
    }

    // Return success.  Note there is no logging, data cubes would create a huge
    // number of log files so only errors are written while writing FITS files.
    return(NO_ERROR);
  }
  /**************** FITS_file::write_image ****************/


  /**************** FITS_file::complete ****************/
  /**
   Completes a FITS data cube; this closes the file and shuts down processing
   for the data cube.
   */
  void complete()
  {
    std::string function("FITS_file::complete");
    std::stringstream message;

    // Set the flag that tells the system the last image has been delivered
    this->last_image = true;

    // Log the cube is flagged as complete
    logwrite(function, "completion signal sent");

    // Wait for the cube thread to finish
    this->fits_mex_thread.join();

    // Log that the mex is closed and everything is good
    logwrite(function, "FITS mex processing complete");
  }
  /**************** FITS_file::complete ****************/


  /**************** FITS_file::get_ismex ****************/
  /**
   Return data cube flag value, true if this is FITS container is a data cube
   and false if it is not.
   */
  bool get_ismex()
  {
    return(this->ismex);
  }
  /**************** FITS_file::get_ismex ****************/

};
