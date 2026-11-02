.. _hpo_embf0:

.. _hpo_rhoform:

.. _hpo_knot_rcut:

.. _hpo_two_runs_one_process:

.. _hpo_density_report:

.. _hpo_dlib_gfs_failures:

.. _hpo_repeat_points:

.. _hpo_end_status:

.. _tadah_anneal_mpi:

.. _hpo_load_transforms:

.. _hpo_stopping:

.. _warmstart_protocol:

.. _hot_value_precision:

.. _performance_parallelism:

.. _hpo_runtime_checks:

.. _hpo_env_overrides:

.. _diagnostic_markers:

.. _tadah_anneal_journal:

.. _anneal_pt_journal:

.. _anneal_pt_guide:

.. _within-one-process-openmp-and-the-worker-pool:

.. _across-mpi-ranks-tadah-hpo-mpi:

.. _bad-potential-early-exit:

.. _cutoffs-of-knot-family-refit-potentials:

.. _designing-failprobe-in:

.. _diagnostic-stdout-markers:

.. _failvar-recipes-for-typical-scripts:

.. _journal-output:

.. _load-time-data-transforms:

.. _multi-node-runs-tadah-hpo-mpi-and-archer2:

.. _order-of-firing-within-one-iteration:

.. _practical-advice:

.. _practical-tips:

.. _setup-effort-honestly:

.. _the-knobs-most-important-first:

.. _warm-start-hot-a0-or-any-hotvar:

.. _warm-start-protocol-and-bad-potential-early-exit:

.. _what-problem-this-solves:

.. _material-reserved-for-the-next-review:

Further Nested Fitting Topics
=============================

The configuration reference is separate from workflows, diagnostic
interpretation and background notes. The text of the long chapter that
these pages replace is available as a source file:

:download:`Nested Fitting chapter, reStructuredText source <../NESTED-FITTING-DEFERRED.rst>`.

Some passages of that file do not match the current settings, so it is
background material and not a reference. :doc:`nfp_reference` documents
current settings.

.. _topics-to-review-next:

Topics without a page
---------------------

* Fitting workflows: pinned coefficients, re-solving coefficients for each
  candidate, density and embedding settings, load-time transforms.
* Run lifecycle: histories, stopping, signals, seed compatibility and
  distinctions between a warm start and optimiser-state resume.
* LAMMPS workflows: hot-variable scripts, fail-probe recipes and failure
  diagnostics.
* Runtime: worker pools, MPI, environment overrides, audits and profiling.
* Optimiser explanations: detailed annealing theory, diagnostic journals,
  finite-difference derivations and worked-example result claims.

Some older documentation links lead here because their detailed workflow
text has no page yet. The shorter, checked option references remain
available from :doc:`nfp_configuration`.
