/**
 * @file   image_process.cpp
 * @brief  implementation of image processing functions
 *
 */

#include "image_process.h"

namespace Camera {

  /***** Camera::DeInterlace_None *********************************************/
  /**
   * @brief      specialization for deinterlacing None
   * @param[in]  bufin   pointer to input buffer
   * @param[out] bufout  pointer to deinterlaced buffer
   */
  class DeInterlace_None : public DeInterlacer {
    public:
      void deinterlace(char* bufin, uint16_t* bufout) {
        const std::string function("Camera::DeInterlace_None::deinterlace");
        logwrite(function, "here");
      }
  };
  /***** Camera::DeInterlace_None *********************************************/


  /***** Camera::DeInterlace_RXRV *********************************************/
  /**
   * @brief      specialization for deinterlacing RXRV
   * @param[in]  imgbuf  pointer to input buffer
   * @param[out] sigbuf  pointer to deinterlaced signal frame from imgbuf
   * @param[out] resbuf  pointer to deinterlaced reset frame from imgbuf
   */
  class DeInterlace_RXRV : public DeInterlacer {
    public:
      void deinterlace(char* imgbuf, uint16_t* sigbuf, uint16_t* resbuf) {
        const std::string function("Camera::DeInterlace_RXRV::deinterlace");
        logwrite(function, "here");
        std::stringstream message;
        message << "contents:";
        for (int i=0; i<10; i++) {
          message << " " << i;
          // modify contents of output buffers
          sigbuf[i]=i;
          resbuf[i]=100-i;
        }
        logwrite(function, message.str());
      }
  };
  /***** Camera::DeInterlace_RXRV *********************************************/


  class SubtractSimple : public Subtractor {
    public:
      void subtract(uint16_t* in1, uint16_t* in2, int16_t* out) {
        logwrite("Camera::SubtractSimple::subtract", "not yet implemented");
      }
      void subtract(uint16_t* in1, uint16_t* in2, int32_t* out) {
        logwrite("Camera::SubtractSimple::subtract", "not yet implemented");
      }
  };


  class CoaddAdd : public Coadder {
    public:
      void coadd(uint16_t* in, uint16_t* out) {
        logwrite("Camera::CoaddAdd::coadd", "not yet implemented");
      }
      void coadd(uint16_t* in, int16_t* out) {
        logwrite("Camera::CoaddAdd::coadd", "not yet implemented");
      }
      void coadd(uint16_t* in, int32_t* out) {
        logwrite("Camera::CoaddAdd::coadd", "not yet implemented");
      }
  };


  /***** Camera::make_image_processor *****************************************/
  /**
   * @brief      factory function creates appropriate image processor object
   * @param[in]  mode
   * @return     unique_ptr to ImageProcessor derived object
   * @throws     std::invalid_argument
   *
   */
  std::unique_ptr<ImageProcessor> make_image_processor(const std::string &mode) {
    const std::string function("Camera::make_image_processor");
    logwrite(function, "factory for mode "+mode);
    if (mode=="none") {
      return std::make_unique<ImageProcessor>(
          std::make_unique<DeInterlace_None>(),
          nullptr,
          nullptr
          );
    }
    else
    if (mode=="rxrv") {
      return std::make_unique<ImageProcessor>(
          std::make_unique<DeInterlace_RXRV>(),
          std::make_unique<SubtractSimple>(),
          std::make_unique<CoaddAdd>()
          );
    }
    else throw std::invalid_argument(function+" unknown mode "+mode);
  }
  /***** Camera::make_image_processor *****************************************/
}
