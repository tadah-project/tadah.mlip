# Runs one tadah_version_reader_* case (../CMakeLists.txt): configures the
# throw-away project in project/ with CASE, the cmake-tools of the build and
# Tadah.CORE's source directory CORE (empty for the no_core case) in
# WORK_DIR, which is removed first so that no cache of an earlier run
# carries over, and passes the output on: the entry's
# PASS_REGULAR_EXPRESSION checks it.
#
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DCORE=<CORE fixture>
#         -DWORK_DIR=<scratch> -P check_version_reader.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_version_reader.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/project"
          -B "${WORK_DIR}" "-DCASE=${CASE}"
          "-DTADAH_CMAKE_DIR=${TADAH_CMAKE_DIR}"
          -DFETCHCONTENT_FULLY_DISCONNECTED=ON
          "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${CORE}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
message(STATUS "configure (exit ${rc}):\n${out}")
