# ML‑TADAH CMake integration for LAMMPS (via `CMAKE_PROJECT_INCLUDE`)

This directory contains the **CMake “injection” hook** that enables **ML‑TADAH** in **LAMMPS** as a **pair style**:

- `pair_style tadah`

The integration compiles the ML‑TADAH sources **directly from the `Tadah.LAMMPS` directory of a clone of the Tadah! repository into the LAMMPS build**, without copying files into `lammps/src` and without editing the LAMMPS source tree.

> **Note:** ML‑TADAH / Tadah.LAMMPS is **not part of the official LAMMPS distribution**. It is built from a clone of the Tadah! repository and enabled in the LAMMPS CMake configure step.

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

### 1) Clone Tadah! (somewhere outside LAMMPS)

Tadah.LAMMPS is a directory of the Tadah! repository and is built from inside it, with the modules it needs (Tadah.MD, Tadah.MODELS and Tadah.CORE) beside it. The repository is cloned anywhere, at `v1-dev` (the development line) or at a release tag such as `v1.6.0`:

```bash
git clone -b v1-dev https://github.com/tadah-project/tadah.mlip.git /path/to/tadah
# or: git clone -b v1.6.0 ...
```

A plain `git clone` checks out the repository's default branch, which need not be `v1-dev`. In the steps below `/path/to/Tadah.LAMMPS` stands for `/path/to/tadah/Tadah.LAMMPS`.

### 2) Configure LAMMPS and add the ML‑TADAH integration options

From the **LAMMPS root directory**, run your usual LAMMPS configure command **and add** these options to it:

```bash
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
```

Minimal example, for a `v1-dev` checkout:

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
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

### 4) `TADAH_LAMMPS_QUIET` (ON/OFF)

Controls the verbosity of Tadah.LAMMPS when it is configured/built as part of the LAMMPS build.

Default (in the integration scripts): `ON`

```bash
-DTADAH_LAMMPS_QUIET=ON
```

For debugging:

```bash
-DTADAH_LAMMPS_QUIET=OFF
```

### 5) `TADAH_LAMMPS_PAIR_STYLES` (list)

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
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
```

### Verbose Tadah.LAMMPS output (debugging)

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_LAMMPS_QUIET=OFF
```

### Custom style list

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_LAMMPS_PAIR_STYLES="tadah:PairTadah:pair_tadah.h"
```
