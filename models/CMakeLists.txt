cmake_minimum_required(VERSION 3.15)
project(Tadah.MODELS LANGUAGES CXX)

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

# 2. Bring in the shared compile/link options
# >>> build support (the same block in every build file of the repository
#     but for the path and the name)
# The build support is the directory cmake at the root of this repository;
# the file read here adds it to the build (tadah::compile_options, the
# tadah_* functions).
get_filename_component(_tadah_here "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
if(NOT EXISTS "${_tadah_here}/../cmake/TadahRepository.cmake")
  message(FATAL_ERROR
    "The module MODELS is built from inside the Tadah! repository, which holds "
    "its build support in '${_tadah_here}/../cmake': TadahRepository.cmake is "
    "not there. A copy of this directory alone cannot be built.")
endif()
include("${_tadah_here}/../cmake/TadahRepository.cmake")
unset(_tadah_here)
# <<< build support

# Declare the BUILD_TESTING switch (default OFF) before any sub-module
# is added, so a parent project's value propagates down. See
# cmake/TadahTesting.cmake.
tadah_declare_testing()

option(TADAH_ENABLE_ASAN "Enable AddressSanitizer (-fsanitize=address -fno-omit-frame-pointer)" OFF)

# 3. The module this one builds on: the directory of its name in this
# repository.
tadah_add_module(Tadah.CORE core)

# Single-source-of-truth version (from CORE's blueprint TOML). Read after
# the Tadah modules this one builds on are added, so that CORE's blueprint
# is found; then re-state project().
tadah_read_version_from_blueprint(TADAH_VERSION)
project(Tadah.MODELS VERSION ${TADAH_VERSION} LANGUAGES CXX)
message(STATUS "Tadah.MODELS version: ${TADAH_VERSION}")

file(GLOB MODELS_SRC CONFIGURE_DEPENDS "src/*.cpp")
add_library(tadah.models ${MODELS_SRC})
target_include_directories(tadah.models
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(tadah.models
  PUBLIC
    tadah::compile_options    # get warnings/asan/-pg flags
    tadah.core)

if(TADAH_ENABLE_ASAN)
  target_compile_options(tadah.models PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_libraries(tadah.models PUBLIC -fsanitize=address)
endif()

if(TADAH_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  target_link_libraries(tadah.models PUBLIC OpenMP::OpenMP_CXX)
endif()

tadah_add_tests()

# Installation rules
install(TARGETS tadah.models
        EXPORT  tadahModelsTargets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY include/ DESTINATION include)
install(EXPORT tadahModelsTargets
        NAMESPACE tadah::
        DESTINATION lib/cmake/tadah.models)
