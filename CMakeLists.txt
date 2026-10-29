cmake_minimum_required(VERSION 3.15)

include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)
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

set(TADAH "Tadah!")
# Initial project() call; VERSION is set below once CORE is fetched.
project(${TADAH} LANGUAGES CXX)

# 0. Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
    "In-source builds are not allowed.\n"
    "Create a separate build directory and run CMake from there.")
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

# Shared vs static libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(TADAH_ENABLE_NFP "Enable Nested Fitting Procedure." OFF)
option(TADAH_ENABLE_OPENMP "Enable shared-memory parallelisation via OpenMP" OFF)
option(TADAH_MARCH_NATIVE "Build with -march=native" OFF)
option(TADAH_ENABLE_FAST_MATH "Enable fast math optimizations" OFF)
option(TADAH_ENABLE_ASAN "Enable AddressSanitizer (-fsanitize=address -fno-omit-frame-pointer)" OFF)
option(TADAH_BUILD_DOCS_ONLY "Skip checks for required libraries; only build docs" OFF)

set(TADAH_LAMMPS_LIB "" CACHE FILEPATH
  "NFP: Full path to LAMMPS library (e.g. /path/to/liblammps_serial.a or liblammps.so)")

set(TADAH_LAMMPS_DIR "" CACHE PATH
  "NFP: Path to LAMMPS root directory (source or install prefix)")

set(TADAH_LAMMPS_LIBS "" CACHE STRING
  "NFP: Extra link libraries for static LAMMPS linking (CMake list, e.g. -lz;-lzstd)")

if(BUILD_SHARED_LIBS)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# 2. Bring in the shared compile/link options
include(FetchContent)
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
    GIT_TAG         v1-dev)
  FetchContent_MakeAvailable(tadah_cmake)   # defines tadah::compile_options
endif()

# Declare the BUILD_TESTING switch (default OFF) before any sub-module
# is fetched, so this top-level value propagates to every consumed
# Tadah module. See cmake-tools/TadahTesting.cmake.
tadah_declare_testing()

# (option) Developer builds -DTADAH_DEVELOP=ON
tadah_use_local_module(Tadah.MLIP MLIP)  # FetchContent name + folder name

# 3. Git branch information, for informational purposes 
tadah_get_git_branch(TADAH_GIT_BRANCH)
tadah_message(STATUS "Using TADAH_GIT_BRANCH:  ${TADAH_GIT_BRANCH}")


if(TADAH_ENABLE_NFP)
  tadah_use_local_module(Tadah.HPO HPO)  # FetchContent name + folder name
  # tadah_use_local_module(Tadah.LAMMPS Tadah.LAMMPS)  # FetchContent name + folder name
  add_compile_definitions(TADAH_ENABLE_NFP)
  FetchContent_Declare(
    Tadah.HPO
    GIT_REPOSITORY ${TADAH_GIT_BASE}/hpo.git
    GIT_TAG ${TADAH_GIT_BRANCH}
  )
  FetchContent_MakeAvailable(Tadah.HPO)

  # FetchContent_Declare(
  #   Tadah.LAMMPS
  #   GIT_REPOSITORY ${TADAH_GIT_BASE}/tadah.lammps.git
  #   GIT_TAG ${TADAH_GIT_BRANCH}
  # )
  # FetchContent_MakeAvailable(Tadah.LAMMPS)
else()
  tadah_message("Fetching ${PROJECT_NAME} modules...")
  FetchContent_Declare(
    Tadah.MLIP
    GIT_REPOSITORY ${TADAH_GIT_BASE}/mlip.git
    GIT_TAG ${TADAH_GIT_BRANCH}
  )
  FetchContent_MakeAvailable(Tadah.MLIP)
endif()

# Set the project version from the blueprint TOML (single source of truth).
# CORE has been populated by the FetchContent step above, so its blueprint
# is available at ${Tadah.CORE_SOURCE_DIR}/blueprint/blueprint_tadah.toml.
if(NOT COMMAND tadah_read_version_from_blueprint)
  message(FATAL_ERROR "cmake-tools is too old: tadah_read_version_from_blueprint() is missing")
endif()
tadah_read_version_from_blueprint(TADAH_VERSION REQUIRED)
project(${TADAH} VERSION ${TADAH_VERSION} LANGUAGES CXX)
tadah_message(STATUS "VERSION: ${TADAH_VERSION}")


file(GLOB TADAH_SRC_FILES src/*.cpp)
# mallinfo_shim.cpp is built separately (LTO off) for the --wrap interpose set up
# further below; keep it out of the main (LTO'd) source list.
list(REMOVE_ITEM TADAH_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/mallinfo_shim.cpp")
add_executable (tadah ${TADAH_SRC_FILES})
target_include_directories(tadah
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${PROJECT_SOURCE_DIR}/external/CLI11/include
  )

if(TADAH_ENABLE_NFP)
  target_link_libraries(tadah PUBLIC tadah::compile_options tadah.hpo)
else()
  target_link_libraries(tadah PUBLIC tadah::compile_options tadah.mlip)
endif()

if(TADAH_ENABLE_ASAN)
  target_compile_options(tadah PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_libraries(tadah PUBLIC -fsanitize=address)
endif()

#########################################################################
# Neutralise LAMMPS' per-run mallinfo2() memory-report heap-walk.
#
# LAMMPS Output::memory_usage() calls mallinfo2() at every run/minimize setup
# to build a memory report that pc_lammps discards (LAMMPS is created with
# -screen none -log none). mallinfo2() is an O(heap) walk under the global
# malloc lock; under the shared worker-pool fan-out (many LAMMPS instances in
# one large process heap) it dominates wall-clock and serialises the pool.
#
# We interpose it at link time with the linker's --wrap mechanism -- the wrapper
# in src/mallinfo_shim.cpp returns a zeroed struct. This keeps the stock LAMMPS
# source unpatched (no fork to maintain), needs nothing installed on deployment
# machines, and travels inside the binary. Linux/glibc only (the only place
# LAMMPS calls mallinfo2). See src/mallinfo_shim.cpp for the full rationale.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Build the wrapper in its own object library with IPO/LTO explicitly OFF: the
  # main build enables -flto, which emits slim objects with no real symbol table,
  # so the linker's --wrap cannot bind __wrap_mallinfo2. An IPO-off object library
  # yields a genuine ELF symbol and its objects are always linked into `tadah`.
  add_library(tadah_mallinfo_shim OBJECT src/mallinfo_shim.cpp)
  set_target_properties(tadah_mallinfo_shim PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
  target_link_libraries(tadah PRIVATE tadah_mallinfo_shim)
  target_link_options(tadah PRIVATE "-Wl,--wrap=mallinfo2")
endif()
#########################################################################

#########################################################################
if(TADAH_MARCH_NATIVE)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  else()
    tadah_message(STATUS "...but -march=native is not supported")
  endif()
endif()
tadah_message(STATUS "Build with -march=native in ${TADAH_MARCH_NATIVE}")
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
tadah_message(STATUS "Build with fast math optimisation is ${TADAH_ENABLE_FAST_MATH}")
#########################################################################

#########################################################################     
tadah_message(STATUS "Build with Nested Fitting Procedure support is ${TADAH_ENABLE_NFP}")
#########################################################################

#########################################################################
if(TADAH_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(tadah PUBLIC OpenMP::OpenMP_CXX)
    add_compile_definitions(TADAH_ENABLE_OPENMP)
  endif()
endif()
tadah_message(STATUS "Build with OpenMP is ${TADAH_ENABLE_OPENMP}")
#########################################################################

#########################################################################

if (NOT TADAH_BUILD_DOCS_ONLY)
  find_package(LAPACK REQUIRED)
  target_link_libraries (tadah PUBLIC ${LAPACK_LIBRARIES})
endif()

install(TARGETS tadah
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY include/ DESTINATION include)

tadah_add_tests()


find_package(Doxygen)
# INPUT: the include/ directories of the configured modules (HPO with NFP).
include(${CMAKE_SOURCE_DIR}/cmake/doxygen_input.cmake)
tadah_doxygen_input(TADAH_DOXYGEN_INPUT)
configure_file(${CMAKE_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_BINARY_DIR}/docs/Doxyfile @ONLY)
add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
)

message(STATUS "===================================================================")
message(STATUS "")
message(STATUS "${PROJECT_NAME} is now configured and ready for installation.")
message(STATUS "")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS " Command      | Description                                        ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS " make install | Install ${PROJECT_NAME}.                                  ")
message(STATUS "              |   • Binaries will go into:                         ")
message(STATUS "              |       ${CMAKE_INSTALL_PREFIX}/bin                  ")
message(STATUS "              |   • Headers will go into:                          ")
message(STATUS "              |       ${CMAKE_INSTALL_PREFIX}/include/tadah        ")
message(STATUS "              |   • Libraries will go into:                        ")
message(STATUS "              |       ${CMAKE_INSTALL_PREFIX}/lib                  ")
message(STATUS "              |                                                    ")
message(STATUS "              | To change the install location, run:               ")
message(STATUS "              |   cmake .. -DCMAKE_INSTALL_PREFIX=/your/path       ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS " make docs    | Generate a local copy of the documentation.        ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS " make test    | Run the test framework after building the code.    ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS "")
message(STATUS "--------------------- OPTIONAL FEATURES ---------------------------")
message(STATUS "")

message(STATUS "  * Nested Fitting Procedure (NFP):")
message(STATUS "    TADAH_ENABLE_NFP = ${TADAH_ENABLE_NFP}")
if(TADAH_ENABLE_NFP)
  message(STATUS "    Requires LAMMPS compiled with the ${PROJECT_NAME} interface.")
  if(NOT TADAH_LAMMPS_DIR)
    message(STATUS "    Additional CMake variables:")
    message(STATUS "      REQUIRED: TADAH_LAMMPS_DIR")
    message(STATUS "      Example:  -DTADAH_LAMMPS_DIR=/path/to/lammps")
  else()
    message(STATUS "      -DTADAH_LAMMPS_DIR=${TADAH_LAMMPS_DIR}")
  endif()

  if(NOT TADAH_LAMMPS_LIB)
    message(STATUS "      REQUIRED: TADAH_LAMMPS_LIB")
    message(STATUS "      Example:  -DTADAH_LAMMPS_LIB=/path/to/liblammps_serial.a")
  else()
    message(STATUS "      -DTADAH_LAMMPS_LIB=${TADAH_LAMMPS_LIB}")
  endif()

  if(NOT TADAH_LAMMPS_LIBS)
    message(STATUS "      OPTIONAL: TADAH_LAMMPS_LIBS")
    message(STATUS "        Libraries to be linked with TADAH_LAMMPS_LIB.")
    message(STATUS "        This may be required for static LAMMPS builds (e.g. -lz).")
    message(STATUS "        Example: -DTADAH_LAMMPS_LIBS=\"-lz\"")
  else()
    message(STATUS "      -DTADAH_LAMMPS_LIBS=${TADAH_LAMMPS_LIBS}")
  endif()

  message(STATUS "    To disable NFP:")
  message(STATUS "      cmake .. -DTADAH_ENABLE_NFP=OFF")
else()
  message(STATUS "    To enable NFP:")
  message(STATUS "      cmake .. -DTADAH_ENABLE_NFP=ON")
endif()

message(STATUS "")
message(STATUS "  * OpenMP Support:")
message(STATUS "    TADAH_ENABLE_OPENMP = ${TADAH_ENABLE_OPENMP}")
message(STATUS "")
message(STATUS "  * MPI (HPO parallel annealing, NFP builds only):")
if(TADAH_ENABLE_NFP)
  message(STATUS "    TADAH_HPO_MPI = ${TADAH_HPO_MPI}")
else()
  message(STATUS "    TADAH_HPO_MPI: needs TADAH_ENABLE_NFP=ON")
endif()
message(STATUS "")
message(STATUS "  * March Native:")
message(STATUS "    TADAH_MARCH_NATIVE = ${TADAH_MARCH_NATIVE}")
message(STATUS "")
message(STATUS "  * Fast Math:")
message(STATUS "    TADAH_ENABLE_FAST_MATH = ${TADAH_ENABLE_FAST_MATH}")
message(STATUS "")
message(STATUS "  * AddressSanitizer:")
message(STATUS "    TADAH_ENABLE_ASAN = ${TADAH_ENABLE_ASAN}")
message(STATUS "")
message(STATUS "  * Build with Tests:")
message(STATUS "    BUILD_TESTING = ${BUILD_TESTING}")
message(STATUS "")

message(STATUS "===================================================================")
