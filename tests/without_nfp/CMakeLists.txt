# tadah_without_nfp: the tadah executable as a build without TADAH_ENABLE_NFP
# (the default) makes it, in a build with it: tadah's sources (TADAH_SRC_FILES)
# and include directories, compiled without TADAH_ENABLE_NFP and linked as the
# umbrella links tadah without NFP (tadah::compile_options, tadah.mlip, OpenMP
# with TADAH_ENABLE_OPENMP, LAPACK, AddressSanitizer with TADAH_ENABLE_ASAN),
# with no HPO and no LAMMPS. Not part of `all`: the CTest entry
# tadah_without_nfp_links (../CMakeLists.txt) builds and runs it. The
# mallinfo2 interposition of tadah is left out: it changes nothing a source
# can refer to.

# This directory compiles without the umbrella's TADAH_ENABLE_NFP
# (add_compile_definitions in Tadah.MLIP/CMakeLists.txt).
get_directory_property(_tadah_defs COMPILE_DEFINITIONS)
list(REMOVE_ITEM _tadah_defs TADAH_ENABLE_NFP)
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${_tadah_defs}")

add_executable(tadah_without_nfp EXCLUDE_FROM_ALL ${TADAH_SRC_FILES})
target_include_directories(tadah_without_nfp PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/external/CLI11/include)
target_link_libraries(tadah_without_nfp PRIVATE
  tadah::compile_options tadah.mlip)
if(TADAH_ENABLE_OPENMP)
  target_link_libraries(tadah_without_nfp PRIVATE OpenMP::OpenMP_CXX)
endif()
if(NOT TADAH_BUILD_DOCS_ONLY)
  target_link_libraries(tadah_without_nfp PRIVATE ${LAPACK_LIBRARIES})
endif()
if(TADAH_ENABLE_ASAN)
  target_compile_options(tadah_without_nfp PRIVATE
    -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_libraries(tadah_without_nfp PRIVATE -fsanitize=address)
endif()

# test_tadah_without_nfp: test_tadah as a build without TADAH_ENABLE_NFP
# makes it, in a build with it: the same sources (TEST_SOURCES and the CLI
# sources test_tadah compiles, ../CMakeLists.txt), include directories and
# definitions, compiled without TADAH_ENABLE_NFP and linked as test_tadah
# is without NFP (no HPO, no LAMMPS). Not part of `all`: the CTest entry
# tadah_without_nfp_tests (../CMakeLists.txt) builds it and runs its
# [not-built] cases (without_nfp/check_tests_without_nfp.cmake).
add_executable(test_tadah_without_nfp EXCLUDE_FROM_ALL
  ${TEST_SOURCES}
  "${PROJECT_SOURCE_DIR}/src/cli_task.cpp"
  "${PROJECT_SOURCE_DIR}/src/tadah_cli.cpp")
target_include_directories(test_tadah_without_nfp PRIVATE
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/external/CLI11/include
  ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_tadah_without_nfp PRIVATE
  tadah::compile_options Catch2 tadah.core tadah.models tadah.mlip)
target_compile_definitions(test_tadah_without_nfp PRIVATE
  TADAH_PUBLISHED_MLIPS_DIR="${PROJECT_SOURCE_DIR}/docs/source/mlips"
  TADAH_QUICKSTART_DIR="${PROJECT_SOURCE_DIR}/docs/source/quickstart")
