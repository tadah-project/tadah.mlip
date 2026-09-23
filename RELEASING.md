# Releasing Tadah!

This document captures the multi-repo release process. It lives in
the umbrella project (`tadah.mlip`); the per-module repos do not
duplicate it.

## Repos in tag order

A coordinated release tags the following repos in this order. The
order matters because each later repo's CI fetches earlier repos by
tag during the `Release` build:

1. `cmake-tools`
2. `core`
3. `models`
4. `mlip`
5. `md`
6. `hpo`
7. `tadah.lammps`
8. `tadah.mlip` (umbrella — last; this is the user-visible release)

## Single source of truth: blueprint version

The version string is a single field, `version = "..."`, inside
`CORE/blueprint/blueprint_tadah.toml`. Every repo's CMake reads it
through `cmake-tools/cmake/TadahVersion.cmake`. Bumping the version
means editing exactly that one line in `core` and tagging.

Pre-release suffixes (`-beta.1`, `-rc.1`, etc.) are allowed in the
TOML; CMake's `project(VERSION)` call sees only the numeric M.m.p
portion (handled by `tadah_read_version_from_blueprint`).

Each module reads the version *after* its Tadah dependencies have been
made available, so CORE's blueprint is found in both the sibling
(developer) and the FetchContent layout; a missing blueprint or version
line is a configure error, never a `0.0.0` default. The blueprint is a
configure dependency, so a bump re-runs CMake on the next build. The
Sphinx docs read the same file (`docs/source/config/source_locator.py`:
`$TADAH_CORE_SOURCE_DIR`, the sibling CORE checkout, `$TADAH_PATH/CORE`,
or a clone of the built branch on CI/RTD; see CONTRIBUTING.md, "Building
the documentation"), and `ctest -R tadah_version` checks that
`tadah --version` matches it.

Not derived from the blueprint, update them by hand with each release:
the version badge in every repo's `README.md`, `CITATION.cff`
(`version`, `date-released`) and the `CHANGELOG.md` files.

## Process for a beta / RC / final release

1. **Open a release branch on `core`**: `release/v<X.Y.Z>` off
   `develop`. Bump `version` in `blueprint_tadah.toml`. MR + merge.
2. **Tag in order** (steps 1–8 above): `git tag -a v<X.Y.Z> -m "..."`
   then `git push origin v<X.Y.Z>` for each repo.
3. **Watch the `release` CI stage** on each repo — it produces a
   tarball in the GitLab Generic Package Registry. The tag-only
   examples job on HPO runs `--full` (≈52 s) and checks against
   reference fingerprints; if it fails, do not advance to step 4.
4. **Cut the umbrella tag** (`tadah.mlip` `v<X.Y.Z>`). The umbrella's
   `release` CI archives the umbrella source and produces the docs
   build for ReadTheDocs.
5. **Mint a Zenodo DOI** (umbrella tag). Update `CITATION.cff` on
   `develop` to replace the `TBD` identifier with the real DOI for
   the next release.
6. **Open a GitLab Release** on the umbrella project pointing at the
   tag, the package registry tarballs, and a short changelog summary
   (link to `CHANGELOG.md` for the full list).

## Hotfixes

Patch releases follow the same order but branch off `main` (or the
release tag) on each affected repo. If the fix is local to one repo,
you still bump the blueprint version on `core` and re-tag every repo
so the umbrella build sees a consistent set — Tadah!'s "single
version across all modules" invariant is the simplest part of the
release contract and worth preserving.

## Visibility check before a public release

Before announcing a release, verify each repo's GitLab visibility is
**Public** (not *Internal*). The list:

- <https://git.ecdf.ed.ac.uk/tadah/cmake-tools>
- <https://git.ecdf.ed.ac.uk/tadah/core>
- <https://git.ecdf.ed.ac.uk/tadah/models>
- <https://git.ecdf.ed.ac.uk/tadah/mlip>
- <https://git.ecdf.ed.ac.uk/tadah/md>
- <https://git.ecdf.ed.ac.uk/tadah/hpo>
- <https://git.ecdf.ed.ac.uk/tadah/tadah.lammps>
- <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip>
