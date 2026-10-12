# Configure-time check for build options removed from Tadah
# (cmake-tools/cmake/TadahCompileOptions.cmake), run on the throw-away
# project in project/:
#
#   CASE=on          -DTADAH_BUILD_MPI=ON       -> configure fails, and the
#                                                  message names the removal
#                                                  and TADAH_HPO_MPI
#   CASE=preset_on   the same through a -C preset
#   CASE=off         -DTADAH_BUILD_MPI=OFF      -> configures
#   CASE=preset_off  set(TADAH_BUILD_MPI OFF CACHE BOOL ...) in a -C preset,
#                    as in older presets       -> configures
#   CASE=unset       no TADAH_BUILD_MPI         -> configures
#
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGENERATOR=<CMake generator> -DMAKE_PROGRAM=<build tool>
#         -DCXX=<C++ compiler> -P check_removed_option.cmake
cmake_minimum_required(VERSION 3.18)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR GENERATOR MAKE_PROGRAM CXX)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_removed_option.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(preset "${WORK_DIR}/preset.cmake")

set(extra "")
if(CASE STREQUAL "on")
  set(extra "-DTADAH_BUILD_MPI=ON")
  set(expect_ok FALSE)
elseif(CASE STREQUAL "preset_on")
  file(WRITE "${preset}" "set(TADAH_BUILD_MPI ON CACHE BOOL \"\")\n")
  set(extra "-C${preset}")
  set(expect_ok FALSE)
elseif(CASE STREQUAL "off")
  set(extra "-DTADAH_BUILD_MPI=OFF")
  set(expect_ok TRUE)
elseif(CASE STREQUAL "preset_off")
  file(WRITE "${preset}"
    "set(TADAH_BUILD_MPI OFF CACHE BOOL\n"
    "  \"Build MPI-enabled Tadah (experimental).\")\n")
  set(extra "-C${preset}")
  set(expect_ok TRUE)
elseif(CASE STREQUAL "unset")
  set(expect_ok TRUE)
else()
  message(FATAL_ERROR "check_removed_option.cmake: unknown CASE '${CASE}'")
endif()

# WORK_DIR was removed above: the build directory is new.
execute_process(
  COMMAND "${CMAKE_COMMAND}" ${extra}
          -S "${CMAKE_CURRENT_LIST_DIR}/project" -B "${WORK_DIR}/build"
          -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
          "-DCMAKE_CXX_COMPILER=${CXX}"
          -DFETCHCONTENT_FULLY_DISCONNECTED=ON
          "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${TADAH_CMAKE_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)

set(nfail 0)
macro(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

string(FIND "${out}" "TADAH_REMOVED_OPTION_CASE_CONFIGURED" configured)
# CMake wraps FATAL_ERROR text; compare with whitespace runs collapsed.
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
if(expect_ok)
  if(NOT rc EQUAL 0)
    fail("configure failed (exit ${rc})")
  endif()
  if(configured EQUAL -1)
    fail("the project did not finish configuring")
  endif()
  if(flat MATCHES "TADAH_BUILD_MPI")
    fail("a TADAH_BUILD_MPI message was printed")
  endif()
else()
  if(rc EQUAL 0)
    fail("configure succeeded")
  endif()
  if(NOT configured EQUAL -1)
    fail("the project finished configuring")
  endif()
  foreach(text
      "TADAH_BUILD_MPI=ON: the legacy MPI training build"
      "was removed in Tadah 1.6.0"
      "configure with -DTADAH_HPO_MPI=ON instead"
      "Set -DTADAH_BUILD_MPI=OFF")
    string(FIND "${flat}" "${text}" at)
    if(at EQUAL -1)
      fail("the message lacks '${text}'")
    endif()
  endforeach()
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed for CASE=${CASE}; "
                      "configure output:\n${out}")
endif()
message(STATUS "CASE=${CASE}: ok")
