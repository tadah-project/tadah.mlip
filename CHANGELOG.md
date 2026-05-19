# Changelog

This is the **umbrella changelog** for the Tadah! framework — the curated, cross-module view that users browsing the umbrella repository read first. Each entry is prefixed with the originating module:

- `[core]` — Tadah.CORE (blueprint / config / registry)
- `[mlip]` — Tadah.MLIP module (training / predict / data-balance orchestration)
- `[models]` — Tadah.MODELS (descriptors / cutoffs / basis functions / regressors)
- `[md]` — Tadah.MD (lightweight MD drivers)
- `[hpo]` — Tadah.HPO (hyper-parameter optimisation)
- `[lammps]` — Tadah.LAMMPS (`pair_tadah` LAMMPS plugin)
- `[umbrella]` — this repository (`tadah` binary, docs site, examples)

For the full per-module commit-level log, see each module's own `CHANGELOG.md`.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [SemVer](https://semver.org/) at the umbrella level.

## [Unreleased]

Nothing yet.

## [1.3.0-beta.1] — 2026-05-20

The first public beta of the Tadah! framework. Highlights: a new family of rotationally invariant power-spectrum descriptors (`DM_PS_*` / `DM_OW_*` / `DM_GPS_*`), a wholesale HPO restructure (init strategies, resume, varloss, `TADAH:ANNEAL` simulated annealing, `LIB CERES` gradient family, hot-variable two-phase eval), in-place HPO via `Trainer::reinit()`, load-time dataset transforms, an HPO regression-example suite, single-source-of-truth versioning (blueprint TOML → CMake → docs), and a corrected `Cut_PT` normalisation.

### Added — User-visible features
- `[models]` New descriptor family: `DM_Poly<ChannelFn, AggMode, RadialBasis>` — rotationally invariant power-spectrum descriptors. Registered variants:
  - Power-spectrum / blip radial: `DM_PS_SQ`, `DM_PS_SQRT`, `DM_PS_RLR`, `DM_PS_Blip`.
  - Power-spectrum / Gaussian radial: `DM_GPS_SQ`, `DM_GPS_SQRT`, `DM_GPS_RLR`, `DM_GPS_Blip`.
  - Orbital-wise aggregation: `DM_OW_*`, `DM_GOW_*`.
- `[models]` New `F_Blip` embedding function and `DM_mBlip` / `DM_mBlipEnv` descriptors. `DM_mBlipEnv` wraps the embedding in a `Cut_SinInv` envelope controlled by the new `RCUTENV` blueprint key.
- `[hpo]` Init strategies: `CONFIG`, `RANDOM`, `LHS`, `SOBOL`, `WARM`.
- `[hpo]` Resume / restart of long sweeps from the structured `HPOLog` journal.
- `[hpo]` Variance-of-loss acquisition (`--varloss` / `--outvar` over LAMMPS `equal`-style script variables, validated up-front).
- `[hpo]` Standalone dlib BFGS local optimiser (alongside the existing nlopt path).
- `[hpo]` Per-dataset validation metrics and stats output files.
- `[hpo]` Reference example suite under `HPO/examples/` with checked-in fingerprints and a `run_reference_suite.sh` driver.
- `[mlip][core]` Load-time dataset transforms: `LSCALE`, `ESHIFT` (per-species, plus auto-derive variants `ESHIFT_ATOM` / `ESHIFT_DBATOM`), `EWEIGHT_TEMP`, `EFILTER` / `FFILTER`, `WDBFILE` / `WDBFILE_AUTO`, `ZERO_COM_FORCE`. Applied uniformly across `tadah train`, `tadah predict`, `tadah hpo`, and `tadah data balance`; persisted into `pot.tadah` for round-trip on prediction.
- `[mlip][hpo][models]` `Trainer::reinit()` + `descriptor::reinit()` + `DC_Selector::reinit()` — in-place rebuild for HPO; per-thread Trainer/TrainerHost is now cached and reused across evaluations when structural parameters do not change.
- `[lammps]` `pair_tadah` now prints the Tadah! VERSION banner from `PairTadah::coeff()` so LAMMPS logs record the producing version.
- `[umbrella]` New worked example `ex_3_DM_mBlipEnv`.
- `[umbrella]` HPO examples page wired into the Sphinx toctree (link-only — content lives in `HPO/examples/`).
- `[core]` Container-style `begin()` / `end()` iteration on `Config`.
- `[core][mlip][models][md][hpo][lammps][umbrella]` `TADAH_ENABLE_ASAN` CMake option for AddressSanitizer builds.
- `[core][mlip][hpo][umbrella]` **HPO bound-validation pipeline** — catches a class of silent training failures (descriptors that collapse to zero weights when a fixed parameter or HPO bound puts them outside the dataset's support).
  - Blueprint: `AUDIT` (off / warn / error; default off so large training DBs don't pay the data-stats scan unless opted in), `AUDIT_SAMPLE` (NN-sweep cap, default 256 — `0` = full DB), `HEALTH_LOG` (per-iter monitor verbosity).
  - New `tadah` analysis subcommands: `dataset_stats`, `suggest_bounds`, `check_bounds`.
  - Internals (`MLIP/`): design-matrix `ColumnLayout`, `compute_data_stats` + `classify_dataset`, per-`ParamMeta` data-driven `bound_suggester`, `preflight_audit` with `AuditMode::{Off,Warn,Error}`.
  - HPO wires the audit hook + per-iter health monitor; `EFILTER` / `FFILTER` are now also forwarded from `--validation` (symmetric LOAD-TRANSFORMS log; mean-share breakdown).
- `[hpo][umbrella]` **`TADAH:ANNEAL` — classic Kirkpatrick/Metropolis Simulated Annealing driver**, dispatched from `run_inner()` alongside `TADAH:RANDOM` / `TADAH:GRID`. Operates in transformed (log/identity per-`OPTIM`) space; reflect-into-box on out-of-bound proposal components for high-dim robustness. Documented in `hpo.rst` (seed-consumption table + journal-trace section).
- `[hpo][umbrella]` **`TADAH:ANNEAL` Corana adaptive variant** — opt-in via `PARAM CORANA 1` (refs: Corana et al., *ACM Trans. Math. Soft.* 13:262, 1987; Brommer, Univ. Stuttgart MSc, 2003). Per-dim step vector `V[i]` adapts every `NS`-step cycle from the per-dim accept ratio; cools every `NT` cycles. Tunable: `NS` (default 5), `NT` (default 2), `C` (default 2.0). Adds `PARAM V_FLOOR_FRAC` (default `1e-3`) as a lower clamp on `V[i]` to guard against runaway collapse on `FAILSCORE`-cliff landscapes, and `PARAM RESET_TO_BEST 1` to snap the chain back to the best-so-far point at every cooling step.
- `[hpo][umbrella]` **`LIB CERES` gradient-based optimiser family** — `LBFGS` (limited-memory BFGS, Wolfe line search), `WOLFE` (full BFGS, Wolfe line search), `NLCG` (nonlinear conjugate gradient), `STEEPEST_DESCENT` (Armijo line search). All four use Ceres' `GradientProblemSolver`; gradient via `DynamicNumericDiffCostFunction` (central differences from `PARAM fd_step`). The Ceres NLLS family (`LEVENBERG_MARQUARDT`, `DOGLEG`, `SUBSPACE_DOGLEG`) is deferred at dispatch.
- `[hpo]` **`OptimizerCaps` registry** — single source of truth for `(LIB, ALGO)` capabilities (gradient-based / global / population / accepts-inner / multi-seed); routes `ftol_rel` for NLopt vs `ftol_abs` for DLIB:BFGS automatically.
- `[hpo][umbrella]` **`PC_LAMMPS --hotvar <name> <min> <max> [<initial>]`** — two-phase eval. Hot scripts run alone in phase A and expose a validated value via `variable <name> equal …`; phase B re-runs cold scripts with the propagated `${<name>}`. Caching keyed on `(script, name)`. Replaces the legacy per-script `${a0}` post-hoc range check.
- `[hpo][umbrella]` **`PC_<METRIC> --skip-above <cap>` / `--skip-rel <ratio>`** — per-metric bad-pot pre-filter on `PC_EMAE`, `PC_ERMSE`, `PC_ErRMSE`, `PC_ERSQ`, `PC_FRMSE`, `PC_SRMSE`. Fires before LAMMPS; fail-scores when metric exceeds the cap or `metric > ratio × running_best`.

### Changed
- `[models]` **`Cut_PT` normalised so f(0) = 1** — corrects a constant rescaling that previously contaminated descriptors using the polynomial cutoff. Energies / forces / stresses computed with `Cut_PT` against v1.1.x will differ. See **Migration** below.
- `[core]` Blueprint key casing: `atom` → `ATOM` (consistency with the rest of the schema).
- `[core]` `THRESHOLD` lost its top-level default (was `1e-6`); the per-task `[TASKS.data.balance.keys.threshold]` default tightened from `["1e-1", "2e-2", "3e-3"]` to `["1e-4", "1e-5", "1e-6"]` — affects `tadah data balance` results when `THRESHOLD` is not passed explicitly.
- `[core]` `TYPEMB` value-type regex generalised to accept the additional spherical-harmonics-order parameter (existing 2-UINT lines still parse; the new 3-UINT example form is documented).
- `[core]` `WATOM` blueprint type/handling corrected; bundled `DM_*` examples updated.
- `[hpo]` `best_pot` output writer rewritten (v2 layout) and now flushes on short runs.
- `[hpo]` Include-file parsing is case-insensitive.
- `[hpo]` GitLab CI rewritten by hand (replaces the auto-generated Auto-DevOps pipeline).
- `[lammps]` Per-species bias lookup precomputed at `initialize()` (small per-step speed-up).
- `[core][mlip][models][md]` C++ standard upgraded from C++14 to **C++17** — a C++17 compiler is now required.
- `[umbrella]` `ex_3_DM_mBlipEnv` worked example: blip embedding widened so `F(rho)` has no dead zone (example output is now non-trivial across the full density range encountered by typical inputs).

### Fixed — Critical correctness / crash fixes
- `[models]` `Cut_PT` normalisation (see Changed).
- `[mlip]` MPI / BLACS lifetime — `MPI_Type_free` and `blacs_gridexit` moved to destructors; `TrainerHost::run()` resets state and clears `wpckgs` between rebuilds. Fixes the long-standing inability to reuse a TrainerHost across HPO evaluations.
- `[mlip]` Memory leak in the pairwise predict engine (2b and mb model / fb pointers were not deleted).
- `[mlip]` `value_N` index now clamped — out-of-bounds when `default_value` is longer than the configured value count no longer crashes.
- `[mlip]` `bspan` formula and shift in `calc_binned` corrected.
- `[mlip]` R² seg-fault when computing on forces.
- `[mlip]` `--config` parsing and `-F` without `--outfile` no longer error.
- `[models]` `DM_mJoin` corrections + `drfidx`.
- `[models]` Incorrect blip argument in the density formula (LaTeX docs and code).
- `[hpo]` `rcut` optimisation path corrected.
- `[hpo]` MPI rank-handle leaks resolved (paired with the MLIP MPI fix).
- `[hpo]` `LammpsRunner` API alignment (after a brief revert/redo cycle).
- `[hpo]` `best_pot.tadah` headers no longer embed an absolute `DBFILE` path — the producer wrote it via `fs::absolute()`, leaking the author's filesystem layout into shipped potentials; the path is now recorded as written in `config.tadah` (relative).
- `[lammps]` Pair indexing: `i` / `j` swap corrected.
- `[core][models][md]` `Registry` static-initialisation order across translation units — descriptors / cutoffs / models registered in different `.cpp` files no longer race the factory map.

### Performance
- `[models]` `final` qualifier on `F_RLR` / `F_SQRT` / `F_SQ` / `F_Blip` `calc_F` and `calc_dF` for guaranteed devirtualisation.
- `[hpo]` Per-thread `Trainer` / `TrainerHost` cache — reused via `reinit()` when structural params unchanged; large speed-up for sweeps that vary only weights or scalar regression parameters.

### Deprecated
- `[models]` `DM_mEAD` for `L > 0` with `F ≠ F_SQ` now warns at load time about the rotational-invariance defect — recommended replacement is the new `DM_PS_*` family.
- `[umbrella]` `set_project_name` CMake shim deprecated (one-line wrapper retained for back-compat).

### Documentation milestones
- `[umbrella]` `hpo.rst` rewrite covering the user-facing HPO surface (init strategies, run policies, all driver knobs, all output files, hot-variable two-phase eval, ANNEAL + Corana, Ceres family).
- `[umbrella]` Substantively rewritten umbrella README + 5 module READMEs (CORE / MODELS / MD / MLIP / Tadah.MLIP).
- `[umbrella]` Refreshed cutoff and descriptor doc pages.
- `[umbrella]` Sphinx `-W` clean — title-level adornments fixed (`installation`, `quickstart`, `regressors`, `descriptors`), unknown `.. abbreviations::` directive in `glossary.rst` resolved, broken cross-refs (`PC_MAE` → `PC_EMAE`), API/* page label collisions, malformed table in `descriptors.rst`, `outvars.tadah` → `outvar.tadah` typo in `ex_2`, multi-line description re-indent in `read_toml.py`, cwd-relative subprocess fix in `conf.py`.
- `[umbrella]` New `training.rst` page expanding `tadah train` (CLI/config-key map, six-stage pipeline, `pot.tadah` / `weights.tadah` output formats). Load-time data transforms moved here from `hpo.rst`; the HPO page keeps a short pointer plus the validation-set sanitisation rules.
- `[umbrella]` Documentation audit pass on `index`, `intro`, `installation`, `hpo`, `hpo_examples`: branding aligned to `Tadah!MLIP` / `Tadah!LAMMPS`, MPI flagged broken-in-this-beta, stale env-var docs replaced with the `--hotvar` / `--skip-above` / `--skip-rel` CLI forms, analytical-gradient WIP note added, `bound_validation.rst` removed, `Reference suite` / `Anonymised data` sections dropped from the examples page, `Profiling harness` and `Migration from legacy syntax` sections dropped from `hpo.rst`, broken `-ltadah` link command fixed, `SSH` → `SSC` typo, code-block lexer tags corrected, `make` / `make install` / `make test` / `make docs` snippets replaced with the modern `cmake --build` / `ctest` forms.
- `[umbrella]` Examples docs synced with the actual `HPO/examples/` bundle contents; two further malformed tables repaired; stale `PRIOR` init-strategy references removed.
- `[hpo]` README updated: stale init / run-policy tables fixed, marker spellings unified.

### Internal (cross-cutting)
- Single-source-of-truth versioning: every module's `project(... VERSION ...)` and Sphinx `conf.py` reads from the blueprint TOML via `cmake-tools/cmake/TadahVersion.cmake` (`tadah_read_version_from_blueprint()`).
- Community / governance: `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, `CITATION.cff`, `AUTHORS.md`, `RELEASING.md`, `.pre-commit-config.yaml`, GitLab issue + MR templates added on the umbrella; pointer files in each module repo.
- Privacy scrub: 0 `/home/marcin` paths in tracked source post-cleanup; tracked vim swap removed; `examples/` per-run output artefacts un-tracked.
- CI/CD: lint stage (codespell), docs stage (`sphinx-build -W`, umbrella only), tag-gated release-tarball stage to GitLab package registry across all 7 module repos. HPO CI fully rewritten.
- Build wiring: `tadah_use_local_module` consumes sibling Tadah.MD / Tadah.LAMMPS sources under `TADAH_DEVELOP=1` instead of fetching.
- Build wiring: per-module test setup routed through `cmake-tools/cmake/TadahTesting.cmake` helpers (`tadah_declare_testing` / `tadah_add_tests` / `tadah_begin_external_deps` / `tadah_end_external_deps`). Replaces the hand-rolled `BUILD_TESTING` capture/restore that previously dropped the whole `tests/` subdir on a bare reconfigure (the capture ran before the cache entry existed and FORCE-restored an empty value).
- `[hpo]` Pre-beta clean-up of the init-strategy surface: dropped `PRIOR` (parser path + tutorial), dropped `INIT { INCLUDE_CONFIG }` flag and the chain-reorder behaviour it triggered.
- `[hpo]` In-tree engineer notes `HPO/LOAD_TRANSFORMS.md` and `HPO/INIT_STRATEGIES.md` removed (content folded into `hpo.rst`).
- `[models]` Unused `suggested_value()` descriptor hook dropped from the descriptor interface (no external callers).

---

## Migrating from v1.2.0 to 1.3.0-beta.1

> **Tag note.** No `v1.2.0` git tag was ever pushed; the most recent git tag in every module is `v1.1.1`. The blueprint TOML `version = "1.2.0"` field was bumped during the playground era but never tagged. In practice, "migrate from 1.2.0" means "migrate from `v1.1.1` head as of late 2025 / early 2026".

### Breaking changes

1. **`Cut_PT` normalisation (`[models]`).** The polynomial cutoff `Cut_PT` was previously off by a constant; it has been fixed to satisfy `f(0) = 1`. Any potential trained against v1.1.x with `RCUT_TYPE Cut_PT` will produce numerically different energies / forces / stresses on the new build. **Action:** retrain the affected potential, or pin the older Tadah.MODELS commit if reproducibility against an existing pot.tadah is required for your workflow.

2. **C++17 toolchain (`[core][mlip][models][md]`).** The minimum compiler standard moved from C++14 to C++17. **Action:** ensure GCC ≥ 7, Clang ≥ 5, or equivalent. CMake will fail at configure if the compiler is too old.

3. **Blueprint key casing: `atom` → `ATOM` (`[core]`).** The schema now uses upper-case `ATOM` consistently. **Action:** if you have hand-written input files using the lowercase form *and* you parse with a case-sensitive tool, capitalise. The Tadah! parser itself is case-insensitive for blueprint keys, so most users are unaffected.

4. **`set_project_name` CMake shim deprecated (`[umbrella]`).** Out-of-tree CMake users that called `set_project_name(...)` directly will see a deprecation warning. The shim still works as a one-line forward; it will be removed in a future release. **Action:** switch to `tadah_read_version_from_blueprint(TADAH_VERSION REQUIRED)` + standard `project(... VERSION ${TADAH_VERSION})`.

### Behavioural changes that don't break compilation but may change results

1. **`tadah data balance` defaults (`[core]`).** The `[TASKS.data.balance.keys.threshold]` default tightened from `["1e-1", "2e-2", "3e-3"]` to `["1e-4", "1e-5", "1e-6"]`. Runs that relied on the old defaults will now treat far more configs as outside threshold (and rescale them). **Action:** if you want the v1.1.1 behaviour, pass `THRESHOLD 1e-1 2e-2 3e-3` explicitly.
2. **`THRESHOLD` top-level default removed (`[core]`).** The top-level `THRESHOLD` ALLOWED_KEY no longer carries a `default_value = ["1e-6"]`. Tasks that previously inherited this default may now require an explicit value. **Action:** set `THRESHOLD` explicitly in your blueprint section if a task complains.
3. **`HPO best_pot` writer v2 (`[hpo]`).** The output layout differs from v1.1.1; downstream parsers should be re-validated. **Action:** re-run any post-processing scripts on a known fingerprint.
4. **`DM_mEAD` warning (`[models]`).** No behavioural change at runtime, but `DM_mEAD` with `L > 0` and `F ≠ F_SQ` now emits a load-time warning about its rotational-invariance defect. **Action:** ignore for legacy potentials; for new work, switch to a `DM_PS_*` descriptor.
5. **`TADAH_HPO_HOT_A0_*` and `TADAH_HPO_BAD_POT_ERMSE_*` env vars removed (`[hpo]`).** All five environment variables (`TADAH_HPO_HOT_A0_DEFAULT`, `TADAH_HPO_A0_MIN`, `TADAH_HPO_A0_MAX`, `TADAH_HPO_BAD_POT_ERMSE_MEV`, `TADAH_HPO_BAD_POT_ERMSE_REL`) no longer have any effect; the parser does not consult `getenv()`. **Action:** replace the `${hot_a0}` script convention with explicit `PC_LAMMPS --hotvar hot_a0 <min> <max> [<initial>]` on the producing line; replace `TADAH_HPO_BAD_POT_ERMSE_MEV=N` with `PC_ERMSE … --skip-above N`.
6. **`best_pot.tadah` warmstart-cache header (`[hpo]`).** The producer-side header line changed from `# HOT_A0 <script> <a0>` to `# HOT <script> <name> <value>` (multiple hot variables per script supported). Older `best_pot.tadah` files still parse on read for back-compat. **Action:** none for consumers; writers should expect the new format on subsequent runs.

### Deprecations

- `[models]` `DM_mEAD` with `L > 0` and `F ≠ F_SQ` — still works, prefer `DM_PS_*`.
- `[umbrella]` `set_project_name` CMake shim — still works, prefer `tadah_read_version_from_blueprint`.

### What's the same

All v1.2.0 / v1.1.1 blueprint inputs still parse (the `atom` / `ATOM` rename is handled case-insensitively by the parser, and `TYPEMB DM_<name> <mu> <nu> <El1> <El2>` still matches the 2-UINT regex form). Default behaviour is unchanged for `tadah train`, `tadah predict`, and the `pair_tadah` LAMMPS pair-style (other than the per-step bias precomputation, which only affects performance). Existing `pot.tadah` files load and predict; **only** potentials trained with `Cut_PT` will produce numerically different results — a retrain is the cleanest path forward.

## [1.2.0] and earlier (condensed)

The `v1.0.0` → `v1.1.1` releases were the project's "internal playground" era. Substantive features landed across the modules in this period — DM_Blip, DM_mEAD, the regressor zoo, NLOpt-based HPO, the LAMMPS pair-style, MPI training, install-section docs reshuffles — but no curated, cross-module changelog was kept. The blueprint TOML `version` field was rolled to `1.2.0` during this period without a corresponding git tag. Treat anything before `1.3.0-beta.1` as undocumented in the modern Keep-a-Changelog sense; the per-module `CHANGELOG.md` files contain a one-paragraph era summary for each module.
