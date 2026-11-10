include_guard(GLOBAL)

if(NOT PKG_ML-TADAH)
  return()
endif()

if(NOT TARGET lammps)
  message(FATAL_ERROR "ML-TADAH: target 'lammps' not found")
endif()

if(NOT TARGET tadah.lammps)
  message(FATAL_ERROR "ML-TADAH: target 'tadah.lammps' not found (Tadah.LAMMPS did not configure?)")
endif()

if(NOT DEFINED Tadah_LAMMPS_SOURCE_DIR)
  message(FATAL_ERROR "ML-TADAH: Tadah_LAMMPS_SOURCE_DIR is not defined")
endif()

set(ml_tadah_dir "${Tadah_LAMMPS_SOURCE_DIR}/ML-TADAH")
if(NOT IS_DIRECTORY "${ml_tadah_dir}")
  message(FATAL_ERROR "ML-TADAH: Cannot find ML-TADAH directory: ${ml_tadah_dir}")
endif()

file(GLOB ml_tadah_sources CONFIGURE_DEPENDS
  "${ml_tadah_dir}/*.cpp"
)
file(GLOB ml_tadah_headers CONFIGURE_DEPENDS
  "${ml_tadah_dir}/*.h"
)

if(ml_tadah_sources STREQUAL "")
  message(FATAL_ERROR "ML-TADAH: No .cpp sources found in ${ml_tadah_dir}")
endif()

# Build ML-TADAH into LAMMPS directly (no copying into lammps/src)
target_sources(lammps PRIVATE ${ml_tadah_sources} ${ml_tadah_headers})
target_include_directories(lammps PRIVATE "${ml_tadah_dir}")

# Link Tadah.LAMMPS library (propagates include dirs and link deps)
target_link_libraries(lammps PRIVATE tadah.lammps)

message(STATUS "ML-TADAH: Enabled (compiled sources from ${ml_tadah_dir}, linked tadah.lammps)")

