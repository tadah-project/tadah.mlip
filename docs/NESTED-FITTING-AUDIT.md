# Nested Fitting documentation audit

The rewritten chapter separates syntax and key placement from option values.
The earlier text is preserved in `NESTED-FITTING-DEFERRED.rst` for the next
workflow and diagnostics review. Titles use `=`, sections `-`, subsections `~`.
This pass changes documentation and help strings, not numerical behaviour.

## Source checks

Paths below refer to the sibling repositories. The main configuration grammar
is `HPO/src/hpo_targets.cpp`; common optimiser keys are defined by
`HPO/src/optimizer_block.cpp`. Backend restrictions come from
`HPO/src/optimizer_caps.cpp`, `HPO/src/backend_rules.cpp` and the adapters under
`HPO/include/tadah/hpo/`. Descriptors and named parameter bindings are checked
against MODELS, rather than inferred from legacy key names.

| Topic | Checked implementation | Correction or clarification |
|---|---|---|
| Block placement | `hpo_targets.cpp`, `strategy_rules.cpp` | STRATEGY is optional; INIT can belong to the main search or a phase; INNER is not a direct OPTIMIZER child. |
| Numeric syntax | `strict_parse.cpp`, `optimizer_block.cpp`, `asa_options.h` | Dedicated integer keys require digits; numeric PARAM values have parameter-specific validation. |
| Loss formulas | `loss_function.h`, `loss_function.cpp` | MAPE uses the measured value plus epsilon; RMSLE is a squared logarithmic difference with epsilon. |
| Observable losses | `observable_parser.cpp`, `cohesive_energy.cpp` | PC_OBSERVABLE defaults to L2 independently of root LOSS. |
| Validation targets | `validation_metric.cpp`, `hpo_host_impl.hpp`, MLIP `src/analytics.cpp` | Global metrics and indexed targets average at different stages; indexed zero-weight datasets remain in the denominator. |
| Script options | `lammps_sim.cpp`, `pc_task_exec.h`, `strict_parse.cpp` | Distinguish root failure ceilings from substituted variable values, and soft script limits from hard worker deadlines. |
| Search coordinates | `hpo_targets.cpp`, `search_space.cpp` | LOG_HP also changes explicit LIN dimensions; named smooth-cutoff radii can use AUTO. |
| Initial points | `init_strategies.cpp`, `run_policy.h` | A lone sampling method and a chain interpret K_TOTAL differently; WARM starts a new search. |
| Budgets and noise | `hpo.cpp`, `startup_validation.cpp`, `run_policy.h`, adapters | Separate per-search budgets, repeated NOISE evaluations and backend-specific tolerances. |
| Finite differences | `fd_stencil.cpp`, `analytical_gate.h` | Default kernel depends on scaling and gradient mode; analytical support is conditional on the fit and targets. |
| Multistart | `run_policy.h` | PARALLEL is parsed, but starts currently execute sequentially with a warning for values above one. |
| Algorithm catalogue | `optimizer_caps.cpp`, live `tadah explain ALGO` | List the 50 supported driver names separately from recognised-but-refused names. |
| Annealing metadata | `anneal_keys.toml`, `anneal_jitter.h`, `algorithms_pa.h`, `algorithms_pt.h`, `anneal_core.h` | Correct jitter probe accounting, PA calibration limitations and per-algorithm NEPS semantics in the shared registry. |
| ASA settings | `asa_options.cpp`, `algorithms_asa.cpp`, `external/asa/tadah_asa_config.h` | Separate integer-valued options, zero-based Sequential_Parameters and one-based indexed options. |

Detailed source locations and commands are recorded with the review evidence.

## Remaining work

- **OPEN — parser delimiter validation:** repeated OPTIMIZER blocks and some
  unclosed blocks are accepted. The syntax guide describes canonical placement
  without promising that every malformed delimiter is rejected. Add focused
  parser regressions before changing this behaviour.
- **OPEN — population-annealing automatic jitter:** the PA driver accepts
  JITTER AUTO and calibration controls but uses proposal-scale uniform bounds
  without calibration. The reference and shared help now state this. Implement
  the feature or reject inactive settings in a separately tested code change.
- **OPEN — parallel multistart:** PARALLEL above one currently warns and runs
  sequentially. The reference now states the limitation; concurrency needs a
  separate implementation and validation.
- **DEFERRED — workflow material:** review the temporary file by topic rather
  than restoring it as one long page. Recheck its claims before publishing.
- **DEFERRED — backend-specific external PARAM catalogue:** NLopt forwards
  numeric settings through its own `set_param` interface. The reference does
  not imply that every forwarded name affects every algorithm.

## Verification

The documented first run uses the existing CLI example datasets and a six-call
GRID search: the configured cutoff and five grid nodes. It is run with the
actual CLI. The parser tests cover the existing semantics used by the new
reference. Documentation checks cover Sphinx warnings, section references,
legacy `hpo.html` routes and direct links into the former long configuration
page. The registry generator checks the edited help against its checked-in
header; comparison excluding documentation strings confirms unchanged schema,
defaults and validation rules.
