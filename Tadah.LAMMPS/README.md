[![license](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![version](https://img.shields.io/badge/version-1.6.0-blue.svg)](https://github.com/tadah-project/tadah.lammps/tags)

# ML‑TADAH for LAMMPS (Tadah.LAMMPS)

This repository adds **ML‑TADAH** to **LAMMPS** as a **pair style** (`pair_style tadah`) for running interatomic potentials developed with **Tadah!MLIP**.

> **Note:** ML‑TADAH / Tadah.LAMMPS is **not part of the official LAMMPS distribution**. You must clone this repository yourself and enable it in your LAMMPS build.

Documentation (recommended starting point, incl. extended instructions):
https://tadah.readthedocs.io/v1-dev/

---

## Installation overview

You first **clone Tadah.LAMMPS** (this repository), since it is not shipped with LAMMPS, and then build LAMMPS with CMake enabling the ML‑TADAH package.

For general LAMMPS build options and prerequisites, see:
https://docs.lammps.org/Build_cmake.html

---

## Build with CMake

### Step 1 — Clone Tadah.LAMMPS

Clone this repository anywhere you like, at `v1-dev` (the development line) or at a release tag such as `v1.6.0`:

```bash
git clone -b v1-dev https://github.com/tadah-project/tadah.lammps.git /path/to/Tadah.LAMMPS
# or: git clone -b v1.6.0 ...
```

Name the branch or tag: a plain `git clone` checks out the repository's default branch, which need not be `v1-dev`. The branch or tag checked out in `/path/to/Tadah.LAMMPS` is the Tadah.LAMMPS source LAMMPS compiles.

Tadah.LAMMPS fetches the Tadah modules it needs (Tadah.MD, and through it Tadah.MODELS and Tadah.CORE) at the git ref `TADAH_GIT_BRANCH`. Pass the ref that matches your checkout when you configure LAMMPS:

- `-DTADAH_GIT_BRANCH=v1-dev` (the development line)
- `-DTADAH_GIT_BRANCH=v1.6.0` (a release tag)

Without it the ref is detected from the checkout of the project CMake was started on, which in this build is **LAMMPS**: its upstream branch, else its local branch: a LAMMPS branch name such as `stable` or `develop`, unrelated to your Tadah.LAMMPS checkout. A LAMMPS tarball or a detached HEAD gives no ref at all, with a warning. So always pass `-DTADAH_GIT_BRANCH` here; the [installation guide](https://tadah.readthedocs.io/v1-dev/installation.html) describes the detection in full.

---

### Step 2 — Configure LAMMPS and enable ML‑TADAH

From the **LAMMPS root directory**, run your usual LAMMPS CMake configure command **and add** the ML‑TADAH integration options below **to that command**:

```bash
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS
```

Those three options are the "integration hook": they enable the `ML-TADAH` package and tell LAMMPS where to find the Tadah.LAMMPS sources.

#### Example A: minimal LAMMPS configure + ML‑TADAH
Run from the LAMMPS root directory (for a `v1-dev` checkout):
```bash
cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev
```

#### Example B: use the LAMMPS GCC preset + build a serial binary + ML‑TADAH

This example uses the LAMMPS preset file `lammps/cmake/presets/gcc.cmake`, enables ML‑TADAH, fetches the Tadah modules at `v1-dev`, and sets the LAMMPS machine target to `serial`:

```bash
cmake -C cmake/presets/gcc.cmake -S cmake -B build \
  -DPKG_ML-TADAH=ON \
  -DCMAKE_PROJECT_INCLUDE=/path/to/Tadah.LAMMPS/integration/TadahLAMMPSProjectInclude.cmake \
  -DTADAH_LAMMPS_SOURCE_DIR=/path/to/Tadah.LAMMPS \
  -DTADAH_GIT_BRANCH=v1-dev \
  -DLAMMPS_MACHINE=serial
```

---

### Step 3 — Build

```bash
cmake --build build
```

Parallel builds are supported; on Linux a common choice is:

```bash
cmake --build build --parallel $(nproc)
```

Result: the resulting LAMMPS binary includes **ML‑TADAH** and provides `pair_style tadah`.

---

## What’s in this repository?

- **LAMMPS pair style sources**: **ML‑TADAH** (`pair_style tadah`)
- Supporting **C++ library** used by the integration: `tadah.lammps`
- CMake integration hook for LAMMPS:
  `integration/TadahLAMMPSProjectInclude.cmake`

For usage, examples, and troubleshooting, see:
https://tadah.readthedocs.io/v1-dev/
