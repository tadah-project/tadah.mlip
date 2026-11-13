# ====================================================================
#  The build support of the repository
#
#  Every build file of the repository includes this file by its path in
#  the repository: cmake/TadahRepository.cmake at the root,
#  ../cmake/TadahRepository.cmake in a module directory. It adds the
#  build support to the build, once, whichever build file asks first:
#  the target tadah::compile_options and the tadah_* functions of the
#  files beside it.
#
#  The functions are defined with the policies of CMake 3.15, whatever
#  the build file that asks first declares.
# ====================================================================
cmake_minimum_required(VERSION 3.15)

if(NOT TARGET tadah::compile_options)
  include("${CMAKE_CURRENT_LIST_DIR}/TadahCompileOptions.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/TadahDevelop.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/TadahTesting.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/TadahVersion.cmake")

  # install only the *real* target, ALIAS targets are never installable
  install(TARGETS tadah_compile_options EXPORT tadah_cmakeTargets)

  install(EXPORT tadah_cmakeTargets
          NAMESPACE tadah::
          DESTINATION lib/cmake/tadah_cmake)
endif()
