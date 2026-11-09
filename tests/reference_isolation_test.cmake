# Exercise the configure-time guard without changing the actual project targets.
set(probe "${BINARY_DIR}/tests/reference-isolation-probe")
file(MAKE_DIRECTORY "${probe}/source")
file(WRITE "${probe}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.21)
project(IsolationProbe LANGUAGES NONE)
include("${GUARD_FILE}")
add_library(probe INTERFACE)
if(BAD_INPUT)
    target_sources(probe INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/legacy/reference/old.cpp")
endif()
yacreader_check_reference_isolation("${CMAKE_CURRENT_SOURCE_DIR}")
]=])
foreach(bad OFF ON)
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${probe}/source" -B "${probe}/build-${bad}"
        "-DGUARD_FILE=${SOURCE_DIR}/cmake/ReferenceIsolation.cmake" "-DBAD_INPUT=${bad}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(bad AND (result EQUAL 0 OR NOT "${output}${error}" MATCHES "Reference isolation:"))
        message(FATAL_ERROR "Reference source was not rejected: ${output}${error}")
    elseif(NOT bad AND NOT result EQUAL 0)
        message(FATAL_ERROR "Clean source failed: ${output}${error}")
    endif()
endforeach()
