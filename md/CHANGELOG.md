# Changelog

All notable changes to **Tadah.MD** (the MD-side regressors of Tadah!LAMMPS and HPO) are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [2.0.0] — unreleased

### Added
- Tests: `tadah.md_headers` (`ctest -L headers`, cmake-tools' `tadah_add_header_check`) compiles every header of `include/` on its own and twice in a row, with what a user of `tadah.md` gets. Every MD header passes.
- Tests: `tadah.md_text` (`ctest -L text`, cmake-tools' `tadah_add_text_check`) reads the text files of the repository (those git tracks or would track, in a clone) and fails on a line that carries a person's initials, a reference to co-workers, a decision attributed to the code's owner, the name of an AI tool, a note kept outside the repository or filler wording.

### Changed
- **The sources are published at <https://github.com/tadah-project>.** Bugs and feature requests go to the issue tracker of the umbrella project, `tadah.mlip`, and security issues to its private vulnerability reporting (`SECURITY.md`). The README carries no CI badges.
- Framework version `2.0.0`, read from CORE's `blueprint/blueprint_tadah.toml` after the Tadah dependencies are fetched, so the module reports the right version in every layout (sibling checkout or FetchContent). A missing blueprint is now a configure error instead of a silent `0.0.0`.
- Tests take Catch2 and its test discovery from cmake-tools (`tadah_setup_tests()`, `include(Catch)`); the vendored `tests/catch2/catch.hpp`, `tests/Catch.cmake` and `tests/CatchAddTests.cmake` are removed. Every discovered CTest entry now selects exactly its own test case and passes `--warn NoTests`, so an entry that matches no test case fails instead of passing. New test sources are picked up without a manual re-configure (`CONFIGURE_DEPENDS`).
- cmake-tools is fetched at `v1-dev` instead of `main`.
- **The refusal of an in-source build states the rule it checks and gives an example**: `In-source builds are not allowed. The build directory must differ from the source directory (for example: cmake -S . -B build).` (1.5.0: `... Create a separate build directory and run CMake from there.`).
- The README links and the pointers to the umbrella's community files (`CITATION.md`, `CONTRIBUTING.md`, …) point to the `v1-dev` line: the `v1-dev` branch, `https://tadah.readthedocs.io/v1-dev/`. They pointed to `develop`.

## [1.4.0] — 2026-07-14

Nothing yet.

## [1.3.0-beta.1] — 2026-05-20

A small module with mostly cross-cutting build / infrastructure updates.

### Changed
- C++ standard upgraded from C++14 to **C++17**.

### Fixed
- `M_MD_Base` factory now uses `Registry::instance()` instead of a static map — fixes static-initialisation order across translation units.

### Added
- `TADAH_ENABLE_ASAN` CMake option.

### Internal
- Substantive README rewrite.
- Community / governance pointer files added.
- CI/CD: lint + release stages, VERSION wired from blueprint TOML.
- Test build routed through `cmake-tools/cmake/TadahTesting.cmake` helpers (`tadah_declare_testing` / `tadah_add_tests`).

## [1.2.0] and earlier (condensed)

The `v1.0.0` → `v1.1.1` releases were the "internal playground" era. MD remained a thin orchestration layer in this period; releases primarily comprised build-system fixes and typo corrections. No published changelog covers this era; treat anything before `1.3.0-beta.1` as undocumented in the modern sense.
