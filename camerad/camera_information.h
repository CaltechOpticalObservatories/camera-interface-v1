
#pragma once

namespace Camera {

  class Information {
    friend class CameraInterface;
    public:
      Common::FitsKeys userkeys;

      std::string base_name;                //!< base image name
//    std::map<int, std::string> firmware;  //!< firmware file for given controller @TODO support multiple controllers
      std::string firmware;                 //!< firmware file
      int nexp;                             //!< number passed to do_expose
  };
}
