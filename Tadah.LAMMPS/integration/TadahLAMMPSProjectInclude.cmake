cmake_minimum_required(VERSION 3.20)

option(PKG_ML-TADAH "Enable Tadah ML-TADAH integration (external)" OFF)

set(TADAH_LAMMPS_SOURCE_DIR
    ""
    CACHE PATH "Path to a local Tadah.LAMMPS checkout")

set(TADAH_LAMMPS_QUIET
    ON
    CACHE BOOL "Minimize Tadah.LAMMPS configure output when built as dependency")

# Obsolete since the style-registry change described below: LAMMPS discovers
# our styles from the `PairStyle(...)` markers inside the ML-TADAH headers, so
# nothing has to name them here. Kept defined so an existing -D on someone's
# command line produces a warning instead of a silent unused-variable.
set(TADAH_LAMMPS_PAIR_STYLES
    "tadah:PairTadah"
    CACHE STRING
    "Deprecated and ignored; styles are discovered from the ML-TADAH headers")

# Capture the directory of THIS script now (before any DEFER happens).
# This must be stable and must NOT rely on CMAKE_CURRENT_LIST_DIR later.
set(_TADAH_INTEGRATION_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(_tadah_require_dir varname)
  if(NOT ${varname})
    message(FATAL_ERROR "ML-TADAH: ${varname} is not set")
  endif()
  if(NOT IS_DIRECTORY "${${varname}}")
    message(FATAL_ERROR "ML-TADAH: ${varname} is not a directory: ${${varname}}")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Style registration
#
# ML-TADAH is compiled into LAMMPS from outside lammps/src, so it never passes
# through LAMMPS's own `foreach(PKG ${STANDARD_PACKAGES})` loop and its styles
# are not discovered automatically. We therefore call LAMMPS's own
# RegisterStyles() on the ML-TADAH directory, which appends our headers to the
# same global properties LAMMPS fills for its packages.
#
# This replaces appending `#include "pair_tadah.h"` to the generated
# `styles/style_pair.h`. LAMMPS commit 7ee0259e ("style registry: stop
# generating the now-unused style_*.h and packages_*.h", 2 Jun 2026) deleted
# that file: pair/fix/compute styles are now emitted as `styles/style_*.cpp`
# that populate a RUNTIME registry, and only the array/mask categories
# (NBIN/NPAIR/NSTENCIL/NTOPO) still get a header. Patching a file that no
# longer exists made the configure hard-error on newer LAMMPS -- and a LAMMPS
# built without this hook links and runs fine while every input dies on
# `pair_style tadah` with "Unrecognized pair style". Going through
# RegisterStyles() instead is correct for BOTH generations, because both read
# the same global properties, and it needs no per-style bookkeeping here.
#
# TIMING is the whole difficulty. The registration has to happen after
# `include(StyleHeaderUtils)` (which defines RegisterStyles) and before
# GenerateStyleHeaders/GenerateStyleSources consume the properties. This file
# is pulled in via CMAKE_PROJECT_INCLUDE, i.e. straight after project() and
# hundreds of lines too early; a cmake_language(DEFER) runs at the end of the
# directory and is too late. LAMMPS sets LAMMPS_STYLE_HEADERS_DIR on the line
# immediately before those generators in both generations, so watching that
# variable lands us in exactly the right window.
# ---------------------------------------------------------------------------
function(_tadah_register_styles style_dir)
  if(NOT COMMAND RegisterStyles)
    message(FATAL_ERROR
      "ML-TADAH: LAMMPS's RegisterStyles() is not defined at style-generation "
      "time. This hook expects cmake/Modules/StyleHeaderUtils.cmake to have "
      "been included before LAMMPS sets LAMMPS_STYLE_HEADERS_DIR; this LAMMPS "
      "does not follow that layout.")
  endif()

  set(ml_tadah_dir "${TADAH_LAMMPS_SOURCE_DIR}/ML-TADAH")
  if(NOT IS_DIRECTORY "${ml_tadah_dir}")
    message(FATAL_ERROR "ML-TADAH: Cannot find ML-TADAH directory: ${ml_tadah_dir}")
  endif()

  RegisterStyles("${ml_tadah_dir}")
  set_property(GLOBAL PROPERTY TADAH_ML_TADAH_STYLE_DIR "${style_dir}")
  message(STATUS "ML-TADAH: registered styles from ${ml_tadah_dir}")
endfunction()

function(_tadah_on_style_headers_dir variable access value current_list_file stack)
  if(NOT access STREQUAL "MODIFIED_ACCESS")
    return()
  endif()
  get_property(_done GLOBAL PROPERTY TADAH_ML_TADAH_STYLES_REGISTERED)
  if(_done)
    return()
  endif()
  set_property(GLOBAL PROPERTY TADAH_ML_TADAH_STYLES_REGISTERED TRUE)
  _tadah_register_styles("${value}")
endfunction()

# Prove the registration actually produced a build artefact naming our style.
# Without this a future LAMMPS reshuffle degrades to a potential that builds,
# links and starts, then rejects `pair_style tadah` at run time -- the failure
# this hook exists to prevent, and the one that is most expensive to diagnose.
function(_tadah_verify_style_registration)
  get_property(_done GLOBAL PROPERTY TADAH_ML_TADAH_STYLES_REGISTERED)
  if(NOT _done)
    message(FATAL_ERROR
      "ML-TADAH: LAMMPS never set LAMMPS_STYLE_HEADERS_DIR, so the ML-TADAH "
      "styles were never registered. The pair style would be missing at run "
      "time ('Unrecognized pair style tadah'). This LAMMPS is not compatible "
      "with the ML-TADAH integration hook.")
  endif()

  get_property(style_dir GLOBAL PROPERTY TADAH_ML_TADAH_STYLE_DIR)
  set(_found "")
  # style_pair.cpp: LAMMPS >= 2 Jun 2026 (runtime registry).
  # style_pair.h:   older LAMMPS (compile-time include list).
  foreach(_cand "${style_dir}/style_pair.cpp" "${style_dir}/style_pair.h")
    if(EXISTS "${_cand}")
      file(READ "${_cand}" _contents)
      if(_contents MATCHES "pair_tadah\\.h")
        set(_found "${_cand}")
        break()
      endif()
    endif()
  endforeach()

  if(NOT _found)
    message(FATAL_ERROR
      "ML-TADAH: no generated style file in ${style_dir} references "
      "pair_tadah.h, so `pair_style tadah` would not be available at run "
      "time. Check that ${TADAH_LAMMPS_SOURCE_DIR}/ML-TADAH/pair_tadah.h "
      "still declares its style inside an `#ifdef PAIR_CLASS` block.")
  endif()
  message(STATUS "ML-TADAH: pair style 'tadah' registered via ${_found}")
endfunction()

function(_tadah_apply_after_lammps)
  if(NOT PKG_ML-TADAH)
    return()
  endif()

  if(NOT TARGET lammps)
    message(FATAL_ERROR "ML-TADAH: LAMMPS target 'lammps' not found (unexpected)")
  endif()

  if(NOT TARGET tadah.lammps)
    message(FATAL_ERROR "ML-TADAH: target 'tadah.lammps' not found (Tadah.LAMMPS did not configure?)")
  endif()

  # Use the saved integration directory, not CMAKE_CURRENT_LIST_DIR
  if(NOT EXISTS "${_TADAH_INTEGRATION_DIR}/ML-TADAH.cmake")
    message(FATAL_ERROR "ML-TADAH: Cannot find ${_TADAH_INTEGRATION_DIR}/ML-TADAH.cmake")
  endif()
  include("${_TADAH_INTEGRATION_DIR}/ML-TADAH.cmake")

  _tadah_verify_style_registration()
endfunction()

get_property(_tadah_done GLOBAL PROPERTY TADAH_ML_TADAH_INTEGRATION_DONE)
if(_tadah_done)
  return()
endif()
set_property(GLOBAL PROPERTY TADAH_ML_TADAH_INTEGRATION_DONE TRUE)

if(PKG_ML-TADAH)
  _tadah_require_dir(TADAH_LAMMPS_SOURCE_DIR)

  if(NOT TADAH_LAMMPS_PAIR_STYLES STREQUAL "tadah:PairTadah")
    message(WARNING
      "ML-TADAH: TADAH_LAMMPS_PAIR_STYLES is ignored since the LAMMPS style "
      "registry change; styles are discovered from the `PairStyle(...)` "
      "markers in ${TADAH_LAMMPS_SOURCE_DIR}/ML-TADAH. Drop the -D.")
  endif()

  set(TADAH_LAMMPS_QUIET "${TADAH_LAMMPS_QUIET}" CACHE BOOL "" FORCE)

  add_subdirectory("${TADAH_LAMMPS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tadah_lammps_build")

  set(Tadah_LAMMPS_SOURCE_DIR "${TADAH_LAMMPS_SOURCE_DIR}")

  # Fires when LAMMPS sets it, immediately before the style generators run.
  variable_watch(LAMMPS_STYLE_HEADERS_DIR _tadah_on_style_headers_dir)

  cmake_language(DEFER CALL _tadah_apply_after_lammps)
endif()
