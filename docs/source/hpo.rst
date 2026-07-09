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
   proposes new :term:`hyperparameters<Hyperparameter>`, the model is
   retrained, and each trial potential is validated against
   :term:`performance constraints<Performance constraint>` that feed back
   into the global loss.

Nested fitting is Tadah!MLIP’s automated, two-level fitting workflow:

* **inner loop** — ordinary regression that determines the
  :term:`learned parameters<Learned Parameter>` :math:`\mathbf w` for a
  fixed model definition;
* **outer loop** — a global optimiser that samples from the
  :term:`search space constraints<Search Space Constraint>` (SSC),
  retrains the model, and judges the result against user-defined
  :term:`performance constraints<Performance constraint>` (PC).

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

Nested fitting needs up to three input files:

#. A :term:`training configuration` — the same configuration file you
   would use for ``tadah train``.  See :ref:`ConfigSection` for the
   full key list.
#. A *validation* file — a list of validation datasets passed via the
   ``DBFILE`` keyword.  *Optional*: only required when the loss uses
   validation-set metrics (``PC_ERMSE``, ``PC_FRMSE``, …); a run whose
   loss comes entirely from ``PC_OBSERVABLE`` / ``PC_LAMMPS`` lines can
   omit ``--validation``.
#. An :term:`HPO configuration` — the file documented on this page.
   Conventionally named ``config.hpo``.

The training configuration's ``DBFILE`` is itself *optional* when every
model weight is pinned via ``FIXINDEX``/``FIXWEIGHT`` (see
:ref:`hpo_fixweight`) — with no free weights there is nothing for the
least-squares stage to fit.  In that mode the element set must be
declared explicitly with the ``ATOM`` key, at least one loss source must
remain, and the run is serial-only (MPI builds reject it).

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
:ref:`hpo_examples` for a graded series of fifteen worked examples.

.. _nfp_manual:

Configuration file
------------------

This section is the reference for the HPO configuration file.  For an
even terser keyword reference — every flag, every default, plus the
*OPTIM-able context keys* table — see the in-tree `HPO/README.md
<https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_.

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
       EXPLORE  ... ENDEXPLORE       # required for TYPE PRESTAGE
       REFINE   ... ENDREFINE        # required for TYPE PRESTAGE
       INNER    ... ENDINNER         # optional: NLopt MLSL/AUGLAG inner
       RESTART  ... ENDRESTART       # optional decorator
   ENDOPTIMIZER

Block keywords are case-sensitive.  Comments start with ``#``; long
lines may be continued with a trailing ``\``.

A legacy form without a ``STRATEGY`` block — ``LIB``/``ALGO``/``MAXEVAL``
written directly inside ``OPTIMIZER`` — is still accepted and behaves
exactly like ``STRATEGY { TYPE PLAIN ... }``.  It is fine for short
single-shot runs but cannot express ``MULTISTART``, ``PRESTAGE``,
init chains, or ``RESTART`` decorators; those features
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

Two weight-related special cases:

* ``OPTIM FIXWEIGHT`` — the ``(indices)`` name **model weights** (the
  weights ``FIXINDEX`` pins), *not* positions in the ``FIXWEIGHT`` value
  list.  ``OPTIM FIXWEIGHT (5) -1 1`` searches model weight 5 and it is
  an error if 5 is not listed in ``FIXINDEX``.  See :ref:`hpo_fixweight`.
* ``OPTIM WEIGHTS`` — **rejected**.  The linear ``WEIGHTS`` are re-fitted
  by least squares on every evaluation, so optimising them directly
  would be a silent no-op; pin them with ``FIXINDEX``/``FIXWEIGHT``
  instead.

.. _hpo_knot_rcut:

Cutoffs of knot-family refit potentials
.......................................

For potentials written by ``tadah refit`` with the knot basis families
(``D2_Knot5`` two-body, ``DM_REAM`` knot density), the effective cutoff
**is the largest knot position**: each knot function :math:`(t-r)_+^k`
vanishes smoothly at its own knot, so ``RCUT2B``/``RCUTMB`` are *derived*
quantities (see :ref:`refit_rcut_contract`).  ``tadah hpo`` enforces this
automatically:

* at startup, a stated ``RCUT`` that differs from the largest knot is
  corrected (with a warning) before anything is built;
* on **every evaluation**, the ``RCUT`` keys are re-derived from the
  current (possibly ``OPTIM``'d) knot positions, so searching
  ``OPTIM CGRID2B`` / ``CGRIDMB`` moves the effective cutoff with the
  knots;
* the neighbour list is sized **once** for the largest cutoff the search
  can reach — including the upper bounds of any ``OPTIM`` block on the
  knot grids — so no evaluation can silently outrun it.

Consequently ``OPTIM RCUT2B`` / ``OPTIM RCUTMB`` on a knot-family
potential is a *wasted search dimension* (the value is overridden each
evaluation; a startup warning says so).  To search the effective cutoff,
put an ``OPTIM`` block on the knot positions instead — e.g. uncomment the
``OPTIM CGRID2B`` line of the outermost knot in the refit-generated
hpotarget and widen its upper bound.

Blip-family potentials are unaffected: their hard truncation at the
stated ``RCUT`` is part of the fitted model, so the key remains
authoritative (warnings fire if ``OPTIM`` bounds let a blip's support
cross it).

.. _hpo_fixweight:

Pinning and searching linear coefficients
.........................................

By default the model's linear ``WEIGHTS`` are re-fitted by least squares
inside **every** objective evaluation, while ``OPTIM`` searches the
nonlinear hyperparameters.  ``FIXINDEX``/``FIXWEIGHT`` in the *training
configuration* carve weights out of that LS fit: pinned columns are
removed from the design matrix, their contribution is subtracted from
the target, and the remaining weights are fitted against the residual.
The i-th ``FIXWEIGHT`` value pins the weight named by the i-th
``FIXINDEX`` entry; a lone key is an error, and ``NORM true`` is
incompatible with pinning (the values would live in normalized feature
space).

**Mixed mode** — LS-fit most weights, hold one, search another.  With a
5-weight model: weights 1–3 LS-fitted, weight 4 held at 0.75, weight 5
searched by the optimiser:

.. code-block:: none

   # training configuration (config.train)
   DBFILE    train.tadahdb
   FIXINDEX  4 5
   FIXWEIGHT 0.75 1.20        # 0.75 pins w4; 1.20 seeds w5

   # config.hpo
   OPTIM FIXWEIGHT (5) -2.0 2.0    # (5) = model weight 5

**All-fixed polish** — the ``tadah refit`` workload.  ``FIXINDEX`` covers
every weight, so the LS solve is vacuous and skipped (including the
per-evaluation design-matrix build); the optimiser searches the
coefficients directly:

.. code-block:: none

   FIXINDEX  1-28
   FIXWEIGHT <28 fitted values>
   # config.hpo: one OPTIM FIXWEIGHT (i) line per coefficient
   # (this is exactly what `tadah refit` emits as refit.hpotarget)

**No training data** — with every weight pinned, the training ``DBFILE``
may be omitted entirely.  Declare the element set with ``ATOM`` and keep
at least one loss source (``--validation`` metrics, ``PC_OBSERVABLE``,
or ``PC_LAMMPS``):

.. code-block:: none

   # config.train: a fitted potential, no DBFILE line
   ATOM      Ta
   FIXINDEX  1-28
   FIXWEIGHT <28 values>

   # config.hpo
   PC_LAMMPS --script Ecoh.in --hotvar hot_a0 2.5 4.5 3.302 --varloss Ecoh 8.10 800
   OPTIM FIXWEIGHT (1-28) ...

   # command line: --validation may be omitted
   tadah hpo -c config.train --hpotarget config.hpo

This mode is serial-only (MPI builds reject it — there is no design
matrix to distribute).  Misconfigurations (lone keys, size mismatches,
out-of-range indices, missing loss sources, incomplete pin coverage
without a ``DBFILE``) are rejected at startup with actionable messages.

With ``FIXINDEX`` present, analytical/HYBRID gradient strategies fall
back to finite differences for all parameters: the analytical chain
assumes the unconstrained LS solution map, which pinning invalidates.
Derivative-free algorithms (e.g. ``NLOPT LN_BOBYQA``) are a natural
choice for coefficient searches.

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

``--hotvar <name> <min> <max> [<initial>]``
   Declare this ``PC_LAMMPS`` line as *hot*: a two-phase evaluator
   for parameters that other scripts depend on. Hot scripts run
   alone in **phase A** of each iteration (in parallel with other
   hot scripts), expose ``variable <name> string …`` from the
   LAMMPS script (see :ref:`warmstart_protocol` for the required
   ``string``-style readback), and the returned value is validated
   against ``[min, max]``. Validated values are cached per
   ``(script, name)`` and propagated as ``${<name>}`` strings into
   every cold (no-``--hotvar``) ``PC_LAMMPS`` script in **phase B**.
   ``<initial>`` is optional; when omitted the midpoint of
   ``[<min>, <max>]`` seeds the first iteration. If a hot value
   lands outside ``[min, max]`` HPO fail-scores the iteration and
   skips phase B.

Per-metric bad-pot pre-filter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``PC_<METRIC> --skip-above <cap>`` and ``PC_<METRIC> --skip-rel <ratio>``
attach a per-metric pre-filter on the dataset error counters
(``PC_EMAE``, ``PC_ERMSE``, ``PC_ErRMSE``, ``PC_ERSQ``, ``PC_FRMSE``,
``PC_SRMSE``).  Both gates fire **before** any ``PC_LAMMPS`` work for
the iteration:

* ``--skip-above <cap>`` fail-scores the iteration when the metric
  exceeds the absolute ``<cap>``.
* ``--skip-rel <ratio>`` fail-scores when
  ``metric > ratio × running_best`` for that same metric.

Both filters can be applied to the same metric line.  They short-cut
obviously bad potentials so wall-time is not spent on LAMMPS phases
that would never compete.

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
     - Optimisation library: ``DLIB``, ``NLOPT``, ``TADAH``, ``CERES``.
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

.. list-table:: Algorithms by library
   :header-rows: 1
   :widths: 12 88

   * - LIB
     - ALGO (algorithms from the selected library)
   * - ``TADAH``
     - ``RANDOM`` (uniform random search), ``GRID``, ``ANNEAL`` (classic
       Kirkpatrick/Metropolis Simulated Annealing — see
       :ref:`tadah_anneal_block` below).
   * - ``NLOPT``
     - All gradient-free NLOpt globals (``GN_*``) and locals (``LN_*``)
       — see the `NLOpt algorithm catalogue
       <https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/>`_ for
       the full list. Gradient-using algorithms (``GD_*`` / ``LD_*``)
       route through the shared finite-difference gradient seam used by
       ``DLIB:BFGS`` and the Ceres drivers; ``LD_LBFGS`` is the only
       member exercised by the bundled examples.

       .. note::

          **Analytical loss gradients are work in progress** and are
          not enabled in this beta release. Every gradient-using
          algorithm (``LD_*``, ``GD_*``, ``DLIB:BFGS``, all CERES
          drivers) currently obtains its gradient through numerical
          finite differences (see ``PARAM fd_step`` below).
   * - ``DLIB``
     - ``BFGS``, ``LBFGS``, ``CG``, ``BOBYQA`` (local), ``GFS`` (the
       dlib MaxLIPO+TR global *Global Function Search*).
   * - ``CERES``
     - ``LBFGS`` (limited-memory BFGS, Wolfe line search), ``WOLFE``
       (full BFGS, Wolfe line search), ``NLCG`` (nonlinear conjugate
       gradient), ``STEEPEST_DESCENT`` (Armijo line search). All four
       use Ceres' ``GradientProblemSolver``; the gradient is computed
       numerically via Ceres' ``DynamicNumericDiffCostFunction`` (central
       differences; step size from ``PARAM fd_step``).  The Ceres NLLS
       family (``LEVENBERG_MARQUARDT``, ``DOGLEG``, ``SUBSPACE_DOGLEG``)
       is **deferred** — mathematically degenerate over a scalar HPO
       loss; it will be enabled in a later phase together with
       residual-mode loss exposure. Ceres optimisers do **not** enforce
       box constraints; use ``DLIB:BFGS`` or ``NLOPT:LD_LBFGS`` for
       bounds-enforced gradient optimisation.

CERES PARAM keys
~~~~~~~~~~~~~~~~

The four CERES algorithms accept these algorithm-specific ``PARAM``
entries (all keys are optional unless noted):

================== =================================================================================
Key                Effect
================== =================================================================================
``fd_step``        Relative finite-difference step for the numerical gradient (default ``1e-6``).
``diff_method``    Numeric-diff method: ``0`` = CENTRAL (default), ``1`` = FORWARD, ``2`` = RIDDERS.
                   RIDDERS extrapolates assuming a smooth Taylor expansion; not recommended near
                   ``FAIL_SCORE`` plateaus or for noisy LAMMPS-loss components.
``tol_gradinf``    Gradient-infinity-norm convergence tolerance (Ceres ``gradient_tolerance``,
                   default ``1e-10``).
``max_iter``       Override on Ceres' ``max_num_iterations``.  By default the iteration cap is set
                   to a large sentinel and ``MAXEVAL`` is enforced externally on host evaluations.
``max_ls_iter``    Maximum line-search step-size iterations.
``max_lbfgs_rank`` (LBFGS only) limited-memory rank, default ``20``.
``nlcg_type``      (NLCG only) variant: ``0`` = FLETCHER_REEVES (default), ``1`` = POLAK_RIBIERE,
                   ``2`` = HESTENES_STIEFEL.
================== =================================================================================

The ``MAXEVAL`` cap is enforced at the host-evaluation level by an
external counter shared with the ``GradientProvider``.  Ceres'
internal iteration counting is intentionally given a high sentinel so
that one Ceres iteration's worth of finite-diff samples (2 × N under
CENTRAL) is correctly accounted for against the user budget.

The ``DLIB:BFGS`` path also routes its gradient through the same seam.
``PARAM fd_step`` keeps its existing semantics; the chief observable
change is that gradient evaluations now count toward ``MAXEVAL`` via
the same shared counter (previously dlib's internal central-diff
samples bypassed the cap by ``2 × N`` per iteration).

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
     - single ``CONFIG`` point when no ``STRATEGY`` line is given
     - Append a strategy entry to the chain (see table above).
   * - ``K_TOTAL``
     - uint
     - sum of strategies' default K
     - Cap on total starting points produced.
   * - ``SEED``
     - uint
     - random
     - Seed for stochastic strategies (RANDOM / LHS / SOBOL).
   * - ``WARM_ON_MISMATCH``
     - ``warn`` / ``error`` / ``skip``
     - ``warn``
     - What to do when ``WARM`` cannot fill every ``OPTIM`` key.

There is no ``INCLUDE_CONFIG`` keyword.  The in-config point is part of
the chain only when ``STRATEGY CONFIG`` is listed explicitly; an
``INIT`` block with no ``STRATEGY`` line (or no ``INIT`` block at all)
resolves to a single ``CONFIG`` point.  The removed ``INCLUDE_CONFIG``
and ``INIT_INCLUDE_CONFIG`` keywords are rejected with a message
pointing at ``STRATEGY CONFIG``.

Two implementation details affect reproducibility:

* The chain is resolved in the exact order it is written — no entry is
  reordered.  To make the warm point iter 0, list ``STRATEGY WARM``
  first in the ``INIT`` block.
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
``TADAH:ANNEAL``                 First only — used as the SA chain's starting state after T0 calibration.
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
point). Local optimisers — ``NLOPT:LN_*`` (gradient-free) and
``NLOPT:LD_*`` (gradient via the shared finite-difference seam) — and
``DLIB:GFS`` / ``DLIB:BFGS`` all accept multistart.

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

.. _tadah_anneal_block:

LIB TADAH ALGO ANNEAL — classic Simulated Annealing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The in-tree ``TADAH`` library exposes a classic Kirkpatrick/Metropolis
Simulated Annealing driver as ``ALGO ANNEAL``.  It is a single-shot
optimiser: select it from a ``STRATEGY { TYPE PLAIN }`` block, or wrap
it in ``MULTISTART`` for multiple independent chains.

References:

1. Kirkpatrick, S., Gelatt, C. D., and Vecchi, M. P.  *Optimization by
   simulated annealing.*  Science 220 (4598), 671–680, 1983.
2. Metropolis, N., Rosenbluth, A., Rosenbluth, M., Teller, A., and
   Teller, E.  *Equation of State Calculations by Fast Computing
   Machines.*  J. Chem. Phys. 21, 1087–1092, 1953.

The algorithm operates in transformed (post-``LOG_HP``) parameter
space: every Metropolis trial draws ``x' = x + sigma * N(0, I)``,
rejects out-of-bounds proposals, and applies the Metropolis
acceptance ``Δf <= 0`` ``or`` ``rand < exp(-Δf / T)``.  Temperature
cools geometrically: ``T <- T * RT``.  When ``PARAM T_INIT`` is not
set, the driver auto-calibrates ``T0`` from a small uniform-bounds
random walk: ``T0 = -mean|Δf| / ln(P)``, where ``P =
PARAM T0_PACCEPT`` is the target early acceptance probability.

Knobs are passed via the existing ``PARAM <key> <value>`` grammar
inside the ``STRATEGY`` block.  Defaults are listed below.

.. list-table::
   :header-rows: 1
   :widths: 20 14 66

   * - PARAM
     - Default
     - Effect
   * - ``T_INIT``
     - auto
     - Initial temperature.  When set, T0 calibration is skipped (no
       probes are spent).
   * - ``T_MIN``
     - ``1e-12``
     - Cooling floor; the loop stops when ``T < T_MIN``.
   * - ``RT``
     - ``0.85``
     - Geometric cooling factor in ``(0, 1)``.
   * - ``L``
     - ``50``
     - Markov-chain length per temperature.
   * - ``STEP_FRAC``
     - ``0.10``
     - Per-dim sigma as a fraction of ``(high_i - low_i)``.
   * - ``T0_PROBES``
     - ``20``
     - Random uniform-bounds probes used to calibrate T0.
   * - ``T0_PACCEPT``
     - ``0.80``
     - Target acceptance probability for T0.

Top-level ``STEP <s>`` overrides ``PARAM STEP_FRAC`` with a uniform
sigma applied to every dimension; the entry banner echoes which is in
use.  ``SEED <int>`` seeds the RNG.

Stops honoured: ``MAXEVAL`` (hard cap on host calls — **including** T0
probes), ``MAXTIME``, ``STOPVAL``, and the ANNEAL-specific ``T_MIN``
floor.  ``FTOL_REL`` and ``FTOL_ABS`` are **not** honoured by this
driver (no NEPS history yet).

When the T0 calibration probes all return the same value
(``mean|dE| == 0`` — typically saturated ``FAILSCORE`` everywhere),
HPO falls back to ``T0 = 1.0`` and emits a ``WARNING`` line.  Set
``PARAM T_INIT`` or widen the bounds in that case.

Corana adaptive variant (opt-in)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Setting ``PARAM CORANA 1`` switches the inner loop from classic
Kirkpatrick/Metropolis to the Corana adaptive scheme (refs:
Corana, Marchesi, Martini, Ridella, *ACM Trans. Math. Soft.* 13:262,
1987; Brommer, Univ. Stuttgart MSc, 2003).  The setup phase is
identical (T0 calibration, seed evaluation, stop conditions); only
the per-rung structure differs:

* Perturbations are **one dim at a time** (uniform
  ``U(-1, 1) · V[i]``), not whole-vector Gaussians.
* The per-dim step vector ``V[i]`` (initialised from
  ``STEP_FRAC · (high_i - low_i)``) is **adapted** every ``NS`` step
  cycles based on the per-dim accept ratio: it grows when the rate
  exceeds 0.6 and shrinks when it drops below 0.4.
* T cools by ``RT`` after every ``NT`` step-adjust cycles, so a rung
  consumes ``NT · NS · D`` host evaluations (``L`` is ignored).

Use the Corana variant when the classic variant freezes at low T —
fixed σ proposes broad-search jumps that the cold Metropolis gate
rejects en masse, while ``V[i]`` shrinks naturally as the chain
settles into a basin.

.. list-table::
   :header-rows: 1
   :widths: 18 14 68

   * - PARAM
     - Default
     - Effect
   * - ``CORANA``
     - ``0``
     - Set to ``1`` to enable the adaptive variant.
   * - ``NS``
     - ``5``
     - Step cycles per step-adjust cycle.
   * - ``NT``
     - ``2``
     - Step-adjust cycles per T (cooling triggers every ``NT``).
   * - ``C``
     - ``2.0``
     - Step-adjust gain — controls how fast ``V[i]`` reacts to
       accept-rate deviation from the ``[0.4, 0.6]`` deadband.
   * - ``V_FLOOR_FRAC``
     - ``1e-3``
     - Lower clamp on ``V[i]`` as a fraction of
       ``(high_i - low_i)``.  Vanilla Corana has no explicit floor;
       on ``FAILSCORE``-cliff landscapes ``V[i]`` can collapse by
       factor ``(1 + C)^NT`` per zero-accept rung, killing the chain.
       Default ``1e-3`` keeps ``V[i]`` ≥ 0.1% of box width so the
       chain stays mobile.
   * - ``RESET_TO_BEST``
     - ``0``
     - Brommer (2003) modification: set to ``1`` to snap the chain
       back to the best-so-far point after every cooling step.  Gives
       the adaptive chain "basin memory" — high-T wanders no longer
       cost progress because the next rung restarts from the deepest
       known basin.  Strongly recommended on ``FAILSCORE``-cliff
       landscapes or when the chain wanders during the high-T phase.

The per-rung journal line gains a ``V=[V_min, V_max]`` field so
users can watch the step vector adapt in real time.  The Corana
paper recommends ``NS = 20``, ``NT = max(100, 5D)``, but those
defaults assume cheap function evaluations — for LAMMPS-driven HPO
with ``~600 ms`` per evaluation, the shipped defaults (``NS = 5``,
``NT = 2``) cover a useful T schedule within practical wall budgets.

.. _tadah_anneal_journal:

Journal output
^^^^^^^^^^^^^^

Per run, ``ALGO ANNEAL`` writes the following lines to ``hpo_run.log``
(timestamps elided):

.. code-block:: text

   ANNEAL: seed=1234  L=20  RT=0.85  T_MIN=1e-12  STEP_FRAC=0.15
   ANNEAL: T0=8.188e+00 from 20 probes,  mean|dE|=1.83e+00  loss range=[3.20e-02, 1.40e+01]  std=2.10e+00  target p_accept=8.000e-01
   ANNEAL rung k=0  T=8.188e+00  proposed=20  accepted=14 (70.0%)  oob=2  (9.1%)  f_x=2.31e-01  best=1.84e-01
   ANNEAL rung k=1  T=6.960e+00  proposed=20  accepted=11 (55.0%)  oob=0  (0.0%)  f_x=1.97e-01  best=1.84e-01
   ...
   ANNEAL summary: rungs=12  proposed=240  accepted=98 (40.8%)  oob=4 (1.6%)  T0=8.188e+00  T_final=2.46e-01  best=8.71e-02
   ANNEAL accept-rate trajectory: min=20.0%  max=70.0%
   ANNEAL stopped: MAXEVAL exhausted after 260 host eval(s) (12 rung(s) completed)

Use these to diagnose typical SA mistuning:

* **Accept rate stays at ≈ 100% across all rungs** — ``T_INIT`` /
  ``T0_PACCEPT`` too high; the chain is doing a random walk.  Lower
  ``PARAM T0_PACCEPT`` (e.g. to ``0.5``) or pick a smaller ``T_INIT``.
* **Accept rate collapses in 2–3 rungs** — ``RT`` too aggressive.  Use
  ``RT = 0.90`` or ``0.95``.
* **High ``oob`` fraction (> 30 %)** — ``STEP_FRAC`` (or ``STEP``) too
  large for the bounds; halve it.
* **Best stagnates while accept rate is > 50 %** — chain is escaping
  basins but not converging; raise ``L`` to give each temperature
  more proposals.
* **``ANNEAL stopped: MAXTIME exceeded`` after only a few rungs** —
  the wall-clock cap killed SA before it could cool.  Either raise
  ``MAXTIME`` (in the ``OPTIMIZER`` block or at root level), reduce
  ``L``, or switch to ``LIB NLOPT ALGO LN_BOBYQA`` for a faster local
  refinement.

The standard ``BEST`` events from ``HPO_Host`` (printed by every
optimiser) interleave with the rung lines, so the best-tracking story
is identical to other drivers.

.. _restart_decorator:

RESTART decorator
.................

``RESTART { ... } ENDRESTART`` decorates a ``TYPE PLAIN`` /
``MULTISTART`` / ``PRESTAGE`` strategy with a stagnation watchdog.

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
                                 statistically meaningless.
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

.. _hpo_load_transforms:

Load-time data transforms
.........................

The training-side load transforms (``LSCALE``, ``ESHIFT*``,
``EFILTER`` / ``FFILTER``, ``EWEIGHT_TEMP``, ``WDBFILE`` /
``WDBFILE_AUTO``, ``ZERO_COM_FORCE``) are documented under
:ref:`load_transforms`; place them in ``--config`` (the training
configuration). HPO applies them on every inner training run.

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

Warm-start: ``${hot_a0}`` (or any ``--hotvar``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For LAMMPS scripts that start with a ``box/relax`` from a hard-coded
lattice guess (typical for elastic constants, surface energies, …), HPO
can short-circuit the expensive minimisation:

#. Declare the producing ``PC_LAMMPS`` line as hot:

   .. code-block:: none

      PC_LAMMPS --script in.a0_relax \
                --hotvar hot_a0 2.5 4.5 3.302 \
                --varloss ...

   ``<min> <max>`` bound the physical range; ``<initial>`` is optional
   (midpoint of ``[min, max]`` when omitted).

#. Replace the hard-coded ``lattice bcc 3.2`` in that script with
   ``lattice bcc ${hot_a0}``.

#. End the script with a ``string``-style readback (**not** ``equal``):

   .. code-block:: none

      variable hot_a0 string $(lx/<divisor>)

   where ``<divisor>`` matches the conventional cell size (e.g.
   ``4.0`` for a 4-atom BCC conventional cell). Equal-style readback
   does not currently round-trip across iterations inside the cached
   LAMMPS instance — use string-style until that is lifted.

HPO injects ``${hot_a0}`` as a string-style variable on every call
(``<initial>`` from ``--hotvar`` on the first call, the cached value
thereafter), captures the script's ``${hot_a0}`` after each successful
run, and reuses it on the next evaluation. The variable is then
propagated as ``${hot_a0}`` into every cold (no-``--hotvar``)
``PC_LAMMPS`` script in phase B. When ``WARM`` reads a ``pot.tadah``
whose writer dumped ``# HOT_A0 <script> <a0>`` lines, the cache is
primed automatically; ``[HPO] WARM: primed warmstart cache with N
HOT_A0 entry(ies)`` confirms how many lines were absorbed.

Multiple ``--hotvar`` flags may be repeated on the same ``PC_LAMMPS``
line; ``hot_a0`` is just a conventional name (any identifier works).
Per-line legacy environment variables (``TADAH_HPO_HOT_A0_DEFAULT``,
``TADAH_HPO_A0_MIN``, ``TADAH_HPO_A0_MAX``) were removed in this
release; the per-line ``--hotvar`` flag replaces all three.

Bad-potential early-exit
~~~~~~~~~~~~~~~~~~~~~~~~

Two cheap checks let HPO skip the expensive LAMMPS stage when training
has produced an unphysical potential:

* the per-metric ``PC_<METRIC> --skip-above`` / ``--skip-rel`` filters
  documented at :ref:`performance_constraints` (Per-metric bad-pot
  pre-filter), and
* the post-LAMMPS ``--hotvar`` range filter described above (a relaxed
  value outside ``[min, max]`` fail-scores the iteration and does not
  update the cache).

The legacy ``TADAH_HPO_BAD_POT_ERMSE_MEV`` / ``_REL`` environment
variables were removed in this release; use
``PC_ERMSE … --skip-above <cap>`` / ``--skip-rel <ratio>`` instead.

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
============================================================================ ============================================================

See also
--------

* :ref:`hpo_examples` — fifteen worked examples covering the optimiser
  libraries and the ``INIT`` / ``STRATEGY`` / ``EXPLORE`` / ``RESTART`` /
  ``INNER`` blocks.
* :ref:`cli_example_2` — minimal single-loss-term walkthrough.
* `HPO/README.md
  <https://git.ecdf.ed.ac.uk/tadah/hpo/-/blob/develop/README.md>`_ —
  full keyword reference, defaults, and the *OPTIM-able context keys*
  reinit-safety table.
