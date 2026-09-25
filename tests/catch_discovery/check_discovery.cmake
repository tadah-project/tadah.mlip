# Hermetic regression test for the Catch2 test discovery script that every
# Tadah module uses (cmake-tools testing/CatchAddTests.cmake, wired up by
# tadah_setup_tests() + include(Catch)).
#
#   cmake -DSCRIPT=<CatchAddTests.cmake> -DCANARY=<canary exe>
#         -DLISTER=<fake lister exe> -DWORK_DIR=<scratch dir>
#         -P check_discovery.cmake
#
# Each case runs SCRIPT exactly as catch_discover_tests() does, include()s the
# CTest file it writes with recording stubs for add_test() and
# set_tests_properties(), and checks the recorded arguments:
#
#  * canary (canary.cpp, a real Catch2 binary): every name round-trips
#    exactly, one entry per name, with --warn NoTests and the extra arguments,
#    prefix, suffix and properties; each entry's spec selects exactly its own
#    case (the canary's --list-test-names-only) and the registered command
#    runs exactly one test case; a spec that matches nothing, run with
#    --warn NoTests, fails; a canary name with a hidden twin (same name up to
#    letter case and surrounding whitespace) stops discovery;
#  * EXTRA_ARGS that Catch reads as --warn stop discovery (Catch accepts one
#    --warn, and every entry passes --warn NoTests); similar options pass;
#  * every entry carries the SKIP_REGULAR_EXPRESSION of the TADAH-SKIP
#    marker, and PROPERTIES that set SKIP_REGULAR_EXPRESSION stop discovery
#    (check_skip.cmake runs the marker through a real CTest);
#  * fake lister (fake_lister.cpp): 256 names (exit status wraps to 0), CRLF
#    line ends with a quoted '#' name, a last line ending in a lone '\r', an
#    empty listing; and the listings that must stop discovery: a crash, a
#    count that disagrees with the output, names Catch cannot select on their
#    own, a blank name;
#  * reporter handling and a missing executable.
#
# Every failure is reported; the script fails at the end if there was any.
cmake_minimum_required(VERSION 3.18)  # string(HEX)

foreach(v SCRIPT CANARY LISTER WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_discovery.cmake: -D${v}=... is required")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set_property(GLOBAL PROPERTY CD_NFAIL 0)

# cd_fail(<text>...): the arguments are concatenated exactly.
function(cd_fail)
  set(msg "")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    string(APPEND msg "${ARGV${i}}")
  endforeach()
  get_property(n GLOBAL PROPERTY CD_NFAIL)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CD_NFAIL "${n}")
  message(NOTICE "FAIL: ${msg}")
endfunction()

function(cd_expect_eq what actual expected)
  if(NOT actual STREQUAL expected)
    cd_fail("${what}: got [${actual}], expected [${expected}]")
  endif()
endfunction()

# --- recording stubs --------------------------------------------------------
# ARGV<n> holds each argument exactly (no list splitting), and a single-value
# set_property() stores it verbatim.
function(add_test)
  get_property(n GLOBAL PROPERTY CD_ADD_N)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CD_ADD_N "${n}")
  set_property(GLOBAL PROPERTY "CD_ADD_${n}_ARGC" "${ARGC}")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    set_property(GLOBAL PROPERTY "CD_ADD_${n}_${i}" "${ARGV${i}}")
  endforeach()
endfunction()

function(set_tests_properties)
  get_property(n GLOBAL PROPERTY CD_PROP_N)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CD_PROP_N "${n}")
  set_property(GLOBAL PROPERTY "CD_PROP_${n}_ARGC" "${ARGC}")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    set_property(GLOBAL PROPERTY "CD_PROP_${n}_${i}" "${ARGV${i}}")
  endforeach()
endfunction()

# cd_get(<out> ADD|PROP <record> <arg>): one recorded argument.
function(cd_get out kind record arg)
  get_property(v GLOBAL PROPERTY "CD_${kind}_${record}_${arg}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# --- running the discovery script -------------------------------------------
# Runs SCRIPT the way catch_discover_tests() (Catch.cmake) does, including
# its quoting: each -D value is wrapped in single quotes, which `cmake -D`
# removes, because it would otherwise strip trailing whitespace and one pair
# of enclosing single quotes from the value.
# Inputs in the caller's scope: cd_prefix, cd_suffix, cd_extra_args,
# cd_properties, cd_reporter. Results: cd_rc, cd_log (stdout + stderr) and
# cd_loaded: TRUE when discovery succeeded and the generated CTest file was
# include()d (stubs reset first). The file is parsed in a child process
# first, since a syntax error in an include()d file would abort this script.
macro(cd_discover case exe)
  set(cd_ctest_file "${WORK_DIR}/${case}_tests.cmake")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -D "TEST_TARGET='${case}'"
      -D "TEST_EXECUTABLE='${exe}'"
      -D "TEST_EXECUTOR=''"
      -D "TEST_WORKING_DIR='${WORK_DIR}'"
      -D "TEST_SPEC=''"
      -D "TEST_EXTRA_ARGS='${cd_extra_args}'"
      -D "TEST_PROPERTIES='${cd_properties}'"
      -D "TEST_PREFIX='${cd_prefix}'"
      -D "TEST_SUFFIX='${cd_suffix}'"
      -D "TEST_LIST='${case}_TESTS'"
      -D "TEST_REPORTER='${cd_reporter}'"
      -D "TEST_OUTPUT_DIR=''"
      -D "TEST_OUTPUT_PREFIX=''"
      -D "TEST_OUTPUT_SUFFIX=''"
      -D "CTEST_FILE='${cd_ctest_file}'"
      -P "${SCRIPT}"
    RESULT_VARIABLE cd_rc
    OUTPUT_VARIABLE cd_out
    ERROR_VARIABLE cd_err
  )
  set(cd_log "${cd_out}${cd_err}")
  set_property(GLOBAL PROPERTY CD_ADD_N 0)
  set_property(GLOBAL PROPERTY CD_PROP_N 0)
  unset(${case}_TESTS)
  set(cd_loaded FALSE)
  if(cd_rc EQUAL 0)
    file(WRITE "${WORK_DIR}/${case}_parse.cmake"
      "function(add_test)\nendfunction()\n"
      "function(set_tests_properties)\nendfunction()\n"
      "include([==[${cd_ctest_file}]==])\n")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -P "${WORK_DIR}/${case}_parse.cmake"
      RESULT_VARIABLE cd_parse_rc
      OUTPUT_VARIABLE cd_parse_log
      ERROR_VARIABLE cd_parse_log
    )
    if(cd_parse_rc EQUAL 0)
      include("${cd_ctest_file}")
      set(cd_loaded TRUE)
    else()
      string(APPEND cd_log "the generated CTest file does not parse:\n"
                           "${cd_parse_log}")
    endif()
  endif()
endmacro()

# Fake-lister input: the exact listing bytes and the exit status ("" = the
# number of lines, like Catch).
function(cd_fake_listing case content exit_spec)
  file(WRITE "${WORK_DIR}/${case}.listing" "${content}")
  set(ENV{TADAH_FAKE_LISTER_OUTPUT} "${WORK_DIR}/${case}.listing")
  if(exit_spec STREQUAL "")
    unset(ENV{TADAH_FAKE_LISTER_EXIT})
  else()
    set(ENV{TADAH_FAKE_LISTER_EXIT} "${exit_spec}")
  endif()
endfunction()

# A case where discovery must stop with an error containing PHRASE (a regex;
# CMake may wrap a message at any space, so a space matches "[ \n]+").
function(cd_expect_refusal case phrase)
  string(REPLACE " " "[ \n]+" regex "${phrase}")
  if(cd_rc EQUAL 0)
    cd_fail("${case}: discovery succeeded, expected an error matching '${regex}'")
  elseif(NOT cd_log MATCHES "${regex}")
    cd_fail("${case}: discovery failed without '${regex}':\n${cd_log}")
  endif()
endfunction()

set(cd_prefix "")
set(cd_suffix "")
set(cd_extra_args "")
set(cd_properties "")
set(cd_reporter "")

# Every entry reports a TADAH_SKIP() case as Skipped: the marker line
# "TADAH-SKIP: <reason>" at the start of a line of the output
# (cmake-tools testing/tadah_test_skip.hpp).
set(cd_skip_regex "(^|\n)TADAH-SKIP: ")

# =============================================================================
# 1. The canary: a real Catch2 binary with hostile names.
# =============================================================================
execute_process(COMMAND "${CANARY}" --canary-names-hex
  OUTPUT_VARIABLE hex_out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "'${CANARY} --canary-names-hex' failed: ${rc}")
endif()
string(REGEX MATCHALL "[0-9a-f]+" canary_hex "${hex_out}")
list(LENGTH canary_hex canary_count)
if(canary_count LESS 20)
  message(FATAL_ERROR "the canary lists only ${canary_count} names")
endif()

set(cd_prefix "pfx: ")
set(cd_suffix " :sfx")
set(cd_extra_args "--abort;--order;decl")
set(cd_properties "LABELS;catch_discovery;TIMEOUT;30")
cd_discover(canary "${CANARY}")
if(NOT cd_loaded)
  cd_fail("canary: discovery failed (${cd_rc}):\n${cd_log}")
else()
  get_property(n_add GLOBAL PROPERTY CD_ADD_N)
  get_property(n_prop GLOBAL PROPERTY CD_PROP_N)
  cd_expect_eq("canary: number of add_test() calls" "${n_add}" "${canary_count}")
  cd_expect_eq("canary: number of set_tests_properties() calls"
    "${n_prop}" "${canary_count}")
  if(NOT DEFINED canary_TESTS)
    cd_fail("canary: the generated file does not set canary_TESTS")
  endif()
  string(LENGTH "${cd_prefix}" prefix_len)
  string(LENGTH "${cd_suffix}" suffix_len)
  set(seen_hex "")
  if(n_add GREATER 0)
    foreach(k RANGE 1 ${n_add})
      get_property(argc GLOBAL PROPERTY "CD_ADD_${k}_ARGC")
      cd_get(test ADD ${k} 0)
      cd_get(exe ADD ${k} 1)
      cd_get(spec ADD ${k} 2)
      # <prefix><name><suffix>
      string(LENGTH "${test}" test_len)
      math(EXPR name_len "${test_len} - ${prefix_len} - ${suffix_len}")
      if(name_len LESS 0)
        cd_fail("canary: CTest name too short for prefix and suffix: [${test}]")
        continue()
      endif()
      string(SUBSTRING "${test}" 0 ${prefix_len} got_prefix)
      string(SUBSTRING "${test}" ${prefix_len} ${name_len} name)
      math(EXPR suffix_at "${prefix_len} + ${name_len}")
      string(SUBSTRING "${test}" ${suffix_at} -1 got_suffix)
      cd_expect_eq("canary: prefix of [${test}]" "${got_prefix}" "${cd_prefix}")
      cd_expect_eq("canary: suffix of [${test}]" "${got_suffix}" "${cd_suffix}")
      string(HEX "${name}" name_hex)
      if(NOT name_hex IN_LIST canary_hex)
        cd_fail("canary: registered a name the canary does not have: [${name}]")
      elseif(name_hex IN_LIST seen_hex)
        cd_fail("canary: registered twice: [${name}]")
      endif()
      list(APPEND seen_hex "${name_hex}")

      # add_test(<test> <exe> <spec> --warn NoTests --abort --order decl)
      cd_expect_eq("canary: add_test() argument count for [${name}]" "${argc}" 8)
      cd_expect_eq("canary: executable for [${name}]" "${exe}" "${CANARY}")
      set(i 3)
      foreach(want IN ITEMS --warn NoTests --abort --order decl)
        cd_get(got ADD ${k} ${i})
        cd_expect_eq("canary: argument ${i} for [${name}]" "${got}" "${want}")
        math(EXPR i "${i} + 1")
      endforeach()

      # set_tests_properties(<test> PROPERTIES WORKING_DIRECTORY <dir>
      #                      SKIP_REGULAR_EXPRESSION <TADAH-SKIP marker>
      #                      LABELS catch_discovery TIMEOUT 30)
      get_property(pargc GLOBAL PROPERTY "CD_PROP_${k}_ARGC")
      cd_expect_eq("canary: set_tests_properties() argument count for [${name}]"
        "${pargc}" 10)
      set(i 0)
      foreach(want IN ITEMS "${test}" PROPERTIES WORKING_DIRECTORY
                            "${WORK_DIR}" SKIP_REGULAR_EXPRESSION
                            "${cd_skip_regex}"
                            LABELS catch_discovery TIMEOUT 30)
        cd_get(got PROP ${k} ${i})
        cd_expect_eq("canary: property argument ${i} for [${name}]"
          "${got}" "${want}")
        math(EXPR i "${i} + 1")
      endforeach()

      # The spec selects exactly this case (Catch prints '#' names quoted).
      execute_process(COMMAND "${CANARY}" "${spec}" --list-test-names-only
        OUTPUT_VARIABLE selected RESULT_VARIABLE sel_rc)
      string(SUBSTRING "${name}" 0 1 first)
      if(first STREQUAL "#")
        set(want_listing "\"${name}\"\n")
      else()
        set(want_listing "${name}\n")
      endif()
      if(NOT sel_rc EQUAL 1 OR NOT selected STREQUAL want_listing)
        cd_fail("canary: spec [${spec}] for [${name}] selects "
                "${sel_rc} case(s):\n${selected}")
      endif()

      # The registered command runs exactly one test case, and it passes.
      cd_get(a3 ADD ${k} 3)
      cd_get(a4 ADD ${k} 4)
      cd_get(a5 ADD ${k} 5)
      cd_get(a6 ADD ${k} 6)
      cd_get(a7 ADD ${k} 7)
      execute_process(COMMAND "${exe}" "${spec}" "${a3}" "${a4}" "${a5}"
                              "${a6}" "${a7}"
        OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err RESULT_VARIABLE run_rc)
      if(NOT run_rc EQUAL 0
         OR NOT run_out MATCHES "All tests passed \\(1 assertion in 1 test case\\)")
        cd_fail("canary: the command registered for [${name}] did not run "
                "exactly one passing case (${run_rc}):\n${run_out}${run_err}")
      endif()
    endforeach()
  endif()
  foreach(h IN LISTS canary_hex)
    if(NOT h IN_LIST seen_hex)
      cd_fail("canary: case never registered (hex ${h})")
    endif()
  endforeach()
endif()

# A spec that matches nothing fails once --warn NoTests is given.
execute_process(COMMAND "${CANARY}" "canary: no such test case" --warn NoTests
  OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc)
if(NOT rc MATCHES "^[0-9]+$" OR rc EQUAL 0)
  cd_fail("a spec matching no case exits with [${rc}] under --warn NoTests")
endif()

# A hidden twin: a name spec selects hidden test cases too, so a canary name
# with a hidden twin cannot be selected on its own and stops discovery.
set(hidden_twin "  CANARY: Plain Name\t")
set(ENV{TADAH_CANARY_HIDDEN_CASE} "${hidden_twin}")
execute_process(COMMAND "${CANARY}" --list-test-names-only
  OUTPUT_QUIET RESULT_VARIABLE rc_visible)
execute_process(COMMAND "${CANARY}" "[canary_hidden]" --list-test-names-only
  OUTPUT_VARIABLE hidden_listing RESULT_VARIABLE rc_hidden)
if(NOT rc_visible EQUAL canary_count OR NOT rc_hidden EQUAL 1
   OR NOT hidden_listing STREQUAL "${hidden_twin}\n")
  cd_fail("premise: the hidden twin is registered and hidden (default "
          "listing ${rc_visible} cases, [canary_hidden] ${rc_hidden}: "
          "[${hidden_listing}])")
endif()
set(cd_prefix "")
set(cd_suffix "")
set(cd_extra_args "")
set(cd_properties "")
cd_discover(hidden_twin "${CANARY}")
cd_expect_refusal(hidden_twin "differ only in letter case")
cd_expect_refusal(hidden_twin "'canary: plain name'")
cd_expect_refusal(hidden_twin "'${hidden_twin}'")
unset(ENV{TADAH_CANARY_HIDDEN_CASE})

# EXTRA_ARGS: Catch accepts --warn once, and every entry passes
# --warn NoTests already.
execute_process(COMMAND "${CANARY}" "canary: plain name" --warn NoTests
                        --warn NoAssertions
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(rc EQUAL 0)
  cd_fail("premise: a second --warn is refused by Catch (exit ${rc}):\n"
          "${out}${err}")
endif()
cd_fake_listing(extra_args "alpha\n" "")
set(n 0)
foreach(warn_args IN ITEMS "--warn|NoAssertions" "-w|NoTests"
                           "--warn=NoAssertions" "-w:NoAssertions"
                           "--warn NoAssertions" "-sw|NoAssertions")
  string(REPLACE "|" ";" cd_extra_args "${warn_args}")
  math(EXPR n "${n} + 1")
  cd_discover(warn_extra_${n} "${LISTER}")
  cd_expect_refusal(warn_extra_${n} "reads as --warn")
endforeach()
# Options that only look similar pass through.
set(cd_extra_args "--wait-for-keypress;never;-s;--order;decl")
cd_discover(extra_args "${LISTER}")
if(NOT cd_loaded)
  cd_fail("extra_args: discovery failed (${cd_rc}):\n${cd_log}")
else()
  set(i 3)
  foreach(want IN ITEMS --warn NoTests --wait-for-keypress never -s --order
                        decl)
    cd_get(got ADD 1 ${i})
    cd_expect_eq("extra_args: argument ${i}" "${got}" "${want}")
    math(EXPR i "${i} + 1")
  endforeach()
endif()
set(cd_extra_args "")

# PROPERTIES: every entry already has SKIP_REGULAR_EXPRESSION (the
# TADAH-SKIP marker); a second value would replace it.
set(cd_properties "LABELS;x;SKIP_REGULAR_EXPRESSION;skipped")
cd_fake_listing(skip_property "alpha\n" "")
cd_discover(skip_property "${LISTER}")
cd_expect_refusal(skip_property "set SKIP_REGULAR_EXPRESSION")
cd_expect_refusal(skip_property "TADAH-SKIP")
set(cd_properties "")

# Reporter: passed through when valid, refused when unknown.
set(cd_prefix "")
set(cd_suffix "")
set(cd_extra_args "")
set(cd_properties "")
set(cd_reporter "compact")
cd_discover(reporter "${CANARY}")
if(NOT cd_loaded)
  cd_fail("reporter: discovery with REPORTER compact failed:\n${cd_log}")
else()
  cd_get(a5 ADD 1 5)
  cd_get(a6 ADD 1 6)
  cd_expect_eq("reporter: argument 5" "${a5}" "--reporter")
  cd_expect_eq("reporter: argument 6" "${a6}" "compact")
endif()
set(cd_reporter "no-such-reporter")
cd_discover(bad_reporter "${CANARY}")
cd_expect_refusal(bad_reporter "is not a valid reporter")
set(cd_reporter "")

cd_discover(missing_executable "${WORK_DIR}/no-such-test-executable")
cd_expect_refusal(missing_executable "does not exist")

# =============================================================================
# 2. The fake lister: listings a real binary cannot produce on demand.
# =============================================================================

# 256 names: the listing's exit status wraps to 0; that is not "no tests".
set(content "")
foreach(i RANGE 0 255)
  if(i LESS 10)
    set(num "00${i}")
  elseif(i LESS 100)
    set(num "0${i}")
  else()
    set(num "${i}")
  endif()
  string(APPEND content "fake case ${num}\n")
endforeach()
cd_fake_listing(wrap256 "${content}" "")
execute_process(COMMAND "${LISTER}" --list-test-names-only
  OUTPUT_QUIET RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "premise: 256 names should exit with 0, got ${rc}")
endif()
cd_discover(wrap256 "${LISTER}")
if(NOT cd_loaded)
  cd_fail("wrap256: discovery failed (${cd_rc}):\n${cd_log}")
else()
  if(cd_log MATCHES "contains[ \n]+no[ \n]+tests")
    cd_fail("wrap256: 256 tests reported as none:\n${cd_log}")
  endif()
  get_property(n_add GLOBAL PROPERTY CD_ADD_N)
  cd_expect_eq("wrap256: number of add_test() calls" "${n_add}" 256)
  cd_get(first ADD 1 0)
  cd_get(last ADD 256 0)
  cd_expect_eq("wrap256: first name" "${first}" "fake case 000")
  cd_expect_eq("wrap256: last name" "${last}" "fake case 255")
  list(LENGTH wrap256_TESTS n_list)
  cd_expect_eq("wrap256: length of wrap256_TESTS" "${n_list}" 256)
endif()

# CRLF line ends (execute_process() itself drops the '\r' of each "\r\n");
# a quoted '#' name; ';' and ',' in names.
cd_fake_listing(crlf
  "alpha\r\nbeta ; gamma\r\n\"#delta\"\r\nepsilon, zeta\r\n" "")
cd_discover(crlf "${LISTER}")
if(NOT cd_loaded)
  cd_fail("crlf: discovery failed (${cd_rc}):\n${cd_log}")
else()
  get_property(n_add GLOBAL PROPERTY CD_ADD_N)
  cd_expect_eq("crlf: number of add_test() calls" "${n_add}" 4)
  set(k 1)
  foreach(pair IN ITEMS
      "alpha|\"alpha\"" "beta ; gamma|\"beta ; gamma\"" "#delta|\"#delta\""
      "epsilon, zeta|\"epsilon\\, zeta\"")
    string(FIND "${pair}" "|" bar)
    string(SUBSTRING "${pair}" 0 ${bar} want_name)
    math(EXPR bar "${bar} + 1")
    string(SUBSTRING "${pair}" ${bar} -1 want_spec)
    cd_get(got_name ADD ${k} 0)
    cd_get(got_spec ADD ${k} 2)
    cd_expect_eq("crlf: name ${k}" "${got_name}" "${want_name}")
    cd_expect_eq("crlf: spec ${k}" "${got_spec}" "${want_spec}")
    math(EXPR k "${k} + 1")
  endforeach()
  # <TEST_LIST> keeps a ';' inside a name as one element.
  list(LENGTH crlf_TESTS n_list)
  cd_expect_eq("crlf: length of crlf_TESTS" "${n_list}" 4)
  if(n_list EQUAL 4)
    list(GET crlf_TESTS 1 e1)
    list(GET crlf_TESTS 2 e2)
    cd_expect_eq("crlf: crlf_TESTS element 1" "${e1}" "beta ; gamma")
    cd_expect_eq("crlf: crlf_TESTS element 2" "${e2}" "#delta")
  endif()
endif()

# A last line that ends in a lone '\r' (no '\n' follows, so execute_process()
# keeps it): the discovery script drops it.
cd_fake_listing(cr_eof "alpha\r\nbeta\r" "2")
cd_discover(cr_eof "${LISTER}")
if(NOT cd_loaded)
  cd_fail("cr_eof: discovery failed (${cd_rc}):\n${cd_log}")
else()
  get_property(n_add GLOBAL PROPERTY CD_ADD_N)
  cd_expect_eq("cr_eof: number of add_test() calls" "${n_add}" 2)
  cd_get(got_name ADD 2 0)
  cd_get(got_spec ADD 2 2)
  cd_expect_eq("cr_eof: name 2" "${got_name}" "beta")
  cd_expect_eq("cr_eof: spec 2" "${got_spec}" "\"beta\"")
endif()

# No names: a warning decided from the (empty) listing, no entries.
cd_fake_listing(empty "" "")
cd_discover(empty "${LISTER}")
if(NOT cd_loaded)
  cd_fail("empty: discovery failed (${cd_rc}):\n${cd_log}")
else()
  if(NOT cd_log MATCHES "contains[ \n]+no[ \n]+tests")
    cd_fail("empty: no 'contains no tests' warning:\n${cd_log}")
  endif()
  get_property(n_add GLOBAL PROPERTY CD_ADD_N)
  cd_expect_eq("empty: number of add_test() calls" "${n_add}" 0)
endif()

# Listings that must stop discovery.
cd_fake_listing(crash "alpha\n" "abort")
cd_discover(crash "${LISTER}")
cd_expect_refusal(crash "Error listing the test cases")

cd_fake_listing(count_mismatch "alpha\nbeta\n" "5")
cd_discover(count_mismatch "${LISTER}")
cd_expect_refusal(count_mismatch "reported 5 test cases")

cd_fake_listing(trailing_star "alpha\nNLopt GN_*\n" "")
cd_discover(trailing_star "${LISTER}")
cd_expect_refusal(trailing_star "Test case 'NLopt GN_\\*'")
cd_expect_refusal(trailing_star "leading or trailing '\\*' in a test spec as a wildcard")

cd_fake_listing(leading_star "  *wildcard first\n" "")
cd_discover(leading_star "${LISTER}")
cd_expect_refusal(leading_star "Test case '  \\*wildcard first'")

cd_fake_listing(case_collision "Alpha beta\nalpha BETA \n" "")
cd_discover(case_collision "${LISTER}")
cd_expect_refusal(case_collision "differ only in letter case")

cd_fake_listing(blank_name "alpha\n \t \nbeta\n" "")
cd_discover(blank_name "${LISTER}")
cd_expect_refusal(blank_name "blank test case name")

unset(ENV{TADAH_FAKE_LISTER_OUTPUT})
unset(ENV{TADAH_FAKE_LISTER_EXIT})

get_property(n_fail GLOBAL PROPERTY CD_NFAIL)
if(n_fail GREATER 0)
  message(FATAL_ERROR "Catch discovery check: ${n_fail} failure(s) for ${SCRIPT}")
endif()
message(STATUS "Catch discovery check passed: ${canary_count} canary names, "
               "256/CRLF/lone-CR/empty listings, 9 refused listings, "
               "6 refused --warn EXTRA_ARGS, a refused SKIP_REGULAR_EXPRESSION "
               "property (${SCRIPT})")
