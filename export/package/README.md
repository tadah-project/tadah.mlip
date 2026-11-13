# Tadah!LAMMPS

`pair_style tadah` for LAMMPS (the package ML-TADAH), which runs potentials
trained with [Tadah!](https://github.com/tadah-project/tadah.mlip).

This directory is generated from the Tadah! repository at each release and
is not edited by hand. It holds the pair style and the parts of the Tadah!
libraries that the pair style is built from; the program that trains
potentials is in the full project:

<https://github.com/tadah-project/tadah.mlip>

Questions, defect reports and changes go to the full project as well.

## Building LAMMPS with the pair style

The package is not part of the LAMMPS distribution: a LAMMPS configure step
takes three more options, which name this directory.

```bash
TADAH=/path/to/this/directory
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=$TADAH/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=$TADAH
cmake --build build --parallel
```

The build needs CMake 3.20 or later, a C++17 compiler and a LAPACK library.
It downloads nothing, and reads nothing of Tadah! outside this directory.
The [installation guide](https://tadah.readthedocs.io/latest/installation.html)
gives the requirements in full and the build that runs on several MPI ranks.

## Contents

- `ML-TADAH/`: the pair style, compiled into the LAMMPS library.
- `integration/`: the CMake files that add the pair style to the LAMMPS
  build.
- `lammps/`, `md/`, `models/`, `core/`: the libraries the pair style is
  built on.
- `cmake/`: the build support of those libraries.
- `core/external/`: third-party sources, toml11 (MIT licence, in its
  directory) and headers of Boost.

## Licence

GPL-3.0; see [LICENSE](LICENSE).
