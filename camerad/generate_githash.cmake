# creates include file with git hash
execute_process(
    COMMAND git describe --always --dirty --abbrev=7
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()

set(NEW_CONTENT "#pragma once\n#define GIT_HASH \"${GIT_HASH}\"\n")

# Only write if content is different
if(EXISTS ${OUTPUT_FILE})
    file(READ ${OUTPUT_FILE} OLD_CONTENT)
    if(NOT "${OLD_CONTENT}" STREQUAL "${NEW_CONTENT}")
        message(STATUS "creating file ${OUTPUT_FILE}")
        file(WRITE ${OUTPUT_FILE} "${NEW_CONTENT}")
    endif()
else()
    message(STATUS "creating file ${OUTPUT_FILE}")
    file(WRITE ${OUTPUT_FILE} "${NEW_CONTENT}")
endif()
