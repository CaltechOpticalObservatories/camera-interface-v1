# ----------------------------------------------------------------------------
# utils/CMakeLists.txt
# ----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12)

set(PROJECT_UTILS_DIR ${PROJECT_BASE_DIR}/utils)

add_compile_options(-Wall -O1 -Wno-variadic-macros -ggdb)

add_library(utilities STATIC
        ${PROJECT_UTILS_DIR}/utilities.cpp
        md5
)
target_link_libraries(utilities nlohmann_json::nlohmann_json)

add_library(logentry STATIC
        ${PROJECT_UTILS_DIR}/logentry.cpp
)
target_link_libraries(logentry nlohmann_json::nlohmann_json $<IF:$<PLATFORM_ID:Darwin>,,stdc++fs>)

add_library(network STATIC
        ${PROJECT_UTILS_DIR}/network.cpp
)
target_link_libraries(network nlohmann_json::nlohmann_json)

add_library(md5 STATIC
        ${PROJECT_UTILS_DIR}/md5.cpp
)

add_executable(listener
        ${PROJECT_UTILS_DIR}/listener.cpp
)

add_executable(socksend
        ${PROJECT_UTILS_DIR}/sendcmd.cpp
)
