# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#
# Catch2 (v2) test discovery for the Tadah repositories.
#
# catch_discover_tests() (Catch.cmake) runs this script with `cmake -P` after
# every link of a test executable. It lists the executable's test cases with
# --list-test-names-only and writes CTEST_FILE, which registers one CTest
# entry per test case. Guarantees (see also cmake-tools/README.md, "Testing"):
#
#  * Names are read verbatim, one per output line. The output is split with
#    string(FIND)/string(SUBSTRING), never through a CMake list, so ';', '['
#    ']' and '\' in a name can neither merge nor split entries. The double
#    quotes Catch prints around a name that starts with '#' are removed.
#    execute_process() already drops the '\r' of every "\r\n" pair; a '\r'
#    still left at the end of a line (the last line, when no '\n' follows
#    it) is dropped here.
#  * Each entry selects exactly its own case. Its Catch test spec is the name
#    in double quotes, with '\' (first), ',', '"' and '@' escaped by a '\'.
#    Inside the quotes Catch's spec parser takes every other character
#    literally, leading whitespace, '~', '-', '[' and ']' included; the
#    quotes also keep Catch from trimming the spec and its command-line
#    parser from reading it as an option. Catch expands a tag alias
#    ("[@name]") wherever its text occurs in a spec; with every '@' escaped,
#    "[@" cannot occur. A name that starts with "exclude:" gets a space after
#    the opening quote, so it is not read as an exclusion (matching trims it).
#  * A name Catch v2 cannot select exactly is a hard error; rename the test
#    case. That is a leading or trailing '*' (a wildcard, escaped or not), a
#    blank name, or a name that differs from another test case's name only
#    in letter case or surrounding whitespace (Catch compares names
#    case-insensitively after trimming). Hidden test cases count too: a
#    name spec also selects hidden cases, so the names are checked against
#    a second listing of every test case (spec "*").
#  * Every entry passes `--warn NoTests`, so a spec that matches nothing makes
#    Catch exit non-zero instead of reporting success. Catch accepts --warn
#    only once, so an EXTRA_ARGS element that Catch reads as --warn or -w is
#    a hard error.
#  * Each listing is checked against itself. Its exit status is Catch's count
#    of listed cases, which the OS may truncate modulo 256, so it must equal
#    the number of parsed names or that number modulo 256. A crash (a
#    non-integer result), a negative status or a disagreement (for example a
#    name containing a line break, or other output on stdout) is a hard error.
#    "No tests" is decided from the parsed names and is a warning.
#  * Every value is written to CTEST_FILE as a bracket argument whose closing
#    delimiter cannot occur in the value, so names reach CTest verbatim.
#  * Every entry has the SKIP_REGULAR_EXPRESSION of the TADAH-SKIP marker
#    (TadahTestSkip.cmake, next to this script): a test case that calls
#    TADAH_SKIP() (tadah_test_skip.hpp) is reported as Skipped, not Passed.
#    PROPERTIES that set SKIP_REGULAR_EXPRESSION would replace it and are a
#    hard error.
#
# Limitation: a listed line that starts with '"#' and ends with '"' is read as
# a '#' name. A test case whose name really is quoted that way is registered
# without its quotes; its entry then selects nothing and fails (NoTests).
#
# Inputs (set by catch_discover_tests): TEST_EXECUTABLE, TEST_EXECUTOR,
# TEST_WORKING_DIR, TEST_SPEC, TEST_EXTRA_ARGS, TEST_PROPERTIES, TEST_PREFIX,
# TEST_SUFFIX, TEST_LIST, TEST_REPORTER, TEST_OUTPUT_DIR, TEST_OUTPUT_PREFIX,
# TEST_OUTPUT_SUFFIX and CTEST_FILE. TEST_EXECUTOR, TEST_SPEC,
# TEST_EXTRA_ARGS and TEST_PROPERTIES are lists: one element per argument.

# Policies for the script itself (CMP0054: a quoted value is never
# dereferenced as a variable name in if(), whatever a test case is called).
cmake_minimum_required(VERSION 3.15)

# TADAH_TEST_SKIP_MARKER and TADAH_TEST_SKIP_REGULAR_EXPRESSION.
include("${CMAKE_CURRENT_LIST_DIR}/TadahTestSkip.cmake")

set(prefix "${TEST_PREFIX}")
set(suffix "${TEST_SUFFIX}")
set(spec "${TEST_SPEC}")
set(extra_args "${TEST_EXTRA_ARGS}")
set(properties "${TEST_PROPERTIES}")
set(reporter "${TEST_REPORTER}")
set(output_dir "${TEST_OUTPUT_DIR}")
set(output_prefix "${TEST_OUTPUT_PREFIX}")
set(output_suffix "${TEST_OUTPUT_SUFFIX}")

# Append VALUE to the variable named OUT_VAR as one bracket argument. Nothing
# is interpreted inside a bracket argument (no variable references, escapes or
# list splitting). The '=' level grows until "]<level>]" cannot occur before
# the real end, i.e. neither inside VALUE nor across its end.
function(_catch_append_arg out_var value)
  set(eq "")
  while(TRUE)
    string(FIND "${value}]${eq}" "]${eq}]" pos)
    if(pos EQUAL -1)
      break()
    endif()
    string(APPEND eq "=")
  endwhile()
  set(${out_var} "${${out_var}} [${eq}[${value}]${eq}]" PARENT_SCOPE)
endfunction()

# The Catch v2 test spec that selects exactly the test case NAME: the name in
# double quotes (Catch's QuotedName mode). There only '"' ends the name, ','
# separates specs and '\' escapes. '@' is escaped because Catch replaces the
# text of a tag alias ("[@name]") anywhere in a spec before parsing it.
function(_catch_escape_spec out_var name)
  string(REPLACE "\\" "\\\\" quoted "${name}")  # the escape character first
  string(REPLACE "," "\\," quoted "${quoted}")
  string(REPLACE "\"" "\\\"" quoted "${quoted}")
  string(REPLACE "@" "\\@" quoted "${quoted}")
  # Catch strips an "exclude:" prefix (after unescaping, inside quotes too)
  # and negates the spec. A leading space defeats the prefix; matching trims
  # it.
  string(FIND "${name}" "exclude:" pos)
  if(pos EQUAL 0)
    set(quoted " ${quoted}")
  endif()
  set(${out_var} "\"${quoted}\"" PARENT_SCOPE)
endfunction()

# The form Catch compares a name in: trimmed of "\t\n\r " and lower-cased.
function(_catch_match_key out_var name)
  string(REGEX REPLACE "^[\t\r\n ]+" "" key "${name}")
  string(REGEX REPLACE "[\t\r\n ]+$" "" key "${key}")
  string(TOLOWER "${key}" key)
  set(${out_var} "${key}" PARENT_SCOPE)
endfunction()

# TRUE in OUT_VAR when Catch's command-line parser (Clara) reads ARG as the
# option -w/--warn: "-w", "--warn", "-w=<v>", "--warn:<v>", "--warn <v>" (as
# one argument), or a bundle of single-letter options that contains 'w'
# ("-sw").
function(_catch_is_warn_option out_var arg)
  set(is_warn FALSE)
  string(SUBSTRING "${arg}" 0 1 first)
  if(first STREQUAL "-")
    string(LENGTH "${arg}" arg_len)
    string(REGEX MATCH "^[^ :=]*" opt "${arg}")
    string(LENGTH "${opt}" opt_len)
    string(SUBSTRING "${arg}" 1 1 second)
    if(opt_len LESS arg_len)
      # "<option><' ', ':' or '='><value>"
      if(opt STREQUAL "-w" OR opt STREQUAL "--warn")
        set(is_warn TRUE)
      endif()
    elseif(NOT second STREQUAL "-" AND arg_len GREATER 2)
      # "-abc": the options -a, -b and -c
      string(FIND "${arg}" "w" w_pos)
      if(w_pos GREATER 0)
        set(is_warn TRUE)
      endif()
    elseif(arg STREQUAL "-w" OR arg STREQUAL "--warn")
      set(is_warn TRUE)
    endif()
  endif()
  set(${out_var} ${is_warn} PARENT_SCOPE)
endfunction()

# Parse the output of `--list-test-names-only` (OUTPUT, exit status RESULT)
# into <out>_count and <out>_0, <out>_1, ... in the caller's scope. WHAT
# names the listing in error messages.
function(_catch_parse_listing out output result what)
  if(NOT result MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "Error listing ${what} of '${TEST_EXECUTABLE}':\n"
      "  Result: ${result}\n"
      "  Output: ${output}\n"
    )
  endif()
  set(count 0)
  set(rest "${output}")
  while(NOT rest STREQUAL "")
    string(FIND "${rest}" "\n" nl)
    if(nl EQUAL -1)
      set(name "${rest}")
      set(rest "")
    else()
      string(SUBSTRING "${rest}" 0 ${nl} name)
      math(EXPR nl "${nl} + 1")
      string(SUBSTRING "${rest}" ${nl} -1 rest)
    endif()

    string(LENGTH "${name}" len)
    if(len GREATER 0)
      math(EXPR last "${len} - 1")
      string(SUBSTRING "${name}" ${last} 1 tail)
      if(tail STREQUAL "\r")
        string(SUBSTRING "${name}" 0 ${last} name)
        set(len ${last})
      endif()
    endif()
    # Catch prints a name that starts with '#' as "#name".
    if(len GREATER 2)
      string(SUBSTRING "${name}" 0 2 head)
      math(EXPR last "${len} - 1")
      string(SUBSTRING "${name}" ${last} 1 tail)
      if(head STREQUAL "\"#" AND tail STREQUAL "\"")
        math(EXPR inner "${len} - 2")
        string(SUBSTRING "${name}" 1 ${inner} name)
      endif()
    endif()

    set("${out}_${count}" "${name}" PARENT_SCOPE)
    math(EXPR count "${count} + 1")
  endwhile()

  math(EXPR wrapped_count "${count} % 256")
  if(NOT result EQUAL count AND NOT result EQUAL wrapped_count)
    message(FATAL_ERROR
      "Test executable '${TEST_EXECUTABLE}' reported ${result} test cases (its "
      "exit status) when listing ${what}, but listed ${count} names. A test "
      "case name that contains a line break, or other output on stdout, "
      "makes the listing ambiguous.\n"
      "  Output: ${output}\n"
    )
  endif()
  set("${out}_count" ${count} PARENT_SCOPE)
endfunction()

# Run the test executable to list its test cases.
if(NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR
    "Specified test executable '${TEST_EXECUTABLE}' does not exist"
  )
endif()

# Every entry passes --warn NoTests; Catch refuses a second --warn.
foreach(arg IN LISTS extra_args)
  _catch_is_warn_option(is_warn "${arg}")
  if(is_warn)
    message(FATAL_ERROR
      "EXTRA_ARGS of '${TEST_EXECUTABLE}' contain '${arg}', which Catch2 "
      "reads as --warn. Every discovered test already passes --warn NoTests, "
      "and Catch2 accepts --warn only once (a second one makes every test "
      "fail). Remove it from EXTRA_ARGS.\n"
    )
  endif()
endforeach()

# Every entry reports a TADAH_SKIP() case as Skipped; a second
# SKIP_REGULAR_EXPRESSION would replace that one.
foreach(arg IN LISTS properties)
  if(arg STREQUAL "SKIP_REGULAR_EXPRESSION")
    message(FATAL_ERROR
      "PROPERTIES of '${TEST_EXECUTABLE}' set SKIP_REGULAR_EXPRESSION. "
      "Every discovered test already has one: CTest reports a test case "
      "whose output has a line starting with '${TADAH_TEST_SKIP_MARKER}' "
      "as Skipped (TADAH_SKIP() in tadah_test_skip.hpp), and a second value "
      "would replace it. Remove it from PROPERTIES.\n"
    )
  endif()
endforeach()

# The test cases to register (TEST_SPEC, if any, selects them).
execute_process(
  COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" ${spec} --list-test-names-only
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
  WORKING_DIRECTORY "${TEST_WORKING_DIR}"
)
_catch_parse_listing(_catch_name "${output}" "${result}" "the test cases")
set(names_count ${_catch_name_count})

# Every test case, hidden ones included: an entry's name spec selects any of
# them whose name matches.
execute_process(
  COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" "*" --list-test-names-only
  OUTPUT_VARIABLE all_output
  RESULT_VARIABLE all_result
  WORKING_DIRECTORY "${TEST_WORKING_DIR}"
)
_catch_parse_listing(_catch_all "${all_output}" "${all_result}"
  "all test cases (spec \"*\")")
set(index 0)
while(index LESS _catch_all_count)
  _catch_match_key(key "${_catch_all_${index}}")
  string(MD5 key_id "${key}")
  if(DEFINED "_catch_key_${key_id}")
    math(EXPR "_catch_key_${key_id}" "${_catch_key_${key_id}} + 1")
  else()
    set("_catch_key_${key_id}" 1)
  endif()
  math(EXPR index "${index} + 1")
endwhile()

# Each name to register must be selectable on its own.
set(index 0)
while(index LESS names_count)
  set(name "${_catch_name_${index}}")
  math(EXPR line_no "${index} + 1")
  _catch_match_key(key "${name}")
  if(key STREQUAL "")
    message(FATAL_ERROR
      "Test executable '${TEST_EXECUTABLE}' listed a blank test case name "
      "(line ${line_no} of --list-test-names-only). Catch cannot select such "
      "a test case on its own. Give it a name.\n"
    )
  endif()
  string(SUBSTRING "${key}" 0 1 head)
  string(LENGTH "${key}" key_len)
  math(EXPR last "${key_len} - 1")
  string(SUBSTRING "${key}" ${last} 1 tail)
  if(head STREQUAL "*" OR tail STREQUAL "*")
    message(FATAL_ERROR
      "Test case\n  '${name}'\n"
      "of '${TEST_EXECUTABLE}' cannot be selected on its own: Catch2 treats "
      "a leading or trailing '*' in a test spec as a wildcard, escaped or "
      "not. Rename the test case.\n"
    )
  endif()
  string(MD5 key_id "${key}")
  if(NOT DEFINED "_catch_key_${key_id}")
    message(FATAL_ERROR
      "Test case\n  '${name}'\n"
      "of '${TEST_EXECUTABLE}' is missing from the listing of all its test "
      "cases (spec \"*\"), so the executable does not list its test cases "
      "consistently.\n"
    )
  endif()
  if(_catch_key_${key_id} GREATER 1)
    set(twins "")
    set(j 0)
    while(j LESS _catch_all_count)
      _catch_match_key(other "${_catch_all_${j}}")
      if(other STREQUAL key)
        string(APPEND twins "  '${_catch_all_${j}}'\n")
      endif()
      math(EXPR j "${j} + 1")
    endwhile()
    message(FATAL_ERROR
      "Test cases\n${twins}"
      "of '${TEST_EXECUTABLE}' (hidden ones included) differ only in letter "
      "case or surrounding whitespace. Catch compares test names "
      "case-insensitively after trimming, so none of them can be selected on "
      "its own. Rename them.\n"
    )
  endif()
  math(EXPR index "${index} + 1")
endwhile()

if(names_count EQUAL 0)
  message(WARNING
    "Test executable '${TEST_EXECUTABLE}' contains no tests!\n"
  )
endif()

# Check the requested reporter.
if(NOT reporter STREQUAL "")
  execute_process(
    COMMAND ${TEST_EXECUTOR} "${TEST_EXECUTABLE}" --list-reporters
    OUTPUT_VARIABLE reporters_output
    RESULT_VARIABLE reporters_result
    WORKING_DIRECTORY "${TEST_WORKING_DIR}"
  )
  if(NOT reporters_result MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "Error listing the reporters of '${TEST_EXECUTABLE}':\n"
      "  Result: ${reporters_result}\n"
      "  Output: ${reporters_output}\n"
    )
  endif()
  # One line per reporter: two spaces, the name, a colon.
  string(FIND "${reporters_output}" "\n  ${reporter}:" reporter_pos)
  if(reporter_pos EQUAL -1)
    message(FATAL_ERROR
      "\"${reporter}\" is not a valid reporter!\n"
    )
  endif()
endif()

# Prepare the output directory.
if(NOT output_dir STREQUAL "" AND NOT IS_ABSOLUTE "${output_dir}")
  set(output_dir "${TEST_WORKING_DIR}/${output_dir}")
  if(NOT EXISTS "${output_dir}")
    file(MAKE_DIRECTORY "${output_dir}")
  endif()
endif()

# Generate the CTest script.
set(_catch_script "")
set(_catch_list "")
set(index 0)
while(index LESS names_count)
  set(name "${_catch_name_${index}}")
  set(test "${prefix}${name}${suffix}")
  _catch_escape_spec(test_spec "${name}")

  set(_catch_cmd "add_test(")
  _catch_append_arg(_catch_cmd "${test}")
  foreach(arg IN LISTS TEST_EXECUTOR)
    if(NOT arg STREQUAL "")
      _catch_append_arg(_catch_cmd "${arg}")
    endif()
  endforeach()
  _catch_append_arg(_catch_cmd "${TEST_EXECUTABLE}")
  _catch_append_arg(_catch_cmd "${test_spec}")
  _catch_append_arg(_catch_cmd "--warn")
  _catch_append_arg(_catch_cmd "NoTests")
  foreach(arg IN LISTS extra_args)
    if(NOT arg STREQUAL "")
      _catch_append_arg(_catch_cmd "${arg}")
    endif()
  endforeach()
  if(NOT reporter STREQUAL "")
    _catch_append_arg(_catch_cmd "--reporter")
    _catch_append_arg(_catch_cmd "${reporter}")
  endif()
  if(NOT output_dir STREQUAL "")
    # The file is named after the test case.
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" test_name_clean "${name}")
    _catch_append_arg(_catch_cmd "--out")
    _catch_append_arg(_catch_cmd
      "${output_dir}/${output_prefix}${test_name_clean}${output_suffix}")
  endif()
  string(APPEND _catch_script "${_catch_cmd} )\n")

  set(_catch_cmd "set_tests_properties(")
  _catch_append_arg(_catch_cmd "${test}")
  _catch_append_arg(_catch_cmd "PROPERTIES")
  _catch_append_arg(_catch_cmd "WORKING_DIRECTORY")
  _catch_append_arg(_catch_cmd "${TEST_WORKING_DIR}")
  _catch_append_arg(_catch_cmd "SKIP_REGULAR_EXPRESSION")
  _catch_append_arg(_catch_cmd "${TADAH_TEST_SKIP_REGULAR_EXPRESSION}")
  foreach(arg IN LISTS properties)
    _catch_append_arg(_catch_cmd "${arg}")
  endforeach()
  string(APPEND _catch_script "${_catch_cmd} )\n")

  # <TEST_LIST> is a CMake list: a ';' inside a name must be escaped there.
  string(REPLACE ";" "\\;" list_element "${test}")
  _catch_append_arg(_catch_list "${list_element}")

  math(EXPR index "${index} + 1")
endwhile()

# Make the list of discovered tests available to CTest scripts.
if(NOT TEST_LIST STREQUAL "")
  set(_catch_cmd "set(")
  _catch_append_arg(_catch_cmd "${TEST_LIST}")
  string(APPEND _catch_script "${_catch_cmd}${_catch_list} )\n")
endif()

# Write the CTest script.
file(WRITE "${CTEST_FILE}" "${_catch_script}")
