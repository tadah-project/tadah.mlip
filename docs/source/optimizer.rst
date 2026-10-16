.. _lib-tadah-the-annealing-family:

.. _hpo_optimizer:

.. _choosing-an-optimiser:

Optimiser algorithms
====================

``LIB`` selects the optimisation library; ``ALGO`` selects its driver.
Place both in ``STRATEGY`` for a plain or multistart search, or in the
appropriate ``EXPLORE`` and ``REFINE`` blocks for a staged search.
They may also appear directly in ``OPTIMIZER`` for a plain search.

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 200
     ENDSTRATEGY
   ENDOPTIMIZER

An algorithm searches the parameters selected by ``OPTIM``. The separate
:doc:`run strategy <nfp_strategies>` controls how searches are combined;
``INNER`` configures the local search inside an NLopt composite driver.
The training configuration selects the regression algorithm for model
coefficients.

Algorithms by library
---------------------

The following names are the supported drivers. A particular driver can
still require compatible bounds, dimension count, loss sources and run
settings. Ceres drivers require a build with Ceres enabled.

.. list-table:: Libraries other than NLopt
   :header-rows: 1
   :widths: 16 30 54

   * - LIB
     - ALGO
     - Search
   * - ``TADAH``
     - ``RANDOM``
     - Configured initial point followed by uniform random draws.
   * - ``TADAH``
     - ``GRID``
     - Configured initial point followed by a Cartesian grid.
       ``PARAM GRID_POINTS`` sets its resolution.
   * - ``TADAH``
     - ``ANNEAL``
     - Serial simulated annealing.
   * - ``TADAH``
     - ``ANNEAL_PT``
     - Parallel tempering with exchanging temperature slots.
   * - ``TADAH``
     - ``ANNEAL_PA``
     - Population annealing with cooling and resampling.
   * - ``ASA``
     - ``ASA``
     - Adaptive simulated annealing.
   * - ``DLIB``
     - ``GFS``
     - Derivative-free global function search; consumes multiple
       initial points in one search.
   * - ``DLIB``
     - ``BFGS``
     - Gradient-based local search within the parameter bounds.
   * - ``CERES``
     - ``STEEPEST_DESCENT``
     - Gradient-based local search. The driver has no native bounds;
       HPO rejects proposed points outside the search box with a
       distance-based failure score.

For ``LIB NLOPT``, the complete list is grouped below. The composite
MLSL and AUGLAG drivers obtain gradients according to their local search.
Compatibility therefore depends on the selected local driver.

.. list-table:: NLopt algorithms
   :header-rows: 1
   :widths: 28 72

   * - Group
     - ALGO names
   * - Global, derivative-free
     - ``GN_AGS``, ``GN_CRS2_LM``, ``GN_DIRECT``, ``GN_DIRECT_L``,
       ``GN_DIRECT_L_NOSCAL``, ``GN_DIRECT_L_RAND``,
       ``GN_DIRECT_L_RAND_NOSCAL``, ``GN_DIRECT_NOSCAL``,
       ``GN_ESCH``, ``GN_ISRES``, ``GN_ORIG_DIRECT``, ``GN_ORIG_DIRECT_L``.
   * - Global, gradient-based
     - ``GD_STOGO``, ``GD_STOGO_RAND``.
   * - Global with local searches
     - ``GD_MLSL``, ``GD_MLSL_LDS``, ``GN_MLSL``, ``GN_MLSL_LDS``,
       ``G_MLSL``, ``G_MLSL_LDS``.
   * - Local, derivative-free
     - ``LN_BOBYQA``, ``LN_COBYLA``, ``LN_NELDERMEAD``,
       ``LN_PRAXIS``, ``LN_SBPLX``.
   * - Local, gradient-based
     - ``LD_CCSAQ``, ``LD_LBFGS``, ``LD_MMA``, ``LD_SLSQP``,
       ``LD_TNEWTON``, ``LD_TNEWTON_PRECOND``,
       ``LD_TNEWTON_PRECOND_RESTART``, ``LD_TNEWTON_RESTART``,
       ``LD_VAR1``, ``LD_VAR2``.
   * - Composite local drivers
     - ``AUGLAG``, ``AUGLAG_EQ``, ``LD_AUGLAG``, ``LD_AUGLAG_EQ``,
       ``LN_AUGLAG``, ``LN_AUGLAG_EQ``.

``G_MLSL``, ``G_MLSL_LDS``, ``AUGLAG`` and ``AUGLAG_EQ`` require an
:doc:`INNER block <nfp_inner>`. The other MLSL/AUGLAG variants have a
default local driver that an ``INNER`` block can replace.

.. _checking-a-choice:

Algorithm capabilities
----------------------

Use the executable's help to inspect the available drivers and the
settings consumed by one driver:

.. code-block:: console

   tadah explain ALGO
   tadah explain NLOPT:LN_BOBYQA
   tadah explain TADAH:ANNEAL_PA

The common setting names are documented in :doc:`nfp_optimizer_settings`
and :doc:`nfp_stopping_settings`; each driver supports its own subset.
For example, ``RANDOM``, ``GFS`` and ``ANNEAL_PT`` need an effective stop
condition, and StoGO requires ``MAXEVAL``.

A run with ``PC_LAMMPS`` must use a derivative-free driver or
``DLIB:BFGS`` with its internal finite differences. An MLSL/AUGLAG driver
is compatible when its local search is derivative-free. For validation
metrics and typed observables, check :doc:`nfp_gradients` before selecting
a gradient-based method.

These recognised names are refused: NLopt's ``LN_NEWUOA``,
``LN_NEWUOA_BOUND`` and ``NLOPT_LD_LBFGS_NOCEDAL``; Ceres' ``LBFGS``,
``WOLFE``, ``NLCG``, ``LEVENBERG_MARQUARDT``, ``DOGLEG`` and
``SUBSPACE_DOGLEG``. ``tadah explain <LIB>:<ALGO>`` gives the reason and
supported alternatives.

Annealing references
--------------------

.. toctree::
   :maxdepth: 1

   nfp_annealing_terms
   anneal
   anneal_pt
   anneal_pa
   asa
