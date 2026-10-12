# Check of check_header_check.cmake's CMake guard. The script reads ctest's
# test list with string(JSON), which CMake has from 3.19, while the tests of
# a build without TADAH_ENABLE_NFP accept CMake 3.16 (HPO needs 3.24; the
# build itself 3.15). On an older CMake the script reports a skip and stops,
# instead of failing on a command it does not know. A running CMake cannot be made
# older (-DCMAKE_VERSION=... does not change CMAKE_VERSION): the script's
# -DAS_CMAKE_VERSION=<v> makes its guard decide as CMake <v> would, and this
# check runs it with no other argument:
#
#   CASE=old      AS_CMAKE_VERSION 3.18.4 and 3.15 (the umbrella's minimum;
#                 its tests stop earlier there, in cmake-tools' Catch.cmake):
#                 exit 0, and the output is exactly the line
#                 "SKIPPED: check_header_check.cmake needs CMake 3.19 or
#                 later (string(JSON)); this is CMake <v>": the script
#                 stopped before its argument checks
#   CASE=minimum  AS_CMAKE_VERSION 3.19 and 3.24.2: no SKIPPED line, and
#                 the script went on to its argument checks (it fails with
#                 "check_header_check.cmake: -DCASE=... is required")
#
#   cmake -DCASE=<case> -DSCRIPT=<check_header_check.cmake> -P check_cmake_minimum.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v CASE SCRIPT)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_cmake_minimum.cmake: -D${v}=... is required")
  endif()
endforeach()
if(CASE STREQUAL "old")
  set(versions 3.18.4 3.15)
elseif(CASE STREQUAL "minimum")
  set(versions 3.19 3.24.2)
else()
  message(FATAL_ERROR "check_cmake_minimum.cmake: unknown CASE '${CASE}'")
endif()

set(nfail 0)
macro(fail)
  string(CONCAT what ${ARGV})
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

foreach(v IN LISTS versions)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DAS_CMAKE_VERSION=${v}" -P "${SCRIPT}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  message(STATUS "---- AS_CMAKE_VERSION ${v} (exit ${rc}) ----\n${out}")
  set(skip_line "SKIPPED: check_header_check.cmake needs CMake 3.19 or later (string(JSON)); this is CMake ${v}")
  if(CASE STREQUAL "old")
    if(NOT rc EQUAL 0)
      fail("AS_CMAKE_VERSION ${v}: exit ${rc}, expected 0 (a skip)")
    endif()
    string(STRIP "${out}" stripped)
    if(NOT stripped STREQUAL skip_line)
      message(NOTICE "expected exactly: ${skip_line}")
      fail("AS_CMAKE_VERSION ${v}: the output (above) is not exactly the "
           "expected line")
    endif()
  else()
    string(FIND "${out}" "SKIPPED:" at)
    if(NOT at EQUAL -1)
      fail("AS_CMAKE_VERSION ${v}: the script reported a skip")
    endif()
    # CMake lays out the FATAL_ERROR text: compare with the blanks and
    # line breaks collapsed.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
    string(FIND "${flat}" "check_header_check.cmake: -DCASE=... is required"
           at)
    if(rc EQUAL 0 OR at EQUAL -1)
      fail("AS_CMAKE_VERSION ${v}: exit ${rc}, expected the script's argument "
           "check to fail ('check_header_check.cmake: -DCASE=... is required')")
    endif()
  endif()
endforeach()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed for CASE=${CASE}")
endif()
message(STATUS "CASE=${CASE}: ok")
