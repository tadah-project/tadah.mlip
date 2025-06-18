cmake_minimum_required(VERSION 3.12)

include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)
include(FetchContent)

set(TADAH "Tadah!")
project(${TADAH}, LANGUAGES CXX)

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

# default build type for single-config generators (Ninja/Make)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(BUILD_SHARED_LIBS)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

option(TADAH_ENABLE_HPO "Enable Hyperparamter optimizer." OFF)
option(TADAH_ENABLE_OPENMP "Enable shared-memory parallelisation via OpenMP" OFF)
option(TADAH_BUILD_MPI "Build MPI version of ${PROJECT_NAME}" OFF)
option(TADAH_MARCH_NATIVE "Build with -march=native" OFF)
option(TADAH_ENABLE_FAST_MATH "Enable fast math optimizations" OFF)
option(TADAH_BUILD_DOCS_ONLY "Skip checks for required libraries; only build docs" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# 2. Bring in the shared compile/link options
include(FetchContent)
if(NOT TARGET tadah::compile_options)
  FetchContent_Declare (
    tadah_cmake
    GIT_REPOSITORY  https://git.ecdf.ed.ac.uk/tadah/cmake-tools.git
    GIT_TAG         main)
  FetchContent_MakeAvailable(tadah_cmake)   # defines tadah::compile_options
endif()

# (option) Developer builds -DTADAH_DEVELOP=ON
tadah_use_local_module(Tadah.MLIP MLIP)  # FetchContent name + folder name

# 3. Git branch information, for informational purposes 
tadah_get_git_branch(GIT_BRANCH)
tadah_message(STATUS "Using GIT_BRANCH:  ${GIT_BRANCH}")


if(TADAH_ENABLE_HPO)
  tadah_use_local_module(Tadah.HPO HPO)  # FetchContent name + folder name
  # tadah_use_local_module(Tadah.LAMMPS Tadah.LAMMPS)  # FetchContent name + folder name
  add_compile_definitions(TADAH_ENABLE_HPO)
  FetchContent_Declare(
    Tadah.HPO
    GIT_REPOSITORY https://git.ecdf.ed.ac.uk/tadah/hpo.git
    GIT_TAG origin/${GIT_BRANCH}
  )
  FetchContent_MakeAvailable(Tadah.HPO)

  # FetchContent_Declare(
  #   Tadah.LAMMPS
  #   GIT_REPOSITORY https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git
  #   GIT_TAG origin/${GIT_BRANCH}
  # )
  # FetchContent_MakeAvailable(Tadah.LAMMPS)
else()
  tadah_message("Fetching ${PROJECT_NAME} modules...")
  FetchContent_Declare(
    Tadah.MLIP
    GIT_REPOSITORY https://git.ecdf.ed.ac.uk/tadah/mlip.git
    GIT_TAG origin/${GIT_BRANCH}
  )
  FetchContent_MakeAvailable(Tadah.MLIP)
endif()

# Set the project name and version
set_project_name()


file(GLOB TADAH_SRC_FILES src/*.cpp)
add_executable (tadah ${TADAH_SRC_FILES})
target_include_directories(tadah
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${PROJECT_SOURCE_DIR}/external/CLI11/include
  )

if(TADAH_ENABLE_HPO)
  target_link_libraries(tadah PUBLIC tadah::compile_options tadah.hpo)
else()
  target_link_libraries(tadah PUBLIC tadah::compile_options tadah.mlip)
endif()



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
if(TADAH_BUILD_MPI)
  find_package(MPI REQUIRED)
  if(MPI_CXX_FOUND)
    target_link_libraries(tadah PUBLIC MPI::MPI_CXX)
    add_compile_definitions(TADAH_BUILD_MPI)
  endif()
  find_package(SCALAPACK REQUIRED)
  target_link_libraries(tadah PUBLIC scalapack)
  set_target_properties(tadah PROPERTIES OUTPUT_NAME "tadah_mpi")
endif()
tadah_message(STATUS "Build MPI version of ${PROJECT_NAME} is ${TADAH_BUILD_MPI}")
#########################################################################

#########################################################################     
tadah_message(STATUS "Build with Hyperparameter optimizer is ${TADAH_ENABLE_HPO}")
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

set(BUILD_TESTING OFF CACHE BOOL "Build tests")
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()


find_package(Doxygen)
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

message(STATUS "  * Hyperparameter Optimizer (HPO):")
message(STATUS "    TADAH_ENABLE_HPO = ${TADAH_ENABLE_HPO}")
if(TADAH_ENABLE_HPO)
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
    message(STATUS "      Example:  -DTADAH_LAMMPS_LIB=liblammps_serial.a")
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

  message(STATUS "    To disable HPO:")
  message(STATUS "      cmake .. -DTADAH_ENABLE_HPO=OFF")
else()
  message(STATUS "    To enable HPO:")
  message(STATUS "      cmake .. -DTADAH_ENABLE_HPO=ON")
endif()

message(STATUS "")
message(STATUS "  * OpenMP Support:")
message(STATUS "    TADAH_ENABLE_OPENMP = ${TADAH_ENABLE_OPENMP}")
message(STATUS "")
message(STATUS "  * MPI Build:")
message(STATUS "    TADAH_BUILD_MPI = ${TADAH_BUILD_MPI}")
message(STATUS "")
message(STATUS "  * March Native:")
message(STATUS "    TADAH_MARCH_NATIVE = ${TADAH_MARCH_NATIVE}")
message(STATUS "")
message(STATUS "  * Fast Math:")
message(STATUS "    TADAH_ENABLE_FAST_MATH = ${TADAH_ENABLE_FAST_MATH}")
message(STATUS "")
message(STATUS "  * Build with Tests:")
message(STATUS "    TADAH_BUILD_TESTS = ${TADAH_BUILD_TESTS}")
message(STATUS "")

message(STATUS "===================================================================")
