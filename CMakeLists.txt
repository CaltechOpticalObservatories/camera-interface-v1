# ----------------------------------------------------------------------------
# CMakeLists.txt
# top-level CMake input file
# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# user must define the interface type here
# valid types are: "AstroCam" | "Archon"
# ----------------------------------------------------------------------------


# Interface type can be set via command line ( Archon | AstroCam )
IF (NOT DEFINED INTERFACE_TYPE)
    set(INTERFACE_TYPE "Archon")
endif ()

# Instrument can be set via command line generic is the default value
IF (NOT DEFINED INSTR)
    set(INSTR "generic")
endif ()

# Detector type can be set via command line ( Hxrg | Ccd )
if (NOT DEFINED DETECTOR_TYPE)
    set(DETECTOR_TYPE "Ccd")
endif ()
# ----------------------------------------------------------------------------

# un-comment the following to log verbose debug messages
#add_definitions(-DLOGLEVEL_DEBUG)

cmake_minimum_required(VERSION 3.12)

find_program(GXX_COMPILER NAMES g++)

if(GXX_COMPILER)
    message(STATUS "Found g++: ${GXX_COMPILER}")
    set(CMAKE_CXX_COMPILER ${GXX_COMPILER})
else()
    message(FATAL_ERROR "g++ compiler not found. Please install g++ and try again.")
endif()

project(camera)

set(CMAKE_GXX_STANDARD 17)
set(CMAKE_GXX_STANDARD_REQUIRED ON)
set(CMAKE_GXX_EXTENSIONS OFF)

# Run "cmake .." from the project's build/ directory!
#
set(PROJECT_BASE_DIR $ENV{PWD}/../)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BASE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BASE_DIR}/lib)


if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    include_directories(/usr/local/include)
endif()

find_package(Threads)

add_subdirectory(${PROJECT_BASE_DIR}/utils)
add_subdirectory(${PROJECT_BASE_DIR}/common)
add_subdirectory(${PROJECT_BASE_DIR}/camerad)
#add_subdirectory(${PROJECT_BASE_DIR}/emulator)
#add_subdirectory(${PROJECT_BASE_DIR}/tests EXCLUDE_FROM_ALL)

