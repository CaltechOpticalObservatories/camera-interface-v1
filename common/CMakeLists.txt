# ----------------------------------------------------------------------------
# @file    common/CMakeLists.txt
# @brief   input to CMake build system for building common software packages
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.12)

add_definitions(-Wall -Wno-variadic-macros)

#for now have hard coded path here, better in future
#to add include directory as a link interface property
#of a preceeding target. But that may not be available in
#cmake 3.12
include_directories(${CMAKE_SOURCE_DIR}/utils)    # needed for logentry

add_library(common  common.cpp)
