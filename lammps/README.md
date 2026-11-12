[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-2.0.0-blue.svg)](https://github.com/tadah-project/tadah.lammps/tags)

# ML‑TADAH for LAMMPS (Tadah.LAMMPS)

This directory of the Tadah! repository adds **ML‑TADAH** to **LAMMPS** as a **pair style** (`pair_style tadah`) for running interatomic potentials developed with **Tadah!MLIP**.

> **Note:** ML‑TADAH / Tadah.LAMMPS is **not part of the official LAMMPS distribution**. It is built from a clone of the Tadah! repository and enabled in the LAMMPS build.

Documentation (recommended starting point, incl. extended instructions):
https://tadah.readthedocs.io/v1-dev/

---

## Installation overview

The **Tadah! repository is cloned** first, since ML‑TADAH is not shipped with LAMMPS; LAMMPS is then built with CMake with the ML‑TADAH package enabled.

The LAMMPS documentation describes the general build options and prerequisites:
https://docs.lammps.org/Build_cmake.html

---

## Building with CMake

### Step 1 — Cloning Tadah!

Tadah.LAMMPS is the directory `lammps` of the Tadah! repository and is built from inside it, with the modules it needs (Tadah.MD, Tadah.MODELS and Tadah.CORE) beside it. The repository is cloned anywhere, at `v1-dev` (the development line) or at a release tag such as `v2.0.0`:

```bash
git clone -b v1-dev https://github.com/tadah-project/tadah.mlip.git /path/to/tadah
# or: git clone -b v2.0.0 ...
```

A plain `git clone` checks out the repository's default branch, which need not be `v1-dev`. In the steps below that directory is `/path/to/tadah/lammps`.

---

### Step 2 — Configuring LAMMPS with ML‑TADAH enabled

The usual LAMMPS CMake configure command runs from the **LAMMPS root directory**, with the ML‑TADAH integration options below **added to that command**:

```bash
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
```

Those three options are the "integration hook": they enable the `ML-TADAH` package and tell LAMMPS where to find the Tadah.LAMMPS sources.

#### Example A: minimal LAMMPS configure + ML‑TADAH
From the LAMMPS root directory (for a `v1-dev` checkout):
```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps
```

#### Example B: the LAMMPS GCC preset, a serial binary and ML‑TADAH

This example uses the LAMMPS preset file `lammps/cmake/presets/gcc.cmake`, enables ML‑TADAH, and sets the LAMMPS machine target to `serial`:

```bash
cmake -C cmake/presets/gcc.cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/tadah/lammps/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/tadah/lammps \
  -DLAMMPS_MACHINE=serial
```

---

### Step 3 — Building

```bash
cmake --build build
```

Parallel builds are supported; on Linux a common choice is:

```bash
cmake --build build --parallel $(nproc)
```

Result: the resulting LAMMPS binary includes **ML‑TADAH** and provides `pair_style tadah`.

---

## What this directory holds

- **LAMMPS pair style sources**: **ML‑TADAH** (`pair_style tadah`)
- Supporting **C++ library** used by the integration: `tadah.lammps`
- CMake integration hook for LAMMPS:
  `integration/TadahLAMMPSProjectInclude.cmake`

The documentation covers usage, examples and troubleshooting:
https://tadah.readthedocs.io/v1-dev/
