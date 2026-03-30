# ----------------------------------------------------------------------------
# emulator/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( EMULATOR_DIR ${PROJECT_BASE_DIR}/emulator )

add_compile_options( -Wall -O1 -Wno-variadic-macros -ggdb )

include_directories( ${PROJECT_BASE_DIR}/utils
                     ${PROJECT_BASE_DIR}/camerad
                     ${PROJECT_BASE_DIR}/common )

find_package(PkgConfig REQUIRED)
pkg_check_modules(CFITSIO REQUIRED cfitsio)

if( ${INTERFACE_TYPE} STREQUAL "Archon" )
        add_definitions(-DSTA_ARCHON)
        set( EMULATOR_TARGET emulatorArchon )
        list(APPEND EMULATOR_SOURCE
                "${EMULATOR_DIR}/emulator-archon.cpp"
                )
        add_library(emulatorInterface STATIC
                ${EMULATOR_DIR}/emulator-archon.cpp
                )
        target_link_libraries(emulatorInterface common utilities)
        target_include_directories(emulatorInterface PRIVATE ${CFITSIO_INCLUDE_DIRS})

add_library(${EMULATOR_TARGET} ${EMULATOR_SOURCE})

target_include_directories(${EMULATOR_TARGET} PUBLIC ${PROJECT_INCL_DIR})
target_include_directories(${EMULATOR_TARGET} PRIVATE ${CFITSIO_INCLUDE_DIRS})
target_link_libraries(${EMULATOR_TARGET} common utilities)

add_executable(emulator
        ${EMULATOR_DIR}/emulator-server.cpp
        )

target_link_libraries(emulator
        network
        logentry
        utilities
        common
        ${EMULATOR_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CFITSIO_LIBRARIES}
        )
target_include_directories(emulator PRIVATE ${CFITSIO_INCLUDE_DIRS})
target_link_directories(emulator PRIVATE ${CFITSIO_LIBRARY_DIRS})
elseif( ${INTERFACE_TYPE} STREQUAL "AstroCam" )
        message( STATUS "emulator not implemented for AstroCam" )
else()
        message( FATAL_ERROR " unrecognized or empty INTERFACE_TYPE \"" ${INTERFACE_TYPE} "\": expected Archon or AstroCam" )
endif()

