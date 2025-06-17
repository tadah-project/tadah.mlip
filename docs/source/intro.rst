.. _introduction:

Introduction
============

Tadah! is a fast and modular machine learning **software** and C++ **library** specifically for interatomic potential development. It’s built in modern C++ to provide an easy-to-use, modular, and extensible toolkit.

Tadah! offers a `LAMMPS` interface compatible with all descriptors and models. It can be used as command-line software for training models or using machine learning potentials for prediction, or as an advanced C++ library.

Key Features
------------

- **LAMMPS Integration**: Fully interfaced.
- **Community-Driven**: Open to new ideas and implementations.
- **Speed**: Fast model development cycles reduce wait times.
- **Regular Updates**: Frequent new features and optimizations.
- **Open-Source**: Publicly accessible and modifiable.
- **Modular Design**: Mix and match descriptors for flexible testing.
- **Extensible**: Easy addition of new descriptors.
- **Desktop and MPI Versions**: Suitable for various computational needs.

Available Tools
---------------

- **Training**: Train models using configuration files to generate interatomic potentials.
- **Prediction**: Use trained models to predict energies and optionally forces and stresses.
- **Nested Fitting**: Optimize model architecture and hyperparameters.
- **Database Conversion**: Process computational datasets into Tadah! database format.
- **Database Management**: Identify duplicates, join, split, and sample datasets.
- **Structure Readers**: Access online datasets and read local files.
- **Structure Writers**: Save structures in `CASTEP`, `VASP` or `LAMMPS` format.
- **Descriptor Calculation**: Calculate descriptors for datasets using a potential file.
- **Analysis Toolkit**: Visualize basis functions and cutoffs.
- **Properties Toolkit**: Calculate pairwise energy between two atoms.

.. _obtaining_tadah:

Obtaining Tadah!
----------------

The Tadah! codebase is divided into several interacting modules. There are two user-facing modules: `Tadah!MLIP`, which provides a set of tools for the development of MLIPs, and `Tadah!LAMMPS`, which allows deployment of trained models using molecular dynamics with `LAMMPS`.

The code repository is available at:

    https://git.ecdf.ed.ac.uk/tadah

