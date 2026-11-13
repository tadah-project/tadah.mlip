# Contributing to Tadah!

## Questions and defects

Questions, defect reports and feature requests go to the
[issue tracker](https://github.com/tadah-project/tadah.mlip/issues). A report
with the output of `tadah --version`, the input files and the failing command
can be reproduced.

## Changes

A change is proposed as a pull request against `main`. A pull
request is small and has one topic. It is accepted when:

- a test fails without the change and passes with it;
- the whole test suite passes;
- `CHANGELOG.md` has a line for it, if a user would notice the change.

## Building and testing

```bash
cmake -S . -B build -DTADAH_DEVELOP=ON -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`TADAH_DEVELOP=ON` adds compiler warnings. The
[installation page](https://tadah.readthedocs.io/latest/installation.html)
gives the requirements, every build option and the builds with Nested Fitting
and LAMMPS. [cmake-tools/README.md](cmake-tools/README.md) says how tests are
registered and discovered.

## Building the documentation

The pages are in `docs/source`, and `docs/requirements.txt` lists the Python
packages of the build, which needs Python 3.11 or later and Doxygen.

```bash
cmake --build build --target docs    # Doxygen XML of the API pages
sphinx-build -W --keep-going -b html docs/source build/html
```
