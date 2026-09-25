[![pipeline](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/badges/develop/pipeline.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/commits/develop)
[![coverage](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/badges/develop/coverage.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/commits/develop)
[![docs](https://readthedocs.org/projects/tadah/badge/?version=develop)](https://tadah.readthedocs.io/develop/)
[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.5.1-blue.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/tags)

# Tadah!MLIP

Tadah!MLIP is a fast, modular C++ toolkit for **developing machine
learning interatomic potentials** (MLIPs). It provides the `tadah` CLI
for training, prediction, dataset wrangling, and a hyperparameter
optimisation engine that folds physical-property targets (elastic
constants, equation-of-state points, surface energies, …) directly
into the loss. Trained potentials are deployable in LAMMPS via the
sister project [Tadah!LAMMPS][lammps].

It is built and maintained by [Prof. Ackland's group][group] at the
University of Edinburgh.

## Quick links

- [Documentation](https://tadah.readthedocs.io/develop/)
- [HPO examples](https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [Citation](CITATION.cff)

## Quickstart

Tadah!MLIP installs a single `tadah` binary plus a small set of
companion libraries.

```bash
# 1. Clone the development line, v1-dev, and configure with the bundled
#    defaults preset. For a release, clone its tag instead
#    (git clone -b v1.6.0 ...) and add -DTADAH_GIT_BRANCH=v1.6.0.
git clone -b v1-dev https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git
cd tadah.mlip
cmake -S . -B build -C cmake/presets/tadah-linux-defaults.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local

# 2. Build and install.
cmake --build build --parallel
cmake --install build

# 3. Train a tiny example potential.
cd docs/source/quickstart
tadah train -c config.train -v 2

# 4. Predict on a held-out dataset.
tadah predict -p pot.tadah -d db.predict -A
```

Optional features (OpenMP, MPI, the `tadah hpo` Nested Fitting
Procedure) and the full configuration matrix (Develop vs. Release
builds, troubleshooting) are covered in the
[installation guide](https://tadah.readthedocs.io/develop/installation.html).

## Status & API stability

Tadah!MLIP is in **public beta**. The CLI surface — subcommand names,
flags, configuration-file keys, and the `pot.tadah` file format — is
considered stable for the duration of the 1.x series; existing scripts
should keep working as we ship bug fixes and feature additions. Any
necessary breaking change will be called out in the [changelog](CHANGELOG.md)
with a deprecation window. The C++ API (the `libtadah.*` libraries and
their headers) is **not yet stable**: function signatures and class
layouts may change between minor versions until 2.0.

## Reporting issues & getting help

- File an issue: <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>
- E-mail: marcin.kirsz@ed.ac.uk

A short reproducer (config files plus the failing command line) is
worth a thousand words.

## Contributing

Bug reports, feature requests and merge requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the developer build (multi-repo
sibling layout, `TADAH_PATH`, FetchContent overrides), test workflow,
and merge-request checklist.

## License

GPL-3.0; see [LICENSE](LICENSE).

[lammps]: https://git.ecdf.ed.ac.uk/tadah/tadah.lammps
[group]:  https://www.ph.ed.ac.uk/people/graeme-ackland
