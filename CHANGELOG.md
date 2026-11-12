# Changelog

## [2.0.0] — unreleased

What a user of Tadah! 1.5.0 has to know to move to 2.0.0, the most important first. A message that refuses a line names the line, and its conversion where there is one.

### Breaking

- **Fit weights.** The error scales `ESIGMA` (meV/atom), `FSIGMA` (eV/Å) and `SSIGMA` (GPa) and the priorities of the data replace `EWEIGHT`, `FWEIGHT`, `SWEIGHT`, `WDBFILE_AUTO` and `EWEIGHT_TEMP`. A fit refuses the retired keys and states their conversion; the documentation page *What a Fit Minimises* gives the rule.
- **Data migration.** A dataset converted by 1.5.0 or earlier from VASP output or from a CASTEP `.md` file needs a fresh conversion with `tadah data convert`. The stored stress is positive under tension (VASP data had the sign of the pressure) and, for CASTEP `.md` data, without the kinetic part; the stored VASP energy is the free energy. A structure whose cell a version before 1.3.0-beta.1 stored transposed needs one too; `tadah analysis dataset_stats` lists the structures in which that can occur.
- **Potential files.** 1.5.0 does not read a potential written by 2.0.0, and LAMMPS needs a rebuild with the 2.0.0 package. 2.0.0 reads a potential written by 1.5.0; the next two items are the main exceptions.
- **DM_REAM potentials** declare a density form: the lines `RHOFORM SIGNED` and `EMBF0 FREE`, added to a potential of 1.5.0, keep its energies.
- **Refused potentials.** A potential with `BIAS true` and two or more species, and a linear one with `NORM true` and `BIAS false`, need a retrain.
- **Retraining from a potential** needs `DBFILE` lines of its own: a potential lists its training data and run settings in a comment block, not as keys.
- **One command per run.** Task files (`--task`, the key `TASK`) are removed, and a command line with two commands is refused.
- **Removed.** Uncertainty output (`tadah predict --error`, `tadah train --uncertainty`), whose errors were not standard deviations; the rescaling options of `tadah data balance`; the `BASINS` strategies of `tadah hpo` and their checkpoint resume.
- **Stricter input.** A configuration file and a target file refuse what 1.5.0 ignored: a key of another command, a target-file key given twice, a value that no descriptor reads, values joined by a comma.
- **`tadah hpo`, searches.** `NLOPT:LN_NEWUOA`, `NLOPT:LN_NEWUOA_BOUND`, `CERES:LBFGS`, `CERES:WOLFE` and `CERES:NLCG` are refused. No point outside the `OPTIM` box is evaluated, and `MAXEVAL` counts every evaluation of an NLopt gradient search. `PC_SRMSE_GPA`, in GPa, replaces `PC_SRMSE`.
- **`tadah hpo`, exit status.** It is 1 when no evaluation gave a usable loss, and 128 + the signal number after Ctrl-C or `SIGTERM`; both were 0. The journal says why the search ended, and `COMPLETED` is not written.
- **`tadah refit`** writes the many-body term as a `DM_Embed` component. A build without Nested Fitting or without Ceres needs `REFIT_MODE dense`.
- **Building from source.** One repository holds every module: a build fetches no Tadah! module, and the LAMMPS package is built from the directory `lammps` of the repository. A LAMMPS configure step names it with `-DTADAH_LAMMPS_SOURCE_DIR=<clone>/lammps` and `-DCMAKE_PROJECT_INCLUDE=<clone>/lammps/integration/TadahLAMMPSProjectInclude.cmake`. The options `TADAH_GIT_BRANCH` and `TADAH_BUILD_MPI` are removed. The C++ standard library is libstdc++ 11 or libc++ 14, or later.

### Added

- `DM_Embed`, a many-body descriptor of the EAM form, composed in a `DESCRIPTOR` block; `tadah hpo` searches its parameters by name.
- `tadah hpo --check` makes the startup checks of a run without running it.
- `tadah explain ALGO` lists the optimisers of `tadah hpo`.
- `tadah train` prints a balance table after its fit: per quantity and dataset, the RMSE and the share of the minimised sum.
- `tadah hpo` searches the error scales and the dataset priorities (`OPTIM ESIGMA`, `OPTIM WDBFILE`), takes a target per validation dataset (`PC_ERMSE (i)`) and rejects a DM_REAM candidate by its density (`RHOGATE`, `RHOBARGATE`).
- Documentation: a rewritten Nested Fitting chapter, with a tutorial, examples and a reference of every key and optimiser.

### Fixed

- Neighbour lists hold every periodic image within the cutoff, also for a cell thinner than the cutoff and a triclinic cell. Results change for such structures, and a potential fitted to them needs a retrain.
- A `LAMBDA -1` fit counts its observations only, so its λ and its coefficients change.
- Non-linear models (`M_KRR` with a non-linear kernel, `M_BLR` with `BF_Polynomial2`) train and predict again, and `DIMER` models are trained with the real interatomic distances.
- `tadah predict` applies the `LSCALE` of the potential.
- The cell of a non-orthogonal CIF or COD structure is read as it is; it was transposed.
- `pair_style tadah`: a second `pair_coeff` loads its file.
- Potential and dataset files keep every number at full precision.
- `tadah` exits with a non-zero status when a command fails.

The notes of 1.5.0 and earlier releases are in the `CHANGELOG.md` of the `v1.5.0` tag.
