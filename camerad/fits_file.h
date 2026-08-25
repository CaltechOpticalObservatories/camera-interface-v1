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

/** \class FITS_cube_frame
 \brief FITS cube frame object
 \details This class contains the information that goes into a frame of a data
 cube.  These are put into a vector and written out by the FITS_file class in
 its cube writing thread.
 */
template <class T>
class FITS_cube_frame
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


  /**************** FITS_file::FITS_cube_frame ****************/
  /**
   This function constructs the FITS_cube_frame class.  Note that the array is
   automatically filled by the valarray constructor when this class is
   constructed.
   */
  FITS_cube_frame(T * data, int size, std::string timestamp_in, int sequence_in,
                  Camera::Information camera_info_in): array(data, size)
  {
    // Copy the camera info from the input
    this->camera_info = camera_info_in;
    
    // Copy the timestamp from the input
    this->timestamp = timestamp_in;
    
    // Copy the sequence number from the input
    this->sequence = sequence_in;
  };
  /**************** FITS_file::FITS_cube_frame ****************/

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
  
  /** \var boost::thread fits_cube_thread
   \details Boost thread variable for the cube writing thread */
  boost::thread fits_cube_thread;

  /** \var bool run_cube_thread
   \details Flag that the FITS file is open for writing */
  bool run_cube_thread;
  
  /** \var bool iscube
   \details Flag that this FITS object writes data cubes */
  bool iscube;
  
  /** \var std::vector<FITS_cube_frame <T> > cube_frames
   \details Queue containing the information for each frame of the data cube */
  std::deque<FITS_cube_frame <T> > cube_frames;

  /** \var std::vector<FITS_cube_frame <T> > cube_cache
   \details Cache of incoming frames that will be written to a data cube */
  std::deque<FITS_cube_frame <T> > cube_cache;

  /** \var bool cube_thread_running
   \details Flag the the thread writing the cubes is running */
  bool cube_thread_running;
  
  /** \var long cube_size
   \details The size of the data cube in bytes (more or less) */
  long cube_size;
  
  /** \var bool last_image
   \details Flag that the last image has been delivered to the FITS cube */
  bool last_image;
  
  /** \var long total_frames
   \details Number of frames written for all data cubes processed */
  long total_frames;

  /** \var int max_size
   \details This is the max allowed size for any single data cube */
  int max_size;
  
  /** \var int max_cube_frames
   \details This is the max allowed number of frames that can be written to any
   single data cube */
  int max_cube_frames;

  /** \var     in_process
   *  \details add this extension while fits writing is in process, remove on close */
  inline static const std::string in_process = ".writing";


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
    std::swap(first.fits_cube_thread, second.fits_cube_thread);
    std::swap(first.run_cube_thread, second.run_cube_thread);
    std::swap(first.iscube, second.iscube);
    std::swap(first.cube_frames, second.cube_frames);
    std::swap(first.cube_cache, second.cube_cache);
    std::swap(first.cube_thread_running, second.cube_thread_running);
    std::swap(first.cube_size, second.cube_size);
    std::swap(first.last_image, second.last_image);
    std::swap(first.total_frames, second.total_frames);
    std::swap(first.max_size, second.max_size);
    std::swap(first.max_cube_frames, second.max_cube_frames);
  }
  /**************** FITS_file::swap ****************/
  
  
  /**************** FITS_file::print_compression ****************/
  /**
   Prints a message to show what compression is being used
   */
  std::string print_compression(int compression)
  {
    std::stringstream message;  // Temporary string for compression message
    
    if (compression == FITS_COMPRESSION_NONE){
      message << "(" << FITS_COMPRESSION_NONE << ":FITS_COMPRESSION_NONE)";
    }
    else if (compression == FITS_COMPRESSION_RICE){
      message << "(" << FITS_COMPRESSION_RICE << ":FITS_COMPRESSION_RICE)";
    }
    else if (compression == FITS_COMPRESSION_GZIP){
      message << "(" << FITS_COMPRESSION_GZIP << ":FITS_COMPRESSION_GZIP)";
    }
    else if (compression == FITS_COMPRESSION_PLIO){
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
    this->iscube = false;
    this->run_cube_thread = false;
    this->cube_thread_running = false;
    this->cube_size = 0;
    this->last_image = false;
    this->total_frames = 0;
    this->max_size = MAX_IMAGE_DATA_SIZE;
    this->max_cube_frames = 10000;
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
  int open_file(Camera::Information & camera_info,
                std::string & timestamp, int sequence,
                int compress = FITS_COMPRESSION_NONE)
  {
    // Set the function information for logging
    std::string function("FITS_file::open_file");
    std::stringstream message;
    
    message << "opening FITS file image for " << camera_info.image_name;
    logwrite(function, message.str());
    message.str("");
    
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
      
      // If the image is a data cube, we don't want to write anything into the
      // primary image, so set the axes to 0 to avoid allocating the space. If
      // this is a single frame image, then we need to allocate the space,
      // unless it is compressed in which case we also set the primary image
      // to a null image.
      long naxes[2];     // Local variable of image axes size
      int num_axis = 2;     // Set the number of axes to 2, even on data cubes
      if (this->iscube == true){
        naxes[0] = 0;
        naxes[1] = 0;
        num_axis = 0;
      }
      else {
        naxes[0] = camera_info.naxes[0];
        naxes[1] = camera_info.naxes[1];
        num_axis = 2;
      }
      
      this->fits_name = camera_info.fits_name + this->in_process;


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
      this->pFits.reset( new CCfits::FITS(this->fits_name, camera_info.bitpix,
                                          num_axis, naxes) );
      
      // Create the primary image header
      this->make_header(this->fits_name.substr(camera_info.directory.length()+1),
                          timestamp, sequence, camera_info);
      
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

    // If this is a data cube, start the cube writing thread and set initial
    // cube parameters
    if (this->iscube == true){
      this->cube_size = 0;
      if (this->cube_thread_running == false){
        this->open_info = camera_info;
        this->open_sequence = sequence;
        this->open_timestamp = timestamp;
        this->run_cube_thread = true;
        this->fits_cube_thread = boost::thread(&FITS_file::write_cube_thread,
                                               this);
      }
    }

    // Write a log message and return a success value
    message << "opened FITS file " << this->fits_name << " with compression "
            << print_compression(this->compression);
    logwrite(function, message.str());
    return(NO_ERROR);
  }
  /**************** FITS_file::open_file ****************/


  /**************** FITS_file::close_file ****************/
  /**
   Closes a FITS image file; this version is meant to close a single frame file.
   */
  int close_file()
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

    // Write the checksum
    this->pFits->pHDU().writeChecksum();
    
    // Deallocate the CCfits object and close the FITS file
    this->pFits->destroy();

    // Reset flags to prepare for the next file
    this->file_open = false;

    // Log that the file closed successfully
    message << "successfully closed FITS file " << this->fits_name;
    logwrite(function, message.str());
    return(NO_ERROR);
  }
  /**************** FITS_file::close_file ****************/


  /**************** FITS_file::close_cube ****************/
  /**
   Close a FITS cube file.
   */
  int close_cube()
  {
    // Set the function information for logging
    std::string function("FITS_file::close_file");
    std::stringstream message;

    if (this->file_open == false){
      logwrite(function, "FITS cube file already closed");
      return(NO_ERROR);
    }
    
    // Log what we're doing
    message << "closing FITS data cube " << this->fits_name;
    logwrite(function, message.str());
    
    // Stop the cube writing thread only after the final image is received and
    // processed
    if (this->last_image == true && this->cube_frames.size() == 0 &&
        this->cube_cache.size() == 0){
      logwrite(function, "closing the last cube file...");
      this->run_cube_thread = false;
    }

    // Add a header keyword for the time the file was written (right now!), the
    // number of frames written into the cube, and the time the system stopped
    // taking cube data
    this->pFits->pHDU().addKey("NFRAMES", this->num_frames,
                               "number of frames in FITS file");
    this->pFits->pHDU().addKey("DATE", get_timestamp(), "FITS file write date");

    // Write the checksum
    this->pFits->pHDU().writeChecksum();
    
    // Deallocate the CCfits object and close the FITS file
    this->pFits->destroy();

    // Reset flags to prepare for the next file
    this->file_open = false;

    // Log that the file closed successfully
    message.str("");
    message << "successfully closed FITS data cube " << this->fits_name
            << ", wrote " << this->num_frames << " cube frames and "
            << this->cube_size << " image bytes, frames waiting: "
            << this->cube_frames.size() << " " << this->cube_cache.size();
    logwrite(function, message.str());

    //
    // Rename the file to remove the in_process extension
    //

    std::string finished_file { this->fits_name, 0, this->fits_name.rfind( this->in_process ) };
    std::filesystem::path in_process_path( this->fits_name );
    std::filesystem::path finished_path( finished_file );

    try {
      std::filesystem::rename( in_process_path, finished_path );
      logwrite(function, "renamed "+this->fits_name+" to "+finished_file);
    }
    catch ( const std::filesystem::filesystem_error &e ) {
      logwrite(function, "ERROR renaming "+this->fits_name+" to "+finished_file+": "+std::string(e.what()));
    }

    return(NO_ERROR);
  }
  /**************** FITS_file::close_cube ****************/


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
                         Camera::Information camera_info,
                         int compress = FITS_COMPRESSION_NONE)
  {
    // Set the function information for logging
    std::string function("FITS_file::write_single_image");
    std::stringstream message;

    // Set the FITS system to verbose mode so it writes error messages
    CCfits::FITS::setVerboseMode(true);
    
    // Open the FITS file
    if (this->open_file(camera_info, timestamp, sequence, compress) != NO_ERROR){
      message << "ERROR failed to open FITS file \"" << camera_info.image_name << "\", aborting";
      logwrite(function, message.str());
      return(ERROR);
    }

    try {
      // Move the data into a valarray, necessary to wite it using CCFITS
      std::valarray<T> array(data, camera_info.image_size);

      // Set the first pixel value to 1, this tells the FITS system where it
      // starts writing (we don't start at anything but 1)
      long fpixel = 1;
      
      // Write the primary image into the FITS file if compression is off
      if (this->compression == FITS_COMPRESSION_NONE){
        this->pFits->pHDU().write(fpixel, camera_info.image_size, array);
      }
      // With compression, write to an extension
      else {
        CCfits::ExtHDU& pseudo_primary = this->pFits->extension(1);
        pseudo_primary.write(fpixel, camera_info.image_size, array);
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
    if (this->close_file() != NO_ERROR){
      message << "ERROR failed to close FITS file properly: " << this->fits_name;
      logwrite(function, message.str());
      return(ERROR);
    }
    
    return(NO_ERROR);
  }
  /**************** FITS_file::write_single_image ****************/
  
  
  /**************** FITS_file::write_cube_thread ****************/
  /** Writes FITS data cubes.  This is run in a thread, started when the FITS
   file is opened, and runs until the final image is received.  If the data
   cube goes over the maximum allowed size, the thread closed the cube and
   creates a new one with the same name.
   */
  void write_cube_thread()
  {
    std::string function("FITS_file::write_cube_thread");
    std::stringstream message;
    int error;
    
    // If the thread is already flagged as running, stop here
    if (this->cube_thread_running == true){
      logwrite(function, "thread is already running, stopping!");
      return;
    }
    
    // Set the thread running flag
    this->cube_thread_running = true;
    
    // Log that the thread is starting
    logwrite(function, "starting thread to write cube frames...");

    // Set some initial values for parameters used in writing the thread
    bool finished = false;
    this->cube_size = 0;
    this->last_image = false;
    this->total_frames = 0;

    // Loop until the flag to finish processing is set
    while (this->run_cube_thread == true && finished == false){
      
      // Pull data out of the cube cache if there is something in it.  This is
      // done here to avoid the writing process slowing down the process putting
      // images in the cache.
      if (this->cube_cache.size() > 0){
        
        // Pull a maximum of 5 images over.  This limits how much the system is
        // accessing the cache to avoid slowing the writing process.
        int size = this->cube_cache.size();
        if (size > 5){
          size = 5;
        }
        
        // Get the front image out of the queue, then clear it.  The mutex is
        // locked while clearing.  There are two options here, do it one image
        // at a time, or shift all five and then delete all at once.  This does
        // the former to lock the mutex for as short of a time as possible for
        // each image.
        try {
          for (int i = 0; i < size; i++) {
            if (this->cache_mutex.try_lock_for(boost::chrono::milliseconds(1))){
              boost::lock_guard<boost::timed_mutex> lock(this->cache_mutex,
                                                         boost::adopt_lock_t());
              this->cube_frames.push_back(this->cube_cache[0]);
              this->cube_cache.pop_front();
            }
          }
        }
        catch ( std::exception &e ) {
          message.str(""); message << "ERROR exception occured: " << e.what();
          logwrite( function, message.str() );
        }
      }
      
      // Don't do anything if there are no frames available
      if (this->cube_frames.size() == 0){
        // Time out to save resources, this should be fast enough for data
        // acquisition expected by this system
        timeout(0.00001);
        // Check flags and set an exit flag if the image process is complete and
        // all images have been written out.
        if (this->last_image == true && this->cube_frames.size() == 0 &&
            this->cube_cache.size() == 0){
          finished = true;
        }
        continue;
      }
      
      // Open the cube file if it's not already open
      if (this->file_open == false){
        logwrite(function, "opening a new cube file...");
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

      // Write the image to the cube file
      try {
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
                                        this->cube_frames[0].camera_info.bitpix,
                                        this->cube_frames[0].camera_info.naxes);
        // Make the image cube header
        this->make_cube_header(this->imageExt, this->cube_frames[0].timestamp,
                               this->cube_frames[0].camera_info);

        // Write the image extension into the cube
        this->imageExt->write(fpixel, this->cube_frames[0].camera_info.image_size,
                              this->cube_frames[0].array);

        // Flush the FITS container to make sure the image is written to disk
        this->pFits->flush();

        #ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] wrote extension " << this->cube_frames[0].camera_info.extension;
        logwrite( function, message.str() ); message.str("");
        #endif
        
        // Increment the frame counter
        this->num_frames++;
        this->total_frames++;

        // Add size of the new frame to the size of the cube
        this->cube_size += this->cube_frames[0].camera_info.image_memory;
        
        this->cube_frames.pop_front();
      }
      // Catch any errors from the FITS system and log them
      catch (CCfits::FitsError & error){
        message << "ERROR FITS file error thrown: " << error.message();
        logwrite(function, message.str());
        message.str("");
      }

      // For cubes with lots of files, log the progress for writing the cube
      if (this->num_frames % 1000 == 0){
        message << "number of frames written: " << this->num_frames << " size: "
                << this->cube_size << " bytes, frames waiting: "
                << this->cube_frames.size() << " " << this->cube_cache.size();
        logwrite(function, message.str());
        message.str("");
      }
      
/******** temporarily (?) disable size and frame limit

      // If the cube has grown too large, close it
      if (this->cube_size >= this->max_size ||
          this->num_frames >= this->max_cube_frames){
        logwrite(function, "closing the cube file...");
        #ifdef LOGLEVEL_DEBUG
        message << "[DEBUG] cube_size=" << this->cube_size << " max_size=" << this->max_size
                << " num_frames=" << this->num_frames << " max_frames=" << this->max_cube_frames;
        logwrite( function,  message.str() ); message.str("");
        #endif
        error = this->close_cube();
        if (error != NO_ERROR){
          message << "ERROR there was a problem closing the FITS cube, error code: "
                  << error;
          logwrite(function, message.str());
          message.str("");
        }
      }
********/
      
      // If the flag for a complete observation is set, and if the cache and
      // cube queues are empty, flag that the system can finish.
      if (this->last_image == true && this->cube_frames.size() == 0 &&
          this->cube_cache.size() == 0){
        finished = true;
        logwrite(function, "flag set to finish writing the data cube");
      }
    }
    
    // Close the final cube
    error = this->close_cube();
    if (error != NO_ERROR){
      message << "ERROR there was a problem closing the FITS cube, error code: "
              << error;
      logwrite(function, message.str());
      message.str("");
    }

    // Log that the thread is complete and set the flag
    message << "stopping cube frame writing thread, total frames written: "
            << this->total_frames;
    logwrite(function, message.str());
    this->cube_thread_running = false;
  }
  /**************** FITS_file::write_cube_thread ****************/


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
                   Camera::Information & camera_info)
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
      this->pFits->pHDU().addKey("EXPOSURE", camera_info.exposure_time.value(),
                                 "Total Exposure Time ("+camera_info.exposure_time.unit()+")");
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
    }
    // Catch any errors from the FITS system and log them
    catch (CCfits::FitsError & err) {
      message << "ERROR creating FITS header: " << err.message();
      logwrite(function, message.str());
    }
  }
  /**************** FITS_file::make_header ****************/


  /**************** FITS_file::make_cube_header ****************/
  /**
   Writes the FITS header for image extensions in a data cube.  Each cube should
   have a header, which is limited to just the information required for the 
   individual cube images; information common to all of the images (such as 
   telescope coordinates or sensor data) should be written in the primary 
   header.
   \param [imageExt] The FITS image extension being written
   \param [timestamp] The timestamp when the image was taken
   \param [camera_info] The camera_info class holds image parameters such as
      size, pixel scales, detector information
   \note None.
   */
  void make_cube_header(CCfits::ExtHDU * imageExt, std::string & timestamp,
                        Camera::Information & camera_info)
  {
    // Set the function information for logging
    std::string function("FITS_file::make_cube_header");
    std::stringstream message;
    std::stringstream temp;

    try {
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
    }
    // Catch any errors from the FITS system and log them
    catch (CCfits::FitsError & err) {
      message << "ERROR creating FITS header: " << err.message();
      logwrite(function, message.str());
    }
  }
  /**************** FITS_file::make_cube_header ****************/
  
  
public:


  /**************** FITS_file::FITS_file ****************/
  /**
   This function constructs the FITS file class
   */
  FITS_file(bool cube_state_in = false)
  {
    // Initialize the class
    this->initialize_class();

    // Set flag to make this FITS object output cubes
    this->iscube = cube_state_in;
    logwrite("FITS_file::FITS_file","constructed");
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
                  Camera::Information camera_info,
                  int compress = FITS_COMPRESSION_NONE)
  {
    std::string function("FITS_file::write_image");
    std::stringstream message;
    int error;
    
    // Write into the data cube
    if (this->iscube == true){
      
      // Open the file if it's not already open
      if (this->file_open == false  && this->cube_thread_running == false){
        logwrite(function, "opening the cube file for writing...");
        error = this->open_file(camera_info, timestamp, sequence, compress);
        if (error != NO_ERROR){
          message << "ERROR failed to open FITS file \"" << this->fits_name <<"\"";
          logwrite(function, message.str());
          return(ERROR);
        }
      }

      // Create the FITS frame object from the input data
      FITS_cube_frame <T> frame(data, camera_info.image_size, timestamp,
                                sequence, camera_info);

      boost::unique_lock<boost::timed_mutex> lock(this->cache_mutex);
      this->cube_cache.push_back(frame);
      boost::timed_mutex *m = lock.release();
      m->unlock();
    }

    // Write a single frame FITS image
    else {
      error = this->write_single_image(data, timestamp, sequence, camera_info,
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
    std::string function("FITS_file::write_image");
    std::stringstream message;

    // Set the flag that tells the system the last image has been delivered
    this->last_image = true;
    
    // Log the cube is flagged as complete
    logwrite(function, "completion signal sent");

    // Wait for the cube thread to finish
    this->fits_cube_thread.join();

    // Log that the cube is closed and everything is good
    logwrite(function, "FITS cube processing complete");
  }
  /**************** FITS_file::complete ****************/


  /**************** FITS_file::get_iscube ****************/
  /**
   Return data cube flag value, true if this is FITS container is a data cube
   and false if it is not.
   */
  bool get_iscube()
  {
    return(this->iscube);
  }
  /**************** FITS_file::get_iscube ****************/

};
