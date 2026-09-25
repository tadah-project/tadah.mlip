# tadah_doxygen_input(<out_var>)
#
# Sets <out_var> to the Doxygen INPUT of the `docs` target
# (docs/Doxyfile.in): the include/ directories of the Tadah modules this
# build configures, each quoted, separated by spaces.
#
#   * Tadah.CORE, Tadah.MODELS and Tadah.MLIP: always;
#   * Tadah.HPO: only with TADAH_ENABLE_NFP, the only configuration that
#     fetches it. Without NFP (the Read the Docs docs-only build among
#     them) the HPO API is not documented, and a status line says so.
#
# A module that is needed but not configured (<name>_SOURCE_DIR empty), or
# that has no include/ directory, is a configure error: an empty source
# directory must never turn into a bare "/include" entry.
function(tadah_doxygen_input out_var)
  set(_modules Tadah.CORE Tadah.MODELS Tadah.MLIP)
  if(TADAH_ENABLE_NFP)
    list(APPEND _modules Tadah.HPO)
  endif()

  set(_input "")
  foreach(_module IN LISTS _modules)
    set(_dir "${${_module}_SOURCE_DIR}")
    if("${_dir}" STREQUAL "")
      message(FATAL_ERROR
        "tadah_doxygen_input: ${_module} is not configured "
        "(${_module}_SOURCE_DIR is empty); the docs target documents its "
        "include/ directory.")
    endif()
    if(NOT IS_DIRECTORY "${_dir}/include")
      message(FATAL_ERROR
        "tadah_doxygen_input: ${_dir}/include is not a directory "
        "(${_module}).")
    endif()
    string(APPEND _input " \"${_dir}/include\"")
  endforeach()
  string(STRIP "${_input}" _input)

  if(NOT TADAH_ENABLE_NFP)
    message(STATUS "Doxygen: the docs target leaves out the Tadah.HPO API "
                   "(TADAH_ENABLE_NFP is OFF, so HPO is not fetched)")
  endif()
  set(${out_var} "${_input}" PARENT_SCOPE)
endfunction()
