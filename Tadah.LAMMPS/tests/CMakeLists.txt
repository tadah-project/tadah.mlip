# Catch2 and its test discovery come from cmake-tools (tadah_setup_tests()).
tadah_setup_tests()

file(GLOB tests_src CONFIGURE_DEPENDS "*.cpp")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests_data DESTINATION .)

add_executable(test_tadah.lammps ${tests_src})
target_link_libraries(test_tadah.lammps PRIVATE Catch2 tadah.lammps)

include(Catch)
catch_discover_tests(test_tadah.lammps)

# pair_style tadah domain-decomposed on several MPI ranks, as users run it
# (`mpirun lmp`): mpi/check_pair_tadah_mpi.py runs LAMMPS executables built
# with ML-TADAH outside this project and compares, by atom id, the forces,
# per-atom energies and virials, the energy and the pressure tensor at step 0
# and the positions, velocities and energy after 100 NVE steps of the MPI lmp
# on 1, 2 and 4 ranks (and on 4 ranks in a 4 1 1 grid whose sub-domains are
# narrower than the ghost cutoff) with a reference run, to round-off (the
# tolerances and why: the script's docstring). One entry per case:
#   quickstart   the Quick Start potential (D2_Blip + DM_mBlipEnv): the
#                generic many-body kernel, newton on and off
#   two_species  two LAMMPS types and elements in the generic kernel
#   dm_ream      D2_Knot5 + DM_REAM: the EAM fast path and, EAMFAST false,
#                the generic kernel, newton on and off
#   nonlinear    M_BLR BF_Polynomial2: the full-list kernel
#   pair_2b      a D2_LJ pair potential: compute_2b_fast and compute_2b
#   dimer        the published Tadah!N2 DIMER potential (compute_dimers,
#                newton off, atom_style bond) on 512 molecules: step 0 only,
#                and sum(pe/atom) == PotEng in every run
# Pins: every comparison holds, every run reports its rank count and grid,
# atoms move between ranks (a neighbour-list build; not in the dimer case,
# which does not move them) and each case runs the kernel it names (the
# fast-path line of pair_coeff, or none).
# Before the fix: passes as well (a pin: pair_style tadah has no
# ghost-communication defect on these kernels; a pair_tadah that skips the
# reverse comm of compute_2b_mb_half, the forward comm of the EAM fast path,
# or packs the wrong dF/drho component fails the cases of that kernel, and
# one whose compute_dimers sets each atom's per-atom energy to its energy in
# one molecule pair, as before the DIMER pe/atom fix, fails the dimer case).
# The executables (empty: the entries are Skipped, exit code 77, naming what
# is missing; a value that names no executable is a failure):
set(TADAH_LMP_MPI "" CACHE FILEPATH
  "An lmp built with -DBUILD_MPI=ON and ML-TADAH, for the tadah.lammps_mpi_* tests")
set(TADAH_LMP_MPIEXEC "" CACHE FILEPATH
  "The mpiexec of the MPI that TADAH_LMP_MPI is built with")
set(TADAH_LMP_MPIEXEC_PREFLAGS "" CACHE STRING
  "Flags of TADAH_LMP_MPIEXEC before -n (e.g. --oversubscribe)")
set(TADAH_LMP_SERIAL "" CACHE FILEPATH
  "Optional serial lmp with ML-TADAH: the reference run of the tadah.lammps_mpi_* tests (default: TADAH_LMP_MPI on 1 rank)")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
# One --key=value token per argument: MLIP's add_test() drops an empty
# argument and splits one that contains ';'.
string(REPLACE ";" " " _lmp_preflags "${TADAH_LMP_MPIEXEC_PREFLAGS}")
foreach(_case quickstart two_species dm_ream nonlinear pair_2b dimer)
  add_test(NAME tadah.lammps_mpi_${_case}
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/mpi/check_pair_tadah_mpi.py
      --case=${_case}
      "--lmp-serial=${TADAH_LMP_SERIAL}"
      "--lmp-mpi=${TADAH_LMP_MPI}"
      "--mpiexec=${TADAH_LMP_MPIEXEC}"
      "--preflags=${_lmp_preflags}"
      --pots=${CMAKE_CURRENT_SOURCE_DIR}/tests_data/pots
      --workdir=${CMAKE_CURRENT_BINARY_DIR}/pair_tadah_mpi/${_case})
  set_tests_properties(tadah.lammps_mpi_${_case} PROPERTIES
    LABELS "mpi;lammps"
    PROCESSORS 4
    TIMEOUT 1200
    SKIP_RETURN_CODE 77
    ENVIRONMENT PYTHONDONTWRITEBYTECODE=1)
endforeach()

# tadah.lammps_headers (ctest -L headers): every header of include/
# compiles on its own and twice in a row (cmake-tools'
# tadah_add_header_check). Before the fix: passes as well (a pin: no
# Tadah.LAMMPS header failed when the check was added).
tadah_add_header_check(tadah.lammps)

# tadah.lammps_text (ctest -L text): no line of the repository's text files
# carries a person's initials, a reference to co-workers, a decision
# attributed to the code's owner, the name of an AI tool, a note kept
# outside the repository or filler wording (cmake-tools'
# tadah_add_text_check). Before the fix: fails, 10 lines: 9 with a person's
# initials and .gitignore's entry for an AI tool's state directory.
tadah_add_text_check(tadah.lammps)

# tadah.lammps_git_base (label cmake): the top-level CMakeLists.txt carries
# cmake-tools' TADAH_GIT_BASE block and fetches every Tadah! repository as
# ${TADAH_GIT_BASE}/<repository>.git (testing/tadah_git_base_check.cmake).
# Before the fix: fails (no block; the repositories named by the address of
# the development host).
tadah_add_git_base_check(tadah.lammps)
