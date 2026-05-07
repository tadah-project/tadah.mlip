[![pipeline](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/badges/develop/pipeline.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/commits/develop)
[![coverage](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/badges/develop/coverage.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/commits/develop)
[![docs](https://readthedocs.org/projects/tadah/badge/?version=latest)](https://tadah.readthedocs.io/en/latest/)
[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.2.0-blue.svg)](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/tags)

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

- [Documentation](https://tadah.readthedocs.io/)
- [Examples](HPO/examples/)  ← deferred to Session C for content
- [Changelog](CHANGELOG.md)  ← deferred to Session A
- [Contributing](CONTRIBUTING.md)  ← deferred to Session E
- [Citation](CITATION.cff)  ← deferred to Session E

## Quickstart

Tadah!MLIP installs a single `tadah` binary plus a small set of
companion libraries.

```bash
# 1. Clone and configure with the bundled defaults preset.
git clone https://git.ecdf.ed.ac.uk/tadah/tadah.mlip.git
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

Optional features can be enabled with `-D` flags at configure time:

| Flag | Effect |
|---|---|
| `-DTADAH_ENABLE_OPENMP=ON` | OpenMP parallelism for descriptor / regression hot paths. |
| `-DTADAH_BUILD_MPI=ON`     | Build the MPI variant `tadah_mpi`. |
| `-DTADAH_ENABLE_NFP=ON`    | Enable the Nested Fitting Procedure (`tadah hpo`). Requires LAMMPS built with [Tadah!LAMMPS][lammps]; see [the installation guide](https://tadah.readthedocs.io/en/latest/installation.html#installation-nfp). |

For the full configuration matrix (Develop vs. Release builds, MPI
considerations, troubleshooting), see the
[installation guide](https://tadah.readthedocs.io/en/latest/installation.html).

## Status & API stability

Tadah!MLIP is in **public beta**. The CLI surface — subcommand names,
flags, configuration-file keys, and the `pot.tadah` file format — is
considered stable for the duration of the 1.x series; existing scripts
should keep working as we ship bug fixes and feature additions. Any
necessary breaking change will be called out in the [changelog](CHANGELOG.md)
with a deprecation window. The C++ API (the `libtadah.*` libraries and
their headers) is **not yet stable**: function signatures and class
layouts may change between minor versions until 2.0.

The `tadah hpo` engine is feature-complete and exercised by the
fifteen reference examples in [HPO/examples/]; behavioural defaults
(``BASINS`` ladders, ``LN_BOBYQA`` refinement, basin-pool sizes) are
the result of empirical tuning on real fits and may continue to
evolve as we collect more workloads.

## Reporting issues & getting help

- File an issue: <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>
- E-mail: marcin.kirsz@ed.ac.uk

A short reproducer (config files plus the failing command line) is
worth a thousand words.

## Developer build (multi-repo workflow)

The user-facing `tadah.mlip` repository pulls every dependency
(``CORE``, ``MODELS``, ``MLIP``, ``HPO``, ``MD``, ``Tadah.LAMMPS``,
``cmake-tools``) via `FetchContent`. For day-to-day development, you
can short-circuit the fetch and point CMake at local checkouts:

1. Lay the repos out side by side under one root:

   ```text
   $TADAH_PATH/
       Tadah.MLIP/
       CORE/
       MODELS/
       MLIP/
       HPO/
       MD/
       Tadah.LAMMPS/
   ```

2. Either set `-DCMAKE_BUILD_TYPE=Develop` (which auto-resolves the
   sibling layout via `$TADAH_PATH`), or pass each module's source
   directory explicitly:

   ```bash
   cmake -S . -B build \
       -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=$TADAH_PATH/CORE \
       -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=$TADAH_PATH/MODELS \
       -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=$TADAH_PATH/MLIP \
       -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=$TADAH_PATH/HPO
   ```

   Local edits in any of those repos are picked up on the next
   `cmake --build`.

## License

GPL-3.0; see [LICENSE](LICENSE).

[lammps]: https://git.ecdf.ed.ac.uk/tadah/tadah.lammps
[group]:  https://www.ph.ed.ac.uk/people/graeme-ackland
[HPO/examples/]: https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples
