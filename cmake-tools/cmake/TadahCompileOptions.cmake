# --------------------------------------------------------------------
#  Shared compile / link options for the whole Tadah ecosystem
#
#  Creates interface target: tadah::compile_options
#
#  Warning levels are tiered by build type:
#    Release/MinSizeRel  : -Wall
#    RelWithDebInfo      : -Wall -Wextra
#    Debug               : -Wall -Wextra -Wpedantic -pedantic-errors
#                          -Wshadow -Wconversion -Wsign-conversion
#                          -Wold-style-cast
#
#  TADAH_DEVELOP=ON adds -Wextra regardless of CMAKE_BUILD_TYPE, so e.g.:
#    cmake .. -DTADAH_DEVELOP=ON -DCMAKE_BUILD_TYPE=Release
#  produces an optimised -O3 build with -Wall -Wextra.
#
#  Removed build options are rejected here, because every Tadah module
#  configures this file (it adds cmake-tools) before it declares any
#  target of its own.
# --------------------------------------------------------------------
if(NOT DEFINED _TADAH_COMPILE_OPTS_INCLUDED)
  set(_TADAH_COMPILE_OPTS_INCLUDED 1)

  # ---- Removed options -------------------------------------------
  # TADAH_BUILD_MPI selected the legacy ScaLAPACK training build
  # (`tadah_mpi`), removed in 2.0.0. ON is an error rather than being
  # ignored, so a stale cache or preset cannot silently give a serial
  # build; OFF (as in older presets) is accepted.
  if(TADAH_BUILD_MPI)
    message(FATAL_ERROR
      "TADAH_BUILD_MPI=ON: the legacy MPI training build (tadah_mpi, "
      "ScaLAPACK) was removed in Tadah 2.0.0. Training runs in one process "
      "(OpenMP threads: TADAH_ENABLE_OPENMP). For MPI-parallel "
      "hyperparameter optimisation (TADAH:ANNEAL_PT / ANNEAL_PA replicas "
      "over MPI ranks) configure with -DTADAH_HPO_MPI=ON instead; it needs "
      "TADAH_ENABLE_NFP=ON and an MPI build of LAMMPS. Set "
      "-DTADAH_BUILD_MPI=OFF, or delete it from the CMake cache "
      "(-UTADAH_BUILD_MPI) and from your preset.")
  endif()

  # TADAH_GIT_BRANCH named the branch or tag at which a build fetched the
  # Tadah! modules. A build takes them from the directories of its
  # repository, so every value is an error rather than being ignored: the
  # build would go on with other sources than the ones asked for.
  # A -D on the command line, a -C preset and an earlier cmake command on
  # the build directory each leave the value in the cache, which is asked
  # for the entry itself (CACHE{...}): the answer depends neither on a
  # variable of the same name nor on whether a set(... CACHE ...) removed
  # that variable (policy CMP0126). A variable that a build file sets
  # before it adds Tadah! is refused as well; no -U removes that one.
  string(CONCAT _tadah_removed
    "the build option is removed. A build takes cmake-tools and every "
    "Tadah! module from the directories of its repository, at the commit "
    "that is checked out, and fetches none of them. A checkout of another "
    "branch or tag (git clone -b <ref>, or git checkout <ref> in the "
    "clone) builds that one.")
  if(DEFINED CACHE{TADAH_GIT_BRANCH})
    message(FATAL_ERROR
      "TADAH_GIT_BRANCH=$CACHE{TADAH_GIT_BRANCH}: ${_tadah_removed} The "
      "option is to be dropped from the cmake command and from a preset "
      "(-C) that sets it. The cache of the build directory keeps the "
      "value: -UTADAH_GIT_BRANCH on the next cmake command removes it, as "
      "does a new build directory.")
  elseif(DEFINED TADAH_GIT_BRANCH)
    message(FATAL_ERROR
      "TADAH_GIT_BRANCH=${TADAH_GIT_BRANCH}: ${_tadah_removed} A build "
      "file sets the variable before it adds Tadah!: its line "
      "set(TADAH_GIT_BRANCH ...) is to be removed.")
  endif()
  unset(_tadah_removed)

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
      "Generate compile_commands.json for the whole build" FORCE)

  # ---- Options ---------------------------------------------------
  option(TADAH_ENABLE_ASAN    "Enable AddressSanitizer"          OFF)
  option(TADAH_ENABLE_PROFILING "Add -pg to compiler & linker"   OFF)

  # Bridge: honour legacy variable name
  if(TADAH_USE_ASAN AND NOT TADAH_ENABLE_ASAN)
    message(DEPRECATION "TADAH_USE_ASAN is deprecated; use TADAH_ENABLE_ASAN instead.")
    set(TADAH_ENABLE_ASAN ON CACHE BOOL "" FORCE)
  endif()

  # ---- Interface target ------------------------------------------
  add_library(tadah_compile_options INTERFACE)
  add_library(tadah::compile_options ALIAS tadah_compile_options)

  # ---- Compile options -------------------------------------------
  target_compile_options(tadah_compile_options INTERFACE
    # Base warnings (all build types)
    -Wall

    # Standard warnings (Debug + RelWithDebInfo + DEVELOP)
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<BOOL:${TADAH_DEVELOP}>>:-Wextra>

    # Full warnings (Debug + DEVELOP only)
    # $<$<OR:$<CONFIG:Debug>,$<BOOL:${TADAH_DEVELOP}>>:
    #   -Wpedantic;-pedantic-errors;-Wshadow;-Wconversion;-Wsign-conversion;-Wold-style-cast>

    # Optimisation / debug info
    $<$<CONFIG:Debug>:-g;-Og>
    $<$<CONFIG:RelWithDebInfo>:-g;-O3>
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:MinSizeRel>:-Os>

    # AddressSanitizer
    $<$<BOOL:${TADAH_ENABLE_ASAN}>:-fsanitize=address;-fno-omit-frame-pointer>

    # Profiling
    $<$<BOOL:${TADAH_ENABLE_PROFILING}>:-pg>
  )

  # ---- Link options ----------------------------------------------
  target_link_options(tadah_compile_options INTERFACE
    $<$<BOOL:${TADAH_ENABLE_ASAN}>:-fsanitize=address>
    $<$<BOOL:${TADAH_ENABLE_PROFILING}>:-pg>
  )

endif()
