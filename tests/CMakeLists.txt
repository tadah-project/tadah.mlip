Include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.7
    )

FetchContent_MakeAvailable(Catch2)

file(GLOB tests_src "*.cpp")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

add_executable(run_tests ${tests_src})
target_link_libraries(run_tests PRIVATE Catch2::Catch2 tadah)
find_package(LAPACK)
target_link_libraries (run_tests PRIVATE ${LAPACK_LIBRARIES})

include(Catch.cmake)
catch_discover_tests(run_tests)
