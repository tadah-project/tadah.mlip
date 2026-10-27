# Check that the tests of the tadah executable build and run without
# TADAH_ENABLE_NFP, in a build with it (test_tadah_without_nfp, see
# CMakeLists.txt here):
#   - `cmake --build <BUILD_DIR> --target test_tadah_without_nfp` succeeds
#     (test_tadah's sources compile without TADAH_ENABLE_NFP and link with
#     tadah.mlip, without HPO and LAMMPS: a build with -DBUILD_TESTING=ON and
#     the default TADAH_ENABLE_NFP OFF builds them so);
#   - `test_tadah_without_nfp "[not-built]"` (in WORK_DIR) exits 0 and runs
#     the cases of a tadah built without NFP: "All tests passed" and
#     "in 2 test cases" (tadah hpo refused naming TADAH_ENABLE_NFP; tadah -h,
#     tadah hpo -h and tadah explain hpo say so).
#
#   cmake -DBUILD_DIR=<build> -DCONFIG=<configuration> -DJOBS=<processes>
#         -DEXE=<test_tadah_without_nfp> -DWORK_DIR=<scratch>
#         -P check_tests_without_nfp.cmake
cmake_minimum_required(VERSION 3.15)

foreach(v BUILD_DIR JOBS EXE WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_tests_without_nfp.cmake: -D${v}=... is required")
  endif()
endforeach()

set(config_args "")
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  set(config_args --config "${CONFIG}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}"
          --target test_tadah_without_nfp ${config_args} --parallel ${JOBS}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the tests of tadah do not build without "
                      "TADAH_ENABLE_NFP (cmake --build, exit ${rc}):\n${out}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(COMMAND "${EXE}" "[not-built]"
  WORKING_DIRECTORY "${WORK_DIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
string(FIND "${out}" "All tests passed" passed)
string(FIND "${out}" "in 2 test cases" cases)
if(NOT rc EQUAL 0 OR passed LESS 0 OR cases LESS 0)
  message(FATAL_ERROR "test_tadah_without_nfp [not-built]: exit ${rc}, "
                      "expected exit 0, 'All tests passed' and 'in 2 test "
                      "cases':\n${out}")
endif()
message(STATUS "the tests of tadah without TADAH_ENABLE_NFP: build, and "
               "their [not-built] cases pass")
