# ----------------------------------------------------------------------------
# @file    camerad/CMakeLists.txt
# @brief   input to the CMake build system for building camera daemon
# @author  David Hale <dhale@caltech.edu>
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( CAMERAD_DIR ${PROJECT_BASE_DIR}/camerad )

set( CMAKE_CXX_STANDARD 17 )

include_directories(${CAMERAD_DIR}
                    ${CAMERAD_DIR}/Instruments
                    ${PROJECT_BASE_DIR}/utils
                    ${PROJECT_BASE_DIR}/common
		    )
link_directories( ${PROJECT_BASE_DIR}/lib )

# ----------------------------------------------------------------------------
# controller selection sets interface-specific sources
# ----------------------------------------------------------------------------
option(CONTROLLER "Choose controller type { bob joe archon astrocam }")
# ----------------------------------------------------------------------------
# bob
# ----------------------------------------------------------------------------
if (CONTROLLER STREQUAL "bob")
  message (STATUS "controller: Bob")
  add_definitions(-DCONTROLLER_BOB)
  set (INTERFACE_SOURCES
      ${CAMERAD_DIR}/bob_interface.cpp
      )
# ----------------------------------------------------------------------------
# joe
# ----------------------------------------------------------------------------
elseif (CONTROLLER STREQUAL "joe")
  message (STATUS "controller: Joe")
  add_definitions(-DCONTROLLER_JOE)
  set (INTERFACE_SOURCES
      ${CAMERAD_DIR}/joe_interface.cpp
      )
# ----------------------------------------------------------------------------
# Archon
# ----------------------------------------------------------------------------
elseif (CONTROLLER STREQUAL "archon")
  message (STATUS "controller: Archon")
  add_definitions(-DCONTROLLER_ARCHON)
  set (INTERFACE_TARGET archon)
  set (INTERFACE_SOURCES
      ${CAMERAD_DIR}/archon_interface.cpp
      ${CAMERAD_DIR}/archon_controller.cpp
      ${CAMERAD_DIR}/archon_exposure_modes.cpp
      ${CAMERAD_DIR}/archon_exposure_modes_rxrv.cpp
      )
# ----------------------------------------------------------------------------
# AstroCam ARC-64/66 PCI/e
# ----------------------------------------------------------------------------
elseif (CONTROLLER STREQUAL "astrocam")
  message (STATUS "controller: AstroCam GenIII PCI/PCIe")
  add_definitions (-DCONTROLLER_ASTROCAM)
  set (INTERFACE_TARGET astrocam)
  set (ARCAPI_DIR "${PROJECT_BASE_DIR}/ARC")
  find_path (ARCAPI_BASE "CArcBase.h" PATHS ${ARCAPI_DIR}/CArcBase/inc)
  find_path (ARCAPI_FITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
  find_path (ARCAPI_CFITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
  find_path (ARCAPI_DEVICE "CArcDevice.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path (ARCAPI_PCI "CArcPCI.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path (ARCAPI_EXPIFACE "CExpIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path (ARCAPI_CONIFACE "CConIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  find_path (ARCAPI_DEFS "ArcDefs.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
  add_subdirectory(${PROJECT_BASE_DIR}/ARC ${CMAKE_BINARY_DIR}/ARC)
  set (INTERFACE_INCLUDES
      "${ARCAPI_BASE}"
      "${ARCAPI_FITS}"
      "${ARCAPI_CFITS}"
      "${ARCAPI_DEVICE}"
      "${ARCAPI_PCI}"
      "${ARCAPI_EXPIFACE}"
      "${ARCAPI_CONIFACE}"
      "${ARCAPI_DEFS}"
      )
  set (INTERFACE_SOURCES
      ${CAMERAD_DIR}/astrocam_interface.cpp
      ${CAMERAD_DIR}/astrocam_controller.cpp
      ${CAMERAD_DIR}/astrocam_exposure_modes.cpp
      )
  set (INTERFACE_LIBS
      CArcBase
      CArcDevice
      CArcFitsFile
      )
# ----------------------------------------------------------------------------
# controller undefined
# ----------------------------------------------------------------------------
else()
  message(FATAL_ERROR "You must specify -DCONTROLLER={ bob joe archon astrocam }")
endif()

# ----------------------------------------------------------------------------
# default instrument is none
# ----------------------------------------------------------------------------
option(INSTRUMENT "Choose instrument { generic ... }")
if (INSTRUMENT)
  message(STATUS "instrument: ${INSTRUMENT}")
  include(Instruments/${INSTRUMENT}/${INSTRUMENT}.cmake)
else()
  message(STATUS "instrument: none")
  set (INSTRUMENT_SOURCES "")
  # If no instrument is defined then include the factory for the specified controller
  if (CONTROLLER STREQUAL "archon")
    set (INSTRUMENT_SOURCES ${CAMERAD_DIR}/archon_interface_factory.cpp)
  elseif (CONTROLLER STREQUAL "astrocam")
    set (INSTRUMENT_SOURCES ${CAMERAD_DIR}/astrocam_interface_factory.cpp)
  elseif (CONTROLLER STREQUAL "bob")
    set (INSTRUMENT_SOURCES ${CAMERAD_DIR}/bob_interface_factory.cpp)
  elseif (CONTROLLER STREQUAL "joe")
    set (INSTRUMENT_SOURCES ${CAMERAD_DIR}/joe_interface_factory.cpp)
  endif()
endif()

# ----------------------------------------------------------------------------
# Camera Interface Base
# ----------------------------------------------------------------------------
list (APPEND INTERFACE_SOURCES
  ${CAMERAD_DIR}/camera_interface.cpp
  ${CAMERAD_DIR}/image_process.cpp
  ${INSTRUMENT_SOURCES}
  )
add_library(${INTERFACE_TARGET} ${INTERFACE_SOURCES})
target_link_libraries(${INTERFACE_TARGET}
                      common
                      )
target_include_directories(${INTERFACE_TARGET} PUBLIC ${INTERFACE_INCLUDES})

# ----------------------------------------------------------------------------
# External libraries, such as FITS, etc.
# ----------------------------------------------------------------------------

# FITS
#
find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS /usr/local/lib)
find_library(CFITS_LIB cfitsio NAMES libcfitsio  PATHS /usr/local/lib)

# ZeroMQ
#
find_library( ZMQPP_LIB zmqpp NAMES libzmqpp PATHS /usr/local/lib )
find_library( ZMQ_LIB zmq NAMES libzmq PATHS /usr/local/lib )

# OpenCV
#
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# Boost
#
find_package(Boost REQUIRED COMPONENTS thread chrono)
include_directories(${BOOST_INCLUDES})

find_package(Threads)

# ----------------------------------------------------------------------------
# build the camera daemon
# ----------------------------------------------------------------------------
add_executable(camerad
  ${CAMERAD_DIR}/camerad.cpp
  ${CAMERAD_DIR}/camera_server.cpp
  )

# ----------------------------------------------------------------------------
# link everything
# ----------------------------------------------------------------------------
target_link_libraries(camerad
  network
  utilities
  logentry
  shared_memory_writer
  ${INTERFACE_TARGET}
  ${INTERFACE_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CCFITS_LIB}
  ${CFITS_LIB}
  ${OpenCV_LIBS}
  ${BOOST_INCLUDES}
  Boost::thread
  Boost::chrono
  ${ZMQPP_LIB}
  ${ZMQ_LIB}
  )

