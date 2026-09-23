
# file(GLOB tests_src "*.cpp")
# #file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)
#
# add_executable(test_tadah ${tests_src} "../src/cli_task.cpp" "../src/tadah_cli.cpp")
# target_include_directories(test_tadah PRIVATE ../include)
# target_link_libraries(test_tadah PRIVATE Catch2 tadah.mlip)
# target_link_libraries(test_tadah PRIVATE Catch2 tadah.core)
# target_include_directories(test_tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/include)
# target_include_directories(test_tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/external/toml11)
# target_include_directories(test_tadah PRIVATE ${Tadah.MODELS_SOURCE_DIR}/include)
# target_include_directories(test_tadah PRIVATE ${Tadah.MLIP_SOURCE_DIR}/include)
# if(TADAH_ENABLE_NFP)
#   target_link_libraries(test_tadah PUBLIC tadah.hpo)
#   target_link_libraries(test_tadah PUBLIC tadah.lammps)
# endif()
#
# include(Catch.cmake)
# catch_discover_tests(test_tadah)


# ------------------------------------------------------------------
# 1. Sources
# ------------------------------------------------------------------
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
    Catch2                            # FetchContent imported target
    tadah.core
    tadah.models
    tadah.mlip)
target_include_directories(test_tadah PRIVATE ../include)

if(TADAH_ENABLE_NFP)
  target_link_libraries(test_tadah PRIVATE tadah.hpo tadah.lammps)
endif()

include(Catch.cmake)                  # helper from Catch2
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
