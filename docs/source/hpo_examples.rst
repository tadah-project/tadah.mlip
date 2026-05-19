.. _hpo_examples:

Nested Fitting — Worked Examples
================================

.. note::

   The HPO module ships fifteen end-to-end examples that build up
   gradually: ``01`` is the smallest viable config, each subsequent one
   introduces one or two new features. Between them they exercise the
   optimiser libraries and the ``INIT``, ``STRATEGY``, ``EXPLORE``,
   ``RESTART``, and ``INNER`` blocks. Each example is a self-contained
   directory with one ``config.hpo`` and one ``README.md``; all share a
   small Ta training/validation bundle in ``examples/common/`` and the
   shared ``common/run.sh`` driver (two examples ship their own
   ``run.sh`` to bootstrap a seed first).

   To keep a **single source of truth** the canonical content for each
   example lives in the HPO source tree. The links below point at the
   GitLab tree so the README, configuration, and any helper scripts you
   read here are byte-identical to the files you check out.

How to run one example
----------------------

After cloning the HPO repository alongside your Tadah!MLIP build (see
:ref:`installation`), run:

::

   cd HPO/examples/01_minimal
   bash ../common/run.sh

How to run them all
-------------------

::

   bash HPO/examples/run_all.sh

prints a one-line summary per example (best loss, eval count, status).

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
     - `01_minimal <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/01_minimal>`_
     - Smallest viable config: one ``OPTIM`` knob optimised with
       ``DLIB:BFGS``. With no ``INIT`` block, iter 0 is the implicit
       CONFIG point. Use as a baseline for the elaborate configs.
     - ``TYPE PLAIN``, ``DLIB:BFGS``
   * - 02
     - `02_random_search <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/02_random_search>`_
     - Uniform random sampling inside the bounds via ``LIB TADAH, ALGO
       RANDOM`` — a baseline against any smarter optimiser.
     - ``TADAH:RANDOM``, ``SEED``
   * - 03
     - `03_nlopt_local <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/03_nlopt_local>`_
     - Derivative-free local search (``LN_SBPLX``, a Subplex variant of
       Nelder-Mead). Robust when finite-difference gradients are noisy.
     - ``NLOPT:LN_SBPLX``, ``STEP``
   * - 04
     - `04_skip_bad_pots <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/04_skip_bad_pots>`_
     - ``PC_ERMSE --skip-above`` / ``--skip-rel`` early-reject filter:
       fail-score obviously-broken potentials before LAMMPS is invoked.
     - ``PC_ERMSE``, ``--skip-above``, ``--skip-rel``
   * - 05
     - `05_extra_observables <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/05_extra_observables>`_
     - ``PC_LAMMPS --invar`` injects LAMMPS variables; ``--outvar``
       captures extra per-iter observables from a parametric script.
     - ``PC_LAMMPS``, ``--invar``, ``--outvar``
   * - 06
     - `06_init_random <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/06_init_random>`_
     - Introduces the ``INIT`` block: an explicit, reproducible random
       starting point (``STRATEGY RANDOM`` + ``SEED``).
     - ``INIT.RANDOM``, ``SEED``
   * - 07
     - `07_init_lhs_multistart <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/07_init_lhs_multistart>`_
     - Latin-hypercube ``INIT`` seeds combined with the ``MULTISTART``
       policy — one local optimiser run per seed.
     - ``INIT.LHS``, ``MULTISTART``
   * - 08
     - `08_init_sobol_gfs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/08_init_sobol_gfs>`_
     - Sobol low-discrepancy ``INIT`` seeds feeding ``DLIB:GFS``, the
       multi-seed global consumer.
     - ``INIT.SOBOL``, ``DLIB:GFS``
   * - 09
     - `09_init_warm <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/09_init_warm>`_
     - Warm restart: ``STRATEGY WARM`` reloads a previous best plus its
       hot-variable warmstart cache. Ideal for incremental retunes.
     - ``INIT.WARM``, ``WARM_ON_MISMATCH``
   * - 10
     - `10_init_chain <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/10_init_chain>`_
     - Chained ``INIT`` — ``WARM`` then ``LHS`` — feeding ``DLIB:GFS``;
       the chain runs in written order, so ``WARM`` seeds iter 0.
     - chained ``INIT``
   * - 11
     - `11_grid_sweep <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/11_grid_sweep>`_
     - Exhaustive Cartesian grid sweep (``LIB TADAH, ALGO GRID``) — a
       deterministic survey of a low-dimensional search box.
     - ``TADAH:GRID``, ``STEP``
   * - 12
     - `12_prestage <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/12_prestage>`_
     - ``TYPE PRESTAGE``: a global ``EXPLORE`` stage harvests the
       ``TOPK`` candidates that the outer optimiser then refines.
     - ``TYPE PRESTAGE``, ``EXPLORE``, ``TOPK``
   * - 13
     - `13_restart <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/13_restart>`_
     - The ``RESTART`` decorator relaunches the optimiser from a fresh
       seed after ``AFTER_STAGNATION`` stalled evals, up to ``MAX`` times.
     - ``RESTART``, ``AFTER_STAGNATION``
   * - 14
     - `14_mlsl_inner <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/14_mlsl_inner>`_
     - NLopt MLSL global driver with a mandatory ``INNER`` local
       optimiser (``LN_BOBYQA``).
     - ``NLOPT:G_MLSL_LDS``, ``INNER``
   * - 15
     - `15_simulated_annealing <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/15_simulated_annealing>`_
     - Classic Kirkpatrick/Metropolis simulated annealing (``LIB TADAH,
       ALGO ANNEAL``) with an auto-calibrated initial temperature.
     - ``TADAH:ANNEAL``, ``PARAM``
