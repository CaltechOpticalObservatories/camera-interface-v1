# ----------------------------------------------------------------------------
# camerad/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( CAMERA_DIR ${PROJECT_BASE_DIR}/camerad )

# root directory of libs installed just for nirc2
set( CIT_ROOT "/usr/local/caltech" )

# internal
include_directories( ${PROJECT_BASE_DIR}/utils )
include_directories( ${PROJECT_BASE_DIR}/common )
link_directories( ${PROJECT_BASE_DIR}/lib )

# ----------------------------------------------------------------------------
# OpenCV
# ----------------------------------------------------------------------------
set( ENV{OpenCV_DIR} ${CIT_ROOT}/opencv-454 )
find_package( OpenCV REQUIRED )
include_directories( SYSTEM ${OpenCV_INCLUDE_DIRS} )

# ----------------------------------------------------------------------------
# FITS
# ----------------------------------------------------------------------------
include_directories( SYSTEM ${CIT_ROOT}/heasarc/include )
find_library(CCFITS_LIB CCfits NAMES libCCfits PATHS ${CIT_ROOT}/heasarc/lib /usr/lib/x86_64-linux-gnu)
find_library(CFITS_LIB cfitsio NAMES libcfitsio PATHS ${CIRBASE_DIR}/heasarc/lib /usr/lib/x86_64-linux-gnu)
set(CCFITS_LIB ${CIT_ROOT}/heasarc/lib/libCCfits.so CACHE FILEPATH "Path to CCfits library" FORCE)
set(CFITS_LIB ${CIT_ROOT}/heasarc/lib/libcfitsio.so CACHE FILEPATH "Path to cfitsio library" FORCE)

# ----------------------------------------------------------------------------
# new FITS engine requires Boost
# ----------------------------------------------------------------------------
find_package(Boost REQUIRED PATHS "${CIT_ROOT}/boost" COMPONENTS thread chrono)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

# ----------------------------------------------------------------------------

add_compile_options( -O3 -march=native -DNDEBUG )

add_executable(camerad
        ${CAMERA_DIR}/camerad.cpp
        ${CAMERA_DIR}/camera.cpp
        ${CAMERA_DIR}/archon.cpp
        ${CAMERA_DIR}/nirc2.cpp
        )

target_link_libraries(camerad
        network
        logentry
        common
        utilities
        pthread
        ${CMAKE_THREAD_LIBS_INIT}
        ${CCFITS_LIB}
        ${CFITS_LIB}
        ${OpenCV_LIBS}
        Boost::thread
        Boost::chrono
        )

# ----------------------------------------------------------------------------
# generate githash.h file before camerad is built
# ----------------------------------------------------------------------------
file(TOUCH "${CMAKE_BINARY_DIR}/githash.h")

add_custom_target(githash_target ALL
        COMMAND ${CMAKE_COMMAND}
            -DCMAKE_SOURCE_DIR=${CAMERA_DIR}
            -DOUTPUT_FILE=${CMAKE_BINARY_DIR}/githash.h
            -P ${CAMERA_DIR}/generate_githash.cmake
        COMMENT "checking githash"
)
add_dependencies(camerad githash_target)
target_include_directories(camerad PRIVATE ${CMAKE_BINARY_DIR})
target_sources(camerad PRIVATE ${CMAKE_BINARY_DIR}/githash.h)

# ----------------------------------------------------------------------------
# cURL is not used here, so it's not strictly required, but cfitsio can
# use it. If cfitsio was built with libcurl then it has to be linked here.
# If it's installed on the system then link it, but don't make it mandatory.
# ----------------------------------------------------------------------------
#
include(FindCURL)

if (CURL_FOUND STREQUAL "FALSE")
        message(STATUS "cURL was not found but may be needed by some systems if built into cfitsio.\n   If you experience undefined curl references during linking \n   then please your cURL installation.")
else()
        target_link_libraries(camerad ${CURL_LIBRARIES})
endif()
