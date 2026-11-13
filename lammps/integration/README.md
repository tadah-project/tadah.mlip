# ML‑TADAH CMake integration for LAMMPS (via `CMAKE_PROJECT_INCLUDE`)

This directory contains the **CMake “injection” hook** that enables **ML‑TADAH** in **LAMMPS** as a **pair style**:

- `pair_style tadah`

The integration compiles the ML‑TADAH sources **directly from the `lammps` directory of a clone of the Tadah! repository into the LAMMPS build**, without copying files into `lammps/src` and without editing the LAMMPS source tree.

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

### 1) Cloning Tadah! (somewhere outside LAMMPS)

Tadah.LAMMPS is the directory `lammps` of the Tadah! repository and is built from inside it, with the modules it needs (Tadah.MD, Tadah.MODELS and Tadah.CORE) beside it. The repository is cloned anywhere, at the default branch or at a release tag such as `v2.0.0`:

```bash
git clone https://github.com/tadah-project/tadah.mlip.git /path/to/tadah
# or: git clone -b v2.0.0 ...
```

In the steps below that directory is `/path/to/tadah/lammps`.

### 2) Configuring LAMMPS with the ML‑TADAH integration options

The usual LAMMPS configure command runs from the **LAMMPS root directory**, with these options **added** to it:

```bash
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
```

Minimal example:

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
```

### 3) Building

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

- The ML‑TADAH sources are outside the LAMMPS source tree, so LAMMPS does not find their styles by itself. The integration calls the LAMMPS function `RegisterStyles()` on the `ML-TADAH` directory, just before LAMMPS generates its style files.
- `pair_tadah.h` declares the style in the standard LAMMPS way:
  - `PairStyle(tadah,PairTadah)` (inside `#ifdef PAIR_CLASS ... #else ... #endif`)
- The configure step stops with an error when the generated style files do not name `pair_tadah.h`.
- The ML‑TADAH sources (`ML-TADAH/*.cpp`) from the Tadah.LAMMPS checkout are compiled directly into the LAMMPS target.

---

## CMake options supported by the integration

All options below are passed to the **LAMMPS CMake configure** command (the one that runs from the LAMMPS root directory, such as `cmake -S cmake -B build ...`).

### 1) `PKG_ML-TADAH` (ON/OFF)

Enables the ML‑TADAH package during the LAMMPS configure.

```bash
-DPKG_ML-TADAH=ON
```

### 2) `CMAKE_PROJECT_INCLUDE` (path) — required

CMake hook used by LAMMPS to inject this integration into the configure run.

```bash
-DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake
```

### 3) `TADAH_LAMMPS_SOURCE_DIR` (path) — required

Path to the directory `lammps` of a clone of the Tadah! repository.

```bash
-DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
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

Ignored. The styles are discovered from the `PairStyle(...)` markers in the headers of `ML-TADAH`. A value other than the default `tadah:PairTadah` gives a warning at configure time.

---

## Full example command lines

### Minimal recommended

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
```

### Verbose Tadah.LAMMPS output (debugging)

```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps \
  -DTADAH_LAMMPS_QUIET=OFF
```
