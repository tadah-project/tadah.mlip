# Throw-away project driven by ../check_removed_option.cmake. It brings in
# cmake-tools the way every Tadah module does (FetchContent, source directory
# overridden to the cmake-tools this build uses), which runs
# cmake/TadahCompileOptions.cmake and its check for removed build options.
cmake_minimum_required(VERSION 3.15)
project(tadah_removed_option_case CXX)

include(FetchContent)
FetchContent_Declare(tadah_cmake
  URL "file:///tadah-removed-option-never-downloaded"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(tadah_cmake)

if(NOT TARGET tadah::compile_options)
  message(FATAL_ERROR "cmake-tools did not define tadah::compile_options")
endif()
message(STATUS "TADAH_REMOVED_OPTION_CASE_CONFIGURED")
