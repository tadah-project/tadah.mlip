# Releasing Tadah!

This document captures the multi-repo release process. It lives in
the umbrella project (`tadah.mlip`); the per-module repos do not
duplicate it. The examples use the 2.0.0 release.

## Branches and tags

`v1-dev` is the development branch of the 1.x line in the seven Tadah
repos (core, models, mlip, md, hpo, tadah.lammps, tadah.mlip) and in
cmake-tools; it replaces `develop`. Releases are made from `v1-dev`, and
every repo gets the same tag, `v<X.Y.Z>` (`v2.0.0`).

## Which module builds on which

Every module adds the Tadah modules it builds on from the directories of
the repository (`tadah_add_module` in its `CMakeLists.txt`):

| Module | Builds on |
|------|---------|
| `core` | — |
| `models` | `core` |
| `mlip` | `models` |
| `md` | `models` |
| `tadah.lammps` | `md` |
| `hpo` | `mlip`, `md`, `tadah.lammps` |
| `tadah.mlip` | `mlip`; with `TADAH_ENABLE_NFP`, `hpo` instead |

Each of them also adds cmake-tools, the directory `cmake-tools` of the
repository.

## Repos in tag order

A coordinated release tags the following repos in this order:

1. `cmake-tools`
2. `core`
3. `models`
4. `mlip`
5. `md`
6. `tadah.lammps`
7. `hpo`
8. `tadah.mlip` (umbrella — last; this is the user-visible release)

## Single source of truth: blueprint version

The version string is a single field, `version = "..."`, in the
`[TADAH]` table of `CORE/blueprint/blueprint_tadah.toml`. Every repo's
CMake reads it through `cmake-tools/cmake/TadahVersion.cmake`. Bumping
the version means editing exactly that one line in `core`.

Pre-release suffixes (`-beta.1`, `-rc.1`, etc.) are allowed in the
TOML; CMake's `project(VERSION)` call sees only the numeric M.m.p
portion (handled by `tadah_read_version_from_blueprint`).

Each module reads the version *after* the Tadah modules it builds on
have been added, so CORE's blueprint is found; a missing blueprint or version
line is a configure error, never a `0.0.0` default. The blueprint is a
configure dependency, so a bump re-runs CMake on the next build. The
Sphinx docs read CORE's blueprint as well (`docs/source/config/source_locator.py`:
`$TADAH_CORE_SOURCE_DIR`, else the directory `CORE` of the repository; see
CONTRIBUTING.md, "Building the documentation"), and `ctest -R
tadah_version` checks that `tadah --version` matches it.

Not derived from the blueprint, update them by hand with each release:
the version badge in every repo's `README.md`, `CITATION.cff`
(`version`, `date-released`), the `CHANGELOG.md` files (the
`## [X.Y.Z] — <date>` heading in every repo; cmake-tools has no
changelog), and the release tag the install instructions name
(`git clone -b v2.0.0`; `grep -rn 'v2\.0\.0'`
over `docs/source/installation.rst`, the READMEs and the Tadah.LAMMPS
READMEs finds them).

## What CI does

Pushing a branch runs that repo's pipeline and triggers the repos
downstream of it on the branch of the same name (trigger jobs, which do
not run on tags):

- `core` triggers `models`;
- `models` triggers `mlip` and `md`;
- `mlip` triggers `tadah.mlip`;
- `md` triggers `tadah.lammps`;
- `hpo` has no trigger, and its build jobs are manual (they need a
  LAMMPS library, `TADAH_LAMMPS_DIR`, which the runner image does not
  provide). The `tadah.mlip` jobs configure without
  `TADAH_ENABLE_NFP`, so no pipeline builds or tests HPO: validate it
  locally.

A tag pipeline runs the build jobs and `release_tarball`, which uploads
a `git archive` of the tag to the project's Generic Package Registry. A tarball holds
every module.
`lint_codespell` and `docs_sphinx` do not run on tags (`when: never`).

## Development and public repositories

Development happens in the GitLab group `tadah` (the `origin` remote of
every checkout): pushes, merges, tags and CI. The public repositories
are the GitHub organisation `tadah-project`, one repository per GitLab
project with the same name. Each GitLab project has a push mirror
(*Settings → Repository → Mirroring repositories*) to
`ssh://git@github.com/tadah-project/<repo>.git`, authenticated with the
SSH key GitLab generated for that mirror, added as a write deploy key of
the GitHub repository; it mirrors the protected branches (`main`,
`v1-dev`) and runs after every push. Nothing is pushed to GitHub by
hand: a commit made there is overwritten by the next mirror update.

A pull request opened on GitHub is fetched
(`git fetch github pull/<n>/head:pr-<n>`), merged into `v1-dev` without
rewriting its commits and pushed to GitLab; the mirror then marks it
merged. Issues are taken on `tadah.mlip` and `tadah.lammps` only.

The documentation build reads the base address of the repositories from
`TADAH_GIT_BASE` (`source_locator.py`, the `docs_sphinx` job), by default
`https://github.com/tadah-project`. Every `.gitlab-ci.yml` sets it to
`${CI_SERVER_URL}/${CI_PROJECT_NAMESPACE}`, so that CI clones from
GitLab and never waits for the mirror. When the GitLab group is private,
a group CI/CD variable `TADAH_GIT_BASE` (masked) with the value
`https://gitlab-ci-token:${CI_JOB_TOKEN}@<host>/tadah` takes precedence,
and every project allows the group's job tokens (*Settings → CI/CD →
Job token permissions*).

## Read the Docs

CI does not publish the documentation. Read the Docs builds it from
`https://github.com/tadah-project/tadah.mlip` (its GitHub integration
starts a build on every mirror update), with `.readthedocs.yaml`: a docs-only CMake configure
(`-DTADAH_BUILD_DOCS_ONLY=ON`) and `make docs` for the Doxygen XML, then
Sphinx with `docs/source/conf.py`. The configure takes the modules from
the directories of the repository, and `conf.py` reads CORE and HPO from
them (`source_locator.py`).

In the Read the Docs dashboard (admin access):

- set the project's *Default branch* to `v1-dev`: Read the Docs builds
  its `latest` version from the default branch;
- set the *Default version*, the one the site root
  (`https://tadah.readthedocs.io/`, linked from `CITATION.cff` and the
  installation guide) serves: `v1-dev`, or `stable` once a release tag
  exists (`stable` builds the highest version tag);
- activate the `v1-dev` version and, once the tags exist, the `v2.0.0`
  version.

The READMEs link to `https://tadah.readthedocs.io/v1-dev/`.

## Process for a release

1. **Prepare `v1-dev`** in every repo: the blueprint `version`
   (`2.0.0`), the README version badges, `CITATION.cff` (`version`,
   `date-released`) and the `## [2.0.0]` CHANGELOG sections. Validate
   locally: full `ctest` (which also runs the CLI examples the
   documentation includes) and a Sphinx build with `-W`.
2. **Push `v1-dev`** and watch the pipelines.
3. **Tag cmake-tools**: `git tag -a v2.0.0 -m "Tadah! 2.0.0"` on its
   `v1-dev`, then `git push origin v2.0.0`.
4. **Release commit** on `v1-dev` in each of the seven repos: set the
   date of the `## [2.0.0]` CHANGELOG heading (`## [2.0.0] — unreleased`
   until then) and `date-released` in `CITATION.cff` to the tag date.
   Push it.
5. **Tag the modules in tag order** (`core`, `models`, `mlip`, `md`,
   `tadah.lammps`, `hpo`, `tadah.mlip`): `git tag -a v2.0.0 -m "Tadah!
   2.0.0"`, then `git push origin v2.0.0`. Let each tag pipeline finish
   (build and `release_tarball`).
6. **Read the Docs**: activate `v2.0.0` and check that its build and the
   `v1-dev` build succeed.
7. **Mint a Zenodo DOI** for the umbrella tag, and add it to
   `CITATION.cff` on `v1-dev`.
8. **Open a GitHub Release** on `tadah-project/tadah.mlip` (and
   `tadah.lammps`) for the tag, once the mirror carries it:
   `gh release create v2.0.0 --repo tadah-project/tadah.mlip`, with the
   `release_tarball` archives of the tag pipelines attached and a short
   changelog summary (link to `CHANGELOG.md` for the full list).

## Hotfixes

Patch releases follow the same order from the branch that carries the
fix. If the fix is local to one repo, you still bump the blueprint
version on `core` and re-tag every repo so the umbrella build sees a
consistent set — Tadah!'s "single version across all modules" invariant
is the simplest part of the release contract and worth preserving.

## Visibility check before a public release

Before announcing a release, verify that each GitHub repository is
public and that its mirror is current (`git ls-remote` shows the
release tag and the `v1-dev` of GitLab). The list:

- <https://github.com/tadah-project/cmake-tools>
- <https://github.com/tadah-project/core>
- <https://github.com/tadah-project/models>
- <https://github.com/tadah-project/mlip>
- <https://github.com/tadah-project/md>
- <https://github.com/tadah-project/hpo>
- <https://github.com/tadah-project/tadah.lammps>
- <https://github.com/tadah-project/tadah.mlip>
