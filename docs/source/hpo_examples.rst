.. _hpo_examples:

Worked Examples
===============

.. note::

   The HPO module ships twenty-seven end-to-end examples that build up
   gradually: ``01`` is the smallest viable config, each subsequent one
   introduces one or two new features. Between them they exercise the
   optimiser libraries, the ``INIT``, ``STRATEGY``, ``EXPLORE``,
   ``RESTART`` and ``INNER`` blocks, typed observables, per-dataset
   targets, seed-relative boxes, graded failures, ``PC_LAMMPS``
   isolation and a DM_REAM potential with the zero-referenced embedding.
   Each example is a self-contained directory with one ``config.hpo`` and
   one ``README.md``; all share a small Ta training/validation bundle in
   ``examples/common/`` and the shared ``common/run.sh`` driver, which
   takes an example's own ``config.tadah`` and ``config.val`` when it has
   them (22 and 23) and prints which files it uses. Five examples ship
   their own ``run.sh``: ``09`` and ``10`` bootstrap a seed potential
   first, ``16`` runs with its local ``config.tadah``, ``17`` trains and
   pins the model before the HPO run, and ``27`` checks the ``EMBF0 ZERO``
   contract on its outputs; start those with ``bash ./run.sh``.

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
     - `01_minimal <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/01_minimal>`_
     - Smallest viable config: one ``OPTIM`` knob optimised with
       ``DLIB:BFGS``. With no ``INIT`` block, iter 0 is the implicit
       CONFIG point. Use as a baseline for the elaborate configs.
     - ``TYPE PLAIN``, ``DLIB:BFGS``
   * - 02
     - `02_random_search <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/02_random_search>`_
     - Uniform random sampling inside the bounds via ``LIB TADAH, ALGO
       RANDOM`` — a baseline against any smarter optimiser.
     - ``TADAH:RANDOM``, ``SEED``
   * - 03
     - `03_nlopt_local <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/03_nlopt_local>`_
     - Derivative-free local search (``LN_SBPLX``, a Subplex variant of
       Nelder-Mead). Robust when finite-difference gradients are noisy.
     - ``NLOPT:LN_SBPLX``, ``STEP``
   * - 04
     - `04_skip_bad_pots <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/04_skip_bad_pots>`_
     - ``PC_ERMSE --skip-above`` / ``--skip-rel`` early-reject filter:
       fail-score obviously-broken potentials before LAMMPS is invoked.
     - ``PC_ERMSE``, ``--skip-above``, ``--skip-rel``
   * - 05
     - `05_extra_observables <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/05_extra_observables>`_
     - ``PC_LAMMPS --invar`` injects LAMMPS variables; ``--outvar``
       captures extra per-iter observables from a parametric script.
     - ``PC_LAMMPS``, ``--invar``, ``--outvar``
   * - 06
     - `06_init_random <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/06_init_random>`_
     - Introduces the ``INIT`` block: an explicit, reproducible random
       starting point (``METHOD RANDOM`` + ``SEED``; the shipped config
       uses the ``STRATEGY`` alias).
     - ``INIT.RANDOM``, ``SEED``
   * - 07
     - `07_init_lhs_multistart <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/07_init_lhs_multistart>`_
     - Latin-hypercube ``INIT`` seeds combined with ``STRATEGY TYPE
       MULTISTART`` — one local optimiser run per seed.
     - ``INIT.LHS``, ``MULTISTART``
   * - 08
     - `08_init_sobol_gfs <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/08_init_sobol_gfs>`_
     - Sobol low-discrepancy ``INIT`` seeds feeding ``DLIB:GFS``, the
       multi-seed global consumer.
     - ``INIT.SOBOL``, ``DLIB:GFS``
   * - 09
     - `09_init_warm <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/09_init_warm>`_
     - Warm restart: ``METHOD WARM`` reloads a previous best plus its
       hot-variable warmstart cache. Ideal for incremental retunes.
     - ``INIT.WARM``, ``WARM_ON_MISMATCH``
   * - 10
     - `10_init_chain <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/10_init_chain>`_
     - Chained ``INIT`` — ``WARM`` then ``LHS`` — feeding ``DLIB:GFS``;
       the chain runs in written order, so ``WARM`` seeds iter 0.
     - chained ``INIT``
   * - 11
     - `11_grid_sweep <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/11_grid_sweep>`_
     - Exhaustive Cartesian grid sweep (``LIB TADAH, ALGO GRID``) — a
       deterministic survey of a low-dimensional search box.
     - ``TADAH:GRID``, ``PARAM GRID_POINTS``
   * - 12
     - `12_prestage <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/12_prestage>`_
     - ``TYPE PRESTAGE``: a global ``EXPLORE`` stage harvests the
       ``TOPK`` candidates that the outer optimiser then refines.
     - ``TYPE PRESTAGE``, ``EXPLORE``, ``TOPK``
   * - 13
     - `13_restart <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/13_restart>`_
     - The ``RESTART`` decorator relaunches the optimiser from a fresh
       seed after ``AFTER_STAGNATION`` stalled evals, up to ``MAX`` times.
     - ``RESTART``, ``AFTER_STAGNATION``
   * - 14
     - `14_mlsl_inner <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/14_mlsl_inner>`_
     - NLopt MLSL global driver with a mandatory ``INNER`` local
       optimiser (``LN_BOBYQA``).
     - ``NLOPT:G_MLSL_LDS``, ``INNER``
   * - 15
     - `15_simulated_annealing <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/15_simulated_annealing>`_
     - Classic Kirkpatrick/Metropolis simulated annealing (``LIB TADAH,
       ALGO ANNEAL``) with an auto-calibrated initial temperature.
     - ``TADAH:ANNEAL``, ``PARAM``
   * - 16
     - `16_fixweight_mixed <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/16_fixweight_mixed>`_
     - ``FIXINDEX``/``FIXWEIGHT`` pinning combined with ``OPTIM
       FIXWEIGHT`` — mixed least-squares / pinned / searched model
       weights (ships its own local ``config.tadah``).
     - ``FIXWEIGHT``, ``OPTIM FIXWEIGHT``
   * - 17
     - `17_no_training_db <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/17_no_training_db>`_
     - No training ``DBFILE`` and no ``--validation``: every
       coefficient pinned, the loss is ``PC_LAMMPS``-only (refit-style
       polish of an existing potential).
     - no-``DBFILE`` mode, ``PC_LAMMPS``
   * - 18
     - `18_anneal_pt <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/18_anneal_pt>`_
     - Parallel tempering (``LIB TADAH, ALGO ANNEAL_PT``): a replica
       ladder with exchange moves and cumulative/windowed swap-flow
       diagnostics.
     - ``TADAH:ANNEAL_PT``, ``REPLICAS``, ``THREADS``
   * - 19
     - `19_anneal_pa <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/19_anneal_pa>`_
     - Population annealing (``LIB TADAH, ALGO ANNEAL_PA``) with
       ESS-controlled resampling and explicit all-failed handling.
     - ``TADAH:ANNEAL_PA``, ``POPULATION``, ``ESS_TARGET``
   * - 20
     - `20_anneal_corana <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/20_anneal_corana>`_
     - The Corana adaptive variant of ``LIB TADAH, ALGO ANNEAL``:
       per-coordinate step adaptation with boundary-policy diagnostics.
     - ``PARAM CORANA ON``, ``NS``/``NT``
   * - 21
     - `21_asa <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/21_asa>`_
     - Adaptive Simulated Annealing (``LIB ASA, ALGO ASA``, vendored
       Ingber ASA-30.53) with the original ASA option spellings — see
       :ref:`asa_optimizer`.
     - ``ASA:ASA``, ``PARAM Temperature_Ratio_Scale``
   * - 22
     - `22_pc_observable <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/22_pc_observable>`_
     - Example 01 with a typed observable instead of LAMMPS: the
       cohesive energy is relaxed and differentiated in process
       (:ref:`pc_observable`). ``GRAD_STRATEGY`` is unset and resolves
       to ``ANALYTICAL`` for ``DLIB:BFGS``; a local ``config.tadah`` sets
       ``OALGO 4`` and ``LAMBDA``, which the analytical gradient needs.
       Columns ``L(Ecoh)[1]`` and ``Ecoh[1]``.
     - ``PC_OBSERVABLE``, ``GRAD_STRATEGY``
   * - 23
     - `23_per_dataset_targets <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/23_per_dataset_targets>`_
     - Two validation datasets in a local ``config.val``, each with its
       own ``PC_ERMSE (i)`` target and weight, and a per-dataset
       ``--skip-above``.
     - ``PC_ERMSE (i)``, local ``config.val``
   * - 24
     - `24_optim_auto <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/24_optim_auto>`_
     - Example 03 with a seed-relative box: ``OPTIM SGRIDMB (1) AUTO``
       resolves to 03's ``[0.1, 0.4]`` (written to
       ``hpo_optim_resolved.hpo``) and reproduces 03's run.
     - ``OPTIM ... AUTO``
   * - 25
     - `25_failprobe_grading <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/25_failprobe_grading>`_
     - Example 02's seeded random search with a tight ``--hotvar``
       lattice window: six candidates fail and are graded (band A) by
       ``FAILSCORE_GRADING ON``; ``PC_FAILPROBE`` runs the sanity probe
       of :ref:`failscore_grading` (``common/lammps/failprobe.in``).
     - ``FAILSCORE_GRADING``, ``PC_FAILPROBE``
   * - 26
     - `26_pc_isolation <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/26_pc_isolation>`_
     - Example 01 with ``PC_ISOLATION PROCESS`` and a hard worker
       deadline; the results equal 01's in either isolation mode
       (:ref:`pc_lammps_execution`).
     - ``PC_ISOLATION``, ``PC_WORKER_TIMEOUT``
   * - 27
     - `27_dm_ream_embf0 <https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/v1-dev/examples/27_dm_ream_embf0>`_
     - A DM_REAM ``tadah refit`` seed with the zero-referenced embedding
       (``EMBF0 ZERO``): the search moves the negative embedding knots and
       the isolated-atom energy stays exactly 0 for every candidate.
     - ``OPTIM CEMBFUNC``, ``--outvar Eiso``
