# Checks where a build takes the Tadah modules and cmake-tools from
# (cmake-tools' tadah_use_local_module, cmake/TadahDevelop.cmake, and the
# cmake-tools block of every build file with cmake/TadahRepository.cmake):
# the directories of the repository, whatever TADAH_DEVELOP and the
# environment variable TADAH_PATH say. Another directory given for a module
# or for cmake-tools, a repository without the module and a module directory
# copied out of its repository are refused. The repository's own directory
# given through a symbolic link is taken, by its resolved path.
#
#   cmake -DCASE=<case> -DREPOSITORY=<root of the repository>
#         -DWORK_DIR=<scratch> [-DGENERATOR=<CMake generator>
#         -DMAKE_PROGRAM=<build tool> -DCXX_COMPILER=<compiler>]
#         -P check_local_module.cmake
#
# The function cases run probe.cmake in a child `cmake -P` started in a
# module directory, as a module's build file calls the function. The project
# cases configure a real build file in a scratch build directory, with no
# network (FETCHCONTENT_FULLY_DISCONNECTED), with the generator and the
# compiler of the build that runs the test when they are given.
cmake_minimum_required(VERSION 3.15)

foreach(v CASE REPOSITORY WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_local_module.cmake: -D${v}=... is required")
  endif()
endforeach()

get_filename_component(WORK_DIR "${WORK_DIR}" ABSOLUTE)
# A build names its directories by their resolved paths (a checkout reached
# through a symbolic link included).
get_filename_component(REPOSITORY "${REPOSITORY}" REALPATH)
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/elsewhere/CORE")
set(MODULES CORE MODELS MLIP MD HPO Tadah.LAMMPS)

# ---- the cmake-tools block is the same in every build file ----------------
if(CASE STREQUAL "blocks_identical")
  set(first "")
  foreach(file CMakeLists.txt ${MODULES})
    if(NOT file STREQUAL "CMakeLists.txt")
      set(file "${file}/CMakeLists.txt")
    endif()
    file(READ "${REPOSITORY}/${file}" text)
    string(REGEX MATCH "# >>> cmake-tools.*# <<< cmake-tools" block "${text}")
    if(block STREQUAL "")
      message(FATAL_ERROR "${file}: no cmake-tools block")
    endif()
    # The one path this build file may name, then the name of its module.
    set(path "../cmake-tools")
    if(file STREQUAL "CMakeLists.txt")
      set(path "cmake-tools")
    endif()
    string(REPLACE "\${_tadah_here}/${path}" "<cmake-tools>" block "${block}")
    if(block MATCHES "_tadah_here}/")
      message(FATAL_ERROR
        "${file}: its cmake-tools block names another path than "
        "'\${_tadah_here}/${path}':\n${block}")
    endif()
    string(REGEX REPLACE "\"[^\"\n]* is built from inside" "\"X is built from inside"
           block "${block}")
    if(first STREQUAL "")
      set(first "${block}")
    elseif(NOT block STREQUAL first)
      message(FATAL_ERROR
        "${file}: its cmake-tools block differs from that of CMakeLists.txt "
        "by more than the path and the name:\n${block}")
    endif()
  endforeach()
  message(STATUS "case ${CASE}: seven blocks agree")
  return()
endif()

# A cmake-tools copied to another place, with a CORE directory beside it.
function(make_other_checkout)
  file(COPY "${REPOSITORY}/cmake-tools" DESTINATION "${WORK_DIR}/other")
  file(MAKE_DIRECTORY "${WORK_DIR}/other/CORE")
endfunction()

function(expect_refusal rc out needle)
  if(rc EQUAL 0)
    message(FATAL_ERROR "case ${CASE}: expected a refusal, got:\n${out}")
  endif()
  string(FIND "${out}" "${needle}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR
      "case ${CASE}: the refusal does not say '${needle}':\n${out}")
  endif()
  message(STATUS "case ${CASE}: refused, saying ${needle}")
endfunction()

# ---- project cases: a real configure of a build file -----------------------
if(CASE MATCHES "^project_")
  set(sources "${REPOSITORY}/MODELS")
  set(defines "")
  set(needle "")
  if(CASE STREQUAL "project_unpinned")
    # No pin, no TADAH_DEVELOP, no TADAH_PATH: the repository's directories.
  elseif(CASE STREQUAL "project_same_cmake_tools_given")
    # The repository's own cmake-tools, named through a symbolic link.
    file(CREATE_LINK "${REPOSITORY}/cmake-tools" "${WORK_DIR}/link" SYMBOLIC)
    set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${WORK_DIR}/link")
  elseif(CASE STREQUAL "project_same_module_given")
    # The repository's own CORE, named through a symbolic link.
    file(CREATE_LINK "${REPOSITORY}/CORE" "${WORK_DIR}/link" SYMBOLIC)
    set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${WORK_DIR}/link")
  elseif(CASE STREQUAL "project_other_module_refused")
    set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${WORK_DIR}/elsewhere/CORE")
    set(needle "names another directory than the module of this repository")
  elseif(CASE STREQUAL "project_other_cmake_tools_refused")
    # Every build file refuses it, and before it reads a file of the other
    # directory: that one is empty.
    file(MAKE_DIRECTORY "${WORK_DIR}/other/cmake-tools")
    set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH_CMAKE=${WORK_DIR}/other/cmake-tools")
    set(needle "names another directory than the cmake-tools of this repository")
    set(sources "${REPOSITORY}")
    foreach(module IN LISTS MODULES)
      list(APPEND sources "${REPOSITORY}/${module}")
    endforeach()
  elseif(CASE STREQUAL "project_module_alone_refused")
    # The module's build file without the repository around it.
    file(COPY "${REPOSITORY}/CORE/CMakeLists.txt" DESTINATION "${WORK_DIR}/alone/CORE")
    set(sources "${WORK_DIR}/alone/CORE")
    set(needle "is built from inside the Tadah! repository")
  else()
    message(FATAL_ERROR "check_local_module.cmake: unknown case '${CASE}'")
  endif()
  set(toolchain "")
  if(GENERATOR)
    list(APPEND toolchain -G "${GENERATOR}")
  endif()
  if(MAKE_PROGRAM)
    list(APPEND toolchain "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
  endif()
  if(CXX_COMPILER)
    list(APPEND toolchain "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
  endif()
  set(n 0)
  foreach(source IN LISTS sources)
    math(EXPR n "${n} + 1")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env --unset=TADAH_PATH
              "${CMAKE_COMMAND}" -S "${source}" -B "${WORK_DIR}/build${n}"
              ${toolchain} -DFETCHCONTENT_FULLY_DISCONNECTED=ON ${defines}
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    if(needle)
      expect_refusal("${rc}" "${out}" "${needle}")
    elseif(NOT rc EQUAL 0)
      message(FATAL_ERROR "case ${CASE}: configure failed (${rc}):\n${out}")
    endif()
  endforeach()
  if(needle)
    return()
  endif()
  file(STRINGS "${WORK_DIR}/build1/CMakeCache.txt" cache
       REGEX "^(tadah_cmake_SOURCE_DIR|Tadah.CORE_SOURCE_DIR|TADAH_DEVELOP|FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE|FETCHCONTENT_SOURCE_DIR_TADAH.CORE):")
  # The cache entries too: whichever of the variable and the cache entry
  # the FetchContent of the CMake version reads, it names the repository.
  foreach(expected "tadah_cmake_SOURCE_DIR:STATIC=${REPOSITORY}/cmake-tools"
                   "Tadah.CORE_SOURCE_DIR:STATIC=${REPOSITORY}/CORE"
                   "FETCHCONTENT_SOURCE_DIR_TADAH_CMAKE:PATH=${REPOSITORY}/cmake-tools"
                   "FETCHCONTENT_SOURCE_DIR_TADAH.CORE:PATH=${REPOSITORY}/CORE"
                   "TADAH_DEVELOP:BOOL=OFF")
    if(NOT expected IN_LIST cache)
      message(FATAL_ERROR
        "case ${CASE}: the cache does not hold '${expected}':\n${cache}")
    endif()
  endforeach()
  message(STATUS "case ${CASE}: cmake-tools and CORE come from ${REPOSITORY}")
  return()
endif()

# ---- function cases ---------------------------------------------------------
set(env "--unset=TADAH_PATH")
set(defines "")
set(cmake_dir "${REPOSITORY}/cmake-tools")
set(caller "${REPOSITORY}/MODELS")
set(expect "${REPOSITORY}/CORE")
set(needle "")
if(CASE STREQUAL "repository")
  # No option and no environment variable: the repository's directory.
elseif(CASE STREQUAL "tadah_path_is_not_read")
  set(env "TADAH_PATH=${WORK_DIR}/elsewhere")
  set(defines "-DTADAH_DEVELOP=ON")
elseif(CASE STREQUAL "same_directory_given")
  set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${REPOSITORY}/./CORE/")
elseif(CASE STREQUAL "cmake_tools_through_link")
  # cmake-tools read through a symbolic link in another directory is still
  # the one of the repository.
  file(CREATE_LINK "${REPOSITORY}/cmake-tools" "${WORK_DIR}/link" SYMBOLIC)
  set(cmake_dir "${WORK_DIR}/link")
elseif(CASE STREQUAL "other_directory_refused")
  set(defines "-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${WORK_DIR}/elsewhere/CORE")
  set(needle "FETCHCONTENT_SOURCE_DIR_TADAH.CORE")
elseif(CASE STREQUAL "other_cmake_tools_refused")
  make_other_checkout()
  set(cmake_dir "${WORK_DIR}/other/cmake-tools")
  set(needle "is not the repository of the project being configured")
elseif(CASE STREQUAL "missing_module_refused")
  # cmake-tools alone in a directory: its repository holds no CORE.
  file(COPY "${REPOSITORY}/cmake-tools/cmake" DESTINATION "${WORK_DIR}/alone/cmake-tools")
  set(cmake_dir "${WORK_DIR}/alone/cmake-tools")
  set(caller "${WORK_DIR}/alone")
  set(needle "${WORK_DIR}/alone/CORE")
else()
  message(FATAL_ERROR "check_local_module.cmake: unknown case '${CASE}'")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${env}
          "${CMAKE_COMMAND}" "-DTADAH_CMAKE_DIR=${cmake_dir}" ${defines}
          -P "${CMAKE_CURRENT_LIST_DIR}/probe.cmake"
  WORKING_DIRECTORY "${caller}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)

if(needle)
  expect_refusal("${rc}" "${out}" "${needle}")
  return()
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "case ${CASE}: probe failed (${rc}):\n${out}")
endif()
string(REGEX MATCH "SOURCE=\\[([^]]*)\\]" _ "${out}")
if(NOT "${CMAKE_MATCH_1}" STREQUAL "${expect}")
  message(FATAL_ERROR
    "case ${CASE}: the module is taken from '${CMAKE_MATCH_1}', expected '${expect}':\n${out}")
endif()
message(STATUS "case ${CASE}: ${CMAKE_MATCH_1}")
