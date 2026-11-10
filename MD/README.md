[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/tadah-project/md/tags)

# MD

Molecular-dynamics specialisation layer of the
[Tadah!](https://tadah.readthedocs.io/v1-dev/) MLIP toolchain. Provides:

- `M_KRR_MD` — KRR regressor optimised for the access pattern of the
  MD evaluation kernel.
- `M_BLR_MD` — BLR regressor counterpart.
- `M_MD_Base` — common base class for MD-side regressors.

These specialisations strip away analysis-only code paths from the
generic `M_KRR` / `M_BLR` regressors in
[MODELS](https://github.com/tadah-project/models) so the LAMMPS
plugin and any other on-line evaluator can run the inner loop without
the overhead of training-time bookkeeping.

## You don't need to use this directly

MD is an internal module of the Tadah! MLIP toolchain. It is
fetched automatically (via CMake `FetchContent`) by
[Tadah!LAMMPS](https://github.com/tadah-project/tadah.lammps) (the
`pair_style tadah` implementation) and by
[HPO](https://github.com/tadah-project/hpo). End users install
the umbrella project
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip) or Tadah!LAMMPS
— see the
[installation guide](https://tadah.readthedocs.io/v1-dev/installation.html).

## Contributing

Build instructions, dependencies, and layout live in
[CONTRIBUTING.md](CONTRIBUTING.md). The full multi-repo contributor
guide is on the umbrella project.

## License

GPL-3.0; see [LICENSE](LICENSE).

## Reporting issues

This repository is a component of Tadah! and has no issue tracker of
its own. Bugs, feature requests and questions go to the tracker of
the umbrella project,
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip/issues)
or, for the LAMMPS plugin, the tracker of
[Tadah!LAMMPS](https://github.com/tadah-project/tadah.lammps/issues).
