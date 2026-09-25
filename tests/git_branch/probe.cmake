# Runs cmake-tools' tadah_get_git_branch() once and prints what it chose.
# Driven by check_git_branch.cmake, which sets the environment and the
# working directory (CMAKE_SOURCE_DIR in script mode, the checkout the
# function asks git about):
#
#   cmake -DTADAH_CMAKE_DIR=<cmake-tools> [-DTADAH_GIT_BRANCH=<preset>]
#         -P probe.cmake
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED TADAH_CMAKE_DIR OR "${TADAH_CMAKE_DIR}" STREQUAL "")
  message(FATAL_ERROR "probe.cmake: -DTADAH_CMAKE_DIR=... is required")
endif()
list(APPEND CMAKE_MODULE_PATH "${TADAH_CMAKE_DIR}/cmake")
include(TadahDevelop)

tadah_get_git_branch(TADAH_GIT_BRANCH)
message(STATUS "TADAH_GIT_BRANCH=[${TADAH_GIT_BRANCH}]")
