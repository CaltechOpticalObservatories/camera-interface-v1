# ----------------------------------------------------------------------------
# @file    ARC/CMakeLists.txt
# @brief   input to the CMake build system for building ARC API
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( ARC_DIR ${PROJECT_BASE_DIR}/ARC )

set( CARC_BASE_DIR ${ARC_DIR}/CArcBase )
set( CARC_DEVICE_DIR ${ARC_DIR}/CArcDevice )
set( CARC_FITS_DIR ${ARC_DIR}/CArcFitsFile )

add_subdirectory( ${ARC_DIR}/CArcBase )
add_subdirectory( ${ARC_DIR}/CArcDevice )
add_subdirectory( ${ARC_DIR}/CArcFitsFile )

