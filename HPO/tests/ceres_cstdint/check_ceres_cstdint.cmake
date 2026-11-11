# Check that every C++ source of the Ceres library HPO fetches is compiled
# with <cstdint> included first, and that no other source is. Ceres 2.2.0
# uses the fixed-width integer types in sources that do not include the
# header (int64_t in internal/ceres/file.cc, which includes <cstdio> and
# <string> only). The headers of glibc declare the types all the same; the
# headers of the musl C library (Alpine Linux) do not, and such a source
# stops at "'int64_t' was not declared in this scope".
#
# The build that runs this check configures HPO once more, in WORK_DIR, with
# Ceres and without tests: PRESET is the initial cache
# hpo_write_no_ceres_preset() writes (configure_without_ceres/
# no_ceres_preset.cmake: the cache of that build, the source directory of
# each dependency, FETCHCONTENT_FULLY_DISCONNECTED, so nothing is
# downloaded), read here with TADAH_ENABLE_CERES ON. The check reads the
# configuration through CMake's file API (codemodel-v2), which gives the
# compile command of every source with any generator, and checks that
#   - the configure step succeeds and has a project Ceres with C++ sources;
#   - with GCC or Clang (COMPILER_ID), every C++ source of every target of
#     that project has "-include cstdint" in its compile command; with
#     another compiler, whose option for it is not known, none has;
#   - no source of another language and no source of a target outside that
#     project has it.
#
#   cmake -DHPO_DIR=<HPO source> -DPRESET=<initial cache> -DGENERATOR=<CMake generator>
#         -DCOMPILER_ID=<CMAKE_CXX_COMPILER_ID> -DWORK_DIR=<scratch>
#         -P check_ceres_cstdint.cmake
cmake_minimum_required(VERSION 3.24)

foreach(v HPO_DIR PRESET GENERATOR COMPILER_ID WORK_DIR)
  if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
    message(FATAL_ERROR "check_ceres_cstdint.cmake: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
set(build "${WORK_DIR}/build")
# The file API query: CMake writes the codemodel of the configuration.
file(WRITE "${build}/.cmake/api/v1/query/codemodel-v2" "")
# The initial cache: PRESET, but with Ceres.
set(eq "=")
while(PRESET MATCHES "\\]${eq}\\]")
  string(APPEND eq "=")
endwhile()
file(WRITE "${WORK_DIR}/with_ceres.cmake"
  "include([${eq}[${PRESET}]${eq}])\n"
  "set(TADAH_ENABLE_CERES ON CACHE BOOL \"\" FORCE)\n")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${HPO_DIR}" -B "${build}" -G "${GENERATOR}"
          -C "${WORK_DIR}/with_ceres.cmake"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "HPO did not configure with TADAH_ENABLE_CERES ON "
                      "(exit ${rc}):\n${out}")
endif()

set(reply "${build}/.cmake/api/v1/reply")
file(GLOB index "${reply}/index-*.json")
list(LENGTH index nindex)
if(NOT nindex EQUAL 1)
  message(FATAL_ERROR "expected one file API index in ${reply}, found "
                      "${nindex}: '${index}'")
endif()
file(READ "${index}" json)
string(JSON codemodel_file GET "${json}" reply codemodel-v2 jsonFile)
file(READ "${reply}/${codemodel_file}" codemodel)

set(ceres_project "")
string(JSON nprojects LENGTH "${codemodel}" configurations 0 projects)
math(EXPR last "${nprojects} - 1")
foreach(i RANGE ${last})
  string(JSON name GET "${codemodel}" configurations 0 projects ${i} name)
  if(name STREQUAL "Ceres")
    set(ceres_project ${i})
  endif()
endforeach()
if(ceres_project STREQUAL "")
  message(FATAL_ERROR "the configuration has no project Ceres: it is not "
                      "the one with Ceres")
endif()

if(COMPILER_ID MATCHES "GNU|Clang")
  set(expected TRUE)
else()
  set(expected FALSE)
endif()

set(nfail 0)
macro(fail)
  string(CONCAT what ${ARGV})
  message(NOTICE "FAIL: ${what}")
  math(EXPR nfail "${nfail} + 1")
endmacro()

# Of one target: the sources of each compile group, and whether its compile
# command has the option.
set(ceres_sources 0)
set(ceres_targets "")
set(other_targets 0)
string(JSON ntargets LENGTH "${codemodel}" configurations 0 targets)
math(EXPR last "${ntargets} - 1")
foreach(i RANGE ${last})
  string(JSON name GET "${codemodel}" configurations 0 targets ${i} name)
  string(JSON project GET "${codemodel}" configurations 0 targets ${i} projectIndex)
  string(JSON file GET "${codemodel}" configurations 0 targets ${i} jsonFile)
  file(READ "${reply}/${file}" target)
  string(JSON ngroups ERROR_VARIABLE no_groups LENGTH "${target}" compileGroups)
  if(no_groups OR ngroups EQUAL 0)
    continue()   # nothing of this target is compiled
  endif()
  if(project EQUAL ceres_project)
    list(APPEND ceres_targets "${name}")
  else()
    math(EXPR other_targets "${other_targets} + 1")
  endif()
  math(EXPR last_group "${ngroups} - 1")
  foreach(g RANGE ${last_group})
    string(JSON language GET "${target}" compileGroups ${g} language)
    string(JSON nsources LENGTH "${target}" compileGroups ${g} sourceIndexes)
    string(JSON first GET "${target}" compileGroups ${g} sourceIndexes 0)
    string(JSON first GET "${target}" sources ${first} path)
    set(command "")
    string(JSON n ERROR_VARIABLE none LENGTH "${target}" compileGroups ${g}
           compileCommandFragments)
    if(NOT none AND n GREATER 0)
      math(EXPR last_n "${n} - 1")
      foreach(k RANGE ${last_n})
        string(JSON fragment GET "${target}" compileGroups ${g}
               compileCommandFragments ${k} fragment)
        string(APPEND command " ${fragment}")
      endforeach()
    endif()
    if(command MATCHES " -include +cstdint( |$)")
      set(has TRUE)
    else()
      set(has FALSE)
    endif()
    set(where "target ${name}: ${nsources} ${language} source(s), the first ${first},")
    if(project EQUAL ceres_project AND language STREQUAL "CXX")
      math(EXPR ceres_sources "${ceres_sources} + ${nsources}")
      if(expected AND NOT has)
        fail("${where} are compiled without '-include cstdint' "
             "(${COMPILER_ID}): with the musl C library the sources of "
             "Ceres that use a fixed-width integer type and do not include "
             "<cstdint> (internal/ceres/file.cc) do not compile. The "
             "command:${command}")
      elseif(has AND NOT expected)
        fail("${where} are compiled with '-include cstdint', an option of "
             "GCC and Clang, by ${COMPILER_ID}. The command:${command}")
      endif()
    elseif(has)
      fail("${where} are compiled with '-include cstdint', which is for "
           "the C++ sources of Ceres only. The command:${command}")
    endif()
  endforeach()
endforeach()

if(ceres_sources EQUAL 0)
  fail("no target of the project Ceres has a C++ source: nothing was checked")
endif()
if(other_targets EQUAL 0)
  fail("no target outside the project Ceres has a source: tadah.hpo is "
       "missing from the configuration")
endif()

if(nfail GREATER 0)
  message(FATAL_ERROR "${nfail} check(s) failed")
endif()
list(JOIN ceres_targets ", " shown)
if(expected)
  message(STATUS "Ceres: the ${ceres_sources} C++ sources of ${shown} are "
                 "compiled with '-include cstdint' (${COMPILER_ID}); no "
                 "source of the ${other_targets} other targets is")
else()
  message(STATUS "Ceres: ${COMPILER_ID} is neither GCC nor Clang, and no "
                 "source of ${shown} or of the ${other_targets} other "
                 "targets is compiled with '-include cstdint'")
endif()
