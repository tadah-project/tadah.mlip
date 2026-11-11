# ====================================================================
#  Where the Tadah modules of a build come from
#
#  The repository that holds cmake-tools holds the Tadah modules too, each
#  in the directory of its name (CORE, MODELS, MLIP, MD, HPO,
#  Tadah.LAMMPS). A build takes every module from there: no option and no
#  environment variable chooses another place.
#
#  Usage inside a project, before the module is made available:
#
#     tadah_use_local_module(Tadah.CORE CORE)
#
#  where the first argument is the *FetchContent name* (upper-/lower-case
#  doesn't matter) and the second is the directory name in the repository.
#  A FETCHCONTENT_SOURCE_DIR_<NAME> that names another directory is
#  refused: one build never mixes the modules of two checkouts.
#
#  TADAH_DEVELOP is the developer build: -Wextra in every build type
#  (TadahCompileOptions.cmake) and CMake's deprecation warnings
#  (TadahProjectSetup.cmake). It does not change where a module comes from.
# ====================================================================

option(TADAH_DEVELOP "Developer build: -Wextra and CMake deprecation warnings" OFF)

# cmake-tools/cmake/ -> cmake-tools/ -> the repository, from the resolved
# directory of this file (cmake-tools reached through a symbolic link
# belongs to the repository that holds it, not to the one that holds the
# link). A global property: this file is read in cmake-tools' own
# directory scope, and the function is called from every module.
get_filename_component(_tadah_repository "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
get_filename_component(_tadah_repository "${_tadah_repository}/../.." ABSOLUTE)
set_property(GLOBAL PROPERTY TADAH_REPOSITORY_ROOT "${_tadah_repository}")
unset(_tadah_repository)

function(tadah_use_local_module fetch_name subdir)
  get_property(_root GLOBAL PROPERTY TADAH_REPOSITORY_ROOT)
  get_filename_component(_root_real "${_root}" REALPATH)

  # The project that asks is the repository's root or one of its module
  # directories: a cmake-tools pinned from another checkout would otherwise
  # move every module to that checkout.
  get_filename_component(_caller "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
  get_filename_component(_caller_parent "${_caller}/.." REALPATH)
  if(NOT _caller STREQUAL _root_real AND NOT _caller_parent STREQUAL _root_real)
    message(FATAL_ERROR
      "cmake-tools was loaded from '${_root}/cmake-tools', which is not the "
      "repository of the project being configured ('${_caller}'). A build "
      "takes cmake-tools and every Tadah! module from one repository; remove "
      "the FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE setting.")
  endif()

  set(local_path "${_root}/${subdir}")
  if(NOT IS_DIRECTORY "${local_path}")
    message(FATAL_ERROR
      "The Tadah! module ${subdir} is not in the repository of cmake-tools: "
      "directory '${local_path}' not found.")
  endif()

  # Convert fetch_name to upper case for the magic variable
  string(TOUPPER "${fetch_name}" _uc)
  set(_given "${FETCHCONTENT_SOURCE_DIR_${_uc}}")
  if(_given)
    get_filename_component(_given_real "${_given}" REALPATH)
    get_filename_component(_local_real "${local_path}" REALPATH)
    if(NOT _given_real STREQUAL _local_real)
      message(FATAL_ERROR
        "FETCHCONTENT_SOURCE_DIR_${_uc}=${_given} (resolved: ${_given_real}) "
        "names another directory than the module of this repository, "
        "'${local_path}'. A build takes every Tadah! module from its "
        "repository; remove the setting.")
    endif()
  endif()
  # In the cache as well as in the caller's scope (see TadahRepository.cmake).
  set("FETCHCONTENT_SOURCE_DIR_${_uc}" "${local_path}" CACHE PATH
      "The directory of ${subdir} in this repository (set by its build files)" FORCE)
  set("FETCHCONTENT_SOURCE_DIR_${_uc}" "${local_path}" PARENT_SCOPE)

  message(STATUS "Tadah! module '${fetch_name}' is taken from ${local_path}")
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
#                        HEAD) <outVar> is left empty, with a status line:
#                        no build needs it.
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
        message(STATUS
          "tadah_get_git_branch: cannot determine the git branch of "
          "${CMAKE_SOURCE_DIR} (detached HEAD or not a git checkout); "
          "${out_var} stays empty. No build needs it: the modules come from "
          "the repository.")
      endif()
      message(STATUS "No tracking branch found, using local branch: ${_branch}")
    endif()
  endif()

  set(${out_var} "${_branch}" PARENT_SCOPE)
  message(STATUS "Getting branch done: ${_branch}")
endfunction()

