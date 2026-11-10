# ====================================================================
#  Tadah version reader
#
#  Single-source-of-truth for the Tadah version is the [TADAH] table in
#  CORE's blueprint TOML:
#
#      core/blueprint/blueprint_tadah.toml
#
#  This module exposes one function:
#
#      tadah_read_version_from_blueprint(<output_var>
#          [BLUEPRINT_FILE <path>]
#          [REQUIRED])
#
#  Behaviour:
#    * If BLUEPRINT_FILE is given, parses exactly that file.
#    * Otherwise the blueprint is located through Tadah.CORE itself:
#        - the FetchContent source dir of Tadah.CORE (populated by
#          FetchContent_MakeAvailable(Tadah.CORE), including the
#          FETCHCONTENT_SOURCE_DIR_TADAH.CORE override used by
#          developer builds);
#        - ${Tadah.CORE_SOURCE_DIR}, set by CORE's own project() call.
#      Callers therefore read the version AFTER their Tadah dependencies
#      have been made available, then re-state project(... VERSION ...).
#    * The version is the first `version = "..."` (or '...') line of the
#      [TADAH] table: from the line holding only [TADAH] to the next line
#      that starts with '['. The file is read as text, so square brackets
#      and semicolons elsewhere in it (a key description "a number in
#      (0, 1]") do not move or hide the table.
#    * A missing blueprint, a missing [TADAH].version line or a version
#      without a numeric M.m.p prefix is a configure error. There is no
#      default version. REQUIRED is accepted for backward compatibility
#      and has no further effect.
#    * The blueprint is added to CMAKE_CONFIGURE_DEPENDS of the calling
#      directory, so bumping the version re-runs the configure step.
#
#  Outputs (in the caller's scope):
#      <output_var>          numeric version for project(VERSION), e.g. 1.6.0
#      <output_var>_FULL     version string as written, e.g. 1.6.0-rc.1
#      <output_var>_SOURCE   path of the blueprint that was read
# ====================================================================

include(FetchContent)

function(tadah_read_version_from_blueprint output_var)
  set(_options REQUIRED)
  set(_oneValueArgs BLUEPRINT_FILE)
  set(_multiValueArgs)
  cmake_parse_arguments(_TVR "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})

  # ---- Build candidate list -------------------------------------
  set(_candidates "")
  if(_TVR_BLUEPRINT_FILE)
    list(APPEND _candidates "${_TVR_BLUEPRINT_FILE}")
  else()
    FetchContent_GetProperties(Tadah.CORE)
    if(tadah.core_POPULATED)
      list(APPEND _candidates "${tadah.core_SOURCE_DIR}/blueprint/blueprint_tadah.toml")
    endif()
    if(DEFINED Tadah.CORE_SOURCE_DIR)
      list(APPEND _candidates "${Tadah.CORE_SOURCE_DIR}/blueprint/blueprint_tadah.toml")
    endif()
  endif()

  # ---- Pick the first that exists -------------------------------
  set(_blueprint "")
  foreach(_c IN LISTS _candidates)
    if(EXISTS "${_c}")
      set(_blueprint "${_c}")
      break()
    endif()
  endforeach()

  if(NOT _blueprint)
    if(_candidates)
      set(_searched "Searched: ${_candidates}")
    else()
      string(CONCAT _searched
        "Tadah.CORE has not been made available yet: call "
        "FetchContent_MakeAvailable() for the Tadah dependencies "
        "before reading the version, or pass BLUEPRINT_FILE.")
    endif()
    message(FATAL_ERROR
      "tadah_read_version_from_blueprint: could not find "
      "blueprint_tadah.toml. ${_searched}")
  endif()

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_blueprint}")

  # ---- Parse [TADAH] table ---------------------------------------
  # The [TADAH] table runs from the line that holds only its header (blanks
  # around it allowed) to the next line whose first non-blank character is
  # '['. The file is read as one string, not as a CMake list of lines: in a
  # list an unbalanced square bracket joins the lines up to the next bracket
  # into one element and a semicolon splits a line, so blueprint text such
  # as "a number in (0, 1]" hid the header. Carriage returns are dropped
  # (file(STRINGS) dropped them).
  file(READ "${_blueprint}" _text)
  string(REPLACE "\r" "" _text "${_text}")
  set(_text "\n${_text}\n")
  set(_tadahBlock "")
  string(REGEX MATCH "\n[ \t]*\\[TADAH\\][ \t]*\n" _header "${_text}")
  if(_header)
    # (string(FIND) finds the regex's leftmost match: an earlier copy of the
    # matched text would have been an earlier match.)
    string(FIND "${_text}" "${_header}" _at)
    string(LENGTH "${_header}" _len)
    math(EXPR _start "${_at} + ${_len} - 1")
    # From the newline that ends the header line: every line of the table,
    # each after its newline, up to the next header line.
    string(SUBSTRING "${_text}" ${_start} -1 _rest)
    string(REGEX MATCH "\n[ \t]*\\[" _next "${_rest}")
    if(_next)
      string(FIND "${_rest}" "${_next}" _end)
      string(SUBSTRING "${_rest}" 0 ${_end} _tadahBlock)
    else()
      set(_tadahBlock "${_rest}")
    endif()
  endif()

  # Match either double or single quotes around the version string
  string(REGEX MATCH
    "(^|\n)[ \t]*version[ \t]*=[ \t]*[\"']([^\"']+)[\"']"
    _match "${_tadahBlock}")
  if(NOT _match)
    message(FATAL_ERROR
      "tadah_read_version_from_blueprint: 'version = \"...\"' not found in [TADAH] of ${_blueprint}")
  endif()

  set(_version "${CMAKE_MATCH_2}")

  # CMake project(VERSION) only accepts numeric M.m.p[.t]; strip any
  # SemVer pre-release / build-metadata tail (e.g. 1.3.0-beta.1+abc).
  # CMake regex does not support `{n,m}` quantifiers — use `*`.
  string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _numeric "${_version}")
  if(NOT _numeric)
    message(FATAL_ERROR
      "tadah_read_version_from_blueprint: version '${_version}' in ${_blueprint} "
      "does not start with a numeric M.m.p component")
  endif()

  set(${output_var}        "${_numeric}" PARENT_SCOPE)
  set(${output_var}_FULL   "${_version}" PARENT_SCOPE)
  set(${output_var}_SOURCE "${_blueprint}" PARENT_SCOPE)
endfunction()
