# Throw-away project driven by ../check_header_check.cmake. It brings in
# cmake-tools the way every Tadah module does (FetchContent, source directory
# overridden to the cmake-tools this build uses) and calls
# tadah_add_header_check() from tests/, as a module's tests/CMakeLists.txt
# does. CASE picks the fixture directory (new_header uses good/, and its
# driver adds a header after the first run):
#
#   good         the library probe (HEADER_DIRS include generated): headers
#                that need a header of their own library, of a library probe
#                links PUBLIC (dep), or a definition of the project directory;
#                the executable probe_app (app/, its include/ by default): a
#                header that needs its PRIVATE include directory and
#                definition, and a header of probe
#   bad          probe: two good headers, one that uses a type without
#                including its header, one without an include guard (in a
#                subdirectory)
#   private_dir  probe: a header that needs probe's PRIVATE include directory
#   private_entry
#                probe (private_dir/): the same headers, with probe's PRIVATE
#                include directory the second HEADER_DIRS entry, as CORE's
#                generated/ is
cmake_minimum_required(VERSION 3.15)
project(tadah_header_check_case CXX)

include(FetchContent)
FetchContent_Declare(tadah_cmake
  URL "file:///tadah-header-check-never-downloaded"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(tadah_cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# Each fixture directory defines its targets next to its include/, as a
# module does.
if(CASE STREQUAL "good" OR CASE STREQUAL "new_header")
  set(fixture good)
  add_compile_definitions(PROBE_DIRECTORY_DEFINITION)
elseif(CASE STREQUAL "bad" OR CASE STREQUAL "private_dir")
  set(fixture ${CASE})
elseif(CASE STREQUAL "private_entry")
  set(fixture private_dir)
else()
  message(FATAL_ERROR "unknown CASE '${CASE}'")
endif()
add_subdirectory(${fixture})
add_subdirectory(tests)
