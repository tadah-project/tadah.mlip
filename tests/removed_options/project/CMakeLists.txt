# Throw-away project driven by ../check_removed_option.cmake. It brings in
# cmake-tools with FetchContent, the source directory overridden to the
# cmake-tools this build uses, which runs cmake/TadahCompileOptions.cmake
# and its check for removed build options.
#
# -DTADAH_REMOVED_OPTION_CASE_SETS_VARIABLE=ON: this build file sets the
# removed TADAH_GIT_BRANCH as a variable before it adds cmake-tools, as a
# build file that adds Tadah! to a larger build could.
cmake_minimum_required(VERSION 3.15)
project(tadah_removed_option_case CXX)

option(TADAH_REMOVED_OPTION_CASE_SETS_VARIABLE
  "Set the variable TADAH_GIT_BRANCH before cmake-tools is added" OFF)
if(TADAH_REMOVED_OPTION_CASE_SETS_VARIABLE)
  set(TADAH_GIT_BRANCH "from-a-build-file")
endif()

include(FetchContent)
FetchContent_Declare(tadah_cmake
  URL "file:///tadah-removed-option-never-downloaded"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(tadah_cmake)

if(NOT TARGET tadah::compile_options)
  message(FATAL_ERROR "cmake-tools did not define tadah::compile_options")
endif()
message(STATUS "TADAH_REMOVED_OPTION_CASE_CONFIGURED")
