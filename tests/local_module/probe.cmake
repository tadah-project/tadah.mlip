# Runs cmake-tools' tadah_module_directory() once for CORE and prints the
# directory it names. Driven by check_local_module.cmake, which starts it in
# the directory of the project that asks (CMAKE_CURRENT_SOURCE_DIR in script
# mode):
#
#   cmake -DTADAH_CMAKE_DIR=<cmake-tools> [-DTADAH_DEVELOP=ON]
#         [-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=<dir>] -P probe.cmake
cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED TADAH_CMAKE_DIR OR "${TADAH_CMAKE_DIR}" STREQUAL "")
  message(FATAL_ERROR "probe.cmake: -DTADAH_CMAKE_DIR=... is required")
endif()
list(APPEND CMAKE_MODULE_PATH "${TADAH_CMAKE_DIR}/cmake")
include(TadahDevelop)

tadah_module_directory(source Tadah.CORE CORE)
message(STATUS "SOURCE=[${source}]")
