[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/tadah-project/models/tags)

# MODELS

Physics and machine-learning components of the
[Tadah!](https://tadah.readthedocs.io/v1-dev/) MLIP toolchain. MODELS
implements:

- **Regressors** — `M_KRR` (kernel ridge regression) and `M_BLR`
  (Bayesian linear regression).
- **Kernels** — `Kern_Linear`, `Kern_LQ`, `Kern_Polynomial`,
  `Kern_Quadratic`, `Kern_RBF`, `Kern_Sigmoid`.
- **Two-body descriptors** — Behler-Parrinello (`D2_BP`), B-spline
  (`D2_Blip`), tabulated EAM (`D2_EAM`), Mie (`D2_MIE`), ZBL screening
  (`D2_ZBL`), Lennard-Jones (`D2_LJ`), composite (`D2_mJoin`).
- **Many-body descriptors** — embedded atom (`DM_EAM`), modified EAM
  (`DM_EAD`, `DM_mEAD`), B-spline (`DM_Blip`), polynomial
  (`DM_Poly`), composite (`DM_mJoin`).
- **Cutoff functions** — `Cut_Cos`, `Cut_CosS`, `Cut_Poly`,
  `Cut_PolyS`, `Cut_PT`, `Cut_Tanh`, `Cut_SinInv`.
- **Embedding functions** — `F_RLR`, `F_SQRT`, `F_Blip`, `F_mEnv`.

These classes are referenced by name in user-facing configuration
files; for the user view see
[descriptors](https://tadah.readthedocs.io/v1-dev/descriptors.html),
[cutoffs](https://tadah.readthedocs.io/v1-dev/cutoffs.html), and
[regressors](https://tadah.readthedocs.io/v1-dev/regressors.html)
in the umbrella documentation.

## You don't need to use this directly

MODELS is an internal module of the Tadah! MLIP toolchain. It is
fetched automatically (via CMake `FetchContent`) by MLIP, MD, and HPO. End users install the umbrella project
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip) — see the
[installation guide](https://tadah.readthedocs.io/v1-dev/installation.html).

## Contributing

Build instructions, dependencies, layout, and the recipe for adding a
new descriptor live in [CONTRIBUTING.md](CONTRIBUTING.md). The full
multi-repo contributor guide is on the umbrella project.

## License

GPL-3.0; see [LICENSE](LICENSE).

## Reporting issues

This repository is a component of Tadah! and has no issue tracker of
its own. Bugs, feature requests and questions go to the tracker of
the umbrella project,
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip/issues).
