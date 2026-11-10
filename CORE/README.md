[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/tadah-project/core/tags)

# CORE

Foundation library of the [Tadah!](https://tadah.readthedocs.io/v1-dev/) MLIP
toolchain. Provides the substrate every other Tadah! module is built
on:

- **Context** — the typed key/value store backing every configuration
  file, command-line flag, and persisted potential file.
- **Blueprint parser & validator** — drives the auto-generated CLI
  surface and the documented configuration grammar.
- **Periodic table & element metadata** — atomic numbers, masses, and
  symbol look-ups.
- **Checkpoint manager & signal handler** — resumable runs for any
  long-running engine.
- **Logger, math helpers, LAPACK shim, normaliser** — utilities every
  module re-uses.

## You don't need to use this directly

CORE is an internal module of the Tadah! MLIP toolchain. It is
fetched automatically (via CMake `FetchContent`) by every Tadah!
consumer (MODELS, MLIP, MD, HPO, Tadah.LAMMPS, Tadah.MLIP). End users
install the umbrella project
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip) — see the
[installation guide](https://tadah.readthedocs.io/v1-dev/installation.html).

## Contributing

Build instructions, dependencies, layout, and the per-module dev
workflow live in [CONTRIBUTING.md](CONTRIBUTING.md). The full
multi-repo contributor guide is on the umbrella project.

## License

GPL-3.0; see [LICENSE](LICENSE).

## Reporting issues

This repository is a component of Tadah! and has no issue tracker of
its own. Bugs, feature requests and questions go to the tracker of
the umbrella project,
[Tadah!MLIP](https://github.com/tadah-project/tadah.mlip/issues).
