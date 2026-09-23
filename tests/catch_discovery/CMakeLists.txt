# Regression tests for the Catch2 test discovery in cmake-tools
# (testing/Catch.cmake and testing/CatchAddTests.cmake), which every Tadah
# module uses through tadah_setup_tests() + include(Catch). They need no
# network and no Tadah library.
#
# NB: MLIP redefines add_test() globally and forwards its arguments unquoted
# (${ARGV}), so an argument of an explicit add_test() must not contain ';'
# and must not be empty.
tadah_setup_tests()
include(Catch)

# canary.cpp: a Catch2 binary whose test case names are hostile to CMake,
# CTest and Catch's test-spec parser. fake_lister.cpp: plain C++ standing in
# for a test binary, with listings Catch cannot be made to produce.
add_executable(tadah_catch_canary canary.cpp)
target_link_libraries(tadah_catch_canary PRIVATE Catch2)
add_executable(tadah_catch_fake_lister fake_lister.cpp)

# The discovery script every catch_discover_tests() in this build runs,
# against the canary and the fake lister (see check_discovery.cmake).
add_test(NAME tadah_catch_discovery_script
  COMMAND ${CMAKE_COMMAND}
    -DSCRIPT=${_CATCH_DISCOVER_TESTS_SCRIPT}
    -DCANARY=$<TARGET_FILE:tadah_catch_canary>
    -DLISTER=$<TARGET_FILE:tadah_catch_fake_lister>
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/script_check
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_discovery.cmake)

# catch_discover_tests() end to end on a throw-away project, with the
# cmake-tools whose tadah_setup_tests() this build uses: every option arrives
# exactly, re-discovery when the script changes (see check_module.cmake).
add_test(NAME tadah_catch_discovery_module
  COMMAND ${CMAKE_COMMAND}
    -DTADAH_CMAKE_DIR=${_TADAH_CMAKE_TOOLS_DIR}
    -DLISTER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fake_lister.cpp
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/module_check
    "-DGENERATOR=${CMAKE_GENERATOR}"
    -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DCXX=${CMAKE_CXX_COMPILER}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_module.cmake)

# The canary registered like any Tadah suite, so CTest itself runs every
# canary name through the spec generated for it. The prefix ends in a space
# on purpose (it must reach the discovery script intact).
catch_discover_tests(tadah_catch_canary TEST_PREFIX "tadah_catch_canary: ")
