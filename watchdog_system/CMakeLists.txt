# ----------------------------------------------------------------------------
# watchdog_system/CMakeLists.txt
# builds the camerad liveness watchdog
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)

set(WATCHDOG_DIR ${PROJECT_BASE_DIR}/watchdog_system)

include_directories(${PROJECT_BASE_DIR}/utils)
link_directories(${PROJECT_BASE_DIR}/lib)

add_definitions(-Wall -ansi -O2 -Wno-variadic-macros -std=c++17 -ggdb)

find_package(Threads)

add_executable(camerad-watchdog
        ${WATCHDOG_DIR}/camerad_watchdog.cpp
)

target_link_libraries(camerad-watchdog
        network
        logentry
        utilities
        ${CMAKE_THREAD_LIBS_INIT}
        pthread
)
