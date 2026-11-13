# ====================================================================
#  Where the Tadah modules of a build come from
#
#  The repository that holds this directory holds the Tadah modules too,
#  each in a directory of its own (core, models, mlip, md, hpo, lammps). A
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
#  _deps/<name in lower case>-build.
#
#  TADAH_DEVELOP is the developer build: -Wextra in every build type
#  (TadahCompileOptions.cmake). It does not change where a module comes
#  from.
# ====================================================================

option(TADAH_DEVELOP "Developer build: -Wextra in every build type" OFF)

# cmake/ -> the repository, from the resolved directory of this file. A
# global property: this file is read in the directory scope of the build
# file that asks first, and the macro is called from every module.
get_filename_component(_tadah_repository "${CMAKE_CURRENT_LIST_DIR}" REALPATH)
get_filename_component(_tadah_repository "${_tadah_repository}/.." ABSOLUTE)
set_property(GLOBAL PROPERTY TADAH_REPOSITORY_ROOT "${_tadah_repository}")
unset(_tadah_repository)

# A macro, so that the module's directory scope is a child of the caller's,
# as with add_subdirectory() in the build file itself.
macro(tadah_add_module name subdir)
  string(TOLOWER "${name}" _tadah_module_lc)
  get_property(_tadah_module_added GLOBAL PROPERTY
               "TADAH_MODULE_ADDED_${_tadah_module_lc}")
  if(NOT _tadah_module_added)
    set_property(GLOBAL PROPERTY "TADAH_MODULE_ADDED_${_tadah_module_lc}" TRUE)
    get_property(_tadah_module_dir GLOBAL PROPERTY TADAH_REPOSITORY_ROOT)
    set(_tadah_module_dir "${_tadah_module_dir}/${subdir}")
    message(STATUS "Tadah! module '${name}' is taken from ${_tadah_module_dir}")
    add_subdirectory("${_tadah_module_dir}"
                     "${CMAKE_BINARY_DIR}/_deps/${_tadah_module_lc}-build")
  endif()
  unset(_tadah_module_dir)
  unset(_tadah_module_lc)
  unset(_tadah_module_added)
endmacro()
