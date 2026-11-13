cmake_minimum_required(VERSION 3.15)
project(tadah_cmake LANGUAGES CXX)

include(cmake/TadahCompileOptions.cmake)
include(cmake/TadahDevelop.cmake)
include(cmake/TadahTesting.cmake)
include(cmake/TadahVersion.cmake)

# install only the *real* target, ALIAS targets are never installable
install(TARGETS tadah_compile_options EXPORT tadah_cmakeTargets)

install(EXPORT tadah_cmakeTargets
        NAMESPACE tadah::
        DESTINATION lib/cmake/tadah_cmake)
