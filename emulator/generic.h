#pragma once
#include "image_info_base.h"

namespace Archon {

  /***** Archon::GenericImage *************************************************/
  /**
   * @brief      derived class inherits from ImageInfoBase
   * @details    This derived class provides the specialization of the base
   *             class for a generic instrument. This contains specific
   *             implementations which are not common to all instruments.
   *
   */
  class GenericImage : public ImageInfoBase {
    public:
      GenericImage() = default;

      /***** GenericImage::calc_rowtime ***************************************/
      /**
       * @brief      returns the row time
       * @details    this uses the configured readout time
       * @return     rowtime in usec
       *
       */
      virtual double calc_rowtime() override {
        double rowtime = std::floor( 10 * this->readtime / this->linecount );  // row time in 10 msec
        rowtime *= 90.0;                                                       // 90% of row time in usec
        return rowtime;
      }
      /***** GenericImage::calc_rowtime ***************************************/

      void set_config_parameter( const std::string &key, const std::string &val ) {
        ImageInfoBase::set_config_parameter( key, val ); // Call base class method for common parameters
        if ( key == "READOUT_TIME" ) { this->readtime = std::stoi( val ); }
        return;
      }

      virtual int get_readouttime() override {
         return readouttime;
      }
  };
  /***** Archon::GenericImage *************************************************/

}
