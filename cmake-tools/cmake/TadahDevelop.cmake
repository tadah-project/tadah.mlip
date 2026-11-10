# ====================================================================
#  Developer helper: use local working copies of Tadah repositories
#
#  Enable by configuring with      -DTADAH_DEVELOP=ON
#  Environment variable required:  TADAH_PATH   (root that contains the repos)
#
#  Usage inside a project:
#
#     include(TadahDevelop)               # once
#     tadah_use_local_module(tadah_core core)
#     tadah_use_local_module(tadah_models models)
#
#  where the first argument is the *FetchContent name* (upper-/lower-case
#  doesn't matter) and the second is the directory name inside $TADAH_PATH.
# ====================================================================

option(TADAH_DEVELOP "Use local checkouts in \$TADAH_PATH" OFF)

function(tadah_use_local_module fetch_name subdir)
  if(NOT TADAH_DEVELOP)
    return()             # feature disabled → keep remote FetchContent
  endif()

  if(NOT DEFINED ENV{TADAH_PATH})
    message(FATAL_ERROR
      "TADAH_DEVELOP=ON but environment variable TADAH_PATH is not set.")
  endif()

  # Absolute path we expect
  set(local_path "$ENV{TADAH_PATH}/${subdir}")

  if(NOT IS_DIRECTORY "${local_path}")
    message(FATAL_ERROR
      "TADAH_DEVELOP=ON but directory '${local_path}' not found.")
  endif()

  # Convert fetch_name to upper case for the magic variable
  string(TOUPPER "${fetch_name}" _uc)
  set("FETCHCONTENT_SOURCE_DIR_${_uc}" "${local_path}" PARENT_SCOPE)

  message(STATUS "TADAH_DEVELOP: '${fetch_name}' will be taken from ${local_path}")
endfunction()

#--------------------------------------------------------------------
#  tadah_get_git_branch(<outVar>)
#
#  Returns in <outVar> the git ref the Tadah modules are fetched at
#  (GIT_TAG ${TADAH_GIT_BRANCH}). A value already set (-D<outVar>=<ref>)
#  is kept. Otherwise, the first of:
#
#  A CI or Read the Docs variable counts only when it is set and not
#  empty (as in the docs build's source_locator.py); one that is set but
#  empty is skipped, with a status message.
#
#    • in GitLab CI  →  $CI_COMMIT_REF_NAME (the branch, or the tag on a
#                        tag pipeline)
#    • on ReadTheDocs →  $READTHEDOCS_GIT_IDENTIFIER, the branch or tag
#                        Read the Docs checked out. Its "latest" and
#                        "stable" versions are named after no ref, so
#                        the version name cannot stand in for it there.
#                        Then $READTHEDOCS_VERSION_NAME (verbose ref
#                        name, e.g. "release/beta-prep") and
#                        $READTHEDOCS_VERSION (URL slug,
#                        "release-beta-prep") for environments that
#                        pre-date the newer variables.
#    • local         →  the upstream branch of HEAD (origin/<b> → <b>),
#                        else the local branch name, of the checkout of
#                        the top-level project (CMAKE_SOURCE_DIR; for the
#                        LAMMPS plugin build that is the LAMMPS checkout).
#                        Outside a branch checkout (tarball, detached
#                        HEAD) <outVar> is left empty with a warning; pass
#                        -D<outVar>=<ref> when the modules must be
#                        fetched.
#--------------------------------------------------------------------
function(tadah_get_git_branch out_var)
  # Don't override an existing value (set in caller scope or via -D... cache)
  if(DEFINED ${out_var} AND NOT "${${out_var}}" STREQUAL "")
    message(STATUS "${out_var} already set to '${${out_var}}' — skipping detection")
    return()
  endif()

  message(STATUS "Getting branch name...")
  foreach(_var CI_COMMIT_REF_NAME READTHEDOCS_GIT_IDENTIFIER
               READTHEDOCS_VERSION_NAME READTHEDOCS_VERSION)
    if(DEFINED ENV{${_var}} AND "$ENV{${_var}}" STREQUAL "")
      message(STATUS "tadah_get_git_branch: ${_var} is set but empty; "
                     "it names no ref and is skipped")
    endif()
  endforeach()
  if(NOT "$ENV{CI_COMMIT_REF_NAME}" STREQUAL "")
    set(_branch "$ENV{CI_COMMIT_REF_NAME}")
    message(STATUS "CI/CD branch: ${_branch}")
  elseif(NOT "$ENV{READTHEDOCS_GIT_IDENTIFIER}" STREQUAL "")
    set(_branch "$ENV{READTHEDOCS_GIT_IDENTIFIER}")
    message(STATUS "RTD git identifier: ${_branch}")
  elseif(NOT "$ENV{READTHEDOCS_VERSION_NAME}" STREQUAL "")
    set(_branch "$ENV{READTHEDOCS_VERSION_NAME}")
    message(STATUS "RTD branch (verbose): ${_branch}")
  elseif(NOT "$ENV{READTHEDOCS_VERSION}" STREQUAL "")
    set(_branch "$ENV{READTHEDOCS_VERSION}")
    message(STATUS "RTD branch (slug fallback): ${_branch}")
  else()
    # Prefer the upstream branch name (the ref FetchContent would clone);
    # fall back to the local branch name when nothing is tracked.
    execute_process(
      COMMAND git rev-parse --abbrev-ref --symbolic-full-name "@{upstream}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE _upstream
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND _upstream MATCHES "^[^/]+/(.+)$")
      set(_branch "${CMAKE_MATCH_1}")
    else()
      execute_process(
        COMMAND git branch --show-current
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if("${_branch}" STREQUAL "")
        message(WARNING
          "tadah_get_git_branch: cannot determine the git branch of "
          "${CMAKE_SOURCE_DIR} (detached HEAD or not a git checkout). "
          "A build that fetches the Tadah modules at ${out_var} needs -D${out_var}=<ref>.")
      endif()
      message(STATUS "No tracking branch found, using local branch: ${_branch}")
    endif()
  endif()

  set(${out_var} "${_branch}" PARENT_SCOPE)
  message(STATUS "Getting branch done: ${_branch}")
endfunction()

