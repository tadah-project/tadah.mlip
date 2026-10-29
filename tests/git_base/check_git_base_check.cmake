# Check of cmake-tools' testing/tadah_git_base_check.cmake on throw-away
# CMakeLists.txt files written under WORK_DIR.
#
#   cmake -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -P check_git_base_check.cmake
#
#   good        the block, then GIT_REPOSITORY ${TADAH_GIT_BASE}/x.git, a
#               commented one and a third-party address: it passes
#   changed     the block with its default address changed: fails, "differs"
#   late        the block after the first FetchContent_Declare: fails,
#               "comes after"
#   no block    fails, "no '# >>> TADAH_GIT_BASE'"
#   fixed url   a Tadah! repository named by a fixed address: fails, naming
#               the line
#   no url      no GIT_REPOSITORY line: fails, "no 'GIT_REPOSITORY"
#   refusals    no TADAH_CMAKELISTS, and a file that does not exist
cmake_minimum_required(VERSION 3.16)

foreach(v TADAH_CMAKE_DIR WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_git_base_check.cmake: -D${v}=... is required")
  endif()
endforeach()
set(script "${TADAH_CMAKE_DIR}/testing/tadah_git_base_check.cmake")
file(READ "${TADAH_CMAKE_DIR}/testing/tadah_git_base_block.cmake" block)
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(nfail 0)
# A function, not a macro: a macro would expand the ${...} in an output.
function(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR n "${nfail} + 1")
  set(nfail ${n} PARENT_SCOPE)
endfunction()

set(decl "FetchContent_Declare(x\n  GIT_REPOSITORY \${TADAH_GIT_BASE}/x.git\n  GIT_TAG v1-dev)\n")
string(CONCAT more "#  GIT_REPOSITORY \${TADAH_GIT_BASE}/y.git\n"
       "  GIT_REPOSITORY https://third.example.org/eigen/eigen.git\n")
string(REPLACE "https://github.com/tadah-project" "https://elsewhere.example"
       changed "${block}")
string(CONCAT fixed "FetchContent_Declare(y\n  GIT_REPOSITORY "
       "https://host.example/tadah/core.git\n)\n")

# name | file text | expected output (regex) | expected to pass
set(names good changed late "no block" "fixed url" "no url")
set(texts "include(FetchContent)\n${block}${decl}${more}"
          "${changed}${decl}"
          "${decl}${block}"
          "${decl}"
          "${block}${decl}${fixed}"
          "${block}")
set(wants "every check of [^\n]* passed"
          "block: differs from"
          "block: comes after the first FetchContent_Declare"
          "block: no '# >>> TADAH_GIT_BASE'"
          "urls: 'GIT_REPOSITORY https://host[.]example/tadah/core[.]git' does not use"
          "urls: no 'GIT_REPOSITORY")
foreach(i RANGE 5)
  list(GET names ${i} name)
  list(GET wants ${i} want)
  # The texts hold ';' nowhere, so the list keeps each one whole.
  list(GET texts ${i} text)
  set(f "${WORK_DIR}/case${i}/CMakeLists.txt")
  file(WRITE "${f}" "${text}")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DTADAH_CMAKELISTS=${f}"
                          -P "${script}"
                  WORKING_DIRECTORY "${WORK_DIR}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
  set(out "${o}${e}")
  string(REGEX REPLACE "\n *" " " joined "${out}")
  string(REGEX REPLACE "\n" " " want_j "${want}")
  if(name STREQUAL "good")
    if(NOT r EQUAL 0 OR NOT out MATCHES "${want}")
      fail("${name}: expected a pass:\n${out}")
    endif()
  elseif(r EQUAL 0 OR NOT joined MATCHES "${want_j}")
    fail("${name}: expected a failure matching '${want}':\n${out}")
  endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -P "${script}"
                RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(r EQUAL 0 OR NOT "${o}${e}" MATCHES "-DTADAH_CMAKELISTS=<file> is required")
  fail("refusal without TADAH_CMAKELISTS:\n${o}${e}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}"
                        "-DTADAH_CMAKELISTS=${WORK_DIR}/none/CMakeLists.txt"
                        -P "${script}"
                RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
string(REGEX REPLACE "\n *" " " joined "${o}${e}")
if(r EQUAL 0 OR NOT joined MATCHES "none/CMakeLists.txt does not exist")
  fail("refusal of a missing file:\n${o}${e}")
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} checks of tadah_git_base_check failed")
endif()
message(NOTICE "every check of tadah_git_base_check passed")
