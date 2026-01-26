# =============================================================================
# Tadah!MLIP - Linux defaults (CMake initial cache file)
#
# This file provides DEFAULTS for a new build directory (initial cache).
#
# HOW TO USE (Linux):
#   First either edit this file to your liking or use it as-is. Then, when
#   configuring a new build directory, pass it to CMake with -C. The -S option should point to Tadah.MLIP root directory while -B points to your desired build directory.
# EXAMPLE (generic):
#   cmake -S /path/to/project -B /path/to/project/build -C /path/to/tadah.mlip/cmake/presets/tadah-linux-defaults.cmake
#   cmake --build /path/to/project/build --parallel
#   cmake --install /path/to/project/build
#   
# EXAMPLE (from Tadah.MLIP root directory):
#   cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake
#   cmake --build build --parallel
#   cmake --install build
#
# OVERRIDING A SETTING WITHOUT EDITING THIS FILE:
#   cmake -S . -B build -C cmake/tadah-linux-defaults.cmake -DTADAH_ENABLE_OPENMP=ON
#
# YOU MAY ALSO EDIT THIS FILE:
#   It is safe to modify this file to match your preferences (e.g., enable features,
#   choose an install prefix, etc.) and then reuse it for future build directories.
#
# NOTE:
#   This file is applied only when you pass -C. Re-running CMake without -C
#   will keep the existing cache values in the build directory.
# =============================================================================

# -----------------------------------------------------------------------------
# Install prefix (Linux)
# -----------------------------------------------------------------------------
# Default CMake install prefix is typically /usr/local
# (may require sudo for installation).
#
# For a per-user install, ~/.local is a common choice.
# Leave commented to use the CMake default.
#
# set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix")

# -----------------------------------------------------------------------------
# Core build style (static libraries by default)
# -----------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL
  "Build shared libraries (.so) instead of static (.a)")

# Build type for single-config generators (Ninja/Makefiles) on Linux.
# Values: Debug, Release, RelWithDebInfo
# Uncomment to set a default:
#
# set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
#   "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)")

# -----------------------------------------------------------------------------
# Tadah optional features (defaults: OFF)
# -----------------------------------------------------------------------------
set(TADAH_ENABLE_OPENMP OFF CACHE BOOL
  "Enable OpenMP parallelization (requires OpenMP-capable compiler/toolchain)")

set(TADAH_MARCH_NATIVE OFF CACHE BOOL
  "Use -march=native (fastest on this machine; binaries may not run elsewhere)")

set(TADAH_ENABLE_FAST_MATH OFF CACHE BOOL
  "Enable 'fast-math' optimizations (may change floating-point results)")

# -----------------------------------------------------------------------------
# Tests (defaults: OFF)
# -----------------------------------------------------------------------------
set(BUILD_TESTING OFF CACHE BOOL
  "Build tests (enables CTest). Turn ON if you want `ctest` support.")

# -----------------------------------------------------------------------------
# NFP / LAMMPS integration (defaults: OFF)
# -----------------------------------------------------------------------------
set(TADAH_ENABLE_NFP OFF CACHE BOOL
  "Enable NFP integration that links against LAMMPS")

# The following variables are only used if TADAH_ENABLE_NFP=ON.
# Leave them empty by default and fill them in when enabling NFP.
#
# Notes:
#   - TADAH_LAMMPS_LIB: full path to the LAMMPS library to link against:
#       /path/to/liblammps_serial.a   (static)
#       /path/to/liblammps.so         (shared)
#   - TADAH_LAMMPS_DIR: LAMMPS root directory (source tree or install prefix),
#       used for headers and metadata discovery.
#   - TADAH_LAMMPS_LIBS: extra link items needed when linking to LAMMPS,
#       especially for static linking where transitive dependencies are not
#       propagated. Use a CMake list separated by semicolons, e.g. "-lz;-lzstd".
#       If you see errors like:
#         /usr/bin/ld: ... undefined reference to symbol 'gzflush'
#         /usr/bin/ld: ... libz.so: error adding symbols: DSO missing from command line
#       then you likely need to add zlib explicitly (e.g. "-lz") to TADAH_LAMMPS_LIBS.
#
set(TADAH_LAMMPS_LIB "" CACHE FILEPATH
  "NFP: Full path to LAMMPS library (e.g. /path/to/liblammps_serial.a or liblammps.so)")

set(TADAH_LAMMPS_DIR "" CACHE PATH
  "NFP: Path to LAMMPS root directory (source or install prefix)")

set(TADAH_LAMMPS_LIBS "" CACHE STRING
  "NFP: Extra link libraries for LAMMPS static linking (CMake list, e.g. -lz;-lzstd)")


# -----------------------------------------------------------------------------
# EXPERIMENTAL FEATURES BELOW
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# MPI build (experimental; defaults: OFF)
# -----------------------------------------------------------------------------
set(TADAH_BUILD_MPI OFF CACHE BOOL
  "Build MPI-enabled Tadah (experimental). Requires MPI toolchain and libraries.")
