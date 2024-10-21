cmake_minimum_required(VERSION 3.12)

# There is usually no need to change anything here...

set(TADAH "Tadah!")

# Here are the options... set them with -D,
# for example:
# cmake .. -DTADAH_BUILD_TESTS=ON
option(TADAH_ENABLE_HPO "Enable Hyperparamter optimiser." OFF)
option(TADAH_ENABLE_OPENMP "Enable shared-memory parallelisation via OpenMP" OFF)
option(TADAH_BUILD_MPI "Build MPI version of ${TADAH}" OFF)
option(TADAH_MARCH_NATIVE "Build with -march=native" OFF)
option(TADAH_ENABLE_FAST_MATH "Enable fast math optimizations" OFF)
option(TADAH_BUILD_TESTS "Build with tests" OFF)
option(TADAH_BUILD_DOCUMENTATION "Create and install the HTML based        
documentation (requires Doxygen)")

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt and CMakeFiles dir. ")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(BUILD_SHARED_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (BUILD_SHARED_LIBS)
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

  # the RPATH to be used when installing, but only if it's not a system directory
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  endif("${isSystemDir}" STREQUAL "-1")
endif()

set(CMAKE_CXX_FLAGS_DEBUG
  "-g -O2 -fsanitize=address -Wall -Wpedantic -pedantic-errors -Wextra"
  CACHE STRING
  "Debug build."
  FORCE)

set(CMAKE_CXX_FLAGS_RELEASE
  "-g -O3 -Wall -Wpedantic -Wextra"
  CACHE STRING
  "Release build."
  FORCE)

set(CMAKE_CXX_FLAGS_PROFILE
  "-g -O3 -Wall -Wpedantic -Wextra -pg -fno-inline "
  CACHE STRING
  "Flags used by the C++ compiler during profile builds."
  FORCE)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Check is build supported
string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE STREQUAL "DEBUG" 
    OR
    CMAKE_BUILD_TYPE STREQUAL "RELEASE"
    OR
    CMAKE_BUILD_TYPE STREQUAL "PROFILE"
    )
  message(STATUS "${TADAH} Build type:  ${CMAKE_BUILD_TYPE}.")
else()
  message(FATAL_ERROR "
  Selected build type is not supported: ${CMAKE_BUILD_TYPE}.
  Supported build types: Release, Debug, Profile"
  )
endif()

# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "Choose the type of build, options are: None Debug Release Profile."
  FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
#get_filename_component(BUILD_DIR_NAME ${CMAKE_BINARY_DIR} NAME)

FetchContent_Declare(
  Tadah.MLIP
  GIT_REPOSITORY https://git.ecdf.ed.ac.uk/tadah/mlip.git
  GIT_TAG origin/develop
  )
FetchContent_MakeAvailable(Tadah.MLIP)

if(TADAH_ENABLE_HPO)
  add_compile_definitions(TADAH_ENABLE_HPO)
  FetchContent_Declare(
    Tadah.HPO
    GIT_REPOSITORY https://git.ecdf.ed.ac.uk/tadah/hpo.git
    GIT_TAG origin/develop
    )
  FetchContent_MakeAvailable(Tadah.HPO)
  #add_subdirectory(HPO)
endif()

file(READ "${tadah.mlip_SOURCE_DIR}/include/tadah/mlip/version.h" ver)
string(REGEX MATCH "TADAH_WORLD_VERSION ([0-9]+)" _ "${ver}")
set(ver_world ${CMAKE_MATCH_1})
string(REGEX MATCH "TADAH_MAJOR_VERSION ([0-9]+)" _ "${ver}")
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "TADAH_MINOR_VERSION ([0-9]+)" _ "${ver}")
set(ver_minor ${CMAKE_MATCH_1})

project(Tadah VERSION ${ver_world}.${ver_major}.${ver_minor})
message(STATUS "${TADAH} VERSION: ${ver_world}.${ver_major}.${ver_minor}")

add_executable (tadah bin/main.cpp bin/tadah_cli.cpp)

#add_library(
#  libtadah 
#"${Tadah.MODELS_SOURCE_DIR}/src"
#"${Tadah.MODELS_SOURCE_DIR}/src"
#"${Tadah.MLIP_SOURCE_DIR}/src"
#)

#add_subdirectory(external/CLI11)
include_directories(external/CLI11/include)


#set_target_properties(libtadah PROPERTIES OUTPUT_NAME "tadah")
#target_link_libraries(libtadah PUBLIC config::rc)

if(TADAH_ENABLE_HPO)
  #target_link_libraries(libtadah PUBLIC tadah.hpo)
endif()

target_include_directories(tadah PRIVATE ${Tadah.MLIP_SOURCE_DIR}/include)
target_include_directories(tadah PRIVATE ${Tadah.MODELS_SOURCE_DIR}/include)
target_include_directories(tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/include)
target_include_directories(tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/external/toml11)

#link_directories(<build_dir>/_deps/tadah-mlip-build/)
target_link_libraries(tadah PRIVATE tadah.mlip tadah.models) # TODO

if(TADAH_BUILD_MPI)
  set_target_properties(tadah PROPERTIES OUTPUT_NAME "tadah_mpi")
endif()

#target_link_libraries(tadah PRIVATE libtadah)

#########################################################################
if(TADAH_MARCH_NATIVE)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
  else()
    message(STATUS "${TADAH} ...but -march=native is not supported")
  endif()
endif()
message(STATUS "${TADAH} Build with -march=native in ${TADAH_MARCH_NATIVE}")
#########################################################################

#########################################################################
if(TADAH_BUILD_DOCUMENTATION)
  find_package(Doxygen)
  if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif()

  set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
  set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/doxyfile)

  configure_file(${doxyfile_in} ${doxyfile} @ONLY)

  add_custom_target(
    docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile_in}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
    COMMAND make html
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
    )
endif()
message(STATUS "${TADAH} Build documentation is ${TADAH_BUILD_DOCUMENTATION}")
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
message(STATUS "${TADAH}: Build with fast math optimisation is ${TADAH_ENABLE_FAST_MATH}")
#########################################################################

#########################################################################
if(TADAH_BUILD_MPI)
  find_package(MPI REQUIRED)
  if(MPI_CXX_FOUND)
    target_link_libraries(tadah PUBLIC MPI::MPI_CXX)
    add_compile_definitions(TADAH_BUILD_MPI)
  endif()
  find_package(SCALAPACK REQUIRED)
  target_link_libraries(tadah PUBLIC scalapack)
endif()
message(STATUS "${TADAH}: Build MPI version of ${TADAH} is ${TADAH_BUILD_MPI}")
#########################################################################

#########################################################################     
message(STATUS "${TADAH}: Build with Hyperparameter optimiser is ${TADAH_ENABLE_HPO}")
#########################################################################

#########################################################################
if(TADAH_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  if(OpenMP_CXX_FOUND)
    #target_link_libraries(libtadah PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(tadah PUBLIC OpenMP::OpenMP_CXX)
    add_compile_definitions(TADAH_ENABLE_OPENMP)
  endif()
endif()
message(STATUS "${TADAH}: Build with OpenMP is ${TADAH_ENABLE_OPENMP}")
#########################################################################

#########################################################################

find_package(LAPACK)
#target_link_libraries (libtadah PRIVATE ${LAPACK_LIBRARIES})
target_link_libraries (tadah PUBLIC ${LAPACK_LIBRARIES})

install(TARGETS tadah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
#install(TARGETS libtadah DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

if(TADAH_BUILD_TESTS)
  include(CTest) 
  add_subdirectory(tests)
endif()



message(STATUS "===================================================================")
message(STATUS "                                                                   ")
message(STATUS "${TADAH} in now configured and ready for installation.             ")
message(STATUS "                                                                   ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS "Command       |   Description                                      ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS "              |                                                    ")
message(STATUS "make install  |   Install ${TADAH}                                 ")
message(STATUS "              |                                                    ")
message(STATUS "              |   Executable file will installed to:               ")
message(STATUS "              |      ${CMAKE_INSTALL_PREFIX}/bin                   ")
message(STATUS "              |   Headers will be installed to:                    ")
message(STATUS "              |      ${CMAKE_INSTALL_PREFIX}/include/tadah         ")
message(STATUS "              |   Shared library will be installed to:             ")
message(STATUS "              |      ${CMAKE_INSTALL_PREFIX}/lib                   ")
message(STATUS "              |                                                    ")
message(STATUS "              |   To change default install location run:          ")
message(STATUS "              |     cmake .. -DCMAKE_INSTALL_PREFIX=yourpath       ")
message(STATUS "              |                                                    ")
message(STATUS "make docs     |   Generate a local copy of html documentation:     ")
message(STATUS "              |                                                    ")
message(STATUS "              |   doxygen style:                                   ")
message(STATUS "              |      ${CMAKE_SOURCE_DIR}/docs/html/index.html      ")
message(STATUS "              |   sphinx style:                                    ")
message(STATUS "              |      ${CMAKE_SOURCE_DIR}/docs/build/html/index.html")
message(STATUS "              |                                                    ")
message(STATUS "make test     |   Run testing framework                            ")
message(STATUS "              |                                                    ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS "              OPTIONAL FEATURES:                                   ")
message(STATUS "--------------+----------------------------------------------------")
message(STATUS "                                                                   ")
message(STATUS "            * Build with Hyperparamter optimiser.                  ")
message(STATUS "              TADAH_ENABLE_HPO is ${TADAH_ENABLE_HPO}              ")
if(TADAH_ENABLE_HPO)
  message(STATUS "              Requires LAMMPS to be compiled as either             ")
  message(STATUS "              shared or static library with ${TADAH} interface.    ")

  message(STATUS "              Additional cmake variables that must be set:         ")
  message(STATUS "                1. TADAH_LAMMPS_DIR                                ")
  message(STATUS "                    Must points to the LAMMPS root dir, e.g.       ")
  message(STATUS "                   -DTADAH_LAMMPS_DIR=/path/to/lammps              ")
  message(STATUS "                                                                   ")
  message(STATUS "                2. TADAH_LAMMPS_LIB                                ")
  message(STATUS "                   The name of the LAMMPS lib, e.g.                ")
  message(STATUS "                   -DTADAH_LAMMPS_LIB=liblammps.a                  ")
  message(STATUS "                                                                   ")
  message(STATUS "              To disable this option:                              ")
  message(STATUS "                cmake .. -DTADAH_ENABLE_HPO=OFF                    ")
else()
  message(STATUS "              To enable this option:                               ")
  message(STATUS "                cmake .. -DTADAH_ENABLE_HPO=ON                     ")
endif()
message(STATUS "                                                                   ")
message(STATUS "===================================================================")
