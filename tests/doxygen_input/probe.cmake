# Checks tadah_doxygen_input() (cmake/doxygen_input.cmake), which names the
# Doxygen INPUT of the `docs` target (docs/Doxyfile.in): the include/ trees
# of the Tadah modules the build configures -- CORE, MODELS and MLIP always,
# HPO only with TADAH_ENABLE_NFP (it is fetched only then). A module that is
# needed but not configured, or has no include/ directory, is a configure
# error; nothing is ever listed from an empty source directory (which gave
# a bare "/include" in a build without NFP, such as the Read the Docs one).
#
#   cmake -DCASE=<case> -DFUNCTIONS=<cmake/doxygen_input.cmake>
#         -DWORK_DIR=<scratch> [-DDOXYFILE=<configured Doxyfile>]
#         -P probe.cmake
#
# The function cases print INPUT=[...] with WORK_DIR written as <W>; the
# CTest entry matches it (or the error) with PASS_REGULAR_EXPRESSION. The
# 'configured' case checks the Doxyfile this build configured: every INPUT
# entry is an existing directory.
cmake_minimum_required(VERSION 3.15)

foreach(v CASE FUNCTIONS WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "probe.cmake: -D${v}=... is required")
  endif()
endforeach()

if(CASE STREQUAL "configured")
  if(NOT DEFINED DOXYFILE OR NOT EXISTS "${DOXYFILE}")
    message(FATAL_ERROR "probe.cmake: -DDOXYFILE=<configured Doxyfile> is required")
  endif()
  file(STRINGS "${DOXYFILE}" input_lines REGEX "^INPUT[ \t]*=")
  list(LENGTH input_lines n)
  if(NOT n EQUAL 1)
    message(FATAL_ERROR "${DOXYFILE}: expected one INPUT line, found ${n}")
  endif()
  string(REGEX REPLACE "^INPUT[ \t]*=[ \t]*" "" input "${input_lines}")
  separate_arguments(entries UNIX_COMMAND "${input}")
  list(LENGTH entries n)
  if(n EQUAL 0)
    message(FATAL_ERROR "${DOXYFILE}: INPUT is empty")
  endif()
  foreach(e IN LISTS entries)
    if(NOT IS_ABSOLUTE "${e}" OR NOT IS_DIRECTORY "${e}"
       OR NOT e MATCHES "/include$" OR e STREQUAL "/include")
      message(FATAL_ERROR "${DOXYFILE}: INPUT entry '${e}' is not the "
                          "include/ directory of a configured module")
    endif()
  endforeach()
  message(STATUS "Doxyfile INPUT: ${n} include/ directories, all present")
  return()
endif()

include("${FUNCTIONS}")

file(REMOVE_RECURSE "${WORK_DIR}")
foreach(m CORE MODELS MLIP HPO)
  file(MAKE_DIRECTORY "${WORK_DIR}/${m}/include")
  set(Tadah.${m}_SOURCE_DIR "${WORK_DIR}/${m}")
endforeach()

if(CASE STREQUAL "nfp_off")
  # No NFP: HPO is not fetched, so its source directory is empty.
  set(TADAH_ENABLE_NFP OFF)
  set(Tadah.HPO_SOURCE_DIR "")
elseif(CASE STREQUAL "nfp_on")
  set(TADAH_ENABLE_NFP ON)
elseif(CASE STREQUAL "nfp_on_without_hpo")
  set(TADAH_ENABLE_NFP ON)
  unset(Tadah.HPO_SOURCE_DIR)
elseif(CASE STREQUAL "no_include")
  set(TADAH_ENABLE_NFP OFF)
  file(REMOVE_RECURSE "${WORK_DIR}/MODELS/include")
else()
  message(FATAL_ERROR "probe.cmake: unknown CASE '${CASE}'")
endif()

tadah_doxygen_input(input)
string(REPLACE "${WORK_DIR}" "<W>" shown "${input}")
message(STATUS "INPUT=[${shown}]")
