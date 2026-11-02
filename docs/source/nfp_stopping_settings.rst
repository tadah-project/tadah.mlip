.. _hpo_stop_criteria:

Budgets and Convergence Settings
================================

This page shows how a search ends: its evaluation and time budgets, a
target loss and the convergence tolerances. It needs a target file with an
:hpo:block:`OPTIMIZER <BLOCK.OPTIMIZER>` block. A stopping key that the
chosen optimiser does not support is refused. :doc:`optimizer` compares the
optimisers.

Budgets and target loss
-----------------------

:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` limits the evaluations of one
search. :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>` limits its wall-clock time,
and :hpo:key:`STOPVAL <OPTIMIZER.STOPVAL>` ends it at a target loss.
Without them, the search has no explicit evaluation, time or loss limit.
The syntax, values, defaults and the optimisers that refuse a key are in
the reference entries.

:hpo:key:`!MAXTIME <OPTIMIZER.MAXTIME>` inside the
:hpo:block:`!OPTIMIZER <BLOCK.OPTIMIZER>` block differs from the top-level
key :hpo:key:`MAXTIME <ROOT.MAXTIME>`, another name for
:hpo:key:`PC_SCRIPT_TIMEOUT`, the wall-clock limit of each section of a
:hpo:key:`PC_LAMMPS` or :hpo:key:`PC_FAILPROBE` script. Evaluations already
in progress are allowed to finish.

How :hpo:key:`!STOPVAL <OPTIMIZER.STOPVAL>` is compared depends on the
optimiser. NLopt and adaptive simulated annealing (ASA) stop at any loss
at or below it, the loss of a failed evaluation included. A
:hpo:key:`!STOPVAL <OPTIMIZER.STOPVAL>` below that loss keeps a failed
evaluation from meeting the target.

The total run can exceed one :hpo:key:`!MAXEVAL <OPTIMIZER.MAXEVAL>`:
``MULTISTART`` distributes budgets, ``PRESTAGE`` has two phases, and each
restart of a :hpo:block:`RESTART <BLOCK.RESTART>` block gets new budgets.
The evaluations of :hpo:key:`CALIBRATION <NOISE.CALIBRATION>` in the
:hpo:block:`NOISE <BLOCK.NOISE>` block are additional. These cases are
described in :doc:`nfp_strategies`, :doc:`nfp_restart` and
:doc:`nfp_noise`.

Finite-difference loss evaluations and analytical gradient calls count
against the active search budget. A :hpo:key:`!MAXEVAL <OPTIMIZER.MAXEVAL>`
in an :hpo:block:`INNER <BLOCK.INNER>` block additionally limits NLopt's
calls within each local search.

Convergence tolerances
----------------------

:hpo:key:`FTOL_REL <OPTIMIZER.FTOL_REL>` and
:hpo:key:`FTOL_ABS <OPTIMIZER.FTOL_ABS>` set the relative and the absolute
change in the loss below which a search converges.
:hpo:key:`XTOL_REL <OPTIMIZER.XTOL_REL>` and
:hpo:key:`XTOL_ABS <OPTIMIZER.XTOL_ABS>` set the relative and the absolute
change in the search parameters.

The tolerances are finite numbers, with convergence tests that depend on
the optimiser. NLopt takes a value of zero or below as no tolerance, and
the annealers refuse a negative value. Some optimisers impose stricter
ranges or use a setting differently:

* :hpo:opt:`DLIB:BFGS` uses :hpo:key:`!FTOL_ABS <OPTIMIZER.FTOL_ABS>`
  (default ``1e-7``) as its objective-change stop, or positive
  ``PARAM tol_gradinf`` as an alternative, which tests the largest gradient
  component, as under ``CERES``. Both together are rejected. Both apply to
  the loss as reported. :hpo:opt:`!DLIB:BFGS` runs on the loss divided by
  the length of its first gradient, so its steps do not depend on the unit
  of the loss.
* :hpo:opt:`DLIB:GFS` uses nonnegative
  :hpo:key:`!FTOL_ABS <OPTIMIZER.FTOL_ABS>` as its solver accuracy setting
  (default ``1e-3``); the search uses its other configured stops. A
  negative :hpo:key:`!FTOL_ABS <OPTIMIZER.FTOL_ABS>` is refused at startup.
* :hpo:opt:`CERES:STEEPEST_DESCENT` uses
  :hpo:key:`!FTOL_REL <OPTIMIZER.FTOL_REL>` (default ``1e-6``) and
  :hpo:key:`!XTOL_REL <OPTIMIZER.XTOL_REL>` (default ``1e-8``), both
  nonnegative, and positive ``PARAM tol_gradinf`` (default ``1e-10``,
  largest gradient component). It refuses
  :hpo:key:`!FTOL_ABS <OPTIMIZER.FTOL_ABS>` and
  :hpo:key:`!XTOL_ABS <OPTIMIZER.XTOL_ABS>`.
* The ``FTOL_*`` settings of the annealers govern their plateau rule; see
  :doc:`anneal` and :doc:`anneal_pt`.
* With an explicit :hpo:block:`INNER <BLOCK.INNER>` block, the local
  convergence tolerances belong in that block; they are independent of the
  tolerances of the block around it.

.. versionchanged:: 1.6.0
   In 1.5.0 ``DLIB:BFGS`` compared the L2 norm of the gradient with
   ``tol_gradinf``, and its steps followed the unit of the loss.

Additional Ceres limits
-----------------------

``PARAM max_iter N`` limits solver iterations, an integer from ``0`` to
``2147483647``. Without it, the optimiser uses a large iteration limit
and relies on the evaluation budget and convergence tests.

``PARAM max_ls_iter N`` limits line-search step-size iterations, an
integer from ``1`` to ``2147483647``. Omission retains Ceres' own default.
:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` sets the separate budget for
complete loss evaluations.

Required stopping conditions
----------------------------

An algorithm without a natural end needs a supported stop. For example,
:hpo:opt:`TADAH:RANDOM`, :hpo:opt:`DLIB:GFS` and
:hpo:opt:`TADAH:ANNEAL_PT` require one; :hpo:opt:`TADAH:GRID` ends after
its grid. An optimiser that needs a stop and has none is refused before
fitting begins. An active :hpo:block:`RESTART <BLOCK.RESTART>` block, with
both :hpo:key:`AFTER_STAGNATION <RESTART.AFTER_STAGNATION>` and
:hpo:key:`MAX <RESTART.MAX>`, counts as a stop: after
:hpo:key:`!MAX <RESTART.MAX>` restarts the next stagnation ends the
search. :hpo:opt:`NLOPT:GD_STOGO` and :hpo:opt:`NLOPT:GD_STOGO_RAND`
require :hpo:key:`!MAXEVAL <OPTIMIZER.MAXEVAL>` itself.
