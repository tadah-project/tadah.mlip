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
:ref:`hpo_examples` for a graded series of worked examples.

.. _nfp_manual:

Configuration file
------------------

This section is the reference for the HPO configuration file.  The
annealing-family key tables live on the :doc:`optimizer` pages
(generated from the validation registry); ``tadah hpo`` prints the same
information per algorithm.

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

A short form without a ``STRATEGY`` block — ``LIB``/``ALGO``/``MAXEVAL``
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

* ``<KEY>`` — any optimisable key from the training configuration.
  Whether a key can be re-initialised safely is validated at startup:
  an unsafe combination (e.g. ``RCUT2B`` with ``D2_EAM``) is a hard
  error naming the offending key.
* ``(indices)`` — selects a subset of the values bound to ``<KEY>``.
  Indices follow the order in the training configuration and start
  from **1**.  Comma-separated lists, ranges ``a-b``, and strides
  ``a-b:s`` are all accepted (e.g. ``(1,4,7-10:2)``).  The same index
  is the one every label the run prints carries -- ``KEY[n]`` in
  ``hpo_run.log``, ``params.tadah``, ``hpo_step_scales.tsv`` and the
  search-space sidecars (single-value keys carry no index).
* ``<low>``, ``<high>`` — floating-point bounds.  ``high`` must be
  strictly greater than ``low``.
* an optional shape tag ``LIN`` (default), ``LOG`` (requires
  ``low > 0``; the optimiser works in ``ln x``) or ``ASINH <c>``
  (``asinh(x/c)``: linear with slope ``1/c`` near zero, logarithmic
  beyond ``|x| ~ c``; the tool for sign-crossing ranges that span
  several decades).  See :ref:`search_space_scaling`.  Any other trailing
  token is an error (stray tokens used to be silently ignored).

``OPTIM`` lines may be repeated for the same key with different indices.

Seed-relative boxes.  When the right range is unknown, let the CONFIG
seed size the box:

.. code-block:: none

   OPTIM <KEY> (indices) AUTO [RATIO <f> | FRAC <p>] [FLOOR <a>] [GROUP_FLOOR <r>] [LIN | LOG | ASINH <c>]

* ``RATIO f`` — geometric box ``[x0/f, x0*f]`` (mirrored for a negative
  seed); the default for positive-only keys (``SGRID2B``, ``SGRIDMB``,
  ``SEMBFUNC``; ``f = 2``).  A zero seed is an error under ``RATIO``.
* ``FRAC p`` — linear box ``x0 ± max(p·|x0|, FLOOR, GROUP_FLOOR·max|x0|)``
  where ``max|x0|`` runs over the indices of that ``OPTIM`` line; the
  default otherwise (``p = 0.5``).  ``FLOOR``/``GROUP_FLOOR`` keep a
  zero or tiny seed searchable (``FIXWEIGHT`` coefficients that vanish
  at long range).  ``FRAC 0`` with ``FLOOR a`` gives every index the
  same absolute half-width ``a`` -- the natural box for grid centres
  (``CGRID*``, ``CEMBFUNC``), whose unit is the grid spacing rather
  than the value.
* cutoff keys (``RCUT*``) cannot be ``AUTO`` — the neighbour list is
  sized from an explicit high bound.
* under ``RHOFORM NONNEG``/``MONOTONE`` the ``AMPGRIDMB`` boxes are clamped
  at 0 and marked ``AUTO FRAC>=0`` (see :ref:`hpo_rhoform_positive`).
  Under ``MONOTONE`` an amplitude is a step height: a change to an outer
  amplitude shifts the density by the same amount (times :math:`W f_c`)
  all the way in to the core (see :ref:`density form <dm_ream_rhoform>`),
  so size those boxes more tightly than under ``NONNEG``, where an amplitude
  changes only its own bump.

The resolved boxes are written to ``hpo_optim_resolved.hpo`` (a
paste-ready explicit ``OPTIM`` block) and every run writes
``hpo_search_space.tsv`` (one row per dimension: label, real bounds,
origin, seed, scale, optimiser-side bounds and the search-space version;
a comment header explains every column, and the columns are space-padded
to equal width and tab-separated, so the file reads aligned in an editor
and still parses as TSV once whitespace is stripped).  For data-driven
ranges of descriptor shape keys use ``tadah analysis suggest_bounds``.

Bound pressure.  The run never changes ``OPTIM`` bounds by itself; it
tells you when the running best rides one: every milestone ``SUMMARY``
lists the dimensions whose best sits within 2 % of a limit (in internal
units, so a ``LOG`` dimension is judged in ``ln``), with the value, the
bound, the gap as a fraction of the width and the evaluation since which
the mark has persisted, and the ``DIAGNOSTICS`` section repeats it for
the final best.  A best that keeps riding a bound suggests widening that
side (where the physics permits) and rerunning with ``INIT METHOD WARM``.
The exception is a lower bound the potential itself imposes: an
``AMPGRIDMB`` amplitude at 0 under ``RHOFORM NONNEG``/``MONOTONE`` is an
admissible optimum, marked ``LO*`` and given no such advice (see
:ref:`hpo_rhoform_positive`).

Special cases:

* ``OPTIM FIXWEIGHT`` — the ``(indices)`` name **model weights** (the
  weights ``FIXINDEX`` pins), *not* positions in the ``FIXWEIGHT`` value
  list.  ``OPTIM FIXWEIGHT (5) -1 1`` searches model weight 5 and it is
  an error if 5 is not listed in ``FIXINDEX``.  See :ref:`hpo_fixweight`.
* ``OPTIM WEIGHTS`` — **rejected**.  The linear ``WEIGHTS`` are re-fitted
  by least squares on every evaluation, so optimising them directly
  would be a silent no-op; pin them with ``FIXINDEX``/``FIXWEIGHT``
  instead.
* ``OPTIM EMBF0`` — **rejected** at startup.  ``EMBF0`` (``ZERO``/``FREE``)
  selects the :ref:`DM_REAM embedding reference <dm_ream>`, a discrete
  property of the potential; set it in the ``-c`` potential/configuration.
* ``OPTIM`` on structure data — ``RHOFORM``, ``TYPE2B``, ``TYPEMB``,
  ``RCTYPE2B``, ``RCTYPEMB``, ``KDEG2B``, ``KDEGMB``, ``KDEGEMB`` (any
  case) — is **rejected** at startup: these are discrete properties of the
  potential, not continuous parameters.

.. _hpo_embf0:

DM_REAM potentials and the embedding reference (EMBF0)
......................................................

Under the default ``EMBF0 ZERO`` the DM_REAM embedding is zero-referenced,
:math:`F(\rho) = \sum_m w_m [g_m(\rho) - g_m(0)]`, so the isolated-atom
energy is 0 by construction for **every candidate** — no penalty term is
needed to hold :math:`F(0)` in place.  :math:`g_m(0)` is recomputed for each
candidate from the ``CEMBFUNC``/``SEMBFUNC`` values it evaluates, so the
constraint also holds when only the embedding shape is searched; the search
space, ``hpo_search_space.tsv``, ``hpo_optim_resolved.hpo`` and
``params.tadah`` are unchanged (no dimension is derived or removed), and
``FIXWEIGHT``/``WEIGHTS``/``FIXINDEX`` keep their meaning.  Every potential
the run writes (``best_pot.tadah``, ``BDUMP``/``DUMP`` archives, the
potential handed to ``PC_LAMMPS``) carries the ``EMBF0`` line.

``hpo_run.log`` states the reference in the POTENTIAL SETUP section
(``Embedding F(0)``, and the raw :math:`F(0)=\sum_m w_m g_m(0)` of the
``-c`` potential: from its ``FIXWEIGHT`` when every weight is pinned, else
from its stored ``WEIGHTS``, else "determined per evaluation"), and the
RESULT section prints the raw :math:`F(0)` of the first and of the best
evaluation (under ``ZERO``: the per-atom constant the zero reference
removes).  Both values are reported for models linear in the descriptor
(``BF_Linear``, ``Kern_Linear``) only, and are not checkpointed: after a
resume they read "n/a" unless the resumed session records them.  A
``WARM`` seed whose ``EMBF0`` differs from the run's triggers a WARNING:
WARM takes only the ``OPTIM`` values from the seed and evaluates them
unchanged in the run's reference (no conversion), so the seed's recorded
loss need not be reproduced.  A HINT is printed when a DM_REAM model under
``ZERO`` is trained on data without ``BIAS`` or ``ESHIFT`` (see
:ref:`hpo_load_transforms`); prefer ``ESHIFT*``, because ``BIAS true``
inserts intercept columns before the descriptor weights (``FIXINDEX``/
``FIXWEIGHT`` indices shift) and disables ``EAMFAST``.  Under ``ZERO`` a combination of
embedding coefficients that only shifts :math:`F` by a constant (available
when the bumps nearly sum to a constant) no longer changes any energy, so the
coefficients may drift along it; the raw :math:`F(0)` of the stored
``WEIGHTS`` can then grow, which matters only to tools that evaluate the
weights without honouring ``EMBF0``.

.. _hpo_rhoform:

DM_REAM potentials and the density form (RHOFORM)
.................................................

The ``-c`` configuration of a DM_REAM model must declare its
:ref:`density form <dm_ream_rhoform>` (``RHOFORM SIGNED | NONNEG |
MONOTONE``; startup stops without it), and every potential the run writes
carries it. Under ``SIGNED`` nothing changes for the search. Under
``NONNEG`` and ``MONOTONE`` every candidate must remain a valid density: the
``AMPGRIDMB`` boxes are non-negative and every evaluated candidate is
projected onto them (see :ref:`hpo_rhoform_positive`). Whatever the form,
two gates can keep the density physical during the search, ``RHOGATE`` (the
sign of a ``SIGNED`` density) and ``RHOBARGATE`` (a lattice density that
falls under expansion), and ``hpo_run.log`` reports the density of the
``-c`` potential and of the best (see :ref:`hpo_feasibility_gates`).
``WARM`` seeds must declare the run's form when density keys are searched
(see the same section).

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
  replaced by it before anything is built (a WARNING when the gap exceeds
  ``1e-9`` relative or the channel's cutoff is smooth, INFO otherwise --
  see :ref:`refit_rcut_contract` for why LAMMPS can then differ);
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
authoritative; under a hard cutoff ``BUMPGATE`` keeps every candidate's
bumps inside it (see :ref:`hpo_feasibility_gates`).

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
   hot scripts), redefine ``variable <name> equal …`` at the end of
   the LAMMPS script (see :ref:`warmstart_protocol` for the
   ``equal``-style readback), and the returned value is validated
   against ``[min, max]``. Validated values are cached per
   ``(script, name)`` and propagated as ``${<name>}`` into
   every cold (no-``--hotvar``) ``PC_LAMMPS`` script in **phase B**.
   ``<initial>`` is optional; when omitted the midpoint of
   ``[<min>, <max>]`` seeds the first iteration. If a hot value
   lands outside ``[min, max]`` HPO fail-scores the iteration and
   skips phase B (a *graded* failure when ``FAILSCORE_GRADING ON``
   — see :ref:`failscore_grading`).

``--failvar <name> [<scale>]``
   Declare a *violation variable*: a nonnegative LAMMPS ``equal``-style
   variable that is ``0`` when the simulation vouches for itself and
   ``> 0`` when it failed, with the magnitude measuring *how badly*
   (natural units; ``<scale>`` converts to the dimensionless failure
   measure, default 1). When it reads ``> 0`` the sim is treated as
   failed, its outvars are excluded from the fit, and — with
   ``FAILSCORE_GRADING ON`` — ``scale × value`` grades the failure
   loss so the optimiser is pulled toward feasibility instead of
   hitting a constant. Define it **early** in the script (right after
   the risky command), so it is still readable when a later section
   times out. Like ``--varloss``/``--outvar`` names, the variable must
   be defined in the script as an ``equal``-style variable — a typo'd
   name is a startup error, not a silently-disabled check. Replaces
   the old “sentinel value” idiom (returning ``-1000`` as fake data).
   See :ref:`failscore_grading`.

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
all shared-pool workers (and isolated worker processes) use the same
working directory.

If ``-log <file>`` or ``-screen <file>`` is supplied explicitly inside
a ``PC_LAMMPS`` entry, HPO appends ``.t<worker_slot>`` per shared-pool
slot (and the MPI rank suffix when applicable), so for example
``-log run.log`` becomes ``run.log.t0``, ``run.log.t1``, … on one rank.
``-log none`` and ``-screen none`` are left untouched.

.. _failscore_grading:

Failure handling — graded FAILSCORE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

What problem this solves
^^^^^^^^^^^^^^^^^^^^^^^^

When a candidate potential makes a simulation fail — a relaxation that
does not converge, a hot value outside its window, a hung run that had
to be killed — the iteration traditionally returns the constant
``FAILSCORE``.  A constant is a *flat* landscape: inside a failing
region every proposal looks equally bad, so annealers random-walk or
freeze (their step-size adaptation has two absorbing states on a flat
or cliff-edged landscape), parallel-tempering ladders die on the
plateau, and gradient methods see exactly zero slope.  The optimiser
receives no hint of *which way is out*.

``FAILSCORE_GRADING ON`` replaces the constant with a **graded** value:
each gradable failure returns a loss inside the reserved band
``[0.999 × FAILSCORE, FAILSCORE)`` that *decreases as the candidate
gets closer to working*.  Feasible results always win (they are orders
of magnitude below the band).  The annealing family ranks finite values
inside the band in its Metropolis decisions and chain/walker personal-best
updates, so their ordering supplies a downhill path toward feasibility.
Operations that require a genuinely feasible sample remain stricter:
``STOPVAL``, the Corana ``FTOL`` plateau, T0 calibration, and PA population
weighting/resampling classify the entire reserved band as failed.  Thus a
failure value cannot report convergence; PA gives a graded walker zero
resampling weight whenever a feasible walker exists, while an all-failed
population skips resampling and continues cooling.

The knobs, most important first
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``FAILSCORE <value>`` (root)
   The failure ceiling, as before — but with grading ON it **must** be a
   finite positive number below ``1e300`` (e.g. ``1e9``), large enough
   that all four grading bands are strictly representable; the plain
   ``DBL_MAX`` default and unrepresentable tiny values are rejected at
   startup.  Choose it a few orders of magnitude above your worst
   plausible *real* loss.

``FAILSCORE_GRADING ON|OFF`` (root, default ``OFF``)
   The master switch.  ``OFF`` reproduces the ungraded
   behaviour.  Everything below only matters when this is ``ON``.
   Environment override: ``TADAH_FAILSCORE_GRADING=ON|OFF``.

``--failvar <name> [<scale>]`` (per ``PC_LAMMPS`` line)
   The per-script violation channel (see the option reference above and
   the examples below).  Costs one ``variable`` line per script and
   upgrades that script's failures from “counted” to “measured”.

``PC_FAILPROBE --script <f> --outvar <S> [--gate <T>] [--s0 <v0>]`` (root)
   A tiny, minimiser-free *sanity probe* executed at the front of every
   iteration's parallel batch.  Its first ``--outvar`` is a single
   nonnegative scalar ``S`` measuring how unphysical the candidate is
   (0 = sane).  ``S`` becomes the failure measure for the failure modes
   that cannot measure themselves — hung runs that were killed, crashed
   potential builds — and, optionally, a **gate**: when ``S > <T>`` the
   iteration is short-circuited before wasting the full simulation
   budget.  ``<v0>`` must be finite and strictly positive; it sets the
   saturation scale of the grading curve for ``S`` (default 1; a good
   choice is the seed potential's ``S`` times a small factor).  At most
   one probe line is allowed; ``--varloss``
   on it is rejected (the probe grades failures — it must not shape the
   feasible objective).

``--gate <T>`` (on ``PC_FAILPROBE``)
   **The only knob that needs real care.**  If the gate can fire on a
   candidate that would actually have succeeded, you have changed the
   objective, not just saved time.  Calibrate it from a short run:
   journal the ``S`` statistics of accepted iterations, then set ``T``
   far above anything a feasible candidate produces (same philosophy as
   the ``--skip-above`` caps).  Gate firings are journaled and
   rate-limited; watch the counter.

``PC_WORKER_TIMEOUT`` / per-sim ``--maxtime`` (existing)
   Unchanged, but they *interact*: a killed hung run is graded by ``S``
   (band C below), so with a probe defined even timeouts become
   directional instead of flat.

Order of firing within one iteration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   feasibility gates (BUMPGATE, RHOGATE, RHOBARGATE)  →  [any fails?  → graded band D, stop]
   train/validate  →  [skip caps?  → graded band D, stop]
   dispatch batch:  FAILPROBE first (cheapest → returns first),
                    hot scripts + independent cold scripts alongside
     ├─ probe returns S
     │    ├─ S ≤ gate (or no gate): S is stored for later grading
     │    └─ S > gate: iteration short-circuited → band D graded by S;
     │       queued sims are cancelled; in PROCESS mode this
     │       iteration's in-flight sims are killed (other concurrent
     │       iterations on the shared pool are never touched)
     ├─ each sim finishes → failvars read (0 ⇒ OK; >0 ⇒ band B, graded)
     │    hot values validated → out of window ⇒ band A, graded by the
     │    normalized distance to [min, max]
     ├─ a sim times out in-script (timer timeout) → instance still
     │    alive → failvars still readable → band B
     └─ a sim hangs → parent deadline kill (PROCESS mode) → nothing
          readable → band C, graded by S
   dependent cold wave → loss assembly:
     no failure  → normal loss (identical to grading OFF)
     any failure → L_fail in [0.999·FS, FS), see the band table

The severity bands (worst on top) encode the natural *repair path* —
make the potential sane, then runs complete, then outputs valid, then
windows satisfied, then feasible:

.. list-table::
   :header-rows: 1
   :widths: 8 46 46

   * - Band
     - Failure modes
     - Graded by
   * - D
     - probe gate fired; potential build crashed; skip caps; a
       :ref:`feasibility gate <hpo_feasibility_gates>` rejected the
       candidate before training
     - ``S`` (probe) / metric excess (caps) / the gate's violation
       measure ``v``
   * - C
     - run hung and was killed; worker died
     - ``S`` (probe)
   * - B
     - run completed but failed: in-script timeout, ``--failvar > 0``,
       unreadable/NaN outputs
     - failvar sum + failed fraction
   * - A
     - hot value outside its window
     - normalized distance to the window

Cross-band ordering is fixed (every ``A`` value beats every ``B``
value, etc.), within a band less violation is strictly better, and one
iteration gets exactly one band (the worst one that occurred).
Failures that are *not the potential's fault* — a LAMMPS instance that
could not be created, worker crash-loops — are deliberately **not**
graded (a directionless constant), so the optimiser never chases
infrastructure noise.

Designing ``failprobe.in``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Four rules make a good probe:

1. **It must not be able to fail or hang.**  Single-point evaluations
   only: fixed cells, ``run 0`` — no ``minimize``, no ``box/relax``,
   no dynamics.
2. **It must be smooth in the potential parameters.**  Build ``S`` from
   hinge/softplus penalties of energies, forces and stresses — these
   are smooth functions of the parameters, so ``S`` gives a descent
   direction.
3. **It must be cheap** — a few small single-point cells cost
   milliseconds and disappear into the parallel batch.
4. **No ``label``/``jump`` loops.**  Scripts are executed
   command-by-command through the LAMMPS library interface, where the
   file-rewind machinery behind ``jump`` does not work reliably (it
   can half-work in ways that depend on leftover loop variables from
   the *previous* evaluation in the cached instance).  Unroll the few
   cells you probe — it is also easier to read.

Good ingredients (all already proven in the ``pen_*`` observables):
equation-of-state sanity (energy at a handful of fixed volumes around
equilibrium: hinge on non-monotonic compression branches), near-zero
forces on the ideal lattice, finite and positive stress response.  A
ready-to-adapt example:

.. code-block:: bash

   # failprobe.in — minimiser-free sanity scalar S for bcc metal.
   # Contract: NEVER minimize/box-relax/run>0; define S even on the
   # weirdest potential; keep it O(ms); no label/jump loops (rule 4) —
   # the three volumes are unrolled. `variable X equal $( ... )`
   # captures the value IMMEDIATELY, and equal-style redefinition
   # replaces the old value, so the captures stay fresh in the cached
   # instance on every evaluation.

   # -- E(V) monotonicity: 2-atom bcc cells at 0.90/1.00/1.10 a0 -----
   # (a0 = 3.30 reference lattice constant, baked into the lattice
   #  commands below)
   clear                    # scripts run in a CACHED LAMMPS instance:
                            # always start with clear (variables survive)
   units metal
   dimension 3
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc $(3.30*0.90)
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_m10 equal $(pe/atoms)     # captured now (immediate $)

   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc 3.30
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_0 equal $(pe/atoms)

   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc $(3.30*1.10)
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_p10 equal $(pe/atoms)

   # compressed branch must rise toward small volume; expanded branch
   # must rise toward large volume (bound state). Hinge each violation.
   variable h_comp equal $(ternary(v_e_m10<v_e_0, v_e_0-v_e_m10, 0.0))
   variable h_exp  equal $(ternary(v_e_p10<v_e_0, v_e_0-v_e_p10, 0.0))

   # -- force sanity: rattle a cell DETERMINISTICALLY (fixed seed) and
   # hinge on absurd forces. Do NOT probe fmax on the ideal lattice --
   # there it is zero by symmetry for ANY potential, sick or sane.
   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc 3.30
   region box block 0 2 0 2 0 2
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   displace_atoms all random 0.05 0.05 0.05 12345 units box
   run 0 post no
   variable h_frc equal $(ternary(fmax>20.0, fmax-20.0, 0.0))

   # -- compose S: bounded, nonnegative, 0 for a sane potential ------
   variable S equal $(v_h_comp + v_h_exp + v_h_frc)

with the configuration line

.. code-block:: none

   FAILSCORE_GRADING ON
   PC_FAILPROBE --script pc_lammps/failprobe.in --outvar S --s0 0.5
   # add "--gate 50" ONLY after calibrating S statistics on your seed

(Adapt the element, ``a0``, volumes and thresholds to your system; add
Born-stability hinges via finite-difference stress probes on fixed
strained cells if shear stability matters for your fits — the
``pen_born`` machinery in the examples shows the pattern.)

``--failvar`` recipes for typical scripts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Pressure-target relaxation** (``box/relax`` at pressure ``P``): the
convergence check already computes the violation — export it:

.. code-block:: bash

   fix relax all box/relax iso ${P_bar} vmax 0.003
   minimize 1e-10 1e-10 2000 20000
   unfix relax
   variable Pres equal $(press)*1.0e-4
   # 0 when converged within 1 GPa; the residual (GPa) otherwise:
   variable fail_press equal $(ternary(abs(v_Pres-v_P_GPa)<1.0, 0.0, abs(v_Pres-v_P_GPa)))
   # ... only now compute the real observables ...

.. code-block:: none

   PC_LAMMPS --script pc_lammps/elastic_P_single.in --invar P 300 \
       --varloss Cp_P300 232.8 7.6 --varloss C44_P300 332.9 3.9 \
       --failvar fail_press 0.01

**Minimisation that must actually converge** (vacancy, surface,
stacking-fault energies): grade by the residual force instead of
returning a sentinel:

.. code-block:: bash

   minimize 1e-10 1e-10 2000 20000
   # 0 when converged; the residual max force (eV/Å) otherwise:
   variable fail_fmax equal $(ternary(fmax<1.0e-8, 0.0, fmax))

.. code-block:: none

   PC_LAMMPS --script pc_lammps/vac_bcc_P_fast.in ... --failvar fail_fmax 10

Both patterns follow the same contract: **define the failvar
immediately after the risky command** (it survives a later in-script
timeout), keep it ``0`` on success, and make its magnitude the natural
physical distance from success.  Where your scripts currently return
sentinel values (``-1000``) as fake observables, move that logic into a
``--failvar`` — the sentinel stops polluting the fitted region and its
information starts steering the optimiser instead.

Setup effort, honestly
^^^^^^^^^^^^^^^^^^^^^^

Level 0 (just ``FAILSCORE_GRADING ON``): hot-window rejections and
partial failures become graded automatically — no script changes, and
the worst optimiser pathologies (plateau death, step-size collapse)
disappear.  Level 1 (one ``--failvar`` line per script that has a
convergence check): minutes per script.  Level 2 (the probe): one
script, once, assembled from the same hinge idioms as the ``pen_*``
observables.  The only knob that can *change your objective* if set
carelessly is the probe ``--gate`` — leave it off until you have
calibrated ``S``.

.. _hpo_feasibility_gates:

Feasibility gates
~~~~~~~~~~~~~~~~~

Three root-level keys reject a candidate **before any training work** when
its model is invalid in a way the parameters alone decide:

.. list-table::
   :header-rows: 1
   :widths: 34 16 50

   * - Key
     - Default
     - Rejects a candidate whose
   * - ``BUMPGATE CANDIDATE|STRICT``
     - ``CANDIDATE`` (always on)
     - compact bump reaches past a hard cutoff, :math:`c + w/\eta > r_c`
   * - ``RHOGATE <tol> ...``
     - off
     - DM_REAM pair density :math:`\rho(r)` dips below
       :math:`-\mathrm{tol}\,\rho_{max}` (``RHOFORM SIGNED`` only)
   * - ``RHOBARGATE <lattice> <a0> <f_lo> <f_hi> ...``
     - off
     - lattice density :math:`\bar\rho(a)` is not strictly decreasing on
       :math:`a \in [f_{lo} a_0, f_{hi} a_0]`

**What a reject is.** The candidate gets a :ref:`band-D failure
<failscore_grading>` without being trained, predicted or run through
LAMMPS. With ``FAILSCORE_GRADING ON`` the loss is graded by the gate's
violation measure :math:`v \ge 0`,

.. math::

   L = 0.999\,\mathrm{FS} + 0.001\,\mathrm{FS}\,\Big(0.75 + 0.25\,\frac{v}{1+v}\Big),

so :math:`v = 0` sits at the band-D floor :math:`0.99975\,\mathrm{FS}`,
a smaller violation is always a lower loss, and a *saturated* measure
(:math:`v` = ``DBL_MAX``, for a density gate whose density is not finite,
has no positive value, or has a pair weight :math:`\le 0`; see below) sits
at the top of band D. With
grading ``OFF`` the loss is the flat ``FAILSCORE``. A reject is otherwise a
complete evaluation: it takes an evaluation number and a ``MAXEVAL`` slot
(at almost no wall time, so budget ``MAXEVAL`` for them or stop on
``MAXTIME``), its rows are written, it counts toward stagnation, and it can
never become the best; its ``DUMP`` archive is header-only (no potential
was built). The verdicts are pure functions of the candidate and the
settings (the same at any ``THREADS``), and a gate never changes the
candidate, so a candidate that passes evaluates bit for bit as without the
gate.

**Journal.** The first five rejects of each gate, then every power of two,
are ``INFO`` lines ``eval N: <gate> reject (...); total rejects=K``, where
``K`` counts that gate's rejects. The ``SUMMARY`` failure census gains
``bumpgate=N``, ``rhogate=N`` and ``rhobargate=N`` (each when it is not 0)
after the band counts, and ``DIAGNOSTICS`` breaks the band-D count down::

   SUMMARY ... failed=72 (72.0%; A=0 B=0 C=0 D=72) rhogate=49 rhobargate=23

     Graded failures         : 86  (71.7%)
       A=0 (hot-var out of range)  B=0 (script failed)  C=0 (did not complete)  D=86 (pot build / gated)
       of which rho-sign gate rejects: 57 (rho_min < -tol*rho_max on the RHOGATE window, rejected before training)
       of which lattice-rhobar gate rejects: 29 (rhobar(a) not strictly decreasing, rejected before training)

(``of which bump-support gate rejects: N (c + w/eta past RCUT, rejected
before training)`` for ``BUMPGATE``.)

BUMPGATE — bump support inside a hard cutoff
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   BUMPGATE CANDIDATE | STRICT        # default CANDIDATE

On a channel whose basis is a compact bump family (``D2_Blip`` or
``D2_Blip5`` pair channels, the ``Blip``/``Blip5`` density of ``DM_REAM``)
and whose cutoff is hard (``Cut_Dummy``, also when ``RCTYPE*`` is absent),
every bump must end at or before the cutoff: :math:`c + w/\eta \le r_c`,
with :math:`c` the ``CGRID*`` centre, :math:`\eta` the ``SGRID*`` width,
:math:`w = 2` (Blip) or 3 (Blip5) and :math:`r_c` the ``RCUT*`` value (to a
relative tolerance of ``1e-6``). A bump that overhangs is cut off at
:math:`r_c`: an energy and force step whenever a neighbour crosses the
cutoff. Under a smooth cutoff the overhang is damped to zero and there is no
gate.

* **Startup, both modes.** The ``-c`` potential's own bumps must fit its
  stated cutoff: a centre beyond it, or a support past it, is an error that
  says how to fix it (the ``SGRID*`` value that fits, moving the centre with
  ``tadah refit --edit``, or a larger ``RCUT*``).
* **CANDIDATE** (default). An ``OPTIM`` box is refused only when it holds no
  feasible point at all (even its most favourable corner, lowest centre and
  largest :math:`\eta`, reaches past the largest cutoff a candidate can
  have). A box that is only partly feasible is accepted at full width, and
  an ``INFO`` message per bump (``tadah`` log, verbosity 2) says that the
  gate is armed for it. Every armed bump is then checked on every candidate,
  against the candidate's own sampled cutoff when ``RCUT*`` is ``OPTIM``'d,
  with the measure

  .. math::

     v = \sum_{\text{violating bumps}} \frac{(c + w/\eta - r_c)\,\eta}{w},

  the summed overhang in units of the half-width, which grows with every
  violating coordinate. The reject line reads ``eval N: bump-support gate
  reject (c + w/eta past the cutoff, violation=<v>, graded band D); total
  rejects=K``. A configuration whose boxes cannot violate arms nothing and
  evaluates exactly as without the gate.
* **STRICT** (the behaviour of 1.4.0, before ``CANDIDATE`` became the
  default). The worst corner of every box
  (``CGRID*`` high bound, ``SGRID*`` low bound, the lowest ``OPTIM RCUT*``
  bound) must fit, or startup stops naming the bound to change; no candidate
  is rejected at run time. Use it for strict ``MAXEVAL`` budgeting or to
  reproduce an older run.

The environment variable ``TADAH_BUMPGATE=CANDIDATE|STRICT`` overrides the
key. ``OPTIM RCUT*`` on a bump channel is usually a wasted dimension (a
feasible bump vanishes inside the cutoff, so the sampled value only moves
the feasible region; startup warns).

*Example.* A ``D2_Blip5`` pair channel with ``RCTYPE2B Cut_Dummy`` and
``RCUT2B 5.5`` whose fourth bump sits at ``CGRID2B`` 4.4, ``SGRID2B`` 3
(reach :math:`4.4 + 3/3 = 5.4`), searched with

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM CGRID2B (4) 4.0 5.0
   OPTIM SGRID2B (4) 2.0 4.0

The most favourable corner (4.0, 4.0) reaches 4.75 and the worst (5.0, 2.0)
reaches 6.5, so ``CANDIDATE`` accepts the box and arms the gate for bump 4::

   bump-support gate (CANDIDATE): armed for CGRID2B bump 4 — candidates with c + 3/eta > RCUT2B = 5.5
   are rejected before training as graded band-D failures (worst box corner reaches 6.5). BUMPGATE
   STRICT restores the startup box-corner error.

A candidate at :math:`c = 4.8`, :math:`\eta = 2.5` reaches 6.0: it is
rejected with :math:`v = 0.5 \cdot 2.5/3 = 0.416667` (``eval N:
bump-support gate reject (c + w/eta past the cutoff, violation=0.416667,
graded band D); total rejects=K``) and a loss of about ``9.99824e8``, while
candidates at (4.4, 3.0) or (4.2, 2.5) reach 5.4 and are trained as usual.
``STRICT`` refuses this box at startup: its worst corner needs an ``SGRID2B
(4)`` low bound of at least :math:`w/(r_c - c_{hi}) = 3/(5.5 - 5.0) = 6`.

RHOGATE — the sign of a SIGNED density
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   RHOGATE <tol> [R_LO <r>] [NPTS <n>] [SEED ERROR|WARN]

======== =========================== ==========================================================
Token    Default                     Rule
======== =========================== ==========================================================
``tol``  — (required)                finite, ``>= 0``
``R_LO`` ``0.15`` × the candidate's  finite, ``>= 0``, below the smallest cutoff a candidate can
         ``RCUTMB``                  have (startup error otherwise)
``NPTS`` ``20001``                   integer in ``[2, 10000000]``
``SEED`` ``ERROR``                   ``ERROR`` or ``WARN``: what a failing seed does (below)
======== =========================== ==========================================================

Keywords and their values are case-insensitive; a second ``RHOGATE`` line, an
unknown or repeated keyword, a keyword without a value and a number that is
not one finite token are errors. There is no environment override.
``RHOGATE`` needs a bare DM_REAM many-body term (``INITMB true``, ``TYPEMB
DM_REAM``; ``DM_mJoin`` is refused) with ``RHOFORM SIGNED``: ``NONNEG`` and
``MONOTONE`` guarantee :math:`\rho \ge 0` by construction, and startup says
so.

The gate scans :math:`\rho_{raw}(r) = f_c(r)\sum_n a_n\psi_n(r)`, the pair
density without the pair weight, as DM_REAM trains with it, on ``NPTS``
equally spaced points of :math:`[r_{lo}, r_{hi}]`: :math:`r_{hi}` is the
candidate's ``RCUTMB`` (for a knot density its largest knot) and
:math:`r_{lo}` is ``R_LO`` or :math:`0.15\,r_{hi}`. The pair weights
:math:`W` scale every pair alike, so the verdict does not depend on them,
but each must be finite and :math:`> 0`. The candidate **fails** when

.. math::

   \rho_{min} < -\mathrm{tol}\cdot\rho_{max}

(``tol 0`` rejects any negative value; a small ``tol`` tolerates a small
relative dip), with the measure

.. math::

   v = \min\!\big(10^8,\ v_{peak} + v_{int}\big),\qquad
   v_{peak} = \max\!\Big(0, \frac{-\rho_{min} - \mathrm{tol}\,\rho_{max}}{\rho_{max}}\Big),\qquad
   v_{int} = \sum_i \frac{\max(0, -\rho_i - \mathrm{tol}\,\rho_{max})\,w_i}{\rho_{max}\,(r_{hi} - r_{lo})}

(:math:`w_i` trapezoid weights): the depth of the worst dip plus the mean
depth over the window, both relative to :math:`\rho_{max}`. The measure is
saturated when :math:`\rho_{max} \le 0` on the window, a value is not
finite, a pair weight is :math:`\le 0` or not finite, or the window is
empty. A candidate whose density DM_REAM would refuse (for instance a zero
``SGRIDMB`` entry) gets no verdict: it fails in training exactly as it would
without the gate. The reject line is ``eval N: rho-sign gate reject
(rho_min/rho_max = <rel> at r = <r> A, tol <tol>, v = <v>); total
rejects=K``, or ``(<reason>, v = saturated)``. The scan costs about 0.7 ms
per candidate at 20001 points with 20 Blip5 bumps.

*Example.* A ``SIGNED`` DM_REAM density of three Blip5 bumps (``TYPEMB``
basis code 3) and a search over the sign of the outermost amplitude:

.. code-block:: none

   # -c potential (excerpt)
   CGRIDMB    2.8 3.6 4.4
   SGRIDMB    3.0 3.0 3.0
   AMPGRIDMB  0.5 0.3 0.1
   RHOFORM    SIGNED
   RCTYPEMB   Cut_Dummy
   RCUTMB     5.5

   # config.hpo
   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM AMPGRIDMB (3) -0.5 0.5
   RHOGATE 1e-5

At startup the journal states the gate and the seeds' verdicts::

   INFO  RHOGATE: rho(r) sign gate, tol 1.0000e-05, window [0.15 RCUTMB, RCUTMB] of each candidate,
         20001-point grid, SEED ERROR; armed per candidate on AMPGRIDMB[3]: candidates with
         rho_min < -tol*rho_max are rejected before training as graded band-D failures;
         seeds: -c pot (INIT CONFIG) PASS

(one line in the file). The seed passes (:math:`\rho \ge 0`, ``rel
0.0000e+00``). A candidate with ``AMPGRIDMB(3) = -0.2`` has a dip to
:math:`-0.19966` at :math:`r = 4.4025` Å against :math:`\rho_{max} =
0.50036`, on the window :math:`[0.825, 5.5]`::

   INFO  eval 7: rho-sign gate reject (rho_min/rho_max = -3.9903e-01 at r = 4.4025 A, tol 1.0000e-05,
         v = 4.4515e-01); total rejects=1

and its loss is about ``9.99827e8``. Had the ``-c`` potential itself carried
``AMPGRIDMB 0.5 0.3 -0.2``, startup would stop (``SEED ERROR``)::

   RHOGATE: seed -c pot (INIT CONFIG) fails the rho(r) sign gate: rho_min -1.9966e-01 at r 4.4025 A,
   rho_max 5.0036e-01, rel -3.9903e-01 < -tol = -1.0000e-05 on [0.8250, 5.5000] A (20001 points),
   v = 4.4515e-01. Repair the seed's density (for a WARM seed, its OPTIM'd density values: the
   rest comes from the -c potential) with tadah refit --edit, relax the gate, or add SEED WARN to
   evaluate it as a rejected start.

RHOBARGATE — the lattice density decreases under expansion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   RHOBARGATE <bcc|fcc|hcp> <a0> <f_lo> <f_hi> [NPTS <n>] [SEED ERROR|WARN]

The lattice is case-insensitive (``hcp`` takes the ideal
:math:`c/a = \sqrt{8/3}`, with ``a0`` its in-plane constant); ``a0`` is
finite and :math:`> 0`; :math:`0 < f_{lo} < f_{hi}`, both finite; ``NPTS``
is an integer in ``[3, 10000000]``, default ``1001``; ``SEED`` and the
grammar rules are those of ``RHOGATE``. It works under **every**
``RHOFORM`` and needs a bare DM_REAM many-body term.

For every lattice constant :math:`a_k = a_0\,(f_{lo} + k\,(f_{hi} -
f_{lo})/(\mathrm{NPTS}-1))` (the last point exactly :math:`a_0 f_{hi}`) the
gate sums the density an atom of the perfect monatomic crystal receives,

.. math::

   \bar\rho(a_k) = \sum_s m_s\,\rho_{raw}(d_s\,a_k),

over the neighbour shells :math:`s` (distance :math:`d_s a`, multiplicity
:math:`m_s`) inside the candidate's ``RCUTMB``. The candidate **fails**
when :math:`\bar\rho` does not decrease strictly on the whole grid, i.e.
when :math:`\bar\rho(a_{k+1}) \ge \bar\rho(a_k)` for some :math:`k`, with
the measure

.. math::

   v = \min\!\Big(10^8,\ \frac{\sum_k \max\big(0,\ \bar\rho(a_{k+1}) - \bar\rho(a_k)\big)}{\max_k |\bar\rho(a_k)|}\Big),

the summed rise relative to the largest density. A flat step (a rise of
exactly 0) fails with :math:`v = 0`, at the band-D floor. The measure is
saturated when :math:`\bar\rho = 0` on the whole grid, a value is not
finite, or a pair weight is :math:`\le 0`. Under ``RHOFORM MONOTONE``
:math:`\bar\rho(a)` is non-increasing by construction, but the gate asks
for a strict decrease: a flat stretch still fails (under ``Cut_Dummy``,
every shell on the plateau below the innermost bump, where :math:`\rho` is
constant), and so does a rise at the rounding level. At startup the
first shell at :math:`a = f_{hi} a_0` must lie inside the smallest cutoff a
candidate can have (``RHOBARGATE: the first bcc shell at a = f_hi a0 = ...
A lies at r = ... A, beyond the smallest cutoff a candidate can have ...``);
the shells are enumerated once, for the largest cutoff a candidate can
reach. The reject line is ``eval N: lattice-rhobar gate reject (<K> rises
over a/a0 [<f_lo>, <f_hi>] on <lattice>, first at a/a0 = <f>, v = <v>);
total rejects=K`` (the first rising step, not the worst). The scan costs
about 0.3 ms per candidate at 1001 points (0.9 ms at 3501).

*Example.* The potential of the ``RHOGATE`` example, searched over two
amplitudes with both density gates (bcc Ta, :math:`a_0 = 3.304` Å):

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM AMPGRIDMB (2) 0.0 2.0
   OPTIM AMPGRIDMB (3) -0.5 0.5
   RHOGATE 1e-5
   RHOBARGATE bcc 3.304 0.98 1.10

The startup line reads ``RHOBARGATE: lattice rhobar(a) gate, bcc (a0 3.304
A, a/a0 [0.98, 1.1], 1001 points), 4 shells within the largest reachable
RCUTMB, SEED ERROR; armed per candidate on AMPGRIDMB[2], AMPGRIDMB[3]: ...;
seeds: -c pot (INIT CONFIG) PASS``. In a 120-candidate ``TADAH:RANDOM``
search of this box, 57 candidates failed the sign gate and 29 more only the
lattice gate, for instance::

   INFO  eval 21: lattice-rhobar gate reject (577 rises over a/a0 [0.98, 1.1] on bcc,
         first at a/a0 = 1.0255, v = 3.3868e-02); total rejects=4

Combined grading and attribution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``BUMPGATE`` is checked first, on the raw parameters; ``RHOGATE`` and
``RHOBARGATE`` right after the candidate's configuration is built (still
before training and before any MPI traffic). When any configured gate fails,
every configured gate is evaluated and the candidate is graded by the sum
of the failing gates' measures, ``v = min(DBL_MAX, sum of their v)``,
so a move that shrinks any one violation is downhill even while another
gate still fails (a saturated measure stays saturated). The reject is
counted, and journalled, for the **first** failing gate in the order
``BUMPGATE``, ``RHOGATE``, ``RHOBARGATE``; its line names the others. From
the example above::

   INFO  eval 2: rho-sign gate reject (rho_min/rho_max = -2.3527e-01 at r = 4.4068 A, tol 1.0000e-05,
         v = 2.6067e-01); also failing: lattice-rhobar gate (v = 7.2300e-01); graded by the summed
         v = 9.8367e-01; total rejects=1

A lone failure keeps its own line, so without ``RHOGATE`` and ``RHOBARGATE``
every evaluation, ``BUMPGATE`` line and count is exactly as before.

**Arming and seeds** (``RHOGATE``, ``RHOBARGATE``). A density gate is
*armed*, i.e. evaluated for every candidate, when an ``OPTIM`` key
(case-insensitive) is ``AMPGRIDMB``, ``CGRIDMB``, ``SGRIDMB`` or
``RCUTMB``. Otherwise every candidate has the ``-c`` density: its verdict is
computed once and reused, and an ``OPTIM WATOM`` only has each candidate's
pair weights checked. Because every evaluation would then be rejected, a
failing ``-c`` density with an unarmed gate is a startup error under either
``SEED`` mode, and so is a pair weight no candidate can make positive
(``WATOM`` fixed at :math:`\le 0`, or an ``OPTIM WATOM`` box that does not
reach above 0). At startup every *seed* is evaluated as a candidate: the
``-c`` potential and every ``CONFIG`` or ``WARM`` point of the resolved
``INIT`` chains (``OUTER``, ``PRESTAGE``, ``PRESTAGE.INNER``, the
``BASINS`` stages). A failing seed that the optimiser may start from stops
the run under ``SEED ERROR`` (one message listing every failing seed with
its origin, the extremes, the window and ``v``, or for ``RHOBARGATE`` the
rises, the first :math:`a/a_0` and ``v``) and is a WARNING under ``SEED
WARN`` (``SEED WARN: the run proceeds; if the optimiser starts from this
seed, that evaluation is rejected before training (band D).``). A failing
``-c`` potential that is not an ``INIT`` start (a ``WARM``- or ``LHS``-only
chain) gets a WARNING. ``RANDOM``, ``LHS`` and ``SOBOL`` points are ordinary
candidates. Each configured gate logs one startup ``INFO`` line: its
settings, whether it is armed (and on which dimensions, or why not) and
every seed's verdict.

.. _hpo_density_report:

The density report
^^^^^^^^^^^^^^^^^^

Every run whose many-body term is a bare DM_REAM, gated or not, reports the
density of the ``-c`` potential in the POTENTIAL SETUP section of
``hpo_run.log``, and of the best in RESULT. Example 22::

   Density form          : RHOFORM SIGNED -- rho = W f_c sum a_n psi_n, signed amplitudes (no sign guarantee)
   Density rho(r), -c pot: [0.8226, 5.4840] A, 20001 pts: min 0.0000e+00 (r 5.4840) max 5.5617e+00 (r 0.8226)
                           rel 0.0000e+00; max rho' +4.61e-03 (r 2.1430); non-increasing on window: no; W = 1

(one line in the file). The window is ``RHOGATE``'s (``R_LO``, ``NPTS``)
when it is configured, else :math:`[0.15\,r_c, r_c]` with 20001 points,
:math:`r_c` the ``RCUTMB`` of the potential. The row gives the extremes of
:math:`\rho_{raw}` and where they occur, ``rel`` = :math:`\rho_{min} /
\rho_{max}` (``n/a`` when :math:`\rho_{max} \le 0`), the largest
forward-difference slope ``max rho'`` and where, whether :math:`\rho_{raw}`
is non-increasing on the window, the pair weights (``W = <w>`` or
``W in [<lo>, <hi>]``; ``tadah hpo`` inserts ``WATOM`` = Z, the atomic
number, when the potential has none, so a potential without a ``WATOM``
line reports ``W = Z``) and, with ``RHOGATE``, its verdict.
*Non-increasing* means :math:`\max_i(\rho_{i+1} - \rho_i) \le
8\,\varepsilon \max_i \sum_n |a_n T_n(r_i) f_c(r_i)|`: the tolerance is
relative to the size of the terms, not to :math:`\rho`, because
``MONOTONE`` is non-increasing to rounding only. A ``SIGNED`` density often
reads ``no`` (example 22: ``max rho' +4.61e-03``), which is information, not
an error. With ``RHOBARGATE`` a ``Density rhobar`` row follows
(``strictly decreasing (0 rising steps)`` or ``rises at <K> of <NPTS-1>
steps, first at a/a0 = ... (v = ...): RHOBARGATE FAIL``). Each distinct
``WARM`` seed adds ``Density rho(r), WARM <k>`` rows, evaluated as the run
evaluates it (its ``OPTIM`` dimensions from the seed, the rest from
``-c``). RESULT prints the same rows for the best (``Density rho(r) : best
(eval N) ...``), or ``n/a (the best predates this session)`` after a
resume. A density that cannot be evaluated prints ``not derivable:
<reason>``; the report never stops a run.

.. _hpo_rhoform_positive:

Positive density forms: projection and non-negative boxes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A DM_REAM ``-c`` configuration must declare :ref:`RHOFORM
<dm_ream_rhoform>` (without it startup stops with ``tadah hpo: DM_REAM:
RHOFORM is required: ...``), and its density is validated for its form at
startup: a density DM_REAM would refuse (a negative amplitude under
``NONNEG``, a ``WATOM`` :math:`\le 0` under a positive form, ...) is a
startup error rather than a failure of every evaluation. Under ``NONNEG``
and ``MONOTONE`` every candidate must stay a valid density, so the search
space is restricted:

* ``OPTIM AMPGRIDMB ... AUTO`` (a linear rule; ``FRAC 0.5`` by default)
  clamps the box at 0, :math:`lo = \max(0, x_0 - d)`, and without ``FLOOR``
  or ``GROUP_FLOOR`` it takes an implicit ``GROUP_FLOOR 1e-3`` (the ``tadah
  refit`` writer's group floor), so a zero amplitude, which non-negative
  fits produce, gets the box :math:`[0, 10^{-3}\max|x_0|]` of its line.
  Such boxes carry the origin ``AUTO FRAC>=0`` in the SEARCH SPACE section,
  ``hpo_search_space.tsv`` and ``hpo_optim_resolved.hpo``. ``RATIO`` is
  allowed only when every seed on the line is :math:`> 0`, a negative seed
  is an error, and a line whose seeds are all 0 needs ``FLOOR <a>`` (the box
  :math:`[0, a]`) or explicit bounds. ``LOG`` needs :math:`lo > 0`, so a box
  clamped at 0 cannot be ``LOG``.
* An explicit ``AMPGRIDMB`` low bound below 0 is an error (0 is allowed), a
  ``WATOM`` low bound :math:`\le 0` is an error (explicit or ``AUTO``), and
  under ``MONOTONE`` so is an ``SGRIDMB`` low bound :math:`\le 0`.
* **Projection.** Each evaluation evaluates a copy of the candidate with
  every negative ``AMPGRIDMB`` coordinate set to 0 (a NaN is left for
  DM_REAM to refuse), before the gates and the model build, and that
  projected point is what is recorded (``params.tadah``, the potentials),
  so every recorded point is a valid potential; the optimiser still sees its
  own point with the projection's loss. Inside the box this happens only to
  finite-difference probes next to a zero amplitude: at the bound a central
  difference then becomes a one-sided difference halved, finite and of the
  right sign. The first projection is an ``INFO`` line (``eval 8: RHOFORM
  NONNEG: AMPGRIDMB[3] = -5.0000e-10 below 0 is evaluated and recorded as 0:
  the candidate is projected onto AMPGRIDMB >= 0 (a finite-difference probe
  next to a zero amplitude, or an optimiser that ignores bounds);
  DIAGNOSTICS counts every projection``), and ``DIAGNOSTICS`` prints
  ``Positive-form projection: N evaluation(s) with an AMPGRIDMB amplitude <
  0, ...`` when there was one. An optimiser that can leave the box
  (``CERES`` ``LBFGS``/``WOLFE``/``NLCG``/``STEEPEST_DESCENT``, ``NLOPT
  LN_NEWUOA``, or ``PARAM fd_kernel ceres`` on a gradient-based optimiser)
  gets one startup WARNING when ``AMPGRIDMB`` is ``OPTIM``'d. A zero
  amplitude seed sits on its box's lower bound, and ``PRAXIS`` and
  ``BOBYQA`` may move a start that lies on a bound (see
  :ref:`search_space_scaling`). Under ``SIGNED`` nothing is projected.
* **Hard lower bounds.** An ``AMPGRIDMB`` lower bound of exactly 0 is
  *hard*: when the best rides it, the bound-occupancy audit shows ``LO*``
  instead of ``LO``, with the note ``(* a hard lower bound: RHOFORM NONNEG/MONOTONE require AMPGRIDMB
  >= 0, so an amplitude at 0 is an admissible optimum, not bound
  pressure)``; the advice to widen that side (and its HINT) is printed only
  when another dimension rides a bound. An ``AUTO`` box whose lower bound is
  above 0 can still be widened down to 0 and keeps the normal advice.

WARM seeds and structure keys
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A ``WARM`` seed whose ``RHOFORM`` is absent, invalid or different from the
run's (both being DM_REAM potentials) is an **error** when an ``OPTIM`` key
is ``AMPGRIDMB``, ``CGRIDMB`` or ``SGRIDMB``: density amplitudes mean
something only in the form they were fitted in. A tadah 1.5.1 seed is
``SIGNED``: add ``RHOFORM SIGNED`` to it for a ``SIGNED`` run; a
``NONNEG`` or ``MONOTONE`` run needs a seed fitted in its form. Otherwise
(no density key searched) it is a WARNING: the seed's density keys are not
read. The rule is independent of
``WARM_ON_MISMATCH``.

``OPTIM`` on structure data is refused at startup, case-insensitively:
``RHOFORM``, ``TYPE2B``, ``TYPEMB``, ``RCTYPE2B``, ``RCTYPEMB``,
``KDEG2B``, ``KDEGMB`` and ``KDEGEMB`` (``OPTIM <KEY> is not a valid
search target: ..., structure data rather than a continuous parameter;
set it in the -c potential/configuration.``); ``OPTIM EMBF0`` has its own
message.

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
   * - ``MAXTIME`` (float)
     - Whole-optimiser wall-time in seconds for this active driver.  This is
       distinct from root-level ``PC_SCRIPT_TIMEOUT`` (whose alias is
       root-level ``MAXTIME``).
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
     - Initial step for the NLopt derivative-free algorithms, as a
       **fraction of each OPTIM dimension's (transformed) width**,
       ``0 < STEP <= 0.5`` (the same meaning as the annealers'
       ``PARAM STEP_FRAC``).  NLopt receives one step per dimension:
       SBPLX/NELDERMEAD use them as the first simplex's edges,
       BOBYQA/COBYLA as their per-dimension coordinate rescaling,
       NEWUOA/PRAXIS collapse them to one radius, MLSL/AUGLAG forward
       them to ``INNER`` (an outer ``STEP`` overrides an ``INNER`` one).
       Per-key/per-index overrides: ``STEP <KEY>[(indices)] <fraction>``
       (repeatable).  Near a bound the step is adjusted per dimension
       (BOBYQA: shrunk to 0.75 of the slack so the warm start is not
       relocated; SBPLX: pointed inward).  Unset: NLopt's own
       per-dimension default (25 % of the width, or 75 % of the distance
       to a nearer bound).  Every other backend rejects ``STEP`` at
       startup with a hint (annealing: ``PARAM STEP_FRAC``; GRID:
       ``PARAM GRID_POINTS``; GFS/RANDOM/BFGS/CERES/ASA have no initial
       step).  The effective per-dimension step and the algorithm's
       consumption mode are printed in ``hpo_run.log``.
   * - ``SEED`` (uint)
     - Random seed (default: current time).  Inside an ``INIT`` block
       the same key seeds the stochastic init strategies instead.
   * - ``VECTOR_ARR`` (uint)
     - Storage size required by some NLOpt algorithms.
   * - ``THREADS`` (uint)
     - Width of the shared per-rank worker pool that runs the
       ``PC_LAMMPS`` fan-out and MLIP train/predict for every backend
       (``ANNEAL_PT``/``ANNEAL_PA`` also cap concurrent replicas by it).
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
       :ref:`tadah_anneal_block` below), ``ANNEAL_PT`` (parallel
       tempering, :ref:`tadah_anneal_pt_block`), ``ANNEAL_PA``
       (population annealing, :ref:`tadah_anneal_pa_block`).
   * - ``ASA``
     - ``ASA`` — Lester Ingber's Adaptive Simulated Annealing
       (ASA-30.53, vendored, BSD-3-Clause). Fat-tailed per-dimension
       generating distribution, sensitivity-driven reannealing,
       optional quenching and block-parallel evaluation. Documented on
       its own page: :ref:`asa_optimizer`.
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
``TADAH:ANNEAL_PT``/``_PA``      First only — the coldest slot starts there and the
                                 rest are displaced around it under
                                 ``INIT_MODE JITTER`` (default; sigma from
                                 ``STEP_FRAC``, an explicit fraction, or measured
                                 per dimension by ``JITTER AUTO <p>``). ANNEAL_PT
                                 additionally scales the displacement along the
                                 ladder — see
                                 :ref:`ladder-scaled initial jitter
                                 <anneal_pt_init_jitter_ladder>`.
``ASA:ASA``                      First only — becomes ASA's
                                 ``parameter_initial_final`` and auto-sets
                                 ``User_Initial_Parameters 1``
                                 (:ref:`asa_optimizer`).
================================ ===================================================

``MULTISTART > 1`` runs the inner optimiser ``K`` times and consumes one
seed per start.  ``DLIB:GFS`` collapses to a single inner call because
GFS already takes ``K`` seeds natively.

.. _search_space_scaling:

Search-space scaling
~~~~~~~~~~~~~~~~~~~~

Every ``OPTIM`` dimension has a *real* box ``[lo, hi]`` (what the
configuration and the model see) and an *internal* coordinate ``u`` that
the optimiser iterates on.  One object, the search space, owns the map:

.. code-block:: none

   t = T(x)                                   per-dimension shape: LIN  t = x
                                                                   LOG  t = ln x
                                                                   ASINH c  t = asinh(x / c)
   u = (t - T(lo)) / (T(hi) - T(lo))          OPTIM_SCALE AUTO  (unit box [0,1])
   u = t                                      OPTIM_SCALE NONE  (raw units; ln for LOG dims)

``OPTIM_SCALE AUTO | NONE`` is a root-level key (next to the ``OPTIM``
lines).  Under ``AUTO`` every dimension spans ``[0, 1]``, so the
optimiser-level controls ``STEP``, ``XTOL_ABS`` and ``PARAM fd_step``
mean the same fraction of every box regardless of the keys' units --
the point of the transform: real searches mix box widths across five to
nine decades (knot positions in Angstrom next to amplitude coefficients
of order ``1e-3`` and pinned linear weights of order ``1e4``), and every
derivative-free algorithm assumes its variables are commensurate.
Under ``NONE`` the internal coordinates are the pre-1.5 ones (raw
units, ``ln`` for ``LOG`` dims) -- the escape hatch for A/B checks of the
coordinate system, also reachable through the environment override
``TADAH_OPTIM_SCALE=NONE``.  Runs are not byte-identical to those of
earlier releases, however: numbers are no longer rounded on their way
through the code (grid values, for example), which shifts losses at
about the ``1e-13`` relative level, and starting points are evaluated at
exactly their loaded values (see below; this matters for ``LOG``/``ASINH``
dimensions, whose ``exp``/``log`` round trip is not exact, and for
optimisers that round their start point).  The default is ``AUTO``.

``LOG_HP <ratio>`` (a finite ratio ``>= 1``, declared once at the
``OPTIMIZER`` or ``STRATEGY`` level; rejected inside
``EXPLORE``/``REFINE``/``INNER``/``INIT`` blocks) tags every untagged
dimension with ``low > 0`` and ``high/low >= ratio`` as ``LOG``; an
explicit tag on the ``OPTIM`` line always wins.  Default: no
auto-tagging.

All internal arithmetic happens in ``u``; the model is always evaluated
at the real point; ``best_*.tadah``, ``params.tadah`` and the WARM seed
files are real-space.  The ``SEARCH SPACE`` section of ``hpo_run.log``
prints the policy, the real width ratio across dimensions, and one row
per dimension with its scale, box origin, real and internal bounds and
the starting point in both spaces.

Starting points are evaluated exactly.  The round trip ``x -> u -> x``
is not the identity in floating point: ``lo + u*(hi - lo)`` rounds (in a
``[-300, 300]`` box a weight of ``-2.87486722631648e-4`` comes back
with a relative error of ``3e-11`` and ``1.2e-14`` comes back as ``0``),
and ``LOG``/``ASINH`` add ``exp``/``log`` rounding.  Every optimiser run
therefore maps the internal coordinate of each of its starting points
back to exactly the real value it came from, dimension by dimension:
the ``CONFIG`` and ``WARM`` values, every other ``INIT`` chain point,
``MULTISTART`` starts, ``RESTART`` refreshes, ``PRESTAGE`` top-K seeds
and ``BASINS`` pool seeds.  Whenever the optimiser evaluates one of its
starting points (most do so first; ``ASA`` after its initial
cost-temperature samples), the model sees the loaded doubles bit for
bit, whatever the box (explicit, ``AUTO`` ``FRAC``/``RATIO``/``FLOOR``/
``GROUP_FLOOR``, ``LIN``/``LOG``/``ASINH``, either ``OPTIM_SCALE``; one
exception under ``NONE`` below), and a coordinate the optimiser has not
moved keeps its seed value in later evaluations.  This includes
optimisers that round the start point before evaluating it (NLopt's
``BOBYQA`` and ``COBYLA`` rescale it when the initial steps differ
between dimensions, ``PRAXIS`` maps it through its bound transform),
whose first evaluation lands a few units in the last place away from the
seed's ``u``: any ``u`` within 4 units in the last place of the internal
box's largest magnitude is evaluated at the seed.  The window is
absolute -- ``4 x 2.2e-16`` of the box width under ``AUTO``, capped at
``2^-27`` (``7.5e-9``) but never below 2 units in the last place of the
seed's own ``u`` -- so it replaces only a move smaller than itself away
from a seed, which the plain map resolves where it is finer than the box
(a ``[0, W]`` box near ``0``, or any linear dimension under ``NONE``).
No optimiser moves that little on purpose: steps and finite-difference
steps are far larger (with the default ``PARAM fd_step`` of ``1e-6``, at
least ``1e-6`` in internal units, or ``1.5e-8`` for the Ceres kernel,
whose smallest step is what caps the window), and an annealing proposal
that small evaluates the seed again; only a ``PARAM fd_step`` set by hand
below the window (under ``NONE`` with ``fd_kernel inhouse`` on a linear
box reaching ``1.7e7``: below about ``7.5e-9``) gives a zero derivative
at the seed.  The cap matters only under ``NONE`` for a linear box
reaching beyond ``1.7e7``: there ``PRAXIS``'s start can be evaluated a
few units in the last place of the box off a seed of smaller magnitude
than the box (the exception above).  ``PRAXIS`` rounds its start by a few
units in the last place of the box's midpoint and half-width, while the
capped window is only ``2^-27`` or 2 units in the last place of the seed;
measured with NLopt 2.10 this happens for seeds up to about 0.3 of
``max(|lo|, |hi|)`` on a ``[0, W]`` box and up to about 0.6 (rarely 0.8)
on a box straddling ``0``, never from 0.9 of it on, and only on boxes of
``2^25`` (``3.4e7``) or more.  Any other point maps as before.  Two limits: when
two seeds of one run are closer in a dimension than the box resolves
(e.g. ``0`` and ``1e-20`` in ``[-300, 300]``), the first one listed wins;
and a start point an optimiser moves on purpose is evaluated where it
was moved to (``PRAXIS`` moves a start that lies on a bound ``1e-12`` of
the half-width inside; ``BOBYQA`` moves a start closer to a bound than
its initial step onto the bound or one step away from it).

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

A start that fails on its own (a rejected setup, an optimiser error) is
reported as ``MULTISTART start i/K failed: <error>`` — a ``WARNING`` in
``hpo_run.log`` and on stderr — and the next start runs; the final status
then notes ``(N of M MULTISTART starts failed; see the WARNINGs)``, counted
over the whole run: after a :ref:`RESTART <restart_decorator>` relaunch it
reads ``(N of M MULTISTART starts failed across L launches; ...)``, and it is
also appended to RESTART's ``STOPPED -- stagnation: ...`` status.  When
every start of a launch fails, the run fails like a single run would
(``FAILED -- <first error>``, non-zero exit).  A best potential that cannot
be saved, or a shutdown signal, ends the whole run at once.

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

LIB TADAH — the annealing family
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Monte-Carlo optimizers developed in-tree have their own reference
pages under :doc:`optimizer`:

* :ref:`tadah_anneal_block` — serial simulated annealing, classical and
  Corana-adaptive, with automatic temperature and chain-length
  calibration;
* :ref:`tadah_anneal_pt_block` — parallel tempering (replica exchange),
  with a reference manual and a step-by-step setup guide;
* :ref:`tadah_anneal_pa_block` — population annealing;
* :ref:`asa_optimizer` — Ingber's Adaptive Simulated Annealing.

The :ref:`shared vocabulary <anneal_vocabulary>` used by those pages
and by ``hpo_run.log`` is defined once on the optimizer page.

.. _tadah_anneal_mpi:

Multi-node runs (TADAH_HPO_MPI) and ARCHER2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Both parallel drivers scale past one node through the opt-in
``-DTADAH_HPO_MPI=ON`` CMake option: replicas/walkers distribute over
MPI ranks in contiguous slot blocks, every rank trains locally and runs its
LAMMPS instances on ``MPI_COMM_SELF``.  PT boundary exchanges communicate
``f`` and the full parameter vector ``x``; PA gathers losses and, when
resampling, gathers/scatters full ``(f, x, V)`` walker states.  Control and
best-broadcast messages also travel; training databases and model fitting
remain rank-local.  The same config file can run serially, threaded, or
under ``srun``/``mpirun`` provided the selected driver's global ``REPLICAS`` or ``POPULATION``
count is at least the chosen rank count.
Requirements and behaviour:

* a multi-rank launch accepts only a plain ``TADAH:ANNEAL_PT`` or
  ``TADAH:ANNEAL_PA`` strategy.  Unsupported serial, checkpointed, or
  decorated strategies fail before policy setup; an MPI launch of size one
  keeps the normal serial behaviour;
* an MPI-flavoured LAMMPS library (the serial archive carries MPI stub
  symbols that collide with a real MPI at link time — configure-time
  check);
* ``MPI_THREAD_MULTIPLE`` (evaluation threads run LAMMPS concurrently);
  on Cray MPICH: ``export MPICH_MAX_THREAD_SAFETY=multiple``;
* the global ``REPLICAS`` (PT) or ``POPULATION`` (PA) count must be at
  least the MPI rank count;
* the MPI C interfaces use ``int`` element counts.  PT and PA therefore
  require ``D + 2 <= INT_MAX`` for their calibration packets; PT additionally
  requires ``D <= INT_MAX`` for boundary exchanges.  PA requires
  ``POPULATION <= INT_MAX`` and
  ``POPULATION * (2*D + 1) <= INT_MAX`` for gathered/scattered states.
  In a multi-rank run, PA rejects these sizes before dimension-sized driver
  allocation, host evaluation, or communication;
* rank-local fixed-name outputs carry an explicit ``.r<rank>`` suffix
  on **every** rank, including rank 0;
* the unsuffixed ``best_pot.tadah`` is a separate canonical global-best
  checkpoint, updated live by every rank under a cross-process lock.  A
  complete potential replaces it for a lower loss, or for an exactly equal
  loss and a lexicographically smaller ``(rank, slot, evaluation)`` key;
* ``MAXEVAL`` is a hard global cap across seed/probe evaluation,
  population initialisation, annealing, and finalisation.  No rank-0
  re-evaluation is performed after the budget is exhausted;
* at exit the ranks meet in a barrier of their own.  Ranks that all
  finish — normally or failing together — exit normally, each with its
  journal complete.  A rank that ends with an error and is not joined by
  every other rank within 30 s aborts the whole job (``MPI_Abort`` with its
  exit status; its own ``hpo_run.log.r<rank>`` is already closed): the
  missing ranks are blocked in a collective the failed rank left — after a
  failure confined to one rank, such as a best potential it could not save
  during an ``INIT_MODE JITTER AUTO`` probe or ``NOISE_CALIBRATION`` — and
  would otherwise hold the allocation until it ends.  Their journals then
  end without a RESULT section;
* mutually exclusive with the ``TADAH_BUILD_MPI``
  (ScaLAPACK-distributed training) build.

ARCHER2 sketch (128 cores/node, 2x AMD EPYC 7742; inter-node
communication requires MPI; OpenMP cannot span nodes):

.. code-block:: bash

   #!/bin/bash
   #SBATCH --nodes=2
   #SBATCH --ntasks-per-node=8        # MPI ranks (replica blocks)
   #SBATCH --cpus-per-task=16         # shared-pool workers per rank
   #SBATCH --time=01:00:00
   export OMP_NUM_THREADS=1           # residual OpenMP kernels only
   export MPICH_MAX_THREAD_SAFETY=multiple
   # THREADS 16 in STRATEGY: one 16-worker shared pool per rank
   srun --distribution=block:block --hint=nomultithread \
        tadah hpo --hpotarget config.hpo -c config.train

Size the main worker pool as ``ranks * THREADS <= total cores``.
``OMP_NUM_THREADS`` is an independent width for short residual kernels,
not a factor in that equation; start at 1 for PT/PA and benchmark it
separately.  ``REPLICAS`` (PT) and ``POPULATION`` (PA) must each be at least the
rank count when their respective driver is selected; multiples distribute
slots evenly but are not required.

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
into each other.  The window counts evaluations since the last new
*global* best, so a restart must beat the incumbent within
``AFTER_STAGNATION`` evaluations to keep going.  ``MAXEVAL`` applies to
each launch, so a run can use up to ``(MAX + 1) x MAXEVAL`` evaluations.
When the last launch stagnates again the run stops with ``STOPPED --
stagnation: no new best in W evals after R restart(s)`` (exit status 0).

Under ``MULTISTART`` every start gets its own window: a start that runs out
of it ends (``MULTISTART start i/K done: ...; stopped on stagnation``) and
the next start runs.  After the last start, if any start stagnated, the
batch reports stagnation and the restart relaunches all ``K`` starts from
``K`` fresh points (``STRATEGY LHS 3`` for ``N_STARTS 3``, say).  Under
``PRESTAGE`` a stagnating ``EXPLORE`` phase hands over to the refinement,
whose seeds again get a window each; the run restarts (both phases) only
when a refinement seed stagnated **and** the refinement did not improve the
global best.

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
``MAXTIME`` / ``STOPVAL`` / ``FTOL_*`` / ``XTOL_*`` / ``POPULATION`` /
``STEP`` / ``SEED`` / ``VECTOR_ARR`` / ``THREADS`` / ``GRAD_STRATEGY`` /
``PARAM``) are valid inside ``INNER``.  With MLSL/AUGLAG an outer
``STEP`` overrides an ``INNER`` ``STEP`` (NLopt forwards the outer
initial step to the local optimiser); a startup warning says so.

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

Each file begins with a ``#`` header describing its columns.  Eligible rows
follow the ``OUTPUT`` cadence, and the first column is always the iteration
number.

=====================  =====================================================================
File                   Columns
=====================  =====================================================================
``loss.tadah``         All enabled individual loss terms, then the averaged total loss.
``params.tadah``       The current hyperparameter vector in the order defined by ``OPTIM``.
``outvar.tadah``       Per-evaluation wall time in milliseconds, enabled validation metrics,
                       LAMMPS ``--outvar``/``--varloss`` values, and typed-observable values.
``stats.tadah``        Per-validation-dataset values and their average for each enabled metric.
=====================  =====================================================================

Cadence and formatting:

============ ========= =================================================================
Key          Default   Effect
============ ========= =================================================================
``OUTPUT N`` ``1``     Write a main-file row when the iteration is divisible by ``N``.
``DIGITS d`` ``6``     Decimal digits in scientific-notation columns (also the RESULT
                       table in the ``#`` header of potential archives); ``1 ≤ d ≤ 15``.
``QFLUSH N`` ``10``    Queued completed results that trigger a writer-thread drain;
                       shutdown or an explicit flush drains the remainder.
============ ========= =================================================================

Best-improvement histories
~~~~~~~~~~~~~~~~~~~~~~~~~~

On one process, every new minimum contributes a row to the companion
history files when ``BOUTPUT`` is enabled.  Under multi-rank PT/PA, the
suffixed files record that rank's improvements; the unsuffixed
``best_pot.tadah`` is the canonical cross-rank winner:

* ``best_loss.tadah``
* ``best_params.tadah``
* ``best_outvar.tadah``
* ``best_stats.tadah``

These files append one row per qualifying improvement; they are histories,
not single-row snapshots.

.. list-table::
   :header-rows: 1
   :widths: 22 14 64

   * - Key
     - Default
     - Effect
   * - ``BOUTPUT flag``
     - ``1``
     - ``0`` disables tabular ``best_*`` history rows; any positive value
       enables a row for every new best.  The magnitude is not a cadence.

.. _hpo_potential_archives:

Potential archives
~~~~~~~~~~~~~~~~~~

The current global best potential is saved live to the canonical
``best_pot.tadah``.  Every process-local improvement synchronously
competes for this checkpoint under a cross-process lock.  Candidates are
ordered by ``(loss, source rank, source slot, source evaluation)``: lower
loss wins, with the remaining fields providing an exact-loss tie-break.
The complete winning potential is
written to a unique temporary file, flushed, and atomically renamed, so
an abrupt ``SIGKILL`` leaves either the preceding complete winner or the
new complete winner, never a torn file.  Its header records the loss,
source rank/slot/evaluation, and candidate-local hot-variable values.
A canonical checkpoint write or durability failure is fatal rather than
silently allowing an uncheckpointed optimisation to continue.  A
rank-local diagnostic-mirror failure is reported but cannot invalidate an
already-durable canonical winner; the mirror retries for at most 1 s, and
after a failure makes a single attempt per new best (unlogged) until one
succeeds again.

Transient filesystem errors are retried first.  On network filesystems
(SMB/CIFS, NFS) a lock, write, ``fsync`` or rename can fail for a moment
(``EBUSY``, ``EAGAIN``, ``ESTALE``, ``ETXTBSY``, ``EIO``, ``ENOLCK``,
``ETIMEDOUT``, ``EDEADLK``) and succeed when repeated.  ``EACCES`` (an SMB
sharing violation or delete-pending file) is retried too, but only when the
directory is on a network filesystem (SMB/CIFS, NFS, AFS, Ceph, Lustre,
GPFS, 9P or FUSE); on a local disk it is a real permission problem and
fails at once.  A rename that reports ``ENOENT`` is also retried: on NFS a
retransmitted rename that had already landed fails that way, and the retry
recognises its own potential in place.
Publication then retries its whole lock → read → compare → replace
transaction with exponential backoff (10 ms doubling to 2 s, ±20 %
jitter), releasing its locks between attempts so that a better candidate
published meanwhile still wins.  Retrying stops once 30 s have passed since
the first failure; a failing call that blocks by itself (a soft-mount
timeout) can add its own duration at either end, and no budget bounds a
call that hangs on a hard mount.  Meanwhile the evaluations whose loss may
still become the new best wait at their finalize step, and so does an
evaluation whose :ref:`RESTART <restart_decorator>` or BASINS stagnation
check would stop the optimiser (it first waits until as many new-best
decisions have finished as were in progress when it checked); the others
carry on.  Permanent errors (``ENOSPC``, ``EROFS``, ``ENOENT``, …) are not
retried.  When the error is permanent or the retries are exhausted the run
stops, whatever the optimiser (NLopt, MULTISTART and the
``ANNEAL_PT``/``ANNEAL_PA`` calibration included): the RESULT section
reads ::

   Optimiser status        : FAILED -- best-potential persistence: <error> (gave up after N attempts over T s)

and ``tadah hpo`` exits with a non-zero status.  New bests of other
chains that are still waiting to be saved then get a single attempt each,
without backoff: a permanent error fails at once, as before the failure,
and only a rename that reports ``ENOENT`` (which may have landed) gets one
immediate re-attempt.  A failed one is logged as
``best-potential persistence failure at eval M (no backoff after the
failure at eval N): <error>``, which after a transient error ends in
``(gave up after 1 attempt over 0 s)`` (``2 attempts`` when it follows
such a rename); the ``over 0 s`` counts from that candidate's first
failure, so it does not show the call's own duration.  So the run ends
after about one retry budget, not one per waiting candidate, and the
first failure stays the status -- but storage that has recovered by then
still saves them (``best potential of eval M saved after the persistence
failure at eval N (no backoff); the run still ends FAILED``; see below).
This holds when failing calls fail fast: with a soft NFS/SMB mount that
times out after T seconds each waiting candidate adds about T, and at
most one candidate waits per other thread or chain (``THREADS - 1`` in a
``DLIB:GFS`` run); a hard mount that hangs is not bounded at all.
After a shutdown signal (``SIGINT``, ``SIGTERM``, ``SIGUSR2``) retrying
goes on for at most 2 s, counted from when the retry first sees the
signal, with the delays
shortened to fit: the 30 s budget does not hold up the exit, and one
transient blip right after the signal (a scheduler's time-limit warning,
say) still saves the candidate.  When those 2 s are spent the candidate
was not saved, so the run still ends ``FAILED``, with ``(retry abandoned:
shutdown requested) after N attempts over T s`` in place of the ``gave
up`` note.  The run also fails when the error meets a concurrent stop on
its way up (another chain's stagnation stop or a shutdown request in the
same epoch of a parallel annealer, ASA block or ``DLIB:GFS`` batch, or a
PRESTAGE, MULTISTART or BASINS stage that ends on stagnation): a new best
that could not be saved is never overruled, and the run stops at once.
RESULT names the candidate that was lost and what the file holds::

   Best total loss         : 9.0390e-02  (eval 3) -- NOT saved (best-potential persistence failed)
   Last saved best         : 1.0133e-01  (eval 2)
   Best potential saved    : not confirmed -- best_pot.tadah may hold eval 3 (write not synced) or eval 2; check its HPO_SOURCE_EVAL header

``Last saved best`` appears only when an earlier best was saved; the best
metrics below it (``Best EMAE``, ...) are that potential's.  ``not
confirmed`` means the candidate's rename may have happened (a directory
``fsync`` failed afterwards, or the rename itself reported ``ENOENT`` or a
transient error, which on a network filesystem it can do after landing);
otherwise — also when the rename failed with ``EROFS``, ``EXDEV``,
``ENOSPC``, ``EPERM`` or the like, which leave the file untouched — the
line reads ``best_pot.tadah (eval M)``, or ``(none written)`` for a first
best.  When several chains fail to save, the lowest loss is reported,
and ``may hold`` names each candidate whose rename may have happened
(not necessarily the lowest).  If
another chain saved a *lower* best after the failure (the storage
recovered before the run stopped, and its single attempt succeeded), that
saved best is the result and the lost candidate is listed after it; the
run still ends ``FAILED`` with the first failure as its status and a
non-zero exit, because a best was lost on the way::

   Best total loss         : 8.0185e-02  (eval 23)
   Unsaved candidate       : 9.1781e-02  (eval 9) -- could not be persisted; superseded by eval 23
   Best potential saved    : best_pot.tadah (eval 23)

The per-evaluation files (``loss.tadah``, ``params.tadah``,
``outvar.tadah``, ``stats.tadah`` and their ``best_*`` twins) and the
``DUMP``/``BDUMP`` archives are not fatal.  Each writer drain retries
transient errors for up to 10 s *in total*, shared by all its files and
archives, so the first drain after a filesystem goes bad stalls for about
10 s, not 10 s per file.  Every file and archive still makes its own attempt
in every drain (and, after a permanent error such as ``ENOSPC`` or
``EROFS``, one immediate re-attempt); those calls are not bounded by the
budget.  This matters only when failing calls are slow themselves: with a
soft NFS/SMB mount that times out after T seconds the first drain costs
about 10 s plus (files + archives) × T, and every later drain of the outage
about (files + archives) × T; a hard mount that hangs is not bounded at
all.  A batch that still fails is dropped whole — never
half a row, also when the file was truncated underneath the run — and
reported, and the run goes on; that file (or the archives) then gets a
single attempt per drain, without backoff, until a write succeeds again.
After the first ``WARNING`` of such an outage, repeats are summarised (on
the 2nd, 4th, 8th, … failure, or every 10 min, with the rows lost so far),
and an ``INFO`` line reports when writes resume.  After a shutdown signal
a drain's whole retry budget is 2 s instead of 10 s (the final drain keeps
its rows through a blip and still ends well inside a scheduler's kill
timeout; rows it cannot write count as lost), and the final drain after a
best-potential persistence failure does not back off (one immediate
re-attempt after a permanent error).  An archive that
cannot be written is removed only when this run created or truncated it: a
read-only file already at that path is left alone.  The per-evaluation
files are not ``fsync``'d (that would cost every drain a round trip to the
server), so a write-back error that NFS or SMB reports only later is not
seen; ``best_pot.tadah`` is ``fsync``'d.  Every retry is a ``WARNING`` in
``hpo_run.log``, and DIAGNOSTICS gains ::

   Transient I/O retries   : N (best-potential a, per-eval logs/archives b); rows lost c; archives lost d

whenever one of these counts is non-zero (a single immediate re-attempt
without backoff -- a log writer's after a permanent error, or a waiting
candidate's after a rename's ``ENOENT`` -- is logged but not counted as a
transient retry).

After checkpoint detection, a confirmed **fresh run** removes canonical and
rank-local incumbents left by an unrelated objective before evaluating its
first candidate.  An explicit checkpoint **resume** instead retains the
canonical file and validates its compound key against checkpoint state.  A
``WARM`` seed is parsed before this decision, so starting a fresh run from an
old potential remains supported without letting that old loss suppress the
new run's output.

If ``SIGKILL`` lands after the temporary file is flushed but before its
rename, a hidden ``.best_pot.tadah.tmp.<pid>.<counter>`` file can remain.
It is harmless and ignored: readers and later publishers compare only
the canonical path and each later write chooses a fresh temporary name.

In an MPI HPO run, rank-local potential outputs are additionally named
``best_pot.tadah.r<rank>`` on every rank (including ``.r0``).  They are
diagnostic local views; the unsuffixed canonical file is the
lowest-loss potential observed across all ranks and is suitable for a
live warm start without waiting for normal program termination.

============================ ============== =================================================================
Key                          Default        Effect
============================ ============== =================================================================
``DUMP <N> <DIR>``           ``0`` (off)    Save **every** trial potential every ``N`` iterations to
                                            ``DIR`` as ``pot_<iter>.tadah``.  Directory is created if needed.
``BDUMP <N> <DIR>``          ``0`` (off)    As ``DUMP`` but only for new global bests.
============================ ============== =================================================================

Number format.  ``best_pot.tadah``, its rank-local mirrors and the
``DUMP``/``BDUMP`` archives follow the potential-file rules of
:ref:`pot_number_format`: every non-integer number of the key/value
lines is written with
``NUMERIC`` significant digits, default ``18`` — exact, so the file
re-parses to precisely the doubles the evaluation used — and ``NUMERIC``
itself is never written.  A ``WARM`` restart from it starts from the
file's ``OPTIM`` values bit for bit when they lie inside the boxes
(starting points are evaluated exactly, see :ref:`search_space_scaling`);
every other key comes from the ``-c`` configuration, so with the same
configuration the restart's starting point is the archived potential.
The precision is read once, from the configuration given with ``-c``;
it only shapes the files, never the evaluation.  The ``#`` header is an
annotation that ``NUMERIC`` does not govern.  Its machine-read lines, ``# HPO_BEST_LOSS`` and ``# HOT``, are
always written with 18 significant digits (lossless), whatever
``NUMERIC`` is: the durable-best check compares the loss exactly, and
``WARM`` primes the hot-value cache from ``# HOT``.  The rest of the
header (banner, LOSS, RESULT, DATA and OPTIMISER sections) is for
reading: the RESULT table and the archive banners' ``loss=`` use
``DIGITS``, the other annotations fixed short formats.

.. warning::

   ``best_pot.tadah`` files written by ``tadah hpo`` 1.5.0 contain an
   automatically inserted ``NUMERIC 12`` line.  Used as the ``-c``
   configuration of a new run, that line is an explicit setting: the new
   run writes 12-digit potentials and warns about it (``hpo_run.log`` and
   stderr).  Delete the ``NUMERIC`` line to get exact potentials again.

Run log
~~~~~~~

``hpo_run.log`` (rank-suffixed under MPI, like the other per-rank
outputs) is the run's structured journal. It opens with a banner
(version, working directory, PID, timestamp) followed by nine sections::

   -- FILES         --   config / output files
   -- OPTIMISER     --   algorithm, stop criteria, parameters, the config-level
                         ``Initial step`` and ``Step consumption`` lines
   -- SEARCH SPACE  --   scaling policy (``OPTIM_SCALE``, ``LOG_HP``), real width
                         ratio, one row per dimension: scale, box origin, real
                         and internal bounds, start point in both spaces
   (PROGRESS)        --   NLopt runs add an ``Initial step`` INFO block: provenance,
                         consumption mode, and per dimension the step in internal
                         units, as a fraction of the width, in real units at the
                         start, with near-bound / inward / capped notes (WARNINGs)
   (SUMMARY)         --   ``best point (eval N) sits within 2% of an OPTIM limit on
                         K dim(s)`` table + one HINT (bound-occupancy audit)
   -- DIAGNOSTICS   --   also ``Bound occupancy of the final best`` (or ``[ok]``)
   -- POTENTIAL     --   2B/MB terms, cutoffs, element list; DM_REAM:
                         ``Embedding F(0)`` (EMBF0) and the raw F(0) of -c,
                         the density report (``Density form``, ``Density
                         rho(r)``, ``Density rhobar``) of -c and each WARM seed
   -- LOSS FUNCTION --   targets, weights, FAILSCORE
   -- PROGRESS      --   timestamped events: bests, warnings, milestones,
                         feasibility-gate rejects
   -- DIAGNOSTICS   --   NaN rate, graded failures with the gate rejects,
                         positive-form projections, bound-proximity analysis
   -- PERFORMANCE   --   timing statistics
   -- RESULT        --   final status, best loss, best metrics; DM_REAM:
                         raw F(0) of the first and the best evaluation, the
                         best's density rows

The ``Optimiser status`` line of RESULT, and the exit status of
``tadah hpo``:

.. list-table::
   :header-rows: 1
   :widths: 38 10 52

   * - Status
     - Exit
     - Meaning
   * - the optimiser's stop reason, e.g. ``MAXEVAL_REACHED -- ...``, or
       ``COMPLETED``
     - ``0``
     - normal termination (under MULTISTART: the last successful start's;
       when a start failed, followed by ``(N of M MULTISTART starts
       failed; see the WARNINGs)``, counted over the run)
   * - ``STOPPED -- <reason> (...)``
     - ``0``
     - the algorithm terminated itself (NLopt ``NLOPT_FAILURE`` or
       ``NLOPT_ROUNDOFF_LIMITED``, a masked diagnostic); the best result
       stands
   * - ``STOPPED -- stagnation: no new best in W evals after R restart(s)``
     - ``0``
     - :ref:`RESTART <restart_decorator>` used all its restarts and the
       last launch stagnated again (under MULTISTART followed by the
       failed-starts note when a start failed)
   * - ``SHUTDOWN (<signal>)``
     - ``0``
     - ``SIGINT``/``SIGTERM``/``SIGUSR2``; outputs flushed (see the note
       below for multi-rank runs)
   * - ``FAILED -- best-potential persistence: <error>``
     - non-zero
     - the best potential could not be saved (see
       :ref:`hpo_potential_archives`), also when its retries after a
       shutdown signal used up their 2 s (``(retry abandoned: shutdown
       requested)``)
   * - ``FAILED -- <error>``
     - non-zero
     - any other error: a rejected setup, an optimiser failure, or an
       exception outside the per-evaluation FAILSCORE recovery.  Under
       MULTISTART a failure confined to one start is a ``WARNING``
       (journal and stderr) and the next start runs; the run fails only
       when every start of a launch failed

.. note::

   These statuses are per journal.  In a multi-rank ``TADAH_HPO_MPI``
   ``ANNEAL_PT``/``ANNEAL_PA`` run a shutdown signal during the seed/T0
   calibration (or PT's calibration pass 2) ends only rank 0 as
   ``SHUTDOWN`` with exit status 0; the other ranks end ``FAILED --
   TADAH:ANNEAL_PT: rank 0 aborted during seed/T0 calibration`` (``PA``
   likewise) with exit status 1, and the launcher reports a failure.  A
   rank that fails while other ranks are blocked in a collective aborts
   the whole job (``MPI_Abort``, 30 s after its own journal is closed) so
   they cannot hold the allocation (see :ref:`tadah_anneal_mpi`).

The optimizer journals documented on the algorithm pages (banners,
per-rung and per-window reports, audits, the final ``stopped:`` line)
land in PROGRESS, tagged ``INFO``/``WARNING``/``HINT``/``SUMMARY``:
``WARNING`` states something provable about this run, ``HINT`` an
empirical caution, and multi-line messages carry a ``*`` on the head
line. ``hpo_run.log`` is the canonical record of which features fired
during a run; the stdout markers in :ref:`diagnostic_markers` are the
quick-look subset.

Practical tips
~~~~~~~~~~~~~~

* Large optimisations can produce thousands of potentials; watch disk
  usage when ``DUMP`` is enabled.
* The log files are plain text and append-only.  They are tail-friendly:

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

#. Replace the hard-coded ``lattice bcc 3.2`` in that script with the
   injected value. ``lattice bcc ${hot_a0}`` works, but LAMMPS expands
   ``${hot_a0}`` with 15 significant digits; a ``format``-style copy
   passes the exact value (see :ref:`hot_value_precision`):

   .. code-block:: none

      variable hot_a0_s format hot_a0 %.17g
      lattice bcc ${hot_a0_s}

#. End the script by redefining ``hot_a0`` as an ``equal``-style
   variable (**not** ``string``):

   .. code-block:: none

      variable hot_a0 equal lx/<divisor>

   where ``<divisor>`` matches the conventional cell size (e.g.
   ``4.0`` for a 4-atom BCC conventional cell). HPO injects
   ``hot_a0`` as an ``equal``-style variable, so this is a same-style
   overwrite that LAMMPS accepts without a prior ``variable delete``.
   A ``string``-style readback clashes with the injected variable on
   the next iteration ("variable already exists with a different
   style") and the injected value is then silently ignored.

HPO injects ``${hot_a0}`` as an ``equal``-style variable on every call
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
Per-line environment variables (``TADAH_HPO_HOT_A0_DEFAULT``,
``TADAH_HPO_A0_MIN``, ``TADAH_HPO_A0_MAX``) were removed in this
release; the per-line ``--hotvar`` flag replaces all three.

.. _hot_value_precision:

Precision of values passed to and read from LAMMPS
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

HPO's side of the hand-over is lossless: an injected value (a ``--hotvar``
value, or a hot value propagated to a cold script) is defined as
``variable <name> equal <value>`` with 17 significant digits, and every
``--varloss``, ``--outvar``, ``--failvar`` and ``--hotvar`` readback
evaluates the variable's formula directly (LAMMPS ``compute_equal``),
keeping the full double.  Inside the script, however, LAMMPS turns
variables into text with 15 significant digits in two places:

* ``${name}`` of an ``equal``- or ``internal``-style variable, and
* ``v_name`` of an ``equal``-style variable inside a formula, including
  inside ``$(...)`` (``v_name`` of an ``internal``-style variable reads
  the double directly).

A value assembled through these references therefore carries 15 digits
(a relative change of up to ~5e-15), whatever HPO does at either end.
Lossless idioms:

.. list-table::
   :header-rows: 1
   :widths: 26 34 40

   * - Need
     - 15 digits
     - Exact
   * - an injected value as a command argument
     - ``lattice bcc ${hot_a0}``
     - ``variable hot_a0_s format hot_a0 %.17g`` then
       ``lattice bcc ${hot_a0_s}``
   * - capture a scalar for later formulas
     - ``variable Ebulk equal $(c_eatoms)``, used as ``v_Ebulk``
     - ``variable Ebulk internal $(c_eatoms)``, used as ``v_Ebulk``
   * - an outvar built from other variables
     - ``variable Ecoh equal v_Ecoh_raw`` with ``Ecoh_raw``
       ``equal``-style
     - one formula over computes, thermo keywords and ``internal``
       captures, e.g. ``variable Ecoh equal ternary(v_eq_ok,
       -(v_Ebulk-v_N*v_Eiso)/v_N, 100.0)``

``$(...)`` itself renders with 20 significant digits, and ``format``- and
``internal``-style variables may be redefined with the same style on
every evaluation of the cached LAMMPS instance.  Keep the readback of a
hot script as ``variable hot_a0 equal lx/<divisor>``: HPO reads it with
``compute_equal``, so it is exact.  Every variable HPO reads
(``--varloss``, ``--outvar``, ``--failvar``, ``--hotvar``) must stay
``equal``-style; define it over ``internal`` captures (``variable Eiso
equal v_Eiso_i``).  The example scripts ``HPO/examples/common/lammps/Ecoh.in``
and ``Ecoh_param.in`` follow these idioms.

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

The ``TADAH_HPO_BAD_POT_ERMSE_MEV`` / ``_REL`` environment
variables were removed in this release; use
``PC_ERMSE … --skip-above <cap>`` / ``--skip-rel <ratio>`` instead.

.. _performance_parallelism:

Performance & parallelism
.........................

Tadah!MLIP comes in two flavours; the parallel strategy you can exploit
depends on which one you compiled.

Desktop build (OpenMP)
~~~~~~~~~~~~~~~~~~~~~~

* **Shared worker pool** — ``THREADS`` in the ``STRATEGY`` block is the
  total pool width for ``PC_LAMMPS`` fan-out and MLIP train/predict work.
  Serial drivers use the pool inside each objective evaluation;
  ``DLIB:GFS`` and ``ANNEAL_PT``/``ANNEAL_PA`` can also feed work from
  several candidates, replicas, or walkers into that same pool.

* **Residual OpenMP kernels** — ``OMP_NUM_THREADS`` controls the
  remaining OpenMP work, such as neighbour-list construction and data
  statistics:

  .. code-block:: bash

     export OMP_NUM_THREADS=<n>

  It does not size the shared pool and does not multiply ``THREADS``.
  Start with ``THREADS`` equal to the per-rank physical-core budget and
  ``OMP_NUM_THREADS=1``; tune the residual width separately if those
  kernels are material.  The primary sizing rule is::

     THREADS  ≤  physical cores allocated to this rank

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

* Size ``THREADS`` first: it is the main fan-out and train/predict CPU
  budget, including for a serial annealing chain.
* Increase ``OMP_NUM_THREADS`` only when profiling shows the residual
  OpenMP kernels matter; it does not widen ``PC_LAMMPS`` fan-out.
* Measure, do not guess: a few short test runs sweeping ``THREADS`` and
  then the residual OpenMP width will reveal the sweet spot.

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
``[HPO] RESTART: stagnation reached, max restarts (M) used; stopping``         Restarts exhausted; the run stops.
``[HPO] MULTISTART: N of K start(s) stopped on stagnation; …``                Batch outcome reported to RESTART.
``[HPO] WARNING: MULTISTART start i/K failed: …``                             One start failed; the next one runs.
``[HPO] NOISE_AUTO: PARAM fd_step set to V (diag=…, dim=…)``                  ``NOISE.AUTO`` fired and shows the value used.
``[HPO] Initial step (LIB:ALGO): …``                                         NLopt run started; the effective per-dimension step is in the journal.
``[HPO] OPTIM_SCALE X taken from the TADAH_OPTIM_SCALE …``                    Env override of the scaling policy is active.
``[HPO] search space: N dim(s); OPTIM width min … max …``                     Startup width audit; a WARNING names near-pinned dims.
``[HPO] LIB:ALGO (…) does not use STEP; …``                                    Startup rejection of a ``STEP`` the backend would ignore.
``[HPO] DLIB:GFS: dlib applies its own ln transform to …``                     dlib's hidden log transform fires (``OPTIM_SCALE NONE`` only).
``[HPO] NOISE_CALIBRATION: running N evals at the seed point``                ``NOISE.CALIBRATION`` started.
``[HPO] ASA[Ingber ASA-30.53]: seed=… dim=…``                                 ``ASA:ASA`` entry banner with resolved options.
``[HPO] ASA stopped: <reason> after N host eval(s) (asa exit: …)``            ``ASA:ASA`` stop reason + upstream exit code.
============================================================================ ============================================================

See also
--------

* :ref:`asa_optimizer` — the vendored Adaptive Simulated Annealing
  optimiser (``LIB ASA / ALGO ASA``): algorithm background, full option
  tables, tuning guide.
* :ref:`hpo_examples` — worked examples covering the optimiser
  libraries and the ``INIT`` / ``STRATEGY`` / ``EXPLORE`` / ``RESTART`` /
  ``INNER`` blocks.
* :ref:`cli_example_2` — minimal single-loss-term walkthrough.
