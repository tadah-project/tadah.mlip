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
# --------------------------------------------------------------------
if(NOT DEFINED _TADAH_COMPILE_OPTS_INCLUDED)
  set(_TADAH_COMPILE_OPTS_INCLUDED 1)

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
