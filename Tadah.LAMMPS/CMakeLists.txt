cmake_minimum_required(VERSION 3.12)

project(Tadah.LAMMPS LANGUAGES CXX)

option(TADAH_MARCH_NATIVE "Build with -march=native" OFF)
option(TADAH_ENABLE_FAST_MATH "Enable fast math optimizations" OFF)
option(TADAH_ENABLE_ASAN "Enable AddressSanitizer (-fsanitize=address -fno-omit-frame-pointer)" OFF)
option(TADAH_LAMMPS_QUIET "Reduce configure output when used as a dependency" OFF)
option(TADAH_LAMMPS_QUIET "Reduce configure output when used as a dependency" OFF)

function(tadah_lammps_status)
  if(NOT TADAH_LAMMPS_QUIET)
    message(STATUS "${ARGV}")
  endif()
endfunction()

# True when this project is the top-level build (standalone)
set(TADAH_LAMMPS_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(TADAH_LAMMPS_STANDALONE ON)
endif()

# 0. Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
    "In-source builds are not allowed.\n"
    "The build directory must differ from the source directory (for example: cmake -S . -B build).")
endif()

# 1. Global settings
if(TADAH_LAMMPS_STANDALONE AND NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# default build type for single-config generators (Ninja/Make)
if(TADAH_LAMMPS_STANDALONE)
  if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
  endif()
endif()

if(BUILD_SHARED_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  # Always full RPATH
  # use, i.e. don't skip the full RPATH for the build tree
  set(CMAKE_SKIP_BUILD_RPATH FALSE)

  # when building, don't use the install RPATH already
  # (but later on when installing)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

  # add the automatically determined parts of the RPATH
  # which point to directories outside the build tree to the install RPATH
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# 2. Bring in the shared compile/link options
include(FetchContent)
# >>> TADAH_GIT_BASE (the same block in every Tadah! repository's
#     CMakeLists.txt, compared by cmake-tools' tadah_add_git_base_check)
# Base address of the Tadah! git repositories that FetchContent clones
# (<base>/<repository>.git): -DTADAH_GIT_BASE=<address>, else the
# environment variable TADAH_GIT_BASE when it is set and not empty, else
# the public repositories. A trailing '/' is dropped.
if(NOT DEFINED TADAH_GIT_BASE)
  if(NOT "$ENV{TADAH_GIT_BASE}" STREQUAL "")
    set(_tadah_git_base "$ENV{TADAH_GIT_BASE}")
  else()
    set(_tadah_git_base "https://github.com/tadah-project")
  endif()
  set(TADAH_GIT_BASE "${_tadah_git_base}" CACHE STRING
      "Base address of the Tadah! git repositories (<base>/<repository>.git)")
  unset(_tadah_git_base)
endif()
string(REGEX REPLACE "/+$" "" TADAH_GIT_BASE "${TADAH_GIT_BASE}")
# <<< TADAH_GIT_BASE
# Developer build (-DTADAH_DEVELOP=ON): use a local cmake-tools checkout
# rather than fetching it. Skipped when the source dir is already pinned
# (e.g. -DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=...).
if(TADAH_DEVELOP AND NOT FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE
   AND DEFINED ENV{TADAH_PATH})
  foreach(_tadah_ct "$ENV{TADAH_PATH}/cmake-tools"
                    "$ENV{TADAH_PATH}/../cmake-tools")
    if(IS_DIRECTORY "${_tadah_ct}")
      get_filename_component(FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE
                             "${_tadah_ct}" ABSOLUTE)
      message(STATUS
        "TADAH_DEVELOP: cmake-tools taken from ${FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE}")
      break()
    endif()
  endforeach()
  unset(_tadah_ct)
endif()

if(NOT TARGET tadah::compile_options)
  FetchContent_Declare (
    tadah_cmake
    GIT_REPOSITORY  ${TADAH_GIT_BASE}/cmake-tools.git
    GIT_TAG         v1-dev
  )
  FetchContent_MakeAvailable(tadah_cmake)   # defines tadah::compile_options
endif()

# Declare the BUILD_TESTING switch (default OFF) before any sub-module
# is fetched, so a parent project's value propagates down. See
# cmake-tools/TadahTesting.cmake.
tadah_declare_testing()

# (option) Developer builds -DTADAH_DEVELOP=ON
tadah_use_local_module(Tadah.MD MD)  # FetchContent name + folder name

# 3. Git branch information
tadah_get_git_branch(TADAH_GIT_BRANCH)
tadah_lammps_status("${PROJECT_NAME} is using TADAH_GIT_BRANCH: ${TADAH_GIT_BRANCH}")

#4. Fetch Tadah! modules
FetchContent_Declare(
    Tadah.MD
    GIT_REPOSITORY ${TADAH_GIT_BASE}/md.git
    GIT_TAG ${TADAH_GIT_BRANCH}
)
FetchContent_MakeAvailable(Tadah.MD)

# Single-source-of-truth version (from CORE's blueprint TOML). Read after
# the Tadah dependencies are available so CORE's blueprint is found in
# every layout (sibling checkout or FetchContent); then re-state project().
if(NOT COMMAND tadah_read_version_from_blueprint)
  message(FATAL_ERROR "cmake-tools is too old: tadah_read_version_from_blueprint() is missing")
endif()
tadah_read_version_from_blueprint(TADAH_VERSION REQUIRED)
project(Tadah.LAMMPS VERSION ${TADAH_VERSION} LANGUAGES CXX)
tadah_lammps_status("Tadah.LAMMPS version: ${TADAH_VERSION}")

add_library(tadah.lammps STATIC src/lammps_tadah.cpp)
target_include_directories(tadah.lammps
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(tadah.lammps PUBLIC tadah::compile_options tadah.core tadah.models tadah.md)

if(TADAH_ENABLE_ASAN)
  target_compile_options(tadah.lammps PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_libraries(tadah.lammps PUBLIC -fsanitize=address)
endif()

if (NOT TADAH_BUILD_DOCS_ONLY)
  find_package(LAPACK)
  target_link_libraries (tadah.lammps PUBLIC ${LAPACK_LIBRARIES})
endif()

#########################################################################
if(TADAH_MARCH_NATIVE)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  else()
    tadah_lammps_status("${PROJECT_NAME} ...but -march=native is not supported")
  endif()
endif()
tadah_lammps_status("${PROJECT_NAME} Build with -march=native in ${TADAH_MARCH_NATIVE}")
#########################################################################

#########################################################################
if(TADAH_ENABLE_FAST_MATH)
  if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
    add_compile_options(-ffast-math)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/fp:fast)
  endif()
else()
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/fp:precise)
  endif()
endif()
tadah_lammps_status("${PROJECT_NAME}: Build with fast math optimisation is ${TADAH_ENABLE_FAST_MATH}")
#########################################################################

tadah_add_tests()

tadah_lammps_status("===================================================================")
tadah_lammps_status("                                                                   ")
tadah_lammps_status("${PROJECT_NAME} in now configured and ready for compilation.       ")
tadah_lammps_status("                                                                   ")
tadah_lammps_status("--------------+----------------------------------------------------")
tadah_lammps_status("Command       |   Description                                      ")
tadah_lammps_status("--------------+----------------------------------------------------")
tadah_lammps_status("              |                                                    ")
tadah_lammps_status("make          |   Compile                                          ")
tadah_lammps_status("              |                                                    ")
tadah_lammps_status("make test     |   Run testing framework                            ")
tadah_lammps_status("              |                                                    ")
tadah_lammps_status("===================================================================")
