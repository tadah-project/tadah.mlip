.. _hpo_stop_criteria:

Budgets and convergence settings
================================

Stopping settings belong to an optimiser phase. Support is checked
against the selected backend; an unused setting is rejected rather than
silently ignored. See :doc:`optimizer` for backend comparisons.

Budgets and target loss
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Values and meaning
   * - ``MAXEVAL N``
     - Integer from ``1`` to ``2147483647``. Limits one search's
       evaluation budget. Omit for no explicit evaluation limit;
       ``0`` is invalid.
   * - ``MAXTIME seconds``
     - Positive finite number. Wall-time budget of the active search;
       omit for no explicit time limit. ``DLIB:BFGS`` and ``CERES``
       reject it.
   * - ``STOPVAL loss``
     - Finite target loss. ``DLIB:BFGS`` and ``CERES`` reject it;
       ``INNER`` inherits the enclosing value instead of accepting one.

``MAXTIME`` inside an optimiser is distinct from the root-level LAMMPS
script timeout. It does not forcibly interrupt an evaluation already in
progress. ``STOPVAL`` comparisons are backend-dependent; keep its value
below the failed-evaluation score when using NLopt or ASA.

The total run can exceed one ``MAXEVAL``: ``MULTISTART`` distributes
budgets, ``PRESTAGE`` has two phases, and each ``RESTART`` relaunch gets
new budgets. ``NOISE CALIBRATION`` evaluations are additional. See
:doc:`nfp_strategies`, :doc:`nfp_restart` and :doc:`nfp_noise`.

Finite-difference loss evaluations and analytical gradient calls count
against the active search budget. An ``INNER MAXEVAL`` additionally
limits NLopt's calls within each local search.

Convergence tolerances
----------------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - General role
   * - ``FTOL_REL``
     - Relative change in the loss.
   * - ``FTOL_ABS``
     - Absolute change in the loss.
   * - ``XTOL_REL``
     - Relative change or resolution in optimiser coordinates.
   * - ``XTOL_ABS``
     - Absolute change or resolution in optimiser coordinates.

These are finite numbers. Their precise tests are backend-specific;
they are not four interchangeable convergence guarantees. For supported
NLopt and annealing tests, zero or negative values disable the test.
Some adapters impose stricter ranges or use a setting differently:

* ``DLIB:BFGS`` uses ``FTOL_ABS`` (default ``1e-7``) as its
  objective-change stop, or positive ``PARAM tol_gradinf`` as an
  alternative. Both together are rejected. Despite its name,
  ``tol_gradinf`` tests the gradient's **L2 norm** in this backend.
* ``DLIB:GFS`` uses nonnegative ``FTOL_ABS`` as its solver accuracy
  setting (default ``1e-3``). It does not end the global search.
* ``CERES:STEEPEST_DESCENT`` uses ``FTOL_REL`` (default ``1e-6``),
  ``XTOL_REL`` (default ``1e-8``), both nonnegative, and positive ``PARAM tol_gradinf``
  (default ``1e-10``, largest gradient component). It refuses
  ``FTOL_ABS`` and ``XTOL_ABS``.
* Annealing's ``FTOL_*`` settings govern its plateau rule; see
  :doc:`anneal` and :doc:`anneal_pt`.
* With explicit ``INNER``, put local convergence tolerances in that
  block. They are not copied from the parent.

Additional Ceres limits
-----------------------

``PARAM max_iter N`` limits solver iterations, an integer from ``0`` to
``2147483647``. Without it, the adapter uses a large iteration limit
and relies on the evaluation budget and convergence tests.

``PARAM max_ls_iter N`` limits line-search step-size iterations, an
integer from ``1`` to ``2147483647``. Omission retains Ceres' own default.
Neither setting is a count of complete loss evaluations; use
``MAXEVAL`` for that budget.

Required stopping conditions
----------------------------

An algorithm without a natural end needs a supported stop. For example,
``TADAH:RANDOM``, ``DLIB:GFS`` and ``TADAH:ANNEAL_PT`` require one;
``TADAH:GRID`` ends after its grid. A backend that needs a stop and has
none is refused before fitting begins. Active stagnation restarts can
supply a stop where supported. NLopt's ``GD_STOGO`` and
``GD_STOGO_RAND`` require ``MAXEVAL`` specifically.
