# hpo

Nested Fitting, the command `tadah hpo`: a search over the parameters of a
potential in which every candidate is trained and scored against data and,
optionally, against properties computed with LAMMPS. The
[Nested Fitting chapter](https://tadah.readthedocs.io/v1-dev/nfp/overview.html) of the documentation describes
its use.

`hpo` is a module of the [Tadah! repository](../README.md), built when the
configure step is given `-DTADAH_ENABLE_NFP=ON`. The
[installation guide](https://tadah.readthedocs.io/v1-dev/installation.html) describes the build, and
[blueprint/README.md](blueprint/README.md) the registry of target-file keys.
