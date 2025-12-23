# ----------------------------------------------------------------------------
# @file    Instruments/hispec_tracking_camera/hispec_tracking_camera.cmake
# @brief   HISPEC Tracking Camera-specific input to the CMake build system for camerad
# @author  Michael Langmayr <langmayr@astro.caltech.edu>
# ----------------------------------------------------------------------------

set (INSTRUMENT_SOURCES
    ${CAMERAD_DIR}/Instruments/hispec_tracking_camera/hispec_tracking_camera_instrument.cpp
    ${CAMERAD_DIR}/Instruments/hispec_tracking_camera/hispec_tracking_camera_interface_factory.cpp
    ${CAMERAD_DIR}/Instruments/hispec_tracking_camera/hispec_tracking_camera_exposure_modes.cpp
    )
