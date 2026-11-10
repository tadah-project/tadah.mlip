# ML‑TADAH CMake integration for LAMMPS (via `CMAKE_PROJECT_INCLUDE`)

This directory contains the **CMake “injection” hook** that enables **ML‑TADAH** in **LAMMPS** as a **pair style**:

- `pair_style tadah`

The integration compiles the ML‑TADAH sources **directly from your Tadah.LAMMPS checkout into the LAMMPS build**, without copying files into `lammps/src` and without editing the LAMMPS source tree.

> **Note:** ML‑TADAH / Tadah.LAMMPS is **not part of the official LAMMPS distribution**. You must clone Tadah.LAMMPS yourself and enable it in your LAMMPS CMake configure step.

Extended documentation:
https://tadah.readthedocs.io/

LAMMPS CMake build documentation:
https://docs.lammps.org/Build_cmake.html

---

## Requirements

- **CMake ≥ 3.20**
- A working **C++17** toolchain suitable for building LAMMPS

---

## Quick start (recommended workflow)

### 1) Clone Tadah.LAMMPS (somewhere outside LAMMPS)

Name the branch or tag, `v1-dev` (the development line) or a release tag such as `v1.6.0`; a plain `git clone` checks out the repository's default branch:

```bash
git clone -b v1-dev https://github.com/tadah-project/tadah.lammps.git /path/to/Tadah.LAMMPS
```

### 2) Configure LAMMPS and add the ML‑TADAH integration options

From the **LAMMPS root directory**, run your usual LAMMPS configure command **and add** these options to it:

```bash
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
```

and the git ref the Tadah modules are fetched at, matching your Tadah.LAMMPS checkout (see `TADAH_GIT_BRANCH` below):

```bash
  -DTADAH_GIT_BRANCH=v1-dev
```

Minimal example, for a `v1-dev` checkout:

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev
```

### 3) Build

```bash
cmake --build build
```

Parallel build is supported (commonly on Linux):

```bash
cmake --build build --parallel $(nproc)
```

Result: the LAMMPS binary built from this build directory includes **ML‑TADAH** and provides `pair_style tadah`.

---

## How it works (high level)

- LAMMPS generates `build/styles/style_pair.h` as an include-only file that lists pair style headers (e.g. `#include "pair_*.h"`).
- This integration appends one or more include lines (e.g. `#include "pair_tadah.h"`) to that generated file.
- `pair_tadah.h` registers the style via the standard LAMMPS mechanism:
  - `PairStyle(tadah,PairTadah)` (inside `#ifdef PAIR_CLASS ... #else ... #endif`)
- The ML‑TADAH sources (`ML-TADAH/*.cpp`) from the Tadah.LAMMPS checkout are compiled directly into the LAMMPS target.

---

## CMake options supported by the integration

All options below are passed to your **LAMMPS CMake configure** command (i.e. the command you run from the LAMMPS root directory, such as `cmake -S cmake -B build ...`).

### 1) `PKG_ML-TADAH` (ON/OFF)

Enables the ML‑TADAH package during the LAMMPS configure.

```bash
-DPKG_ML-TADAH=ON
```

### 2) `CMAKE_PROJECT_INCLUDE` (path) — required

CMake hook used by LAMMPS to inject this integration into the configure run.

```bash
-DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake
```

### 3) `TADAH_LAMMPS_SOURCE_DIR` (path) — required

Path to your local Tadah.LAMMPS checkout.

```bash
-DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
```

### 4) `TADAH_GIT_BRANCH` (a branch or tag of the Tadah modules)

The git ref Tadah.LAMMPS fetches the Tadah modules at (Tadah.MD, and through it Tadah.MODELS and Tadah.CORE). The Tadah.LAMMPS sources themselves are your checkout at `TADAH_LAMMPS_SOURCE_DIR`, whatever it has checked out; pass the ref that matches it:

```bash
-DTADAH_GIT_BRANCH=v1-dev     # the development line
# or
-DTADAH_GIT_BRANCH=v1.6.0     # a release tag
```

Without it, cmake-tools' `tadah_get_git_branch` detects the ref: `$CI_COMMIT_REF_NAME` in a CI pipeline that sets it, the Read the Docs variables on Read the Docs, else the upstream branch, then the local branch, of the checkout of the project CMake was started on. In this build that is the **LAMMPS** checkout, whose branch (e.g. `stable` or `develop`) is unrelated to your Tadah.LAMMPS checkout, and a LAMMPS tarball or a detached HEAD gives no ref at all (with a warning). Always pass `-DTADAH_GIT_BRANCH` in the LAMMPS build.

### 5) `TADAH_LAMMPS_QUIET` (ON/OFF)

Controls the verbosity of Tadah.LAMMPS when it is configured/built as part of the LAMMPS build.

Default (in the integration scripts): `ON`

```bash
-DTADAH_LAMMPS_QUIET=ON
```

For debugging:

```bash
-DTADAH_LAMMPS_QUIET=OFF
```

### 6) `TADAH_LAMMPS_PAIR_STYLES` (list)

Semicolon-separated list of pair styles to register.

Format:
- `style:Class[:header]`

Behavior:
- the integration uses this list to decide which header(s) to include into `style_pair.h`
- `Class` is currently retained for future expansion
- if `header` is omitted it defaults to `pair_\<style\>.h`

Default:
- `tadah:PairTadah`

Examples:

Default:

```bash
-DTADAH_LAMMPS_PAIR_STYLES="tadah:PairTadah"
```

Explicit header name:

```bash
-DTADAH_LAMMPS_PAIR_STYLES="tadah:PairTadah:pair_tadah.h"
```

Multiple styles (future use):

```bash
-DTADAH_LAMMPS_PAIR_STYLES="tadah:PairTadah;other:PairOther:pair_other.h"
```

---

## Full example command lines

### Minimal recommended

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev
```

### Fetch the Tadah modules at a release tag

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1.6.0
```

### Verbose Tadah.LAMMPS output (debugging)

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev \
  -DTADAH_LAMMPS_QUIET=OFF
```

### Custom style list

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev \
  -DTADAH_LAMMPS_PAIR_STYLES="tadah:PairTadah:pair_tadah.h"
```
