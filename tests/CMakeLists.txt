cmake_minimum_required(VERSION 3.12)

set(PROJECT_TESTS_DIR ${PROJECT_BASE_DIR}/tests)

# Find the Google Test library
find_package(GTest)

add_executable(
        run_unit_tests utility_tests.cpp) # List all unit test source files here

# Link the Google Test library
target_link_libraries(run_unit_tests
        gtest
        gtest_main
        pthread
        utilities
)
