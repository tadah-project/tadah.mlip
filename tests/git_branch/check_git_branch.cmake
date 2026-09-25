# Checks cmake-tools' tadah_get_git_branch() (cmake/TadahDevelop.cmake):
# every Tadah module fetches its sibling modules at GIT_TAG
# ${TADAH_GIT_BRANCH}, which the function names from, in this order,
#   -DTADAH_GIT_BRANCH (kept as given), $CI_COMMIT_REF_NAME (GitLab CI),
#   $READTHEDOCS_GIT_IDENTIFIER, $READTHEDOCS_VERSION_NAME,
#   $READTHEDOCS_VERSION (Read the Docs), the upstream branch of the
#   checkout, its local branch; nothing (with a warning) otherwise.
# On Read the Docs the version names 'latest' and 'stable' are not refs:
# READTHEDOCS_GIT_IDENTIFIER is the branch or tag that was checked out.
#
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGIT=<git> -P check_git_branch.cmake
#
# Each case runs probe.cmake in a child `cmake -P` whose environment has
# none of the CI/RTD variables but those the case sets. The git cases build
# throw-away repositories under WORK_DIR (a local clone stands in for the
# remote; nothing touches the network), with the user's git configuration
# ignored and GIT_CEILING_DIRECTORIES stopping the search at WORK_DIR.
cmake_minimum_required(VERSION 3.18)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR GIT)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_git_branch.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(branch_vars CI_COMMIT_REF_NAME READTHEDOCS_GIT_IDENTIFIER
                READTHEDOCS_VERSION_NAME READTHEDOCS_VERSION)
set(clean_env "")
foreach(v IN LISTS branch_vars)
  list(APPEND clean_env "--unset=${v}")
endforeach()
list(APPEND clean_env "GIT_CONFIG_GLOBAL=/dev/null" "GIT_CONFIG_NOSYSTEM=1"
                      "GIT_CEILING_DIRECTORIES=${WORK_DIR}")

# git with the user's configuration ignored; stops on failure.
function(run_git dir)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${clean_env}
            "${GIT}" -c user.name=tadah-test -c user.email=test@invalid
                     -c commit.gpgsign=false ${ARGN}
    WORKING_DIRECTORY "${dir}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "git ${ARGN} (in ${dir}) failed (${rc}):\n${out}")
  endif()
endfunction()

# A repository whose only branch is v1-dev, and a clone of it (by path).
function(make_origin_and_clone)
  file(MAKE_DIRECTORY "${WORK_DIR}/origin")
  run_git("${WORK_DIR}/origin" init -q)
  run_git("${WORK_DIR}/origin" checkout -q -b v1-dev)
  run_git("${WORK_DIR}/origin" commit -q --allow-empty -m init)
  run_git("${WORK_DIR}" clone -q --branch v1-dev "${WORK_DIR}/origin" clone)
endfunction()

# case -> environment (VAR=value ...), -D preset, directory, expected branch
set(env "")
set(preset "")
set(dir "${WORK_DIR}")
set(expect_warning FALSE)
if(CASE STREQUAL "ci")
  set(env CI_COMMIT_REF_NAME=followups/x READTHEDOCS_GIT_IDENTIFIER=v1-dev
          READTHEDOCS_VERSION_NAME=latest)
  set(expect "followups/x")
elseif(CASE STREQUAL "rtd_latest")
  set(env READTHEDOCS_GIT_IDENTIFIER=v1-dev READTHEDOCS_VERSION_NAME=latest
          READTHEDOCS_VERSION=latest)
  set(expect "v1-dev")
elseif(CASE STREQUAL "rtd_stable")
  set(env READTHEDOCS_GIT_IDENTIFIER=v1.6.0 READTHEDOCS_VERSION_NAME=stable
          READTHEDOCS_VERSION=stable)
  set(expect "v1.6.0")
elseif(CASE STREQUAL "rtd_version_name")
  set(env READTHEDOCS_VERSION_NAME=release/x READTHEDOCS_VERSION=release-x)
  set(expect "release/x")
elseif(CASE STREQUAL "rtd_version")
  set(env READTHEDOCS_VERSION=release-x)
  set(expect "release-x")
elseif(CASE STREQUAL "preset")
  set(env CI_COMMIT_REF_NAME=followups/x READTHEDOCS_GIT_IDENTIFIER=v1-dev)
  set(preset "-DTADAH_GIT_BRANCH=v1.6.0")
  set(expect "v1.6.0")
elseif(CASE STREQUAL "upstream")
  # A branch tracking origin/v1-dev: the upstream's name, not the local one.
  make_origin_and_clone()
  run_git("${WORK_DIR}/clone" checkout -q -b work --track origin/v1-dev)
  set(dir "${WORK_DIR}/clone")
  set(expect "v1-dev")
elseif(CASE STREQUAL "local_branch")
  # No upstream: the local branch name.
  make_origin_and_clone()
  run_git("${WORK_DIR}/origin" checkout -q -b feature/y)
  set(dir "${WORK_DIR}/origin")
  set(expect "feature/y")
elseif(CASE STREQUAL "detached")
  make_origin_and_clone()
  run_git("${WORK_DIR}/clone" checkout -q --detach)
  set(dir "${WORK_DIR}/clone")
  set(expect "")
  set(expect_warning TRUE)
elseif(CASE STREQUAL "not_git")
  # A release tarball: no .git anywhere up to WORK_DIR.
  file(MAKE_DIRECTORY "${WORK_DIR}/tarball")
  set(dir "${WORK_DIR}/tarball")
  set(expect "")
  set(expect_warning TRUE)
else()
  message(FATAL_ERROR "check_git_branch.cmake: unknown CASE '${CASE}'")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${clean_env} ${env}
          "${CMAKE_COMMAND}" "-DTADAH_CMAKE_DIR=${TADAH_CMAKE_DIR}" ${preset}
          -P "${CMAKE_CURRENT_LIST_DIR}/probe.cmake"
  WORKING_DIRECTORY "${dir}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)

set(nfail 0)
macro(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

if(NOT rc EQUAL 0)
  fail("probe.cmake failed (exit ${rc})")
endif()
string(REGEX MATCH "TADAH_GIT_BRANCH=\\[([^]\n]*)\\]" matched "${out}")
set(got "${CMAKE_MATCH_1}")
if("${matched}" STREQUAL "")
  fail("the probe printed no TADAH_GIT_BRANCH")
elseif(NOT "${got}" STREQUAL "${expect}")
  fail("TADAH_GIT_BRANCH is '${got}', expected '${expect}'")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
string(FIND "${flat}" "cannot determine the git branch" warned)
if(expect_warning AND warned EQUAL -1)
  fail("no warning that the branch cannot be determined")
elseif(NOT expect_warning AND NOT warned EQUAL -1)
  fail("an unexpected warning that the branch cannot be determined")
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed for CASE=${CASE}; "
                      "probe output:\n${out}")
endif()
message(STATUS "CASE=${CASE}: TADAH_GIT_BRANCH='${expect}' ok")
