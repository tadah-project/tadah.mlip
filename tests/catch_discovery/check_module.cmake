# End-to-end check of catch_discover_tests() from cmake-tools
# (testing/Catch.cmake), on the throw-away project in module_project/:
#
#  * every option reaches the discovery script exactly, through the real
#    Catch.cmake: TEST_SPEC, EXTRA_ARGS, WORKING_DIRECTORY, TEST_PREFIX,
#    TEST_SUFFIX, PROPERTIES, TEST_LIST, REPORTER, OUTPUT_DIR/PREFIX/SUFFIX
#    and the target's CROSSCOMPILING_EMULATOR. The values carry enclosing
#    single quotes or trailing whitespace, which `cmake -D` would drop
#    without Catch.cmake's quoting, and the list options have two elements.
#    The listings (the fake lister records its arguments) and the recorded
#    add_test() / set_tests_properties() arguments are compared exactly;
#  * changing the discovery script relinks the test executable and so re-runs
#    discovery (LINK_DEPENDS); a build without a change re-runs nothing.
#
#   cmake -DTADAH_CMAKE_DIR=<cmake-tools> -DLISTER_SOURCE=<fake_lister.cpp>
#         -DWORK_DIR=<scratch dir> -DGENERATOR=<CMake generator>
#         -DMAKE_PROGRAM=<build tool> -DCXX=<C++ compiler>
#         -P check_module.cmake
cmake_minimum_required(VERSION 3.18)

foreach(v TADAH_CMAKE_DIR LISTER_SOURCE WORK_DIR GENERATOR MAKE_PROGRAM CXX)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_module.cmake: -D${v}=... is required")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(build "${WORK_DIR}/build")
set(listing "${WORK_DIR}/listing.txt")
set(args_file "${WORK_DIR}/lister_args.txt")
set(ENV{TADAH_FAKE_LISTER_OUTPUT} "${listing}")
set(ENV{TADAH_FAKE_LISTER_ARGS} "${args_file}")
unset(ENV{TADAH_FAKE_LISTER_EXIT})
unset(ENV{TADAH_FAKE_LISTER_VIA})
set_property(GLOBAL PROPERTY CM_NFAIL 0)

function(run what)
  execute_process(COMMAND ${ARGN}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${what} failed (${rc}):\n${out}")
  endif()
endfunction()

# fail(<text>...): the arguments are concatenated exactly.
function(fail)
  set(msg "")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    string(APPEND msg "${ARGV${i}}")
  endforeach()
  get_property(n GLOBAL PROPERTY CM_NFAIL)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CM_NFAIL "${n}")
  message(NOTICE "FAIL: ${msg}")
endfunction()

# Recording stubs: ARGV<n> holds each argument exactly.
function(add_test)
  get_property(n GLOBAL PROPERTY CM_ADD_N)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY CM_ADD_N "${n}")
  set_property(GLOBAL PROPERTY CM_ADD_ARGC "${ARGC}")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    set_property(GLOBAL PROPERTY "CM_ADD_${i}" "${ARGV${i}}")
  endforeach()
endfunction()
function(set_tests_properties)
  set_property(GLOBAL PROPERTY CM_PROP_ARGC "${ARGC}")
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 0 ${last})
    set_property(GLOBAL PROPERTY "CM_PROP_${i}" "${ARGV${i}}")
  endforeach()
endfunction()

# Load the CTest file the last discovery generated for the fake lister.
function(load_generated)
  file(GLOB files "${build}/fake_lister_tests-*.cmake")
  list(LENGTH files n_files)
  if(NOT n_files EQUAL 1)
    message(FATAL_ERROR "expected one generated tests file, found: ${files}")
  endif()
  set_property(GLOBAL PROPERTY CM_ADD_N 0)
  unset(fake_lister_list)
  include("${files}")
  set(fake_lister_list "${fake_lister_list}" PARENT_SCOPE)
endfunction()

# The listings discovery ran (the fake lister's argument record): the
# requested one with TEST_SPEC, then the one of all test cases, both through
# the executor.
set(want_listings
  "'spec'\n[tag] \n--list-test-names-only\nemulator \n\n*\n--list-test-names-only\nemulator \n\n")

# Check that one test, <prefix><name><suffix>, was registered with every
# option intact, and that the listings ran as expected.
macro(expect_registered step name)
  set(_test "'pfx'${name} sfx ")
  string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _clean "${name}")
  load_generated()
  get_property(_n GLOBAL PROPERTY CM_ADD_N)
  if(NOT _n EQUAL 1)
    fail("${step}: ${_n} add_test() calls, expected 1")
  else()
    get_property(_argc GLOBAL PROPERTY CM_ADD_ARGC)
    get_property(_exe GLOBAL PROPERTY CM_ADD_5)
    get_filename_component(_exe_name "${_exe}" NAME_WE)
    if(NOT _exe_name STREQUAL "fake_lister" OR NOT EXISTS "${_exe}")
      fail("${step}: add_test() argument 5 is not the fake lister: [${_exe}]")
    endif()
    set(_i 0)
    foreach(_want IN ITEMS "${_test}" "${CMAKE_COMMAND}" -E env
                           "TADAH_FAKE_LISTER_VIA=emulator " "${_exe}"
                           "\"${name}\"" --warn NoTests "'extra'" "arg "
                           --reporter fake --out
                           "${build}/work dir /'out' /'op'${_clean}.xml ")
      get_property(_got GLOBAL PROPERTY "CM_ADD_${_i}")
      if(NOT _got STREQUAL _want)
        fail("${step}: add_test() argument ${_i}: got [${_got}], "
             "expected [${_want}]")
      endif()
      math(EXPR _i "${_i} + 1")
    endforeach()
    if(NOT _argc EQUAL _i)
      fail("${step}: add_test() has ${_argc} arguments, expected ${_i}")
    endif()

    get_property(_argc GLOBAL PROPERTY CM_PROP_ARGC)
    set(_i 0)
    foreach(_want IN ITEMS "${_test}" PROPERTIES WORKING_DIRECTORY
                           "${build}/work dir " LABELS "'label' ")
      get_property(_got GLOBAL PROPERTY "CM_PROP_${_i}")
      if(NOT _got STREQUAL _want)
        fail("${step}: set_tests_properties() argument ${_i}: got [${_got}], "
             "expected [${_want}]")
      endif()
      math(EXPR _i "${_i} + 1")
    endforeach()
    if(NOT _argc EQUAL _i)
      fail("${step}: set_tests_properties() has ${_argc} arguments, "
           "expected ${_i}")
    endif()

    if(NOT fake_lister_list STREQUAL _test)
      fail("${step}: TEST_LIST fake_lister_list is [${fake_lister_list}], "
           "expected [${_test}]")
    endif()
  endif()
  if(NOT EXISTS "${build}/work dir /'out' ")
    fail("${step}: the output directory [${build}/work dir /'out' ] "
         "was not created")
  endif()
  set(_got "")
  if(EXISTS "${args_file}")
    file(READ "${args_file}" _got)
  endif()
  if(NOT _got STREQUAL want_listings)
    fail("${step}: the listings ran with [${_got}], expected "
         "[${want_listings}]")
  endif()
  file(REMOVE "${args_file}")
endmacro()

# 1. Configure and build: every option arrives intact.
file(WRITE "${listing}" "first case\n")
run(configure "${CMAKE_COMMAND}"
  -S "${CMAKE_CURRENT_LIST_DIR}/module_project" -B "${build}"
  -G "${GENERATOR}"
  "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
  "-DCMAKE_CXX_COMPILER=${CXX}"
  "-DTADAH_CMAKE_DIR=${TADAH_CMAKE_DIR}"
  "-DLISTER_SOURCE=${LISTER_SOURCE}")
run(build "${CMAKE_COMMAND}" --build "${build}")
expect_registered("first build" "first case")

# 2. Nothing changed: the executable is not relinked, discovery does not run
#    (the fake lister would now list another name and record its arguments).
file(WRITE "${listing}" "second case\n")
run(build "${CMAKE_COMMAND}" --build "${build}")
if(EXISTS "${args_file}")
  file(READ "${args_file}" got)
  fail("rebuild without changes: discovery ran again: [${got}]")
  file(REMOVE "${args_file}")
endif()
load_generated()
get_property(n GLOBAL PROPERTY CM_ADD_N)
get_property(name GLOBAL PROPERTY CM_ADD_0)
if(NOT n EQUAL 1 OR NOT name STREQUAL "'pfx'first case sfx ")
  fail("rebuild without changes: registered ${n} test(s), last [${name}], "
       "expected one: ['pfx'first case sfx ]")
endif()

# 3. The discovery script changed: relink, re-discover.
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.1)
file(APPEND "${build}/CatchAddTests.cmake" "\n# changed by check_module.cmake\n")
run(build "${CMAKE_COMMAND}" --build "${build}")
expect_registered("rebuild after the script changed" "second case")

get_property(failures GLOBAL PROPERTY CM_NFAIL)
if(failures GREATER 0)
  message(FATAL_ERROR "catch_discover_tests() check: ${failures} failure(s) "
                      "for ${TADAH_CMAKE_DIR}")
endif()
message(STATUS "catch_discover_tests() check passed (${TADAH_CMAKE_DIR})")
