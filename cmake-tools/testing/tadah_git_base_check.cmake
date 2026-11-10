# Check of a repository's git base address (tadah_add_git_base_check,
# cmake/TadahTesting.cmake).
#
#   cmake -DTADAH_CMAKELISTS=<CMakeLists.txt> -P tadah_git_base_check.cmake
#
# A Tadah! repository's CMakeLists.txt fetches cmake-tools and the other
# Tadah! repositories with FetchContent. It must:
#   block    carry the TADAH_GIT_BASE block of tadah_git_base_block.cmake
#            (next to this script) exactly, between its '# >>>' and '# <<<'
#            lines: the block has to come before cmake-tools is fetched, so
#            every repository holds a copy and this check keeps the copies
#            equal;
#   urls     name every repository it fetches as
#            GIT_REPOSITORY ${TADAH_GIT_BASE}/<repository>.git, with at
#            least one such line (commented-out lines included).
# The block itself, run in a script with the environment and -D variables
# of each case, sets TADAH_GIT_BASE to:
#   nothing set                            https://github.com/tadah-project
#   the environment variable               its value
#   the environment variable, empty        https://github.com/tadah-project
#   -DTADAH_GIT_BASE and the environment   the -D value
#   a value ending in '/'                  the value without it
# Prints each failure and fails, or "every check of <file> passed".
cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED TADAH_CMAKELISTS OR "${TADAH_CMAKELISTS}" STREQUAL "")
  message(FATAL_ERROR "tadah_git_base_check.cmake: -DTADAH_CMAKELISTS=<file> is required")
endif()
if(NOT EXISTS "${TADAH_CMAKELISTS}")
  message(FATAL_ERROR "tadah_git_base_check.cmake: ${TADAH_CMAKELISTS} does not exist")
endif()
set(_tgb_ref_file "${CMAKE_CURRENT_LIST_DIR}/tadah_git_base_block.cmake")
file(READ "${_tgb_ref_file}" _tgb_ref)
file(READ "${TADAH_CMAKELISTS}" _tgb_text)

set(_tgb_nfail 0)
# A function, not a macro: a macro would expand the ${...} in a message.
function(_tgb_fail)
  list(JOIN ARGV "" what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR n "${_tgb_nfail} + 1")
  set(_tgb_nfail ${n} PARENT_SCOPE)
endfunction()

# --- block ---------------------------------------------------------------
set(_tgb_begin "# >>> TADAH_GIT_BASE")
set(_tgb_end "# <<< TADAH_GIT_BASE\n")
string(FIND "${_tgb_text}" "${_tgb_begin}" _tgb_b)
string(FIND "${_tgb_text}" "${_tgb_end}" _tgb_e)
if(_tgb_b LESS 0 OR _tgb_e LESS _tgb_b)
  _tgb_fail("block: no '${_tgb_begin}' ... '# <<< TADAH_GIT_BASE' lines: copy "
            "${_tgb_ref_file} before the first FetchContent_Declare")
else()
  string(LENGTH "${_tgb_end}" _tgb_n)
  math(EXPR _tgb_len "${_tgb_e} + ${_tgb_n} - ${_tgb_b}")
  string(SUBSTRING "${_tgb_text}" ${_tgb_b} ${_tgb_len} _tgb_block)
  if(NOT _tgb_block STREQUAL _tgb_ref)
    _tgb_fail("block: differs from ${_tgb_ref_file} (copy that file over it)")
  endif()
  string(FIND "${_tgb_text}" "FetchContent_Declare" _tgb_decl)
  if(_tgb_decl GREATER_EQUAL 0 AND _tgb_decl LESS _tgb_b)
    _tgb_fail("block: comes after the first FetchContent_Declare")
  endif()
endif()

# --- urls ----------------------------------------------------------------
file(STRINGS "${TADAH_CMAKELISTS}" _tgb_lines REGEX "GIT_REPOSITORY")
set(_tgb_nurl 0)
foreach(_tgb_line IN LISTS _tgb_lines)
  # Third-party code (Eigen, toml11, ...) is fetched from its own host.
  if(_tgb_line MATCHES "GIT_REPOSITORY[ \t]+\\$\\{TADAH_GIT_BASE\\}/[A-Za-z0-9._-]+\\.git[ \t]*$")
    math(EXPR _tgb_nurl "${_tgb_nurl} + 1")
  elseif(_tgb_line MATCHES "[Tt]adah")
    string(STRIP "${_tgb_line}" _tgb_line)
    _tgb_fail("urls: '${_tgb_line}' does not use \${TADAH_GIT_BASE}/<repository>.git")
  endif()
endforeach()
if(_tgb_nurl EQUAL 0)
  _tgb_fail("urls: no 'GIT_REPOSITORY \${TADAH_GIT_BASE}/<repository>.git' line")
endif()

# --- the block's value ---------------------------------------------------
string(RANDOM LENGTH 8 _tgb_tag)
set(_tgb_probe "${CMAKE_CURRENT_BINARY_DIR}/tadah_git_base_probe_${_tgb_tag}.cmake")
file(WRITE "${_tgb_probe}" "${_tgb_ref}message(NOTICE \"BASE=[\${TADAH_GIT_BASE}]\")\n")
set(_tgb_default "https://github.com/tadah-project")
# case: environment (or "-" for unset) | -D value (or "-") | expected
set(_tgb_cases
  "-|-|${_tgb_default}"
  "https://host.example/tadah|-|https://host.example/tadah"
  "|-|${_tgb_default}"
  "https://host.example/tadah|https://other.example/t|https://other.example/t"
  "https://host.example/tadah//|-|https://host.example/tadah")
foreach(_tgb_case IN LISTS _tgb_cases)
  string(REPLACE "|" ";" _tgb_f "${_tgb_case}")
  list(GET _tgb_f 0 _tgb_env)
  list(GET _tgb_f 1 _tgb_d)
  list(GET _tgb_f 2 _tgb_want)
  if(_tgb_env STREQUAL "-")
    set(_tgb_envarg "--unset=TADAH_GIT_BASE")
  else()
    set(_tgb_envarg "TADAH_GIT_BASE=${_tgb_env}")
  endif()
  set(_tgb_darg "")
  if(NOT _tgb_d STREQUAL "-")
    set(_tgb_darg "-DTADAH_GIT_BASE=${_tgb_d}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "${_tgb_envarg}"
                          "${CMAKE_COMMAND}" ${_tgb_darg} -P "${_tgb_probe}"
                  RESULT_VARIABLE _tgb_r OUTPUT_VARIABLE _tgb_o
                  ERROR_VARIABLE _tgb_err)
  if(NOT _tgb_r EQUAL 0 OR NOT "${_tgb_o}${_tgb_err}" MATCHES "BASE=\\[([^]]*)\\]")
    _tgb_fail("value (environment '${_tgb_env}', -D '${_tgb_d}'): the block "
              "did not run:\n${_tgb_o}${_tgb_err}")
  elseif(NOT CMAKE_MATCH_1 STREQUAL _tgb_want)
    _tgb_fail("value (environment '${_tgb_env}', -D '${_tgb_d}'): "
              "'${CMAKE_MATCH_1}', expected '${_tgb_want}'")
  endif()
endforeach()
file(REMOVE "${_tgb_probe}")

if(_tgb_nfail GREATER 0)
  message(FATAL_ERROR "${_tgb_nfail} checks of ${TADAH_CMAKELISTS} failed")
endif()
message(NOTICE "every check of ${TADAH_CMAKELISTS} passed")
