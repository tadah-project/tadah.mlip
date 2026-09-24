.. _nfp_manual:

Configuration File
==================


This section is the reference for the HPO configuration file.  The
annealing-family key tables live on the :doc:`optimizer` pages
(generated from the validation registry); ``tadah hpo`` prints the same
information per algorithm.

Block hierarchy
---------------

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
       REFINE   ... ENDREFINE        # optional for TYPE PRESTAGE
       INNER    ... ENDINNER         # optional: NLopt MLSL/AUGLAG inner
       RESTART  ... ENDRESTART       # optional decorator
   ENDOPTIMIZER

Block keywords are case-sensitive.  Comments start with ``#``; long
lines may be continued with a trailing ``\``.  An unknown key is an
error naming the line, and so is a stray value after an optimiser
attribute (``LIB``, ``MAXEVAL``, ``PARAM`` …).

Root-level keys are read top to bottom, and **order matters** for three
of them: every ``PC_LAMMPS`` and ``PC_FAILPROBE`` line captures the
``LOSS``, ``FAILSCORE`` and ``PC_SCRIPT_TIMEOUT`` values in force *when
the line is read*.  Put those three keys before the first ``PC_LAMMPS``
line; a ``LOSS`` written after it does not apply to it.

A short form without a ``STRATEGY`` block — ``LIB``/``ALGO``/``MAXEVAL``
written directly inside ``OPTIMIZER`` — is still accepted and behaves
exactly like ``STRATEGY { TYPE PLAIN ... }``.  It is fine for short
single-shot runs (an ``INIT`` block may still precede it) but cannot
express ``MULTISTART``, ``PRESTAGE`` or the ``RESTART`` decorator; those
features require the explicit grammar.

.. _loss_functions:

LOSS — global loss function
---------------------------

::

   LOSS <Name> [<param>]

The default loss applied to every loss term that does not override it
(default ``L2``).  A ``PC_LAMMPS --varloss`` variable may pick a
different loss locally, and a ``PC_OBSERVABLE`` line takes its own
``--loss``.  Every term is the loss of the measured value :math:`y`
against its target :math:`t` (residual :math:`r = y - t`), multiplied by
the term's weight; the loss of an evaluation is the mean of its terms
(:ref:`nested_fitting`).

================ ================= ==========================================================
Name             Parameter         Term (before the weight)
================ ================= ==========================================================
``L1``           —                 :math:`|r|`
``L2``           —                 :math:`r^2`
``HUBER``        ``δ`` (default    :math:`r^2/2` for :math:`|r| \le δ`, else
                 ``50``)           :math:`δ\,(|r| - δ/2)` — quadratic near zero, linear beyond
``TUKEY``        ``c`` (default    :math:`1 - (1 - (r/c)^2)^3` for :math:`|r| \le c`, else ``1``
                 ``4.685``)        — redescending, zero influence beyond ``c``
``LOG_COSH``     —                 :math:`\log\cosh r` — smooth alternative to L1
``MAPE``         —                 :math:`|r| / |y|` — relative to the *measured* value
``RMSLE``        —                 :math:`(\ln y - \ln t)^2` — log-scaled L2 (positive values)
================ ================= ==========================================================

``HUBER`` and ``TUKEY`` take at most one parameter; when it is omitted
the default above is used.  An explicit parameter must be a finite
number greater than zero.  The other losses take no parameter, and a
value after them is an error.  The ``--varloss`` override uses the same
defaults and validation.

``LOSS`` must precede the ``PC_LAMMPS`` lines it is meant for: each
``PC_LAMMPS`` line captures the ``LOSS`` in force when it is read.

Root-level loss control:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key (type)
     - Description
   * - ``FAILSCORE`` (float)
     - Ceiling of the evaluation loss: a total above it, or a non-finite
       total, is clipped to ``FAILSCORE`` (default: the largest double).
       It is also the value a failed ``PC_OBSERVABLE`` term or an
       unreadable ``--varloss`` variable contributes, and the reference
       for graded failures (:ref:`failscore_grading`).  ``PC_LAMMPS``
       lines capture the value in force when they are read; a line's
       own ``--failscore`` changes only the value substituted for that
       line's variables when its simulation fails.
   * - ``FAILSCORE_GRADING`` (``ON``/``OFF``)
     - Graded failure losses instead of the constant ``FAILSCORE``
       (default ``OFF``); see :ref:`failscore_grading`.

.. _search_space_constraints:

OPTIM — search space constraints
--------------------------------

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
  Empty or malformed selectors, duplicate indices within a selector,
  descending ranges, and zero strides are errors.
* ``<low>``, ``<high>`` — floating-point bounds.  ``high`` must be
  strictly greater than ``low``.
* an optional shape tag ``LIN`` (default), ``LOG`` (requires
  ``low > 0``; the optimiser works in ``ln x``) or ``ASINH <c>``
  (``asinh(x/c)``: linear with slope ``1/c`` near zero, logarithmic
  beyond ``|x| ~ c``; the tool for sign-crossing ranges that span
  several decades).  See :ref:`search_space_scaling`.  Any other trailing
  token is an error (stray tokens used to be silently ignored).

``OPTIM`` lines may be repeated for the same key with different indices.

Dimension order.  The optimiser's parameter vector is ordered by key —
alphabetically, byte-wise, so ``CGRID2B`` comes before ``RCUT2B`` and
``SGRID2B`` — and then by index within the key, whatever the order of
the ``OPTIM`` lines.  That order defines the columns of
``params.tadah`` / ``best_params.tadah``, the rows of
``hpo_search_space.tsv`` and the ``dim`` numbers in ``hpo_run.log``.
For example ``OPTIM SGRID2B (2) …`` followed by ``OPTIM CGRID2B (2) …``
and ``OPTIM CGRID2B (1) …`` gives the order ``CGRID2B[1]``,
``CGRID2B[2]``, ``SGRID2B[2]``.

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
side (where the physics permits) and rerunning from the best potential
with ``METHOD WARM best_pot.tadah`` in the ``INIT`` block.

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
---------------------------------------

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
authoritative.

Bump support on hard cutoffs (``BUMPGATE``).  On a hard-cutoff
(``Cut_Dummy``) channel every compact bump must end inside the cutoff,
``c + w/η ≤ RCUT``; otherwise the truncated overhang is an energy and
force step at the cutoff.  The root key ``BUMPGATE`` chooses how this is
enforced over the ``OPTIM`` boxes of the bump centres and widths:

``BUMPGATE CANDIDATE`` (default)
   Startup fails only when a bump's box contains *no* feasible point.
   Otherwise the full boxes are searched and every candidate is tested
   before any training work; a violating candidate returns a failure
   loss (``FAILSCORE``, or a graded band-D value with
   ``FAILSCORE_GRADING ON``, :ref:`failscore_grading`) and can never
   become the best.  The journal says which bumps the gate is armed for.
``BUMPGATE STRICT``
   The legacy startup check: the worst box corner (largest centre,
   smallest width) must already be feasible, so no candidate is ever
   rejected at run time (useful for strict ``MAXEVAL`` budgeting or
   reproducing old runs).

The seed potential's own bumps must fit in both modes.  Environment
override: ``TADAH_BUMPGATE=CANDIDATE|STRICT``.

.. _hpo_fixweight:

Pinning and searching linear coefficients
-----------------------------------------

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

Misconfigurations (lone keys, size mismatches, out-of-range indices,
missing loss sources, incomplete pin coverage without a ``DBFILE``) are
rejected at startup with actionable messages.

With ``FIXINDEX`` present, analytical/HYBRID gradient strategies fall
back to finite differences for all parameters: the analytical chain
assumes the unconstrained LS solution map, which pinning invalidates.
Derivative-free algorithms (e.g. ``NLOPT LN_BOBYQA``) are a natural
choice for coefficient searches.

.. _performance_constraints:

PC_* — performance constraints
------------------------------

Energy / force / stress validation metrics
..........................................

Validation-set metrics are evaluated on every ``DBFILE`` listed in the
``--validation`` file.  The global form takes one target and weight and
applies the loss to the arithmetic average of that metric across datasets:

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

Individual validation targets and weights
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``PC_ERMSE`` and ``PC_FRMSE`` also accept indexed declarations:

.. code-block:: none

   PC_ERMSE (indices) <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]
   PC_FRMSE (indices) <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]

Indices start from **1** and refer to ``DBFILE`` order in the validation
configuration, not the training configuration. Selectors use the same syntax
as ``OPTIM``: lists ``(1,2,3)``, inclusive ranges ``(1-3)``, strides
``(1-5:2)``, and combinations with whitespace ``(1, 3-7:2)``.
Malformed selectors, zero indices or strides, descending ranges, duplicate
indices, and indices beyond the validation list are errors.

For example, with three validation datasets:

.. code-block:: none

   # config.val
   DBFILE bulk.db
   DBFILE surfaces.db
   DBFILE liquid.db

   # config.hpo
   LOSS L2
   PC_ERMSE (1,2) 5.0  1.0
   PC_ERMSE (3)   10.0 0.5
   PC_FRMSE (1)   0.05 2.0
   PC_FRMSE (2-3) 0.10 1.0

ERMSE targets are in **meV/atom**; FRMSE targets are in **eV/Å**. Targets
must be finite, and weights must be finite and nonnegative. An explicit
zero weight satisfies coverage and contributes zero loss; its dataset still
counts in the average. Weights are not normalized by their sum.

For each enabled metric, choose either one global line or disjoint indexed
lines covering **every** validation dataset. Mixing forms for the same
metric, repeated global lines, overlapping selections, and missing targets
or weights are errors. ERMSE and FRMSE may choose different forms, and either
metric may be omitted entirely. Empty validation datasets, including those
emptied by validation filters, are rejected without renumbering later files.
Indexed declarations are not supported for the other validation metrics.

For measured dataset metrics :math:`x_i` and :math:`N` validation datasets,
the metric contribution is:

.. math::

   C_{\mathrm{global}} = \operatorname{LOSS}\!\left(
      \frac{1}{N}\sum_i x_i,\ t,\ w\right),
   \qquad
   C_{\mathrm{indexed}} = \frac{1}{N}\sum_i
      \operatorname{LOSS}(x_i,\ t_i,\ w_i).

Each metric contributes **one term** to the existing total-loss average,
alongside the other enabled constraints. Dataset sizes do not change this
averaging. Even identical indexed targets and weights can differ from the
global form for nonlinear losses such as L2: averaging losses differs from
taking the loss of an average. The forms agree for a single dataset.

The run journal and potential header identify the form and the resolved
indexed assignments. In ``loss.tadah`` and ``best_loss.tadah``, indexed
``L(ERMSE)[avg]`` / ``L(FRMSE)[avg]`` columns are the mean of the corresponding
dataset losses. For the global form they retain the loss of the average
metric. Raw metric averages in ``stats.tadah`` and ``outvar.tadah`` are
unchanged.

Physics-informed constraints (PC_LAMMPS)
........................................

``PC_LAMMPS`` runs a regular LAMMPS script against each trial potential
and feeds one or more LAMMPS *equal-style* variables back into the
loss:

.. code-block:: none

   PC_LAMMPS --script in.mysim \
             --varloss myVar       0   100 \
             --varloss myOtherVar 145    10

A long ``PC_LAMMPS`` line may be split with a trailing back-slash
(``\``).  Tadah!MLIP creates one ``LAMMPS_NS::LAMMPS`` instance per
script per worker slot — inside a persistent worker process under the
default ``PC_ISOLATION PROCESS`` — and reuses it across iterations (the
script's own ``clear`` directive resets simulation state between runs,
so existing scripts work unmodified).  Each ``--varloss`` variable is one
term of the loss (:ref:`nested_fitting`).

``PC_LAMMPS`` and ``PC_OBSERVABLE`` are mutually exclusive: an HPO
configuration may use one or the other, not both.  ``PC_LAMMPS`` also
rejects every gradient-based optimiser except ``DLIB:BFGS``
(:ref:`strategy_block`), and any ``GRAD_STRATEGY`` line.

Each line captures the root ``LOSS``, ``FAILSCORE`` and
``PC_SCRIPT_TIMEOUT`` in force when it is read, so declare those keys
first.

Required options
~~~~~~~~~~~~~~~~

``--script <file>``
   LAMMPS input file containing the variable definitions.

``--varloss <name> <target> <weight> [<loss> [<param>]]``
   The variable ``<name>`` (defined inside the LAMMPS script) is read
   at the end of the run and combined with ``<target>`` and
   ``<weight>`` via the root ``LOSS``.  An optional loss name (and, for
   ``HUBER``/``TUKEY``, an optional parameter) overrides the loss for
   this variable only, with the same defaults and validation as the
   root ``LOSS`` (:ref:`loss_functions`).

.. _pc_lammps_options:

Additional options
~~~~~~~~~~~~~~~~~~

``--invar <name> <value>``
   Inject a variable (equivalent to LAMMPS' ``-var`` flag), letting the
   same script be reused for several structures or pressures.

``--outvar <name>``
   Record the variable in ``outvar.tadah`` without contributing to the
   loss.  Useful for diagnostic plots.

``--failscore <value>``
   The value substituted for each of this script's ``--varloss`` /
   ``--outvar`` variables when the simulation fails (crash, timeout,
   out-of-window hot value, …); the substituted value then passes
   through the loss against the target like a measured one.  Default:
   the root ``FAILSCORE`` in force when the line is read.  The ceiling
   that clips the *whole* evaluation is always the root ``FAILSCORE``.

``--maxtime <seconds>|off``
   Per-script override of the root ``PC_SCRIPT_TIMEOUT`` (see
   :ref:`pc_lammps_execution`).

``--ncpu`` (removed)
   Gave the number of MPI ranks for this script in the legacy MPI build,
   removed in 1.5.1.  A line that contains ``--ncpu`` is rejected at
   startup, with or without a value:

   .. code-block:: none

      Error on line 12: --ncpu was removed in 1.5.1 (legacy MPI build); parallelism over PC_LAMMPS scripts is THREADS / PC_ISOLATION. Delete --ncpu from this line.
      Line: PC_LAMMPS --script in.elastic --varloss C11 250 1 --ncpu 8

   Scripts run in parallel over the ``THREADS`` worker slots, each slot's
   LAMMPS instances in their own worker process under the default
   ``PC_ISOLATION PROCESS`` (:ref:`pc_lammps_execution`,
   :ref:`performance_parallelism`).  ``PC_LAMMPS`` lines keep their
   configuration order everywhere: in the ``outvar.tadah`` and
   ``loss.tadah`` columns, in the journal and in the potential header.

Any other token on the line is passed to LAMMPS as a command-line
argument — for example ``-log run.log`` or ``-screen none`` (see
:ref:`pc_lammps_result_contract` for how HPO renames ``-log`` /
``-screen`` files per worker).

``--hotvar <name> <min> <max> [<initial>]``
   Declare this ``PC_LAMMPS`` line as *hot*: a two-phase evaluator
   for parameters that other scripts depend on. Hot scripts run in
   **phase A** of each iteration (in parallel with other hot scripts
   and with the cold scripts that use no hot value), redefine ``variable <name> equal …`` at the end of
   the LAMMPS script (see :ref:`warmstart_protocol` for the
   ``equal``-style readback), and the returned value is validated
   against ``[min, max]``. Validated values are cached per
   ``(script, name)`` and propagated as ``${<name>}`` into
   the cold (no-``--hotvar``) ``PC_LAMMPS`` scripts that use it, which
   run in **phase B**.
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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

On indexed ``PC_ERMSE`` / ``PC_FRMSE`` lines, the flags apply independently
to each selected dataset; any triggered dataset skips downstream observable
and LAMMPS work. ``--skip-rel`` compares against that dataset's own best
observed value for the metric. A zero loss weight does not disable a filter.
On global lines the filters continue to act on the average metric. Histories
are shared between evaluation threads within a run and start empty for every
run (HPO runs are not resumable; see :ref:`hpo_potential_archives`).
Diagnostics name the triggering metric, dataset index, and filename.

.. _pc_lammps_result_contract:

Result contract for user scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

.. _pc_lammps_execution:

Execution, isolation and timeouts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Four root keys control how ``PC_LAMMPS`` scripts are executed:

``PC_ISOLATION THREAD|PROCESS`` (default ``PROCESS``)
   ``PROCESS`` runs each worker slot's LAMMPS instances in a persistent
   child process, so concurrent instances never share LAMMPS' process
   globals; model training and prediction stay in the main process.
   ``THREAD`` runs the instances in-process; it is a deprecated fallback
   and logs a warning.  The worker processes ignore ``SIGINT``,
   ``SIGTERM`` and ``SIGUSR2``: ``tadah`` stops the run and ends them
   (:ref:`hpo_stopping`).  The MPI-parallel build (``TADAH_HPO_MPI``,
   :ref:`tadah_anneal_mpi`) cannot use worker processes: there the default
   becomes ``THREAD`` (noted in the journal), and an explicit
   ``PC_ISOLATION PROCESS`` is an error.  Environment override:
   ``TADAH_PC_ISOLATION``.

``PC_SCRIPT_TIMEOUT <seconds>|off`` (default ``off``; legacy alias: root ``MAXTIME``)
   A soft wall-clock limit per script, injected as LAMMPS
   ``timer timeout`` after every ``clear`` (a script that issues its
   own ``timer`` command is left alone).  A timed-out script counts as
   failed.  Like ``LOSS``, the value is captured by each ``PC_LAMMPS``
   line when it is read; ``--maxtime`` overrides it for one line.  If
   both spellings are given the later line wins and a warning is
   logged.  Do not confuse it with ``MAXTIME`` *inside* the
   ``OPTIMIZER`` block, which limits the whole optimiser run.

``PC_WORKER_TIMEOUT <seconds>`` (default ``0``; ``PROCESS`` mode only)
   The hard deadline after which a worker process running one script is
   killed.  ``0`` derives it from the script's own timeout
   ``t`` as :math:`\max(3t,\ t + 120\,\mathrm{s})`, and applies no
   deadline to scripts whose timeout is ``off``; a positive value is
   used for every script.  Must be a finite number ``≥ 0``.
   Environment override: ``TADAH_PC_WORKER_TIMEOUT``.

``BUMPGATE CANDIDATE|STRICT``
   Not a ``PC_LAMMPS`` setting as such, but it decides whether a
   candidate reaches the simulations at all; see :ref:`hpo_knot_rcut`.

The startup journal prints the effective soft and hard limits.

.. _pc_observable:

Typed observables (PC_OBSERVABLE)
.................................

``PC_OBSERVABLE`` evaluates a physical property *in process*, without
LAMMPS, with a closed-form derivative with respect to the model
weights.  Each line is one loss term.  It cannot be combined with
``PC_LAMMPS`` in the same configuration, and ``GRAD_STRATEGY NUMERICAL``
is rejected with it.  The only kind at present is ``cohesive_energy``:

.. code-block:: none

   PC_OBSERVABLE cohesive_energy --species Ta --target 8.10 --weight 100 \
                 --hotvar a0 2.8 3.8 3.30

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Flag
     - Meaning
   * - ``--species <X>`` (required)
     - Element of the bcc crystal.
   * - ``--target <E>`` (required)
     - Target cohesive energy in eV/atom, positive for a bound crystal
       (:math:`E_\mathrm{coh} = E_\mathrm{isolated} - E_\mathrm{bulk}/N`).
   * - ``--weight <w>`` (required)
     - Weight of the term.
   * - ``--hotvar a0 <lo> <hi> [<init>]`` (required)
     - Lattice-constant window of the relaxation, and its optional
       starting value (default: the midpoint).  Only ``a0`` is supported.
   * - ``--lattice bcc``
     - Crystal structure; only ``bcc`` is supported (default).
   * - ``--supercell <nx> <ny> <nz>``
     - Supercell size (default ``4 4 4``).
   * - ``--loss <name>``
     - Loss of this term (default ``L2``; ``HUBER``/``TUKEY`` use their
       default parameter).
   * - ``--maxiter <n>``
     - Relaxation iteration cap (default ``5000``).
   * - ``--maxeval <n>``
     - Cap on energy/force evaluations of the relaxation (default ``5000``).
   * - ``--etol <x>``
     - Energy convergence tolerance (default ``0``: force-only convergence).
   * - ``--max-wall-time <s>``
     - Wall-clock cap per relaxation in seconds (default ``0``: none).
   * - ``--fmax-converged <f>``
     - Largest residual force (eV/Å) accepted as converged (default ``0.1``).
   * - ``--unconverged const <V>`` / ``--unconverged force-scaled <slope>``
     - Value reported when the relaxation does not converge: the constant
       ``V`` (default ``const 100``), or ``target + slope × fmax`` which
       keeps a slope toward convergence.

A term whose evaluation fails (divergent relaxation, non-finite value)
contributes ``FAILSCORE``.

.. _failscore_grading:

Failure handling — graded FAILSCORE
...................................

What problem this solves
~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

``PC_WORKER_TIMEOUT`` / ``PC_SCRIPT_TIMEOUT`` / per-sim ``--maxtime``
   Unchanged (:ref:`pc_lammps_execution`), but they *interact*: a killed
   hung run is graded by ``S`` (band C below), so with a probe defined
   even timeouts become directional instead of flat.

Order of firing within one iteration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

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
     - probe gate fired; potential build crashed; skip caps
     - ``S`` (probe) / metric excess (caps)
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
~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
~~~~~~~~~~~~~~~~~~~~~~

Level 0 (just ``FAILSCORE_GRADING ON``): hot-window rejections and
partial failures become graded automatically — no script changes, and
the worst optimiser pathologies (plateau death, step-size collapse)
disappear.  Level 1 (one ``--failvar`` line per script that has a
convergence check): minutes per script.  Level 2 (the probe): one
script, once, assembled from the same hinge idioms as the ``pen_*``
observables.  The only knob that can *change your objective* if set
carelessly is the probe ``--gate`` — leave it off until you have
calibrated ``S``.

OPTIMIZER block — common attributes
-----------------------------------

The driver-attribute keys below are accepted wherever an optimiser is
being configured: directly inside ``OPTIMIZER`` (short form), at the top
level of ``STRATEGY`` (for ``TYPE PRESTAGE`` this is the refinement
optimiser), and inside ``EXPLORE``, ``REFINE`` and ``INNER`` sub-blocks
(``INNER`` takes every key but ``SEED``, :ref:`hpo_inner_block`).
Each backend rejects the attributes it cannot honour at startup, naming
the key.

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Key (type)
     - Description
   * - ``LIB`` (string)
     - Optimisation library: ``TADAH``, ``ASA``, ``NLOPT``, ``DLIB``,
       ``CERES``.
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
     - Random seed of stochastic optimisers (default: drawn from
       ``std::random_device``, so a run without ``SEED`` is not
       reproducible).  ``DLIB:BFGS`` rejects it; ``DLIB:GFS`` is
       deterministic and ignores it with a warning.  ``MULTISTART`` and
       the ``PRESTAGE`` refinement run start *i* with ``SEED + i``, and
       each ``RESTART`` round runs with a seed derived from it
       (:ref:`restart_decorator`).  Inside an ``INIT`` block the same key
       seeds the stochastic init methods instead; of the optimisers, only
       NLopt reads it, and only when its block has no ``SEED`` of its
       own.  An ``INNER`` block takes no ``SEED`` (startup error): NLopt
       has one random stream per run, seeded by the enclosing block, and
       MLSL/AUGLAG draw their ``INNER`` search from it
       (:ref:`hpo_inner_block`).
   * - ``VECTOR_ARR`` (uint)
     - Storage size required by some NLOpt algorithms.
   * - ``THREADS`` (uint)
     - Width of the shared per-rank worker pool that runs the
       ``PC_LAMMPS`` fan-out and MLIP train/predict for every backend
       (default ``1``; ``ANNEAL_PT``/``ANNEAL_PA`` also cap concurrent
       replicas by it).  Only the value at the ``OPTIMIZER`` /
       ``STRATEGY`` level sizes the pool; ``THREADS`` inside ``EXPLORE``,
       ``REFINE`` or ``INNER`` does not resize it.
   * - ``GRAD_STRATEGY`` (string)
     - How gradient-based backends obtain gradients: ``NUMERICAL``
       (default; finite differences, see ``PARAM fd_step`` /
       ``fd_kernel``), ``ANALYTICAL`` or ``HYBRID`` (analytical where a
       derivative is registered, finite differences elsewhere).  The
       analytical modes are experimental.  Rejected when the
       configuration has ``PC_LAMMPS`` lines; ``NUMERICAL`` is rejected
       with ``PC_OBSERVABLE``.
   * - ``PARAM`` (string value)
     - Algorithm-specific key/value (e.g. ``PARAM fd_step 1e-3``).  Keys
       understood by the finite-difference gradient of every
       gradient-based backend: ``fd_step`` (step as a fraction of the
       internal coordinate, default ``1e-6``) and ``fd_kernel``
       (``inhouse`` — central differences in the internal coordinates,
       the default under ``OPTIM_SCALE AUTO`` — or ``ceres`` — Ceres'
       numeric differentiation, the default under ``OPTIM_SCALE NONE``;
       needs a build with ``TADAH_ENABLE_CERES``).

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
     - The NLopt catalogue — globals (``GN_*``, ``G_MLSL*``, …) and
       locals (``LN_*``, ``LD_*``); see the `NLOpt algorithm catalogue
       <https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/>`_ for
       the full list.  Gradient-using algorithms (``GD_*`` / ``LD_*``)
       obtain their gradient from the shared finite-difference seam also
       used by ``DLIB:BFGS`` and the Ceres drivers; like every
       gradient-based backend except ``DLIB:BFGS`` they are rejected
       when the configuration has ``PC_LAMMPS`` lines.

       .. note::

          With the default ``GRAD_STRATEGY NUMERICAL`` every
          gradient-using algorithm (``LD_*``, ``GD_*``, ``DLIB:BFGS``,
          all CERES drivers) obtains its gradient by finite differences
          (``PARAM fd_step``); for ``DLIB:BFGS`` and the CERES drivers
          every difference evaluation counts toward ``MAXEVAL``.  Analytical gradients (``GRAD_STRATEGY
          ANALYTICAL``/``HYBRID``) are experimental.
   * - ``DLIB``
     - ``BFGS`` — local, box-constrained quasi-Newton with a
       finite-difference gradient; the only gradient-based optimiser
       accepted with ``PC_LAMMPS``.  ``GFS`` — the dlib MaxLIPO+TR
       global *Global Function Search*; consumes every ``INIT`` point.
       See :ref:`dlib_param_keys`.
   * - ``CERES``
     - ``LBFGS`` (limited-memory BFGS, Wolfe line search), ``WOLFE``
       (full BFGS, Wolfe line search), ``NLCG`` (nonlinear conjugate
       gradient), ``STEEPEST_DESCENT`` (Armijo line search). All four
       use Ceres' ``GradientProblemSolver`` with a finite-difference
       gradient (``PARAM fd_step``; the kernel follows ``PARAM
       fd_kernel``).  They need a build with ``TADAH_ENABLE_CERES``
       (the default) and are rejected with ``PC_LAMMPS``.  The Ceres NLLS
       family (``LEVENBERG_MARQUARDT``, ``DOGLEG``, ``SUBSPACE_DOGLEG``)
       is **deferred** — mathematically degenerate over a scalar HPO
       loss; it will be enabled in a later phase together with
       residual-mode loss exposure. Ceres optimisers do **not** enforce
       box constraints; use ``DLIB:BFGS`` or ``NLOPT:LD_LBFGS`` for
       bounds-enforced gradient optimisation.

CERES PARAM keys
................

The four CERES algorithms accept these algorithm-specific ``PARAM``
entries (all keys are optional unless noted):

================== =================================================================================
Key                Effect
================== =================================================================================
``fd_step``        Finite-difference step (default ``1e-6``).  In-house kernel: central difference
                   with step ``fd_step * max(|u|, 1)`` in internal coordinates ``u`` — a fraction of
                   the box width under ``OPTIM_SCALE AUTO``.  Ceres kernel: relative step.
``fd_kernel``      ``inhouse`` (default under ``OPTIM_SCALE AUTO``) or ``ceres`` (default under
                   ``OPTIM_SCALE NONE``); see the ``PARAM`` row of the attribute table.
``diff_method``    Ceres kernel only: ``0`` = CENTRAL (default), ``1`` = FORWARD, ``2`` = RIDDERS.
                   RIDDERS extrapolates assuming a smooth Taylor expansion; not recommended near
                   ``FAILSCORE`` plateaus or for noisy loss components.
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

Any other ``PARAM`` key is rejected.  CERES also rejects ``STEP``,
``MAXTIME``, ``STOPVAL``, ``FTOL_ABS``/``XTOL_ABS`` (use ``FTOL_REL`` →
``function_tolerance`` and ``XTOL_REL`` → ``parameter_tolerance``),
``POPULATION``, ``VECTOR_ARR``, ``THREADS``, integer dimensions and
``INNER`` blocks.

.. _dlib_param_keys:

DLIB:BFGS PARAM keys
....................

``DLIB:BFGS`` runs dlib's box-constrained BFGS with the same
finite-difference gradient seam as the CERES drivers; gradient
evaluations count toward ``MAXEVAL`` through a shared counter.

================== =================================================================================
Key                Effect
================== =================================================================================
``fd_step``        Finite-difference step (default ``1e-6``), as for CERES above.
``tol_gradinf``    Stop when the gradient's infinity norm falls below this value (> 0).  Mutually
                   exclusive with ``FTOL_ABS``; without either the run stops when the loss decrease
                   per iteration falls below ``FTOL_ABS = 1e-7``.
================== =================================================================================

Any other ``PARAM`` key is rejected, and so are ``FTOL_REL``, ``XTOL_REL``,
``XTOL_ABS``, ``SEED``, ``VECTOR_ARR``, ``STEP``, ``MAXTIME``,
``STOPVAL``, ``POPULATION``, integer dimensions and ``INNER`` blocks.
``MAXEVAL`` is enforced by HPO (dlib's own stop rules count iterations).
``DLIB:GFS`` takes ``MAXEVAL`` and ``FTOL_ABS`` (its solver epsilon,
default ``1e-3``); it is deterministic, so ``SEED`` has no effect.

INIT block — seeding strategies
-------------------------------

Outside an ``INIT { ... } ENDINIT`` block the optimiser starts at the
single point taken from the values next to your ``OPTIM`` lines (the
implicit ``CONFIG`` seed).  Inside an ``INIT`` block you may declare a
*chain* of strategies that produces multiple starting points.

Chain entries
.............

Each ``METHOD <method> [args]`` line appends one entry to the chain.
``STRATEGY <method>`` is accepted as an alias (older configurations use
it), but ``METHOD`` is the canonical spelling: at the ``OPTIMIZER``
level ``STRATEGY`` opens a block, so reusing it inside ``INIT`` reads
ambiguously.  The chain is resolved into a flat list of starting points
before the optimiser runs.

.. code-block:: none

   OPTIMIZER
     INIT
       METHOD CONFIG
       METHOD LHS 8
       SEED 42
     ENDINIT
     STRATEGY
       ...
     ENDSTRATEGY
   ENDOPTIMIZER

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
........................................

.. list-table::
   :header-rows: 1
   :widths: 22 16 22 40

   * - Key
     - Type
     - Default
     - Effect
   * - ``METHOD`` (alias ``STRATEGY``)
     - repeatable
     - single ``CONFIG`` point when no ``METHOD`` line is given
     - Append an entry to the chain (see table above).
   * - ``K_TOTAL``
     - uint
     - sum of strategies' default K
     - Cap on total starting points produced.
   * - ``SEED``
     - uint
     - ``std::random_device``
     - Seed for the stochastic methods (RANDOM / LHS / SOBOL).  An NLopt
       optimiser without a ``SEED`` of its own also takes its random seed
       from it, and so does the search of its ``INNER`` block; its own
       ``SEED`` takes precedence.  Every other optimiser is seeded only by
       the ``SEED`` of its own block (``OPTIMIZER`` in the short form,
       ``STRATEGY``, ``EXPLORE``, ``REFINE``).
   * - ``WARM_ON_MISMATCH``
     - ``warn`` / ``error`` / ``skip``
     - ``warn``
     - What to do when ``WARM`` cannot fill every ``OPTIM`` key.

There is no ``INCLUDE_CONFIG`` keyword.  The in-config point is part of
the chain only when ``METHOD CONFIG`` is listed explicitly; an
``INIT`` block with no ``METHOD`` line (or no ``INIT`` block at all)
resolves to a single ``CONFIG`` point.  The removed ``INCLUDE_CONFIG``
and ``INIT_INCLUDE_CONFIG`` keywords are rejected with a message
pointing at ``METHOD CONFIG``.

Two implementation details affect reproducibility:

* The chain is resolved in the exact order it is written — no entry is
  reordered.  To make the warm point iter 0, list ``METHOD WARM``
  first in the ``INIT`` block.
* When the writer that produced the ``WARM`` file dumped
  ``# HOT_A0 <script> <a0>`` lines, ``WARM`` reads them and primes the
  warm-start cache (see :ref:`warmstart_protocol`).

How many seeds does the optimiser actually consume?
...................................................

A chain that produces more starting points than the optimiser can use
emits a warning at config time naming how many are discarded.

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

``STRATEGY TYPE MULTISTART`` runs the optimiser once per starting point,
``min(N_STARTS, number of points)`` times.  ``DLIB:GFS`` collapses to a
single call because GFS already takes every seed natively; ``TYPE
PRESTAGE`` feeds its ``TOPK`` candidates to the refinement the same
way.

.. _search_space_scaling:

Search-space scaling
....................

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
Under ``NONE`` the internal coordinates are exactly the pre-1.5 ones
(raw units, ``ln`` for ``LOG`` dims), reproducing every earlier run
byte-identically -- the escape hatch for A/B checks, also reachable
through the environment override ``TADAH_OPTIM_SCALE=NONE``.  The
default is ``AUTO``.

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

.. _strategy_block:

STRATEGY block
--------------

The ``STRATEGY { ... } ENDSTRATEGY`` block is the run-policy hub.  Its
``TYPE`` selects the dispatch shape:

================== =================================================================
``TYPE``           Effect
================== =================================================================
``PLAIN``          Single-shot run of the driver named by ``LIB``/``ALGO``.
``MULTISTART``     Run the driver ``N_STARTS`` independent times, each from a
                   different point of the ``INIT`` chain.
``PRESTAGE``       Two-stage: an ``EXPLORE { ... }`` block runs first, the top-K
                   candidates are then refined by the STRATEGY-level driver
                   (optionally written as a ``REFINE { ... }`` block).
================== =================================================================

Any ``TYPE`` can be decorated with ``RESTART`` (:ref:`restart_decorator`).
The optimiser named by ``LIB``/``ALGO`` may be local or global; the
``TYPE`` only decides how often, and from which points, it runs.

.. list-table:: STRATEGY-level keys besides the driver attributes
   :header-rows: 1
   :widths: 22 78

   * - Key
     - Effect
   * - ``TYPE``
     - ``PLAIN`` (default), ``MULTISTART`` or ``PRESTAGE``.
   * - ``LOG_HP <ratio>``
     - Automatic ``LOG`` tagging of wide positive dimensions
       (:ref:`search_space_scaling`).
   * - ``N_STARTS``, ``PARALLEL``, ``BUDGET_PER_START``
     - ``MULTISTART`` knobs (below).
   * - ``TOPK``
     - ``PRESTAGE`` knob (below).
   * - ``EXPLORE``, ``REFINE``, ``INNER``, ``RESTART``
     - Nested blocks (below).

TYPE PLAIN
..........

The simplest dispatch: a single run of ``LIB``/``ALGO``.  The driver
attribute table above lists every accepted key.  No nested blocks
other than ``RESTART`` and ``INNER`` (for NLopt MLSL/AUGLAG) are
relevant.

TYPE MULTISTART
...............

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

The number of starts is ``min(N_STARTS, number of INIT points)``; when the
``INIT`` chain yields fewer points than ``N_STARTS`` a warning says so,
so pair ``MULTISTART`` with an ``INIT`` chain of at least ``N_STARTS``
points (e.g. ``METHOD LHS 8``).  With ``SEED`` set, start ``i`` uses
``SEED + i`` so stochastic optimisers do not repeat one another; after
restart *r* of a ``RESTART``, ``SEED + r·N_STARTS + i``
(:ref:`restart_decorator`).  An NLopt start draws from that seed even
when the ``INIT`` block has a ``SEED``, which draws the start points, and
so does the ``INNER`` search of an AUGLAG start (:ref:`hpo_inner_block`).

A start that fails with an error is logged as a ``WARNING`` and the next
start runs.  Three stops end the whole ``MULTISTART`` instead, because
they concern the run rather than the start: a signal
(:ref:`hpo_stopping`), the ``RESTART`` stagnation stop (the restart runs
the ``MULTISTART`` again from the fresh points, :ref:`restart_decorator`),
and a failure to save ``best_pot.tadah`` (:ref:`hpo_potential_archives`).

``MULTISTART`` rejects — for any ``N_STARTS``, including ``1`` — every
*global* backend that consumes a single starting point: every NLopt
algorithm whose name starts with ``G`` (``GN_*``, ``GD_*``,
``G_MLSL*``), ``TADAH:RANDOM``, ``TADAH:GRID``, the ``TADAH:ANNEAL``
family and ``ASA:ASA``.  Local optimisers (``NLOPT:LN_*``/``LD_*``,
``DLIB:BFGS``, CERES) and the multi-seed global ``DLIB:GFS`` are
accepted; ``DLIB:GFS`` consumes all points in one run.  Use ``TYPE
PLAIN`` to run a single-seed global once.

TYPE PRESTAGE — staged global → local
.....................................

A self-contained ``EXPLORE { ... } ENDEXPLORE`` sub-block describes the
*first* (exploration) phase; the STRATEGY-level ``LIB``/``ALGO`` drives
the *second* (refinement) phase over the top-K candidates harvested by
the EXPLORE pass.  The refinement optimiser may equally be written as a
``REFINE { ... } ENDREFINE`` block — it is optional and configures the
same STRATEGY-level optimiser.

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
                 (default 1)   forwards to refinement.  With one candidate the refinement
                               runs once from it; with more it is a multistart over the
                               candidates, so the refinement optimiser must be one
                               ``MULTISTART`` accepts, per-start ``MAXEVAL`` is split
                               from the STRATEGY-level ``MAXEVAL`` by the same rule, and
                               candidate ``i`` runs with ``SEED + i``.
================ ============= =============================================================

The ``EXPLORE`` block accepts the full driver-attribute set
(``LIB``/``ALGO``/``MAXEVAL`` / …) and an optional ``INIT`` block; it
inherits the search space automatically and, without its own ``INIT``
block, starts from the OPTIMIZER-level ``INIT`` points.  Candidates are
deduplicated so the ``TOPK`` seeds are distinct.  If the exploration
yields no candidate, the refinement runs from the OPTIMIZER-level
``INIT`` points instead.

With ``RESTART``, stagnation during the exploration ends it early and
hands its top-K to the refinement, which starts with a stagnation window
of its own; stagnation during the refinement restarts the whole
``PRESTAGE``, exploration first.  The restarted exploration starts from
the restart's fresh points, unless the ``EXPLORE`` block has an ``INIT``
block of its own.  In restart *r* a seeded exploration draws from
``SEED + r`` and the refinement's candidate ``i`` from ``SEED + r·TOPK + i``
of the STRATEGY-level ``SEED`` (:ref:`restart_decorator`).

LIB TADAH — the annealing family
................................

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
...........................................

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
  ``TADAH:ANNEAL_PA`` strategy.  Other drivers and ``STRATEGY TYPE
  MULTISTART`` / ``PRESTAGE`` fail before policy setup; an MPI launch of
  size one keeps the normal serial behaviour;
* a multi-rank launch rejects ``RESTART`` (:ref:`restart_decorator`):
  each rank tracks stagnation over its own evaluations, so the ranks
  would restart at different times and the run would hang in the
  driver's next collective.  With ``AFTER_STAGNATION`` and ``MAX`` both
  positive it fails before any evaluation, with ``multi-rank HPO does not
  support RESTART (…): stagnation is tracked per rank; run on one rank or
  remove RESTART``;
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
  file, updated live by every rank under a cross-process lock.  A
  complete potential replaces it for a lower loss, or for an exactly equal
  loss and a lexicographically smaller ``(rank, slot, evaluation)`` key;
* ``MAXEVAL`` is a hard global cap across seed/probe evaluation,
  population initialisation, annealing, and finalisation.  No rank-0
  re-evaluation is performed after the budget is exhausted.

``TADAH_HPO_MPI`` is the only MPI build of Tadah!MLIP: the legacy MPI
training build (``TADAH_BUILD_MPI``) was removed in 1.5.1, and
configuring with ``-DTADAH_BUILD_MPI=ON`` is an error
(:ref:`installation_hpo_mpi`).

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
-----------------

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
       METHOD SOBOL 1
     ENDRESTART
   ENDSTRATEGY

.. list-table::
   :header-rows: 1
   :widths: 22 16 62

   * - Key
     - Args / range
     - Effect
   * - ``AFTER_STAGNATION``
     - int ≥ 0 (default ``0``)
     - After this many consecutive evals without a new global best,
       abort the inner run and restart from a fresh point.
   * - ``MAX``
     - int ≥ 0 (default ``0``)
     - Cap on the number of restarts.
   * - ``METHOD`` (alias ``STRATEGY``)
     - repeatable
     - Init chain used to draw the fresh point(s) on each restart
       (the ``INIT``-block grammar, including ``K_TOTAL``).  Implicit
       ``CONFIG`` is *not* auto-prepended here: a stagnated run already
       has CONFIG in its history.  Without a ``METHOD`` line a restart
       uses the single ``CONFIG`` point.

The decorator is active only when **both** ``AFTER_STAGNATION`` and
``MAX`` are positive; with either at its default ``0`` the ``RESTART``
block has no effect.  A ``SEED`` inside ``RESTART`` is ignored: each
restart draws the seed of its ``METHOD`` chain from
``std::random_device``, so the fresh points are not reproducible.  Each
restart gets its own stagnation window — the windows do not bleed into
each other.  ``RESTART`` runs on one MPI rank only: a launch on more
than one rank rejects it at startup (:ref:`tadah_anneal_mpi`).

A restart re-runs the whole strategy from the *K* fresh points the
``METHOD`` chain draws (``K_TOTAL``, or the chain's default):

* ``PLAIN``: the optimiser starts again from the first of them (a
  multi-seed optimiser such as ``DLIB:GFS`` takes all *K*).
* ``MULTISTART``: runs again from its first start, with ``min(N_STARTS,
  K)`` starts, one per fresh point — use a ``METHOD`` with *K* ≥
  ``N_STARTS`` (e.g. ``METHOD LHS 8``) to re-run every start.  Without a
  ``METHOD`` line, a restart runs a single start, from ``CONFIG``.
* ``PRESTAGE``: only stagnation in the refinement restarts (a stagnating
  exploration hands over to the refinement early).  The exploration
  starts again from the fresh points, or from its own points again when
  the ``EXPLORE`` block has an ``INIT`` block of its own; the refinement
  then refines the new top-K.

Restart *r* (``1`` … ``MAX``) also gives every optimiser that has a
``SEED`` a seed of its own, so a seeded stochastic optimiser or
exploration draws new random numbers instead of repeating round 0's from
the new point:

* the ``STRATEGY``-level optimiser runs with ``SEED + r·S``, where *S*
  is the number of starts the strategy runs from its ``SEED``:
  ``N_STARTS`` under ``MULTISTART``, ``TOPK`` under ``PRESTAGE`` (the
  refinement's candidates) and ``1`` under ``PLAIN``.  Start *i* of the
  round, which uses the round's seed ``+ i``, gets ``SEED + r·S + i`` — a
  seed no other start of the run uses.  With ``SEED 7``, ``TOPK 3`` and
  ``MAX 2``, the refinement's candidates run with seeds 7, 8, 9, then 10,
  11, 12, then 13, 14, 15;
* an ``EXPLORE`` optimiser runs with its ``SEED + r``.

An ``INNER`` block has no ``SEED`` of its own: its search draws from the
random stream of the enclosing optimiser's run, seeded with that
optimiser's derived seed (:ref:`hpo_inner_block`).

The first round keeps every ``SEED``, so up to its first restart a run
draws the same random numbers as without ``RESTART``.  Under
``PRESTAGE`` the first round can still take another course: a stagnating
exploration hands over to the refinement early (above), where without
``RESTART`` it runs to its own stop criteria.  The journal's
``RESTART r/MAX`` line lists the round's seeds, for example ``RESTART
1/3 -- fresh init from SOBOL; SEED 43, EXPLORE SEED 1718``.  An
optimiser without ``SEED``
is unaffected.  NLopt draws from the derived seeds too, even when an
``INIT`` block also has a ``SEED``.  Only an NLopt optimiser without a
``SEED`` of its own takes its random seed from its ``INIT`` block's
``SEED``, which is not derived, so it repeats its random sequence in every
round.

``RESTART`` fires with every backend, including NLopt (also
inside the ``INNER`` local search of MLSL/AUGLAG), ``DLIB:GFS`` with any
``THREADS``, Ceres, ASA and the ``TADAH`` annealers.  Once the ``MAX``
restarts are used up, the next stagnation ends the run normally (exit
status ``0``): the journal records ``RESTART: stagnation reached, max
restarts (MAX) used; stopping``, and its trailer reads ``STOPPED --
stagnation, max restarts (MAX) used``.

NOISE block
-----------

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
                                 internal coordinates.  Applies to any gradient-based
                                 backend configured at the STRATEGY level (ignored with a
                                 message otherwise), and only when ``PARAM fd_step`` is
                                 not set explicitly.  Bareword ``AUTO`` parses as
                                 ``true``; default off.
``CALIBRATION``    int (0 or ≥2) Repeat the seed evaluation ``N`` times to surface
                                 the objective's noise floor and emit a suggested
                                 ``FTOL_REL``.  ``0`` disables; ``1`` is rejected as
                                 statistically meaningless.
================== ============= =================================================================

.. _hpo_inner_block:

INNER block — NLopt MLSL / AUGLAG sub-optimiser
-----------------------------------------------

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
``STEP`` / ``VECTOR_ARR`` / ``THREADS`` / ``GRAD_STRATEGY`` / ``PARAM``)
are valid inside ``INNER``.  With MLSL/AUGLAG an outer ``STEP``
overrides an ``INNER`` ``STEP`` (NLopt forwards the outer initial step
to the local optimiser); a startup warning says so.

``SEED`` is not valid inside ``INNER``.  MLSL and AUGLAG run the
``INNER`` search inside the enclosing block's NLopt run, and NLopt has
one random stream per run.  The enclosing block's ``SEED`` seeds it (or,
without one, the enclosing block's ``INIT``-block ``SEED``), and every
random number of the run comes from it: G_MLSL's sample points and the
draws of the ``INNER`` optimiser, such as a ``GN_CRS2_LM`` population
under AUGLAG.  An ``INNER`` ``SEED`` could only reseed that stream in
place of the enclosing block's seed, so the startup check rejects it and
names the line.  This configuration:

.. code-block:: none

   OPTIMIZER
     INIT
       METHOD LHS 3
     ENDINIT
     STRATEGY
       TYPE MULTISTART
       LIB NLOPT
       ALGO AUGLAG
       N_STARTS 3
       MAXEVAL 300
       SEED 10
       INNER
         LIB NLOPT
         ALGO GN_CRS2_LM
         SEED 20
       ENDINNER
     ENDSTRATEGY
   ENDOPTIMIZER

stops at startup: ``tadah hpo`` prints this error on stderr and exits
with status ``1``:

.. code-block:: none

   HPOEngine error: Error on line 15: SEED is not accepted inside an INNER block: NLopt draws the INNER search from the enclosing run's random stream; set SEED on the enclosing STRATEGY block
   Line:       SEED 20

The message names ``EXPLORE`` or ``REFINE`` when the ``INNER`` block is
there.  Set ``SEED`` on that block instead.  Here, deleting ``SEED 20``
leaves ``SEED 10``: the three starts run with seeds 10, 11 and 12, each
with a ``MAXEVAL`` of 100 (``MAXEVAL 300`` split over ``N_STARTS``), and
each start's ``GN_CRS2_LM`` search draws a population of its own from
its seed.  In the same way the ``PRESTAGE`` refinement runs candidate
*i* with ``SEED + i``, and each ``RESTART`` round runs with a derived seed
(:ref:`restart_decorator`) that its ``INNER`` search draws from too.  An
``INIT`` block is not valid inside ``INNER`` either; MLSL picks its local
starting points itself.

.. _hpo_load_transforms:

Load-time data transforms
-------------------------

The training-side load transforms (``LSCALE``, ``ESHIFT*``,
``EFILTER`` / ``FFILTER``, ``EWEIGHT_TEMP``, ``WDBFILE`` /
``WDBFILE_AUTO``, ``ZERO_COM_FORCE``) are documented under
:ref:`load_transforms`; place them in ``--config`` (the training
configuration). HPO applies them on every inner training run.

Validation-set sanitisation
...........................

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
------------

Unless you change them, every artefact produced by the outer optimiser
is written to **the directory in which you started** ``tadah hpo``.

Main log files
..............

Each file begins with a ``#`` header describing its columns.  Eligible rows
follow the ``OUTPUT`` cadence.  The first two columns are always the
evaluation (iteration) number and the slot (``1`` for serial drivers; the
replica or walker for ``ANNEAL_PT``/``ANNEAL_PA``).

When embedding successive HPO runs in one process, flush each host before
starting the next. Result batches use the current run's output files,
including when the working directory changes between runs.

=====================  =====================================================================
File                   Columns
=====================  =====================================================================
``loss.tadah``         All enabled individual loss terms, then the total loss (their mean).
``params.tadah``       One column per optimised dimension, labelled ``KEY[n]``, in the
                       optimiser order: by key, then by index (see ``OPTIM``) — not the
                       order of the ``OPTIM`` lines.  Values are in real units.
``outvar.tadah``       Per-evaluation wall time in milliseconds, enabled validation metrics,
                       LAMMPS ``--outvar``/``--varloss`` values, and typed-observable values.
``stats.tadah``        Per-validation-dataset values and their average for each enabled metric.
=====================  =====================================================================

Cadence and formatting:

============ ========= =================================================================
Key          Default   Effect
============ ========= =================================================================
``OUTPUT N`` ``1``     Write a main-file row when the iteration is divisible by ``N``;
                       ``N ≥ 1`` (``0`` is rejected — use a large value to write little).
``DIGITS d`` ``6``     Decimal digits in scientific-notation columns; ``1 ≤ d ≤ 15``.
``QFLUSH N`` ``10``    Queued completed results that trigger a writer-thread drain;
                       the end of the run drains the remainder
                       (:ref:`hpo_stopping`).
============ ========= =================================================================

Best-improvement histories
..........................

Every new minimum is numbered by an improvement ordinal ``k = 1, 2, …``
and, following the ``BOUTPUT`` cadence, contributes a row to the
companion history files.  Under multi-rank PT/PA, the suffixed files
record that rank's improvements; the unsuffixed ``best_pot.tadah`` is
the canonical cross-rank winner:

* ``best_loss.tadah``
* ``best_params.tadah``
* ``best_outvar.tadah``
* ``best_stats.tadah``

These files append one row per qualifying improvement; they are histories,
not single-row snapshots.  They have the same columns as their main
counterparts except that the **first column is the improvement ordinal**
``k``, not the evaluation number (the evaluation that produced a best is
recorded in ``hpo_run.log`` and in the header of ``best_pot.tadah``).

.. list-table::
   :header-rows: 1
   :widths: 22 14 64

   * - Key
     - Default
     - Effect
   * - ``BOUTPUT N``
     - ``1``
     - A cadence over new bests: write the ``best_*`` history row of
       every ``N``-th improvement (``k`` divisible by ``N``).  ``N ≥ 1``;
       ``0`` is rejected.  ``best_pot.tadah`` is updated on every
       improvement regardless.

.. _hpo_potential_archives:

Potential archives
..................

The current global best potential is saved live to the canonical
``best_pot.tadah``.  Every process-local improvement synchronously
competes for this file under a cross-process lock, before the evaluation
that produced it returns to the optimiser.  Candidates are
ordered by ``(loss, source rank, source slot, source evaluation)``: lower
loss wins, with the remaining fields providing an exact-loss tie-break.
The complete winning potential is
written to a unique temporary file, flushed, and atomically renamed, so
an abrupt ``SIGKILL`` leaves either the preceding complete winner or the
new complete winner, never a torn file.  Its header records the loss,
source rank/slot/evaluation, and candidate-local hot-variable values.
A write or durability failure of the canonical file is fatal rather than
silently allowing the optimisation to continue without a durable
incumbent.  A rank-local diagnostic-mirror failure is reported but cannot
invalidate an already-durable canonical winner.

``best_pot.tadah`` therefore holds the incumbent — the lowest-loss
evaluation that completed — at all times, however the run ends: stop
criteria, a signal, an error or ``SIGKILL`` (:ref:`hpo_stopping`).  The
one exception is the error that ends a run when ``best_pot.tadah``
cannot be saved: the evaluation whose potential could not be saved has
its ``loss.tadah`` row, but ``best_pot.tadah`` and ``best_params.tadah``
keep the previous incumbent.  ``best_pot.tadah`` carries the complete
configuration, its header names the evaluation that produced it, and it
is the authoritative record of the result.
``best_params.tadah`` and the other history files are appended by the
result writer in ``QFLUSH`` batches, so while the run is going they
trail ``best_pot.tadah`` by the rows still queued.  Every ending except
``SIGKILL`` (or a crash) writes the queued rows before the process exits,
so the history files then have one row per completed evaluation, and the
``best_params.tadah`` row with the highest improvement ordinal is the
incumbent's.  That is the last row unless evaluations ran concurrently:
the rows are written in evaluation order, so a later improvement can
come before an earlier one.  After ``SIGKILL`` the files can lack the
rows still queued, including the incumbent's.

HPO runs are **not resumable**: there is no checkpoint to continue from,
and every run starts fresh.  Before evaluating its first candidate, a run
removes the canonical and rank-local incumbents a previous run left in the
working directory, so an earlier objective's loss cannot suppress the new
run's output (copy ``best_pot.tadah`` elsewhere first to keep it).  A
``WARM`` seed is parsed before this step, so starting a new run from an old
potential remains supported.

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
``DUMP <N> <DIR>``           off            Save the trial potential of every ``N``-th evaluation as
                                            ``<DIR>/pot_<iter>.tadah``.  ``N ≥ 1`` and ``<DIR>`` are both
                                            required (``0`` is rejected); the directory is created if
                                            needed.
``BDUMP <N> <DIR>``          off            Save every ``N``-th new best as
                                            ``<DIR>/best_pot_<k>.tadah``, ``k`` being the improvement
                                            ordinal.  Same rules as ``DUMP``.
============================ ============== =================================================================

Run log
.......

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
   -- POTENTIAL     --   2B/MB terms, cutoffs, element list
   -- LOSS FUNCTION --   targets, weights, FAILSCORE
   -- PROGRESS      --   timestamped events: bests, warnings, milestones
   -- DIAGNOSTICS   --   NaN rate, bound-proximity analysis
   -- PERFORMANCE   --   timing statistics
   -- RESULT        --   final status, best loss, best metrics

The optimizer journals documented on the algorithm pages (banners,
per-rung and per-window reports, audits, the final ``stopped:`` line)
land in PROGRESS, tagged ``INFO``/``WARNING``/``HINT``/``SUMMARY``:
``WARNING`` states something provable about this run, ``HINT`` an
empirical caution, and multi-line messages carry a ``*`` on the head
line. ``hpo_run.log`` is the canonical record of which features fired
during a run; the stdout markers in :ref:`diagnostic_markers` are the
quick-look subset.

Practical tips
..............

* Large optimisations can produce thousands of potentials; watch disk
  usage when ``DUMP`` is enabled.
* The log files are plain text and append-only.  They are tail-friendly:

  .. code-block:: bash

     tail -f loss.tadah

* ``params.tadah`` columns follow the optimiser order (by key, then
  index), which need not match the order of your ``OPTIM`` lines; read
  the columns by their ``KEY[n]`` labels.  To reproduce a candidate,
  use the potential itself (``best_pot.tadah`` or a ``DUMP``/``BDUMP``
  archive), which carries the full configuration.

.. _hpo_stopping:

Stopping a run
--------------

A run ends when the optimiser meets its stop criteria (``MAXEVAL``,
``STOPVAL``, a tolerance, ``MAXTIME``, the ``RESTART`` budget, …), when
a signal stops it, or when an error stops it.  ``best_pot.tadah`` holds
the incumbent in every case (:ref:`hpo_potential_archives`).

**Signals.** ``tadah hpo`` handles ``SIGINT`` (Ctrl-C), ``SIGTERM``
(``kill``, ``scancel``) and ``SIGUSR2`` (an advance warning before a
batch job's time limit, below).  The first one stops the run: no new
evaluation starts, the evaluations already running finish and are
recorded, and the optimiser and strategy stop with them — later
``MULTISTART`` starts, ``RESTART`` rounds and the ``PRESTAGE``
refinement do not run, and a signal during ``NOISE_CALIBRATION`` ends the
calibration.  The queued rows are then written to the history files and
the journal is closed.  A second signal does not interrupt that
shutdown; ``kill -9`` (``SIGKILL``) ends the process at once.  The
``PC_ISOLATION PROCESS`` worker processes ignore all three signals, so a
signal sent to the whole process group or job step (Ctrl-C in a
terminal, ``kill -<signal> -<pgid>``, a signal SLURM sends to a job step)
does not kill the evaluations they are running.

**Batch jobs.** SLURM's ``--signal=[B:]USR2@<seconds>`` sends ``SIGUSR2``
that many seconds before the time limit.  With ``B:`` it signals only
the batch shell, which ``SIGUSR2`` kills without passing it on: make
``tadah`` the batch shell by starting it with ``exec`` as the script's
last command.  Without ``B:``, SLURM signals the job steps: start
``tadah`` with ``srun``.

.. code-block:: bash

   #!/bin/bash
   #SBATCH --time=24:00:00
   #SBATCH --signal=B:USR2@600     # SIGUSR2 to the batch shell 10 min early
   exec tadah hpo -c config.tadah --hpotarget config.hpo --validation config.val

   # or, with  #SBATCH --signal=USR2@600  (every job step is signalled):
   # srun tadah hpo -c config.tadah --hpotarget config.hpo --validation config.val

A plain ``tadah hpo …`` line under ``--signal=B:USR2@…`` never receives
``SIGUSR2``: the signal kills the batch shell instead, and ``tadah`` gets
no advance warning.

.. list-table:: How a run ends
   :header-rows: 1
   :widths: 22 20 32 26

   * - Ending
     - Exit status of ``tadah``
     - ``Optimiser status`` in the journal's ``RESULT`` section
     - History files
   * - Stop criteria met
     - ``0``
     - The reason of the optimiser run that ended the run (under
       ``MULTISTART`` the last start, under ``PRESTAGE`` the refinement)
       where it gives one (NLopt, e.g. ``MAXEVAL_REACHED -- …``;
       ``ASA: …``), otherwise ``COMPLETED``.  When the ``RESTART``
       budget ends the run: ``STOPPED -- stagnation, max restarts (MAX)
       used``.
     - Complete.
   * - ``SIGINT``, ``SIGTERM``, ``SIGUSR2``
     - ``128`` + the signal number: ``130``, ``143``, ``140`` (Linux)
     - ``SHUTDOWN (SIGINT)``, ``SHUTDOWN (SIGTERM)``,
       ``SHUTDOWN (SIGUSR2)``
     - Complete.
   * - An error during the run (including a failure to save
       ``best_pot.tadah``)
     - ``1``
     - ``STOPPED with error: <message>`` (``STOPPED with unknown error``
       for an error that is not a C++ standard exception)
     - Complete.
   * - ``SIGKILL``, a crash, a node failure
     - Killed (a shell reports ``137`` for ``SIGKILL``)
     - None: the journal has no ``RESULT`` section.
     - Can lack the rows still queued: up to ``QFLUSH − 1``, more when
       evaluations run concurrently and a slow one holds later rows back.

"Complete" means one row per completed evaluation (subject to the
``OUTPUT`` / ``BOUTPUT`` cadences), with the ``best_params.tadah`` row of
the highest improvement ordinal belonging to the incumbent — except
after a failure to save ``best_pot.tadah``, whose evaluation has its
``loss.tadah`` row but is not the incumbent
(:ref:`hpo_potential_archives`).  If an evaluation
never delivered its result, the rows after it are written anyway at the
end of the run and the journal warns ``… stranded behind missing
iteration N``.

An error found while the run is set up, before the optimiser starts (a
configuration or data error), ends ``tadah`` with status ``1`` and a
message on stderr; there are no history rows, and the journal, if it was
opened, has no ``RESULT`` section.  An error the optimiser raises before
its first evaluation (for example an unknown ``PARAM`` of ``TADAH:GRID``)
also leaves no history rows, but the journal has a ``RESULT`` section
with ``STOPPED with error: <message>`` and zero evaluations.

In a multi-rank ``ANNEAL_PT`` / ``ANNEAL_PA`` run
(:ref:`tadah_anneal_mpi`), each rank stops on the signal it receives
itself.  A rank that has not seen it when another rank stops ends with an
error instead, status ``1`` and ``STOPPED with error: … worker failed on
another MPI rank …`` (or ``… rank 0 aborted during … calibration`` when
the signal arrives during the calibration); only the ranks that saw the
signal report ``SHUTDOWN`` and ``128`` + the signal number.  Every rank
still writes its queued rows, and ``best_pot.tadah`` holds the global
incumbent.

The run's exit status is also ``tadah``'s: a job script can tell a
completed run (``0``) from a stopped one (``130``, ``143``, ``140``) and
a failed one (``1``).

.. _warmstart_protocol:

Warm-start protocol and bad-potential early-exit
------------------------------------------------

Warm-start: ``${hot_a0}`` (or any ``--hotvar``)
...............................................

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

Bad-potential early-exit
........................

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
-------------------------

Every ``tadah hpo`` run parallelises within one process through the
shared worker pool below.  The MPI-parallel build (``TADAH_HPO_MPI``)
additionally spreads the ``TADAH:ANNEAL_PT`` / ``TADAH:ANNEAL_PA``
optimisers over MPI ranks.

Within one process (OpenMP and the worker pool)
...............................................

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

* **LAMMPS runs** — every LAMMPS instance runs on a single rank: the
  default build links a *serial* LAMMPS library, and the MPI-parallel
  build runs each instance on ``MPI_COMM_SELF``.  Parallelism is driven
  *around* LAMMPS, not inside it.

Across MPI ranks (TADAH_HPO_MPI)
................................

The MPI-parallel build distributes the replicas of ``TADAH:ANNEAL_PT``
and the walkers of ``TADAH:ANNEAL_PA`` over MPI ranks; every rank trains
its own models and runs its own ``THREADS`` pool.  Build it with
``-DTADAH_HPO_MPI=ON`` (:ref:`installation_hpo_mpi`); the requirements
and an ARCHER2 job script are in :ref:`tadah_anneal_mpi`.  Model training
itself is never distributed: the legacy MPI training build and its
``PC_LAMMPS --ncpu`` option were removed in 1.5.1.

Practical advice
................

* Size ``THREADS`` first: it is the main fan-out and train/predict CPU
  budget, including for a serial annealing chain.
* Increase ``OMP_NUM_THREADS`` only when profiling shows the residual
  OpenMP kernels matter; it does not widen ``PC_LAMMPS`` fan-out.
* Measure, do not guess: a few short test runs sweeping ``THREADS`` and
  then the residual OpenMP width will reveal the sweet spot.

.. _hpo_runtime_checks:

Audits and health monitoring
----------------------------

Three keys of the *training configuration* (``--config``) control
diagnostics that ``tadah hpo`` runs around the optimisation.
``AUDIT`` and ``HEALTH_LOG`` are stripped from every potential file HPO
writes.

``AUDIT off|warn|error`` (default ``off``)
   Before the optimiser starts, run the pre-flight audit on the training
   data: per-descriptor health checks (e.g. a fixed, off-shell ``CGRIDMB``
   that collapses the density) plus a check for *critical* descriptor
   parameters that have no ``OPTIM`` line.  ``warn`` logs the findings
   and continues; ``error`` aborts on any FAIL-level finding.  Skipped
   when there is no training ``DBFILE``.
``AUDIT_SAMPLE <n>`` (default ``256``)
   Number of training structures (deterministic sample) the audit's
   dataset scan uses; ``0`` scans all of them.
``HEALTH_LOG off|summary|full`` (default ``summary``)
   Per-evaluation health monitor of the fitted weights (non-finite
   values, collapsed or exploding blocks).  ``summary`` rate-limits its
   warnings, ``full`` reports every offending evaluation, ``off``
   disables it.

.. _hpo_env_overrides:

Environment overrides
---------------------

These environment variables override the corresponding configuration
keys without editing the files (intended for A/B comparisons and test
harnesses).  An invalid value is an error, not a silent fallback.

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Variable
     - Effect
   * - ``TADAH_OPTIM_SCALE=AUTO|NONE``
     - Replaces ``OPTIM_SCALE`` (:ref:`search_space_scaling`).
   * - ``TADAH_PC_ISOLATION=THREAD|PROCESS``
     - Replaces ``PC_ISOLATION`` (:ref:`pc_lammps_execution`); counts as
       an explicit request, so ``PROCESS`` is an error in the MPI-parallel
       build.
   * - ``TADAH_PC_WORKER_TIMEOUT=<seconds>``
     - Replaces ``PC_WORKER_TIMEOUT``; same validation.
   * - ``TADAH_FAILSCORE_GRADING=ON|OFF``
     - Replaces ``FAILSCORE_GRADING`` (:ref:`failscore_grading`).
   * - ``TADAH_BUMPGATE=CANDIDATE|STRICT``
     - Replaces ``BUMPGATE`` (:ref:`hpo_knot_rcut`).
   * - ``TADAH_HPO_FORCE_REBUILD`` (any value)
     - Rebuild the per-thread trainer from scratch on every evaluation
       instead of re-initialising the cached one (serial builds) — slower;
       a reference for checking that re-initialisation is exact.

.. _diagnostic_markers:

Diagnostic stdout markers
-------------------------

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
``[HPO] MULTISTART start i/K -- running inner optimiser (seed=…)``            Per-start; ``(seed=…)`` only when ``SEED`` is set.
``[HPO] PRESTAGE: exploration phase LIB:ALGO (MAXEVAL=…)``                    PRESTAGE phase starts.
``[HPO] PRESTAGE: top-K losses: …``                                           Snapshot quality after PRESTAGE finishes.
``[HPO] REFINEMENT: refining top-K seed(s) via LIB:ALGO``                     Outer (refinement) phase starts.
``[HPO] RESTART i/M -- fresh init from STRATEGY; SEED …``                     Stagnation watchdog fired; lists the round's derived seeds.
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
