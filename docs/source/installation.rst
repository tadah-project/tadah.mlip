.. _installation:

Installation
============

This section provides step-by-step instructions for installation of Tadah!MLIP (MLIP training) and Tadah!LAMMPS (LAMMPS plugin).

.. _requirements:

Requirements
------------

Both Tadah!MLIP and Tadah!LAMMPS utilize CMake for configuration and building of its components.
The Tadah!LAMMPS interface and Tadah!MLIP toolkit require C++11 and C++17 compatible compilers, respectively. 
LAPACK and ScaLAPACK (Tadah!MLIP MPI version only) must be available on the user’s system.

Other necessary libraries are either included in the codebase or downloaded during the compilation process.
The internet connection is required during the installation process.

Installing Tadah!MLIP Software
------------------------------

Tadah!MLIP provides a command-line interface binary, ``tadah``, for developing machine learning interatomic potentials.

Obtaining Source Code
.....................

To obtain Tadah!MLIP, clone the main branch using:

.. code-block:: bash

    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git



.. _building_tadah:

Configuration and Compilation
.............................

Tadah!MLIP uses CMake for configuration and Make for compilation.

The executable ``tadah`` is installed in the ``bin`` directory by default (``${CMAKE_INSTALL_PREFIX}/bin``). If MPI is enabled, the executable is named ``tadah_mpi``.

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

To override individual settings without editing the preset file, append ``-D`` options:

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

To change the installation location:

.. code-block:: bash

    cmake .. -DCMAKE_INSTALL_PREFIX=/your/path

Configuration Options
---------------------

.. note::

   If you configure with ``-C cmake/presets/tadah-linux-defaults.cmake``, the values in that
   file become the initial defaults for the new build directory (e.g. it sets conservative
   end-user defaults such as disabling optional features). You can customize these defaults
   either by (1) appending ``-D<option>=<value>`` to the CMake command, or (2) editing the
   preset file itself and reusing it for future builds.

Customize the Tadah!MLIP build using CMake options:

- ``TADAH_ENABLE_OPENMP``: Enable OpenMP for parallelization (default: OFF).
- ``TADAH_BUILD_MPI``: Build MPI version (default: OFF).
- ``TADAH_MARCH_NATIVE``: Use native optimizations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enable fast math optimizations (default: OFF).
- ``TADAH_ENABLE_HPO``: Enable Hyperparameter Optimizer (default: OFF), see :ref:`installation_hpo`.
- ``TADAH_LAMMPS_DIR``: Absolute path to the LAMMPS directory (required by ``TADAH_ENABLE_HPO``).
- ``TADAH_LAMMPS_LIB``: Absolute path to the LAMMPS library compiled with Tadah!LAMMPS (required by ``TADAH_ENABLE_HPO``).

Additional CMake options to consider:

- ``CMAKE_INSTALL_PREFIX``: Set the install directory destination when ``make install`` is invoked. A common choice is ``-DCMAKE_INSTALL_PREFIX=~/.local``.
- ``BUILD_SHARED_LIBS``: Whether to build shared or static Tadah!MLIP libraries (default: ON).
- ``BUILD_TESTING``: Include tests (default: OFF).

Append any of the flags above to the `cmake` command with the `-D` prefix and a value of `ON`, `OFF`, or a path, e.g.:

.. code-block:: bash

    cmake .. \
        -DTADAH_ENABLE_OPENMP=ON \
        -DTADAH_BUILD_MPI=ON \
        -DCMAKE_INSTALL_PREFIX=~/.local

You can string together as many `-D<option>=<value>` pairs as needed in a single call.

.. code-block:: bash

  make          # compile
  make install  # install to the chosen prefix
  make test     # run tests (requires -DBUILD_TESTING=ON)
  make docs     # generate local API docs (requires Doxygen)

.. _gofasterstripe:

Go Faster Stripe - Desktop Version
..................................

OpenMP is supported if available on your system. Compile Tadah!MLIP with OpenMP support and set the number of threads to match your CPU cores for optimal performance.

.. code-block:: bash

    export OMP_NUM_THREADS=num_of_cores

Massively Parallel Version
..........................

The Tadah!MLIP MPI version is designed for High-Performance Computing (HPC) architectures, making it suitable for handling large datasets like those from the Materials Project. This enables efficient computation and model training at scale.

To enable the MPI version, configure the build with the ``TADAH_BUILD_MPI`` option:

.. code-block:: bash

    cmake .. -DTADAH_BUILD_MPI=ON

This will configure the executable as ``tadah_mpi``, optimized for parallel execution.

.. _installation_lammps:

Installing LAMMPS Interface
---------------------------

The Tadah!LAMMPS interface is essential for simulations with LAMMPS and Tadah! potentials. It is also required for building Tadah!MLIP with the Hyperparameter Optimizer (HPO) module. 
The compilation of Tadah! plugin follows typical LAMMPS compilation steps.

1. Clone the LAMMPS stable version from the GitHub repository:

   .. code-block:: bash

     git clone -b stable --depth 1 https://github.com/lammps/lammps.git /path/to/lammps

2. Example LAMMPS serial build with make typically looks like this:

   .. code-block:: bash

     cd /path/to/lammps/lib
     git clone https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git
     cd /path/to/lammps/src
     make lib-tadah.lammps
     make yes-ml-tadah
     make serial

.. important:: Tadah!LAMMPS is compatible with LAMMPS releases from June 2022 onward. Because the plugin is not yet included in the official LAMMPS distribution, it does not support the LAMMPS CMake build system.

.. seealso:: For building LAMMPS with extra features, visit:

             https://lammps.sandia.gov/doc/Build_package.html


.. _installation_hpo:

Building Tadah!MLIP with Hyperparameter Optimizer
-------------------------------------------------

.. warning::

   The HPO module of Tadah!MLIP requires LAMMPS to be compiled with Tadah!LAMMPS and ``-DLAMMPS_EXCEPTIONS=yes``. Note that this setting is the default only for versions from 29 Aug 2024 onwards. For older versions, users must set it manually.

Building Tadah!MLIP with HPO is slightly more challenging, and for this reason, it is not built by default. Please read these notes carefully before proceeding.

To build Tadah!MLIP with HPO, follow these steps:

1. **Compile LAMMPS with the Tadah.LAMMPS Interface** (:ref:`installation_lammps`):

   **Shared vs. Static Builds**: 
   
   It is recommended to build the shared version of LAMMPS (e.g., ``make mode=shared serial``), as shared libraries are typically less problematic during the linking stage.

   - Shared LAMMPS builds produce files like ``liblammps_serial.so``.
   - Static builds are not compiled with position-independent code by default, which prevents linking with a shared Tadah!MLIP build.
   - **Recommendation**: Build both Tadah!MLIP and LAMMPS as shared libraries unless you have specific requirements.

   **Serial vs. MPI**:

   - **Serial**: For single-threaded applications, use commands like ``make mode=shared serial``. The serial LAMMPS version must be linked to the Desktop version of Tadah!.
   - **MPI**: For parallel applications, compile LAMMPS with MPI support, e.g., ``make mode=shared mpi``. The MPI version of LAMMPS must be linked to the Massively Parallel version of Tadah!.

2. **Configure Tadah!** (:ref:`building_tadah`) using CMake with these additional flags:

   - ``-DTADAH_ENABLE_HPO=ON``
   - ``-DTADAH_LAMMPS_DIR=/path/to/lammps``: This should be the root directory of your LAMMPS installation, not ``lammps/src``.
   - ``-DTADAH_LAMMPS_LIB=liblammps_name.ext``: Replace ``liblammps_name`` with the name of your compiled LAMMPS library and .ext with appropriate file extension (.so for shared builds, e.g., ``liblammps_serial.so`` for serial or ``liblammps_mpi.so`` for MPI; .a for static builds, e.g., ``liblammps_serial.a``).

Libraries and C++ Headers
-------------------------

.. note::
   This section is relevant only if you intend to use the Tadah!API. If you're using the command line interface (CLI), you can safely skip this section.

Tadah!API installs its headers to  
``${CMAKE_INSTALL_PREFIX}/include`` and its libraries to  
``${CMAKE_INSTALL_PREFIX}/lib`` (or ``lib64``).  If the prefix is a standard
location such as ``/usr`` or ``/usr/local`` nothing extra is required:

.. code-block:: bash

    g++ my_code.cpp -ltadah.core -o my_code            # headers and libs found automatically

Non-standard prefix (e.g. ``$HOME/.local``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
When Tadah! is installed in a private directory you must give the compiler and
linker a hint.  The quickest way is to add the include and library paths on the
command line:

.. code-block:: bash

    g++ my_code.cpp \
        -I$HOME/.local/include \
        -L$HOME/.local/lib \
        -ltadah -o my_code

Run-time resolution  
``LD_LIBRARY_PATH`` tells the dynamic loader where to find the shared object
after the executable has been built:

.. code-block:: bash

    export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH
    ./my_code

Optional convenience variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
If you prefer not to repeat ``-I``/``-L`` flags, set the environment variables
once for your shell (or in a module file):

.. code-block:: bash

    # compile-time
    export CPATH=$HOME/.local/include:$CPATH        # headers
    export LIBRARY_PATH=$HOME/.local/lib:$LIBRARY_PATH  # libraries

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

For C++ library use, compile with C++17 or higher. Include library headers using the prefix `tadah/` and link relevant libraries. For ``libtadah.mlip`` it is often the case that both ``libtadah.models`` and ``libtadah.core`` must be linked as well.

.. code-block:: c

    #include <tadah/mlip/atom.h>

Link with ``libtadah.mlip``.

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp -ltadah.mlip

If the library is outside the standard path:

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp -I/home/$USER/.local/include -L/home/$USER/.local/lib -ltadah.mlip

.. _troubleshooting:

Troubleshooting
---------------

#. **Permission Denied** when running ``make install``
   
   Run ``make install`` with ``sudo`` or change the installation path:
   
   .. code-block:: bash

     cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
     make install

#. **Shared Object ImportError** when running ``tadah``

   Update ``LD_LIBRARY_PATH``:

   .. code-block:: bash

     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/your/path/

#. **Compatibility Issues with LAMMPS** 

   Ensure compatibility with the correct LAMMPS version. See :ref:`installation_lammps`.

#. **New Issues**

   Contact support for any unlisted issues.
