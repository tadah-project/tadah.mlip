# Checks the CMake that the TADAH-SKIP marker of cmake-tools needs
# (testing/TadahTestSkip.cmake, which Catch.cmake and the discovery script
# include): CTest reports a test case that calls TADAH_SKIP() as Skipped
# through the test property SKIP_REGULAR_EXPRESSION, which CTest has from
# 3.16; an older CTest ignores the property and reports the test Passed.
# The module is included in a child `cmake -P` as CMake 3.15.7 would include
# it (the child sets CMAKE_VERSION so first; a -D cannot, a running CMake
# defines it), as 3.16.0, and as this CMake:
#   - 3.15.7: the child fails with a message that names CMake 3.16, the
#     SKIP_REGULAR_EXPRESSION property and the version it runs as, and says
#     what to do (BUILD_TESTING OFF);
#   - 3.16.0 and this CMake: the child succeeds and the module defines
#     TADAH_TEST_SKIP_REGULAR_EXPRESSION, the marker at the start of a line.
#
#   cmake -DMODULE=<TadahTestSkip.cmake> -DWORK_DIR=<scratch>
#         -P check_skip_minimum.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v MODULE WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_skip_minimum.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(WRITE "${WORK_DIR}/probe.cmake" [=[
if(DEFINED AS_CMAKE_VERSION)
  set(CMAKE_VERSION "${AS_CMAKE_VERSION}")
endif()
include("${MODULE}")
string(REPLACE "\n" "<LF>" shown "${TADAH_TEST_SKIP_REGULAR_EXPRESSION}")
message(STATUS "REGEX=[${shown}]")
]=])

set(nfail 0)
macro(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

# Runs the probe as CMake `as` (empty: as this CMake); sets rc and flat (the
# output with each run of whitespace one space: CMake wraps an error).
function(probe as)
  set(as_arg "")
  if(NOT as STREQUAL "")
    set(as_arg "-DAS_CMAKE_VERSION=${as}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DMODULE=${MODULE}" ${as_arg}
            -P "${WORK_DIR}/probe.cmake"
    RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE out)
  string(REGEX REPLACE "[ \t\r\n]+" " " f "${out}")
  set(rc "${r}" PARENT_SCOPE)
  set(flat "${f}" PARENT_SCOPE)
endfunction()

probe(3.15.7)
if(rc EQUAL 0)
  fail("as CMake 3.15.7 the module was included (exit 0): ${flat}")
endif()
foreach(want "need CMake 3.16 or later" "SKIP_REGULAR_EXPRESSION"
             "This is CMake 3.15.7" "-DBUILD_TESTING=OFF")
  string(FIND "${flat}" "${want}" at)
  if(at LESS 0)
    fail("as CMake 3.15.7 the output does not say '${want}': ${flat}")
  endif()
endforeach()

foreach(as 3.16.0 "")
  probe("${as}")
  if(as STREQUAL "")
    set(as "this CMake (${CMAKE_VERSION})")
  endif()
  string(FIND "${flat}" "REGEX=[(^|<LF>)TADAH-SKIP: ]" at)
  if(NOT rc EQUAL 0 OR at LESS 0)
    fail("as ${as}: exit ${rc}, expected 0 and REGEX=[(^|<LF>)TADAH-SKIP: ]: "
         "${flat}")
  endif()
endforeach()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed")
endif()
message(STATUS "TadahTestSkip.cmake: refused as CMake 3.15.7, included as "
               "3.16.0 and ${CMAKE_VERSION}")
