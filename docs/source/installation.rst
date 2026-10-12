.. _installation:

Installation
============

This page provides step-by-step installation instructions for:

- **Tadah!MLIP** (ML potential training; provides the ``tadah`` CLI)
- **ML‑TADAH for LAMMPS** (Tadah!LAMMPS; adds the ``pair_style tadah`` pair style to LAMMPS)

For more detailed and up-to-date guidance (including platform-specific notes), see:
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

Clone Tadah!MLIP at a release tag, or at ``v1-dev``, the development
branch of the 1.x line, from which the releases are tagged:

.. code-block:: bash

    # the 1.6.0 release
    git clone -b v1.6.0 https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

    # or the development line
    git clone -b v1-dev https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

Name the branch or tag: a plain ``git clone`` checks out the repository's
default branch, which need not be ``v1-dev``. CMake fetches the other
Tadah! modules at the same ref. It finds a branch by itself, but a tag
checkout has no branch, so configure a release clone with
``-DTADAH_GIT_BRANCH=v1.6.0`` (see :ref:`installation_git_branch`).

.. _building_tadah:

Configuration and Compilation
.............................

Tadah!MLIP uses CMake for configuration and a standard build tool for compilation.

The executable ``tadah`` is installed in the ``bin`` directory by default (``${CMAKE_INSTALL_PREFIX}/bin``).
After a successful compilation (before installation), the ``tadah`` binary should also be present inside your build directory (commonly as ``build/tadah`` or under a subdirectory like ``build/bin/``, depending on your platform and CMake generator).

Using the provided CMake defaults preset (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Tadah!MLIP ships a CMake *initial cache* file with Linux end-user defaults at
``cmake/presets/tadah-linux-defaults.cmake``. You can use it as-is, or edit it to
match your preferences and reuse it for future builds.

.. code-block:: bash

    # Navigate to the project directory
    cd tadah.mlip

    # Configure a new build directory using the defaults preset
    cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake

    # Compile and install (uses the configured build tool, e.g. Make)
    cmake --build build --parallel
    cmake --install build

To override individual settings without editing the preset file, append ``-D`` options (see :ref:`configuration_options`), e.g.:

.. code-block:: bash

    cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake \
        -DTADAH_ENABLE_OPENMP=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

Alternatively, you can configure and build without the preset as follows:

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

   If you configure with ``-C cmake/presets/tadah-linux-defaults.cmake``, the values in that
   file become the initial defaults for the new build directory (e.g. it sets conservative
   end-user defaults such as disabling optional features). You can customize these defaults
   either by (1) appending ``-D<option>=<value>`` to the CMake command, or (2) editing the
   preset file itself and reusing it for future builds.

Installation location
^^^^^^^^^^^^^^^^^^^^^

To change the installation location from the default (typically ``/usr/local`` on Linux), set
``CMAKE_INSTALL_PREFIX`` during configuration:

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

Alternative: set the build type in your own preset
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you prefer to keep your build type in a preset file, you can copy the provided preset and
add a default build type:

.. code-block:: cmake

    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)")

Customize the Tadah!MLIP build using CMake options:

- ``TADAH_ENABLE_OPENMP``: Enable OpenMP for parallelization (default: OFF).
- ``TADAH_MARCH_NATIVE``: Use native optimizations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enable fast math optimizations (default: OFF).
- ``TADAH_ENABLE_NFP``: Enable Nested Fitting Procedure (default: OFF), see :ref:`installation_nfp`. A ``tadah`` built without it refuses ``tadah hpo`` and ``TASK hpo`` before any task runs, ``tadah hpo needs the Nested Fitting Procedure, which this tadah was built without; reconfigure with -DTADAH_ENABLE_NFP=ON (see Installation)``, and its ``tadah -h`` says so on the ``hpo`` line.
- ``TADAH_LAMMPS_DIR``: Absolute path to the LAMMPS directory (required by ``TADAH_ENABLE_NFP``).
- ``TADAH_LAMMPS_LIB``: Absolute path to the LAMMPS library compiled with Tadah!LAMMPS (required by ``TADAH_ENABLE_NFP``).
- ``TADAH_LAMMPS_LIBS``: Extra link items for the LAMMPS library, as a CMake list (e.g. ``"-lz;-lzstd"``); needed mainly for a static LAMMPS library, whose own dependencies are not propagated (optional, used by ``TADAH_ENABLE_NFP``).
- ``TADAH_ENABLE_CERES``: Build the Ceres Solver optimiser family of ``tadah hpo`` (``LIB CERES``) (default: ON; only relevant with ``TADAH_ENABLE_NFP``). ``PARAM fd_kernel ceres``, Ceres' finite-difference step rule, does not need it: Tadah evaluates that rule itself (before 1.6.0 it ran inside Ceres and needed this option). The compact fit of ``tadah refit`` does need it: without it ``tadah refit`` refuses ``REFIT_MODE compact``, the default, so a refit configuration of such a build sets ``REFIT_MODE dense``.
- ``TADAH_GIT_BRANCH``: The git branch or tag the Tadah! modules are fetched at (detected when not given, see :ref:`installation_git_branch`; pass it for a release source tarball, e.g. ``v1.6.0``).
- ``TADAH_HPO_MPI``: Distribute the replicas of the ``TADAH:ANNEAL_PT`` / ``TADAH:ANNEAL_PA`` optimisers of ``tadah hpo`` over MPI ranks (default: OFF; only relevant with ``TADAH_ENABLE_NFP``; needs an MPI build of LAMMPS), see :ref:`installation_hpo_mpi`.

Additional CMake options to consider:

- ``CMAKE_INSTALL_PREFIX``: Set the install destination when ``make install`` is invoked. A common choice is ``-DCMAKE_INSTALL_PREFIX=~/.local``.
- ``BUILD_SHARED_LIBS``: Whether to build shared or static Tadah!MLIP libraries (default: OFF).
- ``BUILD_TESTING``: Include tests (default: OFF). Needs CMake 3.16 or later, and a Python 3 interpreter, which runs the unit tests of the documentation build tools; with ``TADAH_ENABLE_NFP``, Python 3.11 or later, for the Nested Fitting tests (their key-registry check reads TOML with ``tomllib``). The configure step stops when it finds no suitable interpreter.

Append flags to the ``cmake`` command with the ``-D`` prefix and a value of ``ON``, ``OFF``, or a path, e.g.:

.. code-block:: bash

    cmake .. \
        -DTADAH_ENABLE_OPENMP=ON \
        -DTADAH_MARCH_NATIVE=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

You can provide multiple ``-D<option>=<value>`` pairs in a single call.

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
has no branch: CMake warns, and you pass the ref yourself, for a release
its tag:

.. code-block:: bash

    cmake -S . -B build -DTADAH_GIT_BRANCH=v1.6.0

Build, install, test, and docs from an out-of-tree ``build`` directory:

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

OpenMP is supported if available on your system. Compile Tadah!MLIP with OpenMP support and set the number of threads to match your CPU cores for best performance.

.. code-block:: bash

    export OMP_NUM_THREADS=num_of_cores

.. _installation_hpo_mpi:

MPI-Parallel Hyperparameter Optimisation
........................................

Training (``tadah train``) and the other ``tadah`` commands run in one
process; use OpenMP threads (:ref:`gofasterstripe`) to occupy the cores of a
node. MPI parallelism is available for hyperparameter optimisation: with
``-DTADAH_HPO_MPI=ON`` the ``TADAH:ANNEAL_PT`` and ``TADAH:ANNEAL_PA``
optimisers of ``tadah hpo`` distribute their replicas over MPI ranks, which
lets one run span several nodes. It needs ``-DTADAH_ENABLE_NFP=ON``
(:ref:`installation_nfp`) and an MPI build of LAMMPS with ML‑TADAH, both
built with the same MPI library.

**1. LAMMPS with MPI.** Configure LAMMPS with ML‑TADAH as in
:ref:`installation_lammps`, adding ``-DBUILD_MPI=ON`` and a library name
that contains ``mpi`` (``-DLAMMPS_MACHINE=mpi`` names it
``liblammps_mpi.a``), and build the library target:

.. code-block:: bash

    cd /path/to/lammps
    cmake -S cmake -B build-mpi \
      -DBUILD_MPI=ON -DLAMMPS_MACHINE=mpi \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/tadah.lammps/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah.lammps \
      -DTADAH_GIT_BRANCH=v1-dev
    cmake --build build-mpi --target lammps --parallel

**2. Tadah!MLIP with TADAH_HPO_MPI.** Point ``TADAH_LAMMPS_DIR`` and
``TADAH_LAMMPS_LIB`` at that LAMMPS, and enable OpenMP:

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
the like; alternatively configure LAMMPS with ``-DBUILD_OMP=OFF``. Nor
does a static LAMMPS library bring the libraries that its packages use.
The LAMMPS of step 1, with ML‑TADAH alone, needs none; one with
COMPRESS needs zlib (and libzstd, when LAMMPS found it), one with GRAPHICS
libjpeg, libpng and zlib (those it found), and one with VORONOI that
build's own ``build-mpi/voro_build-prefix/src/voro_build/src/libvoro++.a``.
Add them with ``TADAH_LAMMPS_LIBS``, as in the serial build (step 2 of
:ref:`installation_nfp`), for example
``-DTADAH_LAMMPS_LIBS="-lzstd;-lz;-lpng;-ljpeg;/path/to/lammps/build-mpi/voro_build-prefix/src/voro_build/src/libvoro++.a"``
for all three; the link errors name the ones that are missing.

The executable is still ``tadah`` and runs without ``mpirun`` too (a
one-rank job; Open MPI then makes its session directory under ``TMPDIR``,
which must be a writable directory). The configure step refuses a LAMMPS
library whose file name lacks ``mpi``, because the serial LAMMPS library carries MPI stub symbols
that collide with a real MPI library at link time; set
``-DTADAH_HPO_MPI_SKIP_LIB_CHECK=ON`` when an MPI build of LAMMPS is simply
named differently. The MPI library must provide ``MPI_THREAD_MULTIPLE``.

**3. Launching.** Launch with ``srun`` or with the ``mpirun`` /
``mpiexec`` of the MPI library ``tadah`` links, the one next to the
``mpicxx`` that CMake recorded as ``MPI_CXX_COMPILER`` in
``build-mpi/CMakeCache.txt``. A launcher of another MPI — one that comes
first on ``PATH``, from a conda environment for example — does not start
one job of N ranks but N independent one-rank runs, which all write the
unsuffixed ``hpo_run.log`` and result files of a one-rank run. The
requirements and an ARCHER2 job script are in :ref:`tadah_anneal_mpi`.

**4. Testing an MPI build.** With ``-DBUILD_TESTING=ON`` the whole suite
runs in the MPI build too (``ctest --test-dir build-mpi``): each test
program is a one-rank MPI job. The cases that start ``PC_ISOLATION
PROCESS`` workers -- those of the worker pool, and two HPO runs under
``PROCESS`` -- are *Skipped* there (the MPI build has ``PC_ISOLATION
THREAD`` only), as are the optional-data cases of the serial build. If the
build stops while it lists the tests (the MPI build's test programs
initialise MPI to list them), Open MPI's ``MPI_Init`` may be waiting for an
X display through its hwloc GL component: build and test with
``HWLOC_COMPONENTS=-gl``. The multi-rank
tests of ``tadah hpo`` exist only in this build: ``ctest -L mpi`` runs them
through ``MPIEXEC_EXECUTABLE`` at 1, 2 and 4 ranks and needs four process
slots
(``MPIEXEC_MAX_NUMPROCS``). CMake takes the first ``mpiexec`` on ``PATH``
for ``MPIEXEC_EXECUTABLE``, whichever MPI it belongs to; when another
MPI's comes first, configure with ``-DMPIEXEC_EXECUTABLE=/path/to/mpiexec``,
the launcher of step 3 (the wrong one starts one-rank jobs, and the
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
   On recent LAMMPS versions this is often enabled by default; on older versions it may need to be set explicitly in your LAMMPS build configuration. See :ref:`installation_lammps` for details on building LAMMPS with ML‑TADAH.

Building Tadah!MLIP with NFP is more involved, and for this reason it is not built by default. Please read these notes carefully before proceeding.

To build Tadah!MLIP with NFP, follow these steps:

1. **Compile LAMMPS with ML‑TADAH enabled** (:ref:`installation_lammps`).

   **Shared vs. Static builds**

   It is often simplest to use a shared-library build of LAMMPS when linking against Tadah!MLIP.

   **Serial vs. MPI**

   - Use a serial LAMMPS build for the default (non-MPI) Tadah!MLIP build.
   - Use an MPI build of LAMMPS for the MPI-parallel HPO build (``TADAH_HPO_MPI``, :ref:`installation_hpo_mpi`).

   For details on producing a LAMMPS library (shared/static) with your chosen build system, consult the LAMMPS build documentation:
   https://docs.lammps.org/Build_cmake.html

2. **Configure Tadah!MLIP** (:ref:`building_tadah`) adding:

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
   You must clone Tadah!LAMMPS yourself and enable it during the LAMMPS build.

For general LAMMPS build instructions and prerequisites, see the official LAMMPS documentation:

- CMake build: https://docs.lammps.org/Build_cmake.html

There are two ways to build LAMMPS with ML‑TADAH:

1. **CMake build (recommended)**
2. **Traditional make-based build (deprecated)**

CMake build with ML‑TADAH (recommended)
.......................................

This method integrates ML‑TADAH into a normal LAMMPS CMake configure/build, by adding a small set of extra CMake options.

**Step 1 — Clone LAMMPS**

Choose a LAMMPS version appropriate for your project (example: git clone stable branch):

.. code-block:: bash

    git clone -b stable --depth 1 https://github.com/lammps/lammps.git /path/to/lammps

**Step 2 — Clone Tadah!LAMMPS**

Clone Tadah!LAMMPS anywhere on your filesystem, at a release tag or at
``v1-dev``, the development line (a plain ``git clone`` checks out the
repository's default branch, which need not be either):

.. code-block:: bash

    # the 1.6.0 release
    git clone -b v1.6.0 https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git /path/to/tadah.lammps

    # or the development line
    git clone -b v1-dev https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git /path/to/tadah.lammps

**Step 3 — Configure LAMMPS and enable ML‑TADAH**

From the **LAMMPS root directory** (``/path/to/lammps``), run your usual LAMMPS CMake configure command and **add** these four options:

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
are whatever you have checked out at ``TADAH_LAMMPS_SOURCE_DIR``.  Give
the ref of that checkout, so that both come from the same line:
``-DTADAH_GIT_BRANCH=v1-dev`` for a ``v1-dev`` clone,
``-DTADAH_GIT_BRANCH=v1.6.0`` for a clone of the 1.6.0 release.

Without it the ref is detected (:ref:`installation_git_branch`) from the
checkout of the project CMake was started on, which in this build is
**LAMMPS**: a LAMMPS branch (``stable``, ``develop``, …) that has nothing
to do with your Tadah!LAMMPS checkout, or no ref at all for a LAMMPS
release tarball or a detached HEAD (CMake warns).  Always pass
``-DTADAH_GIT_BRANCH`` in the LAMMPS build.

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

**Step 4 — Build**

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

The CMake integration also supports additional options (e.g. verbosity controls and custom style registration lists). These are documented here:

- ``tadah.lammps/integration/README.md`` (in your Tadah!LAMMPS checkout)

.. _installation_lammps_mpi:

Running pair_style tadah on several MPI ranks
.............................................

``pair_style tadah`` runs domain-decomposed like any LAMMPS pair style.
Configure LAMMPS with the options of Step 3 and ``-DBUILD_MPI=ON`` (CMake
finds the MPI of ``mpicxx``), build ``lmp`` and start it with the
``mpirun`` / ``mpiexec`` of that MPI (or ``srun``):

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
426 K after 100 steps), though each run conserves its energy. Use ``loop
geom`` (velocities from the positions), or start every run from one data
file, to compare runs across rank counts: with ``loop geom`` the three
``in.ml`` runs print the same thermo rows for all 1000 steps.

The test suite checks this agreement when it is given an MPI ``lmp``
(``tadah.lammps_mpi_*``, see ``CONTRIBUTING.md``, *Running tests*):
configure a Tadah!MLIP build with tests and NFP (:ref:`installation_nfp`)
with ``-DTADAH_LMP_MPI=/path/to/lammps/build-mpi/lmp_mpi`` and
``-DTADAH_LMP_MPIEXEC=/path/to/mpiexec``, the launcher of that MPI, and
run ``ctest -L lammps``; without them the entries are *Skipped*.

Traditional make-based build
............................

The classic LAMMPS ``make`` workflow is **not supported** for
Tadah!LAMMPS in this release. Use the CMake build documented above.

Libraries and C++ Headers
-------------------------

.. note::
   This section is relevant only if you intend to use the Tadah!API. If you're using the command line interface (CLI), you can safely skip this section.

Tadah!API installs its headers to
``${CMAKE_INSTALL_PREFIX}/include`` and its libraries to
``${CMAKE_INSTALL_PREFIX}/lib`` (or ``lib64``). If the prefix is a standard
location such as ``/usr`` or ``/usr/local`` nothing extra is required:

.. code-block:: bash

    g++ my_code.cpp -ltadah.core -o my_code  # headers and libs found automatically

Non-standard prefix (e.g. ``$HOME/.local``)
...........................................
When Tadah! is installed in a private directory you must give the compiler and
linker a hint. The quickest way is to add the include and library paths on the
command line:

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
If you prefer not to repeat ``-I``/``-L`` flags, set environment variables
once for your shell (or in a module file):

.. code-block:: bash

    # compile-time
    export CPATH=$HOME/.local/include:$CPATH
    export LIBRARY_PATH=$HOME/.local/lib:$LIBRARY_PATH

    # run-time
    export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH

Keep the scope of these variables as narrow as possible (script, shell session,
or environment module) to avoid accidental conflicts with other software.

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

For C++ library use, compile with C++17 or higher. Include library headers using the prefix ``tadah/`` and link relevant libraries. For ``libtadah.mlip`` it is often the case that both ``libtadah.models`` and ``libtadah.core`` must be linked as well.

.. code-block:: c++

    #include <tadah/mlip/atom.h>

Link with ``libtadah.mlip``:

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

   Ensure CMake is installed and up-to-date (3.15 or later; 3.24 with ``TADAH_ENABLE_NFP``, 3.16 with ``BUILD_TESTING``). On Linux, you can often install or upgrade CMake via your package manager, or download a binary from the CMake website.

#. **Compiler not found / version too old**

   Ensure your C++ compiler supports C++17, including floating-point ``std::to_chars`` in its standard library. Update your compiler if necessary (e.g., GCC 11+, Clang with libc++ 14+ or libstdc++ 11+, MSVC 2019 16.4+).

#. **LAPACK not found**

   Install LAPACK development files via your package manager (e.g., ``liblapack-dev`` on Debian/Ubuntu).

#. **CMake configuration errors related to missing dependencies**

   Review the CMake output for missing dependencies. Install any required libraries or development packages.

#. **Build errors during compilation**

   Check the compiler output for specific error messages. Ensure all dependencies are correctly installed and that your compiler supports the required C++ standard.

#. **Permission denied** when running ``make install``

   Choose a user-writeable prefix:

   .. code-block:: bash

      cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
      make install

#. **Shared object ImportError** when running ``tadah``

   Update ``LD_LIBRARY_PATH``:

   .. code-block:: bash

      export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/your/path/

#. **LAMMPS build issues**

   Consult the LAMMPS build documentation and ensure your toolchain is working for LAMMPS first:

   - https://docs.lammps.org/Build_cmake.html

   Then re-check the ML‑TADAH integration steps in :ref:`installation_lammps`.

#. **Other issues**

   Contact us for any unlisted problems.
