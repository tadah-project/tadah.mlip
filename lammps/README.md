# lammps

`pair_style tadah` for LAMMPS (the package ML-TADAH), which runs potentials
trained with Tadah!. The package is not part of the LAMMPS distribution: a
LAMMPS configure step takes three more options, which name this directory
of a clone of the Tadah! repository.

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
cmake --build build --parallel
```

The [installation guide](https://tadah.readthedocs.io/latest/installation.html) gives the requirements and
the build that runs on several MPI ranks, and
[integration/README.md](integration/README.md) describes the integration
files.
