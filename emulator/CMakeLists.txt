# ----------------------------------------------------------------------------
# emulator/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )

set( EMULATOR_DIR ${PROJECT_BASE_DIR}/emulator )

add_definitions( -Wall -ansi -O1 -Wno-variadic-macros -std=c++17 -ggdb )

include_directories( ${PROJECT_BASE_DIR}/utils 
                     ${PROJECT_BASE_DIR}/camerad 
                     ${PROJECT_BASE_DIR}/common )

if( ${INTERFACE_TYPE} STREQUAL "Archon" )
        add_definitions(-DSTA_ARCHON)
        set( EMULATOR_TARGET emulatorArchon )
        list(APPEND EMULATOR_SOURCE
                "${EMULATOR_DIR}/emulator-archon.cpp"
                )
        add_library(emulatorInterface STATIC
                ${EMULATOR_DIR}/emulator-archon.cpp
                )

add_library(${EMULATOR_TARGET} ${EMULATOR_SOURCE})

target_include_directories(${EMULATOR_TARGET} PUBLIC ${PROJECT_INCL_DIR})

add_executable(emulator
        ${EMULATOR_DIR}/emulator-server.cpp
        )

target_link_libraries(emulator
        network
        logentry
        utilities
        common
        camera
        ${EMULATOR_TARGET}
        ${CMAKE_THREAD_LIBS_INIT}
        )
elseif( ${INTERFACE_TYPE} STREQUAL "AstroCam" )
        message( STATUS "emulator not implemented for AstroCam" )
else()
        message( FATAL_ERROR " unrecognized or empty INTERFACE_TYPE \"" ${INTERFACE_TYPE} "\": expected Archon or AstroCam" )
endif()

