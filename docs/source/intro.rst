.. _introduction:

Introduction
============

Tadah! is a fast and modular machine learning **software** and C++ **library** specifically for interatomic potential development. It’s built in modern C++ to provide an easy-to-use, modular, and extensible toolkit.

Tadah! offers a LAMMPS interface compatible with all descriptors and models. It can be used as command-line software for training models or using machine learning potentials for prediction, or as an advanced C++ library.

Key Features
------------

- **Hyperparameter Optimizer**: Enhances potential transferability.
- **Community-Driven**: Open to new ideas and implementations.
- **Speed**: Fast model development cycles reduce wait times.
- **LAMMPS Integration**: Fully interfaced.
- **Regular Updates**: Frequent new features and optimizations.
- **Open-Source**: Publicly accessible and modifiable.
- **Modular Design**: Mix and match descriptors for flexible testing.
- **Extensible**: Easy addition of new descriptors.
- **Desktop and MPI Versions**: Suitable for various computational needs.

.. _requirements:

Requirements
------------

Tadah! automatically includes all necessary libraries, so no external downloads are needed.

.. _obtaining_tadah:

Obtaining Tadah!
----------------

The code repository is available at:

    https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

Clone the main branch using:

.. code-block:: bash

    git clone https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git

.. _installation:

Installation
------------

Tadah! uses CMake for configuration.

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

Use CMake options to customize the build:

- ``TADAH_ENABLE_HPO``: Enable Hyperparameter Optimizer (default: OFF).
- ``TADAH_ENABLE_OPENMP``: Enable OpenMP for parallelisation (default: OFF).
- ``TADAH_BUILD_MPI``: Build MPI version (default: OFF).
- ``TADAH_MARCH_NATIVE``: Use native optimizations (default: OFF).
- ``TADAH_ENABLE_FAST_MATH``: Enable fast math optimizations (default: OFF).
- ``TADAH_BUILD_TESTS``: Include tests (default: OFF).
- ``TADAH_BUILD_DOCUMENTATION``: Build documentation with Doxygen (default: OFF).

Tadah! Software Binary
----------------------

The executable `tadah` is installed in the `bin` directory by default (${CMAKE_INSTALL_PREFIX}/bin). If MPI is enabled, the executable is named `tadah_mpi`.

Example Commands
................

- Display help information:

  .. code-block:: bash

    tadah -h

- Train a model with a specified configuration file:

  .. code-block:: bash

    tadah train -c <config_file> -V

  Use the `-V` flag for verbose output during training.

- Make predictions using a trained potential file and datasets:

  .. code-block:: bash

    tadah predict -p <pot.tadah> -d <dataset1> <dataset2> -V

Massively Parallel Version
..........................

The Tadah! MPI version is designed for High-Performance Computing (HPC) architectures, making it suitable for handling large datasets like those from the Materials Project. This enables efficient computation and model training at scale.

To enable the MPI version, configure the build with the `TADAH_BUILD_MPI` option:

.. code-block:: bash

    cmake .. -DTADAH_BUILD_MPI=ON

This will configure the executable as `tadah_mpi`, which is specifically optimized for parallel execution.

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
     git clone https://git.ecdf.ed.ac.uk/tadah.lammps.git
     cd /path/to/lammps/src
     make lib-tadah.lammps
     make yes-ml-tadah
     make serial

.. important:: Compatible with LAMMPS from June 2022 onwards.

.. seealso:: For building LAMMPS with extra features, visit:

             https://lammps.sandia.gov/doc/Build_package.html

.. _hyperparameter_optimisation:

Hyperparameter Optimisation
---------------------------

A common challenge in developing machine learning interatomic potentials (MLIPs) is that while obtaining a model that performs well on static validation sets is relatively straightforward, it takes significant time to adjust a model for stable molecular dynamics (MD) trajectories. However, even such a model might not work well beyond the bounds of the training data.

Tadah! uses a physics-informed approach, where the model is iteratively improved based on MD simulation outcomes. Starting with models that fail and progressing to a working model, this process optimizes key features such as model complexity, resulting in increased computational performance and greatly increased transferability beyond the training dataset. The iterative process is performed automatically.

An external optimization loop tweaks the model's hyperparameters, which are then used to train the ML model and test it with MD simulations. This allows for automatic refinement with minimal user input.

Example Command
...............

To use Hyperparameter Optimisation (HPO), LAMMPS must be compiled with the Tadah! interface. Tadah! will link to this version of LAMMPS.

Run HPO using the following command:

.. code-block:: bash

    tadah hpo -t <targets> -c <config_file> -v <config_validation>

HPO allows automatic model adjustments by running custom LAMMPS simulations, scoring them, and using these scores to guide the training process.

For additional examples, please see the examples section in the left-hand sidebar.

Libraries and C++ Headers
.......................................

Headers and libraries are installed according to `-DCMAKE_INSTALL_PREFIX` or default locations. Set `LD_LIBRARY_PATH` and `CPATH` as needed.

.. code-block:: bash

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/lib
    export CPATH=$CPATH:/path/to/include

Compiled Libraries
------------------

The following libraries are compiled, depending on the build settings:

- `libtadah.core`
- `libtadah.models`
- `libtadah.mlip`
- `libtadah.md`
- `libtadah.hpo` (an MPI version: `libtadah.mpi.hpo`)

.. _compiling_and_linking:

Libraries - Compiling and Linking
-------------------------------

For C++ library use, compile with C++17 or higher. Include library headers using the prefix `tadah/`.

.. code-block:: c

    #include <tadah/mlip/atom.h>

Link with `ltadah.mlip`.

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp -ltadah.mlip

If the library is outside the standard path:

.. code-block:: bash

    g++ -O3 -std=c++17 -o output_file test.cpp -I/home/$USER/.local/include -L/home/$USER/.local/lib -ltadah.mlip

.. _gofasterstripe:

Go Faster Stripe - Desktop version
----------------------------------

OpenMP is supported if available. Set thread usage:

.. code-block:: bash

    export OMP_NUM_THREADS=num_of_cores

Always link `fopenmp` when compiling with Tadah!. 

.. _troubleshooting:

Troubleshooting
---------------

#. **Permission Denied**
   
   Run `make install` with `sudo` or change the installation path:
   
   .. code-block:: bash

     cmake .. -DCMAKE_INSTALL_PREFIX=~/.local
     make install

#. **Shared Object ImportError**

   Update `LD_LIBRARY_PATH`:

   .. code-block:: bash

     export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/your/path/

#. **Compatibility Issues with LAMMPS**

   Ensure compatibility with the correct LAMMPS version. See :ref:`installation_lammps`.

#. **New Issues**

   Contact support for any unlisted issues.

