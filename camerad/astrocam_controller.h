#pragma once

#include <map>
#include <atomic>

#include "CArcBase.h"
#include "ArcDefs.h"
#include "CExpIFace.h"
#include "CConIFace.h"
#include "CArcPCI.h"
#include "CArcPCIe.h"
#include "CArcDevice.h"
#include "CArcBase.h"
#include "CooExpIFace.h"

namespace Camera {

  class AstroCamInterface;  //!< forward declaration of outer class

  /***** Camera::AstroCamInterface::Callback **********************************/
  /**
   * @class Callback
   * @brief Callback class inherited from the ARC API
   */
  class Callback : public arc::gen3::CooExpIFace {
    public:
      Callback() = default;
      ~Callback() = default;
      void exposeCallback( int devnum, std::uint32_t uiElapsedTime, std::uint32_t uiExposureTime ); ///< called by CArcDevice::expose() during exposure
      void readCallback( int expbuf, int devnum, std::uint32_t uiPixelCount, std::uint32_t uiFrameSize );       ///< called by CArcDevice::expose() during readout
      void frameCallback( int expbuf,
                          int devnum,
                          std::uint32_t uiFramesPerBuffer,
                          std::uint32_t uiFrameCount,
                          std::uint32_t uiRows,
                          std::uint32_t uiCols,
                          void* pBuffer );                            ///< called by CArcDevice::expose() when a frame has been received
      void ftCallback( int expnun, int devnum );
  };
  /***** Camera::AstroCamInterface::Callback **********************************/


  /***** Camera::AstroCamInteface::Controller *********************************/
  /**
   * @class    Controller
   * @brief    contains information for each controller
   * @details  The Controller class is a sub-class of Interface and is
   *           here to contain the Camera::Information class and FITS_file
   *           class objects.  There will be a vector of Controller class
   *           objects which matches the vector of controller objects.
   *
   */
  class Controller {
    friend class AstroCamInterface;

    public:
      Controller();                 //!< class constructor
      ~Controller() = default;      //!< no deconstructor

    private:
      uint32_t bufsize;
      int framecount;               //!< keep track of the number of frames received per expose
      long workbuf_size;

      void* workbuf;                //!< pointer to workspace for performing deinterlacing

      /**
       * @var     expinfo
       * @brief   vector of Camera::Information class, one for each exposure buffer
       * @details This contains the camera information for each exposure buffer.
       *          Note that since Camera::Information contains extkeys and prikeys objects,
       *          expinfo will contain those objects, but prikeys should not be used.
       *          Use only Controller::expinfo.extkeys because keys specific to a given
       *          controller will be added to that controller's extension.
       */
//    std::vector<Camera::Information> expinfo;

      // The frameinfo structure holds frame information for each frame
      // received by the callback. This is used to keep track of all the 
      // threads spawned by handle_frame.
      //
      typedef struct {
        int   tid;                      //!< use fpbcount as the thread ID here
        int   framenum;                 //!< the current frame from ARC_API's fcount, counts from 1
        int   rows;                     //!< number of rows in this frame
        int   cols;                     //!< number of cols in this frame
        void* buf;                      //!< pointer to the start of memory holding this frame
        bool  inuse;                    //!< this thread ID is in use, set when thread is spawned, cleared when handle_frame is done
      } frameinfo_t;

      int error;

      int cols;                        //!< total number of columns read (includes overscan)
      int rows;                        //!< total number of rows read (includes overscan)

      // These are detector image geometry values for each device,
      // unaffected by binning.
      //
      int detcols;                     //!< number of detector columns (unchanged by binning)
      int detrows;                     //!< number of detector rows (unchanged by binning)
      int oscols0;                     //!< requested number of overscan rows
      int osrows0;                     //!< requested number of overscan columns
      int oscols;                      //!< realized number of overscan rows (can be modified by binning)
      int osrows;                      //!< realized number of overscan columns (can be modified by binning)
      int skiprows;
      int skipcols;

      int defcols;                     //!< number of detector columns (unchanged by binning)
      int defrows;                     //!< number of detector rows (unchanged by binning)
      int defoscols;                   //!< requested number of overscan rows
      int defosrows;                   //!< requested number of overscan columns

      std::string imsize_args;         ///< IMAGE_SIZE arguments read from config file, used to restore default

      arc::gen3::CArcDevice* pArcDev;  //!< arc::CController object pointer -- things pointed to by this are in the ARC API
      Callback* pCallback;             //!< Callback class object must be pointer because the API functions are virtual
      bool connected;                  //!< true if controller connected (requires successful TDL command)
      bool inactive;                   //!< set true to skip future use of controllers when unable to connect
      bool firmwareloaded;             //!< true if firmware is loaded, false otherwise
      std::string firmware;            //!< name of firmware (.lod) file
      std::string channel;             //!< name of spectrographic channel
      std::string ccd_id;              //!< CCD identifier (E.G. serial number, name, etc.)
      int devnum;                      //!< this controller's devnum
      std::string devname;             //!< comes from arc::gen3::CArcPCI::getDeviceStringList()
      std::uint32_t retval;            //!< convenient place to hold return values for threaded commands to this controller
      std::map<int, frameinfo_t>  frameinfo;  //!< STL map of frameinfo structure (see above)
      uint32_t readout_arg;

      bool have_ft;                    //!< Do I have (and am I using) frame transfer?
      std::atomic<bool> in_readout;    //!< Is the controller currently reading out/transmitting pixels?
      std::atomic<bool> in_frametransfer;  //!< Is the controller currently performing a frame transfer?

  };
  /***** Camera::AstroCamInteface::Controller *********************************/
}
