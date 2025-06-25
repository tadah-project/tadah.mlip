
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
# if(TADAH_ENABLE_HPO)
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

if(TADAH_ENABLE_HPO)
  target_link_libraries(test_tadah PRIVATE tadah.hpo tadah.lammps)
endif()

include(Catch.cmake)                  # helper from Catch2
catch_discover_tests(test_tadah)

