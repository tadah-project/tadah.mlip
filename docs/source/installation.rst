.. _installation:

Installation
============

This page provides step-by-step installation instructions for:

- **Tadah!MLIP** (ML potential training; provides the ``tadah`` CLI)
- **ML‑TADAH for LAMMPS** (Tadah.LAMMPS; adds the ``pair_style tadah`` pair style to LAMMPS)

For more detailed and up-to-date guidance (including platform-specific notes), see:
https://tadah.readthedocs.io/

.. _requirements:

Requirements
------------

Tadah!MLIP and the ML‑TADAH LAMMPS integration use **CMake** for configuration/build.

General requirements:

- A **C++17**-capable compiler toolchain
- **CMake** (3.15+)
- **LAPACK** (and **ScaLAPACK** for Tadah!MLIP MPI builds)
- Internet access during installation (some dependencies may be fetched at configure/build time)

LAMMPS-specific notes:

- Modern LAMMPS builds use **C++17**; the ML‑TADAH integration assumes a LAMMPS toolchain that can build LAMMPS successfully.

Installing Tadah!MLIP Software
------------------------------

Tadah!MLIP provides the command-line interface binary, ``tadah``, for developing machine-learning interatomic potentials.

Obtaining Source Code
.....................

To obtain Tadah!MLIP, clone the main branch:

.. code-block:: bash

    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

.. _building_tadah:

Configuration and Compilation
.............................

Tadah!MLIP uses CMake for configuration and a standard build tool for compilation.

The executable ``tadah`` is installed in the ``bin`` directory by default (``${CMAKE_INSTALL_PREFIX}/bin``).
If MPI is enabled, the executable is named ``tadah_mpi``.
After a successful compilation (before installation), the ``tadah`` (or ``tadah_mpi``) binary should also be present inside your build directory (commonly as ``build/tadah`` or under a subdirectory like ``build/bin/``, depending on your platform and CMake generator).

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
- ``TADAH_BUILD_MPI``: Build MPI version (default: OFF).
- ``TADAH_MARCH_NATIVE``: Use native optimizations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enable fast math optimizations (default: OFF).
- ``TADAH_ENABLE_NFP``: Enable Nested Fitting Procedure (default: OFF), see :ref:`installation_nfp`.
- ``TADAH_LAMMPS_DIR``: Absolute path to the LAMMPS directory (required by ``TADAH_ENABLE_NFP``).
- ``TADAH_LAMMPS_LIB``: Absolute path to the LAMMPS library compiled with Tadah!LAMMPS (required by ``TADAH_ENABLE_NFP``).

Additional CMake options to consider:

- ``CMAKE_INSTALL_PREFIX``: Set the install destination when ``make install`` is invoked. A common choice is ``-DCMAKE_INSTALL_PREFIX=~/.local``.
- ``BUILD_SHARED_LIBS``: Whether to build shared or static Tadah!MLIP libraries (default: OFF).
- ``BUILD_TESTING``: Include tests (default: OFF).

Append flags to the ``cmake`` command with the ``-D`` prefix and a value of ``ON``, ``OFF``, or a path, e.g.:

.. code-block:: bash

    cmake .. \
        -DTADAH_ENABLE_OPENMP=ON \
        -DTADAH_BUILD_MPI=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

You can provide multiple ``-D<option>=<value>`` pairs in a single call.

.. code-block:: bash

    make          # compile
    make install  # install to the chosen prefix
    make test     # run tests (requires -DBUILD_TESTING=ON)
    make docs     # generate local API docs (requires Doxygen)

.. _gofasterstripe:

Go Faster Stripe - Desktop Version
..................................

OpenMP is supported if available on your system. Compile Tadah!MLIP with OpenMP support and set the number of threads to match your CPU cores for best performance.

.. code-block:: bash

    export OMP_NUM_THREADS=num_of_cores

Massively Parallel Version
..........................

The Tadah!MLIP MPI version is designed for HPC architectures and large datasets.

To enable the MPI version, configure the build with ``TADAH_BUILD_MPI``:

.. code-block:: bash

    cmake .. -DTADAH_BUILD_MPI=ON

This will configure the executable as ``tadah_mpi``.

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

   - Use a serial LAMMPS build when linking to the desktop (non-MPI) Tadah!MLIP build
   - Use an MPI-enabled LAMMPS build when linking to the MPI Tadah!MLIP build

   For details on producing a LAMMPS library (shared/static) with your chosen build system, consult the LAMMPS build documentation:
   https://docs.lammps.org/Build_cmake.html

2. **Configure Tadah!MLIP** (:ref:`building_tadah`) adding:

   - ``-DTADAH_ENABLE_NFP=ON``
   - ``-DTADAH_LAMMPS_DIR=/path/to/lammps`` (LAMMPS root directory)
   - ``-DTADAH_LAMMPS_LIB=/full/path/to/liblammps_name.ext`` (full path to the built LAMMPS library file)


.. _installation_lammps:

Installing ML‑TADAH for LAMMPS (Tadah.LAMMPS)
---------------------------------------------

ML‑TADAH adds the **``pair_style tadah``** pair style to LAMMPS.
It does not require Tadah!MLIP to be installed.

.. important::

   ML‑TADAH / Tadah.LAMMPS is **not part of the official LAMMPS distribution**.
   You must clone Tadah.LAMMPS yourself and enable it during the LAMMPS build.

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

**Step 2 — Clone Tadah.LAMMPS**

Clone Tadah.LAMMPS anywhere on your filesystem:

.. code-block:: bash

    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git /path/to/Tadah.LAMMPS

**Step 3 — Configure LAMMPS and enable ML‑TADAH**

From the **LAMMPS root directory** (``/path/to/lammps``), run your usual LAMMPS CMake configure command and **add** the ML‑TADAH integration options:

.. code-block:: bash

    -DPKG_ML-TADAH=ON \
    -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
    -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS

Minimal example:

.. code-block:: bash

    cd /path/to/lammps
    cmake -S cmake -B build \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS

Optional: select the Tadah.LAMMPS branch (default is ``main``):

.. code-block:: bash

    -DTADAH_GIT_BRANCH=develop
    # (or: -DTADAH_GIT_BRANCH=main)

Example using the LAMMPS GCC preset and building a serial binary:

.. code-block:: bash

    cd /path/to/lammps
    cmake -C cmake/presets/gcc.cmake -S cmake -B build \
      -DPKG_ML-TADAH=ON \
      -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
      -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
      -DTADAH_GIT_BRANCH=develop \
      -DLAMMPS_MACHINE=serial

**Step 4 — Build**

.. code-block:: bash

    cmake --build build

Parallel builds are supported. A typical Linux choice is:

.. code-block:: bash

    cmake --build build --parallel $(nproc)

After building, the produced LAMMPS executable from this build directory includes ML‑TADAH and supports ``pair_style tadah``.

**Advanced integration options**

The CMake integration also supports additional options (e.g. verbosity controls and custom style registration lists). These are documented here:

- ``tadah.lammps/integration/README.md`` (in your Tadah.LAMMPS checkout)

Traditional make-based build (deprecated)
.........................................

Some environments still use the classic LAMMPS ``make`` workflow. ML‑TADAH can be built this way as well.

For background on the LAMMPS make-based build and “extras”, see:

- https://docs.lammps.org/Build_extras.html

Typical steps:

.. code-block:: bash

    # From your LAMMPS source tree
    cd /path/to/lammps/lib
    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git tadah.lammps

    cd /path/to/lammps/src
    make lib-tadah.lammps
    make yes-ml-tadah
    make serial

Replace ``serial`` with the appropriate target for your system (MPI/OpenMP, etc.).

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
        -ltadah -o my_code

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

The following libraries can be compiled, depending on the build settings:

- ``libtadah.core``
- ``libtadah.models``
- ``libtadah.mlip``
- ``libtadah.md``
- ``libtadah.hpo`` (MPI version: ``libtadah.mpi.hpo``)

.. _compiling_and_linking:

Compiling and Linking with Library
..................................

For C++ library use, compile with C++17 or higher. Include library headers using the prefix ``tadah/`` and link relevant libraries. For ``libtadah.mlip`` it is often the case that both ``libtadah.models`` and ``libtadah.core`` must be linked as well.

.. code-block:: c

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

   Ensure CMake is installed and up-to-date (3.15+). On Linux, you can often install or upgrade CMake via your package manager, or download a binary from the CMake website.

#. **Compiler not found / version too old**

   Ensure your C++ compiler supports C++17. Update your compiler if necessary (e.g., GCC 7+, Clang 5+, MSVC 2017+).

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
