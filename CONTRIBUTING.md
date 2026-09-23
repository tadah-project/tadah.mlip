# Contributing to Tadah!MLIP

Thanks for your interest in Tadah! This document is the canonical
contributor guide for the multi-repo Tadah! project. Every per-module
repository points back here.

The project is hosted at <https://git.ecdf.ed.ac.uk/tadah/>.

## Repositories

Tadah! is split across several GitLab projects on `git.ecdf.ed.ac.uk`:

| Repo | Role |
|------|------|
| `tadah.mlip`   | Umbrella project — produces the `tadah` CLI, ships the docs build, drives the multi-module cmake configure. |
| `core`         | Core types, blueprint TOML, registries, parser, signal/checkpoint infrastructure. |
| `models`       | Cutoffs, descriptors, regressors, basis functions. |
| `mlip`         | Training / load-time data pipeline used by `tadah train` and friends. |
| `md`           | Molecular dynamics surface (`Tadah.MD`). |
| `hpo`          | Hyperparameter optimisation driver (`tadah hpo`). |
| `tadah.lammps` | LAMMPS plugin and `pair_tadah`. |
| `cmake-tools`  | Shared CMake helpers (version reader, install helpers, etc.). |

The working branch in every repo is `develop`. Releases are tagged off
`main`. The pre-release prep happens on `release/beta-prep` (see
[`RELEASING.md`](RELEASING.md)).

## Reporting bugs

File a bug report on the GitLab project that owns the affected code.
If you're unsure which one, file it on
<https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues> — we'll move it.

A useful bug report includes:

1. Tadah! version (`tadah --version`).
2. Build mode: `Release` or `Develop`, MPI on/off, compiler + version.
3. The minimal `config.tadah` (or `config.hpo`) that reproduces.
4. The exact command you ran and the full stderr/stdout (or a tail
   long enough to show the error).
5. If a LAMMPS run is involved, the LAMMPS input script and the
   `pair_tadah` line.

The `Bug` issue template prompts for these fields.

## Proposing features

Open a feature-request issue on the relevant GitLab project before
sending a merge request. The `Feature` issue template asks for the
motivation, the proposed surface change (CLI flag / config keyword /
API call), and any backwards-compatibility implications. A short
discussion on the issue is faster than an MR round-trip when the
design is still moving.

## Setting up a development environment

The umbrella repo (`tadah.mlip`) drives the multi-module build. The
quickest path:

```bash
# Pick a directory; clone the seven repos as siblings.
export TADAH_PATH=/path/to/your/tadah_root
mkdir -p "$TADAH_PATH" && cd "$TADAH_PATH"
for repo in cmake-tools core mlip models md hpo tadah.mlip tadah.lammps; do
  git clone "https://git.ecdf.ed.ac.uk/tadah/${repo}.git" \
            "$(echo $repo | tr '[:lower:]' '[:upper:]' | sed 's/TADAH\./Tadah./')"
done

# Develop build (uses the local sibling copies, not GitLab fetch).
cd Tadah.MLIP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Develop
cmake --build build -j
ctest --test-dir build
```

For a `Release` build (everything fetched from GitLab — no local
sibling worktrees needed):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If your local checkouts do **not** follow the `$TADAH_PATH` sibling
convention, point CMake at each module explicitly with the
`FETCHCONTENT_SOURCE_DIR_TADAH.<MODULE>` overrides — `FetchContent`
then short-circuits the GitLab clone and uses your tree:

```bash
cmake -S . -B build \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=/abs/path/CORE \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=/abs/path/MODELS \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=/abs/path/MLIP \
    -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=/abs/path/HPO
```

Local edits in any of those repos are picked up on the next
`cmake --build`.

LAMMPS integration: see `Tadah.LAMMPS/README.md` and the docs page on
LAMMPS.

## Running tests

Each repo ships a Catch2 suite, built when the project is configured
with `-DBUILD_TESTING=ON`. The umbrella build aggregates the suites of
every module it builds:

```bash
cd Tadah.MLIP/build
ctest --output-on-failure
```

Every non-hidden Catch2 test case is its own CTest entry (`ctest -N`
lists them). Hidden test cases (tags `[.]`, `[.tag]` or `[!benchmark]`)
are not discovered; they run under CTest only through an explicit entry
(see below), where one exists. The suite should run clean on `v1-dev`.

Test discovery comes from cmake-tools: each module's `tests/CMakeLists.txt`
calls `tadah_setup_tests()`, `include(Catch)` and `catch_discover_tests()`
(see the cmake-tools README). It guarantees that:

- each non-hidden test case is registered exactly once, under its exact
  name, and its entry selects exactly that case, whatever characters the
  name contains (`;`, `[`, `,`, `\`, a leading `#`, `~`, `-` or tab, ...);
- every entry passes `--warn NoTests`, so an entry that matches no test
  case fails instead of passing silently. Catch accepts `--warn` only
  once, so `EXTRA_ARGS` of `catch_discover_tests()` containing `--warn` or
  `-w` stop the build;
- a name Catch cannot select on its own stops the build with a message
  naming the test: a leading or trailing `*` (always a wildcard), a blank
  name, or a name that differs from another test case's name, hidden ones
  included, only in letter case or surrounding whitespace. Rename the test
  case.

Do not vendor `catch.hpp` or the Catch CMake scripts in a module. A hidden
tag (`[.statistical]`) runs through an explicit entry, which also needs the
flag: `add_test(NAME x COMMAND test_foo "[.tag]" --warn NoTests)`.
`ctest -R tadah_catch` runs the regression tests for the discovery itself
(`tests/catch_discovery`).

**Pitfall:** MLIP redefines `add_test()` globally (to skip Gemmi's own
test) and forwards its arguments unquoted. In every `add_test()` that runs
after MLIP is configured (in the umbrella build: MLIP, MD, HPO, Tadah.LAMMPS
and the umbrella itself), an argument containing `;` is split in two and an
empty argument is dropped. Discovered entries are not affected.

For the HPO worked-example regression suite (added during the beta
prep — fast subset runs in ~5 s):

```bash
bash HPO/examples/run_reference_suite.sh --fast
# or
bash HPO/examples/run_reference_suite.sh --full
```

The driver compares the example outputs against
`HPO/examples/<NN>/reference_fingerprint.json`. Per-example tolerance
overrides live in `HPO/examples/tools/tolerance_overrides.json`.

## Coding style

There is no committed `.clang-format`. Match the surrounding code:
two-space indent, braces on the same line for free functions and on
the next line for class definitions. Headers use `#pragma once`.
Avoid trailing whitespace; UTF-8, LF line endings, terminal newline.

`pre-commit` config is optional but recommended (see
[`.pre-commit-config.yaml`](.pre-commit-config.yaml) — codespell,
trailing-whitespace, end-of-file-fixer, no formatter so we don't
churn unrelated lines).

## Submitting a merge request

1. Fork the relevant project on `git.ecdf.ed.ac.uk/tadah/<module>` (or,
   for maintainers, push a topic branch).
2. Branch off `develop` (`git checkout -b feature/short-name develop`).
3. Keep the change focused — one logical change per MR. Cross-repo
   work that needs coordinated tags goes through the multi-repo
   release process described in `RELEASING.md`.
4. Add or update tests. Pure-refactor MRs are fine, but a behaviour
   change without a test that pins it down is a hard sell.
5. Run `ctest` in the umbrella build. Ensure the CI pipeline on your
   branch is green; the `lint_codespell` job and the existing
   `build_debug` / `build_release` / `compile_lammps_with_tadah` jobs
   must pass.
6. Write a clear MR description: motivation, summary of the change,
   anything a reviewer should pay attention to, anything explicitly
   left out of scope.

A reviewer looks for:

- Does the change do what the description says, and only that?
- Are the tests at the right level (unit vs. integration vs. example)?
- Public API or config keyword changes — are they documented and
  backwards-compatible, or explicitly called out as breaking?
- For numerical changes, is determinism preserved (same SEED → same
  output)? Cross-repo numerical drift is the most expensive class of
  regression; see the HPO `reference_fingerprint.json` mechanism for
  how we catch it.

## Code of Conduct

Participation in the Tadah! project is governed by the
[Code of Conduct](CODE_OF_CONDUCT.md). Reports of unacceptable
behaviour are handled through a confidential GitLab issue (see
[SECURITY.md](SECURITY.md) for the channel and process).
