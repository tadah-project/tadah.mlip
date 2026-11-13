# ====================================================================
#  The cmake-tools of a build is the one of its repository
#
#  Every build file of the repository includes this file by its path in
#  the repository (../cmake-tools/cmake/TadahRepository.cmake). It adds
#  cmake-tools to the build, once: the target tadah::compile_options and
#  the tadah_* functions.
# ====================================================================

if(NOT TARGET tadah::compile_options)
  get_filename_component(_tadah_own "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
  add_subdirectory("${_tadah_own}" "${CMAKE_BINARY_DIR}/_deps/tadah_cmake-build")
  unset(_tadah_own)
endif()
