cmake_minimum_required(VERSION 3.15)
# Initial project() call; VERSION is set below after the version reader
# from cmake-tools is available (single-source-of-truth: blueprint TOML).
project(Tadah.CORE LANGUAGES CXX)

# 0. Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
    "In-source builds are not allowed.\n"
    "The build directory must differ from the source directory (for example: cmake -S . -B build).")
endif()

# 1. Global settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type for single-config generators (Ninja/Unix Makefiles).
set(TADAH_DEFAULT_BUILD_TYPE "RelWithDebInfo" CACHE STRING
    "Default build type for single-config generators when CMAKE_BUILD_TYPE is not set.")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to ${TADAH_DEFAULT_BUILD_TYPE}")
  set(CMAKE_BUILD_TYPE "${TADAH_DEFAULT_BUILD_TYPE}" CACHE STRING
      "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel).")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
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
# 2. Bring in the shared compile/link options
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

# Read version from CORE's local blueprint and re-state project() with VERSION.
if(NOT COMMAND tadah_read_version_from_blueprint)
  message(FATAL_ERROR "cmake-tools is too old: tadah_read_version_from_blueprint() is missing")
endif()
tadah_read_version_from_blueprint(TADAH_VERSION
  BLUEPRINT_FILE ${PROJECT_SOURCE_DIR}/blueprint/blueprint_tadah.toml
  REQUIRED)
project(Tadah.CORE VERSION ${TADAH_VERSION} LANGUAGES CXX)
message(STATUS "Tadah.CORE version: ${TADAH_VERSION}")

option(TADAH_ENABLE_ASAN "Enable AddressSanitizer (-fsanitize=address -fno-omit-frame-pointer)" OFF)

# 3. Git branch information, for informational purposes
tadah_get_git_branch(GIT_BRANCH)
message(STATUS "${PROJECT_NAME} module is using GIT_BRANCH:  ${GIT_BRANCH}")

#4. Generate blueprint files
# The blueprint is embedded verbatim (it also carries the version printed by
# `tadah --version`), so editing it must re-run the configure step.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/blueprint/blueprint_tadah.toml)
file(READ ${PROJECT_SOURCE_DIR}/blueprint/blueprint_tadah.toml
     BLUEPRINT_CONTENT)

configure_file(
  ${PROJECT_SOURCE_DIR}/blueprint/embed/blueprint_tadah.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/blueprint_tadah.hpp
  @ONLY)

configure_file(
  ${PROJECT_SOURCE_DIR}/blueprint/embed/blueprint_tadah.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/blueprint_tadah.cpp
  @ONLY)

# 5. External dependencies
add_subdirectory(external/toml11)

# 6. Library target
file(GLOB CORE_SRC CONFIGURE_DEPENDS "src/*.cpp")
add_library(tadah.core 
    ${CORE_SRC}
    )
target_sources(tadah.core PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/blueprint_tadah.cpp)

find_package(LAPACK)
target_link_libraries(tadah.core
  PUBLIC 
    tadah::compile_options
    ${LAPACK_LIBRARIES}
    toml11::toml11
)
target_include_directories(tadah.core
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${PROJECT_SOURCE_DIR}/external/boost/include)

if(TADAH_ENABLE_ASAN)
  target_compile_options(tadah.core PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_libraries(tadah.core PUBLIC -fsanitize=address)
endif()

if(TADAH_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  target_link_libraries(tadah.core PRIVATE OpenMP::OpenMP_CXX)
endif()

# 7. Tests
tadah_add_tests()

# 8. Installation rules
install(TARGETS tadah.core
  EXPORT  tadahCoreTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(DIRECTORY include/
  ${CMAKE_CURRENT_BINARY_DIR}/generated/
  DESTINATION include)

install(EXPORT tadahCoreTargets
        NAMESPACE tadah::
        DESTINATION lib/cmake/tadah.core)
