#pragma once

namespace Camera {

  /***** Camera::Shutter ******************************************************/
  /**
   * @class    Camera
   * @brief    This class interfaces the Bonn shutter control port to a serial port.
   * @details  This class uses ioctl calls to read and write RS232 signals, normally
   *           used for handshaking. RTS(7) drives the shutter open pin 7, where
   *           pin 8 is tied to GND(5). Blade A (1) and B (2) status pins are open
   *           collector outputs that are pulled up to +5V (6) through 1k and are
   *           read by DSR(6) and CTS(8), respectively. Similarly, the error pin (4)
   *           is read by DCD(1). Consequently, these are active-LO outputs.
   *           Since the computer has no RS232 port, a USB-RS232 converter is used.
   *
   *           If is_enabled is cleared (false) then everything goes through the
   *           motions except do not send the ioctl commands to actually operate
   *           the shutter. This allows conditions that might wait on shutter open
   *           and close to behave as normal. The only difference is that the
   *           mechanism doesn't move.
   *
   */
  class Shutter {
    private:
      int state;    //!< shutter open state: 1=open, 0=closed, -1=uninitialized
      int RTS_bit;  //!< bitmask representing RTS line
      int fd;
      std::chrono::time_point<std::chrono::high_resolution_clock> open_time, close_time;
      double duration_sec;
    public:
      std::condition_variable condition;
      std::mutex lock;
      bool is_enabled;  //!< is shutter enabled?

      inline bool isopen()   const { return ( this->state==1 ? true : false ); }
      inline bool isclosed() const { return ( this->state==0 ? true : false ); }
      inline void arm()            { this->state = -1; }

      /***** Camera::Shutter:init *********************************************/
      /*
       * @brief      initialize the Shutter class
       * @details    Shutter class must be initialized before use.
       *             This opens a connection to the USB device for the USB-RS232
       *             serial converter. This requires a proper udev rule to assign
       *             the appropriate USB device to /dev/shutter
       * @return     ERROR or NO_ERROR
       *
       */
      long init()  {
        std::string function = "Camera::Shutter::init";
        long error = ERROR;
        int state = -1;

        // close any open fd
        //
        if ( this->fd>=0 ) { close( this->fd ); this->fd=-1; }

        // open the USB device
        //
        this->fd = open( "/dev/shutter", O_RDWR | O_NOCTTY );

        // If USB device opened then make sure the shutter is actually okay.
        // Check this by closing it, waiting to make sure it has closed,
        // then reading the state.
        //
        if ( this->fd>=0 ) {
          error  = this->set_close();
          if ( error != NO_ERROR ) {
            std::stringstream message;
            message << "ERROR closing shutter: " << std::strerror(errno);
            logwrite( function, message.str() );
          }
          std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
          if ( error==NO_ERROR ) error = this->get_state( state );
        }
        else logwrite( function, "ERROR: failed to open /dev/shutter USB device (check udev)" );

        if ( error==NO_ERROR && state==0) logwrite( function, "shutter initialized OK" );
        else logwrite( function, "ERROR: failed to initialize shutter" );

        return ( error );
      }
      /***** Camera::Shutter:init *********************************************/


      /***** Camera::Shutter:shutdown *****************************************/
      /*
       * @brief      closes the USB connection
       *
       */
      void shutdown() {
        if ( this->fd>=0 ) { close( this->fd ); logwrite( "Camera::Shutter::shutdown", "USB device closed" ); }
        this->fd=-1;
        return;
      }
      /***** Camera::Shutter:shutdown *****************************************/


      /**
       * @brief      properly sets duration_sec when taking a 0s exposure
       */
      inline void zero_exposure() { this->duration_sec = 0.0; }


      /***** Camera::Shutter:set_open *****************************************/
      /*
       * @brief      opens the shutter
       * @details    Uses iotcl( TIOCMBIS ) to set modem control register RTS bit.
       *             Sets the class variable open_time to record the time it opened.
       * @return     ERROR or NO_ERROR
       *
       * If this returns ERROR then it means the shutter class has not been initialized.
       *
       */
      inline long set_open() {
        this->state=1;
        this->duration_sec=NAN;  // reset duration, set on shutter close
        this->open_time = std::chrono::high_resolution_clock::now();
        if ( this->is_enabled ) {  // operate the mechanism only if enabled
          return( ioctl( this->fd, TIOCMBIS, &this->RTS_bit ) < 0 ? ERROR : NO_ERROR );
        }
        else return NO_ERROR;
      }
      /***** Camera::Shutter:set_open *****************************************/


      /***** Camera::Shutter:set_close ****************************************/
      /*
       * @brief      closes the shutter and calculates shutter open time in sec
       * @details    Uses iotcl( TIOCMBIC ) to clear modem control register RTS bit.
       *             Sets the class variable close_time to record the time it closed.
       * @return     ERROR or NO_ERROR
       *
       * If this returns ERROR then it means the shutter class has not been initialized.
       *
       */
      inline long set_close() {
        this->state=0;
        this->close_time = std::chrono::high_resolution_clock::now();
        // close shutter here
        long ret;
        if ( this->is_enabled ) {  // operate the mechanism only if enabled
          ret=( ioctl( this->fd, TIOCMBIC, &this->RTS_bit ) < 0 ? ERROR : NO_ERROR );
        }
        else ret=NO_ERROR;
        // set shutter duration now
        this->duration_sec = std::chrono::duration_cast<std::chrono::nanoseconds>(this->close_time
                                                                                - this->open_time).count() / 1000000000.;
        return ret;
      }
      /***** Camera::Shutter:set_close ****************************************/


      /***** Camera::Shutter:get_duration *************************************/
      /*
       * @brief      returns the shutter open/close time duration in seconds
       * @details    This was calculated by Shutter::set_close()
       * @return     double precision duration in sec
       *
       */
      inline double get_duration() const {
        return this->duration_sec;
      }
      /***** Camera::Shutter:get_duration *************************************/


      /***** Camera::Shutter:get_state ****************************************/
      /*
       * @brief      reads the shutter state
       * @details    Uses ioctl( TIOCMGET ) to read modem control status bits,
       *             but since the Bonn signals are active low, a loss of power
       *             could trigger an active state.
       * @param[out] state  reference to int to contain state, where {-1,0,1}={error,closed,open}
       * @return     ERROR or NO_ERROR
       *
       * Note there is a discrepancy among the various Bonn documentation pages
       * as to which pins are blade A and B. I am using Table 5 of the user manual,
       * which agrees with the schematic title "Interface to Bonn-Shutter" REV 2.0,
       * dated Sep 5, 2013.
       *
       */
      long get_state( int &state ) {
        std::string function = "Camera::Shutter::get_state";
        std::stringstream message;
        int serial;

        // get all modem status bits (TIOCMGET) and store in serial
        //
        int err = ioctl( this->fd, TIOCMGET, &serial );

        if ( err ) {
          message.str(""); message << "ERROR: ioctl system call: " << std::strerror(errno);
          logwrite( function, message.str() );
          state = -1;
          return ERROR;
        }

#ifdef LOGLEVEL_DEBUG
        message.str(""); message << "[DEBUG] serial=0x" << std::hex << serial
                                 << " CAR=0x" << TIOCM_CAR 
                                 << " DSR=0x" << TIOCM_DSR 
                                 << " CTS=0x" << TIOCM_CTS;
        logwrite( function, message.str() );
        message.str(""); message << "[DEBUG] serial & CAR=0x" << (serial & TIOCM_CAR)
                                 << ( (serial&TIOCM_CAR)==0 ? " <-- Bonn error":"" );
        logwrite( function, message.str() );
        message.str(""); message << "[DEBUG] serial & DSR=0x" << (serial & TIOCM_DSR)
                                 << ( (serial&TIOCM_DSR)==0 ? " <-- Blade A closed":"" );
        logwrite( function, message.str() );
        message.str(""); message << "[DEBUG] serial & CTS=0x" << (serial & TIOCM_CTS)
                                 << ( (serial&TIOCM_CTS)==0 ? " <-- Blade B closed":"" );
        logwrite( function, message.str() );
#endif

        // Check if all bits are low, which is an indicator of a power or connection problem,
        // since both blades can't be closed at the same time (but they can both be open).
        //
        if ( (serial & TIOCM_CAR)==0 &&
             (serial & TIOCM_DSR)==0 &&
             (serial & TIOCM_CTS)==0 ) {
          logwrite( function, "ERROR: all Bonn status bits are LO, indicating possible power loss or connection fault" );
          state = -1;
          return ERROR;
        }

        // If the CAR bit is not set in serial then this is a Bonn error (active low)
        //
        if ( (serial & TIOCM_CAR) == 0 ) {
          logwrite( function, "ERROR: Bonn shutter fatal error" );
          state = -1;
          return ERROR;
        }

        // If either DSR or CTS are not set then then shutter is closed
        //
        if ( ( (serial & TIOCM_DSR) == 0 ) || ( (serial & TIOCM_CTS) == 0 ) ) {  // shutter closed
          state = 0;
          return NO_ERROR;
        }
        else
        // If both DSR and CTS are set then the shutter is open
        //
        if ( ( serial & TIOCM_DSR ) && ( serial & TIOCM_CTS ) ) {                // shutter open
          state = 1;
          return NO_ERROR;
        }
        else {
          message.str(""); message << "ERROR: unknown state 0x" << std::hex << serial;
          logwrite( function, message.str() );
          state = -1;
          return ERROR;
        }
      }
      /***** Camera::Shutter:get_state ****************************************/


      Shutter() : state(-1), RTS_bit(TIOCM_RTS), fd(-1), duration_sec(NAN), is_enabled(true) {
        this->open_time = this->close_time = std::chrono::high_resolution_clock::now();
      }

      Shutter(const Shutter&) = delete;

      ~Shutter() { this->shutdown(); }
  };
  /***** Camera::Shutter ******************************************************/
}
