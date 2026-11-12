# ====================================================================
#  The cmake-tools of a build is the one of its repository
#
#  Every build file of the repository includes this file by its path in
#  the repository (../cmake-tools/cmake/TadahRepository.cmake). It adds
#  cmake-tools to the build, once: the target tadah::compile_options and
#  the tadah_* functions. A FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE that names
#  another directory is refused here, whatever that directory holds:
#  tadah_add_module (TadahDevelop.cmake) takes the modules from the
#  repository of the cmake-tools it is read from, so a cmake-tools of
#  another checkout would move every module there.
# ====================================================================

get_filename_component(_tadah_own "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
if(FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE)
  get_filename_component(_tadah_given
                         "${FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE}" REALPATH)
  if(NOT _tadah_given STREQUAL _tadah_own)
    message(FATAL_ERROR
      "FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE} "
      "(resolved: ${_tadah_given}) names another directory than the "
      "cmake-tools of this repository, '${_tadah_own}'. A build takes "
      "cmake-tools and every Tadah! module from one repository. The cache of "
      "the build directory keeps the setting: "
      "-UFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE on the next cmake command "
      "removes it, as does a new build directory.")
  endif()
  unset(_tadah_given)
endif()
if(NOT TARGET tadah::compile_options)
  add_subdirectory("${_tadah_own}" "${CMAKE_BINARY_DIR}/_deps/tadah_cmake-build")
endif()
unset(_tadah_own)
