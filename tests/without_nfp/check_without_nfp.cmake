# Check that the tadah executable builds and runs without TADAH_ENABLE_NFP,
# in a build with it (tadah_without_nfp, see CMakeLists.txt here):
#   - `cmake --build <BUILD_DIR> --target tadah_without_nfp` succeeds (its
#     sources compile without TADAH_ENABLE_NFP and link with tadah.mlip,
#     without HPO);
#   - `tadah_without_nfp --version` prints the blueprint version, VERSION;
#   - `tadah_without_nfp --tadah-pc-worker` (the PC_ISOLATION worker entry,
#     which only a build with NFP has) is an unknown argument: a non-zero
#     status and "The following argument was not expected:
#     --tadah-pc-worker".
#
#   cmake -DBUILD_DIR=<build> -DCONFIG=<configuration> -DJOBS=<processes>
#         -DEXE=<tadah_without_nfp> -DVERSION=<version>
#         -P check_without_nfp.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v BUILD_DIR JOBS EXE VERSION)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_without_nfp.cmake: -D${v}=... is required")
  endif()
endforeach()

set(config_args "")
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  set(config_args --config "${CONFIG}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target tadah_without_nfp
          ${config_args} --parallel ${JOBS}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "tadah does not build without TADAH_ENABLE_NFP "
                      "(cmake --build, exit ${rc}):\n${out}")
endif()

set(nfail 0)
macro(fail)
  string(CONCAT what ${ARGV})
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

execute_process(COMMAND "${EXE}" --version
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT rc EQUAL 0 OR NOT out STREQUAL VERSION)
  fail("tadah --version without NFP: exit ${rc}, '${out}' (stderr '${err}'), "
       "expected exit 0 and '${VERSION}'")
endif()

execute_process(COMMAND "${EXE}" --tadah-pc-worker
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
string(FIND "${out}"
  "The following argument was not expected: --tadah-pc-worker" at)
if(rc EQUAL 0 OR at LESS 0)
  fail("tadah --tadah-pc-worker without NFP: exit ${rc}, output '${out}', "
       "expected a non-zero status and 'The following argument was not "
       "expected: --tadah-pc-worker'")
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed")
endif()
message(STATUS "tadah without TADAH_ENABLE_NFP: builds, --version ${VERSION}, "
               "no --tadah-pc-worker")
