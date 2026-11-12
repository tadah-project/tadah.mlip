[![docs](https://readthedocs.org/projects/tadah/badge/?version=v1-dev)](https://tadah.readthedocs.io/v1-dev/)
[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-2.0.0-blue.svg)](https://github.com/tadah-project/tadah.mlip/tags)

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

- [Documentation](https://tadah.readthedocs.io/v1-dev/)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)

## Quickstart

The [installation guide](https://tadah.readthedocs.io/v1-dev/installation.html) and the
[quick start](https://tadah.readthedocs.io/v1-dev/quickstart.html) describe the build and a first run.

## Status & API stability

Tadah!MLIP is in public beta. CLI, interfaces and file formats are subject to change.

## Reporting issues & getting help

- File an issue: <https://github.com/tadah-project/tadah.mlip/issues>
- E-mail: marcin.kirsz@ed.ac.uk

A short reproducer (config files plus the failing command line) is
worth a thousand words.

## Contributing

Bug reports, feature requests and pull requests are welcome:
[CONTRIBUTING.md](CONTRIBUTING.md) says how a change is proposed, built and tested.

## License

GPL-3.0; see [LICENSE](LICENSE).

[lammps]: https://github.com/tadah-project/tadah.lammps
[group]:  https://www.ph.ed.ac.uk/people/graeme-ackland
