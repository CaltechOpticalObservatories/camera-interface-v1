/**
 * @file     archon_exposure_modes.h
 * @brief    implements Archon-specific exposure modes
 *
 */

#include "archon_exposure_modes.h"

namespace Camera {

  /***** Camera::Expose_CCD **************************************************/
  /**
   * @brief  implementation of Archon-specific expose for CCD
   *
   */
  long Expose_CCD::expose() {
    const std::string function("Camera::Expose_CCD::expose");
    logwrite(function, "hi");
    return NO_ERROR;
  }
  /***** Camera::Expose_CCD **************************************************/


  /***** Camera::Expose_RXRV *************************************************/
  /**
   * @brief  implementation of Archon-specific expose for RXR-Video
   *
   */
  long Expose_RXRV::expose() {
    const std::string function("Camera::Expose_RXRV::expose");
    logwrite(function, "hi");

    // read first frame pair

    // process (deinterlace) first frame pair

    // loop:
    // subsequent frame pairs, read, deinterlace, write

    return NO_ERROR;
  }
  /***** Camera::Expose_RXRV *************************************************/

}
