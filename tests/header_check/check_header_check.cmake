# Check of cmake-tools' tadah_add_header_check() (cmake/TadahTesting.cmake),
# run on a copy of the throw-away project in project/ (see its CMakeLists.txt
# for the fixtures). In every case the project configures, `all` builds (the
# header checks are excluded from it) and `ctest -N -L headers` lists exactly
# the expected <target>_headers entries. Then `ctest -L headers` runs them:
#
#   CASE=good         probe_headers and probe_app_headers pass, and every
#                     header of the HEADER_DIRS (both of probe's; probe_app's
#                     include/ by default) has its object file; probe_headers
#                     builds on as many processes as the machine has physical
#                     cores (its --parallel and PROCESSORS)
#   CASE=bad          configured with TADAH_HEADER_CHECK_JOBS=1, so that
#                     probe_headers builds on one process (its --parallel and
#                     PROCESSORS are 1); it fails, the compiler errors are in
#                     exactly the header that misses an include and the
#                     header without a guard, the two good headers have their
#                     object files and the bad ones none: on one process the
#                     good headers, which sort after the bad ones, are
#                     compiled only because the build keeps going
#   CASE=private_dir  probe_headers fails on exactly the header that needs
#                     probe's PRIVATE include directory: the headers compile
#                     with what a consumer of the library gets
#   CASE=private_entry
#                     as private_dir, with that PRIVATE directory a second
#                     HEADER_DIRS entry: its header compiles, and the header
#                     of include/ that needs it still fails on exactly that
#                     include: a HEADER_DIRS entry is on the include path of
#                     its own headers only
#   CASE=new_header   as good; then a header without a guard is added to
#                     probe's include/ and, with no other step, the next
#                     `ctest -L headers` fails on exactly that header
#
# CASE=refusals instead configures the project once per refused call of the
# function (REFUSAL, see project/refusals/CMakeLists.txt; a value of
# TADAH_HEADER_CHECK_JOBS that is not a number of processes) and checks that
# the configure step fails with that call's message.
#
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGENERATOR=<CMake generator> -DMAKE_PROGRAM=<build tool>
#         -DCXX=<C++ compiler> -DCTEST=<ctest> -P check_header_check.cmake
#
# The check needs CMake 3.19: it reads ctest's test list
# (--show-only=json-v1, ctest 3.14) with string(JSON). The umbrella build
# accepts CMake 3.15 (a build without TADAH_ENABLE_NFP; HPO needs 3.24), so
# on an older CMake the script says so on one line,
#   SKIPPED: check_header_check.cmake needs CMake 3.19 or later (string(JSON)); this is CMake <version>
# which the entries' SKIP_REGULAR_EXPRESSION reports as Skipped, and stops.
# -DAS_CMAKE_VERSION=<v> makes that decision as CMake <v> would, for
# check_cmake_minimum.cmake (a running CMake cannot be made older).
cmake_minimum_required(VERSION 3.15)
if(DEFINED AS_CMAKE_VERSION)
  set(_thc_cmake_version "${AS_CMAKE_VERSION}")
else()
  set(_thc_cmake_version "${CMAKE_VERSION}")
endif()
if(_thc_cmake_version VERSION_LESS 3.19)
  message(NOTICE "SKIPPED: check_header_check.cmake needs CMake 3.19 or "
                 "later (string(JSON)); this is CMake ${_thc_cmake_version}")
  return()
endif()
cmake_minimum_required(VERSION 3.19)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR GENERATOR MAKE_PROGRAM CXX CTEST)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_header_check.cmake: -D${v}=... is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^(good|bad|private_dir|private_entry|new_header|refusals)$")
  message(FATAL_ERROR "check_header_check.cmake: unknown CASE '${CASE}'")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/project/" DESTINATION "${WORK_DIR}/src")
set(src "${WORK_DIR}/src")
set(build "${WORK_DIR}/build")

set(nfail 0)
macro(fail what)
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()
macro(stop_if_failed)
  if(nfail GREATER 0)
    message(FATAL_ERROR "${nfail} check(s) failed for CASE=${CASE}")
  endif()
endmacro()

function(run label)
  execute_process(COMMAND ${ARGN}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  message(STATUS "---- ${label} (exit ${rc}) ----\n${out}")
  set(rc "${rc}" PARENT_SCOPE)
  set(out "${out}" PARENT_SCOPE)
endfunction()

# ctest with its arguments, in the probe's build directory (run()'s rc, out).
function(run_ctest label)
  execute_process(COMMAND "${CTEST}" ${ARGN} WORKING_DIRECTORY "${build}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  message(STATUS "---- ${label} (exit ${rc}) ----\n${out}")
  set(rc "${rc}" PARENT_SCOPE)
  set(out "${out}" PARENT_SCOPE)
endfunction()

# The headers (file names) in which the compiler reported an error.
function(error_headers out_var text)
  string(REGEX MATCHALL "[^\r\n :]+\\.(h|hpp):[0-9]+:[0-9]+: (fatal )?error:"
         hits "${text}")
  set(names "")
  foreach(h IN LISTS hits)
    string(REGEX REPLACE ":[0-9]+:[0-9]+: (fatal )?error:$" "" h "${h}")
    get_filename_component(h "${h}" NAME)
    list(APPEND names "${h}")
  endforeach()
  list(REMOVE_DUPLICATES names)
  list(SORT names)
  set(${out_var} "${names}" PARENT_SCOPE)
endfunction()

# The headers (paths relative to their HEADER_DIRS entry) of <check> whose
# object file exists, and those whose object file is missing.
function(objects check built_var missing_var)
  file(STRINGS "${build}/objects_${check}.txt" objs)
  set(built "")
  set(missing "")
  foreach(o IN LISTS objs)
    string(REGEX MATCH "${check}_header_check/(.+)\\.cpp\\.[^./]+$" m "${o}")
    if(NOT m)
      message(FATAL_ERROR "unexpected object file name ${o}")
    endif()
    if(EXISTS "${o}")
      list(APPEND built "${CMAKE_MATCH_1}")
    else()
      list(APPEND missing "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  list(SORT built)
  list(SORT missing)
  set(${built_var} "${built}" PARENT_SCOPE)
  set(${missing_var} "${missing}" PARENT_SCOPE)
endfunction()

macro(expect_equal what got expected)
  if(NOT "${got}" STREQUAL "${expected}")
    fail("${what}: '${got}', expected '${expected}'")
  endif()
endmacro()

# The value after --parallel in the command of the CTest entry <name>, and
# its PROCESSORS property, from `ctest --show-only=json-v1` (which leaves
# PROCESSORS out when it is CTest's default, 1).
function(entry_jobs name parallel_var processors_var)
  execute_process(COMMAND "${CTEST}" --show-only=json-v1
    WORKING_DIRECTORY "${build}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE json ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "ctest --show-only=json-v1 failed (exit ${rc}): ${err}")
  endif()
  set(parallel "")
  set(processors "")
  string(JSON ntests LENGTH "${json}" tests)
  math(EXPR last "${ntests} - 1")
  foreach(i RANGE ${last})
    string(JSON test_name GET "${json}" tests ${i} name)
    if(NOT test_name STREQUAL name)
      continue()
    endif()
    string(JSON nargs LENGTH "${json}" tests ${i} command)
    math(EXPR last_arg "${nargs} - 2")
    foreach(a RANGE ${last_arg})
      string(JSON arg GET "${json}" tests ${i} command ${a})
      if(arg STREQUAL "--parallel")
        math(EXPR v "${a} + 1")
        string(JSON parallel GET "${json}" tests ${i} command ${v})
      endif()
    endforeach()
    set(processors 1)
    string(JSON nprops LENGTH "${json}" tests ${i} properties)
    math(EXPR last_prop "${nprops} - 1")
    foreach(p RANGE ${last_prop})
      string(JSON prop GET "${json}" tests ${i} properties ${p} name)
      if(prop STREQUAL "PROCESSORS")
        string(JSON processors GET "${json}" tests ${i} properties ${p} value)
      endif()
    endforeach()
  endforeach()
  set(${parallel_var} "${parallel}" PARENT_SCOPE)
  set(${processors_var} "${processors}" PARENT_SCOPE)
endfunction()

# 0. refusals: each refused call stops the configure step with its message
#    (compared with the blanks and line breaks of CMake's layout collapsed).
if(CASE STREQUAL "refusals")
  set(fixture "${src}/refusals")
  set(refusals unexpected_argument not_a_target alias imported no_header_dirs
               not_a_directory no_headers same_relative_path
               jobs_zero jobs_text jobs_empty)
  set(message_unexpected_argument
    "tadah_add_header_check(probe): unexpected arguments: EXTRA")
  set(message_not_a_target
    "tadah_add_header_check: 'no_such_target' is not a target")
  set(message_alias
    "tadah_add_header_check: 'probe::probe' is an imported or ALIAS target; name the target that is built here")
  set(message_imported
    "tadah_add_header_check: 'imported_probe' is an imported or ALIAS target; name the target that is built here")
  set(message_no_header_dirs
    "tadah_add_header_check(probe): HEADER_DIRS names no directory")
  set(message_not_a_directory
    "tadah_add_header_check(probe): HEADER_DIRS entry ${fixture}/no_such_directory is not a directory")
  set(message_no_headers
    "tadah_add_header_check(probe): no *.h or *.hpp file in ${fixture}/no_headers")
  set(message_same_relative_path
    "tadah_add_header_check(probe): probe/x.h is in more than one of the HEADER_DIRS, so #include <probe/x.h> is ambiguous")
  foreach(j zero:0 text:two empty:)
    string(REGEX REPLACE ":.*" "" name "${j}")
    string(REGEX REPLACE "^[^:]*:" "" value "${j}")
    set(args_jobs_${name} "-DTADAH_HEADER_CHECK_JOBS=${value}")
    set(message_jobs_${name}
      "tadah_add_header_check(probe): TADAH_HEADER_CHECK_JOBS must be a whole number of processes, 1 or more, got '${value}'")
  endforeach()
  foreach(r IN LISTS refusals)
    run("configure REFUSAL=${r}" "${CMAKE_COMMAND}" -S "${src}"
        -B "${WORK_DIR}/build_${r}" -G "${GENERATOR}"
        "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}" "-DCMAKE_CXX_COMPILER=${CXX}"
        "-DCASE=${CASE}" "-DREFUSAL=${r}" ${args_${r}}
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${TADAH_CMAKE_DIR}")
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
    string(FIND "${flat}" "${message_${r}}" at)
    if(rc EQUAL 0)
      fail("REFUSAL=${r}: the configure step succeeded")
    elseif(at LESS 0)
      fail("REFUSAL=${r}: the configure step failed without '${message_${r}}'")
    endif()
  endforeach()
  stop_if_failed()
  message(STATUS "CASE=${CASE}: ok")
  return()
endif()

# 1. Configure, build `all`, list the entries. (Every build directory is new:
#    WORK_DIR was removed above.)
set(case_args "")
if(CASE STREQUAL "bad")
  set(case_args -DTADAH_HEADER_CHECK_JOBS=1)
endif()
run("configure" "${CMAKE_COMMAND}" -S "${src}" -B "${build}"
    -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
    "-DCMAKE_CXX_COMPILER=${CXX}" "-DCASE=${CASE}" ${case_args}
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON
    "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${TADAH_CMAKE_DIR}")
if(NOT rc EQUAL 0)
  fail("configure failed (exit ${rc})")
endif()
stop_if_failed()

run("build all" "${CMAKE_COMMAND}" --build "${build}")
if(NOT rc EQUAL 0)
  fail("`all` did not build (exit ${rc}): a header check is part of it")
endif()

if(CASE STREQUAL "good" OR CASE STREQUAL "new_header")
  set(entries "probe_app_headers;probe_headers")
else()
  set(entries "probe_headers")
endif()
run_ctest("ctest -N -L headers" -N -L headers)
string(REGEX MATCHALL "Test +#[0-9]+: [^\r\n]+" listed "${out}")
list(TRANSFORM listed REPLACE "^Test +#[0-9]+: " "")
list(SORT listed)
expect_equal("entries labelled headers" "${listed}" "${entries}")
stop_if_failed()

if(CASE STREQUAL "bad")
  set(jobs 1)
else()
  cmake_host_system_information(RESULT jobs QUERY NUMBER_OF_PHYSICAL_CORES)
  if(NOT jobs GREATER 0)
    set(jobs 1)
  endif()
endif()
entry_jobs(probe_headers parallel processors)
expect_equal("probe_headers --parallel" "${parallel}" "${jobs}")
expect_equal("probe_headers PROCESSORS" "${processors}" "${jobs}")
stop_if_failed()

# 2. Run the checks.
run_ctest("ctest -L headers" -L headers --output-on-failure)
error_headers(errors "${out}")
objects(probe probe_built probe_missing)

if(CASE STREQUAL "good" OR CASE STREQUAL "new_header")
  if(NOT rc EQUAL 0)
    fail("ctest -L headers failed (exit ${rc})")
  endif()
  expect_equal("headers with errors" "${errors}" "")
  expect_equal("probe headers compiled" "${probe_built}"
    "probe/needs_dependency.h;probe/needs_directory_definition.h;probe/uses_widget.h;probe/widget.h;probe_generated.h")
  expect_equal("probe headers not compiled" "${probe_missing}" "")
  objects(probe_app app_built app_missing)
  expect_equal("probe_app headers compiled" "${app_built}" "app/app.h")
  expect_equal("probe_app headers not compiled" "${app_missing}" "")
elseif(CASE STREQUAL "bad")
  if(rc EQUAL 0)
    fail("ctest -L headers passed")
  endif()
  expect_equal("headers with errors" "${errors}" "missing_include.h;no_guard.hpp")
  expect_equal("probe headers compiled" "${probe_built}"
    "probe/uses_widget.h;probe/widget.h")
  expect_equal("probe headers not compiled" "${probe_missing}"
    "probe/detail/no_guard.hpp;probe/missing_include.h")
elseif(CASE STREQUAL "private_dir" OR CASE STREQUAL "private_entry")
  if(rc EQUAL 0)
    fail("ctest -L headers passed")
  endif()
  expect_equal("headers with errors" "${errors}" "needs_private.h")
  if(CASE STREQUAL "private_dir")
    expect_equal("probe headers compiled" "${probe_built}" "probe/widget.h")
  else()
    expect_equal("probe headers compiled" "${probe_built}"
      "probe/widget.h;probe_private.h")
  endif()
  expect_equal("probe headers not compiled" "${probe_missing}"
    "probe/needs_private.h")
endif()
stop_if_failed()

# 3. new_header: a header added after the first run is checked by the next
#    run, without re-running CMake by hand or registering it.
if(CASE STREQUAL "new_header")
  file(WRITE "${src}/good/include/probe/added_no_guard.h"
    "// Added after the first run, without an include guard.\n"
    "namespace probe {\nstruct Added {\n  int n;\n};\n}  // namespace probe\n")
  run_ctest("ctest -L headers after adding a header" -L headers
            --output-on-failure)
  if(rc EQUAL 0)
    fail("ctest -L headers passed with the added header")
  endif()
  error_headers(errors "${out}")
  expect_equal("headers with errors" "${errors}" "added_no_guard.h")
  objects(probe probe_built probe_missing)
  expect_equal("probe headers not compiled" "${probe_missing}"
    "probe/added_no_guard.h")
  stop_if_failed()
endif()

message(STATUS "CASE=${CASE}: ok")
