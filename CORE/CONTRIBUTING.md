# Contributing to CORE

The full multi-repo contributor guide for the Tadah! project lives on
the umbrella repository:
<https://github.com/tadah-project/tadah.mlip/blob/v1-dev/CONTRIBUTING.md>

This file covers what is specific to CORE. Bug reports and feature
requests on this module use the issue tracker of the umbrella project,
<https://github.com/tadah-project/tadah.mlip/issues>; cross-cutting
changes (work that touches more than one Tadah! module) should be
discussed on the umbrella project first.

## Build & test in isolation

CORE depends only on the
[cmake-tools](https://github.com/tadah-project/cmake-tools) helper repo
(fetched automatically) and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build/tests --output-on-failure
```

In-source builds are blocked deliberately; always pick a separate
build directory.

## Layout

| Path | Purpose |
|---|---|
| `include/tadah/core/` | Public headers (`context.h`, `checkpoint.h`, `logger.h`, …). |
| `src/`                | Implementation translation units. |
| `blueprint/`          | `blueprint_tadah.toml` — the single source of truth for config keys, CLI flags, and version. The umbrella docs render it via `read_toml.py`. |
| `tests/`              | Unit tests. |
| `external/`           | Third-party headers vendored at fixed versions. |

## Versioning

The Tadah! version is read from `blueprint/blueprint_tadah.toml`
(`[TADAH] version = "..."`). All Tadah! consumers — including the
documentation build — pull their version from this file, so a single
edit propagates project-wide.
