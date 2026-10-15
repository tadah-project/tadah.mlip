# ------------------------------------------------------------------
# 1. Sources
# ------------------------------------------------------------------
# Catch2 and its test discovery come from cmake-tools (tadah_setup_tests()).
tadah_setup_tests()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(test_tadah
  ${TEST_SOURCES}
  "${PROJECT_SOURCE_DIR}/src/cli_task.cpp"
  "${PROJECT_SOURCE_DIR}/src/tadah_cli.cpp")

target_include_directories(test_tadah
  PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
  PRIVATE ${PROJECT_SOURCE_DIR}/external/CLI11/include   # only if tests <CLI/CLI.hpp>
)
target_link_libraries(test_tadah
  PRIVATE
    tadah::compile_options            # warnings / sanitiser / -pg
    Catch2                            # from tadah_setup_tests()
    tadah.core
    tadah.models
    tadah.mlip)
target_include_directories(test_tadah PRIVATE ../include)
# test_published_potentials.cpp loads the potentials published with the docs;
# test_quickstart_md.cpp runs the LAMMPS example of the Quick Start page.
target_compile_definitions(test_tadah PRIVATE
  TADAH_PUBLISHED_MLIPS_DIR="${PROJECT_SOURCE_DIR}/docs/source/mlips"
  TADAH_QUICKSTART_DIR="${PROJECT_SOURCE_DIR}/docs/source/quickstart")

if(TADAH_ENABLE_NFP)
  target_link_libraries(test_tadah PRIVATE tadah.hpo tadah.lammps)
endif()

include(Catch)
# The tests need CMake 3.16: CTest reports a native skip (TADAH_SKIP, the
# skipped tests below) through the SKIP_REGULAR_EXPRESSION test property,
# which it has from 3.16 and ignores before, reporting the test Passed.
# include(Catch) above already stops on an older CMake, saying so
# (cmake-tools testing/TadahTestSkip.cmake); the build without tests
# accepts the umbrella's 3.15. The scripts these tests run declare at most
# this (tadah_cmake_minimum_features).
cmake_minimum_required(VERSION 3.16)
catch_discover_tests(test_tadah)


# ------------------------------------------------------------------
# 2. Version single source of truth (CORE blueprint TOML)
# ------------------------------------------------------------------
# The binary reports the blueprint version it was built from.
string(REPLACE "." "\\." _tadah_version_re "${TADAH_VERSION_FULL}")
add_test(NAME tadah_version_matches_blueprint COMMAND tadah --version)
set_tests_properties(tadah_version_matches_blueprint PROPERTIES
  PASS_REGULAR_EXPRESSION "^${_tadah_version_re}[\r\n]*$")

# cmake-tools' reader: found through FetchContent'd CORE, [TADAH]-scoped,
# registered as a configure dependency; a missing blueprint or version
# line is a configure error (no 0.0.0 default).
# Each case configures version_reader/project in a new build directory
# (version_reader/check_version_reader.cmake removes the last run's).
FetchContent_GetProperties(tadah_cmake)
set(_vr ${CMAKE_CURRENT_SOURCE_DIR}/version_reader)
function(tadah_version_reader_test name case core regex)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${case}
      -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
      "-DCORE=${core}"
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/version_reader/${name}
      -P ${_vr}/check_version_reader.cmake)
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${regex}")
endfunction()
tadah_version_reader_test(tadah_version_reader_fetchcontent fetchcontent
  ${_vr}/core_ok "RESULT=9\\.8\\.7\\|9\\.8\\.7-rc\\.1\n.*DEPENDS=[^\n]*core_ok/blueprint/blueprint_tadah\\.toml")
# Pins: a blueprint whose text has unbalanced square brackets and a
# semicolon (a key description "a number in (0, 1]", before, in and after
# [TADAH]) gives the version of its [TADAH] table. Before the fix: the
# reader split the file into a CMake list of lines, in which a bracket
# joins the lines up to the matching one into one element, so [TADAH] was
# not found ("'version = \"...\"' not found in [TADAH]").
tadah_version_reader_test(tadah_version_reader_brackets fetchcontent
  ${_vr}/core_brackets "RESULT=9\\.8\\.7\\|9\\.8\\.7-rc\\.1\n.*DEPENDS=[^\n]*core_brackets/blueprint/blueprint_tadah\\.toml")
tadah_version_reader_test(tadah_version_reader_no_version fetchcontent
  ${_vr}/core_noversion "not found in \\[TADAH\\][ \n]+of[ \n]+[^\n]*core_noversion")
tadah_version_reader_test(tadah_version_reader_no_core no_core
  "" "could not find blueprint_tadah\\.toml\\.[ \n]+Tadah\\.CORE has not been made available")


# ------------------------------------------------------------------
# 3. Removed build options (cmake-tools)
# ------------------------------------------------------------------
# cmake-tools stops the configure step when a removed option is ON
# (TADAH_BUILD_MPI, the legacy MPI training build removed in 1.6.0), and
# accepts OFF, as set by older presets, and an unset option. Each case
# configures removed_options/project, which fetches this build's cmake-tools
# like every Tadah module does.
foreach(_case on preset_on off preset_off unset)
  add_test(NAME tadah_removed_option_build_mpi_${_case}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${_case}
      -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/removed_options/${_case}
      "-DGENERATOR=${CMAKE_GENERATOR}"
      -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
      -DCXX=${CMAKE_CXX_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/removed_options/check_removed_option.cmake)
  set_tests_properties(tadah_removed_option_build_mpi_${_case} PROPERTIES
    LABELS cmake
    TIMEOUT 120)
endforeach()


# ------------------------------------------------------------------
# 4. Branch detection (cmake-tools' tadah_get_git_branch)
# ------------------------------------------------------------------
# The ref every Tadah module fetches its sibling modules at: CI and Read
# the Docs variables (READTHEDOCS_GIT_IDENTIFIER before the version name,
# which is 'latest' or 'stable' for those versions; a variable counts only
# when it is set and non-empty), then the checkout's upstream or local
# branch. Each case runs git_branch/probe.cmake with a controlled
# environment; the git cases use throw-away local repositories. Each case
# also checks that the docs build (docs/source/config/source_locator.py)
# names the same ref from the same variables.
find_package(Git REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
foreach(_case ci rtd_latest rtd_stable rtd_version_name rtd_version
              rtd_empty_identifier ci_empty preset
              upstream local_branch detached not_git)
  add_test(NAME tadah_git_branch_${_case}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${_case}
      -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/git_branch/${_case}
      -DGIT=${GIT_EXECUTABLE}
      -DPYTHON=${Python3_EXECUTABLE}
      -DLOCATOR_DIR=${PROJECT_SOURCE_DIR}/docs/source/config
      -P ${CMAKE_CURRENT_SOURCE_DIR}/git_branch/check_git_branch.cmake)
  set_tests_properties(tadah_git_branch_${_case} PROPERTIES
    LABELS cmake
    TIMEOUT 60)
endforeach()


# ------------------------------------------------------------------
# 5. Doxygen input of the `docs` target (cmake/doxygen_input.cmake)
# ------------------------------------------------------------------
# The include/ trees of the configured modules only: HPO with
# TADAH_ENABLE_NFP, never a bare "/include" from an empty source
# directory. The function cases use scratch directories; 'configured'
# checks this build's docs/Doxyfile.
set(_dx ${CMAKE_CURRENT_SOURCE_DIR}/doxygen_input/probe.cmake)
function(tadah_doxygen_input_test case regex)
  add_test(NAME tadah_doxygen_input_${case}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${case}
      -DFUNCTIONS=${PROJECT_SOURCE_DIR}/cmake/doxygen_input.cmake
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/doxygen_input/${case}
      -DDOXYFILE=${CMAKE_BINARY_DIR}/docs/Doxyfile
      -P ${_dx})
  set_tests_properties(tadah_doxygen_input_${case} PROPERTIES
    LABELS "cmake;docs"
    TIMEOUT 60)
  if(NOT "${regex}" STREQUAL "NONE")
    set_tests_properties(tadah_doxygen_input_${case} PROPERTIES
      PASS_REGULAR_EXPRESSION "${regex}")
  endif()
endfunction()
tadah_doxygen_input_test(nfp_off
  "INPUT=\\[\"<W>/CORE/include\" \"<W>/MODELS/include\" \"<W>/MLIP/include\"\\]")
tadah_doxygen_input_test(nfp_on
  "INPUT=\\[\"<W>/CORE/include\" \"<W>/MODELS/include\" \"<W>/MLIP/include\" \"<W>/HPO/include\"\\]")
# CMake wraps a long error message: allow a line break between words.
tadah_doxygen_input_test(nfp_on_without_hpo
  "Tadah\\.HPO[ \n]+is[ \n]+not[ \n]+configured")
tadah_doxygen_input_test(no_include
  "/MODELS/include[ \n]+is[ \n]+not[ \n]+a[ \n]+directory")
tadah_doxygen_input_test(configured NONE)


# ------------------------------------------------------------------
# 6. Catch test discovery (cmake-tools), used by every Tadah module
# ------------------------------------------------------------------
add_subdirectory(catch_discovery)


# ------------------------------------------------------------------
# 7. Documentation build tools (docs/tools/tests)
# ------------------------------------------------------------------
# Unit tests of the docs input lookup (docs/source/config/source_locator.py),
# of the hpo.html redirect check (docs/tools/check_redirect.py), of the
# configuration-keys and CLI page generators (docs/source/config/read_toml.py,
# docs/source/cli/generate_cli.py) and of the ANNEAL key tables' generator
# (docs/source/config/read_anneal_keys.py, which needs Python 3.11 or later
# for tomllib). They never use the network.
#
# Some cases need node, sphinx-build or the Python modules sphinx,
# sphinx_rtd_theme or yaml, and skip themselves when one is missing.
# unittest still exits 0 then, ending with "OK (skipped=N)";
# that run is reported as Skipped, not Passed, so a pass means every case
# ran. A failing run ends with "FAILED (...)" and stays a failure. "Ran 0
# tests" is a failure too (before Python 3.12 unittest exits 0 on it).
# No bytecode is written: the tests import modules from the source tree.
# NB: add_test() arguments must not contain ';' or be empty (MLIP's
# add_test() override; see catch_discovery/CMakeLists.txt).
add_test(NAME tadah_docs_tools
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${PROJECT_SOURCE_DIR}/docs/tools/tests -v)
set_tests_properties(tadah_docs_tools PROPERTIES
  LABELS docs
  TIMEOUT 120
  ENVIRONMENT PYTHONDONTWRITEBYTECODE=1
  SKIP_REGULAR_EXPRESSION "\nOK \\(skipped=[0-9]+"
  FAIL_REGULAR_EXPRESSION "\nRan 0 tests ")


# ------------------------------------------------------------------
# 8. `tadah hpo` stopped by a signal exits with 128 + the signal number
# ------------------------------------------------------------------
# cli_signal/check_hpo_signal.py runs `tadah hpo` on a tiny problem (no
# LAMMPS, no network), signals it once evaluations are on disk, and checks
# the exit status (130 SIGINT, 143 SIGTERM, 140 SIGUSR2), the journal
# trailer, best_pot.tadah and loss.tadah.
#
# cli_signal/check_hpo_second_signal.py signals `tadah hpo` twice while a
# PC_LAMMPS script's long `run` is in progress: the first signal is
# announced and the evaluation goes on, the second ends tadah at once,
# killed by it, and under PC_ISOLATION PROCESS its workers with it. Before
# the fix: nothing announced, and the second signal did not end it (the
# check kills it after 20 s). The MPI build has no PROCESS workers.
if(TADAH_ENABLE_NFP)
  foreach(_sig SIGINT SIGTERM SIGUSR2)
    add_test(NAME tadah_hpo_exit_status_${_sig}
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/cli_signal/check_hpo_signal.py
        --tadah $<TARGET_FILE:tadah> --signal ${_sig})
    set_tests_properties(tadah_hpo_exit_status_${_sig} PROPERTIES
      LABELS "hpo;shutdown"
      TIMEOUT 300
      ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
  endforeach()
  set(_second_signal_modes THREAD)
  if(NOT TADAH_HPO_MPI)
    list(APPEND _second_signal_modes PROCESS)
  endif()
  foreach(_sig SIGINT SIGTERM)
    foreach(_mode IN LISTS _second_signal_modes)
      add_test(NAME tadah_hpo_second_signal_${_sig}_${_mode}
        COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/cli_signal/check_hpo_second_signal.py
          --tadah $<TARGET_FILE:tadah> --signal ${_sig} --isolation ${_mode})
      set_tests_properties(tadah_hpo_second_signal_${_sig}_${_mode} PROPERTIES
        LABELS "hpo;shutdown"
        TIMEOUT 300
        ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
    endforeach()
  endforeach()
endif()


# ------------------------------------------------------------------
# 8b. `tadah hpo` when evaluations fail
# ------------------------------------------------------------------
# cli_failed_evaluations/check_hpo_failed_evaluations.py runs `tadah hpo`
# on a tiny problem (no LAMMPS, no network) whose evaluations fail. When
# none produced a usable loss (random_skip, gfs_throws) the run exits with
# status 1, says so on stderr and in the journal trailer, and writes no
# best_pot.tadah; DLIB:GFS reaches MAXEVAL (it used to crash, SIGSEGV).
# gfs_mixed starts with failures and ends as a successful run.
# fd_straddle: a failed evaluation in DLIB:BFGS's finite-difference stencil
# is not differenced, and the gradient takes the usable side (status 0);
# fd_inside: a start inside the failure region ends the search at its first
# gradient, after one evaluation; failscore_zero: FAILSCORE 0 is a startup
# error at its line; ceres_box (a build with Ceres): a point
# CERES:STEEPEST_DESCENT proposes outside the OPTIM box is scored by its
# distance, not evaluated, and is not the best.
if(TADAH_ENABLE_NFP)
  set(_failed_evaluation_cases random_skip gfs_throws gfs_mixed fd_straddle
                               fd_inside failscore_zero)
  if(TADAH_ENABLE_CERES)
    list(APPEND _failed_evaluation_cases ceres_box)
  endif()
  foreach(_case IN LISTS _failed_evaluation_cases)
    add_test(NAME tadah_hpo_failed_evaluations_${_case}
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/cli_failed_evaluations/check_hpo_failed_evaluations.py
        --tadah $<TARGET_FILE:tadah> --case ${_case})
    set_tests_properties(tadah_hpo_failed_evaluations_${_case} PROPERTIES
      LABELS "hpo"
      TIMEOUT 300
      ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
  endforeach()
endif()


# ------------------------------------------------------------------
# 9. The CLI examples the documentation includes
# ------------------------------------------------------------------
# One entry per examples/CLI/<name>/README.rst that docs/source/examples.rst
# includes: cli_examples/check_cli_example.py runs the README's `tadah`
# commands, in a scratch copy of the example, and checks their results. An
# example that runs `tadah hpo` is Skipped in a build without NFP.
set(_examples_rst ${PROJECT_SOURCE_DIR}/docs/source/examples.rst)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_examples_rst})
file(STRINGS ${_examples_rst} _cli_example_includes
  REGEX "^\\.\\. include:: \\.\\./\\.\\./examples/CLI/[^/]+/README\\.rst$")
if(NOT _cli_example_includes)
  message(FATAL_ERROR "${_examples_rst} includes no examples/CLI/<name>/README.rst")
endif()
if(TADAH_ENABLE_NFP)
  set(_cli_example_nfp ON)
else()
  set(_cli_example_nfp OFF)
endif()
foreach(_inc IN LISTS _cli_example_includes)
  string(REGEX REPLACE "^.*/examples/CLI/([^/]+)/README\\.rst$" "\\1" _ex "${_inc}")
  add_test(NAME tadah_cli_example_${_ex}
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/cli_examples/check_cli_example.py
      --tadah $<TARGET_FILE:tadah>
      --example ${PROJECT_SOURCE_DIR}/examples/CLI/${_ex}
      --workdir ${CMAKE_CURRENT_BINARY_DIR}/cli_examples/${_ex}
      --nfp ${_cli_example_nfp})
  set_tests_properties(tadah_cli_example_${_ex} PROPERTIES
    LABELS examples
    TIMEOUT 900
    SKIP_RETURN_CODE 77
    ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
endforeach()


# ------------------------------------------------------------------
# 10. The Quick Start potential (docs/source/quickstart)
# ------------------------------------------------------------------
# A guard against a stale shipped potential: quickstart/check_quickstart_pot.py
# retrains docs/source/quickstart/pot.tadah, in scratch copies of the
# directory, with `tadah train -c config.train` and with `tadah --task
# taskfile.txt`, and checks that both potentials have the keys of the shipped
# one and predict the same energies, forces and stresses on db.predict. (The
# LAMMPS run of the page, in.ml, is the test_tadah case "quickstart: ...".)
add_test(NAME tadah_quickstart_reproduces_pot
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/quickstart/check_quickstart_pot.py
    --tadah $<TARGET_FILE:tadah>
    --quickstart ${PROJECT_SOURCE_DIR}/docs/source/quickstart
    --workdir ${CMAKE_CURRENT_BINARY_DIR}/quickstart_pot)
set_tests_properties(tadah_quickstart_reproduces_pot PROPERTIES
  LABELS examples
  TIMEOUT 600
  ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)


# ------------------------------------------------------------------
# 11. Header self-containment (cmake-tools' tadah_add_header_check)
# ------------------------------------------------------------------
# tadah_headers (label headers): every header of include/ compiles on its
# own and twice in a row, with the include directories, definitions and
# options of the tadah executable. Each module checks its own library the
# same way (<library>_headers). Before the fix: passes as well (a pin: no
# header of include/ failed when the check was added).
tadah_add_header_check(tadah)

# The function itself: each case configures header_check/project, which
# fetches this build's cmake-tools like every Tadah module does, and runs
# its header checks (see header_check/check_header_check.cmake). Before
# the fix (cmake-tools without the function): every case fails at its
# configure step ('Unknown CMake command "tadah_add_header_check"').
# private_entry: a header of include/ that includes a header of the second
# HEADER_DIRS entry, a directory PRIVATE to the library (as CORE's
# generated/ is), fails. Before that fix: it passed, because every
# HEADER_DIRS entry was on the include path of every header's check.
# refusals: each refusal of the function stops the configure step with its
# message (an unexpected argument, no such target, an ALIAS or IMPORTED
# target, HEADER_DIRS without a value, an entry that is not a directory or
# has no header, the same relative path in two entries). Before the fix:
# passes as well (a pin of refusals the function had from the start).
# TADAH_HEADER_CHECK_JOBS: bad builds on one process, so its good headers
# compile only if the build keeps going past the failed ones; good builds
# on the physical cores (the default); refusals include a value that is
# not a number of processes. Before that fix: the variable was not read,
# so bad's entry ran on every physical core ('probe_headers --parallel:
# '20', expected '1'') and compiled its good headers whether the build
# kept going or not, and no value was refused.
# The script needs CMake 3.19 (string(JSON)); on CMake 3.16-3.18, which
# these tests accept, it prints "SKIPPED: check_header_check.cmake needs
# CMake 3.19 or later (string(JSON)); this is CMake <v>" and the entries are
# Skipped (see tadah_header_check_cmake_* below).
foreach(_case good bad private_dir private_entry new_header refusals)
  add_test(NAME tadah_header_check_${_case}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${_case}
      -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/header_check/${_case}
      "-DGENERATOR=${CMAKE_GENERATOR}"
      -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
      -DCXX=${CMAKE_CXX_COMPILER}
      -DCTEST=${CMAKE_CTEST_COMMAND}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/header_check/check_header_check.cmake)
  set_tests_properties(tadah_header_check_${_case} PROPERTIES
    LABELS cmake
    TIMEOUT 300
    SKIP_REGULAR_EXPRESSION
      "(^|\n)SKIPPED: check_header_check\\.cmake needs CMake ")
endforeach()

# The CMake that header_check/check_header_check.cmake needs: string(JSON),
# CMake 3.19, while these tests accept 3.16. tadah_header_check_cmake_old
# and _minimum run the script as CMake 3.18.4 and 3.15, and 3.19 and
# 3.24.2, would (its -DAS_CMAKE_VERSION; header_check/check_cmake_minimum.cmake):
# below 3.19 it prints exactly "SKIPPED: check_header_check.cmake needs
# CMake 3.19 or later (string(JSON)); this is CMake <v>" and exits 0, from
# 3.19 on it goes on to its argument checks. Before the fix: _old fails
# (the script ignored AS_CMAKE_VERSION and went on, failing at its
# argument check; on a real CMake 3.18-3.23 it failed on --fresh,
# --test-dir or string(JSON)); _minimum passes as well (a pin).
foreach(_case old minimum)
  add_test(NAME tadah_header_check_cmake_${_case}
    COMMAND ${CMAKE_COMMAND}
      -DCASE=${_case}
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/header_check/check_header_check.cmake
      -P ${CMAKE_CURRENT_SOURCE_DIR}/header_check/check_cmake_minimum.cmake)
  set_tests_properties(tadah_header_check_cmake_${_case} PROPERTIES
    LABELS cmake
    TIMEOUT 60)
endforeach()

# tadah_cmake_minimum_features: no CMake file of Tadah.MLIP (CMakeLists.txt,
# cmake/, tests/) uses a command, sub-command, command-line option or test
# property newer than the cmake_minimum_required() that governs it, its own
# or that of the nearest CMakeLists.txt above it, and none declares first a
# minimum above that of what runs it: the tests' (this file's) for a file
# under tests/, the umbrella's for the others
# (cmake_minimum/check_cmake_features.py, which has the table of features
# and versions). Before the fix: 17 uses -- tests/CMakeLists.txt (cmake
# --fresh, 3.24, in the umbrella's 3.15), header_check/check_header_check.cmake
# (--fresh, ctest --test-dir 3.20, string(JSON) 3.19, in its 3.18),
# removed_options/check_removed_option.cmake (--fresh) and
# catch_discovery/check_skip.cmake (--test-dir), both 3.18. Before an
# earlier fix: 9 -- this file's two SKIP_REGULAR_EXPRESSION (3.16) under the
# umbrella's 3.15, and seven scripts that declare 3.18 above it
# (catch_discovery/check_discovery.cmake, check_module.cmake,
# check_skip.cmake, doxygen_input/probe.cmake,
# git_branch/check_git_branch.cmake, git_branch/probe.cmake,
# removed_options/check_removed_option.cmake), which stopped on CMake
# 3.15-3.17 ("CMake 3.18 or higher is required").
add_test(NAME tadah_cmake_minimum_features
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake_minimum/check_cmake_features.py
    ${PROJECT_SOURCE_DIR})
set_tests_properties(tadah_cmake_minimum_features PROPERTIES
  LABELS cmake
  TIMEOUT 60
  ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)


# ------------------------------------------------------------------
# 12. The tadah executable without TADAH_ENABLE_NFP
# ------------------------------------------------------------------
# A build with NFP links tadah with HPO; the default build (NFP OFF) links
# it with tadah.mlip alone. tadah_without_nfp_links builds the second from
# the first's sources (without_nfp/CMakeLists.txt: tadah_without_nfp, not
# part of `all`) and runs it (without_nfp/check_without_nfp.cmake):
# `--version` prints the version, `--tadah-pc-worker`, the PC_ISOLATION
# worker entry, is an unknown argument, and `tadah hpo` is refused naming
# TADAH_ENABLE_NFP. In a build without NFP the tadah target is that
# executable (tadah_version_matches_blueprint runs it).
# Before the fix: main() called tadah::hpo::pc_worker_main in every build,
# so the link failed ("undefined reference to
# `tadah::hpo::pc_worker_main(int, char**)'") and a build without NFP had
# no tadah. Before the refusal was added: `tadah hpo` said
# "EngineFactory::create: No engine found for HPO".
if(TADAH_ENABLE_NFP)
  add_subdirectory(without_nfp)
  cmake_host_system_information(RESULT _wn_jobs QUERY NUMBER_OF_PHYSICAL_CORES)
  if(NOT _wn_jobs GREATER 0)
    set(_wn_jobs 1)
  endif()
  add_test(NAME tadah_without_nfp_links
    COMMAND ${CMAKE_COMMAND}
      -DBUILD_DIR=${CMAKE_BINARY_DIR}
      -DCONFIG=$<CONFIG>
      -DJOBS=${_wn_jobs}
      -DEXE=$<TARGET_FILE:tadah_without_nfp>
      -DVERSION=${TADAH_VERSION_FULL}
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/without_nfp/work
      -P ${CMAKE_CURRENT_SOURCE_DIR}/without_nfp/check_without_nfp.cmake)
  # It builds in this build tree, as the header checks do: one at a time.
  set_tests_properties(tadah_without_nfp_links PROPERTIES
    LABELS cmake
    TIMEOUT 900
    PROCESSORS ${_wn_jobs}
    RESOURCE_LOCK tadah_header_check)
endif()


# ------------------------------------------------------------------
# 13. Text check (cmake-tools' tadah_add_text_check)
# ------------------------------------------------------------------
# tadah_text and cmake-tools_text (label text): no line of the text files
# of this repository, or of the cmake-tools this build uses, carries a
# person's initials, a reference to co-workers, a decision attributed to
# the code's owner, the name of an AI tool, a note kept outside the
# repository or filler wording (testing/tadah_text_check.cmake lists the
# rules and the files read). Each module checks its own repository the same
# way (<library>_text). Before the fix: tadah_text fails, 108 lines (97 with
# a person's initials, 6 decision notes, 4 workspace notes and .gitignore's
# entry for an AI tool's state directory); cmake-tools_text fails, 2 lines
# (that entry and its comment in .gitignore).
tadah_add_text_check(tadah)
tadah_add_text_check(cmake-tools ROOT ${tadah_cmake_SOURCE_DIR})

# The function and its script, on throw-away trees and a throw-away project
# (text_check/check_text_check.cmake lists the cases: a clean tree, a line
# breaking each rule, the skipped directories and files, a line split by a
# ';', a git work tree's ignored and untracked files, the refusals of the
# script and of the function). Before the fix: passes as well (the check's
# own test). The git cases need git (GIT); without it the script says
# "git cases not run".
find_package(Git QUIET)
add_test(NAME tadah_text_check_tool
  COMMAND ${CMAKE_COMMAND}
    -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/text_check
    "-DGENERATOR=${CMAKE_GENERATOR}"
    -DCTEST=${CMAKE_CTEST_COMMAND}
    "-DGIT=${GIT_EXECUTABLE}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/text_check/check_text_check.cmake)
set_tests_properties(tadah_text_check_tool PROPERTIES
  LABELS cmake
  TIMEOUT 300)
