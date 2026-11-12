# ====================================================================
#  Common project setup for all Tadah repositories
#
#  Call once, immediately after project():
#
#     cmake_minimum_required(VERSION 3.15)
#     project(Tadah.FOO LANGUAGES CXX)
#     ...                           # the cmake-tools block
#     tadah_project_setup()
#
#  Handles:
#   • In-source build guard
#   • C++17 standard
#   • Default build type (RelWithDebInfo)
#   • Shared-library RPATH + fPIC
#   • TADAH_DEVELOP verbose diagnostics
# ====================================================================

function(tadah_project_setup)

  # ---- In-source build guard ------------------------------------
  if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
      "In-source builds are not allowed.\n"
      "The build directory must differ from the source directory (for example: cmake -S . -B build).")
  endif()

  # ---- C++ standard ---------------------------------------------
  set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
  set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
  set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)

  # ---- Default build type (single-config generators) ------------
  set(TADAH_DEFAULT_BUILD_TYPE "RelWithDebInfo" CACHE STRING
      "Default build type when CMAKE_BUILD_TYPE is not set.")

  if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    message(STATUS "[${PROJECT_NAME}] CMAKE_BUILD_TYPE not set; defaulting to ${TADAH_DEFAULT_BUILD_TYPE}")
    set(CMAKE_BUILD_TYPE "${TADAH_DEFAULT_BUILD_TYPE}" CACHE STRING
        "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
  endif()

  # ---- TADAH_DEVELOP verbose diagnostics ------------------------
  if(TADAH_DEVELOP)
    set(CMAKE_WARN_DEPRECATED ON PARENT_SCOPE)
  endif()

  # ---- Shared library RPATH + fPIC ------------------------------
  if(BUILD_SHARED_LIBS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    set(CMAKE_SKIP_BUILD_RPATH FALSE PARENT_SCOPE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE PARENT_SCOPE)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" PARENT_SCOPE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE PARENT_SCOPE)
  endif()

  # ---- Summary line (one line per module) -----------------------
  message(STATUS "[${PROJECT_NAME}] ${CMAKE_BUILD_TYPE}")

endfunction()
