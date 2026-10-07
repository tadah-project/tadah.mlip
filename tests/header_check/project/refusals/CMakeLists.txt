# CASE=refusals: REFUSAL picks one call of tadah_add_header_check() that
# must stop the configure step with its message (the driver lists them).
add_library(probe STATIC probe.cpp)
target_include_directories(probe PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(REFUSAL STREQUAL "unexpected_argument")
  tadah_add_header_check(probe EXTRA)
elseif(REFUSAL STREQUAL "not_a_target")
  tadah_add_header_check(no_such_target)
elseif(REFUSAL STREQUAL "alias")
  add_library(probe::probe ALIAS probe)
  tadah_add_header_check(probe::probe)
elseif(REFUSAL STREQUAL "imported")
  add_library(imported_probe INTERFACE IMPORTED)
  tadah_add_header_check(imported_probe)
elseif(REFUSAL STREQUAL "no_header_dirs")
  tadah_add_header_check(probe HEADER_DIRS)
elseif(REFUSAL STREQUAL "not_a_directory")
  tadah_add_header_check(probe HEADER_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/no_such_directory")
elseif(REFUSAL STREQUAL "no_headers")
  tadah_add_header_check(probe HEADER_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/no_headers")
elseif(REFUSAL STREQUAL "same_relative_path")
  # include/ and second/ both hold probe/x.h; second/'s has no guard, and
  # one translation unit for #include <probe/x.h> would check include/'s.
  tadah_add_header_check(probe HEADER_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/second")
else()
  message(FATAL_ERROR "unknown REFUSAL '${REFUSAL}'")
endif()
