cmake_minimum_required(VERSION 3.12)

# There is usually no need to change anything here...

set(TADAH "Ta-dah!")
file(READ "MLIP/version.h" ver)
string(REGEX MATCH "TADAH_WORLD_VERSION ([0-9])" _ "${ver}")
set(ver_world ${CMAKE_MATCH_1})
string(REGEX MATCH "TADAH_MAJOR_VERSION ([0-9])" _ "${ver}")
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "TADAH_MINOR_VERSION ([0-9])" _ "${ver}")
set(ver_minor ${CMAKE_MATCH_1})

project(Ta-dah VERSION ${ver_world}.${ver_major}.${ver_minor})
message(STATUS "${TADAH} VERSION: ${ver_world}.${ver_major}.${ver_minor}")

# Here are the options... set them with -D,
# for example:
# cmake .. -DTADAH_BUILD_TESTS=ON
option(TADAH_ENABLE_HPO "Enable Hyperparamter optimiser." OFF)
option(TADAH_ENABLE_OPENMP "Enable shared-memory parallelisation via OpenMP" OFF)
option(TADAH_ENABLE_MPI "Enable parallelisation via MPI" OFF)
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

if(UNIX AND NOT APPLE)
endif()

if(APPLE)
endif()

if(WIN32)
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

# Here we only need name so for e.g. liblammps_serial.so use lammps_serial
if(NOT DEFINED TADAH_LAMMPS_LIB)
    set(TADAH_LAMMPS_LIB "lammps_serial")
endif()

# Set CONFIG_FILE_DIR before adding CORE module
# otherwise CORE module will set it itself
set(CONFIG_FILE_DIR "${CMAKE_INSTALL_PREFIX}/include/tadah/config/")

add_subdirectory(CORE)
add_subdirectory(MODELS)
add_subdirectory(MLIP)

if(BUILD_SHARED_LIBS) 
    add_library(tadah SHARED
        $<TARGET_OBJECTS:tadah.core.obj>
        $<TARGET_OBJECTS:tadah.models.obj>
        $<TARGET_OBJECTS:tadah.mlip.obj>
        )
else()
    # so far this is the only working solution to obtain static libtadah
    # which combines everything and most importantly works when linked to
    # the ta-dah binary
    add_custom_target(libtadah ALL
        COMMAND ${CMAKE_AR} rc libtadah.a
        $<TARGET_FILE:tadah.core>
        $<TARGET_FILE:tadah.models>
        $<TARGET_FILE:tadah.mlip>
        )
endif()

add_executable (ta-dah bin/tadah.cpp)
target_link_libraries(ta-dah PRIVATE tadah)

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
if(TADAH_ENABLE_MPI)
    find_package(MPI REQUIRED)
    if(MPI_CXX_FOUND)
        target_link_libraries(ta-dah PUBLIC MPI::MPI_CXX)
        add_compile_definitions(TADAH_ENABLE_MPI)
    endif()
endif()
message(STATUS "${TADAH}: Build with MPI is ${TADAH_ENABLE_MPI}")
#########################################################################

#########################################################################
if(TADAH_ENABLE_HPO)
    if (NOT TADAH_ENABLE_MPI)
        if (NOT TADAH_LAMMPS_MPI_STUBS)
            message(FATAL_ERROR "${TADAH}: You are compiling with TADAH_ENABLE_HPO=ON
            This option requires either:
            -DTADAH_ENABLE_MPI=ON (preferred)
            or providing path to lammps MPI STUBS in the lammps src directory
            -DTADAH_LAMMPS_MPI_STUBS=/path/to/lammps/src/STUBS")
        endif()
        target_include_directories(ta-dah PRIVATE ${TADAH_LAMMPS_MPI_STUBS})
    endif()

    if (TADAH_ENABLE_MPI AND TADAH_LAMMPS_MPI_STUBS)
        message(FATAL_ERROR "${TADAH}: Either use:
        -DTADAH_ENABLE_MPI=ON (preferred)
        or
        -DTADAH_LAMMPS_MPI_STUBS=${TADAH_LAMMPS_MPI_STUBS}")
    endif()

    # c++ need to know which library LAMMPS will use
    add_compile_definitions(TADAH_LAMMPS_LIB=\"lib${TADAH_LAMMPS_LIB}\")

    # add static libs definitions to our shared lib
    message(STATUS "${TADAH}: Fetching Dlib from git...")
    include(FetchContent)
    FetchContent_Declare(dlib
        GIT_REPOSITORY https://github.com/davisking/dlib.git
        GIT_TAG        v19.24
        )
    FetchContent_MakeAvailable(dlib)

    target_link_libraries(ta-dah PUBLIC dlib::dlib)

    target_link_libraries(ta-dah PRIVATE "${TADAH_LAMMPS_LIB}")

    # Finalise by setting preprocessor symbol
    add_compile_definitions(TADAH_ENABLE_HPO)

    file(GLOB_RECURSE hpo_src CONFIGURE_DEPENDS "HPO/*.cpp")
    target_sources(ta-dah PRIVATE ${hpo_src})

    # add resource lib to the project so we can embed lammps scripts into
    # the library project
    include(${PROJECT_SOURCE_DIR}/LIBS/cmrc-master/CMakeRC.cmake)
    # Add resources...
    cmrc_add_resource_library(
        hpo-resources 
        ALIAS hpo::rc
        NAMESPACE hpo 
        WHENCE HPO/lammps
        PREFIX hpo
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.lata
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.envol
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.elastic
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.surface
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.usurface
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.FR_vacancy_at_p
        ${PROJECT_SOURCE_DIR}/HPO/lammps/tadah.unrelaxed_vacancy_at_p
        )
    target_link_libraries(ta-dah PUBLIC hpo::rc)
    target_link_libraries(ta-dah PUBLIC)
    target_link_libraries(ta-dah PUBLIC -llapacke)
    #target_link_libraries(tadah PUBLIC -llapacke)
endif()
message(STATUS "${TADAH}: Build with Hyperparameter optimiser is ${TADAH_ENABLE_HPO}")
#########################################################################

#########################################################################
if(TADAH_ENABLE_OPENMP)
    find_package(OpenMP REQUIRED)
	if(OpenMP_CXX_FOUND)
        if(BUILD_SHARED_LIBS) 
            target_link_libraries(tadah PUBLIC OpenMP::OpenMP_CXX)
        endif()
        target_link_libraries(ta-dah PUBLIC OpenMP::OpenMP_CXX)
        add_compile_definitions(TADAH_ENABLE_OPENMP)
    endif()
endif()
message(STATUS "${TADAH}: Build with OpenMP is ${TADAH_ENABLE_OPENMP}")
#########################################################################

#########################################################################

find_package(LAPACK)
target_link_libraries (ta-dah PRIVATE ${LAPACK_LIBRARIES})

install(DIRECTORY "CORE/config" DESTINATION ${CMAKE_INSTALL_PREFIX}/include/tadah)
install(TARGETS ta-dah DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)


if(BUILD_SHARED_LIBS) 
    install(TARGETS tadah DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
else()
    install(FILES ${CMAKE_BINARY_DIR}/libtadah.a DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

include(CTest) 
if(TADAH_BUILD_TESTS)
    add_subdirectory(tests)
endif()

set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/catch2)


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
message(STATUS "              Requires LAMMPS to be compiled as shared serial lib  ")
message(STATUS "              Linking to serial LAMMPS: ${TADAH_LAMMPS_LIB}              ")
message(STATUS "              To set LAMMPS lib path to different value use:       ")
message(STATUS "              -DTADAH_LAMMPS_LIB=lammps_serial                           ")
message(STATUS "              which corresponds to liblammps_serial                ")
message(STATUS "              To disable this option:                              ")
message(STATUS "                cmake .. -DTADAH_ENABLE_HPO=OFF                    ")
else()
message(STATUS "              To enable this option:                               ")
message(STATUS "                cmake .. -DTADAH_ENABLE_HPO=ON                     ")
endif()
message(STATUS "                                                                   ")
message(STATUS "===================================================================")
