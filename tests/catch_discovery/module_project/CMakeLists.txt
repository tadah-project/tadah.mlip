# Throw-away project driven by ../check_module.cmake: catch_discover_tests()
# from cmake-tools (testing/Catch.cmake), used the way the Tadah modules use
# it, on the fake lister. Discovery runs through a copy of the discovery
# script in the build tree, so the check can change the script.
cmake_minimum_required(VERSION 3.15)
project(tadah_catch_module_case CXX)

include(${TADAH_CMAKE_DIR}/cmake/TadahTesting.cmake)
tadah_setup_tests()
include(Catch)

configure_file("${_CATCH_DISCOVER_TESTS_SCRIPT}"
  "${CMAKE_BINARY_DIR}/CatchAddTests.cmake" COPYONLY)
set(_CATCH_DISCOVER_TESTS_SCRIPT "${CMAKE_BINARY_DIR}/CatchAddTests.cmake")

add_executable(fake_lister "${LISTER_SOURCE}")
# The executor (a list) runs the listing and every registered test. Its last
# element ends in a space, like the other values below.
set_property(TARGET fake_lister PROPERTY CROSSCOMPILING_EMULATOR
  "${CMAKE_COMMAND};-E;env;TADAH_FAKE_LISTER_VIA=emulator ")
# Discovery runs the listing here, so the directory must exist.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/work dir ")

# Every value below would be mangled by `cmake -D` if Catch.cmake passed it
# unquoted: `cmake -D` removes enclosing single quotes and strips trailing
# whitespace. List options have two elements, so a list must survive the
# custom command too.
catch_discover_tests(fake_lister
  TEST_SPEC "'spec'" "[tag] "
  EXTRA_ARGS "'extra'" "arg "
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/work dir "
  TEST_PREFIX "'pfx'"
  TEST_SUFFIX " sfx "
  PROPERTIES LABELS "'label' "
  TEST_LIST fake_lister_list
  REPORTER fake
  OUTPUT_DIR "'out' "
  OUTPUT_PREFIX "'op'"
  OUTPUT_SUFFIX ".xml "
)
