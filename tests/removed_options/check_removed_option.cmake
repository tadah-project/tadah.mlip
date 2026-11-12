# Configure-time check for build options removed from Tadah
# (cmake-tools/cmake/TadahCompileOptions.cmake), run on the throw-away
# project in project/:
#
# TADAH_BUILD_MPI, the legacy MPI training build: ON is refused.
#   CASE=on          -DTADAH_BUILD_MPI=ON       -> configure fails, and the
#                                                  message names the removal
#                                                  and TADAH_HPO_MPI
#   CASE=preset_on   the same through a -C preset
#   CASE=off         -DTADAH_BUILD_MPI=OFF      -> configures
#   CASE=preset_off  set(TADAH_BUILD_MPI OFF CACHE BOOL ...) in a -C preset,
#                    as in older presets       -> configures
#   CASE=unset       no TADAH_BUILD_MPI         -> configures
#
# TADAH_GIT_BRANCH, the git ref of fetched modules: every value is refused.
#   CASE=git_branch_command_line  -DTADAH_GIT_BRANCH=develop -> configure
#                    fails; the message names the value, the removal, the
#                    checkout that selects the sources and
#                    -UTADAH_GIT_BRANCH, and the same build directory
#                    configures with that -U
#   CASE=git_branch_preset  the same for a -C preset that sets it
#   CASE=git_branch_cache   the value an earlier cmake command left in the
#                    cache of the build directory: a command that does not
#                    give the option is refused too, until -UTADAH_GIT_BRANCH
#   CASE=git_branch_variable  the build file sets a variable of that name
#                    before it adds cmake-tools -> configure fails; the
#                    message names that line, and no -U
#   CASE=git_branch_environment  TADAH_GIT_BASE, CI_COMMIT_REF_NAME and
#                    READTHEDOCS_GIT_IDENTIFIER in the environment ->
#                    configures, no message; -DTADAH_GIT_BRANCH=develop in
#                    that environment is refused, naming its own value
#   CASE=git_base_cache  the cache entry TADAH_GIT_BASE of a build directory
#                    (the value and help text an unreleased build file
#                    wrote) -> configures, no message, the entry stays;
#                    -DTADAH_GIT_BRANCH=develop there is refused without
#                    naming TADAH_GIT_BASE
#   CASE=git_branch_lammps  a LAMMPS configure with the ML-TADAH package
#                    (-DLAMMPS_DIR=<LAMMPS sources> -DREPOSITORY=<root of
#                    the repository>) given -DTADAH_GIT_BRANCH=develop, and
#                    the next one that finds it in the cache -> both fail
#                    with the message
#
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGENERATOR=<CMake generator> -DMAKE_PROGRAM=<build tool>
#         -DCXX=<C++ compiler> -P check_removed_option.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR GENERATOR MAKE_PROGRAM CXX)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_removed_option.cmake: -D${v}=... is required")
  endif()
endforeach()

# WORK_DIR is removed here: the build directory of a case is new.
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(preset "${WORK_DIR}/preset.cmake")
set(cache_file "${WORK_DIR}/build/CMakeCache.txt")

# What every cmake command of a case ends with: the project, the build
# directory, the toolchain of the build that runs the test, no network.
set(configure_args
  -S "${CMAKE_CURRENT_LIST_DIR}/project" -B "${WORK_DIR}/build"
  -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
  "-DCMAKE_CXX_COMPILER=${CXX}"
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${TADAH_CMAKE_DIR}")
# Empty, or `cmake -E env NAME=VALUE...` for a case that sets the environment.
set(launcher "")
set(transcript "")

function(fail what)
  message(NOTICE "FAIL: ${what}")
  set_property(GLOBAL APPEND PROPERTY removed_option_failures "${what}")
endfunction()

# One cmake command on the build directory, ARGN before the arguments above.
# Sets rc, out, flat (out with whitespace runs collapsed: CMake wraps the
# text of an error) and configured (-1: the project did not reach its end).
macro(configure)
  execute_process(
    COMMAND ${launcher} "${CMAKE_COMMAND}" ${ARGN} ${configure_args}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
  string(FIND "${out}" "TADAH_REMOVED_OPTION_CASE_CONFIGURED" configured)
  string(APPEND transcript "---- cmake ${ARGN}: exit ${rc}\n${out}\n")
endmacro()

# The last configure step went through and printed nothing that matches
# `silent` (a regular expression).
function(expect_configured step silent)
  if(NOT rc EQUAL 0)
    fail("${step}: configure failed (exit ${rc})")
  endif()
  if(configured EQUAL -1)
    fail("${step}: the project did not finish configuring")
  endif()
  if(flat MATCHES "${silent}")
    fail("${step}: a message was printed that matches '${silent}'")
  endif()
endfunction()

# The last configure step stopped, and its output has every text of ARGN.
function(expect_refused step)
  if(rc EQUAL 0)
    fail("${step}: configure succeeded")
  endif()
  if(NOT configured EQUAL -1)
    fail("${step}: the project finished configuring")
  endif()
  foreach(text IN LISTS ARGN)
    string(FIND "${flat}" "${text}" at)
    if(at EQUAL -1)
      fail("${step}: the message lacks '${text}'")
    endif()
  endforeach()
endfunction()

# The last configure step printed none of the texts of ARGN.
function(expect_absent step)
  foreach(text IN LISTS ARGN)
    string(FIND "${flat}" "${text}" at)
    if(NOT at EQUAL -1)
      fail("${step}: the output has '${text}'")
    endif()
  endforeach()
endfunction()

# The lines of the build directory's cache that set `entry` are `expected`
# (a list; empty: the cache has no such entry).
function(expect_cache step entry expected)
  set(lines "")
  if(EXISTS "${cache_file}")
    file(STRINGS "${cache_file}" lines REGEX "^${entry}[:=]")
  endif()
  if(NOT "${lines}" STREQUAL "${expected}")
    fail("${step}: the cache has '${lines}' for ${entry}, expected '${expected}'")
  endif()
endfunction()

set(mpi_texts
  "TADAH_BUILD_MPI=ON: the legacy MPI training build"
  "was removed in Tadah 2.0.0"
  "configure with -DTADAH_HPO_MPI=ON instead"
  "Set -DTADAH_BUILD_MPI=OFF")
# What the refusal of TADAH_GIT_BRANCH says after the value, and what it
# adds for a value that the cache of the build directory holds.
set(branch_texts
  ": the build option is removed. A build takes cmake-tools and every Tadah! module from the directories of its repository"
  "git clone -b <ref>"
  "git checkout <ref>")
set(cache_texts
  "from the cmake command and from a preset (-C) that sets it"
  "-UTADAH_GIT_BRANCH on the next cmake command removes it")
set(git "TADAH_GIT_")

if(CASE STREQUAL "on")
  configure(-DTADAH_BUILD_MPI=ON)
  expect_refused("-DTADAH_BUILD_MPI=ON" ${mpi_texts})
elseif(CASE STREQUAL "preset_on")
  file(WRITE "${preset}" "set(TADAH_BUILD_MPI ON CACHE BOOL \"\")\n")
  configure("-C${preset}")
  expect_refused("the preset" ${mpi_texts})
elseif(CASE STREQUAL "off")
  configure(-DTADAH_BUILD_MPI=OFF)
  expect_configured("-DTADAH_BUILD_MPI=OFF" "TADAH_BUILD_MPI")
elseif(CASE STREQUAL "preset_off")
  file(WRITE "${preset}"
    "set(TADAH_BUILD_MPI OFF CACHE BOOL\n"
    "  \"Build MPI-enabled Tadah (experimental).\")\n")
  configure("-C${preset}")
  expect_configured("the preset" "TADAH_BUILD_MPI")
elseif(CASE STREQUAL "unset")
  configure()
  expect_configured("no option" "TADAH_BUILD_MPI")

elseif(CASE STREQUAL "git_branch_command_line")
  configure(-DTADAH_GIT_BRANCH=develop)
  expect_refused("-DTADAH_GIT_BRANCH=develop"
    "TADAH_GIT_BRANCH=develop: the build option is removed"
    ${branch_texts} ${cache_texts})
  configure(-UTADAH_GIT_BRANCH)
  expect_configured("-UTADAH_GIT_BRANCH" "${git}")
  expect_cache("-UTADAH_GIT_BRANCH" TADAH_GIT_BRANCH "")
elseif(CASE STREQUAL "git_branch_preset")
  file(WRITE "${preset}"
    "set(TADAH_GIT_BRANCH \"main\" CACHE STRING \"Git ref of the modules\")\n")
  configure("-C${preset}")
  expect_refused("the preset"
    "TADAH_GIT_BRANCH=main: the build option is removed"
    ${branch_texts} ${cache_texts})
  configure(-UTADAH_GIT_BRANCH)
  expect_configured("-UTADAH_GIT_BRANCH, no preset" "${git}")
  expect_cache("-UTADAH_GIT_BRANCH, no preset" TADAH_GIT_BRANCH "")
elseif(CASE STREQUAL "git_branch_cache")
  # An earlier cmake command leaves the value in the cache.
  configure(-DTADAH_GIT_BRANCH=v1.5.0)
  expect_cache("the earlier command" TADAH_GIT_BRANCH
    "TADAH_GIT_BRANCH:UNINITIALIZED=v1.5.0")
  configure()
  expect_refused("the value in the cache"
    "TADAH_GIT_BRANCH=v1.5.0: the build option is removed"
    ${branch_texts} ${cache_texts})
  configure(-UTADAH_GIT_BRANCH)
  expect_configured("-UTADAH_GIT_BRANCH" "${git}")
  configure()
  expect_configured("the command after -UTADAH_GIT_BRANCH" "${git}")
  expect_cache("the command after -UTADAH_GIT_BRANCH" TADAH_GIT_BRANCH "")
elseif(CASE STREQUAL "git_branch_variable")
  configure(-DTADAH_REMOVED_OPTION_CASE_SETS_VARIABLE=ON)
  expect_refused("the variable of the build file"
    "TADAH_GIT_BRANCH=from-a-build-file: the build option is removed"
    ${branch_texts}
    "A build file sets the variable before it adds Tadah!"
    "set(TADAH_GIT_BRANCH ...)")
  expect_absent("the variable of the build file" "-UTADAH_GIT_BRANCH")
  configure(-DTADAH_REMOVED_OPTION_CASE_SETS_VARIABLE=OFF)
  expect_configured("without the variable" "${git}")
elseif(CASE STREQUAL "git_branch_environment")
  set(launcher "${CMAKE_COMMAND}" -E env
    "TADAH_GIT_BASE=https://mirror.example/tadah"
    "CI_COMMIT_REF_NAME=from-ci" "READTHEDOCS_GIT_IDENTIFIER=from-rtd")
  set(environment "mirror.example" "from-ci" "from-rtd")
  configure()
  expect_configured("the environment variables" "${git}")
  expect_absent("the environment variables" ${environment})
  expect_cache("the environment variables" "${git}[A-Z]+" "")
  configure(-DTADAH_GIT_BRANCH=develop)
  expect_refused("-DTADAH_GIT_BRANCH=develop in that environment"
    "TADAH_GIT_BRANCH=develop: the build option is removed"
    ${branch_texts} ${cache_texts})
  expect_absent("-DTADAH_GIT_BRANCH=develop in that environment"
    "TADAH_GIT_BASE" ${environment})
elseif(CASE STREQUAL "git_base_cache")
  set(entry "TADAH_GIT_BASE:STRING=https://github.com/tadah-project")
  file(WRITE "${preset}"
    "set(TADAH_GIT_BASE \"https://github.com/tadah-project\" CACHE STRING\n"
    "  \"Base address of the Tadah! git repositories (<base>/<repository>.git)\")\n")
  configure("-C${preset}")
  expect_configured("the command that writes the entry" "${git}")
  expect_cache("the command that writes the entry" TADAH_GIT_BASE "${entry}")
  configure()
  expect_configured("the entry in the cache" "${git}")
  expect_cache("the entry in the cache" TADAH_GIT_BASE "${entry}")
  configure(-DTADAH_GIT_BRANCH=develop)
  expect_refused("-DTADAH_GIT_BRANCH=develop beside the entry"
    "TADAH_GIT_BRANCH=develop: the build option is removed"
    ${branch_texts} ${cache_texts})
  expect_absent("-DTADAH_GIT_BRANCH=develop beside the entry" "TADAH_GIT_BASE")
  expect_cache("-DTADAH_GIT_BRANCH=develop beside the entry"
    TADAH_GIT_BASE "${entry}")
elseif(CASE STREQUAL "git_branch_lammps")
  foreach(v LAMMPS_DIR REPOSITORY)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
      message(FATAL_ERROR "check_removed_option.cmake: -D${v}=... is required")
    endif()
  endforeach()
  # The LAMMPS build file with the package, as the installation page
  # configures it; the refusal comes before LAMMPS looks for anything.
  set(configure_args
    -S "${LAMMPS_DIR}/cmake" -B "${WORK_DIR}/build"
    -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
    "-DCMAKE_CXX_COMPILER=${CXX}"
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DDOWNLOAD_POTENTIALS=OFF
    -DBUILD_MPI=OFF -DPKG_ML-TADAH=ON
    "-DTADAH_LAMMPS_SOURCE_DIR=${REPOSITORY}/Tadah.LAMMPS"
    "-DCMAKE_PROJECT_INCLUDE=${REPOSITORY}/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake")
  configure(-DTADAH_GIT_BRANCH=develop)
  expect_refused("the LAMMPS configure given -DTADAH_GIT_BRANCH=develop"
    "TADAH_GIT_BRANCH=develop: the build option is removed"
    ${branch_texts} ${cache_texts})
  configure()
  expect_refused("the LAMMPS configure with the value in the cache"
    "TADAH_GIT_BRANCH=develop: the build option is removed"
    ${branch_texts} ${cache_texts})
else()
  message(FATAL_ERROR "check_removed_option.cmake: unknown CASE '${CASE}'")
endif()

get_property(failures GLOBAL PROPERTY removed_option_failures)
list(LENGTH failures nfail)
if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed for CASE=${CASE}; "
                      "configure output:\n${transcript}")
endif()
message(STATUS "CASE=${CASE}: ok")
