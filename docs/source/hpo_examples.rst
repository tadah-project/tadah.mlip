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
   * - Add a LAMMPS property target
     - ``01_minimal``
   * - Try random search or a grid
     - ``02_random_search``, ``11_grid_sweep``
   * - Use several starting points
     - ``07_init_lhs_multistart``, ``08_init_sobol_gfs``
   * - Start from a saved potential
     - ``09_init_warm``
   * - Explore then refine
     - ``12_prestage``
   * - Restart after stagnation
     - ``13_restart``
   * - Use an NLopt local sub-optimiser
     - ``14_mlsl_inner``
   * - Search pinned coefficients
     - ``16_fixweight_mixed``, ``17_no_training_db``
   * - Use a built-in observable
     - ``22_pc_observable``
   * - Set targets per validation dataset
     - ``23_per_dataset_targets``
   * - Derive bounds from the seed
     - ``24_optim_auto``

The `HPO examples README
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/v1-dev/examples/README.md>`_
lists the complete set and the per-example runners. The earlier extended
catalogue, its result claims and workflow discussion are preserved in
:doc:`nfp_review_pending` for the next review pass.
