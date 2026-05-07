.. _nested_fitting:

Nested Fitting
==============

.. note::

   We use :term:`NFP` (Nested Fitting Procedure) and :term:`HPO`
   (Hyperparameter Optimization) interchangeably to refer to this feature.
   The main :term:`CLI` entry point is ``tadah hpo``.  *NFP* is preferred in
   prose to emphasise the two-level fitting structure that is specific to
   Tadah!MLIP; *HPO* is the spelling used by every keyword and source-file
   identifier.

.. figure:: images/nested_fitting.jpg
   :alt: Nested Fitting Procedure
   :width: 95%
   :align: center

   Workflow of the :term:`nested fitting procedure`: the outer optimiser
   proposes new :term:`hyperparameters<hyperparameter>`, the model is
   retrained, and each trial potential is validated against
   :term:`performance constraints<performance constraint>` that feed back
   into the global loss.

Nested fitting is Tadah!MLIP’s automated, two-level fitting workflow:

* **inner loop** — ordinary regression that determines the
  :term:`learned parameters<learned parameter>` :math:`\mathbf w` for a
  fixed model definition;
* **outer loop** — a global optimiser that samples from the
  :term:`search space constraints<search space constraint>` (SSH),
  retrains the model, and judges the result against user-defined
  :term:`performance constraints<performance constraint>` (PC).

Letting a computer search the :term:`hyperparameter` space helps to

* escape the “good on the validation set, unstable in MD” trap;
* trade accuracy for speed (or vice versa) in a reproducible way;
* fold real-world performance constraints — elastic constants, phase
  stability, surface energies, … — directly into the loss function.

Background
----------

A traditional MLIP workflow stops once regression converges on a
training/validation split.  Unfortunately, a potential that interpolates
perfectly may still

* disintegrate during an MD run,
* produce an incorrect equation of state,
* predict the wrong ground-state crystal,
* extrapolate poorly beyond the training set.

Manual hyperparameter tuning or simply enlarging the training set are
both tedious and not guaranteed to succeed.

Nested fitting tackles the problem by *measuring* emergent properties
during the fit.  Each trial potential is dropped into a short LAMMPS
run; the resulting quantities are compared to their targets and the
discrepancy contributes to a **global loss**

.. math::

  L_{\text{total}} = \sum_{i} w_i\;
                     \mathcal L_i\!\bigl(|y_i-\hat y_i|\bigr),

where :math:`w_i` is the user-supplied weight, :math:`y_i` the target,
:math:`\hat y_i` the prediction, and :math:`\mathcal L_i` one of the
built-in loss functions (L1, L2, Huber, Tukey, …).

The outer optimiser explores the hyperparameter space :math:`\Theta`
declared with the ``OPTIM`` directive:

.. math::

  \theta^\star
  = \operatorname*{arg\,min}_{\theta\in\Theta} L_{\text{total}}(\theta).

Because the model is retrained for every candidate :math:`\theta`, the
procedure is computationally expensive but also powerful: it can expose
regions of hyperparameter space that yield stable, accurate, and fast
potentials.  It is not a silver bullet — **success still depends on
sensible choices of performance constraints and search space limits.**

.. _nfp_quick_start:

Quick Start
-----------

Nested fitting needs three input files:

#. A :term:`training configuration` — the same configuration file you
   would use for ``tadah train``.  See :ref:`ConfigSection` for the
   full key list.
#. A *validation* file — a list of validation datasets passed via the
   ``DBFILE`` keyword.
#. An :term:`HPO configuration` — the file documented on this page.
   Conventionally named ``config.hpo``.

The driving command line is

::

   tadah hpo --config <config.train> --hpotarget <config.hpo> --validation <config.val>

A minimal ``config.hpo``:

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB DLIB
       ALGO BFGS
       MAXEVAL 50
     ENDSTRATEGY
   ENDOPTIMIZER

   LOSS L2
   PC_ERMSE 0 1.0
   OPTIM RCUT2B (1) 4.0 6.0

See :ref:`cli_example_2` for a slightly bigger walkthrough, and
:ref:`hpo_examples` for a series of fifteen worked examples that exercise
every feature on this page.

.. _nfp_manual:

Configuration file
------------------

This section is the reference for the HPO configuration file.  Three
in-tree notes go deeper than the user-facing documentation can afford
to:

* `HPO/README.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_
  — keyword reference, every flag and default, plus the
  *OPTIM-able context keys* table.
* `HPO/INIT_STRATEGIES.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/INIT_STRATEGIES.md>`_
  — tutorial for the ``INIT`` / ``STRATEGY`` / ``RESTART`` / ``NOISE``
  blocks with worked scenarios.
* `HPO/LOAD_TRANSFORMS.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/LOAD_TRANSFORMS.md>`_
  — load-time dataset transforms (LSCALE, ESHIFT, EFILTER, weighting…).

Block hierarchy
...............

The HPO configuration is a sequence of *root-level* keywords (``LOSS``,
``OPTIM``, ``PC_*``, ``OUTPUT``, …) plus a single ``OPTIMIZER``
``ENDOPTIMIZER`` block.  The optimiser block has nested blocks, listed
in the order they normally appear:

.. code-block:: none

   OPTIMIZER
     INIT       ... ENDINIT          # optional: seeding strategy chain
     NOISE      ... ENDNOISE         # optional: objective-noise calibration
     STRATEGY   ... ENDSTRATEGY      # required: TYPE + driver + decorators
       EXPLORE  ... ENDEXPLORE       # required for TYPE BASINS / PRESTAGE
       REFINE   ... ENDREFINE        # required for TYPE BASINS / PRESTAGE
       INNER    ... ENDINNER         # optional: NLopt MLSL/AUGLAG inner
       RESTART  ... ENDRESTART       # optional decorator
   ENDOPTIMIZER

Block keywords are case-sensitive.  Comments start with ``#``; long
lines may be continued with a trailing ``\``.

A legacy form without a ``STRATEGY`` block — ``LIB``/``ALGO``/``MAXEVAL``
written directly inside ``OPTIMIZER`` — is still accepted and behaves
exactly like ``STRATEGY { TYPE PLAIN ... }``.  It is fine for short
single-shot runs but cannot express ``MULTISTART``, ``PRESTAGE``,
``BASINS``, init chains, or ``RESTART`` decorators; those features
require the explicit grammar.

.. _loss_functions:

LOSS — global loss function
...........................

::

   LOSS <Name> [<extra params>]

The default loss applied to every metric that does not override it.
``PC_LAMMPS`` per-variable lines may pick a different loss locally.

================ =============== ==============================
Name             Extra params    Comment
================ =============== ==============================
``L1``           —               :math:`|x|`
``L2``           —               :math:`x^2`
``HUBER``        ``δ``           quadratically near zero, linear beyond δ
``TUKEY``        ``c``           redescending, zero influence for ``|x| > c``
``LOG_COSH``     —               smooth alternative to L1
``RMSLE``        —               log-scaled L2 (non-negative targets)
================ =============== ==============================

Root-level loss control:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key (type)
     - Description
   * - ``FAILSCORE`` (float)
     - Cap on total loss; values above are clipped (default: max
       double).  May be overridden per-script with ``--failscore`` on
       a ``PC_LAMMPS`` line.

.. _search_space_constraints:

OPTIM — search space constraints
................................

The ``OPTIM`` directive declares which configuration keys the outer
optimiser may vary, and over what bounds:

.. code-block:: none

   OPTIM <KEY> (indices) <low> <high>

* ``<KEY>`` — any optimisable key from the training configuration.  For
  the full list of OPTIM-safe keys (and the tightly enumerated set of
  *unsafe* ones, e.g. ``RCUT2B`` with ``D2_EAM``), see the
  *OPTIM-able context keys: reinit-safety reference* table in
  `HPO/README.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_.
* ``(indices)`` — selects a subset of the values bound to ``<KEY>``.
  Indices follow the order in the training configuration and start
  from **1**.  Comma-separated lists, ranges ``a-b``, and strides
  ``a-b:s`` are all accepted (e.g. ``(1,4,7-10:2)``).
* ``<low>``, ``<high>`` — floating-point bounds.  ``high`` must be
  strictly greater than ``low``.

``OPTIM`` lines may be repeated for the same key with different indices.

.. _performance_constraints:

PC_* — performance constraints
..............................

Energy / force / stress validation metrics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Validation-set metrics are evaluated on every ``DBFILE`` listed in the
``--validation`` file.  Each line takes a target value and a weight:

.. code-block:: none

   PC_<METRIC> <target> <weight>

================== =================================================================
Key                Quantity
================== =================================================================
``PC_EMAE``        Energy mean absolute error (per atom)
``PC_ERMSE``       Energy root-mean-square error (per atom)
``PC_ErRMSE``      Energy *relative* RMSE
``PC_ERSQ``        Coefficient of determination (R²) for energies
``PC_FRMSE``       Force component RMSE
``PC_SRMSE``       Stress component RMSE
================== =================================================================

These constraints use the global ``LOSS`` selected above.

Physics-informed constraints (PC_LAMMPS)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``PC_LAMMPS`` runs a regular LAMMPS script against each trial potential
and feeds one or more LAMMPS *equal-style* variables back into the
loss:

.. code-block:: none

   PC_LAMMPS --script in.mysim \
             --varloss myVar       0   100 \
             --varloss myOtherVar 145    10

A long ``PC_LAMMPS`` line may be split with a trailing back-slash
(``\``).  Tadah!MLIP creates an isolated ``LAMMPS_NS::LAMMPS`` instance
per script per worker thread; instances are reused across iterations
(the script's own ``clear`` directive resets simulation state between
runs, so existing scripts work unmodified).

Required options
^^^^^^^^^^^^^^^^

``--script <file>``
   LAMMPS input file containing the variable definitions.

``--varloss <name> <target> <weight> [loss-type p₁ p₂ …]``
   The variable ``<name>`` (defined inside the LAMMPS script) is read
   at the end of the run and combined with ``<target>`` and
   ``<weight>`` via the global ``LOSS`` (or the per-line override).
   Optional trailing tokens override the loss for this variable only.

Additional options
^^^^^^^^^^^^^^^^^^

``--invar <name> <value>``
   Inject a variable (equivalent to LAMMPS' ``-var`` flag), letting the
   same script be reused for several structures or pressures.

``--outvar <name>``
   Record the variable in ``outvar.tadah`` without contributing to the
   loss.  Useful for diagnostic plots.

``--failscore <value>``
   Override the global ``FAILSCORE`` for this script.  If LAMMPS
   crashes or the loss exceeds this value the optimiser receives
   ``FAILSCORE`` for the whole iteration.

``--ncpu <m>``
   Request *m* MPI ranks for this script in MPI builds.  In the
   default serial build the option is parsed and accepted but does
   not change runtime; HPO logs a stderr warning when ``--ncpu > 1``
   is set in a non-MPI build.  See :ref:`performance_parallelism`.

Result contract for user scripts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

LAMMPS scripts driven by HPO must communicate their results back
*only* through ``--varloss`` / ``--outvar`` (LAMMPS ``equal``-style
variables).  They should avoid writing files (dumps, restarts,
``write_data``, custom logs) unless filenames are made unique, because
all OpenMP threads share the process working directory.

If ``-log <file>`` or ``-screen <file>`` is supplied explicitly inside
a ``PC_LAMMPS`` entry, HPO appends ``.t<thread_id>`` per OpenMP thread
(so e.g. ``-log run.log`` becomes ``run.log.t0``, ``run.log.t1``, …).
``-log none`` and ``-screen none`` are left untouched.

OPTIMIZER block — common attributes
...................................

The driver-attribute keys below are accepted wherever an
``OptimizerSpec`` is the active receiver: at the top level of
``STRATEGY`` (when ``TYPE`` is ``PLAIN`` or ``MULTISTART``) and inside
``EXPLORE``, ``REFINE``, and ``INNER`` sub-blocks.

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Key (type)
     - Description
   * - ``LIB`` (string)
     - Optimisation library: ``DLIB``, ``NLOPT``, ``TADAH``.
   * - ``ALGO`` (string)
     - Algorithm (see table below).
   * - ``MAXEVAL`` (uint)
     - Maximum number of evaluations.
   * - ``MAXTIME`` (uint)
     - Maximum wall-time in seconds (also accepted at root level).
   * - ``STOPVAL`` (float)
     - Stop when the loss falls below this value.
   * - ``FTOL_REL`` (float)
     - Relative tolerance on the loss.
   * - ``FTOL_ABS`` (float)
     - Absolute tolerance on the loss.
   * - ``XTOL_REL`` (float)
     - Relative tolerance on hyperparameters.
   * - ``XTOL_ABS`` (float)
     - Absolute tolerance on hyperparameters.
   * - ``POPULATION`` (uint)
     - Population size for population-based algorithms.
   * - ``STEP`` (float)
     - Initial step size.
   * - ``SEED`` (uint)
     - Random seed (default: current time).  Inside an ``INIT`` block
       the same key seeds the stochastic init strategies instead.
   * - ``VECTOR_ARR`` (uint)
     - Storage size required by some NLOpt algorithms.
   * - ``THREADS`` (uint)
     - Outer thread count for ``DLIB:GFS`` (default: 1).
   * - ``PARAM`` (string float)
     - Generic key/value passed to the algorithm (e.g.
       ``PARAM fd_step 1e-3``).

.. table:: Algorithms by library
   :widths: auto

   +-----------+------------------------------------------------------------------------------------------+
   | LIB       | ALGO (algorithms from the selected library)                                              |
   +-----------+------------------------------------------------------------------------------------------+
   | ``TADAH`` | ``RANDOM`` (uniform random search), ``GRID``                                             |
   +-----------+------------------------------------------------------------------------------------------+
   | ``NLOPT`` | All gradient-free NLOpt globals (``GN_*``) and locals (``LN_*``) — see                   |
   |           | the `NLOpt algorithm catalogue                                                           |
   |           | <https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/>`_ for the                     |
   |           | full list.  Algorithms whose names begin with ``GD_``/``LD_``                            |
   |           | (gradient-using) are *not* supported because Tadah!MLIP does not                         |
   |           | expose analytic loss gradients.                                                          |
   +-----------+------------------------------------------------------------------------------------------+
   | ``DLIB``  | ``BFGS``, ``LBFGS``, ``CG``, ``BOBYQA`` (local), ``GFS`` (the dlib                       |
   |           | MaxLIPO+TR global *Global Function Search*).                                             |
   +-----------+------------------------------------------------------------------------------------------+

INIT block — seeding strategies
...............................

Outside an ``INIT { ... } ENDINIT`` block the optimiser starts at the
single point taken from the values next to your ``OPTIM`` lines (the
implicit ``CONFIG`` seed).  Inside an ``INIT`` block you may declare a
*chain* of strategies that produces multiple starting points.

Chain entries
~~~~~~~~~~~~~

Each ``STRATEGY <method>`` line appends one entry to the chain.
The chain is resolved into a flat list of starting points before the
optimiser runs.

================ ============== ============= ===========================================================
Method           Args           Default K     Effect
================ ============== ============= ===========================================================
``CONFIG``       (none)         1             Take the value next to each ``OPTIM`` key.
``RANDOM K``     ``K`` (uint)   ``K``         ``K`` uniform samples within (transformed) bounds.
``LHS K``        ``K`` (uint)   ``K``         Latin-hypercube sample (one per stratum per dim).
``SOBOL K``      ``K`` (uint)   ``K``         Sobol low-discrepancy sequence.
``WARM <file>``  path           1             Read a previous ``pot.tadah`` and pull values for
                                              every ``OPTIM`` key.
``PRIOR``        (none)         1             Per-descriptor ``suggested_value``; midpoint fallback.
================ ============== ============= ===========================================================

Top-level keys inside the ``INIT`` block
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 22 16 22 40

   * - Key
     - Type
     - Default
     - Effect
   * - ``STRATEGY``
     - repeatable
     - implicit ``CONFIG`` when chain is empty
     - Append a strategy entry to the chain (see table above).
   * - ``K_TOTAL``
     - uint
     - sum of strategies' default K
     - Cap on total starting points produced.
   * - ``SEED``
     - uint
     - random
     - Seed for stochastic strategies (RANDOM / LHS / SOBOL).
   * - ``INCLUDE_CONFIG``
     - bool
     - ``true``
     - When the chain is **empty**, include the implicit ``CONFIG``
       seed.  *A non-empty chain suppresses the implicit CONFIG
       regardless of this flag* — write ``STRATEGY CONFIG`` if you
       want both.
   * - ``WARM_ON_MISMATCH``
     - ``warn`` / ``error`` / ``skip``
     - ``warn``
     - What to do when ``WARM`` cannot fill every ``OPTIM`` key.

Two implementation details affect reproducibility:

* The first ``WARM`` entry is moved to the front of the chain regardless
  of where the user wrote it; iter 0 then starts from the warm point.
  HPO logs ``[HPO] InitStrategy: WARM moved to chain front`` when the
  reorder actually changes the order.
* When the writer that produced the ``WARM`` file dumped
  ``# HOT_A0 <script> <a0>`` lines, ``WARM`` reads them and primes the
  warm-start cache (see :ref:`warmstart_protocol`).

How many seeds does the optimiser actually consume?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A chain that produces more starting points than the optimiser can use
emits a stderr warning at config time and silently discards the rest.

================================ ===================================================
Optimiser                        Seeds consumed
================================ ===================================================
``DLIB:GFS``                     All K (forwarded as ``initial_function_evals``).
``DLIB:BFGS``                    First only (``optim_init.front()``).
Any ``NLOPT`` algorithm          First only.
``TADAH:RANDOM``, ``TADAH:GRID`` First only — used as eval 0, then sweep.
================================ ===================================================

``MULTISTART > 1`` runs the inner optimiser ``K`` times and consumes one
seed per start.  ``DLIB:GFS`` collapses to a single inner call because
GFS already takes ``K`` seeds natively.

Bound transforms
~~~~~~~~~~~~~~~~

Search-space dimensions whose ``high/low`` ratio exceeds ``LOG_HP``
(declared inside ``STRATEGY`` or ``OPTIMIZER``) are log-transformed.
All internal arithmetic happens in transformed space; only the
reported parameter values are exponentiated back.  Default is *no
transform* — set ``LOG_HP <ratio>`` to opt in.

For deeper guidance — including chain reorderings, the
``[HPO] InitStrategy: explicit chain set, implicit CONFIG suppressed``
diagnostic, and worked scenarios — see
`HPO/INIT_STRATEGIES.md <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/INIT_STRATEGIES.md>`_.

.. _strategy_block:

STRATEGY block
..............

The ``STRATEGY { ... } ENDSTRATEGY`` block is the run-policy hub.  Its
``TYPE`` selects the dispatch shape:

================== =================================================================
``TYPE``           Effect
================== =================================================================
``PLAIN``          Single-shot run of the driver named by ``LIB``/``ALGO``.
``MULTISTART``     Run the driver ``N_STARTS`` independent times, each from a
                   different seed in the chain.
``PRESTAGE``       Two-stage: an ``EXPLORE { ... }`` block runs first, the top-K
                   seeds are then refined by the outer driver.
``BASINS``         Anytime/interruptible round-by-round basin exploration —
                   alternates ``EXPLORE`` and ``REFINE`` stages over a persistent
                   basin pool.
================== =================================================================

TYPE PLAIN
~~~~~~~~~~

The simplest dispatch: a single run of ``LIB``/``ALGO``.  The driver
attribute table above lists every accepted key.  No nested blocks
other than ``RESTART`` and ``INNER`` (for NLopt MLSL/AUGLAG) are
relevant.

TYPE MULTISTART
~~~~~~~~~~~~~~~

Runs the inner driver ``N_STARTS`` independent times.  The host's
best-tracker keeps the global minimum across all runs.

.. list-table::
   :header-rows: 1
   :widths: 22 16 14 48

   * - Key
     - Args
     - Default
     - Effect
   * - ``N_STARTS``
     - int ≥ 1
     - ``1``
     - Number of independent starts.
   * - ``PARALLEL``
     - int ≥ 1
     - ``1``
     - Concurrent starts (currently parsed and validated; runs
       sequentially with a stderr warning if ``PARALLEL > 1``).
   * - ``BUDGET_PER_START``
     - int ≥ 1
     - derived
     - Per-start ``MAXEVAL`` override.  When unset HPO splits the
       outer ``MAXEVAL`` equally across ``N_STARTS`` (ceil division).

``MULTISTART`` with ``N_STARTS > 1`` is rejected at validate-time for
``NLOPT:GN_*`` algorithms (those globals consume only one starting
point).  Pick ``LD_*`` / ``LN_*`` or ``DLIB:GFS`` / ``DLIB:BFGS``
instead.

TYPE PRESTAGE — staged global → local
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A self-contained ``EXPLORE { ... } ENDEXPLORE`` sub-block describes the
*first* (exploration) phase; the outer ``LIB``/``ALGO`` drives the
*second* (refinement) phase over the top-K candidates harvested by the
EXPLORE pass.

.. code-block:: none

   STRATEGY
     TYPE PRESTAGE
     LIB DLIB
     ALGO BFGS              # local refinement, runs SECOND
     MAXEVAL 60
     TOPK 3
     EXPLORE
       LIB DLIB
       ALGO GFS             # global exploration, runs FIRST
       MAXEVAL 200
     ENDEXPLORE
   ENDSTRATEGY

================ ============= =============================================================
Key              Args          Effect
================ ============= =============================================================
``TOPK``         int ≥ 1       Number of best (loss, params) pairs the EXPLORE phase
                               forwards to refinement.  ``TOPK > 1`` triggers an automatic
                               multistart over the K seeds; per-start ``MAXEVAL`` is split
                               from the outer ``MAXEVAL`` by the same rule MULTISTART uses.
================ ============= =============================================================

The ``EXPLORE`` block accepts the full driver-attribute set
(``LIB``/``ALGO``/``MAXEVAL`` / …); it inherits the outer bounds and
log-transform automatically.

TYPE BASINS — anytime "run and forget"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``BASINS`` is a self-contained top-level strategy designed for
hour/day/week-long runs on workstations or SLURM clusters.  Round
``k = 1, 2, …`` alternates an ``EXPLORE`` stage at
``EXPLORE.FTOL_LADDER[k]`` (broad exploration; harvests basin
candidates) with a ``REFINE`` stage that visits the cumulative
``REFINE.FTOL_LADDER[1..k]`` and locally refines every basin in the
persistent pool.  ``SIGINT`` (ctrl-C), ``SIGTERM``, and ``SIGUSR2``
all trigger a clean checkpoint commit; re-running the same command
resumes from ``checkpoint.tadah``.

Default behaviour without any per-stage overrides:

* ``EXPLORE`` = ``NLOPT GN_CRS2_LM``.
* ``REFINE`` = ``NLOPT LN_BOBYQA``.
* Default ladders auto-derived from ``NOISE.CALIBRATION``, with a
  ``1e-9`` floor.
* 32-basin pool, 5-round emergency stop, no checkpoint cadence
  (stage-boundary commits only).

The outer ``OPTIMIZER`` ``MAXEVAL`` and ``MAXTIME`` remain valid
overall budgets.  ``BASINS`` ignores ``RESTART``, ``MULTISTART``, and
``PRESTAGE`` (it drives global+local internally).

.. table:: BASINS-wide knobs (inside the ``STRATEGY`` block, ``TYPE BASINS``)
   :widths: 30 18 14 38

   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | Key                        | Args            | Default  | Effect                                                   |
   +============================+=================+==========+==========================================================+
   | ``BASIN_POOL_MAX``         | int ≥ 1         | 32       | Capacity of the persistent basin pool (worst-loss        |
   |                            |                 |          | eviction on overflow).                                   |
   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | ``BASIN_DEDUP_DIST``       | float ≥ 0       | 0.05     | Relative L2 in normalised bbox: candidates closer than   |
   |                            |                 |          | this merge with an existing basin.                       |
   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | ``STOP_NO_IMPROVE_ROUNDS`` | int             | 5        | Emergency stop after this many consecutive rounds        |
   |                            |                 |          | without a new global-best improvement.                   |
   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | ``SEED_TOP_N``             | int ≥ 1         | 1        | Pre-evaluate top-N basins at the start of each round     |
   |                            |                 |          | ``k > 1`` ``EXPLORE`` stage; seed the optimiser from the |
   |                            |                 |          | lowest-loss of them.                                     |
   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | ``CLAMP_TO_NOISE_FLOOR``   | bool            | ``false``| Strictly trim FTOL ladder rungs below the                |
   |                            |                 |          | ``NOISE.CALIBRATION``-suggested noise floor.  Opt in     |
   |                            |                 |          | only when the calibration is trustworthy (a converged    |
   |                            |                 |          | WARM seed, not a fresh BAD_POT).                         |
   +----------------------------+-----------------+----------+----------------------------------------------------------+
   | ``CHECKPOINT_EVERY``       | ``Ns`` or ``N`` | 0 (off;  | Time- (``Ns``) or eval-based (``N``) checkpoint cadence. |
   |                            |                 | stage-   | Either way, a checkpoint is also committed at every      |
   |                            |                 | boundary | stage boundary.                                          |
   |                            |                 | only)    |                                                          |
   +----------------------------+-----------------+----------+----------------------------------------------------------+

.. table:: Per-stage knobs (inside ``EXPLORE { ... }`` or ``REFINE { ... }``, ``TYPE BASINS``)
   :widths: 30 18 14 38

   +-----------------------------+----------+-----------+----------------------------------------------------------+
   | Key                         | Args     | Default   | Effect                                                   |
   +=============================+==========+===========+==========================================================+
   | ``LIB``, ``ALGO``           | string   | see above | Stage driver.                                            |
   +-----------------------------+----------+-----------+----------------------------------------------------------+
   | ``FTOL_LADDER``             | list     | auto from | Per-round ``ftol_rel`` rungs.  Round k visits the first  |
   |                             | of >0    | NOISE     | ``k`` rungs (``REFINE``) or rung ``k`` (``EXPLORE``).    |
   |                             |          |           | Lengths past the list cap to the last rung.              |
   +-----------------------------+----------+-----------+----------------------------------------------------------+
   | ``BUDGET_MIN_EVALS``        | int      | 30        | Floor on per-stage evals before stuck-detection fires.   |
   +-----------------------------+----------+-----------+----------------------------------------------------------+
   | ``BUDGET_MAX_EVALS``        | int      | 5000      | Per-stage / per-basin safety cap for the inner driver.   |
   +-----------------------------+----------+-----------+----------------------------------------------------------+
   | ``BUDGET_RATE_FACTOR``      | float ≥0 | 0.5       | Per-stage stuck threshold: ``is_stuck`` requires         |
   |                             |          |           | ``rate_per_eval < factor × tol``.                        |
   +-----------------------------+----------+-----------+----------------------------------------------------------+

For the rationale behind the LADDER / BUDGET defaults, the running-best
diagnostics, and recommended tuning of ``BASIN_POOL_MAX`` for high-D
problems, see
`HPO/README.md
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_
and the worked example
`15_basins_run_and_forget
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/tree/develop/examples/15_basins_run_and_forget>`_.

.. _restart_decorator:

RESTART decorator
.................

``RESTART { ... } ENDRESTART`` decorates a ``TYPE PLAIN`` /
``MULTISTART`` / ``PRESTAGE`` strategy with a stagnation watchdog.  It
does *not* compose with ``TYPE BASINS`` (BASINS drives its own
global+local cycle internally).

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB DLIB
     ALGO BFGS
     MAXEVAL 1000
     RESTART
       AFTER_STAGNATION 80
       MAX 5
       STRATEGY SOBOL 1
     ENDRESTART
   ENDSTRATEGY

.. list-table::
   :header-rows: 1
   :widths: 22 16 62

   * - Key
     - Args / range
     - Effect
   * - ``AFTER_STAGNATION``
     - int ≥ 0
     - After this many consecutive evals without a new global-best,
       abort the inner run and restart from a fresh seed.  ``0``
       disables.
   * - ``MAX``
     - int ≥ 0
     - Cap on restart count.
   * - ``STRATEGY``
     - repeatable
     - Init chain used to draw the fresh point on each restart
       (re-uses the ``INIT``-block grammar).  Implicit ``CONFIG`` is
       *not* auto-prepended here: a stagnated run already has CONFIG
       in its history.

Each restart gets its own stagnation window — the windows do not bleed
into each other.

NOISE block
...........

``NOISE { ... } ENDNOISE`` controls how HPO handles objective noise.

.. code-block:: none

   NOISE
     AUTO
     CALIBRATION 0
   ENDNOISE

================== ============= =================================================================
Key                Args          Effect
================== ============= =================================================================
``AUTO``           bool          Auto-derive the finite-difference step ``PARAM fd_step``
                                 from the search-space geometry:
                                 ``fd_step = max(1e-3 × diag / sqrt(dim), 1e-6)``,
                                 where ``diag`` is the L2 norm of ``high − low`` in
                                 transformed space.  **DLIB:BFGS only**, and only when
                                 ``PARAM fd_step`` is not already set explicitly.  Bareword
                                 ``AUTO`` parses as ``true``.
``CALIBRATION``    int (0 or ≥2) Repeat the seed evaluation ``N`` times to surface
                                 the objective's noise floor and emit a suggested
                                 ``FTOL_REL``.  ``0`` disables; ``1`` is rejected as
                                 statistically meaningless.  The result is also fed into
                                 ``CLAMP_TO_NOISE_FLOOR`` for ``BASINS``.
================== ============= =================================================================

INNER block — NLopt MLSL / AUGLAG sub-optimiser
...............................................

NLopt's MLSL / G_MLSL_LDS / AUGLAG drivers delegate inner work to a
local optimiser.  Configure the inner via:

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB NLOPT
     ALGO G_MLSL_LDS
     MAXEVAL 400
     INNER
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 50
     ENDINNER
   ENDSTRATEGY

Only the driver-attribute keys (``LIB`` / ``ALGO`` / ``MAXEVAL`` /
``FTOL_*`` / ``STEP`` / ``SEED`` / ``PARAM`` / ``POPULATION`` /
``THREADS`` / ``VECTOR_ARR``) are valid inside ``INNER``.

.. _load_transforms:

Load-time data transforms
.........................

The transforms below reshape the training set *before* any neighbour
finding, descriptor calculation, or training.  They are shared across
``tadah train``, ``tadah predict``, and ``tadah hpo`` — for HPO they
are written either at the *root* level of the HPO config or inside
``--config`` (the training configuration).  Two of them (``LSCALE``,
``ESHIFT``) are stamped into ``pot.tadah`` so prediction round-trips
them automatically.

For per-key reference (defaults, examples, the exact validation
contract) see :ref:`ConfigSection` and ``tadah explain <key>`` (e.g.
``tadah explain train.lscale``).  For the conceptual layer — order of
application, derivation paths, MPI caveats, the worked Ta example —
see `HPO/LOAD_TRANSFORMS.md
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/LOAD_TRANSFORMS.md>`_.

============================ ===========================================================================
Key                          One-line summary
============================ ===========================================================================
``LSCALE``                   Uniform length rescale (DFT → experimental scale).  Persisted to pot.tadah.
``ESHIFT``                   Explicit per-element energy shift; one value per element ordered by Z.
``ESHIFT_ATOM``              Derive ESHIFT from isolated-atom configurations in the dataset.
``ESHIFT_DBATOM``            Derive ESHIFT by least-squares atomic-energy fit over the database.
``EFILTER  <lo> <hi>``       Drop configurations whose per-atom energy is outside ``[lo, hi]``.
``FFILTER  <thr>``           Drop configurations with any single force component above ``thr``.
``WDBFILE  <w_1> ... <w_n>`` Per-dataset weight multipliers; one per ``DBFILE``.
``WDBFILE_AUTO  <α>``        Automatic size balancing: per-config weight × ``1 / N_i^α``, ``α ∈ [0,1]``.
``EWEIGHT_TEMP  <T>``        Boltzmann reweighting at temperature ``T`` (Kelvin).
``ZERO_COM_FORCE``           Subtract the mean force per configuration.
============================ ===========================================================================

The training-set diagnostic emitted at host startup logs the lowest-
energy configuration after every transform has been applied — useful
for sanity-checking the reference state.

Validation-set sanitisation
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The validation set is transformed with a *stripped* context that
contains:

* ``LSCALE`` from the training (master) configuration,
* the **resolved** ``ESHIFT`` from the training run (so
  ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` derivations propagate as
  per-Z values; the raw ``*_ATOM`` / ``*_DBATOM`` keys are not
  re-derived on validation),
* ``EFILTER`` and ``FFILTER`` **only when they appear in the
  --validation file itself**.

Training-only knobs — ``EFILTER`` / ``FFILTER`` set in the master
config, ``WDBFILE`` / ``WDBFILE_AUTO``, ``EWEIGHT_TEMP``,
``ZERO_COM_FORCE`` — never reach the validation set: they would
silently change what the validation metric is computed on without
the user opting in, breaking apples-to-apples comparison across HPO
trials.

Per-validation outlier removal is therefore declared in the file you
pass to ``--validation`` directly:

.. code-block:: none

   # validation.in
   DBFILE   val_eos.tadahdb
   EFILTER  -12.0  -2.0       # honoured for validation only
   FFILTER   20.0

The host emits a single ``INFO`` log line at start-up summarising
the resulting validation set::

   Validation set: 532 configs loaded; dropped 4 by EFILTER, 1 by FFILTER; 527 remain.

The line is unconditional — when no filter is active the dropped
counts are zero and the line still confirms the loaded size, so it
is always clear which configs the validation metric is being
computed on.

MPI training caveat
~~~~~~~~~~~~~~~~~~~

``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` / ``EWEIGHT_TEMP`` /
``WDBFILE_AUTO`` need a global-view pass over the whole database that
is not yet wired through the MPI ``TrainerWorker``.  The engine fails
fast if any of these are set under MPI; either run without MPI or
provide explicit ``ESHIFT`` values.

.. _output_files:

Output files
............

Unless you change them, every artefact produced by the outer optimiser
is written to **the directory in which you started** ``tadah hpo``.

Main log files
~~~~~~~~~~~~~~

Each file begins with a header line that starts with ``#`` and
describes the columns.  One line per iteration.  The first column is
always the step number.

=====================  =====================================================================
File                   Columns
=====================  =====================================================================
``loss.tadah``         Wall-time, all individual loss terms, then the total loss.
``params.tadah``       The current hyperparameter vector in the order defined by ``OPTIM``.
``outvar.tadah``       Additional variables requested with ``--outvar``, plus all
                       ``--varloss`` variables.
=====================  =====================================================================

Cadence and formatting:

============ ========= =================================================================
Key          Default   Effect
============ ========= =================================================================
``OUTPUT N`` ``10``    Write a new line every ``N`` iterations.
``DIGITS d`` ``6``     Decimal digits in scientific-notation columns; ``1 ≤ d ≤ 15``.
``QFLUSH N`` ``10``    Number of rows buffered before the writer thread flushes to disk.
============ ========= =================================================================

Best-only snapshots
~~~~~~~~~~~~~~~~~~~

Whenever a new minimum of the global loss is found the corresponding
rows are copied to companion files:

* ``best_loss.tadah``
* ``best_params.tadah``
* ``best_outvar.tadah``

Each contains a single line — the best result so far — useful for
monitoring progress without parsing the full history.

.. list-table::
   :header-rows: 1
   :widths: 22 14 64

   * - Key
     - Default
     - Effect
   * - ``BOUTPUT N``
     - ``1``
     - Write to the ``best_*`` files every ``N`` iterations.

Potential archives
~~~~~~~~~~~~~~~~~~

The current best potential is always saved to ``best_pot.tadah``.

============================ ============== =================================================================
Key                          Default        Effect
============================ ============== =================================================================
``DUMP <N> <DIR>``           ``0`` (off)    Save **every** trial potential every ``N`` iterations to
                                            ``DIR`` as ``pot_<iter>.tadah``.  Directory is created if needed.
``BDUMP <N> <DIR>``          ``0`` (off)    As ``DUMP`` but only for new global bests.
============================ ============== =================================================================

Run log
~~~~~~~

``hpo_run.log`` records a single structured snapshot of the resolved
configuration at run start, including:

* ``Init strategy:`` block — the resolved init chain.
* ``Run policy:`` block — the active run policy.
* ``LOAD TRANSFORMS`` section — every applied load-time transform plus
  the equilibrium-volume diagnostic.

Either ``hpo_run.log`` or the stdout markers in
:ref:`diagnostic_markers` is the canonical record of which features
fired during a run.

Practical tips
~~~~~~~~~~~~~~

* Large optimisations can produce thousands of potentials; watch disk
  usage when ``DUMP`` is enabled.
* The log files are plain text and append-only.  They tail-friendly:

  .. code-block:: bash

     tail -f loss.tadah

* Each row in ``params.tadah`` matches the order of ``OPTIM`` directives
  exactly, so a row may be replayed verbatim with ``tadah train``.

.. _warmstart_protocol:

Warm-start protocol and bad-potential early-exit
................................................

Warm-start: ``${hot_a0}``
~~~~~~~~~~~~~~~~~~~~~~~~~

For LAMMPS scripts that start with a ``box/relax`` from a hard-coded
lattice guess (typical for elastic constants, surface energies, …), HPO
can short-circuit the expensive minimisation:

#. Replace the hard-coded ``lattice bcc 3.2`` with
   ``lattice bcc ${hot_a0}``.
#. Add ``variable a0 equal lx/<divisor>`` (whatever divisor matches
   the box size) right after the equilibrium minimisation.

HPO injects ``${hot_a0}`` as a string-style variable on every call
(default 3.2 Å on the first call), captures the script's ``${a0}``
after each successful run, and reuses the cached value on the next
evaluation.  When ``WARM`` reads a ``pot.tadah`` whose writer dumped
``# HOT_A0 <script> <a0>`` lines, the cache is primed automatically;
``[HPO] WARM: primed warmstart cache with N HOT_A0 entry(ies)``
confirms how many lines were absorbed.

Tuning environment variables:

================================== =================================================================
Variable                           Effect
================================== =================================================================
``TADAH_HPO_HOT_A0_DEFAULT``       Initial guess used when the cache is empty (default ``3.2``).
``TADAH_HPO_A0_MIN``               Physical lower bound (default ``1.0``).  A relaxed value below
                                   this is treated as unphysical: outputs are clamped to the
                                   script's ``--failscore`` and the cache is not updated.
``TADAH_HPO_A0_MAX``               Physical upper bound (default ``10.0``).  Same handling as
                                   ``A0_MIN``.  System-specific tightening (e.g. ``2.5`` /
                                   ``4.5`` for Ta) is an effective extra filter against runaway
                                   exploration.
================================== =================================================================

Bad-potential early-exit
~~~~~~~~~~~~~~~~~~~~~~~~

Two cheap pre/post checks let HPO skip the expensive LAMMPS stage when
training has produced an unphysical potential:

================================== =================================================================
Variable                           Effect
================================== =================================================================
``TADAH_HPO_BAD_POT_ERMSE_MEV``    Absolute validation-ERMSE cap in meV/atom (default ``200``).
                                   When the validation ERMSE exceeds this, LAMMPS is skipped
                                   and the eval reports ``FAIL_SCORE``.
``TADAH_HPO_BAD_POT_ERMSE_REL``    Multiplier vs the running-best ERMSE; eval is skipped when
                                   ``current > REL × best`` (default ``0`` = disabled).
================================== =================================================================

The post-LAMMPS ``a0``-range filter described above is the second
cheap check.

Profiling harness
~~~~~~~~~~~~~~~~~

Setting ``TADAH_HPO_BENCH=1`` writes ``hpo_phase_times.csv`` and
``hpo_script_times.csv`` next to the run, with per-phase wall times
(train / predict / lammps / sims_loss / submit) and per-script
create / run timing.  Useful with ``perf record`` or
``valgrind --tool=callgrind`` to verify where time is being spent.

.. _performance_parallelism:

Performance & parallelism
.........................

Tadah!MLIP comes in two flavours; the parallel strategy you can exploit
depends on which one you compiled.

Desktop build (OpenMP)
~~~~~~~~~~~~~~~~~~~~~~

* **Inner loop** — regression and descriptor evaluation are OpenMP
  parallel.  Set the number of threads in the usual way:

  .. code-block:: bash

     export OMP_NUM_THREADS=<n>

  Within a single iteration HPO drives ``min(N, n_scripts)``
  ``PC_LAMMPS`` instances concurrently — one per OpenMP thread, each
  on its own freshly-constructed ``LAMMPS_NS::LAMMPS`` instance.

* **Outer loop** — ``DLIB:GFS`` evaluates several hyperparameter sets
  concurrently when ``THREADS K`` is set in the ``STRATEGY`` block.
  Other optimisers ignore ``THREADS``.

  Rule of thumb::

     THREADS × OMP_NUM_THREADS  ≤  number of physical cores

  Exceeding this limit will not crash the run, but the OS will
  oversubscribe cores and overall performance will drop.

* **LAMMPS runs** — Tadah!MLIP always links a *serial* LAMMPS library;
  parallelism is driven *around* LAMMPS, not inside it.

MPI build
~~~~~~~~~

The MPI variant parallelises the *inner* regression across **all**
ranks (host–client pattern).  It must be linked against the MPI version
of LAMMPS.  Each LAMMPS calculation is spawned independently from the
main communicator:

``--ncpu <m>``
   On a ``PC_LAMMPS`` line, requests that *m* ranks form a mini-MPI
   world and execute the script.  Several ``PC_LAMMPS`` lines may run
   side by side, each with its own ``--ncpu`` value.  The sum of all
   requested ranks must not exceed *available ranks − 1* (one rank is
   reserved for the host).

.. note::

   The MPI launcher is functional but still experimental; improved
   error handling and dynamic load balancing are in development.

Example::

   srun -n 64 tadah hpo …             # 64 MPI ranks available
   ...
   PC_LAMMPS --script in.elastic --ncpu 8 …
                                       # spawns mpirun -n 8 lammps …

Practical advice
~~~~~~~~~~~~~~~~

* For inexpensive models you usually gain more by increasing
  ``THREADS`` than by adding OpenMP threads — context-switch overhead
  is lower.
* For very large training sets the regression dominates; in that case
  set ``THREADS = 1`` and devote the cores to OpenMP (desktop build) or
  MPI.
* Measure, do not guess: a few short test runs sweeping
  ``OMP_NUM_THREADS`` and ``THREADS`` over {1, 2, 4, …} will quickly
  reveal the sweet spot on your machine.

.. _diagnostic_markers:

Diagnostic stdout markers
.........................

HPO emits a one-line stdout (or stderr) marker each time a notable
event fires.  They are stable and intended to be grep-friendly.

============================================================================ ============================================================
Marker                                                                       Meaning
============================================================================ ============================================================
``[HPO] LIB:ALGO   dim=N``                                                    Active driver and search-space dimension.
``[HPO] InitStrategy NAME produced N starting points``                        After init resolution.
``[HPO] InitStrategy: WARM moved to chain front``                             Reorder happened.
``[HPO] InitStrategy: explicit chain set, implicit CONFIG suppressed.``       User wrote a chain; implicit CONFIG dropped.
``[HPO] PRIOR: N parameter(s) seeded from descriptor priors``                 ``PRIOR`` success.
``[HPO] PRIOR: N parameter(s) used bounds-midpoint fallback``                 Some descriptors had no prior.
``[HPO] WARM: loaded N parameter(s) from <file>``                             ``WARM`` read all keys.
``[HPO] WARM: primed warmstart cache with N HOT_A0 entry(ies)``               ``HOT_A0`` lines parsed from a ``WARM`` file.
``[HPO] WARM: clamped dim[d] from X to Y (bounds […])``                       Warm point fell outside narrowed bounds.
``[HPO] DLIB:GFS seeding with N starting points``                             Multi-seed forwarding.
``[HPO] MULTISTART: running K starts (parallel=N)``                           MULTISTART fires.
``[HPO] MULTISTART per-start MAXEVAL = N``                                    Either from ``BUDGET_PER_START`` or split.
``[HPO] MULTISTART start i/K — running inner optimiser``                      Per-start.
``[HPO] PRESTAGE: exploration phase LIB:ALGO (MAXEVAL=…)``                    PRESTAGE phase starts.
``[HPO] PRESTAGE: top-K losses: …``                                           Snapshot quality after PRESTAGE finishes.
``[HPO] REFINEMENT: refining top-K seed(s) via LIB:ALGO``                     Outer (refinement) phase starts.
``[HPO] RESTART i/M — fresh init from STRATEGY``                              Stagnation watchdog fired.
``[HPO] NOISE_AUTO: PARAM fd_step set to V (diag=…, dim=…)``                  ``NOISE.AUTO`` fired and shows the value used.
``[HPO] NOISE_CALIBRATION: running N evals at the seed point``                ``NOISE.CALIBRATION`` started.
``[HPO] BASINS: resumed from checkpoint (n_eval=…, best_loss=…)``             Resume-from-checkpoint fired.
============================================================================ ============================================================

Migration from legacy syntax
............................

The pre-block grammar (``MULTISTART <int>``, ``STAGE_GLOBAL true``,
``LOCAL ... ENDLOCAL`` for MLSL, ``INIT_STRATEGY <method>``,
``NOISE_AUTO``, …) has been removed.  The parser explicitly rejects each
of these keywords with a one-line "renamed to X" message.  The most
common renames:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Old keyword
     - New form
   * - ``MULTISTART N``
     - ``STRATEGY { TYPE MULTISTART N_STARTS N }``
   * - ``MULTISTART_PARALLEL N``
     - ``STRATEGY { TYPE MULTISTART PARALLEL N }``
   * - ``MULTISTART_BUDGET_PER_START``
     - ``STRATEGY { TYPE MULTISTART BUDGET_PER_START N }``
   * - ``STAGE_GLOBAL true``
     - ``STRATEGY { TYPE PRESTAGE EXPLORE { … } ENDEXPLORE … }``
   * - ``LOCAL ... ENDLOCAL``
     - ``INNER ... ENDINNER`` (NLopt MLSL/AUGLAG inner sub-optimiser)
   * - ``GLOBAL_LIB`` / ``GLOBAL_ALGO``
     - ``STRATEGY { TYPE BASINS EXPLORE { LIB ... ALGO ... } }``
   * - ``LOCAL_LIB`` / ``LOCAL_ALGO``
     - ``STRATEGY { TYPE BASINS REFINE { LIB ... ALGO ... } }``
   * - ``TOL_GLO_LADDER`` / ``TOL_LOC_LADDER``
     - ``EXPLORE { FTOL_LADDER ... }`` / ``REFINE { FTOL_LADDER ... }``
   * - ``STAGE_*``
     - ``EXPLORE { BUDGET_* }`` / ``REFINE { BUDGET_* }``
   * - ``RESTART_AFTER_STAGNATION N``
     - ``RESTART { AFTER_STAGNATION N }`` (inside ``STRATEGY``)
   * - ``RESTART_MAX N``
     - ``RESTART { MAX N }``
   * - ``RESTART_INIT_STRATEGY``
     - ``RESTART { STRATEGY <method> ... }``
   * - ``INIT_STRATEGY <method>``
     - ``INIT { STRATEGY <method> ... } ENDINIT``
   * - ``INIT_K``
     - ``INIT { K_TOTAL ... }``
   * - ``INIT_SEED``
     - ``INIT { SEED ... }``
   * - ``INIT_INCLUDE_CONFIG``
     - ``INIT { INCLUDE_CONFIG ... }``
   * - ``NOISE_AUTO``
     - ``NOISE { AUTO true|false } ENDNOISE``
   * - ``NOISE_CALIBRATION N``
     - ``NOISE { CALIBRATION N } ENDNOISE``

The rejection messages always quote the new spelling, so a stale
config will fail loud and informatively rather than silently.

See also
--------

* :ref:`hpo_examples` — fifteen worked examples covering every block
  in the new grammar.
* :ref:`cli_example_2` — minimal single-loss-term walkthrough.
* `HPO/README.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_ —
  full keyword reference, defaults, and the *OPTIM-able context keys*
  reinit-safety table.
* `HPO/INIT_STRATEGIES.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/INIT_STRATEGIES.md>`_
  — tutorial for ``INIT`` / ``STRATEGY`` / ``RESTART`` / ``NOISE`` with
  worked scenarios.
* `HPO/LOAD_TRANSFORMS.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/LOAD_TRANSFORMS.md>`_
  — load-time dataset transforms in depth.
