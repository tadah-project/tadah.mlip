# Checks that a module directory of the repository configures on its own
# (cmake -S <repository>/<module> -B <build>), which is a supported build:
# the module finds the build support (cmake/ at the root of the repository)
# and the modules it builds on, each in the directory of its name beside it.
#
#   cmake -DMODULE=<core|models|mlip|md|hpo|lammps>
#         -DREPOSITORY=<root of the repository> -DPRESET=<initial cache>
#         -DGENERATOR=<CMake generator> -DWORK_DIR=<scratch>
#         -P check_module_alone.cmake
#
# 1. The directory is configured, not built, in a new build directory, with
#    PRESET as its initial cache (the compilers, hints and dependency
#    sources of the build that runs the check; nothing is downloaded). The
#    configure step must succeed, and its cache must name cmake/ of the
#    repository as the build support and, as source directories, the
#    module's own directory and that of every module it builds on.
# 2. The module's build file, copied to a directory without the repository
#    around it, is configured the same way: the configure step must stop
#    with the message that names the build support it did not find.
cmake_minimum_required(VERSION 3.16)

foreach(v MODULE REPOSITORY PRESET GENERATOR WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_module_alone.cmake: -D${v}=... is required")
  endif()
endforeach()

# The modules a module directory adds to the build, itself first, as
# <name of its project()>=<its directory>.
set(needs_core "Tadah.CORE=core")
set(needs_models "Tadah.MODELS=models" ${needs_core})
set(needs_mlip "Tadah.MLIP=mlip" ${needs_models})
set(needs_md "Tadah.MD=md" ${needs_models})
set(needs_lammps "Tadah.LAMMPS=lammps" ${needs_md})
set(needs_hpo "Tadah.HPO=hpo" ${needs_mlip} "Tadah.MD=md" "Tadah.LAMMPS=lammps")
if(NOT DEFINED needs_${MODULE})
  message(FATAL_ERROR "check_module_alone.cmake: unknown MODULE '${MODULE}'")
endif()

# A build names its directories by their resolved paths.
get_filename_component(REPOSITORY "${REPOSITORY}" REALPATH)
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
get_filename_component(WORK_DIR "${WORK_DIR}" REALPATH)

set(nfail 0)
macro(fail)
  string(CONCAT what ${ARGV})
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

# ---- 1. the module directory of the repository ---------------------------
set(build "${WORK_DIR}/build")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${REPOSITORY}/${MODULE}" -B "${build}"
          -G "${GENERATOR}" -C "${PRESET}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "${MODULE} did not configure on its own "
                      "(exit ${rc}):\n${out}")
endif()

file(STRINGS "${build}/CMakeCache.txt" cache
     REGEX "^(_TADAH_CMAKE_DIR|Tadah\\.[A-Z]+_SOURCE_DIR):")
set(expected "_TADAH_CMAKE_DIR:INTERNAL=${REPOSITORY}/cmake")
foreach(pair IN LISTS needs_${MODULE})
  string(REPLACE "=" ";" pair "${pair}")
  list(GET pair 0 name)
  list(GET pair 1 dir)
  list(APPEND expected "${name}_SOURCE_DIR:STATIC=${REPOSITORY}/${dir}")
endforeach()
list(REMOVE_DUPLICATES expected)
foreach(entry IN LISTS expected)
  if(NOT entry IN_LIST cache)
    fail("the cache of ${MODULE} configured on its own does not hold "
         "'${entry}':\n${cache}")
  endif()
endforeach()
list(LENGTH cache nfound)
list(LENGTH expected nexpected)
if(NOT nfound EQUAL nexpected)
  fail("the cache of ${MODULE} configured on its own names ${nfound} "
       "directories of the build support and the modules, expected "
       "${nexpected}:\n${cache}")
endif()

# ---- 2. the build file without the repository around it ------------------
set(alone "${WORK_DIR}/alone/${MODULE}")
file(COPY "${REPOSITORY}/${MODULE}/CMakeLists.txt" DESTINATION "${alone}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${alone}" -B "${WORK_DIR}/alone-build"
          -G "${GENERATOR}" -C "${PRESET}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
# CMake wraps the text of an error: compare with the line breaks dropped.
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
string(CONCAT needle
  "is built from inside the Tadah! repository, which holds its build "
  "support in '${alone}/../cmake': TadahRepository.cmake is not there.")
string(FIND "${flat}" "${needle}" at)
if(rc EQUAL 0)
  fail("a copy of ${MODULE}/CMakeLists.txt alone configured:\n${out}")
elseif(at LESS 0)
  fail("a copy of ${MODULE}/CMakeLists.txt alone was refused without "
       "'${needle}':\n${out}")
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed")
endif()
math(EXPR nmodules "${nexpected} - 1")
message(STATUS "${MODULE} configures on its own: the build support and "
               "${nmodules} module(s) come from ${REPOSITORY}")
