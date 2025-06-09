# ----------------------------------------------------------------------------
# camerad/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.12)

include_directories(${CMAKE_SOURCE_DIR}/utils)
include_directories(${CMAKE_SOURCE_DIR}/common)


# ----------------------------------------------------------------------------
# Setup for appropriate hardware interface...
#
# Each interface must define the following:
#   INTERFACE_TARGET    name to use for the target interface
#   INTERFACE_SOURCE    list of sources required for target interface
#   INTERFACE_INCLUDES  list of include paths for target interface
#   interface           target for interfacing to appropriate hardware
# ----------------------------------------------------------------------------



# ----------------------------------------------------------------------------
# AstroCam ARC-64/66 PCI/e interfaces
# ----------------------------------------------------------------------------
if (${INTERFACE_TYPE} STREQUAL "AstroCam")
    message(STATUS "compiling for AstroCam GenIII PCI/PCIe")
    set(INTERFACE_TARGET astrocam)
    set(ARCAPI_DIR "/opt/ARC_API/3.6")
    add_definitions(-Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb)
    add_definitions(-DASTROCAM)
    add_definitions(-DARC66_PCIE)
    find_path(ARCAPI_BASE "CArcBase.h" PATHS ${ARCAPI_DIR}/CArcBase/inc)
    find_path(ARCAPI_FITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
    find_path(ARCAPI_CFITS "CArcFitsFile.h" PATHS ${ARCAPI_DIR}/CArcFitsFile/inc)
    find_path(ARCAPI_DEVICE "CArcDevice.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
    find_path(ARCAPI_PCI "CArcPCI.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
    find_path(ARCAPI_EXPIFACE "CExpIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
    find_path(ARCAPI_CONIFACE "CConIFace.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)
    find_path(ARCAPI_DEFS "ArcDefs.h" PATHS ${ARCAPI_DIR}/CArcDevice/inc)

    set(INTERFACE_SOURCE
            "${CAMERA_DIR}/astrocam.cpp"
    )

    set(INTERFACE_INCLUDES
            "${ARC_INTERFACE}"
            "${ARCAPI_BASE}"
            "${ARCAPI_FITS}"
            "${ARCAPI_CFITS}"
            "${ARCAPI_DEVICE}"
            "${ARCAPI_PCI}"
            "${ARCAPI_EXPIFACE}"
            "${ARCAPI_CONIFACE}"
            "${ARCAPI_DEFS}"
    )
    find_library(CARC_BASE "CArcBase3.6" NAMES "libCArcBase3.6" PATHS ${ARCAPI_DIR}/Release)
    find_library(CARC_DEVICE "CArcDevice3.6" NAMES "libCArcDevice3.6.so" PATHS ${ARCAPI_DIR}/Release)
    find_library(CARC_FITS "CArcFitsFile3.6" NAMES "libCArcFitsFile3.6.so" PATHS ${ARCAPI_DIR}/Release)

    add_library(interface STATIC ${INTERFACE_SOURCE})

    
    # ----------------------------------------------------------------------------
    # STA Archon interfaces
    # ----------------------------------------------------------------------------

elseif (${INTERFACE_TYPE} STREQUAL "Archon")
    message(STATUS "compiling for STA Archon")
    add_definitions(-Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb)
    add_definitions(-DSTA_ARCHON)
    list(APPEND INTERFACE_SOURCE "archon.cpp")
    add_library(interface STATIC "${INTERFACE_SOURCE}")
    if (${DETECTOR_TYPE} STREQUAL "Hxrg")
        message(STATUS "compiling for HXRG detector")
        add_definitions(-DDET_HXRG)
    else ()
        message(STATUS "compiling for CCD detector")
    endif ()
else ()
    message(FATAL_ERROR " unknown interface type: " ${INTERFACE_TYPE})
endif ()

# ----------------------------------------------------------------------------
# Add appropriate instrument library here.
# The user adds a "-DINSTR=<instr>" to the cmake command line.
# Currently supported <instr> names are: nirc2 generic
# ----------------------------------------------------------------------------

if (NOT DEFINED INSTR)
    set(INSTR "undefined")
else ()
    if (${INSTR} STREQUAL "nirc2")
        message(STATUS "building for nirc2 instrument")
        list(APPEND INTERFACE_SOURCE "nirc2.cpp")
    elseif (${INSTR} STREQUAL "generic")
        message(STATUS "building for generic instrument")
        list(APPEND INTERFACE_SOURCE "generic.cpp")
    elseif (${INSTR} STREQUAL "undefined")
        message(STATUS "no INSTR defined. using generic but other options are:")
        message(STATUS "cmake -DINSTR=nirc2,generic ..")
        list(APPEND INTERFACE_SOURCE "generic.cpp")
    else ()
        message(STATUS "unknown instrument " ${INSTR} ": using generic")
        list(APPEND INTERFACE_SOURCE "generic.cpp")
    endif ()
endif ()

# ----------------------------------------------------------------------------
# End interface setup
# ----------------------------------------------------------------------------

# Now add the defined interface target:
#
target_link_libraries(interface PUBLIC cfitsio CCfits)



#Note from danw: I'm afraid camera.h technically includes CCfits, and so a public
#dependency on those is needed
add_library(camera camera.cpp)
target_link_libraries(camera PUBLIC cfitsio CCfits)


find_package(Threads)
add_executable(camerad camerad.cpp  ${INTERFACE_INCLUDES})

# ----------------------------------------------------------------------------
# Everyone gets this:
# ----------------------------------------------------------------------------

target_link_libraries(camerad
        camera
        network
        logentry
        common
        utilities
	interface
        ${CMAKE_THREAD_LIBS_INIT}
	cfitsio
	CCfits
)

target_link_libraries(camerad ${CARC_BASE} ${CARC_DEVICE} ${CARC_FITS})


# ----------------------------------------------------------------------------
# cURL is not used here, so it's not strictly required, but cfitsio can
# use it. If cfitsio was built with libcurl then it has to be linked here.
# If it's installed on the system then link it, but don't make it mandatory.
# ----------------------------------------------------------------------------
#
find_package(CURL)

if (CURL_FOUND STREQUAL "FALSE")
    message(STATUS "cURL was not found but may be needed by some systems if built into cfitsio.\n   If you experience undefined curl references during linking \n   then please your cURL installation.")
else ()
    target_link_libraries(camerad ${CURL_LIBRARIES})
endif ()

