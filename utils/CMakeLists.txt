# ----------------------------------------------------------------------------
# utils/CMakeLists.txt
# ----------------------------------------------------------------------------
#cmake_minimum_required(VERSION 3.12)

add_definitions(-Wall -Wno-variadic-macros)

add_library(utilities utilities.cpp md5)

add_library(logentry  logentry.cpp)

target_link_libraries(logentry $<IF:$<PLATFORM_ID:Darwin>,,stdc++fs>)

add_library(network STATIC network.cpp)

add_library(md5 STATIC md5.cpp)

add_executable(listener listener.cpp)

add_executable(socksend  sendcmd.cpp)
