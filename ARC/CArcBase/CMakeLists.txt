# ----------------------------------------------------------------------------
# @file    ARC/CArcBase/CMakeLists.txt
# @brief   input to the CMake build system for building CArcBase lib of ARC API
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

add_definitions( -std=c++17 -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF )

include_directories( ${CARC_BASE_DIR}/inc )

find_path( CARC_BASE_DIR "CArcBase.h" PATHS ${CARC_BASE_DIR}/inc )
find_path( CARC_BASE_DIR "CArcStringList.h" PATHS ${CARC_BASE_DIR}/inc )
find_path( CARC_BASE_DIR "CArcBaseDllMain.h" PATHS ${CARC_BASE_DIR}/inc )

add_library( CArcBase SHARED
        ${CARC_BASE_DIR}/src/CArcBase.cpp
        ${CARC_BASE_DIR}/src/CArcBaseDllMain.cpp
        ${CARC_BASE_DIR}/src/CArcStringList.cpp
        )

