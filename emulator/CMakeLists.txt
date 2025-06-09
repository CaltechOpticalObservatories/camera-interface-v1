# ----------------------------------------------------------------------------
# emulator/CMakeLists.txt
# ----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.12 )
add_definitions( -Wall -Wno-variadic-macros)


include_directories( ${CMAKE_SOURCE_DIR}/utils 
                     ${CMAKE_SOURCE_DIR}/camerad 
                     ${CMAKE_SOURCE_DIR}/common )

if( ${INTERFACE_TYPE} STREQUAL "Archon" )
        add_definitions(-DSTA_ARCHON)
        set( EMULATOR_TARGET emulatorArchon )
        list(APPEND EMULATOR_SOURCE
                "emulator-archon.cpp"
                )
        add_library(emulatorInterface STATIC
                emulator-archon.cpp
                )

add_library(${EMULATOR_TARGET} ${EMULATOR_SOURCE})
target_link_libraries(${EMULATOR_TARGET} PUBLIC cfitsio CCfits)
target_link_libraries(emulatorInterface PUBLIC cfitsio CCfits)

add_executable(emulator emulator-server.cpp
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

