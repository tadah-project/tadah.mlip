# Changelog

## [2.0.0] — unreleased

What changes on moving from any 1.x.x release, the most important first. A message that refuses a line names the line, and its conversion where there is one.

### Breaking

- **Fit weights.** The error scales `ESIGMA` (meV/atom), `FSIGMA` (eV/Å) and `SSIGMA` (GPa) and the priorities of the data replace `EWEIGHT`, `FWEIGHT`, `SWEIGHT`, `WDBFILE_AUTO` and `EWEIGHT_TEMP`. A fit refuses the retired keys and states their conversion; the documentation page *What a Fit Minimises* gives the rule.
- **Data migration.** A dataset converted by an earlier release from VASP output or from a CASTEP `.md` file needs a fresh conversion with `tadah data convert`: the stored stress (its sign and, for CASTEP `.md` data, the kinetic part) and the VASP energy changed. A dataset with a cell stored transposed needs one too; `tadah analysis dataset_stats` lists the structures in which that can occur.
- **Potential files.** An earlier release does not read a potential written by this one, and LAMMPS needs a rebuild with this package. A potential of an earlier release is read, with the exceptions of the next item.
- **Refused potentials.** A potential with `BIAS true` and two or more species, and a linear one with `NORM true` and `BIAS false`, need a retrain.
- **Retraining from a potential** needs `DBFILE` lines of its own: a potential lists its training data and run settings in a comment block, not as keys.
- **One command per run.** Task files (`--task`, the key `TASK`) are removed, and a command line with two commands is refused.
- **Removed.** Uncertainty output (`tadah predict --error`, `tadah train --uncertainty`), whose errors were not standard deviations; the rescaling options of `tadah data balance`; the `BASINS` strategies of `tadah hpo` and their checkpoint resume.
- **Stricter input.** A configuration file and a target file refuse what an earlier release ignored: a key of another command, a target-file key given twice, a value that no descriptor reads, values joined by a comma.
- **`tadah hpo`, searches.** `NLOPT:LN_NEWUOA`, `NLOPT:LN_NEWUOA_BOUND`, `CERES:LBFGS`, `CERES:WOLFE` and `CERES:NLCG` are refused. No point outside the `OPTIM` box is evaluated, and `MAXEVAL` counts every evaluation of an NLopt gradient search. `PC_SRMSE_GPA`, in GPa, replaces `PC_SRMSE`.
- **`tadah hpo`, exit status.** It is 1 when no evaluation gave a usable loss, and 128 + the signal number after Ctrl-C or `SIGTERM`; both were 0. The journal says why the search ended, and `COMPLETED` is not written.
- **Building from source.** One repository holds every module: a build fetches no Tadah! module, and the LAMMPS package is built from the directory `lammps` of the repository. The options `TADAH_GIT_BRANCH` and `TADAH_BUILD_MPI` are removed. The C++ standard library is libstdc++ 11 or libc++ 14, or later.

### Added

- `DM_Embed`, a many-body descriptor of the EAM form, composed in a `DESCRIPTOR` block; `tadah hpo` searches its parameters by name.
- `tadah hpo --check` makes the startup checks of a run without running it.
- `tadah explain ALGO` lists the optimisers of `tadah hpo`.
- `tadah train` prints a balance table after its fit: per quantity and dataset, the RMSE and the share of the minimised sum.
- `tadah hpo` searches the error scales and the dataset priorities (`OPTIM ESIGMA`, `OPTIM WDBFILE`) and takes a target per validation dataset (`PC_ERMSE (i)`).
- `tadah refit` converts a tabulated EAM potential (a setfl file) into a native potential and a target file for `tadah hpo`; the many-body term is a `DM_Embed` component. A build without Nested Fitting or without Ceres needs `REFIT_MODE dense`.
- Documentation: a rewritten Nested Fitting chapter, with a tutorial, examples and a reference of every key and optimiser.

### Fixed

- Neighbour lists hold every periodic image within the cutoff, also for a cell thinner than the cutoff and a triclinic cell. Results change for such structures, and a potential fitted to them needs a retrain.
- A `LAMBDA -1` fit counts its observations only, so its λ and its coefficients change.
- Non-linear models (`M_KRR` with a non-linear kernel, `M_BLR` with `BF_Polynomial2`) train and predict again, and `DIMER` models are trained with the real interatomic distances.
- A search of `tadah hpo` or `tadah refit` with `NLOPT:LN_SBPLX`, the `NLOPT:GN_DIRECT` family, `NLOPT:GN_ISRES` or `NLOPT:GN_ESCH` gives the same result with every C library; it followed the sort of the C library. Results of such searches change.
- `tadah predict` applies the `LSCALE` of the potential.
- The cell of a non-orthogonal CIF or COD structure is read as it is; it was transposed.
- `pair_style tadah`: a second `pair_coeff` loads its file.
- Potential and dataset files keep every number at full precision.
- `tadah` exits with a non-zero status when a command fails.
- A `SUMMARY` line of the `tadah hpo` journal counts the evaluations made, also after a stop during a batch of an annealing search; it gave the number of the last evaluation.
- The VASP and CASTEP converters of `tadah data convert` read the stress and the energy correctly, and name the file and the place of a value they cannot read.
