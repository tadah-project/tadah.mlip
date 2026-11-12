# Checks that the headers of the vendored ASA compile in a C++ translation
# unit under -pedantic-errors, with each of the two configuration headers
# the tests use:
#   - featured: <asa/tadah_asa_hook.h> (tadah_asa_config.h, then
#     asa_usr_asa.h), as test_asa.cpp includes it but for its extern "C"
#     and OpenMP;
#   - pristine: <asa/tadah_asa_pristine_config.h>, then <asa/asa_usr_asa.h>,
#     inside extern "C", as test_asa_golden_pristine_tu.cpp does.
# A macro a configuration header defines and asa_usr_asa.h defines again
# with other tokens is an error there. The headers are found with -I, so
# the compiler does not treat them as system headers.
#
#   cmake -DCXX=<C++ compiler (GCC or Clang)> -DEXTERNAL=<HPO/external>
#         -DWORK_DIR=<scratch> -P check_asa_headers.cmake
cmake_minimum_required(VERSION 3.16)

foreach(v CXX EXTERNAL WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_asa_headers.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(WRITE "${WORK_DIR}/featured.cpp" "#include <asa/tadah_asa_hook.h>\nint main() { return 0; }\n")
file(WRITE "${WORK_DIR}/pristine.cpp" [=[
extern "C" {
#include <asa/tadah_asa_pristine_config.h>
#include <asa/asa_usr_asa.h>
}
int main() { return 0; }
]=])

set(failed "")
foreach(unit featured pristine)
  execute_process(
    COMMAND "${CXX}" -std=c++17 -fsyntax-only -Wall -Wpedantic -pedantic-errors
            "-I${EXTERNAL}" "${WORK_DIR}/${unit}.cpp"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(NOTICE "FAIL: ${unit}.cpp does not compile with -pedantic-errors:\n${out}")
    list(APPEND failed ${unit})
  endif()
endforeach()
if(failed)
  message(FATAL_ERROR "the ASA headers do not compile with -pedantic-errors: ${failed}")
endif()
message(STATUS "the ASA headers compile with -pedantic-errors, featured and pristine")
