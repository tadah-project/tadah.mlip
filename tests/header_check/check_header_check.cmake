# Check of cmake-tools' tadah_add_header_check() (cmake/TadahTesting.cmake),
# run on a copy of the throw-away project in project/ (see its CMakeLists.txt
# for the fixtures). In every case the project configures, `all` builds (the
# header checks are excluded from it) and `ctest -N -L headers` lists exactly
# the expected <target>_headers entries. Then `ctest -L headers` runs them:
#
#   CASE=good         probe_headers and probe_app_headers pass, and every
#                     header of the HEADER_DIRS (both of probe's; probe_app's
#                     include/ by default) has its object file
#   CASE=bad          probe_headers fails; the compiler errors are in exactly
#                     the header that misses an include and the header
#                     without a guard, the two good headers have their object
#                     files (the build keeps going) and the bad ones none
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
#   cmake -DCASE=<case> -DTADAH_CMAKE_DIR=<cmake-tools> -DWORK_DIR=<scratch>
#         -DGENERATOR=<CMake generator> -DMAKE_PROGRAM=<build tool>
#         -DCXX=<C++ compiler> -DCTEST=<ctest> -P check_header_check.cmake
cmake_minimum_required(VERSION 3.18)

foreach(v CASE TADAH_CMAKE_DIR WORK_DIR GENERATOR MAKE_PROGRAM CXX CTEST)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_header_check.cmake: -D${v}=... is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^(good|bad|private_dir|private_entry|new_header)$")
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

# 1. Configure, build `all`, list the entries.
run("configure" "${CMAKE_COMMAND}" --fresh -S "${src}" -B "${build}"
    -G "${GENERATOR}" "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
    "-DCMAKE_CXX_COMPILER=${CXX}" "-DCASE=${CASE}"
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
run("ctest -N -L headers" "${CTEST}" --test-dir "${build}" -N -L headers)
string(REGEX MATCHALL "Test +#[0-9]+: [^\r\n]+" listed "${out}")
list(TRANSFORM listed REPLACE "^Test +#[0-9]+: " "")
list(SORT listed)
expect_equal("entries labelled headers" "${listed}" "${entries}")
stop_if_failed()

# 2. Run the checks.
run("ctest -L headers" "${CTEST}" --test-dir "${build}" -L headers
    --output-on-failure)
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
  run("ctest -L headers after adding a header" "${CTEST}" --test-dir "${build}"
      -L headers --output-on-failure)
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
