# ----------------------------------------------------------------------------
# @file    ARC/CArcBase/CMakeLists.txt
# @brief   input to the CMake build system for building CArcBase lib of ARC API
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

include_directories( ${ARC_DIR}/CArcBase/inc )
include_directories( ${CARC_DEVICE_DIR}/inc )

find_path( CARC_DEVICE_DIR "CArcDevice.h" PATHS ${CARC_DEVICE_DIR}/inc )
find_path( CARC_BASE_DIR "CArcBase.h" PATHS ${ARC_DIR}/CArcBase/inc )

add_library( CArcDevice SHARED
        ${CARC_DEVICE_DIR}/src/CArcDevice.cpp
        ${CARC_DEVICE_DIR}/src/ArcDeviceCAPI.cpp
        ${CARC_DEVICE_DIR}/src/ArcOSDefs.cpp
        ${CARC_DEVICE_DIR}/src/CArcDeviceDllMain.cpp
        ${CARC_DEVICE_DIR}/src/CArcLog.cpp
        ${CARC_DEVICE_DIR}/src/CArcPCI.cpp
        ${CARC_DEVICE_DIR}/src/CArcPCIBase.cpp
        ${CARC_DEVICE_DIR}/src/CArcPCIe.cpp
        ${CARC_DEVICE_DIR}/src/TempCtrl.cpp
        )

target_link_libraries( CArcDevice CArcBase )
