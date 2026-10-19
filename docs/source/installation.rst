.. _installation:

Installation
============

This page provides step-by-step installation instructions for:

- **Tadah!MLIP** (ML potential training; provides the ``tadah`` CLI)
- **ML‑TADAH for LAMMPS** (Tadah!LAMMPS; adds the ``pair_style tadah`` pair style to LAMMPS)

More detailed and up-to-date guidance (including platform-specific notes) is at:
https://tadah.readthedocs.io/

.. _requirements:

Requirements
------------

Tadah!MLIP and the ML‑TADAH LAMMPS integration use **CMake** for configuration/build.

General requirements:

- A **C++17**-capable compiler toolchain whose standard library implements
  floating-point ``std::to_chars`` (GCC 11+ / libstdc++ 11+, Clang with
  libc++ 14+ or libstdc++ 11+, MSVC 2019 16.4+); Tadah! uses it to write
  numbers losslessly
- **CMake** 3.15 or later; 3.24 with ``TADAH_ENABLE_NFP`` (Tadah.HPO declares
  it) and 3.16 with ``BUILD_TESTING`` (CTest reports a skipped test through
  the ``SKIP_REGULAR_EXPRESSION`` test property, which it has from 3.16; the
  configure step stops on an older CMake, saying so)
- **LAPACK**
- **libcurl** development files (``libcurl4-openssl-dev`` on Debian and
  Ubuntu, ``libcurl-devel`` on Fedora and Red Hat): Tadah! reads structures
  from the AFLOW, Materials Project, COD and NOMAD databases with it, and
  CMake stops with an error without them
- Internet access during installation (some dependencies may be fetched at configure/build time)

LAMMPS-specific notes:

- Modern LAMMPS builds use **C++17**; the ML‑TADAH integration assumes a LAMMPS toolchain that can build LAMMPS successfully.

Installing Tadah!MLIP Software
------------------------------

Tadah!MLIP provides the command-line interface binary, ``tadah``, for developing machine-learning interatomic potentials.

Obtaining Source Code
.....................

Tadah!MLIP is cloned at a release tag, or at ``v1-dev``, the development
branch of the 1.x line, from which the releases are tagged:

.. code-block:: bash

    # the 1.6.0 release
    git clone -b v1.6.0 https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

    # or the development line
    git clone -b v1-dev https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

Both commands name the branch or tag: a plain ``git clone`` checks out the
repository's default branch, which need not be ``v1-dev``. CMake fetches
the other Tadah! modules at the same ref. It finds a branch by itself, but
a tag checkout has no branch, so a release clone is configured with
``-DTADAH_GIT_BRANCH=v1.6.0`` (:ref:`installation_git_branch`).

.. _building_tadah:

Configuration and Compilation
.............................

Tadah!MLIP uses CMake for configuration and a standard build tool for compilation.

The executable ``tadah`` is installed in the ``bin`` directory by default (``${CMAKE_INSTALL_PREFIX}/bin``).
After a successful compilation (before installation), the ``tadah`` binary should also be present inside the build directory (commonly as ``build/tadah`` or under a subdirectory like ``build/bin/``, depending on the platform and the CMake generator).

Using the provided CMake defaults preset (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Tadah!MLIP ships a CMake *initial cache* file with Linux end-user defaults at
``cmake/presets/tadah-linux-defaults.cmake``. It serves as-is, or edited to match
local preferences and reused for future builds.

.. code-block:: bash

    # Navigate to the project directory
    cd tadah.mlip

    # Configure a new build directory using the defaults preset
    cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake

    # Compile and install (uses the configured build tool, e.g. Make)
    cmake --build build --parallel
    cmake --install build

``-D`` options appended to the command override individual settings without editing the preset file (:ref:`configuration_options`), e.g.:

.. code-block:: bash

    cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake \
        -DTADAH_ENABLE_OPENMP=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

The build can also be configured and compiled without the preset:

.. code-block:: bash

    # Navigate to the project directory
    cd tadah.mlip

    # Create and move into the build directory
    mkdir build && cd build

    # Configure with CMake
    cmake ..

    # Compile and install
    make && make install

.. _configuration_options:

Configuration Options
.....................

.. note::

   With ``-C cmake/presets/tadah-linux-defaults.cmake``, the values in that file become the
   initial defaults for the new build directory (e.g. it sets conservative end-user defaults
   such as disabling optional features). These defaults are customised either by (1) appending
   ``-D<option>=<value>`` to the CMake command, or (2) editing the preset file itself and
   reusing it for future builds.

Installation location
^^^^^^^^^^^^^^^^^^^^^

``CMAKE_INSTALL_PREFIX``, set during configuration, changes the installation location from the
default (typically ``/usr/local`` on Linux):

.. code-block:: bash

    cmake .. -DCMAKE_INSTALL_PREFIX=/your/path

Choosing a build type
^^^^^^^^^^^^^^^^^^^^^

On Linux, CMake commonly uses a *single-config* generator (Ninja or Unix Makefiles). In this case the build type is selected at **configure time** using ``CMAKE_BUILD_TYPE``.

Common values:

- ``Release``: fastest (recommended for production runs)
- ``Debug``: best for debugging
- ``RelWithDebInfo``: optimised with debug info (default)
- ``MinSizeRel``: smaller binaries

Recommended workflow: one build directory per build type:

.. code-block:: bash

    cd tadah.mlip

    # Release build
    cmake -S . -B build-release -C cmake/presets/tadah-linux-defaults.cmake \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build-release --parallel
    cmake --install build-release

.. code-block:: bash

    # Debug build
    cmake -S . -B build-debug -C cmake/presets/tadah-linux-defaults.cmake \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-debug --parallel

Alternative: the build type in a custom preset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A copy of the provided preset can also hold a default build type:

.. code-block:: cmake

    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)")

These CMake options customise the Tadah!MLIP build:

- ``TADAH_ENABLE_OPENMP``: Enables OpenMP for parallelisation (default: OFF).
- ``TADAH_MARCH_NATIVE``: Uses native optimisations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enables fast math optimisations (default: OFF).
- ``TADAH_ENABLE_NFP``: Enables the Nested Fitting Procedure (default: OFF; :ref:`installation_nfp`). A ``tadah`` built without it refuses ``tadah hpo`` and ``TASK hpo`` before any task runs, ``tadah hpo needs the Nested Fitting Procedure, which this tadah was built without; reconfigure with -DTADAH_ENABLE_NFP=ON (see Installation)``, and its ``tadah -h`` says so on the ``hpo`` line. Such a build has no compact fitter either: ``tadah refit`` refuses ``REFIT_MODE compact``, the default, so its refit configurations set ``REFIT_MODE dense`` (as without ``TADAH_ENABLE_CERES``, below).
- ``TADAH_LAMMPS_DIR``: Absolute path to the LAMMPS directory (required by ``TADAH_ENABLE_NFP``).
- ``TADAH_LAMMPS_LIB``: Absolute path to the LAMMPS library compiled with Tadah!LAMMPS (required by ``TADAH_ENABLE_NFP``).
- ``TADAH_LAMMPS_LIBS``: Extra link items for the LAMMPS library, as a CMake list (e.g. ``"-lz;-lzstd"``); needed mainly for a static LAMMPS library, whose own dependencies are not propagated (optional, used by ``TADAH_ENABLE_NFP``).
- ``TADAH_ENABLE_CERES``: Builds the Ceres Solver optimiser family of ``tadah hpo`` (``LIB CERES``) (default: ON; only relevant with ``TADAH_ENABLE_NFP``). ``PARAM fd_kernel ceres``, Ceres' finite-difference step rule, does not need it: Tadah evaluates that rule itself (before 1.6.0 it ran inside Ceres and needed this option). The compact fit of ``tadah refit`` does need it: without it ``tadah refit`` refuses ``REFIT_MODE compact``, the default, so a refit configuration of such a build sets ``REFIT_MODE dense``.
- ``TADAH_GIT_BRANCH``: The git branch or tag the Tadah! modules are fetched at (detected when not given, :ref:`installation_git_branch`; a release source tarball needs it, e.g. ``v1.6.0``).
- ``TADAH_HPO_MPI``: Distributes the replicas of the ``TADAH:ANNEAL_PT`` / ``TADAH:ANNEAL_PA`` optimisers of ``tadah hpo`` over MPI ranks (default: OFF; only relevant with ``TADAH_ENABLE_NFP``; needs an MPI build of LAMMPS; :ref:`installation_hpo_mpi`).

Further CMake options:

- ``CMAKE_INSTALL_PREFIX``: Sets the install destination of ``make install``. A common choice is ``-DCMAKE_INSTALL_PREFIX=~/.local``.
- ``BUILD_SHARED_LIBS``: Whether to build shared or static Tadah!MLIP libraries (default: OFF).
- ``BUILD_TESTING``: Includes tests (default: OFF). Needs CMake 3.16 or later, and a Python 3 interpreter, which runs the unit tests of the documentation build tools; with ``TADAH_ENABLE_NFP``, Python 3.11 or later, for the Nested Fitting tests (their key-registry check reads TOML with ``tomllib``). The configure step stops when it finds no suitable interpreter.

Each option is appended to the ``cmake`` command with the ``-D`` prefix and a value of ``ON``, ``OFF``, or a path, e.g.:

.. code-block:: bash

    cmake .. \
        -DTADAH_ENABLE_OPENMP=ON \
        -DTADAH_MARCH_NATIVE=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

One call takes several ``-D<option>=<value>`` pairs.

.. _installation_git_branch:

Which ref the Tadah! modules are fetched at
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Tadah!MLIP fetches the Tadah! modules it builds on from their git
repositories (Tadah.MLIP, Tadah.MODELS and Tadah.CORE; with
``TADAH_ENABLE_NFP`` also Tadah.HPO, Tadah.MD and Tadah!LAMMPS), all at
one git branch or tag, ``TADAH_GIT_BRANCH``.  There is no fixed default:
``-DTADAH_GIT_BRANCH=<branch or tag>`` is used when given, and otherwise
the first of these (cmake-tools' ``tadah_get_git_branch``):

#. ``$CI_COMMIT_REF_NAME`` on GitLab CI (the branch, or the tag of a tag
   pipeline);
#. on Read the Docs, ``$READTHEDOCS_GIT_IDENTIFIER`` (the branch or tag it
   checked out; its ``latest`` and ``stable`` versions are named after no
   ref), then ``$READTHEDOCS_VERSION_NAME`` and ``$READTHEDOCS_VERSION``;
#. the upstream branch of the checkout of the project CMake was started on
   (``origin/v1-dev`` gives ``v1-dev``), else that checkout's local branch.

A Tadah!MLIP clone on ``v1-dev`` therefore fetches every module at
``v1-dev``.  A detached HEAD (a clone of a release tag, ``git clone -b
v1.6.0``, is one), or a release source tarball (which has no ``.git``),
has no branch: CMake warns, and the ref is passed explicitly, for a
release its tag:

.. code-block:: bash

    cmake -S . -B build -DTADAH_GIT_BRANCH=v1.6.0

Building, installing, testing and generating the docs from an out-of-tree
``build`` directory:

.. code-block:: bash

    cmake --build build --parallel        # compile
    cmake --install build                 # install to the chosen prefix
    ctest --test-dir build                # run tests (requires -DBUILD_TESTING=ON)
    cmake --build build --target docs     # generate local API docs (requires Doxygen)

The ``docs`` target writes the Doxygen XML to ``build/docs/xml``, where the
Sphinx build of this site looks for it by default. ``CONTRIBUTING.md``
(*Building the documentation*) lists every input the Sphinx build reads and
the variables that name them explicitly (``TADAH_DOXYGEN_XML_DIR``,
``TADAH_CORE_SOURCE_DIR``, ``TADAH_HPO_SOURCE_DIR``).

.. _gofasterstripe:

Go Faster Stripe - Desktop Version
..................................

OpenMP is supported where the system provides it. A Tadah!MLIP build with OpenMP support performs best with as many threads as CPU cores:

.. code-block:: bash

    export OMP_NUM_THREADS=num_of_cores

.. _installation_hpo_mpi:

MPI-Parallel Hyperparameter Optimisation
........................................

Training (``tadah train``) and the other ``tadah`` commands run in one
process; OpenMP threads (:ref:`gofasterstripe`) occupy the cores of a
node. MPI parallelism is available for hyperparameter optimisation: with
``-DTADAH_HPO_MPI=ON`` the ``TADAH:ANNEAL_PT`` and ``TADAH:ANNEAL_PA``
optimisers of ``tadah hpo`` distribute their replicas over MPI ranks, which
lets one run span several nodes. It needs ``-DTADAH_ENABLE_NFP=ON``
(:ref:`installation_nfp`) and an MPI build of LAMMPS with ML‑TADAH, both
built with the same MPI library.

**1. LAMMPS with MPI.** LAMMPS is configured with ML‑TADAH as in
:ref:`installation_lammps`, with ``-DBUILD_MPI=ON`` and a library name
that contains ``mpi`` added (``-DLAMMPS_MACHINE=mpi`` names it
``liblammps_mpi.a``), and its library target is built:

.. code-block:: bash

    cd /path/to/lammps
    cmake -S cmake -B build-mpi \
      -DBUILD_MPI=ON -DLAMMPS_MACHINE=mpi \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps \
      -DTADAH_GIT_BRANCH=v1-dev
    cmake --build build-mpi --target lammps --parallel

**2. Tadah!MLIP with TADAH_HPO_MPI.** ``TADAH_LAMMPS_DIR`` and
``TADAH_LAMMPS_LIB`` point at that LAMMPS, and OpenMP is enabled:

.. code-block:: bash

    cmake -S . -B build-mpi -C cmake/presets/tadah-linux-defaults.cmake \
        -DTADAH_ENABLE_NFP=ON \
        -DTADAH_HPO_MPI=ON \
        -DTADAH_ENABLE_OPENMP=ON \
        -DTADAH_LAMMPS_DIR=/path/to/lammps \
        -DTADAH_LAMMPS_LIB=/path/to/lammps/build-mpi/liblammps_mpi.a
    cmake --build build-mpi --parallel

LAMMPS builds with OpenMP when its configure finds it (``BUILD_OMP``),
and a static LAMMPS library then calls the OpenMP runtime: without
``-DTADAH_ENABLE_OPENMP=ON`` (the defaults preset leaves it ``OFF``)
linking ``tadah`` fails on ``omp_get_num_threads``, ``GOMP_parallel`` and
the like; a LAMMPS configured with ``-DBUILD_OMP=OFF`` avoids this. Nor
does a static LAMMPS library bring the libraries that its packages use.
The LAMMPS of step 1, with ML‑TADAH alone, needs none; one with
COMPRESS needs zlib (and libzstd, when LAMMPS found it), one with GRAPHICS
libjpeg, libpng and zlib (those it found), and one with VORONOI that
build's own ``build-mpi/voro_build-prefix/src/voro_build/src/libvoro++.a``.
``TADAH_LAMMPS_LIBS`` adds them, as in the serial build (step 2 of
:ref:`installation_nfp`), for example
``-DTADAH_LAMMPS_LIBS="-lzstd;-lz;-lpng;-ljpeg;/path/to/lammps/build-mpi/voro_build-prefix/src/voro_build/src/libvoro++.a"``
for all three; the link errors name the ones that are missing.

The executable is still ``tadah`` and runs without ``mpirun`` too (a
one-rank job; Open MPI then makes its session directory under ``TMPDIR``,
which must be a writable directory). The configure step refuses a LAMMPS
library whose file name lacks ``mpi``, because the serial LAMMPS library carries MPI stub symbols
that collide with a real MPI library at link time;
``-DTADAH_HPO_MPI_SKIP_LIB_CHECK=ON`` lifts this check for an MPI build of
LAMMPS that is only named differently. The MPI library must provide ``MPI_THREAD_MULTIPLE``.

**3. Launching.** ``tadah`` starts with ``srun`` or with the ``mpirun`` /
``mpiexec`` of the MPI library it links, the one next to the
``mpicxx`` that CMake recorded as ``MPI_CXX_COMPILER`` in
``build-mpi/CMakeCache.txt``. A launcher of another MPI — one that comes
first on ``PATH``, from a conda environment for example — does not start
one job of N ranks but N independent one-rank runs, which all write the
unsuffixed ``hpo_run.log`` and result files of a one-rank run. The
requirements and an ARCHER2 job script are in :ref:`tadah_anneal_mpi`.

**4. Testing an MPI build.** With ``-DBUILD_TESTING=ON`` the whole suite
runs in the MPI build too (``ctest --test-dir build-mpi``): each test
program is a one-rank MPI job. The cases that build a ``PC_ISOLATION
PROCESS`` worker pool or ask for that mode -- those of the worker pool, and
two HPO runs under ``PROCESS`` -- are *Skipped* there (the MPI build has
``PC_ISOLATION THREAD`` only), as are the optional-data cases of the serial
build. If the
build stops while it lists the tests (the MPI build's test programs
initialise MPI to list them), Open MPI's ``MPI_Init`` may be waiting for an
X display through its hwloc GL component: ``HWLOC_COMPONENTS=-gl``, set
for the build and the tests, avoids this. The multi-rank
tests of ``tadah hpo`` exist only in this build: ``ctest -L mpi`` runs them
through ``MPIEXEC_EXECUTABLE`` at 1, 2 and 4 ranks and needs four process
slots
(``MPIEXEC_MAX_NUMPROCS``). CMake takes the first ``mpiexec`` on ``PATH``
for ``MPIEXEC_EXECUTABLE``, whichever MPI it belongs to; when another
MPI's comes first, ``-DMPIEXEC_EXECUTABLE=/path/to/mpiexec`` at configure
time names the launcher of step 3 (the wrong one starts one-rank jobs, and the
multi-rank tests fail). They check the rank invariance of ``ANNEAL_PT``
/ ``ANNEAL_PA`` (``hpo_anneal_mpi_matrix``), a signal that one rank
receives (``hpo_mpi_signal_*``) and ``tadah hpo`` end to end on 1 and 2
ranks (``hpo_mpi_e2e_*``). The serial build runs none of them. ``ctest -L
mpi`` also selects the ``pair_style tadah`` entries of
:ref:`installation_lammps_mpi`, which every NFP build has: they run an MPI
``lmp`` of their own and are *Skipped* unless it is named.

.. note::

   The legacy MPI training build (``-DTADAH_BUILD_MPI=ON``, the
   ``tadah_mpi`` executable and its ScaLAPACK solver) was removed in 1.6.0.
   Configuring with ``-DTADAH_BUILD_MPI=ON`` stops with an error that
   points to ``TADAH_HPO_MPI``; ``OFF`` is accepted, so older presets still
   work. Its configuration keys :ref:`MPIWPCKG`, :ref:`MBLOCK` and
   :ref:`NBLOCK` are deprecated: files that contain them still load, with a
   warning on standard error (once per key and process; ``pair_style
   tadah`` writes it as a LAMMPS warning, in ``log.lammps``, each time it
   loads such a potential), and the values are ignored. The ``PC_LAMMPS --ncpu`` option of
   ``tadah hpo`` was removed with it (:ref:`pc_lammps_options`).

.. _installation_nfp:

Building Tadah!MLIP with Nested Fitting Procedure support
.........................................................

.. warning::

   The NFP module requires a LAMMPS build that includes ML‑TADAH and has LAMMPS exceptions enabled (``LAMMPS_EXCEPTIONS``).
   On recent LAMMPS versions this is often enabled by default; on older versions it may need to be set explicitly in the LAMMPS build configuration. :ref:`installation_lammps` describes building LAMMPS with ML‑TADAH.

Building Tadah!MLIP with NFP is more involved, and for this reason it is not built by default. The notes below describe what such a build needs.

A Tadah!MLIP build with NFP takes these steps:

1. **Compiling LAMMPS with ML‑TADAH enabled** (:ref:`installation_lammps`).

   **Shared vs. Static builds**

   A shared-library build of LAMMPS is often the simplest to link against Tadah!MLIP.

   **Serial vs. MPI**

   - The default (non-MPI) Tadah!MLIP build takes a serial LAMMPS build.
   - The MPI-parallel HPO build (``TADAH_HPO_MPI``, :ref:`installation_hpo_mpi`) takes an MPI build of LAMMPS.

   The LAMMPS build documentation describes how each build system produces a LAMMPS library (shared/static):
   https://docs.lammps.org/Build_cmake.html

2. **Configuring Tadah!MLIP** (:ref:`building_tadah`) with these options added:

   - ``-DTADAH_ENABLE_NFP=ON``
   - ``-DTADAH_LAMMPS_DIR=/path/to/lammps`` (LAMMPS root directory)
   - ``-DTADAH_LAMMPS_LIB=/full/path/to/liblammps_name.ext`` (full path to the built LAMMPS library file)
   - optionally ``-DTADAH_LAMMPS_LIBS="-lz;-lzstd"`` when linking a static LAMMPS library fails with
     undefined references to its dependencies (e.g. ``gzflush`` needs ``-lz``)
   - ``-DTADAH_ENABLE_OPENMP=ON`` for a static LAMMPS library built with OpenMP (``BUILD_OMP``, on
     when the LAMMPS configure finds OpenMP): it calls the OpenMP runtime, and linking otherwise
     fails on ``omp_get_num_threads``, ``GOMP_parallel`` and the like
   - optionally ``-DTADAH_ENABLE_CERES=OFF`` to build without the Ceres Solver (``LIB CERES`` is then
     unavailable, and so is ``tadah refit``'s compact fit: such a build refuses ``REFIT_MODE compact``,
     the default, and needs ``REFIT_MODE dense``)


.. _installation_lammps:

Installing ML‑TADAH for LAMMPS (Tadah!LAMMPS)
---------------------------------------------

ML‑TADAH adds the **``pair_style tadah``** pair style to LAMMPS.
It does not require Tadah!MLIP to be installed.

.. important::

   ML‑TADAH / Tadah!LAMMPS is **not part of the official LAMMPS distribution**.
   Tadah!LAMMPS is cloned separately and enabled during the LAMMPS build.

The official LAMMPS documentation gives the general LAMMPS build instructions and prerequisites:

- CMake build: https://docs.lammps.org/Build_cmake.html

There are two ways to build LAMMPS with ML‑TADAH:

1. **CMake build (recommended)**
2. **Traditional make-based build (deprecated)**

CMake build with ML‑TADAH (recommended)
.......................................

This method integrates ML‑TADAH into a normal LAMMPS CMake configure/build, by adding a small set of extra CMake options.

**Step 1 — Cloning LAMMPS**

Any LAMMPS version appropriate for the project serves; the example below clones the stable branch:

.. code-block:: bash

    git clone -b stable --depth 1 https://github.com/lammps/lammps.git /path/to/lammps

**Step 2 — Cloning Tadah!LAMMPS**

Tadah!LAMMPS is cloned anywhere on the filesystem, at a release tag or at
``v1-dev``, the development line (a plain ``git clone`` checks out the
repository's default branch, which need not be either):

.. code-block:: bash

    # the 1.6.0 release
    git clone -b v1.6.0 https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git /path/to/tadah.lammps

    # or the development line
    git clone -b v1-dev https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git /path/to/tadah.lammps

**Step 3 — Configuring LAMMPS with ML‑TADAH enabled**

The usual LAMMPS CMake configure command runs from the **LAMMPS root directory** (``/path/to/lammps``), with these four options **added**:

* ``-DPKG_ML-TADAH=ON``
* ``-DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake``
* ``-DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps``
* ``-DTADAH_GIT_BRANCH=<the branch or tag checked out in Step 2>``

Minimal example, for a ``v1-dev`` checkout:

.. code-block:: bash

    cd /path/to/lammps
    cmake -S cmake -B build \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps \
      -DTADAH_GIT_BRANCH=v1-dev

``TADAH_GIT_BRANCH`` is the git branch or tag the Tadah! modules are
fetched at.  Tadah!LAMMPS pulls ``Tadah.MD`` and, through it,
``Tadah.MODELS`` and ``Tadah.CORE``; the Tadah!LAMMPS sources themselves
are whatever is checked out at ``TADAH_LAMMPS_SOURCE_DIR``.
``TADAH_GIT_BRANCH`` names the ref of that checkout, so that both come
from the same line:
``-DTADAH_GIT_BRANCH=v1-dev`` for a ``v1-dev`` clone,
``-DTADAH_GIT_BRANCH=v1.6.0`` for a clone of the 1.6.0 release.

Without it the ref is detected (:ref:`installation_git_branch`) from the
checkout of the project CMake was started on, which in this build is
**LAMMPS**: a LAMMPS branch (``stable``, ``develop``, …) that has nothing
to do with the Tadah!LAMMPS checkout, or no ref at all for a LAMMPS
release tarball or a detached HEAD (CMake warns).  The LAMMPS build
therefore always needs ``-DTADAH_GIT_BRANCH``.

Example using the LAMMPS GCC preset and building a serial binary, for a
``v1-dev`` checkout:

.. code-block:: bash

    cd /path/to/lammps
    cmake -C cmake/presets/gcc.cmake -S cmake -B build \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps \
      -DTADAH_GIT_BRANCH=v1-dev \
      -DLAMMPS_MACHINE=serial

**Step 4 — Building**

.. code-block:: bash

    cmake --build build

Parallel builds are supported. A typical Linux choice is:

.. code-block:: bash

    cmake --build build --parallel $(nproc)

After building, the produced LAMMPS executable from this build directory includes ML‑TADAH and supports ``pair_style tadah``.
It is ``build/lmp``, or ``build/lmp_<machine>`` with
``-DLAMMPS_MACHINE=<machine>`` (``lmp_serial`` in the preset example
above); ``cmake --build build --target lmp`` builds it alone, and
``cmake --build build --target lammps`` the library alone (what
:ref:`installation_nfp` links).

**Advanced integration options**

The CMake integration also supports additional options (e.g. verbosity controls and custom style registration lists), documented in:

- ``tadah.lammps/integration/README.md`` (in the Tadah!LAMMPS checkout)

.. _installation_lammps_mpi:

Running pair_style tadah on several MPI ranks
.............................................

``pair_style tadah`` runs domain-decomposed like any LAMMPS pair style.
LAMMPS is configured with the options of Step 3 and ``-DBUILD_MPI=ON``
(CMake finds the MPI of ``mpicxx``), and ``lmp`` is built and started with
the ``mpirun`` / ``mpiexec`` of that MPI (or ``srun``):

.. code-block:: bash

    cd /path/to/lammps
    cmake -S cmake -B build-mpi -DBUILD_MPI=ON -DLAMMPS_MACHINE=mpi \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps \
      -DTADAH_GIT_BRANCH=v1-dev
    cmake --build build-mpi --target lmp --parallel
    mpiexec -n 4 /path/to/lammps/build-mpi/lmp_mpi -in in.ml

On several ranks the energies, forces, per-atom energies and the virial
are those of the serial run up to round-off (the order in which
contributions are summed differs). This was checked on 2 and 4 ranks,
also with sub-domains narrower than the cutoff, for the generic many-body
kernel of, for example, the *Quick Start* potential, the ``EAMFAST``
kernel of a linear ``DM_REAM`` potential, the full-list kernel of a
non-linear model and the kernels of a pair potential, and on 8 ranks for
all but the pair kernels. For a ``DIMER`` potential (*Tadah!N2*,
:ref:`mlips`) it was checked at step 0 only, on 2 and 4 ranks (also in a
``4 1 1`` grid), where the per-atom energies also sum to the energy; its
dynamics on several ranks were not. A ``DIMER`` potential needs ``newton
off``, whatever its model; any other potential runs with ``newton on`` or
``off`` when its model is linear and needs ``newton on`` when it is
non-linear.

Runs on different numbers of ranks start from the same state only when
the input makes them: ``create_atoms`` numbers the atoms rank by rank, and
``velocity all create ...`` with its default ``loop all`` assigns the
velocities by atom ID, so each lattice site starts with a velocity that
depends on the number of ranks. The trajectories then differ from the
first step (the *Quick Start* ``in.ml`` at 1, 2 and 4 ranks: 415, 419 and
426 K after 100 steps), though each run conserves its energy. Runs on
different rank counts compare with ``loop geom`` (velocities from the
positions), or when every run starts from one data file: with ``loop
geom`` the three ``in.ml`` runs print the same thermo rows for all 1000
steps.

The test suite checks this agreement when it is given an MPI ``lmp``
(``tadah.lammps_mpi_*``, described in ``CONTRIBUTING.md``, *Running
tests*): a Tadah!MLIP build with tests and NFP (:ref:`installation_nfp`),
configured with ``-DTADAH_LMP_MPI=/path/to/lammps/build-mpi/lmp_mpi`` and
``-DTADAH_LMP_MPIEXEC=/path/to/mpiexec``, the launcher of that MPI, runs
them with ``ctest -L lammps``; without them the entries are *Skipped*.

Traditional make-based build
............................

The classic LAMMPS ``make`` workflow is **not supported** for
Tadah!LAMMPS in this release. The CMake build documented above replaces it.

Libraries and C++ Headers
-------------------------

.. note::
   This section is relevant only to the Tadah!API. The command line interface (CLI) needs none of it.

Tadah!API installs its headers to
``${CMAKE_INSTALL_PREFIX}/include`` and its libraries to
``${CMAKE_INSTALL_PREFIX}/lib`` (or ``lib64``). If the prefix is a standard
location such as ``/usr`` or ``/usr/local`` nothing extra is required:

.. code-block:: bash

    g++ my_code.cpp -ltadah.core -o my_code  # headers and libs found automatically

Non-standard prefix (e.g. ``$HOME/.local``)
...........................................
When Tadah! is installed in a private directory, the compiler and linker need
a hint. The quickest way is the include and library paths on the command
line:

.. code-block:: bash

    g++ my_code.cpp \
        -I$HOME/.local/include \
        -L$HOME/.local/lib \
        -ltadah.mlip -o my_code

Run-time resolution
...................
``LD_LIBRARY_PATH`` tells the dynamic loader where to find the shared object
after the executable has been built:

.. code-block:: bash

    export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH
    ./my_code

Optional convenience variables
..............................
Environment variables, set once for the shell (or in a module file), save
repeating the ``-I``/``-L`` flags:

.. code-block:: bash

    # compile-time
    export CPATH=$HOME/.local/include:$CPATH
    export LIBRARY_PATH=$HOME/.local/lib:$LIBRARY_PATH

    # run-time
    export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH

These variables are best kept to the narrowest scope possible (script, shell
session, or environment module), which avoids accidental conflicts with other
software.

Compiled Libraries
..................

The following libraries are produced, depending on the build settings:

- ``libtadah.core`` — always built.
- ``libtadah.models`` — always built.
- ``libtadah.md`` — always built.
- ``libtadah.mlip`` — always built.
- ``libtadah.hpo`` — built only when ``-DTADAH_ENABLE_NFP=ON`` (also in the
  MPI-parallel HPO build, ``-DTADAH_HPO_MPI=ON``).

.. _compiling_and_linking:

Compiling and Linking with Library
..................................

C++ library use needs C++17 or higher. Library headers are included with the prefix ``tadah/``, and the relevant libraries are linked. For ``libtadah.mlip`` it is often the case that both ``libtadah.models`` and ``libtadah.core`` must be linked as well.

.. code-block:: c++

    #include <tadah/mlip/atom.h>

Linking with ``libtadah.mlip``:

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp -ltadah.mlip

If the library is outside the standard path:

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp \
        -I/home/$USER/.local/include \
        -L/home/$USER/.local/lib \
        -ltadah.mlip

.. _troubleshooting:

Troubleshooting
---------------

#. **CMake not found / version too old**

   The build needs an installed, up-to-date CMake (3.15 or later; 3.24 with ``TADAH_ENABLE_NFP``, 3.16 with ``BUILD_TESTING``). On Linux, the package manager can often install or upgrade CMake; the CMake website also offers binaries.

#. **Compiler not found / version too old**

   The C++ compiler must support C++17, including floating-point ``std::to_chars`` in its standard library (e.g., GCC 11+, Clang with libc++ 14+ or libstdc++ 11+, MSVC 2019 16.4+); an older compiler needs an update.

#. **LAPACK not found**

   The package manager installs the LAPACK development files (e.g., ``liblapack-dev`` on Debian/Ubuntu).

#. **CMake configuration errors related to missing dependencies**

   The CMake output names the missing dependencies; the required libraries or development packages then need installing.

#. **Build errors during compilation**

   The compiler output gives the specific error messages. Common causes are a dependency that is not correctly installed and a compiler without the required C++ standard.

#. **Permission denied** when running ``make install``

   A user-writeable prefix avoids it:

   .. code-block:: bash

      cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
      make install

#. **Shared object ImportError** when running ``tadah``

   ``LD_LIBRARY_PATH`` needs the library directory:

   .. code-block:: bash

      export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/your/path/

#. **LAMMPS build issues**

   The first check is that the toolchain builds LAMMPS on its own, following the LAMMPS build documentation:

   - https://docs.lammps.org/Build_cmake.html

   The second check covers the ML‑TADAH integration steps in :ref:`installation_lammps`.

#. **Other issues**

   Problems not listed here go to the contact address in :ref:`contact`.
