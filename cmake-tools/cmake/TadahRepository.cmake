# ====================================================================
#  The cmake-tools of a build is the one of its repository
#
#  Every build file of the repository includes this file by its path in
#  the repository (../cmake-tools/cmake/TadahRepository.cmake), before
#  cmake-tools is made available. A FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE
#  that names another directory is refused here, whatever that directory
#  holds: tadah_use_local_module (TadahDevelop.cmake) takes the modules
#  from the repository of the cmake-tools it is read from, so a
#  cmake-tools of another checkout would move every module there.
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
      "cmake-tools and every Tadah! module from one repository; remove the "
      "setting.")
  endif()
  unset(_tadah_given)
endif()
# The directory is written to the cache as well as to a variable: where the
# setting came from the command line without a type, the FetchContent of
# some CMake versions drops the variable (policy CMP0126 not set) and reads
# the cache entry, which may name the directory through a symbolic link or
# relative to another directory.
set(FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE "${_tadah_own}" CACHE PATH
    "The cmake-tools of this repository (set by its build files)" FORCE)
set(FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE "${_tadah_own}")
unset(_tadah_own)
