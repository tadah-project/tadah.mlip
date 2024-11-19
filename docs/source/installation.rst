.. _installation:

Installation
============

Installing Tadah! Software
--------------------------

Tadah!MLIP provides a command-line interface binary, ``tadah``, for developing machine learning interatomic potentials.

Obtaining Source Code
.....................

To obtain Tadah!MLIP, clone the main branch using:

.. code-block:: bash

    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

.. _requirements:

Requirements
............

Tadah!MLIP includes all necessary libraries, so no external downloads are needed.

The internet connection is required during the installation process.

.. _building_tadah:

Configuration and Compilation
.............................

Tadah!MLIP uses CMake for configuration and Make for compilation.

The executable ``tadah`` is installed in the ``bin`` directory by default (``${CMAKE_INSTALL_PREFIX}/bin``). If MPI is enabled, the executable is named ``tadah_mpi``.

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

Customize the Tadah!MLIP build using CMake options:

- ``TADAH_ENABLE_OPENMP``: Enable OpenMP for parallelization (default: OFF).
- ``TADAH_BUILD_MPI``: Build MPI version (default: OFF).
- ``TADAH_MARCH_NATIVE``: Use native optimizations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enable fast math optimizations (default: OFF).
- ``TADAH_BUILD_TESTS``: Include tests (default: OFF).
- ``TADAH_BUILD_DOCUMENTATION``: Build documentation with Doxygen (default: OFF).
- ``TADAH_ENABLE_HPO``: Enable Hyperparameter Optimizer (default: OFF), see :ref:`installation_hpo`.
- ``TADAH_LAMMPS_DIR``: Absolute path to the LAMMPS directory (required by ``TADAH_ENABLE_HPO``).
- ``TADAH_LAMMPS_LIB``: Name of the LAMMPS library compiled with Tadah!LAMMPS (required by ``TADAH_ENABLE_HPO``).

Additional CMake options to consider:

- ``CMAKE_INSTALL_PREFIX``: Set the install directory destination when ``make install`` is invoked. A common choice is ``-DCMAKE_INSTALL_PREFIX=~/.local``.
- ``BUILD_SHARED_LIBS``: Whether to build shared or static Tadah!MLIP libraries (default: ON).

.. _gofasterstripe:

Go Faster Stripe - Desktop Version
..................................

OpenMP is supported if available on your system. Compile Tadah!MLIP with OpenMP support.

Set thread usage:

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

The Tadah!LAMMPS interface is essential for simulations with LAMMPS and Tadah! potentials. Follow these steps:

1. Clone the LAMMPS stable version from the GitHub repository:

   .. code-block:: bash

     git clone -b stable https://github.com/lammps/lammps.git /path/to/lammps

2. Build the interface:

   .. code-block:: bash

     cd /path/to/lammps/lib
     git clone https://git.ecdf.ed.ac.uk/tadah/tadah.lammps.git
     cd /path/to/lammps/src
     make lib-tadah.lammps
     make yes-ml-tadah
     make serial

.. important:: Compatible with LAMMPS from June 2022 onwards.

.. seealso:: For building LAMMPS with extra features, visit:

             https://lammps.sandia.gov/doc/Build_package.html


.. _installation_hpo:

Building Tadah!MLIP with Hyperparameter Optimiser
-------------------------------------------------

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
   - ``-DTADAH_LAMMPS_LIB=liblammps_name.ext``: Replace ``liblammps_name`` with the name of your compiled LAMMPS library (.so for shared builds, e.g., ``liblammps_serial.so`` for serial or ``liblammps_mpi.so`` for MPI; .a for static builds, e.g., ``liblammps_serial.a``).

Libraries and C++ Headers
-------------------------

Headers and libraries are installed according to ``-DCMAKE_INSTALL_PREFIX`` or default locations. Set ``LD_LIBRARY_PATH`` and ``CPATH`` as needed.

.. code-block:: bash

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/lib
    export CPATH=$CPATH:/path/to/include

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

For C++ library use, compile with C++17 or higher. Include library headers using the prefix `tadah/`.

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

#. **Permission Denied**
   
   Run ``make install`` with ``sudo`` or change the installation path:
   
   .. code-block:: bash

     cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
     make install

#. **Shared Object ImportError**

   Update ``LD_LIBRARY_PATH``:

   .. code-block:: bash

     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/your/path/

#. **Compatibility Issues with LAMMPS**

   Ensure compatibility with the correct LAMMPS version. See :ref:`installation_lammps`.

#. **New Issues**

   Contact support for any unlisted issues.

