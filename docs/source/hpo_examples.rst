.. _hpo_examples:

Worked examples
===============

For a short validation-only run, follow :doc:`nfp_quickstart`. The full
CLI example is :ref:`cli_example_2`.

The HPO repository also ships runnable examples under ``HPO/examples``.
Their own README files describe the required inputs and launch commands.
Start with a copy of the example directory; the runners write histories
and potentials and can clean outputs from an earlier run.

.. list-table:: Starting points
   :header-rows: 1
   :widths: 35 65

   * - Task
     - Example directory
   * - LAMMPS property targets
     - ``01_minimal``
   * - Random and grid searches
     - ``02_random_search``, ``11_grid_sweep``
   * - Multiple starting points
     - ``07_init_lhs_multistart``, ``08_init_sobol_gfs``
   * - Warm starts from saved potentials
     - ``09_init_warm``
   * - Staged exploration and refinement
     - ``12_prestage``
   * - Stagnation restarts
     - ``13_restart``
   * - NLopt local sub-optimisers
     - ``14_mlsl_inner``
   * - Pinned-coefficient searches
     - ``16_fixweight_mixed``, ``17_no_training_db``
   * - Built-in observables
     - ``22_pc_observable``
   * - Dataset-specific targets
     - ``23_per_dataset_targets``
   * - Automatic bounds from the seed
     - ``24_optim_auto``

The `HPO examples README
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/v1-dev/examples/README.md>`_
lists the complete set and the per-example runners. The earlier extended
catalogue, its result claims and workflow discussion are preserved in
:doc:`nfp_review_pending` for the next review pass.
