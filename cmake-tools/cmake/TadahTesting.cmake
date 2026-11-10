# ====================================================================
#  Centralised Catch2 test setup for Tadah repositories
#
#  Usage in a test CMakeLists.txt:
#
#     tadah_setup_tests()       # creates Catch2 target if needed
#
#     add_executable(test_foo ${test_sources})
#     target_link_libraries(test_foo PRIVATE Catch2 tadah.core ...)
#
#     include(Catch)
#     catch_discover_tests(test_foo)
#
#  The Catch2 header and discovery scripts (Catch.cmake,
#  CatchAddTests.cmake) are bundled in cmake-tools/testing/.
# ====================================================================

# Directory containing Catch2 header and helpers (relative to this file)
get_filename_component(_TADAH_CMAKE_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_TADAH_CMAKE_TOOLS_DIR "${_TADAH_CMAKE_TOOLS_DIR}" CACHE INTERNAL
    "Root of cmake-tools for locating testing helpers")

function(tadah_setup_tests)
  if(NOT TARGET Catch2)
    add_library(Catch2 INTERFACE)
    target_include_directories(Catch2 INTERFACE
      "${_TADAH_CMAKE_TOOLS_DIR}/testing")
  endif()

  # Make Catch.cmake discoverable via include(Catch)
  list(APPEND CMAKE_MODULE_PATH "${_TADAH_CMAKE_TOOLS_DIR}/testing")
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

# ====================================================================
#  Tadah test-build switch
#
#  Every Tadah module builds its unit tests under one switch,
#  BUILD_TESTING, which is OFF by default. Configuring any Tadah
#  project with -DBUILD_TESTING=ON builds the tests of that project
#  AND of every Tadah module it consumes. External dependencies
#  (Eigen, Ceres, nlopt, dlib, ...) never build their own tests --
#  see tadah_begin_external_deps / tadah_end_external_deps.
#
#  Usage in a module's CMakeLists.txt:
#
#     FetchContent_MakeAvailable(tadah_cmake)
#     tadah_declare_testing()          # once, before any sub-module
#     ...
#     tadah_add_tests()                # at the end, after the targets
# ====================================================================

# Declare the BUILD_TESTING cache entry (default OFF). Must be called
# early -- before the FetchContent of any sub-module -- so the entry is
# settled before children configure and a parent's value propagates
# down to them. Idempotent: a no-op once the entry exists (created by a
# parent module, a -D on the command line, or a -C preset).
macro(tadah_declare_testing)
  set(BUILD_TESTING OFF CACHE BOOL "Build Tadah module unit tests")
endmacro()

# Add the module's tests/ subdirectory when BUILD_TESTING is ON. Call at
# the end of the module's CMakeLists, after its targets are defined.
#
# include(CTest) is called unconditionally so this directory's
# CTestTestfile.cmake is regenerated on every configure -- otherwise a
# stale one from an earlier BUILD_TESTING=ON configure would linger and
# ctest would still list now-disabled tests. option(BUILD_TESTING) run
# inside CTest is a harmless no-op: tadah_declare_testing() already
# created the cache entry.
macro(tadah_add_tests)
  include(CTest)
  if(BUILD_TESTING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
      add_subdirectory(tests)
    endif()
  endif()
endmacro()

# Shield external (non-Tadah) dependencies from BUILD_TESTING. Libraries
# such as Ceres key their own test suites off BUILD_TESTING, and Tadah
# never wants those built. Wrap external FetchContent calls:
#
#     tadah_begin_external_deps()
#     FetchContent_MakeAvailable(ceres nlopt ...)
#     tadah_end_external_deps()
#
# The cache entry is force-set OFF for the duration and restored after.
# tadah_declare_testing() is invoked first so the saved value is always
# a real cache value -- never an empty string that would corrupt the
# entry on restore.
macro(tadah_begin_external_deps)
  tadah_declare_testing()
  set(_tadah_ext_build_testing "$CACHE{BUILD_TESTING}")
  set(BUILD_TESTING OFF CACHE BOOL "Build Tadah module unit tests" FORCE)
endmacro()

macro(tadah_end_external_deps)
  set(BUILD_TESTING "${_tadah_ext_build_testing}"
      CACHE BOOL "Build Tadah module unit tests" FORCE)
  unset(_tadah_ext_build_testing)
endmacro()

# ====================================================================
#  Header self-containment check
#
#  Usage in a module's tests/CMakeLists.txt:
#
#     tadah_add_header_check(tadah.foo)
#     tadah_add_header_check(tadah.foo HEADER_DIRS <dir> ...)
#
#  Every header (*.h, *.hpp) under the HEADER_DIRS (default: the include/
#  directory next to the target's CMakeLists.txt) gets a translation unit
#  of its own, generated in the binary directory, that includes it twice
#  by its path relative to its HEADER_DIRS entry. The header must compile
#  on its own, with nothing included before it, and the second inclusion
#  must be harmless (an include guard or #pragma once). The headers are
#  globbed with CONFIGURE_DEPENDS, so a new header is checked without
#  being registered anywhere.
#
#  The translation units form the object library <target>_header_check,
#  excluded from `all`, so a header that fails the check fails its test
#  and does not break the build. They compile with what a consumer of
#  <target> gets (they link it; an executable, which cannot be linked,
#  lends its own include directories, compile definitions and options
#  instead), the definitions of the calling directory (such as a
#  module's add_compile_definitions()) and the target's C++ standard.
#  Each one also has its own header's HEADER_DIRS entry as an include
#  directory, searched first, and no other entry: a header that needs a
#  directory the target keeps PRIVATE fails even when that directory is
#  another HEADER_DIRS entry (CORE's generated/). The object files stay
#  in the build tree, so a rerun recompiles only the headers that changed
#  or that include a changed header.
#
#  The CTest entry <target>_headers (label `headers`) builds that
#  library, keeping going past a failed header with the Makefile and
#  Ninja generators so that one run names every header that fails. It
#  runs on TADAH_HEADER_CHECK_JOBS processes (a whole number, 1 or more,
#  read when the function is called; default: as many as the machine has
#  physical cores), which is also its PROCESSORS (for `ctest -j`), and
#  never alongside another entry of this kind (RESOURCE_LOCK): they build
#  in the same tree.
# ====================================================================
function(tadah_add_header_check target)
  cmake_parse_arguments(PARSE_ARGV 1 _thc "" "" "HEADER_DIRS")
  if(_thc_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "tadah_add_header_check(${target}): unexpected "
                        "arguments: ${_thc_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "tadah_add_header_check: '${target}' is not a target")
  endif()
  get_target_property(_thc_imported "${target}" IMPORTED)
  get_target_property(_thc_alias "${target}" ALIASED_TARGET)
  if(_thc_imported OR _thc_alias)
    message(FATAL_ERROR "tadah_add_header_check: '${target}' is an imported "
                        "or ALIAS target; name the target that is built here")
  endif()
  get_target_property(_thc_type "${target}" TYPE)
  if("HEADER_DIRS" IN_LIST _thc_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR "tadah_add_header_check(${target}): HEADER_DIRS "
                        "names no directory")
  endif()
  if(NOT DEFINED _thc_HEADER_DIRS)
    get_target_property(_thc_src "${target}" SOURCE_DIR)
    set(_thc_HEADER_DIRS "${_thc_src}/include")
  endif()

  set(_thc_name "${target}_header_check")
  set(_thc_root "${CMAKE_CURRENT_BINARY_DIR}/${_thc_name}")
  set(_thc_dirs "")
  set(_thc_tus "")
  set(_thc_seen "")
  foreach(_thc_dir IN LISTS _thc_HEADER_DIRS)
    get_filename_component(_thc_dir "${_thc_dir}" ABSOLUTE)
    if(NOT IS_DIRECTORY "${_thc_dir}")
      message(FATAL_ERROR "tadah_add_header_check(${target}): HEADER_DIRS "
                          "entry ${_thc_dir} is not a directory")
    endif()
    list(APPEND _thc_dirs "${_thc_dir}")
    file(GLOB_RECURSE _thc_headers CONFIGURE_DEPENDS RELATIVE "${_thc_dir}"
         "${_thc_dir}/*.h" "${_thc_dir}/*.hpp")
    list(SORT _thc_headers)
    foreach(_thc_rel IN LISTS _thc_headers)
      if(_thc_rel IN_LIST _thc_seen)
        message(FATAL_ERROR "tadah_add_header_check(${target}): ${_thc_rel} "
                            "is in more than one of the HEADER_DIRS, so "
                            "#include <${_thc_rel}> is ambiguous")
      endif()
      list(APPEND _thc_seen "${_thc_rel}")
      set(TADAH_HEADER_CHECK_TARGET "${target}")
      set(TADAH_HEADER_CHECK_HEADER "${_thc_rel}")
      set(TADAH_HEADER_CHECK_PATH "${_thc_dir}/${_thc_rel}")
      # configure_file rewrites a file only when its content changes, so a
      # re-configure does not recompile the checks.
      configure_file("${_TADAH_CMAKE_TOOLS_DIR}/testing/tadah_header_check.cpp.in"
                     "${_thc_root}/${_thc_rel}.cpp" @ONLY)
      # Its own entry only, so that #include <rel> finds this header; a
      # source file's include directories come before the target's
      # (except with Xcode).
      set_source_files_properties("${_thc_root}/${_thc_rel}.cpp" PROPERTIES
        INCLUDE_DIRECTORIES "${_thc_dir}")
      list(APPEND _thc_tus "${_thc_root}/${_thc_rel}.cpp")
    endforeach()
  endforeach()
  if(NOT _thc_tus)
    message(FATAL_ERROR "tadah_add_header_check(${target}): no *.h or *.hpp "
                        "file in ${_thc_dirs}")
  endif()

  add_library(${_thc_name} OBJECT EXCLUDE_FROM_ALL ${_thc_tus})
  if(_thc_type STREQUAL "EXECUTABLE")
    # $<TARGET_PROPERTY> of these build properties includes the usage
    # requirements of the executable's link dependencies.
    target_include_directories(${_thc_name} PRIVATE
      "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>")
    target_compile_definitions(${_thc_name} PRIVATE
      "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
    target_compile_options(${_thc_name} PRIVATE
      "$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>")
  else()
    target_link_libraries(${_thc_name} PRIVATE "${target}")
  endif()
  if(NOT _thc_type STREQUAL "INTERFACE_LIBRARY")
    foreach(_thc_prop CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS)
      get_target_property(_thc_value "${target}" ${_thc_prop})
      if(NOT _thc_value MATCHES "-NOTFOUND$")
        set_property(TARGET ${_thc_name} PROPERTY ${_thc_prop} "${_thc_value}")
      endif()
    endforeach()
  endif()

  set(_thc_command "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
                   --target ${_thc_name})
  get_property(_thc_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(_thc_multi)
    list(APPEND _thc_command --config "$<CONFIG>")
  endif()
  # A bare --parallel is an unbounded `make -j`: give a number.
  if(DEFINED TADAH_HEADER_CHECK_JOBS)
    if(NOT TADAH_HEADER_CHECK_JOBS MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR "tadah_add_header_check(${target}): "
                          "TADAH_HEADER_CHECK_JOBS must be a whole number of "
                          "processes, 1 or more, got "
                          "'${TADAH_HEADER_CHECK_JOBS}'")
    endif()
    set(_thc_jobs "${TADAH_HEADER_CHECK_JOBS}")
  else()
    # 0 when CMake cannot tell.
    cmake_host_system_information(RESULT _thc_jobs
                                  QUERY NUMBER_OF_PHYSICAL_CORES)
    if(NOT _thc_jobs GREATER 0)
      set(_thc_jobs 1)
    endif()
  endif()
  list(APPEND _thc_command --parallel ${_thc_jobs})
  if(CMAKE_GENERATOR MATCHES "Makefiles")
    list(APPEND _thc_command -- -k)
  elseif(CMAKE_GENERATOR MATCHES "^Ninja")
    list(APPEND _thc_command -- -k 0)
  endif()
  add_test(NAME ${target}_headers COMMAND ${_thc_command})
  set_tests_properties(${target}_headers PROPERTIES
    LABELS headers
    TIMEOUT 900
    PROCESSORS ${_thc_jobs}
    RESOURCE_LOCK tadah_header_check)
endfunction()

# ====================================================================
#  Text check
#
#  Usage in a module's tests/CMakeLists.txt:
#
#     tadah_add_text_check(tadah.foo)
#     tadah_add_text_check(<name> ROOT <dir>)
#
#  The CTest entry <name>_text (label `text`) reads every text file under
#  ROOT (default: the calling project's source directory, the module's
#  repository) and fails on each line that matches a rule of
#  testing/tadah_text_check.cmake, naming the file, the rule and the line:
#  a person's initials, a reference to co-workers, a decision attributed
#  to the code's owner, the name or state directory of an AI tool, a note
#  kept outside the repository, or filler wording. That script lists the
#  rules, the files it reads and the directories it skips (vendored code,
#  .git, build trees). When CMake finds git and ROOT is the top of a git
#  work tree, the files read are those git tracks or would track, so that
#  ignored files (a build's or a run's outputs) are not read.
# ====================================================================
function(tadah_add_text_check name)
  cmake_parse_arguments(PARSE_ARGV 1 _ttc "" "ROOT" "")
  if(_ttc_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "tadah_add_text_check(${name}): unexpected "
                        "arguments: ${_ttc_UNPARSED_ARGUMENTS}")
  endif()
  if("ROOT" IN_LIST _ttc_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR "tadah_add_text_check(${name}): ROOT names no "
                        "directory")
  endif()
  if(NOT DEFINED _ttc_ROOT)
    set(_ttc_ROOT "${PROJECT_SOURCE_DIR}")
  endif()
  get_filename_component(_ttc_ROOT "${_ttc_ROOT}" ABSOLUTE)
  if(NOT IS_DIRECTORY "${_ttc_ROOT}")
    message(FATAL_ERROR "tadah_add_text_check(${name}): ROOT ${_ttc_ROOT} "
                        "is not a directory")
  endif()
  find_package(Git QUIET)
  add_test(NAME ${name}_text
    COMMAND "${CMAKE_COMMAND}" "-DTADAH_TEXT_ROOT=${_ttc_ROOT}"
            "-DGIT=${GIT_EXECUTABLE}"
            -P "${_TADAH_CMAKE_TOOLS_DIR}/testing/tadah_text_check.cmake")
  set_tests_properties(${name}_text PROPERTIES
    LABELS text
    TIMEOUT 300)
endfunction()

# ====================================================================
#  Git base address check
#
#  Usage in a module's tests/CMakeLists.txt:
#
#     tadah_add_git_base_check(tadah.foo)
#
#  The CTest entry <name>_git_base (label `cmake`) checks the calling
#  project's CMakeLists.txt with testing/tadah_git_base_check.cmake: it
#  carries the TADAH_GIT_BASE block of testing/tadah_git_base_block.cmake
#  unchanged, before its first FetchContent_Declare, and fetches every
#  Tadah! repository as ${TADAH_GIT_BASE}/<repository>.git; the block gives
#  the public address unless -DTADAH_GIT_BASE or the environment variable
#  TADAH_GIT_BASE names another.
# ====================================================================
function(tadah_add_git_base_check name)
  if(ARGN)
    message(FATAL_ERROR "tadah_add_git_base_check(${name}): unexpected "
                        "arguments: ${ARGN}")
  endif()
  add_test(NAME ${name}_git_base
    COMMAND "${CMAKE_COMMAND}"
            "-DTADAH_CMAKELISTS=${PROJECT_SOURCE_DIR}/CMakeLists.txt"
            -P "${_TADAH_CMAKE_TOOLS_DIR}/testing/tadah_git_base_check.cmake")
  set_tests_properties(${name}_git_base PROPERTIES
    LABELS cmake
    TIMEOUT 120)
endfunction()
