# Contributing to MODELS

The full multi-repo contributor guide for the Tadah! project lives on
the umbrella repository:
<https://github.com/tadah-project/tadah.mlip/blob/v1-dev/CONTRIBUTING.md>

This file covers what is specific to MODELS. Bug reports and feature
requests on this module use the issue tracker of the umbrella project,
<https://github.com/tadah-project/tadah.mlip/issues>; cross-cutting
changes (work that touches more than one Tadah! module) should be
discussed on the umbrella project first.

## Build & test in isolation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build/tests --output-on-failure
```

In-source builds are blocked deliberately; always pick a separate
build directory.

### Dependencies

- C++17 compiler.
- [CORE](https://github.com/tadah-project/core) — fetched
  automatically.
- OpenMP (optional; enabled with `-DTADAH_ENABLE_OPENMP=ON`).
- LAPACK / BLAS at link time (provided by the build environment).

## Layout

| Path | Purpose |
|---|---|
| `include/tadah/models/` | Public headers; one per public class. |
| `src/`                  | Implementation translation units. |
| `tests/`                | Unit tests for every descriptor, kernel, and basis function. |

## Adding a new descriptor

The general recipe is

1. Implement a class derived from `D2_Base` or `DM_Base` in
   `include/tadah/models/` and `src/`.
2. Register the new class in `d2_all.h` / `dm_all.h` so the factory
   can dispatch on the configuration name.
3. Add a test under `tests/`.
4. Document the new class in
   [Tadah.MLIP/docs/source/descriptors.rst](https://github.com/tadah-project/tadah.mlip/blob/v1-dev/docs/source/descriptors.rst).
