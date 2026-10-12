# Contributing to Tadah!MLIP

Thanks for your interest in Tadah! This document is the canonical
contributor guide for the multi-repo Tadah! project. Every per-module
repository points back here.

The project is hosted at <https://git.ecdf.ed.ac.uk/tadah/>.

## Repositories

Tadah! is split across several GitLab projects on `git.ecdf.ed.ac.uk`:

| Repo | Role |
|------|------|
| `tadah.mlip`   | Umbrella project — produces the `tadah` CLI, ships the docs build, drives the multi-module cmake configure. |
| `core`         | Core types, blueprint TOML, registries, parser, signal/checkpoint infrastructure. |
| `models`       | Cutoffs, descriptors, regressors, basis functions. |
| `mlip`         | Training / load-time data pipeline used by `tadah train` and friends. |
| `md`           | Molecular dynamics surface (`Tadah.MD`). |
| `hpo`          | Hyperparameter optimisation driver (`tadah hpo`). |
| `tadah.lammps` | LAMMPS plugin and `pair_tadah`. |
| `cmake-tools`  | Shared CMake helpers (version reader, install helpers, etc.). |

The working branch of the 1.x line in every repo is `v1-dev` (it
replaces `develop`). Releases are made from it and tagged `v<X.Y.Z>` in
every repo (see [`RELEASING.md`](RELEASING.md)).

## Reporting bugs

File a bug report on the GitLab project that owns the affected code.
If you're unsure which one, file it on
<https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues> — we'll move it.

A useful bug report includes:

1. Tadah! version (`tadah --version`).
2. Build mode: `Release` or `Develop`, MPI on/off, compiler + version.
3. The minimal `config.tadah` (or `config.hpo`) that reproduces.
4. The exact command you ran and the full stderr/stdout (or a tail
   long enough to show the error).
5. If a LAMMPS run is involved, the LAMMPS input script and the
   `pair_tadah` line.

The `Bug` issue template prompts for these fields.

## Proposing features

Open a feature-request issue on the relevant GitLab project before
sending a merge request. The `Feature` issue template asks for the
motivation, the proposed surface change (CLI flag / config keyword /
API call), and any backwards-compatibility implications. A short
discussion on the issue is faster than an MR round-trip when the
design is still moving.

## Setting up a development environment

The umbrella repo (`tadah.mlip`) drives the multi-module build. The
quickest path:

```bash
# Pick a directory; clone the repos as siblings, on v1-dev (a plain
# clone checks out each repository's default branch, which need not be
# v1-dev), under the directory names the develop build looks for:
# cmake-tools, CORE, MODELS, MLIP, MD, HPO, Tadah.LAMMPS, Tadah.MLIP.
export TADAH_PATH=/path/to/your/tadah_root
mkdir -p "$TADAH_PATH" && cd "$TADAH_PATH"
for repo in cmake-tools core models mlip md hpo tadah.lammps tadah.mlip; do
  case "$repo" in
    cmake-tools) dir=cmake-tools ;;
    tadah.*)     dir="Tadah.$(echo "${repo#tadah.}" | tr '[:lower:]' '[:upper:]')" ;;
    *)           dir="$(echo "$repo" | tr '[:lower:]' '[:upper:]')" ;;
  esac
  git clone -b v1-dev "https://git.ecdf.ed.ac.uk/tadah/${repo}.git" "$dir"
done

# Develop build: TADAH_DEVELOP=ON takes cmake-tools and the Tadah modules
# from the sibling checkouts in $TADAH_PATH instead of fetching them.
cd Tadah.MLIP
cmake -S . -B build -DTADAH_DEVELOP=ON -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build
```

For a `Release` build (everything fetched from GitLab — no local
sibling worktrees needed; the modules are fetched at the branch of the
`Tadah.MLIP` checkout, here `v1-dev`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If your local checkouts do **not** follow the `$TADAH_PATH` sibling
convention, point CMake at each module explicitly with the
`FETCHCONTENT_SOURCE_DIR_TADAH.<MODULE>` overrides — `FetchContent`
then short-circuits the GitLab clone and uses your tree:

```bash
cmake -S . -B build \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=/abs/path/CORE \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=/abs/path/MODELS \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=/abs/path/MLIP \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=/abs/path/HPO
```

Local edits in any of those repos are picked up on the next
`cmake --build`.

LAMMPS integration: see `Tadah.LAMMPS/README.md` and the docs page on
LAMMPS.

## Running tests

Each repo ships a Catch2 suite, built when the project is configured
with `-DBUILD_TESTING=ON`. The umbrella build aggregates the suites of
every module it builds:

```bash
cd Tadah.MLIP/build
ctest --output-on-failure
```

Every non-hidden Catch2 test case is its own CTest entry (`ctest -N`
lists them). Hidden test cases (tags `[.]`, `[.tag]` or `[!benchmark]`)
are not discovered; they run under CTest only through an explicit entry
(see below), where one exists. The suite should run clean on `v1-dev`.

Test discovery comes from cmake-tools: each module's `tests/CMakeLists.txt`
calls `tadah_setup_tests()`, `include(Catch)` and `catch_discover_tests()`
(see the cmake-tools README). It guarantees that:

- each non-hidden test case is registered exactly once, under its exact
  name, and its entry selects exactly that case, whatever characters the
  name contains (`;`, `[`, `,`, `\`, a leading `#`, `~`, `-` or tab, ...);
- every entry passes `--warn NoTests`, so an entry that matches no test
  case fails instead of passing silently. Catch accepts `--warn` only
  once, so `EXTRA_ARGS` of `catch_discover_tests()` containing `--warn` or
  `-w` stop the build;
- a name Catch cannot select on its own stops the build with a message
  naming the test: a leading or trailing `*` (always a wildcard), a blank
  name, or a name that differs from another test case's name, hidden ones
  included, only in letter case or surrounding whitespace. Rename the test
  case.

Do not vendor `catch.hpp` or the Catch CMake scripts in a module. A hidden
tag (`[.statistical]`) runs through an explicit entry, which also needs the
flag: `add_test(NAME x COMMAND test_foo "[.tag]" --warn NoTests)`.
`ctest -R tadah_catch` runs the regression tests for the discovery itself
(`tests/catch_discovery`). `ctest -L docs` runs the unit tests of the
documentation build tools (see *Building the documentation*).
`ctest -L examples` runs the CLI examples the documentation includes
(`tadah_cli_example_<name>`, one per `examples/CLI/<name>/README.rst` that
`docs/source/examples.rst` includes): `tests/cli_examples` copies the
example to the build tree, runs every `tadah` command of the README's bash
code blocks and checks that each exits 0 and leaves its result (the
potential of `tadah train`; a best potential and a `best_loss.tadah` row
for `tadah hpo`). An example included in the docs is tested with no further
registration. The same label has `tadah_quickstart_reproduces_pot`, which
retrains the *Quick Start* potential (`docs/source/quickstart`) with the
page's `tadah train` and `tadah --task` commands and checks that the shipped
`pot.tadah` is what they write (its keys, and its predictions on
`db.predict`): after a change to `config.train`, or to the code, that changes
the potential, regenerate `pot.tadah` with `tadah train -c config.train` in
that directory and check the numbers the page quotes. The Catch test case
`quickstart: in.ml with the shipped pot.tadah ...` (test_tadah, NFP builds)
runs the page's LAMMPS example, `in.ml`, with it.
`ctest -L e2e` runs HPO's host end-to-end groups
(`hpo_fixweight_e2e`, `hpo_durable_e2e`, `hpo_embf0_e2e`, `hpo_rcut_e2e`,
`hpo_rhoform_e2e`, `hpo_rhogate_e2e`, `hpo_rhobargate_e2e`,
`hpo_bumpgate_e2e`, `hpo_nn_e2e`, `hpo_valmetric_e2e`, `hpo_pc_readback_e2e`, `hpo_trainer_e2e`, `hpo_hotvar_e2e`) and the `hpo_tadah_cli_*` cases, which run the built `tadah`: their entries
set `TADAH_BINARY` to it in a build with the `tadah` target, and in one
without it (a standalone HPO build) the cases are Skipped unless
`TADAH_BINARY` names an executable. `hpo_tadah_cli_pc_readback` and
`hpo_tadah_cli_pc_lammps_verbose` run `tadah` under `PC_ISOLATION THREAD`
and `PROCESS` and leave `TADAH_PC_ISOLATION` out of its environment (it
would override both), so their `PROCESS` half needs `socketpair` even where
`TADAH_PC_ISOLATION=THREAD` is set. In an MPI build they run `THREAD` only,
and `tadah` under the launcher of one rank (`TADAH_MPIEXEC`, see *The MPI
build* below).

`ctest -L bench` runs HPO's LAMMPS-driven cases, four entries of hidden
Catch cases (`[.][!benchmark]`): `hpo_lammps_hessian` (the Hessian of a
relaxed 4-atom bcc Ta cell and a `pair_tadah` virial microbench),
`hpo_phase3_pilot` (the `PC_LAMMPS` adjoint chain on real relaxations,
against finite differences), `hpo_bench_pair_tadah` (a `pair_tadah`
microbench on 256 atoms) and `hpo_host_e2e` (`HPO_Host`'s analytical
gradient, a `PC_LAMMPS` script among its cases). They run in the full
suite, in about 8 s. The first three run on the Ta potential shipped with
HPO's tests, `HPO/tests/data/bench_ta.tadah`: a copy of the *Quick Start*
potential, whose header gives its provenance. When you regenerate that
potential, the copy stays as it is. `TADAH_BENCH_POT` names another
potential, and `TADAH_BENCH_CELL_DATA` a LAMMPS data file for the
microbench's cell. A potential that cannot be read or run is a failure, not
a skip; the tolerances of the cases are set for the shipped potential. The
other hidden `[!benchmark]` case of HPO, the bump gate, runs in
`hpo_bumpgate_e2e` (`ctest -L e2e`).

`ctest -L lammps` runs `pair_style tadah` domain-decomposed, as users run
it (`mpirun lmp`), in an NFP build: Tadah.LAMMPS's `tadah.lammps_mpi_<case>`
entries (`tests/mpi/check_pair_tadah_mpi.py`, labels `mpi` and `lammps`)
run an `lmp` built with `-DBUILD_MPI=ON` and ML-TADAH under the launcher of
its MPI on 1, 2 and 4 ranks, and on 4 ranks in a `4 1 1` grid whose
sub-domains are narrower than the ghost cutoff, and compare every run by
atom id with a reference run: the forces, per-atom energies and virials,
the energy and the pressure tensor at step 0, and the positions,
velocities and energy after 100 NVE steps agree to round-off (the script
states each tolerance and why). The cases are `quickstart` (the *Quick
Start* potential: the generic many-body kernel, newton on and off),
`two_species`, `dm_ream` (D2_Knot5 + DM_REAM: the `EAMFAST` kernel and,
with `EAMFAST false`, the generic one), `nonlinear` (M_BLR BF_Polynomial2:
the full-list kernel), `pair_2b` and `dimer` (the published Tadah!N2
`DIMER` potential on 512 N2 molecules, newton off: step 0 only, and the
per-atom energies sum to the energy in every run). The executables are
built outside Tadah! (the docs' *Installation*, *Running pair_style tadah
on several MPI ranks*) and named at configure time:

```bash
cmake -S Tadah.MLIP -B build ... \
    -DTADAH_LMP_MPI=/path/to/lammps/build-mpi/lmp_mpi \
    -DTADAH_LMP_MPIEXEC=/usr/bin/mpiexec \
    -DTADAH_LMP_SERIAL=/path/to/lammps/build/lmp_serial   # optional
ctest --test-dir build -L lammps --output-on-failure
```

`TADAH_LMP_MPIEXEC` is the launcher of the MPI that `TADAH_LMP_MPI` is
built with (another one starts one-rank jobs, which the entries detect and
fail on); `TADAH_LMP_MPIEXEC_PREFLAGS` adds flags before `-n` (for example
`--oversubscribe` on a machine with fewer than four slots). The reference
run is the serial `lmp` of `TADAH_LMP_SERIAL` when it is set, else the MPI
`lmp` on one rank. Without `TADAH_LMP_MPI` or `TADAH_LMP_MPIEXEC` the
entries are *Skipped* (exit code 77), naming what is missing; a variable
that names no executable is a failure. They run in the serial build as well
as in the MPI build: the executables they drive are LAMMPS', not Tadah's.

`ctest -L headers` checks that every header compiles on its own: one entry
per module the build configures (`tadah.core_headers`, `tadah.models_headers`,
`tadah.mlip_headers` and, with `TADAH_ENABLE_NFP`, `tadah.md_headers`,
`tadah.lammps_headers` and `tadah.hpo_headers`) and, for the `tadah`
executable, `tadah_headers`, from cmake-tools'
`tadah_add_header_check()`. Each header of the module's `include/` (and
CORE's generated blueprint header) is compiled in a translation unit that
includes it twice and nothing else, with the include directories and
definitions a user of the library gets (for `tadah`, the executable's own):
a header that relies on something included before it, or has no include
guard, fails, and the output names it.
A new header is checked without being registered. The checks are not part of
the default build; their entries build them (`cmake --build`), on the
machine's physical cores or on `-DTADAH_HEADER_CHECK_JOBS=<n>` processes, and
a rerun compiles only what changed. `ctest -R tadah_header_check` tests the function
itself; those entries need CMake 3.19 (they read CTest's test list with
`string(JSON)`), and on CMake 3.16-3.18, which the tests of a build without
`TADAH_ENABLE_NFP` accept, they are Skipped with a line that says so. The
installed tree is not checked.

**Skipped, not passed.** A test that cannot run because an optional input
is absent (an external dataset, an environment variable) must be reported
as *Skipped*, so that a passing `ctest` means the checks ran:

- a Catch test case includes `tadah_test_skip.hpp` (cmake-tools) before,
  or instead of, `catch2/catch.hpp` and calls `TADAH_SKIP("<reason>" << ...)`
  before its first check, directly in the `TEST_CASE` body (not in a helper
  function, from which it returns only to the test case) and outside every
  `SECTION`. The macro returns from the test case; when the test case ends
  without a failed assertion, the marker line `TADAH-SKIP: <reason>` is
  written to standard error, and every discovered entry has a
  `SKIP_REGULAR_EXPRESSION` for that line. A test case that asked to be
  skipped and failed stays *Failed*. Do not `WARN` and `return`: that is a
  pass. An optional input that the user names explicitly (an environment
  variable that is set) and that cannot be read is a failure, not a skip.
  An explicit `add_test` entry that runs exactly one test case sets
  `SKIP_REGULAR_EXPRESSION "${TADAH_TEST_SKIP_REGULAR_EXPRESSION}"`
  itself;
- a script entry either exits with a code of its own and sets
  `SKIP_RETURN_CODE` (`hpo_strategy_matrix` and `tadah_cli_example_*`:
  77), or prints a line that its own `SKIP_REGULAR_EXPRESSION` matches
  (`tadah_docs_tools`, see *Building the documentation*).

The CTest summary lists skipped entries under "The following tests did not
run". With the documentation tools installed and without the optional data,
the suite skips exactly the two MLIP `NNFinder: real CH4 phase-I structure`
cases (`TADAH_TEST_PHASEI_DB`) and `hpo_strategy_matrix`
(`TADAH_HPO_INTEGRATION_BUNDLE`), and in a build with `TADAH_ENABLE_NFP` the
six `tadah.lammps_mpi_*` entries unless `TADAH_LMP_MPI` and
`TADAH_LMP_MPIEXEC` name an MPI `lmp` and its launcher (`ctest -L lammps`
above); `tadah_docs_tools` is Skipped too when one
of its tools is missing. A build without `TADAH_ENABLE_NFP` also skips
`tadah_cli_example_ex_2`, whose README runs `tadah hpo`, and the Catch case
`quickstart: in.ml with the shipped pot.tadah conserves energy and keeps
the bcc crystal` (`pair_style tadah` is not built); a build without
`TADAH_ENABLE_CERES` skips `RHOFORM NONNEG/MONOTONE: one WARNING when an
optimiser can leave the box` (its optimiser is `CERES:LBFGS`). See the
cmake-tools README, *Skipped tests: the TADAH-SKIP marker*, for the rules.

**Build configurations tested.** The suite above is that of a build with
`TADAH_ENABLE_NFP` and `TADAH_ENABLE_CERES` (Ceres is ON by default, NFP
OFF). The configurations without them are built and tested the same way,
from the same sources, each in a build directory of its own:

```bash
# Without Ceres: LIB CERES is refused as not built, and tadah refit
# refuses REFIT_MODE compact (use REFIT_MODE dense).
cmake -S Tadah.MLIP -B build_tadah_noceres -DTADAH_DEVELOP=ON -DBUILD_TESTING=ON \
    -DTADAH_ENABLE_NFP=ON -DTADAH_ENABLE_CERES=OFF \
    -DTADAH_LAMMPS_DIR=... -DTADAH_LAMMPS_LIB=...    # as for the NFP build
# Without NFP (the default): no HPO, no LAMMPS.
cmake -S Tadah.MLIP -B build_tadah_nonfp -DTADAH_DEVELOP=ON -DBUILD_TESTING=ON

cmake --build build_tadah_noceres -j     # and build_tadah_nonfp
ctest --test-dir build_tadah_noceres --output-on-failure --no-tests=error
```

With an initial-cache preset (`-C <preset>`), add `-DTADAH_ENABLE_CERES=OFF`
or `-DTADAH_ENABLE_NFP=OFF` to its command line. Each suite runs clean with
the skips listed above (on CMake 3.16-3.18 also the six
`tadah_header_check_*` entries are Skipped).
The tests need CMake 3.16 in every configuration: CTest reports a skipped
test through the `SKIP_REGULAR_EXPRESSION` test property, which it has from
3.16 and ignores before (reporting the test Passed), so cmake-tools'
`include(Catch)` stops an older CMake at the configure step, saying so; the
build without tests accepts 3.15, and one with NFP needs 3.24 (HPO). A build
without NFP refuses `tadah hpo` and `TASK hpo` before any task runs, naming
`TADAH_ENABLE_NFP`, and its `tadah -h` says so on the `hpo` line; the
`[not-built]` cases of test_tadah check it there.
Without Ceres, the test cases that need it are not compiled (they are not registered, as `test_hpo` is not built without
NFP), and those that use `LIB CERES` as an example check its refusal
instead. Without NFP the tests of HPO, MD and Tadah.LAMMPS are not built,
and the labels `e2e`, `bench`, `shutdown`, `statistical`, `anneal`, `slow`,
`hpo` and `mpi` have no entry: `ctest -L e2e` there runs nothing, which
`--no-tests=error` reports as a failure (without it CTest exits 0). Every build checks these configurations
in part: `tadah_without_nfp_links` (label `cmake`, builds with NFP) builds
and runs `tadah` as a build without NFP links it (`tadah hpo` is refused
naming `TADAH_ENABLE_NFP`), and `hpo_configure_without_ceres` (label
`cmake`) configures HPO without Ceres, from the build's own cache and the
sources of its dependencies, and checks that `tadah.hpo` still has Eigen
(`hpo_configure_without_ceres_preset` checks that a configure from that
cache sees what the build saw). Neither compiles HPO without
Ceres or runs the suites of the other configurations: build and test both
after a change to the CMake files or to code under `TADAH_ENABLE_NFP` or
`TADAH_USE_CERES`. `tadah_cmake_minimum_features` checks that no CMake file
of Tadah.MLIP uses a command, option or test property newer than its
`cmake_minimum_required()` (`tests/cmake_minimum/check_cmake_features.py`
lists them), and that no file under `tests/` declares first more than the
tests' minimum, 3.16 (`tests/CMakeLists.txt`): a script whose body needs
more checks the version first and skips, saying why, as the header check
does.

**The MPI build (`TADAH_HPO_MPI`).** The registered suite above is the
serial build's, and it runs no MPI code: the multi-rank `ANNEAL_PT` /
`ANNEAL_PA` drivers, their agreement on a stop or an error, the `.r<rank>`
outputs and the shared `best_pot.tadah` of a multi-rank run, and the MPI
LAMMPS (`MPI_COMM_SELF` instances) are covered only by a build with
`-DTADAH_HPO_MPI=ON`, which needs a LAMMPS library built with MPI and
ML-TADAH (the docs' *Installation*, *MPI-Parallel Hyperparameter
Optimisation*). A develop build of both, with the system MPI on `PATH`:

```bash
# LAMMPS with MPI and ML-TADAH; the library name must contain "mpi"
cmake -S LAMMPS/cmake -B build_lammps_mpi \
    -DBUILD_MPI=ON -DLAMMPS_MACHINE=develop_mpi \
    -DPKG_ML-TADAH=ON -DTADAH_LAMMPS_SOURCE_DIR="$TADAH_PATH/Tadah.LAMMPS" \
    -DCMAKE_PROJECT_INCLUDE="$TADAH_PATH/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake" \
    -DTADAH_DEVELOP=ON
cmake --build build_lammps_mpi --target lammps -j

# Tadah with TADAH_HPO_MPI against it, with OpenMP (LAMMPS builds with it
# when it finds it), and the launcher of the MPI that mpicxx belongs to
# (MPIEXEC_EXECUTABLE, see below)
cmake -S Tadah.MLIP -B build_tadah_mpi -DTADAH_DEVELOP=ON -DBUILD_TESTING=ON \
    -DTADAH_ENABLE_NFP=ON -DTADAH_HPO_MPI=ON -DTADAH_ENABLE_OPENMP=ON \
    -DTADAH_LAMMPS_DIR="$TADAH_PATH/LAMMPS" \
    -DTADAH_LAMMPS_LIB="$TADAH_PATH/build_lammps_mpi/liblammps_develop_mpi.a" \
    -DMPIEXEC_EXECUTABLE="$(dirname "$(command -v mpicxx)")/mpiexec"
cmake --build build_tadah_mpi -j

ctest --test-dir build_tadah_mpi --output-on-failure   # the whole suite
ctest --test-dir build_tadah_mpi -L mpi                # the multi-rank tests
```

LAMMPS builds with OpenMP when its configure finds it (`BUILD_OMP`), and
its static library then calls the OpenMP runtime: without
`-DTADAH_ENABLE_OPENMP=ON` linking `tadah` fails on `omp_get_num_threads`,
`GOMP_parallel` and the like (or configure LAMMPS with `-DBUILD_OMP=OFF`).
This LAMMPS, with ML-TADAH alone, needs nothing else. A static LAMMPS with
more packages does not bring the libraries they use: list them in
`TADAH_LAMMPS_LIBS` (a CMake list), as for the serial NFP build -- zlib
(and libzstd, when LAMMPS found it) for COMPRESS, libjpeg, libpng and zlib
for GRAPHICS (those it found), and
`build_lammps_mpi/voro_build-prefix/src/voro_build/src/libvoro++.a` for
VORONOI; the link errors name the ones that are missing.

The whole suite runs in the MPI build as well, each test program a
one-rank MPI job (test_hpo and test_tadah initialise MPI as `tadah`
does). It Skips, besides the optional-data cases above, every case that
starts `PC_ISOLATION PROCESS` workers, which the MPI build does not have
(`pc pool: in the MPI build the worker refuses to start` pins why): the
eleven `pc pool:` cases that build a worker pool and the two `Two runs in
one process` cases under `PROCESS`. Two things a test case does differ
between the builds, and the serial suite does not run the MPI build's: a
case that starts PROCESS workers calls `SKIP_WITHOUT_PC_PROCESS_WORKERS()`
(HPO `tests/pc_process_workers.h`) as its first statement, and a LAMMPS
instance a test creates comes from `LammpsRunner::create_instance`, which
takes no communicator and runs it on the build's (`hpo_lammps_comm()`:
the serial build's 0 is a null communicator under Open MPI, and LAMMPS
aborts). `hpo_mpi_build_tests_lint`, which runs in every build, reads HPO's
test sources as the MPI build compiles them and fails a case that breaks
either rule. The MPI build's test discovery runs test_hpo and test_tadah,
so a host where Open MPI's `MPI_Init` blocks (its hwloc GL component
waiting for an X display that accepts no connection) blocks the build:
set `HWLOC_COMPONENTS=-gl` (and `UCX_VFS_ENABLE=n` where UCX writes inotify
errors into the listing) for the build and the tests. `ctest -L mpi`
selects the multi-rank entries: the
`tadah.lammps_mpi_*` entries of every NFP build (`ctest -L lammps` above,
which run their own `lmp` and launcher) and the MPI-only entries, run
through CMake's `MPIEXEC_EXECUTABLE` and needing four process slots
(`MPIEXEC_MAX_NUMPROCS`): `hpo_anneal_mpi_matrix` (the PT/PA trajectories,
statuses and Corana census are the same at 1, 2 and 4 ranks),
`hpo_mpi_signal_<scenario>` (a signal that one rank receives during
rank 0's seed, T0 or pass-2 calibration, the `INIT_MODE JITTER AUTO`
calibration or a replica/walker epoch ends every rank `SHUTDOWN`, 128 +
the signal, at 1, 2 and 4 ranks) and `hpo_mpi_e2e_<case>` (`tadah hpo` on
1 and 2 ranks: the `RESTART` and gradient-search refusals, the LAMMPS
EAM fast kernels with the evaluated set the same on both and a global
best that rank 1 finds and publishes, the `RHOGATE`/`RHOBARGATE` counts
and the density report). `MPIEXEC_EXECUTABLE` must be the launcher of
the MPI the build links. CMake's FindMPI takes the first `mpiexec` on
`PATH` (or under `MPI_HOME`), whichever MPI it belongs to: with conda's
MPICH ahead of the system Open MPI on `PATH` it records conda's `mpiexec`
and still compiles with the system `mpicxx`, and that launcher starts
independent one-rank jobs, which the multi-rank tests detect and fail
on. Hence the `-DMPIEXEC_EXECUTABLE` above, the `mpiexec` next to
`mpicxx`; check it in `build_tadah_mpi/CMakeCache.txt`. The MPI build is
not part of the default presets.

**Pitfall:** MLIP redefines `add_test()` globally (to skip Gemmi's own
test) and forwards its arguments unquoted. In every `add_test()` that runs
after MLIP is configured (in the umbrella build: MLIP, MD, HPO, Tadah.LAMMPS
and the umbrella itself), an argument containing `;` is split in two and an
empty argument is dropped. Discovered entries are not affected.

For the HPO worked-example regression suite (added during the beta
prep — fast subset runs in ~5 s). It runs the `tadah` that `TADAH` names
(an absolute path: the suite runs it from each example directory), else
the first one on `PATH`, and writes its outputs into the example
directories:

```bash
TADAH="$TADAH_PATH/Tadah.MLIP/build/tadah" bash HPO/examples/run_reference_suite.sh --fast
# or
TADAH="$TADAH_PATH/Tadah.MLIP/build/tadah" bash HPO/examples/run_reference_suite.sh --full
```

The driver compares the example outputs against
`HPO/examples/<NN>/reference_fingerprint.json`. Per-example tolerance
overrides live in `HPO/examples/tools/tolerance_overrides.json`.

## Building the documentation

The Sphinx sources live in `docs/source` (Python requirements in
`docs/requirements.txt`). The build reads three inputs from outside
this repository. It never falls back to another branch: when an input
is missing it stops and lists every location it tried.

| Input | Where the build looks, in order |
|-------|---------------------------------|
| CORE sources (version, configuration keys, CLI pages) | `$TADAH_CORE_SOURCE_DIR`; the `CORE` checkout next to `Tadah.MLIP`; `$TADAH_PATH/CORE`; on GitLab CI and Read the Docs only, a shallow clone of the branch or tag being built, named the way cmake-tools' `tadah_get_git_branch` names it: `$CI_COMMIT_REF_NAME`, else `$READTHEDOCS_GIT_IDENTIFIER` (Read the Docs' `latest` and `stable` versions are named after no ref), `$READTHEDOCS_VERSION_NAME`, `$READTHEDOCS_VERSION` |
| HPO sources (ANNEAL key tables) | the same, with `$TADAH_HPO_SOURCE_DIR` and `HPO` |
| Doxygen XML (API pages) | `$TADAH_DOXYGEN_XML_DIR`; otherwise `build/docs/xml`, which `cmake --build build --target docs` writes. Its input is the `include/` directory of each module the build configures (`cmake/doxygen_input.cmake`): CORE, MODELS and MLIP, and HPO only with `TADAH_ENABLE_NFP`, so the Read the Docs build (NFP off) has no HPO API |

A variable that is set must be valid. A source override has to name a
checkout that contains the file the build reads, and the XML override
has to contain `index.xml`; neither is a hint to be skipped.

An offline build from a workspace, keeping the Doxygen output out of
the source tree (`build_dir` is any configured `Tadah.MLIP` build; its
`docs/Doxyfile` writes `docs/xml` relative to where Doxygen runs, and
Doxygen does not create the `docs` directory itself):

```bash
mkdir -p "$TMPDIR/doxy/docs"
(cd "$TMPDIR/doxy" && doxygen "$build_dir/docs/Doxyfile")
TADAH_DOXYGEN_XML_DIR="$TMPDIR/doxy/docs/xml" \
  sphinx-build -W --keep-going -b html Tadah.MLIP/docs/source "$TMPDIR/docs_html"
python3 -m unittest discover -s Tadah.MLIP/docs/tools/tests -v
```

The unit tests cover the input lookup (`docs/source/config/source_locator.py`),
the `hpo.html` redirect check, the configuration-keys page generator
(`docs/source/config/read_toml.py`) and the CLI page generator
(`docs/source/cli/generate_cli.py`). The GitLab `docs_sphinx` job runs them
and the same build, with all three variables set explicitly, in a virtual
environment of its own into which it installs `docs/requirements.txt` every
time (`python3 -m venv`: the runner image needs Python's `venv` module), so a
Sphinx the runner has is not used (`docs/tools/tests/test_ci_docs_job.py`).

The same unit tests are the CTest entry `tadah_docs_tools` (label `docs`,
so `ctest -L docs` runs them) in every build configured with
`-DBUILD_TESTING=ON`, which therefore needs a Python 3 interpreter. With
`TADAH_ENABLE_NFP` it needs Python 3.11 or later: HPO's tests declare it
(`HPO/cmake/hpo_test_python.cmake`), because their registry drift gate
`hpo_anneal_registry_sync` reads TOML with `tomllib`, and the configure step
stops on an older interpreter; `ctest -R hpo_test_python_minimum` checks
that. The cases that need `node`, `sphinx-build` or the Python modules `sphinx`,
`sphinx_rtd_theme`, `yaml` (PyYAML) or `pytomlpp` skip themselves when one
is missing. CTest then reports the entry as *Skipped*, not *Passed*, and its
output names the skipped cases, so a pass means every case ran.

`hpo.html`, the Nested Fitting page before 1.6.0, is now a redirect
(`docs/source/_extra/hpo.html`) to the pages it was split into. After
every build with the `html` builder, the `check_redirect` Sphinx
extension (`docs/tools/check_redirect.py`, listed in `conf.py`) checks
that each anchor of the old page (`docs/tools/hpo_legacy_anchors.txt`)
still reaches a page that has it, and any miss is a build warning. If you
rename or move a section of the Nested Fitting pages, add a route for its
old anchor to the routes table in `hpo.html`. The page links to the flat
page names of the `html` builder; other HTML builders (`dirhtml`,
`singlehtml`, `epub`) copy it too, and the build then warns that the
redirect is not checked. `.readthedocs.yaml` keeps it out of the Read the
Docs search index.

## Coding style

There is no committed `.clang-format`. Match the surrounding code:
two-space indent, braces on the same line for free functions and on
the next line for class definitions. Headers use `#pragma once`.
Avoid trailing whitespace; UTF-8, LF line endings, terminal newline.

`pre-commit` config is optional but recommended (see
[`.pre-commit-config.yaml`](.pre-commit-config.yaml) — codespell,
trailing-whitespace, end-of-file-fixer, no formatter so we don't
churn unrelated lines).

## Submitting a merge request

1. Fork the relevant project on `git.ecdf.ed.ac.uk/tadah/<module>` (or,
   for maintainers, push a topic branch).
2. Branch off `v1-dev` (`git checkout -b feature/short-name v1-dev`).
3. Keep the change focused — one logical change per MR. Cross-repo
   work that needs coordinated tags goes through the multi-repo
   release process described in `RELEASING.md`.
4. Add or update tests. Pure-refactor MRs are fine, but a behaviour
   change without a test that pins it down is a hard sell.
5. Run `ctest` in the umbrella build. Ensure the CI pipeline on your
   branch is green; the `lint_codespell` job and the existing
   `build_debug` / `build_release` / `compile_lammps_with_tadah` jobs
   must pass.
6. Write a clear MR description: motivation, summary of the change,
   anything a reviewer should pay attention to, anything explicitly
   left out of scope.

A reviewer looks for:

- Does the change do what the description says, and only that?
- Are the tests at the right level (unit vs. integration vs. example)?
- Public API or config keyword changes — are they documented and
  backwards-compatible, or explicitly called out as breaking?
- For numerical changes, is determinism preserved (same SEED → same
  output)? Cross-repo numerical drift is the most expensive class of
  regression; see the HPO `reference_fingerprint.json` mechanism for
  how we catch it.

## Code of Conduct

Participation in the Tadah! project is governed by the
[Code of Conduct](CODE_OF_CONDUCT.md). Reports of unacceptable
behaviour are handled through a confidential GitLab issue (see
[SECURITY.md](SECURITY.md) for the channel and process).
