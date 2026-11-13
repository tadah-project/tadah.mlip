# Throw-away project driven by the tadah_version_reader_* tests (see
# ../../CMakeLists.txt). It exercises cmake-tools' TadahVersion.cmake the
# way the Tadah modules use it: Tadah.CORE (a fixture, the directory CORE)
# is added to the build, then the version is read.
cmake_minimum_required(VERSION 3.15)
project(tadah_version_reader_case NONE)

include(${TADAH_CMAKE_DIR}/cmake/TadahVersion.cmake)

if(CASE STREQUAL "core_added")
  add_subdirectory("${CORE}" core)
  tadah_read_version_from_blueprint(V)
  get_property(_deps DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
  message(STATUS "RESULT=${V}|${V_FULL}")
  message(STATUS "DEPENDS=${_deps}")
elseif(CASE STREQUAL "no_core")
  tadah_read_version_from_blueprint(V)
  message(STATUS "RESULT=${V}")
else()
  message(FATAL_ERROR "unknown CASE '${CASE}'")
endif()
