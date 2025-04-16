# ----------------------------------------------------------------------------
# @file    ARC/CArcFitsFile/CMakeLists.txt
# @brief   input to the CMake build system for building CArcFItsFile lib of ARC API
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

add_definitions( -std=c++17 -O0 -Wall -c -fmessage-length=0 -D_REENTRANT -fPIC -MMD -MP -MF )

include_directories( ${CARC_FITS_DIR}/inc )
include_directories( ${CARC_BASE_DIR}/inc )

find_path( CARC_FITS_DIR "CArcFitsFile.h" PATHS ${CARC_FITS_DIR}/inc )
find_path( CARC_FITS_DIR "ArcFitsFileCAPI.h" PATHS ${CARC_FITS_DIR}/inc )
find_path( CARC_FITS_DIR "CArcFitsFileDllMain.h" PATHS ${CARC_FITS_DIR}/inc )
find_path( CARC_BASE_DIR "CArcStringList.h" PATHS ${CARC_BASE_DIR}/inc )

find_library( CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib )

add_library( CArcFitsFile SHARED
        ${CARC_FITS_DIR}/src/CArcFitsFile.cpp
        ${CARC_FITS_DIR}/src/ArcFitsFileCAPI.cpp
        ${CARC_FITS_DIR}/src/CArcFitsFileDllMain.cpp
        )

target_link_libraries( CArcFitsFile stdc++fs )
target_link_libraries( CArcFitsFile CArcBase )
target_link_libraries( CArcFitsFile ${CFITS_LIB} )

