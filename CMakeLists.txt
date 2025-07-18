# ----------------------------------------------------------------------------
# CMakeLists.txt
# top-level CMake input file
# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# user must define the interface type here
# valid types are: "AstroCam" | "Archon"
# ----------------------------------------------------------------------------
set(INTERFACE_TYPE "Archon")
# ----------------------------------------------------------------------------

# un-comment the following to log verbose debug messages
#add_definitions(-DLOGLEVEL_DEBUG)

cmake_minimum_required( VERSION 3.12 )

set( CMAKE_CXX_STANDARD 17 )

project( camera )

# Run "cmake .." from the project's build/ directory!
#
set( PROJECT_BASE_DIR $ENV{PWD}/../ )

set( EXECUTABLE_OUTPUT_PATH ${PROJECT_BASE_DIR}/bin )
set( LIBRARY_OUTPUT_PATH ${PROJECT_BASE_DIR}/lib )
set( CMAKE_C_COMPILER /usr/bin/g++ )
set( CMAKE_CXX_COMPILER /usr/bin/g++ )

find_package( Threads )

add_subdirectory( ${PROJECT_BASE_DIR}/utils )
add_subdirectory( ${PROJECT_BASE_DIR}/common )
add_subdirectory( ${PROJECT_BASE_DIR}/camerad )
add_subdirectory( ${PROJECT_BASE_DIR}/emulator )
