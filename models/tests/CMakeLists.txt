# Catch2 and its test discovery come from cmake-tools (tadah_setup_tests()).
tadah_setup_tests()

file(GLOB tests_src CONFIGURE_DEPENDS "*.cpp")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

# The DM_REAM density before RHOFORM that test_ream_goldens.cpp compares
# with: a frozen copy of the code the goldens were captured from, compiled
# once with MODELS' flags, never with -march=native (an FMA would change its
# bits), for test_models and test_models_ream_native_link alike.
list(REMOVE_ITEM tests_src
     ${CMAKE_CURRENT_SOURCE_DIR}/ream_density_before_rhoform.cpp)
add_library(test_models_ream_before_rhoform OBJECT
            ream_density_before_rhoform.cpp)
target_link_libraries(test_models_ream_before_rhoform PRIVATE tadah.models)

add_executable(test_models ${tests_src})
# Threads: test_kernel_settings_once.cpp builds kernels in std::threads.
find_package(Threads REQUIRED)
target_link_libraries(test_models PRIVATE Catch2 tadah.models tadah.core
                                          Threads::Threads
                                          test_models_ream_before_rhoform)
target_include_directories(test_models PRIVATE ../include)
# test_api_examples.cpp reads the configuration examples of the class
# documentation (the API reference) from the headers.
target_compile_definitions(test_models PRIVATE
    TADAH_MODELS_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../include")

include(Catch)
# WORKING_DIRECTORY pins each registered Catch test case's cwd to the
# binary's directory so tests opening fixtures via relative paths
# (e.g. "tests_data/basis", "tests_data/eam.alloy" in test_kernels.cpp /
# test_dm.cpp / test_d2.cpp) resolve under ctest and IDE test runners
# regardless of the caller's cwd. Direct ./test_models invocations from
# elsewhere still need to cd into the binary dir.
catch_discover_tests(test_models
    WORKING_DIRECTORY $<TARGET_FILE_DIR:test_models>
)

# tadah.models_ream_goldens_native_link. Pins: the DM_REAM goldens (the
# [ream][rhoform][goldens] cases of test_ream_goldens.cpp) pass in a program
# compiled and linked with -march=native, under LTO when the build has it
# (CMAKE_INTERPROCEDURAL_OPTIMIZATION), as the tadah executable is built
# with TADAH_MARCH_NATIVE: MODELS' objects, compiled with MODELS' flags,
# keep their code at such a link, and give the program the density before
# RHOFORM bit for bit when those flags have no FMA contraction (the
# presets'), as the frozen copy of that density, compiled with the same
# flags (test_models_ream_before_rhoform), computes it. Two of the cases
# reach the density through DM_REAM's out-of-line methods; one calls
# ReamDensity from this program's own -march=native unit, and fails when a
# ReamDensity method is compiled here, as an inline function moved into
# ream_density.h would be (its multiply-adds fused into FMAs on an FMA
# machine). Not built when the compiler has no -march=native
# (TADAH_MARCH_NATIVE is ignored then too).
# Before the fix: passes as well (a pin of the build layout; with value,
# deriv and value_deriv moved into ream_density.h it fails, GCC 15.2).
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-march=native TADAH_MODELS_TESTS_HAVE_MARCH_NATIVE)
if(TADAH_MODELS_TESTS_HAVE_MARCH_NATIVE)
  add_executable(test_models_ream_native_link
      test_ream_goldens.cpp test_main.cpp)
  target_link_libraries(test_models_ream_native_link
      PRIVATE Catch2 tadah.models tadah.core test_models_ream_before_rhoform)
  target_compile_options(test_models_ream_native_link PRIVATE -march=native)
  target_link_options(test_models_ream_native_link PRIVATE -march=native)
  add_test(NAME tadah.models_ream_goldens_native_link
      COMMAND test_models_ream_native_link "[ream][rhoform][goldens]"
              --warn NoTests)
else()
  message(STATUS "Tadah.MODELS tests: the compiler has no -march=native; "
                 "tadah.models_ream_goldens_native_link is not built")
endif()

# tadah.models_ream_goldens_glibc_no_fma. Pins: the DM_REAM goldens (the
# [ream][rhoform][goldens] cases of test_ream_goldens.cpp) pass with the
# cos, sin and exp that glibc gives an x86-64 CPU without AVX2 and FMA
# (GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA selects them on any x86-64
# CPU), which round some arguments differently from those of a CPU with
# AVX2 and FMA, with which the results of ream_goldens_libm.h were
# recorded. tadah.models_ream_goldens_glibc_no_fma_libm, under the same
# setting, checks that the C library gives one or more of the recorded
# results differently, and is Skipped where it gives them all (a glibc that
# ignores the setting). Added with glibc on x86-64 only, the C library and
# processor the setting is for.
# Before the fix: the first fails, and the second passes: the anchor case
# asked glibc's own cos, sin and exp for the stored goldens bit for bit, and
# with this setting (glibc 2.43) exp(-0x1.1839f3fdcf5f8p+1) is
# 0x1.cabf14e03b9b2p-4 against the recorded 0x1.cabf14e03b9b3p-4, so that 8
# golden values differ ('8 == 0'; the first, Gaussian Cut_Cos signed,
# calc_rho at r = 0x1.064f765fd8adap+1).
include(CheckCXXSourceCompiles)
check_cxx_source_compiles([=[
#include <cstdlib>
#if !defined(__x86_64__) || !defined(__GLIBC__) || defined(__UCLIBC__)
#error "not glibc on x86-64"
#endif
int main() { return 0; }
]=] TADAH_MODELS_TESTS_GLIBC_X86_64)
if(TADAH_MODELS_TESTS_GLIBC_X86_64)
  add_test(NAME tadah.models_ream_goldens_glibc_no_fma
      COMMAND test_models "[ream][rhoform][goldens]" --warn NoTests)
  add_test(NAME tadah.models_ream_goldens_glibc_no_fma_libm
      COMMAND test_models "[.ream_goldens_libm]" --warn NoTests)
  set_tests_properties(tadah.models_ream_goldens_glibc_no_fma
                       tadah.models_ream_goldens_glibc_no_fma_libm
      PROPERTIES ENVIRONMENT "GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA")
  set_tests_properties(tadah.models_ream_goldens_glibc_no_fma_libm
      PROPERTIES
        SKIP_REGULAR_EXPRESSION "${TADAH_TEST_SKIP_REGULAR_EXPRESSION}")
endif()

# tadah.models_headers (ctest -L headers): every header of include/
# compiles on its own and twice in a row (cmake-tools'
# tadah_add_header_check). Before the fix: fails on
# descriptors/dm/dm_mEAD.hpp and dm_poly.hpp, which have no include guard
# ('redefinition of ...').
tadah_add_header_check(tadah.models)

# tadah.models_text (ctest -L text): no line of the repository's text files
# carries a person's initials, a reference to co-workers, a decision
# attributed to the code's owner, the name of an AI tool, a note kept
# outside the repository or filler wording (cmake-tools'
# tadah_add_text_check). Before the fix: fails, 13 lines: 7 with a person's
# initials, 4 decision notes, a workspace note and .gitignore's entry for an
# AI tool's state directory.
tadah_add_text_check(tadah.models)
