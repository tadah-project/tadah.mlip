# End-to-end check of the TADAH-SKIP marker of cmake-tools
# (testing/tadah_test_skip.hpp, testing/TadahTestSkip.cmake): a test case
# that calls TADAH_SKIP() is reported by CTest as Skipped, not Passed, even
# though it exits with 0.
#
#   cmake -DSCRIPT=<CatchAddTests.cmake> -DSKIP_CANARY=<skip canary exe>
#         -DCTEST=<ctest> -DWORK_DIR=<scratch dir> -P check_skip.cmake
#
# The skip canary (skip_canary.cpp) is discovered by SCRIPT exactly as
# catch_discover_tests() does it, into a scratch directory whose
# CTestTestfile.cmake includes the generated file, and a real CTest runs it:
#
#  * the TADAH_SKIP cases are Skipped (and listed under "did not run"), in
#    the translation unit of the other cases and in a second one;
#  * an ordinary pass is Passed;
#  * a failure whose assertion quotes the marker, and a failure that prints
#    the marker text inside a line, stay Failed;
#  * a test case that asks to be skipped and fails stays Failed: a check
#    that failed before TADAH_SKIP, a SECTION that failed while another
#    SECTION skipped, and a check that failed after TADAH_SKIP returned from
#    a helper;
#  * the same holds with REPORTER junit, whose reporter captures the test
#    case's std::cout/std::cerr;
#  * run directly, the TADAH_SKIP case exits with 0, writes the marker line
#    with its reason at the start of a line, once, and returns before the
#    rest of the test case; a case that asked to be skipped and failed
#    exits with 1 and writes no marker line.
#
# Every failure is reported; the script fails at the end if there was any.
# 3.16: the marker is CTest's SKIP_REGULAR_EXPRESSION test property.
cmake_minimum_required(VERSION 3.16)

foreach(v SCRIPT SKIP_CANARY CTEST WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_skip.cmake: -D${v}=... is required")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set_property(GLOBAL PROPERTY CS_NFAIL 0)

function(cs_fail)
  set(msg "")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    string(APPEND msg "${ARGV${i}}")
  endforeach()
  get_property(n GLOBAL PROPERTY CS_NFAIL)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CS_NFAIL "${n}")
  message(NOTICE "FAIL: ${msg}")
endfunction()

set(skip_case "skip canary: optional data absent")
set(skip_reason "optional data absent (42 files missing)")
set(skip_case_more
    "skip canary: optional data absent in another translation unit")
set(n_failing 5)

# Discover the skip canary into DIR the way catch_discover_tests() does
# (Catch.cmake's quoting included) and make DIR a CTest directory.
function(cs_discover dir reporter)
  file(MAKE_DIRECTORY "${dir}")
  set(tests_file "${dir}/skip_canary_tests.cmake")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -D "TEST_TARGET='skip_canary'"
      -D "TEST_EXECUTABLE='${SKIP_CANARY}'"
      -D "TEST_EXECUTOR=''"
      -D "TEST_WORKING_DIR='${dir}'"
      -D "TEST_SPEC=''"
      -D "TEST_EXTRA_ARGS=''"
      -D "TEST_PROPERTIES=''"
      -D "TEST_PREFIX=''"
      -D "TEST_SUFFIX=''"
      -D "TEST_LIST='skip_canary_TESTS'"
      -D "TEST_REPORTER='${reporter}'"
      -D "TEST_OUTPUT_DIR=''"
      -D "TEST_OUTPUT_PREFIX=''"
      -D "TEST_OUTPUT_SUFFIX=''"
      -D "CTEST_FILE='${tests_file}'"
      -P "${SCRIPT}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out
  )
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "discovering the skip canary (reporter "
                        "[${reporter}]) failed (${rc}):\n${out}")
  endif()
  file(WRITE "${dir}/CTestTestfile.cmake" "include([==[${tests_file}]==])\n")
endfunction()

# Run CTest in DIR and check the status CTest reports for every case.
function(cs_expect_statuses dir what)
  execute_process(
    COMMAND "${CTEST}"
    WORKING_DIRECTORY "${dir}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out
  )
  foreach(pair IN ITEMS
      "${skip_case}|\\*\\*\\*Skipped"
      "${skip_case_more}|\\*\\*\\*Skipped"
      "skip canary: passes|Passed"
      "skip canary: fails quoting the marker|\\*\\*\\*Failed"
      "skip canary: fails after printing the marker mid-line|\\*\\*\\*Failed"
      "skip canary: fails a check and then skips|\\*\\*\\*Failed"
      "skip canary: one section fails, another skips|\\*\\*\\*Failed"
      "skip canary: skips in a helper and then fails|\\*\\*\\*Failed")
    string(FIND "${pair}" "|" bar)
    string(SUBSTRING "${pair}" 0 ${bar} name)
    math(EXPR bar "${bar} + 1")
    string(SUBSTRING "${pair}" ${bar} -1 status)
    if(NOT out MATCHES "Test +#[0-9]+: ${name} \\.* *${status} ")
      cs_fail("${what}: CTest did not report [${name}] as [${status}]:\n"
              "${out}")
    endif()
  endforeach()
  foreach(name IN ITEMS "${skip_case}" "${skip_case_more}")
    if(NOT out MATCHES "did not run:\n([^\n]*\n)*[^\n]*${name} \\(Skipped\\)")
      cs_fail("${what}: [${name}] is not listed as Skipped under "
              "\"did not run\":\n${out}")
    endif()
  endforeach()
  if(NOT out MATCHES "([0-9]+) tests failed out of")
    cs_fail("${what}: CTest reports no failed tests although ${n_failing} "
            "cases fail:\n${out}")
  elseif(NOT CMAKE_MATCH_1 EQUAL n_failing)
    cs_fail("${what}: CTest reports ${CMAKE_MATCH_1} failed tests, expected "
            "${n_failing}:\n${out}")
  endif()
  if(rc EQUAL 0)
    cs_fail("${what}: CTest exited with 0 although ${n_failing} cases "
            "fail:\n${out}")
  endif()
endfunction()

# 1. Default (console) reporter.
cs_discover("${WORK_DIR}/console" "")
cs_expect_statuses("${WORK_DIR}/console" "console reporter")

# 2. A reporter that captures the test case's std::cout and std::cerr.
cs_discover("${WORK_DIR}/junit" "junit")
cs_expect_statuses("${WORK_DIR}/junit" "junit reporter")

# 3. The TADAH_SKIP case run directly.
execute_process(
  COMMAND "${SKIP_CANARY}" "\"${skip_case}\"" --warn NoTests
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  cs_fail("run directly, [${skip_case}] exits with ${rc}, expected 0:\n"
          "${out}${err}")
endif()
# The marker line, its reason verbatim, at the start of a line, once.
string(FIND "\n${err}" "\nTADAH-SKIP: ${skip_reason}\n" marker_at)
if(marker_at EQUAL -1)
  cs_fail("run directly, [${skip_case}] does not write the marker line "
          "'TADAH-SKIP: ${skip_reason}' at the start of a line on "
          "stderr:\n[${err}]")
endif()
string(REGEX MATCHALL "TADAH-SKIP: " markers "${out}${err}")
list(LENGTH markers n_markers)
if(NOT n_markers EQUAL 1)
  cs_fail("run directly, [${skip_case}] writes the marker ${n_markers} "
          "times, expected once:\n[${out}${err}]")
endif()
if(NOT out MATCHES "skipped: optional data absent \\(42 files missing\\)")
  cs_fail("run directly, [${skip_case}] does not report its reason as a "
          "Catch warning:\n${out}")
endif()
if(out MATCHES "TADAH_SKIP returned" OR err MATCHES "TADAH_SKIP returned")
  cs_fail("TADAH_SKIP did not return from the test case:\n${out}${err}")
endif()

# 4. A case that asked to be skipped and failed, run directly: it fails and
#    writes no marker line (whatever the SKIP_REGULAR_EXPRESSION of an entry).
execute_process(
  COMMAND "${SKIP_CANARY}" "\"skip canary: fails a check and then skips\""
          --warn NoTests
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(rc EQUAL 0)
  cs_fail("run directly, [skip canary: fails a check and then skips] exits "
          "with 0:\n${out}${err}")
endif()
if("${out}${err}" MATCHES "TADAH-SKIP: ")
  cs_fail("run directly, [skip canary: fails a check and then skips] writes "
          "the marker:\n[${out}${err}]")
endif()

get_property(n_fail GLOBAL PROPERTY CS_NFAIL)
if(n_fail GREATER 0)
  message(FATAL_ERROR "TADAH-SKIP marker check: ${n_fail} failure(s) for "
                      "${SCRIPT}")
endif()
message(STATUS "TADAH-SKIP marker check passed (${SCRIPT})")
