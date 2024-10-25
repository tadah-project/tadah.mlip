
file(GLOB tests_src "*.cpp")
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

add_executable(test_tadah ${tests_src})
target_link_libraries(test_tadah PRIVATE Catch2 tadah.mlip)
target_include_directories(test_tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/include)
target_include_directories(test_tadah PRIVATE ${Tadah.CORE_SOURCE_DIR}/external/toml11)
target_include_directories(test_tadah PRIVATE ${Tadah.MODELS_SOURCE_DIR}/include)
target_include_directories(test_tadah PRIVATE ${Tadah.MLIP_SOURCE_DIR}/include)

include(Catch.cmake)
catch_discover_tests(test_tadah)
