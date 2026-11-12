# ====================================================================
#  Where the Tadah modules of a build come from
#
#  The repository that holds cmake-tools holds the Tadah modules too, each
#  in a directory of its own (core, models, mlip, md, hpo, lammps). A
#  build takes every module from there: no option and no
#  environment variable chooses another place, and nothing is fetched.
#
#  Usage inside a build file of the repository, where it needs the module:
#
#     tadah_add_module(Tadah.CORE core)
#
#  The first argument is the name of the module's project() and the second
#  its directory in the repository. The module is added to the build once,
#  whichever build file asks first, in the build directory
#  _deps/<name in lower case>-build. tadah_module_directory(<outVar> <name>
#  <directory>) only names the directory, after the same checks. A
#  FETCHCONTENT_SOURCE_DIR_<NAME> that names another directory is
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

function(tadah_module_directory out_var name subdir)
  get_property(_root GLOBAL PROPERTY TADAH_REPOSITORY_ROOT)
  get_filename_component(_root_real "${_root}" REALPATH)

  # The project that asks is the repository's root or one of its module
  # directories: a cmake-tools of another checkout would otherwise
  # move every module to that checkout.
  get_filename_component(_caller "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
  get_filename_component(_caller_parent "${_caller}/.." REALPATH)
  if(NOT _caller STREQUAL _root_real AND NOT _caller_parent STREQUAL _root_real)
    message(FATAL_ERROR
      "cmake-tools was loaded from '${_root}/cmake-tools', which is not the "
      "repository of the project being configured ('${_caller}'): the "
      "build holds build files of two checkouts. A build takes cmake-tools "
      "and every Tadah! module from one repository.")
  endif()

  set(local_path "${_root}/${subdir}")
  if(NOT IS_DIRECTORY "${local_path}")
    message(FATAL_ERROR
      "The Tadah! module ${subdir} is not in the repository of cmake-tools: "
      "directory '${local_path}' not found.")
  endif()

  string(TOUPPER "${name}" _uc)
  set(_given "${FETCHCONTENT_SOURCE_DIR_${_uc}}")
  if(_given)
    get_filename_component(_given_real "${_given}" REALPATH)
    get_filename_component(_local_real "${local_path}" REALPATH)
    if(NOT _given_real STREQUAL _local_real)
      message(FATAL_ERROR
        "FETCHCONTENT_SOURCE_DIR_${_uc}=${_given} (resolved: ${_given_real}) "
        "names another directory than the module of this repository, "
        "'${local_path}'. A build takes every Tadah! module from its "
        "repository. The cache of the build directory keeps the setting: "
        "-UFETCHCONTENT_SOURCE_DIR_${_uc} on the next cmake command removes "
        "it, as does a new build directory.")
    endif()
  endif()
  set(${out_var} "${local_path}" PARENT_SCOPE)
endfunction()

# A macro, so that the module's directory scope is a child of the caller's,
# as with add_subdirectory() in the build file itself.
macro(tadah_add_module name subdir)
  tadah_module_directory(_tadah_module_dir "${name}" "${subdir}")
  string(TOLOWER "${name}" _tadah_module_lc)
  get_property(_tadah_module_added GLOBAL PROPERTY
               "TADAH_MODULE_ADDED_${_tadah_module_lc}")
  if(NOT _tadah_module_added)
    set_property(GLOBAL PROPERTY "TADAH_MODULE_ADDED_${_tadah_module_lc}" TRUE)
    message(STATUS "Tadah! module '${name}' is taken from ${_tadah_module_dir}")
    add_subdirectory("${_tadah_module_dir}"
                     "${CMAKE_BINARY_DIR}/_deps/${_tadah_module_lc}-build")
  endif()
  unset(_tadah_module_dir)
  unset(_tadah_module_lc)
  unset(_tadah_module_added)
endmacro()
