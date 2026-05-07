.. _hpo_examples:

Nested Fitting — Worked Examples
================================

.. note::

   The HPO module ships fifteen end-to-end examples that exercise every
   block in the ``STRATEGY``-rooted grammar — ``INIT``, ``NOISE``,
   ``RESTART``, ``EXPLORE``, ``REFINE``, ``INNER``. Each example is a
   self-contained directory with one ``config.hpo``, one ``README.md``,
   and a ``run.sh`` driver. They share a small training/validation
   bundle in ``examples/common/``.

   To keep a **single source of truth** the canonical content for each
   example lives in the HPO source tree. The links below point at the
   GitLab tree so the README, configuration, and any helper scripts you
   read here are byte-identical to the files you check out.

How to run one example
----------------------

After cloning the HPO repository alongside your Tadah!MLIP build (see
:ref:`installation`), run:

::

   cd HPO/examples/01_baseline_bfgs
   bash ../common/run.sh

How to run them all
-------------------

::

   bash HPO/examples/run_all.sh

The bundle is designed to complete in well under a minute on a modern
workstation; example 15 (BASINS) is the only example that runs longer
than a second. See :ref:`hpo_examples_ci` below for the regression
harness.

Index
-----

.. list-table::
   :header-rows: 1
   :widths: 6 26 50 18

   * - #
     - Example
     - What it exercises
     - Knobs
   * - 01
     - `01_baseline_bfgs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/01_baseline_bfgs>`_
     - Simplest HPO run: ``STRATEGY { TYPE PLAIN }`` with no ``INIT`` chain and
       no decorators. Iter 0 is the implicit CONFIG point. Use as a baseline
       when comparing more elaborate configurations.
     - ``TYPE PLAIN``, ``DLIB:BFGS``
   * - 02
     - `02_random_seeded <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/02_random_seeded>`_
     - Seeded random init chain into ``DLIB:BFGS``. Demonstrates
       ``INIT { STRATEGY RANDOM, INCLUDE_CONFIG false, SEED }``.
     - ``INIT.RANDOM``, ``SEED``
   * - 03
     - `03_lhs_multistart <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/03_lhs_multistart>`_
     - Latin-hypercube seeds combined with the MULTISTART policy.
       ``N_STARTS`` × ``BUDGET_PER_START`` BFGS refinements.
     - ``INIT.LHS``, ``MULTISTART``
   * - 04
     - `04_sobol_seeded_gfs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/04_sobol_seeded_gfs>`_
     - Sobol-quasirandom seeds piped into ``DLIB:GFS`` (the only optimiser that
       consumes every seed in the chain).
     - ``INIT.SOBOL``, ``DLIB:GFS``
   * - 05
     - `05_warm_refinement <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/05_warm_refinement>`_
     - Warm-start from the persistent ``HOT_A0`` cache, with
       ``WARM_ON_MISMATCH`` controlling reuse. Ideal for incremental retunes.
     - ``INIT.WARM``, ``HOT_A0``
   * - 06
     - `06_prior_seeded <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/06_prior_seeded>`_
     - Seed the optimiser with the descriptor's ``suggested_value`` prior. Use
       when a descriptor exposes a physically motivated starting point.
     - ``INIT.PRIOR``
   * - 07
     - `07_stage_global <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/07_stage_global>`_
     - PRESTAGE: a global EXPLORE stage harvests ``TOPK`` candidates that feed
       the local refinement engine. The classic explore-then-refine pattern.
     - ``TYPE PRESTAGE``, ``EXPLORE``, ``TOPK``
   * - 08
     - `08_restart_on_stagnation <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/08_restart_on_stagnation>`_
     - The RESTART decorator: relaunch the optimiser when no progress has been
       made for ``AFTER_STAGNATION`` evaluations, up to ``MAX`` times.
     - ``RESTART``, ``AFTER_STAGNATION``
   * - 09
     - `09_noise_auto_bfgs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/09_noise_auto_bfgs>`_
     - ``NOISE { AUTO }`` (bareword form) auto-calibrates the finite-difference
       step before BFGS. Removes one manual knob from gradient runs.
     - ``NOISE.AUTO``
   * - 10
     - `10_derivative_free_local <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/10_derivative_free_local>`_
     - NLopt SBPLX local search — derivative-free, robust on noisy losses.
       Useful when the gradient is unreliable.
     - ``NLOPT:LN_SBPLX``, ``STEP``
   * - 11
     - `11_chained_init_warm_lhs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/11_chained_init_warm_lhs>`_
     - Chained init combining WARM + LHS into ``DLIB:GFS``, with the WARM seed
       reordered to the front of the chain.
     - chained ``INIT``
   * - 12
     - `12_random_search <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/12_random_search>`_
     - Pure uniform random search via ``LIB TADAH, ALGO RANDOM``. The simplest
       baseline against any global optimiser.
     - ``TADAH:RANDOM``
   * - 13
     - `13_nlopt_crs_global <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/13_nlopt_crs_global>`_
     - NLopt CRS — controlled random search, pure stochastic global, no
       internal local refinement.
     - ``NLOPT:GN_CRS2_LM``
   * - 14
     - `14_nlopt_mlsl_lds <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/14_nlopt_mlsl_lds>`_
     - MLSL global + ``LN_BOBYQA`` local hybrid. Demonstrates how ``INNER {
       LIB NLOPT, ALGO ... }`` slots into a global driver.
     - ``NLOPT:G_MLSL_LDS``, ``INNER``
   * - 15
     - `15_basins_run_and_forget <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/15_basins_run_and_forget>`_
     - BASINS: the anytime/interruptible ``run and forget`` strategy. Alternates
       EXPLORE (broad sweep) and REFINE (per-basin local) at a tightening
       ``FTOL_LADDER``. Sending SIGINT/SIGTERM drops a checkpoint that the next
       invocation resumes from.
     - ``TYPE BASINS``, ``EXPLORE``, ``REFINE``, ``CHECKPOINT_EVERY``

.. _hpo_examples_ci:

Reference suite (regression harness)
-------------------------------------

Each example carries a ``reference_fingerprint.json`` next to its
``README.md``. The fingerprint records the example's converged loss,
per-component contributions (``L(ERMSE)``, ``L(Ecoh)``, …), final
hyperparameter vector, evaluation count, and an accepted-status list,
each tagged with absolute and relative tolerances chosen to absorb
cross-machine BLAS/LAPACK reorder while still flagging a material
regression in the optimiser path.

The driver

::

   bash HPO/examples/run_reference_suite.sh --fast    # CI smoke (~5 s)
   bash HPO/examples/run_reference_suite.sh --full    # release validation (~50 s)
   bash HPO/examples/run_reference_suite.sh --example=15

runs the chosen subset, compares each output against its fingerprint
within tolerance, and exits non-zero if any example regresses. Examples
flagged ``"fast": true`` in the fingerprint are eligible for
push-triggered CI; the slow subset (currently only example 15) is gated
to release-time validation.

Tolerance choices and rationale are kept beside the generator at
`tools/tolerance_overrides.json
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/examples/tools/tolerance_overrides.json>`_.

Anonymised data
---------------

The ``data/*.tadahdb`` files in ``common/`` are slimmed copies of public
DFT data. Filesystem paths in those bundles have been rewritten to
``examples/data/...`` so no private absolute path leaks. Element identity
(Ta) is retained — it is a public chemical symbol.
