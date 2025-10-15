
#pragma once

namespace Camera {

    /***** Camera::ExposureTime ***********************************************/
    /**
     * @class    ExposureTime
     * @brief    base class for exposure time handling
     * @details  Exposure time is always represented in seconds.
     *
     */
    class ExposureTime {
      protected:
        double exptime_sec;
      public:
        ExposureTime() : exptime_sec(0) { }

        virtual ~ExposureTime()=default;
        virtual void set(double value) { exptime_sec=value; }
        virtual double get() const { return exptime_sec; }
    };
    /***** Camera::ExposureTime ***********************************************/


  class Information {
    friend class CameraInterface;
    public:
      Information() {
        this->exposure_time = std::make_unique<ExposureTime>();
      }
      Information(const Information&) = delete;
      Information& operator=(const Information&) = delete;

      Common::FitsKeys userkeys;            //!< FITS keys for command line use
      Common::FitsKeys systemkeys;          //!< FITS keys for system use

      std::unique_ptr<ExposureTime> exposure_time;

      std::string base_name;                //!< base image name
//    std::map<int, std::string> firmware;  //!< firmware file for given controller @TODO support multiple controllers
      std::string firmware;                 //!< firmware file
      int nexp;                             //!< number passed to do_expose
  };
}
