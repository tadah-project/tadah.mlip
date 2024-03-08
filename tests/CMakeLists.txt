
file(GLOB tests_src "*.cpp")
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

add_executable(test_tadah ${tests_src})
target_link_libraries(test_tadah PRIVATE 
    Catch2 tadah config::rc)
find_package(LAPACK)
target_link_libraries (test_tadah PRIVATE ${LAPACK_LIBRARIES})

include(Catch.cmake)
catch_discover_tests(test_tadah)
