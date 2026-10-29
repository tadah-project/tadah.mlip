# Releasing Tadah!

This document captures the multi-repo release process. It lives in
the umbrella project (`tadah.mlip`); the per-module repos do not
duplicate it. The examples use the 1.6.0 release.

## Branches and tags

`v1-dev` is the development branch of the 1.x line in the seven Tadah
repos (core, models, mlip, md, hpo, tadah.lammps, tadah.mlip) and in
cmake-tools; it replaces `develop`. Releases are made from `v1-dev`, and
every repo gets the same tag, `v<X.Y.Z>` (`v1.6.0`).

## Which repo fetches which

Every module fetches the Tadah modules it builds on with
`GIT_TAG ${TADAH_GIT_BRANCH}` (its top-level `CMakeLists.txt`):

| Repo | Fetches |
|------|---------|
| `core` | — |
| `models` | `core` |
| `mlip` | `models` |
| `md` | `models` |
| `tadah.lammps` | `md` |
| `hpo` | `mlip`, `md`, `tadah.lammps` |
| `tadah.mlip` | `mlip`; with `TADAH_ENABLE_NFP`, `hpo` instead |

Each of them also fetches cmake-tools, at the `GIT_TAG` pinned in its
`CMakeLists.txt` (see *cmake-tools pin*), not at `TADAH_GIT_BRANCH`.

## Repos in tag order

A coordinated release tags the following repos in this order, each
after every repo it fetches:

1. `cmake-tools`
2. `core`
3. `models`
4. `mlip`
5. `md`
6. `tadah.lammps`
7. `hpo`
8. `tadah.mlip` (umbrella — last; this is the user-visible release)

The order matters because a repo's tag pipeline fetches the repos it
builds on at that tag: cmake-tools' `tadah_get_git_branch` sets
`TADAH_GIT_BRANCH` from `$CI_COMMIT_REF_NAME`, which is the tag name on
a tag pipeline, so every repo in its row of the table above must carry
the tag already. cmake-tools itself is fetched at the pinned `GIT_TAG`,
so it is tagged first and the modules pin its tag.

## cmake-tools pin

Each of the seven repos fetches cmake-tools in its top-level
`CMakeLists.txt`:

```cmake
FetchContent_Declare(tadah_cmake
  GIT_REPOSITORY  ${TADAH_GIT_BASE}/cmake-tools.git
  GIT_TAG         v1-dev)
```

`TADAH_GIT_BASE` comes from the block of
`cmake-tools/testing/tadah_git_base_block.cmake`, which every one of the
seven `CMakeLists.txt` carries unchanged before its first
`FetchContent_Declare` (`<name>_git_base`, `ctest -L cmake`, checks it).

During development the pin is `GIT_TAG v1-dev`. At a release, tag
cmake-tools `v1.6.0` first; then the release commit of each of the seven
repos switches the pin from `GIT_TAG v1-dev` to `GIT_TAG v1.6.0`, before
any module is tagged, so a module's tag always builds with the tagged
cmake-tools. Once the tags are pushed, a commit on `v1-dev` switches the
pins back to `GIT_TAG v1-dev`, so that development builds follow
cmake-tools' `v1-dev` again.

## Single source of truth: blueprint version

The version string is a single field, `version = "..."`, in the
`[TADAH]` table of `CORE/blueprint/blueprint_tadah.toml`. Every repo's
CMake reads it through `cmake-tools/cmake/TadahVersion.cmake`. Bumping
the version means editing exactly that one line in `core`.

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
or, on GitLab CI and Read the Docs, a clone of the ref being built; see
CONTRIBUTING.md, "Building the documentation"), and `ctest -R
tadah_version` checks that `tadah --version` matches it.

Not derived from the blueprint, update them by hand with each release:
the version badge in every repo's `README.md`, `CITATION.cff`
(`version`, `date-released`), the `CHANGELOG.md` files (the
`## [X.Y.Z] — <date>` heading in every repo; cmake-tools has no
changelog), and the release tag the install instructions name
(`git clone -b v1.6.0`, `-DTADAH_GIT_BRANCH=v1.6.0`; `grep -rn 'v1\.6\.0'`
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

A tag pipeline runs the build jobs, which fetch the sibling modules at
the tag, and `release_tarball`, which uploads a `git archive` of the
tag to the GitLab project's Generic Package Registry. A tarball has no
`.git`, so `tadah_get_git_branch` finds no branch in it: users who
build from a tarball pass the tag, `-DTADAH_GIT_BRANCH=v1.6.0`
(installation guide, "Which ref the Tadah! modules are fetched at").
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

Builds read the base address of the repositories from `TADAH_GIT_BASE`
(CMake, `source_locator.py`, `Tadah.LAMMPS/configure`), by default
`https://github.com/tadah-project`. Every `.gitlab-ci.yml` sets it to
`${CI_SERVER_URL}/${CI_PROJECT_NAMESPACE}`, so that CI fetches from
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
Sphinx with `docs/source/conf.py`. The configure fetches the modules at
the ref `tadah_get_git_branch` names, and `conf.py` clones CORE and HPO
at the same ref (`source_locator.py`). On Read the Docs that ref is
`$READTHEDOCS_GIT_IDENTIFIER`, the branch or tag Read the Docs checked
out (the version names `latest` and `stable` are not refs), so every
module must have that branch or tag on GitHub.

In the Read the Docs dashboard (admin access):

- set the project's *Default branch* to `v1-dev`. Read the Docs builds
  its `latest` version from the default branch, and
  `$READTHEDOCS_GIT_IDENTIFIER` then names that branch, so it must exist
  in every module;
- set the *Default version*, the one the site root
  (`https://tadah.readthedocs.io/`, linked from `CITATION.cff` and the
  installation guide) serves: `v1-dev`, or `stable` once a release tag
  exists (`stable` builds the highest version tag, which every module
  carries too);
- activate the `v1-dev` version and, once the tags exist, the `v1.6.0`
  version.

The READMEs link to `https://tadah.readthedocs.io/v1-dev/`.

## Pushing `v1-dev`

Push cmake-tools `v1-dev` first: every other repo fetches it. Then push
the modules from the leaves to the root, each before every repo it
fetches (see *Which repo fetches which*), `core` last:

1. `cmake-tools`
2. `tadah.mlip`
3. `hpo`
4. `tadah.lammps`
5. `md`
6. `mlip`
7. `models`
8. `core`

Each push triggers the pipelines downstream of it, and the triggered
pipelines fetch the modules at `v1-dev`; pushing `core` last runs the
whole chain once more against the pushed modules. To avoid the
intermediate pipelines, push all but `core` with
`git push -o ci.skip origin v1-dev` and let `core`'s push trigger the
chain.

## Process for a release

1. **Prepare `v1-dev`** in every repo: the blueprint `version`
   (`1.6.0`), the README version badges, `CITATION.cff` (`version`,
   `date-released`) and the `## [1.6.0]` CHANGELOG sections. Validate
   locally: full `ctest` (which also runs the CLI examples the
   documentation includes) and a Sphinx build with `-W`.
2. **Push `v1-dev`** in the order above and watch the pipelines.
3. **Tag cmake-tools**: `git tag -a v1.6.0 -m "Tadah! 1.6.0"` on its
   `v1-dev`, then `git push origin v1.6.0`.
4. **Release commit** on `v1-dev` in each of the seven repos: switch the
   cmake-tools pin to `GIT_TAG v1.6.0`, and set the date of the
   `## [1.6.0]` CHANGELOG heading (`## [1.6.0] — unreleased` until
   then) and `date-released` in `CITATION.cff` to the tag date. Push
   it, in the order above.
5. **Tag the modules in tag order** (`core`, `models`, `mlip`, `md`,
   `tadah.lammps`, `hpo`, `tadah.mlip`): `git tag -a v1.6.0 -m "Tadah!
   1.6.0"`, then `git push origin v1.6.0`. Let each tag pipeline finish
   (build and `release_tarball`) before tagging a repo that fetches it.
6. **Restore the development pin**: a commit on `v1-dev` in the seven
   repos switches `GIT_TAG v1.6.0` back to `GIT_TAG v1-dev`.
7. **Read the Docs**: activate `v1.6.0` and check that its build and the
   `v1-dev` build succeed.
8. **Mint a Zenodo DOI** for the umbrella tag, and add it to
   `CITATION.cff` on `v1-dev`.
9. **Open a GitHub Release** on `tadah-project/tadah.mlip` (and
   `tadah.lammps`) for the tag, once the mirror carries it:
   `gh release create v1.6.0 --repo tadah-project/tadah.mlip`, with the
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
