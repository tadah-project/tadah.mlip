# ------------------------------------------------------------------
# 1. Sources
# ------------------------------------------------------------------
# Catch2 and its test discovery come from cmake-tools (tadah_setup_tests()).
tadah_setup_tests()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(test_tadah
  ${TEST_SOURCES}
  "${PROJECT_SOURCE_DIR}/src/cli_task.cpp"
  "${PROJECT_SOURCE_DIR}/src/tadah_cli.cpp")

target_include_directories(test_tadah
  PRIVATE
    ${PROJECT_SOURCE_DIR}/tests
  PRIVATE ${PROJECT_SOURCE_DIR}/external/CLI11/include   # only if tests <CLI/CLI.hpp>
)
target_link_libraries(test_tadah
  PRIVATE
    tadah::compile_options            # warnings / sanitiser / -pg
    Catch2                            # from tadah_setup_tests()
    tadah.core
    tadah.models
    tadah.mlip)
target_include_directories(test_tadah PRIVATE ../include)

if(TADAH_ENABLE_NFP)
  target_link_libraries(test_tadah PRIVATE tadah.hpo tadah.lammps)
endif()

include(Catch)
catch_discover_tests(test_tadah)


# ------------------------------------------------------------------
# 2. Version single source of truth (CORE blueprint TOML)
# ------------------------------------------------------------------
# The binary reports the blueprint version it was built from.
string(REPLACE "." "\\." _tadah_version_re "${TADAH_VERSION_FULL}")
add_test(NAME tadah_version_matches_blueprint COMMAND tadah --version)
set_tests_properties(tadah_version_matches_blueprint PROPERTIES
  PASS_REGULAR_EXPRESSION "^${_tadah_version_re}[\r\n]*$")

# cmake-tools' reader: found through FetchContent'd CORE, [TADAH]-scoped,
# registered as a configure dependency; a missing blueprint or version
# line is a configure error (no 0.0.0 default).
FetchContent_GetProperties(tadah_cmake)
set(_vr ${CMAKE_CURRENT_SOURCE_DIR}/version_reader)
function(tadah_version_reader_test name case core regex)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} --fresh
      -S ${_vr}/project -B ${CMAKE_CURRENT_BINARY_DIR}/version_reader/${name}
      -DCASE=${case}
      -DTADAH_CMAKE_DIR=${tadah_cmake_SOURCE_DIR}
      -DFETCHCONTENT_FULLY_DISCONNECTED=ON
      "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${core}")
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${regex}")
endfunction()
tadah_version_reader_test(tadah_version_reader_fetchcontent fetchcontent
  ${_vr}/core_ok "RESULT=9\\.8\\.7\\|9\\.8\\.7-rc\\.1\n.*DEPENDS=[^\n]*core_ok/blueprint/blueprint_tadah\\.toml")
tadah_version_reader_test(tadah_version_reader_no_version fetchcontent
  ${_vr}/core_noversion "not found in \\[TADAH\\][ \n]+of[ \n]+[^\n]*core_noversion")
tadah_version_reader_test(tadah_version_reader_no_core no_core
  "" "could not find blueprint_tadah\\.toml\\.[ \n]+Tadah\\.CORE has not been made available")


# ------------------------------------------------------------------
# 3. Catch test discovery (cmake-tools), used by every Tadah module
# ------------------------------------------------------------------
add_subdirectory(catch_discovery)


# ------------------------------------------------------------------
# 4. Documentation build tools (docs/tools/tests)
# ------------------------------------------------------------------
# Unit tests of the docs input lookup (docs/source/config/source_locator.py)
# and of the hpo.html redirect check (docs/tools/check_redirect.py). They
# never use the network.
#
# Some cases need node, sphinx-build or the Python modules sphinx,
# sphinx_rtd_theme or yaml, and skip themselves when one is missing.
# unittest still exits 0 then, ending with "OK (skipped=N)";
# that run is reported as Skipped, not Passed, so a pass means every case
# ran. A failing run ends with "FAILED (...)" and stays a failure. "Ran 0
# tests" is a failure too (before Python 3.12 unittest exits 0 on it).
# No bytecode is written: the tests import modules from the source tree.
# NB: add_test() arguments must not contain ';' or be empty (MLIP's
# add_test() override; see catch_discovery/CMakeLists.txt).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME tadah_docs_tools
  COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${PROJECT_SOURCE_DIR}/docs/tools/tests -v)
set_tests_properties(tadah_docs_tools PROPERTIES
  LABELS docs
  TIMEOUT 120
  ENVIRONMENT PYTHONDONTWRITEBYTECODE=1
  SKIP_REGULAR_EXPRESSION "\nOK \\(skipped=[0-9]+"
  FAIL_REGULAR_EXPRESSION "\nRan 0 tests ")
