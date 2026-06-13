.. _refit:

Refitting Tabulated EAM Potentials
==================================

``tadah refit`` converts a tabulated EAM potential (a *setfl* file, the
LAMMPS ``eam/alloy`` / ``eam/fs`` format) into a **native Tadah!
potential**.  The three tabulated functions — the pair interaction
:math:`\phi(r)`, the electron density :math:`\rho(r)`, and the embedding
energy :math:`F(\rho)` — are re-expressed as flexible basis-function
expansions and fitted by least squares.  The result is a standard
``pot.tadah`` (``D2_Blip`` two-body descriptor + ``DM_REAM`` many-body
descriptor) that works everywhere a trained Tadah! model works: with
``tadah predict``, with the Tadah!LAMMPS plugin, and — most importantly —
with the :ref:`nested fitting <nested_fitting>` module.

When to use it
--------------

A tabulated EAM is a black box: its splined tables cannot be tuned.
After ``tadah refit`` the same physics lives in a parametric Tadah!
model, so you can:

* **tune a published EAM** against your own reference data with
  ``tadah hpo`` — the command emits a ready-made HPO target file with
  ``OPTIM`` bounds around every fitted parameter;
* **use an EAM as a physically-sound starting point** for a machine
  learning potential instead of starting from random initialisation;
* **inspect and document** how well a smooth basis can represent a
  given table (faithfulness self-test, per-function inspection curves).

The refit is judged internally by a compression-aware crystal
self-test: cold-curve (energy–volume) errors of the refit versus the
original table are measured over ``a/a0`` from ``REFIT_REACH_SMIN``
(default 0.75, roughly :math:`V/V_0 \approx 0.42`) up to 1.10, and the
basis is escalated automatically until the ``REFIT_TOL_*`` tolerances
are met.

.. note::

   Only **single-element** setfl files are supported.  Multi-element
   tables are rejected at validation time.

.. _refit_quick_start:

Quick Start
-----------

All fit controls live in a configuration file; the command line carries
only the config path and verbosity.  A minimal config is one line:

.. code-block:: none

   # refit.cfg
   EAM Ta2_Ravelo_2013.eam.alloy

Run:

.. code-block:: bash

   tadah refit -c refit.cfg -v 2

With defaults this reads the table, fits it in *compact* mode with the
*Knot5* basis family, runs the crystal self-test, and writes:

* ``pot.tadah`` — the native Tadah! potential,
* ``refit.eam`` — a round-trip setfl built from the fitted functions,
* ``refit.hpotarget`` — a starter HPO configuration (see
  :ref:`refit_hpo_handoff`),
* the inspection files described in :ref:`refit_outputs`.

A slightly more explicit config:

.. code-block:: none

   EAM                  Fe_2.eam.fs
   OUTFILE              pot.tadah
   SETFL_OUT            refit.eam
   REFIT_MODE           compact      # default
   REFIT_BASIS          Knot5        # default in compact mode
   REFIT_TOL_AMBIENT    0.1          # meV/atom
   REFIT_TOL_COMPRESSED 1.0          # meV/atom

How it works
------------

Fitting modes
.............

``REFIT_MODE`` selects one of two fitting pipelines:

``compact`` (default)
   Basis-function centres and widths are *optimised*, not placed on a
   fixed grid.  Each function is grown by a per-function **ladder**:
   a global search at the smallest basis size, then one basis function
   inserted per rung at an energy-aware location, with a
   derivative-free explore stage and a high-precision analytic-gradient
   polish (Ceres Levenberg–Marquardt).  A final **joint refinement**
   optimises all three functions together against curve residuals
   *and* multi-lattice energy–volume residuals.  The result is a small
   search space (~30–120 parameters) — the HPO-friendly choice.
   Compact mode requires the Ceres-enabled HPO build; without it the
   command falls back to dense mode with a warning.

``dense``
   Region-uniform basis grids with automatic escalation
   (~300–450 parameters).  The fastest fit; the larger parameter count
   makes subsequent hyperparameter optimisation impractical.

Faithfulness self-test and escalation
.....................................

After every fit the crystal self-test compares cohesive energy and
energy–volume curves of the refit against the original table on the
lattice declared in the setfl header (``bcc`` / ``fcc`` / ``hcp``).
Two tolerances drive automatic escalation:

* ``REFIT_TOL_AMBIENT`` (default ``0.1`` meV/atom) over
  ``a/a0`` 0.95–1.10, and
* ``REFIT_TOL_COMPRESSED`` (default ``1.0`` meV/atom) over
  ``a/a0`` ``REFIT_REACH_SMIN``–0.95.

When the test fails, the dominant offender among
:math:`\phi` / :math:`\rho` / :math:`F` (attributed by swapping one
fitted function at a time into the original crystal sum) gets a larger
basis and the fit reruns, up to ``REFIT_MAX_ESCALATIONS`` rounds.  If
the header lattice is not bcc/fcc/hcp the self-test is skipped — there
is then **no internal faithfulness judge** and you must validate the
refit externally before use.

Windowed basis placement
........................

With ``REFIT_PLACEMENT window`` (the default) the basis is concentrated
on the *physically reachable* part of each function's domain: pair
distances above the first-neighbour distance at the strongest probed
compression, and embedding densities below the maximum reachable
density (times ``REFIT_REACH_MARGIN``).  The unreachable tails keep a
small uniform allocation.  ``REFIT_PLACEMENT uniform`` restores a
single uniform grid over the full tabulated range.

.. _refit_basis_families:

Basis families
--------------

``REFIT_BASIS`` selects the basis family.  The default is **Knot5** in
compact mode and **Blip** in dense mode (Blip is also the automatic
fallback when the compact fitter is unavailable, i.e. on non-Ceres
builds; an explicit ``REFIT_BASIS`` always wins — explicitly requesting
a knot family on a Ceres-less build is a hard error rather than a
silent substitution).

.. list-table::
   :header-rows: 1
   :widths: 12 30 14 44

   * - Family
     - Functional form
     - Modes
     - Maturity / guidance
   * - ``Knot5``
     - One-sided quintic knot functions :math:`(t-r)_+^5`,
       :math:`C^4`
     - compact only
     - **Default in compact mode; recommended.**  Best measured
       worst-case elastic constants of all families (1.6 GPa vs Blip's
       2.3 GPa across the five reference EAMs) with every E–V
       observable in tolerance.
   * - ``Blip``
     - Cubic B-spline bumps, :math:`C^2`
     - dense **and** compact
     - **Default in dense mode.**  The most validated family; the safe
       choice when compact mode is unavailable.
   * - ``Blip5``
     - M6 quintic B-spline bumps, :math:`C^4`, local support
     - dense **and** compact
     - Validated but **material-dependent**: best measured Fe elastic
       constants of all families, but one reference element (Al)
       degraded to ~4 GPa in :math:`C_{11}`.  Always verify against
       reference observables.
   * - ``Knot3``
     - One-sided cubic knot functions :math:`(t-r)_+^3`,
       :math:`C^2`
     - compact only
     - Specialised: the Mendelev/Ackland functional form — *exact* for
       knotted-cubic source tables.
   * - ``KnotMix``
     - Auto mix: quintic knot atoms by default, cubic atoms accepted
       per insertion under a double-win rule (a cubic must beat the
       quintic on the energy metric **and** be no worse on the
       curvature residual)
     - compact only
     - **The measured best across the reference set**: the double-win
       rule deploys cubic atoms selectively (where the source table has
       genuine :math:`f'''` breaks) — worst-case elastic-constant error
       0.7 GPa vs ``Knot5``'s 1.6 and ``Blip``'s 2.3, with best-in-class
       E–V faithfulness.  Newer than ``Knot5`` (one full validation
       round vs many) — verify with the faithfulness battery on your
       own tables.
   * - ``Gaussian``
     - Gaussians, hard-truncated at ``rcut``
     - dense only
     - **Experimental / unvalidated** for refit: never gated through
       the LAMMPS faithfulness battery, and the hard-cutoff truncation
       leaves a value discontinuity at ``rcut``.  Prefer any other
       family.

.. important::

   For the knot families (``Knot3`` / ``Knot5`` / ``KnotMix``) the
   width parameter :math:`\eta` is a **pure gauge**: the transformation
   :math:`(c, \eta) \to (c\,s^{k},\, \eta/s)` leaves the function
   invariant.  Knot potentials therefore carry **no width keys at all**
   (no ``SGRID2B`` / ``SGRIDMB`` / ``SEMBFUNC`` entries — supplying one
   is an error); the knot positions ``CGRID2B`` / ``CGRIDMB`` /
   ``CEMBFUNC`` define the basis alone.  The per-atom degree lists
   ``KDEG2B`` / ``KDEGMB`` / ``KDEGEMB`` (3 = cubic atom, 5 = quintic;
   written by ``Knot3``/``KnotMix``) are *data*, not parameters: the
   mJoin meta-descriptors split them, but they are not optimisable —
   values outside ``{3, 5}`` are rejected when the potential is read.

.. _refit_config_keys:

Configuration keys
------------------

Everything below goes into the file passed with ``-c``.  ``tadah
explain refit`` prints the same key list from the command line.

Input and output
................

.. list-table::
   :header-rows: 1
   :widths: 24 18 58

   * - Key (type)
     - Default
     - Description
   * - ``EAM`` (file)
     - — (required)
     - Input tabulated EAM potential (setfl: ``eam/alloy`` or
       ``eam/fs``, single element).
   * - ``OUTFILE`` (file)
     - ``pot.tadah``
     - Output native Tadah! potential.
   * - ``SETFL_OUT`` (file)
     - ``refit.eam``
     - Output setfl file rebuilt from the native fitted functions
       (round trip).
   * - ``HPOTARGET`` (file)
     - ``refit.hpotarget``
     - Output starter HPO target file (see
       :ref:`refit_hpo_handoff`).
   * - ``OUTPUT_GRID_SCALE`` (float)
     - ``1.0``
     - Scale factor for the output setfl grid density
       (``1.0`` = same grid as the input table).
   * - ``REFIT_LOG`` (file)
     - ``refit_opt.log``
     - Live optimisation-log file: one timestamped, immediately-flushed
       line per ladder event (global, rung, coalescence merge, final
       optimisation), plus the joint-refinement summary and crystal
       self-test — inspectable *during* the fit.

Basis selection and size
........................

.. list-table::
   :header-rows: 1
   :widths: 24 18 58

   * - Key (type)
     - Default
     - Description
   * - ``REFIT_MODE`` (string)
     - ``compact``
     - ``compact`` (optimised centres/widths via ladder +
       analytic-gradient Ceres polish, ~30–120 parameters — the
       HPO-friendly search space; requires the Ceres-enabled HPO
       build, else falls back to dense with a warning) or ``dense``
       (region-uniform grids + auto-escalation, ~300–450 parameters,
       fastest fit).
   * - ``REFIT_BASIS`` (string)
     - ``Knot5`` (compact) / ``Blip`` (dense)
     - Basis family — see :ref:`refit_basis_families`.
   * - ``REFIT_LEVEL`` (string)
     - ``balanced``
     - Grid-accuracy preset: ``coarse`` | ``balanced`` | ``fine`` |
       ``accurate``.  Sets the starting basis sizes.
   * - ``N2B`` (uint)
     - from ``REFIT_LEVEL``
     - Override: number of two-body (:math:`\phi`) basis functions.
   * - ``NRHO_BASIS`` (uint)
     - from ``REFIT_LEVEL``
     - Override: number of density (:math:`\rho`) basis functions.
   * - ``NF_BASIS`` (uint)
     - from ``REFIT_LEVEL``
     - Override: number of embedding (:math:`F`) basis functions.

Faithfulness tolerances and escalation
......................................

.. list-table::
   :header-rows: 1
   :widths: 26 14 60

   * - Key (type)
     - Default
     - Description
   * - ``REFIT_TOL_AMBIENT`` (float)
     - ``0.1``
     - Faithfulness tolerance (meV/atom) for the *ambient* crystal
       self-test (E–V, ``a/a0`` 0.95–1.10).  The basis auto-escalates
       until met.
   * - ``REFIT_TOL_COMPRESSED`` (float)
     - ``1.0``
     - Faithfulness tolerance (meV/atom) for the *compressed* crystal
       self-test (``a/a0`` 0.75–0.95, :math:`V/V_0` down to ~0.42).
   * - ``REFIT_MAX_ESCALATIONS`` (uint)
     - ``4``
     - Maximum automatic basis-escalation rounds when the self-test is
       above tolerance (``0`` disables escalation).

Domain windows and constraints
..............................

.. list-table::
   :header-rows: 1
   :widths: 26 14 60

   * - Key (type)
     - Default
     - Description
   * - ``REFIT_PLACEMENT`` (string)
     - ``window``
     - ``window`` (basis concentrated on the physically reachable
       pair-distance / density window, small uniform tail allocation)
       or ``uniform`` (legacy single uniform grid over the full
       tabulated range).
   * - ``REFIT_REACH_SMIN`` (float)
     - ``0.75``
     - Reachability probe: smallest linear compression ``a/a0`` the
       fit must stay faithful at (0.75 ≈ :math:`V/V_0 = 0.42`, the
       multi-hundred-GPa regime).  Sets the reachable window and the
       self-test span.
   * - ``REFIT_REACH_MARGIN`` (float)
     - ``1.25``
     - Safety margin multiplying the maximum reachable embedding
       density when choosing the :math:`F(\rho)` fit window.
   * - ``REFIT_CONSTRAIN_F0`` (bool)
     - ``true``
     - Pin the embedding to the table's :math:`F(0)` (isolated-atom
       reference) with a constraint row.
   * - ``REFIT_R_LO_FRAC`` (float)
     - ``0.20``
     - Inner radial fit bound as a fraction of ``rcut``.  Below it the
       round-trip setfl tabulates a smooth :math:`C^2`-matched
       repulsive continuation and the native ``pot.tadah`` is outside
       its validity domain (see :ref:`refit_limits`).

Compact-mode schedule
.....................

These keys apply only when ``REFIT_MODE compact`` is active.

.. list-table::
   :header-rows: 1
   :widths: 30 14 56

   * - Key (type)
     - Default
     - Description
   * - ``REFIT_COMPACT_NMAX`` (uint)
     - ``40``
     - Per-function basis-function ceiling for the ladder.
   * - ``REFIT_COMPACT_OPT_EVERY`` (uint)
     - ``1``
     - Knot5 family: run the full per-rung optimisation (explore +
       polish) only every K-th ladder rung; intermediate rungs grow by
       energy-aware insertion + linear solve, with one final full
       optimisation of the best fit.  ``1`` (optimise every rung) is
       the measured most consistent setting.  ``999`` =
       insertion-only growth + final optimisation: best results when
       no ladder stalls (measured Ta: :math:`|\Delta C_{ij}| \le 0.09`
       GPa), but can underfit if a ladder stalls during growth.
   * - ``REFIT_COMPACT_STALL`` (uint)
     - ``6`` (Blip) / ``12`` (Knot5)
     - Stop the ladder after this many consecutive rungs without
       energy-metric improvement.
   * - ``REFIT_COMPACT_INSERT_TOPK`` (uint)
     - ``3`` (Knot5) / ``1`` (Blip)
     - Energy-aware insertion: evaluate the top-k separated
       curve-residual peaks and insert at the one that most improves
       the crystal energy metric (``1`` = curve peak only).
   * - ``REFIT_COMPACT_SHARE`` (float)
     - ``0.5``
     - Fraction of the ``REFIT_TOL_*`` budget each function must reach
       alone during its ladder (the joint refinement then handles
       cross-function coupling).
   * - ``REFIT_COMPACT_SEED`` (uint)
     - ``12345``
     - RNG seed for the jittered ladder restarts (deterministic
       reruns).

.. _refit_optimizer_block:

Compact-mode optimiser block
............................

The compact fit runs in four stages — **global** (global structure
search at the smallest basis), **explore** (per-rung derivative-free
search of the basis centres/widths), **polish** (high-precision local
refinement) and **joint** (a final all-function refinement against the
curve *and* the multi-lattice E–V residuals).  Each stage's optimiser
and its stopping criteria are set by an optional inline ``OPTIMIZER``
block in the config file.  **The block is optional** — omit it and the
measured-best defaults are used; add only the subblocks/attributes you
want to change.

.. code-block:: none

   OPTIMIZER
     GLOBAL                     # global structure search at the smallest basis
       ALGO     MLSL            # MLSL | DIRECT | CRS2 | DLIB_GLOBAL | UNIFORM
       MAXEVAL  60000           # global evaluation budget
     ENDGLOBAL
     EXPLORE                    # per-rung centres/widths search
       ALGO     SBPLX           # SBPLX | NELDERMEAD | BOBYQA | PRAXIS
                                #   | NLOPT_LBFGS | DLIB_LBFGS | NONE
       MAXEVAL  20000           # evaluation budget per candidate
       FTOL_REL 1e-13           # NLopt relative function tolerance
       XTOL_REL 1e-11           # NLopt relative parameter tolerance
     ENDEXPLORE
     POLISH                     # high-precision local refinement
       ALGO     CERES_LM        # CERES_LM | CERES_DOGLEG | NLOPT_LBFGS
                                #   | DLIB_LBFGS | NONE
       MAXITER  1000            # Ceres max iterations
       FTOL     1e-15           # Ceres function_tolerance
       GTOL     1e-14           # Ceres gradient_tolerance
     ENDPOLISH
     JOINT                      # final all-function refinement
       ALGO     CERES_DOGLEG    # CERES_DOGLEG | CERES_LM | NONE
       MAXITER  2000
       FTOL     1e-15
     ENDJOINT
   ENDOPTIMIZER

The values above are the defaults.  Notes:

* Every subblock and every attribute is optional.  ``GLOBAL`` and
  ``EXPLORE`` take NLopt-style ``MAXEVAL`` / ``FTOL_REL`` / ``XTOL_REL``;
  the Ceres-based ``POLISH`` and ``JOINT`` take ``MAXITER`` / ``FTOL``
  (function tolerance) / ``GTOL`` (``POLISH`` only).
* The ``GLOBAL`` stage's inner local refinement (when ``ALGO MLSL``)
  reuses the ``EXPLORE`` tolerances.
* ``ALGO NONE`` skips a stage (a closed-form linear solve replaces
  explore/polish; ``JOINT NONE`` skips the joint Ceres step).
* The RNG seed (``REFIT_COMPACT_SEED``) and the ladder schedule keys in
  the table above stay flat — they govern the *ladder*, not a single
  optimiser.

How each stage actually terminates (converged on a tolerance, or hit its
budget) is reported in the optimisation log — see
:ref:`refit_reading_log`.  If a stage always shows ``MAXEVAL_REACHED`` /
``NO_CONVERGENCE`` you are budget-bound (raise ``MAXEVAL`` / ``MAXITER``
or loosen the tolerance); if it converges immediately you can tighten it.

.. _refit_outputs:

Outputs and how to inspect them
-------------------------------

All files are written to the directory in which ``tadah refit`` was
started.

==============================  ==============================================================
File                            Contents
==============================  ==============================================================
``pot.tadah``                   The native potential (``OUTFILE``).
``refit.eam``                   Round-trip setfl built from the fitted functions
                                (``SETFL_OUT``); drop-in usable with the original
                                ``pair_style eam/alloy`` setup for an apples-to-apples
                                comparison in LAMMPS.
``refit.hpotarget``             Starter HPO target file (``HPOTARGET``) — see
                                :ref:`refit_hpo_handoff`.
``refit_opt.log``               Live optimisation log (``REFIT_LOG``) — see below.
``refit_curve_phi.dat``         Inspection curve for :math:`\phi(r)` — see below.
``refit_curve_rho.dat``         Inspection curve for :math:`\rho(r)`.
``refit_curve_F.dat``           Inspection curve for :math:`F(\rho)`.
``refit_ladder_trace.dat``      Compact mode only: one row per ladder rung — see below.
``refit_range_report.dat``      How the automatic decisions split each function's domain
                                (full range, fit window, reachability numbers).
``refit_compare.dat``           Legacy combined original-vs-fit dump (kept for plotting
                                scripts).
==============================  ==============================================================

Inspection curves (``refit_curve_*.dat``)
.........................................

One plot-friendly file per function, sampled on a dense grid.  The
derivative columns are central finite differences on the dump grid —
a plotting aid, not the analytic derivatives.  Columns:

.. code-block:: none

   # x  v_orig  v_fit  d1_orig  d1_fit  d2_orig  d2_fit  resid  in_window

* ``x`` — the abscissa (``r`` for :math:`\phi` and :math:`\rho`,
  ``rho`` for :math:`F`);
* ``v_orig`` / ``v_fit`` — original tabulated value vs native fit;
* ``d1_*`` / ``d2_*`` — first and second derivatives of each;
* ``resid`` — ``v_fit − v_orig``;
* ``in_window`` — ``1`` inside the physically reachable region the fit
  concentrates on, ``0`` in the tail.

Comparing ``d2_orig`` against ``d2_fit`` is the quickest way to spot
curvature artefacts (e.g. sawtooth wiggles) that barely show in the
values but move elastic constants.

Live optimisation log (``REFIT_LOG``)
.....................................

The log (default ``refit_opt.log``) is flushed line-by-line, so you can
``tail -f`` it while a long compact fit runs.  It records the per-rung
ladder events (global, insertions, coalescence merges, final
optimisation), the joint-refinement summary
(``max |dE|/tol`` and final parameter counts), the crystal self-test
results, and the list of output files.  In dense mode it records the
escalation loop and per-attempt goodness-of-fit instead.

.. _refit_reading_log:

Reading a compact-ladder line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

   [..] [phi] global(MLSL): n=2 metric=1.1e+04 ...  term=MLSL:FTOL_REACHED polish=CONVERGENCE(21it)
   [..] [phi] rung 3 (opt): n=5 metric=172.8 curve_rmse=0.0009 d2_rmse=0.39  KEPT  (0.76s)  explore={FTOL_REACHED:3} polish={CONVERGENCE(28it):3}
   [..] joint: max |dE|/tol = 0.21  params n2b/nrho/nF = 5/5/4  term=CONVERGENCE(143it, cost 2.1e-09)

* ``[phi]`` / ``[rho]`` / ``[F]`` — which of the three functions this
  ladder is fitting.  The functions are fitted **one at a time**, each
  with the other two held at their original tabulated shape; the final
  joint refinement couples them.
* ``rung 3`` — the ladder step.  Each rung **inserts one new basis
  function** at an energy-aware location (the best of
  ``REFIT_COMPACT_INSERT_TOPK`` candidate residual peaks), then
  re-optimises **all** basis-function positions/widths of the current
  expansion — not just the new one.  The linear coefficients are
  re-solved in closed form (variable projection) at every evaluation.
* ``(opt)`` vs ``(grow)`` — ``(opt)`` rungs run the full explore +
  polish.  ``(grow)`` rungs (only when ``REFIT_COMPACT_OPT_EVERY`` > 1)
  just insert + linear-solve, ~300× cheaper; a single full optimisation
  then runs once at the end.  With the default ``OPT_EVERY 1`` every
  rung is ``(opt)``.
* ``n=5`` — basis functions in the current expansion.  The ladder stops
  when ``n`` reaches ``REFIT_COMPACT_NMAX`` (default 40).
* ``metric`` — the **energy** selection metric: the worst per-atom
  energy error across a multi-lattice E–V scan, divided by
  ``REFIT_COMPACT_SHARE`` × the ``REFIT_TOL_*`` tolerance.  ``metric ≤
  1`` means the function alone is within its share of the budget; the
  ladder stops on success when it reaches this.  This is the quantity
  that decides whether a rung is **kept** and when the ladder stops —
  *not* the curve RMSE (curve-optimal is not energy-optimal).
* ``curve_rmse`` — RMSE of the fitted curve against the tabulated EAM
  samples.  ``d2_rmse`` — RMSE of the fit's second derivative against a
  finite-difference second derivative of the table (a curvature-artefact
  detector).  Both are **diagnostics**; neither drives rung selection.
* ``KEPT`` vs ``stall=k/S`` — a rung is ``KEPT`` when its ``metric``
  strictly improves on the best so far (the stall counter resets).
  Otherwise ``stall=k/S`` shows ``k`` consecutive non-improving rungs
  against the limit ``S`` = ``REFIT_COMPACT_STALL`` (default 12 for the
  knot families, 6 for blips); the ladder stops when ``k`` reaches
  ``S``.  ``(0.76s)`` is the rung's wall time.
* ``term`` / ``explore={...}`` / ``polish={...}`` — **which convergence
  criterion each optimiser stage hit**, the key signal for tuning the
  :ref:`OPTIMIZER block <refit_optimizer_block>`.  The ``global`` and
  ``joint`` lines carry a single ``term=`` (e.g. ``FTOL_REACHED`` or
  ``MAXEVAL_REACHED`` for NLopt; ``CONVERGENCE`` / ``NO_CONVERGENCE`` +
  iteration count for Ceres).  Each ``(opt)`` rung runs explore + polish
  once per candidate, so those are shown as a tally over the rung's
  candidates, e.g. ``explore={FTOL_REACHED:3}`` (all three converged on
  the tolerance) or ``polish={NO_CONVERGENCE(501it):3}`` (all three hit
  the iteration cap).  Read it directly: a stage that repeatedly shows
  ``MAXEVAL_REACHED`` / ``NO_CONVERGENCE`` is **budget-bound** — raise
  that stage's ``MAXEVAL`` / ``MAXITER`` (or loosen its tolerance) in the
  OPTIMIZER block; a stage that always converges quickly can be
  tightened.

What gets optimised, and by which optimiser, is staged: a **global**
structure search at the smallest basis size, a per-rung derivative-free
**explore** of the basis centres/widths, a high-precision **polish**,
and a final all-function **joint refinement**.  Each stage's optimiser
and stopping criteria come from the
:ref:`OPTIMIZER block <refit_optimizer_block>` (or its defaults).  The
within-rung objective the explore/polish minimise is the curve fit (with
an anti-coalescence guard for the knot families); the energy ``metric``
above is computed separately and used only to select and stop.

.. note::

   **Two different OPTIMIZER blocks.**  The block in the *refit config*
   (:ref:`above <refit_optimizer_block>`) tunes the four stages of this
   ladder.  The separate ``OPTIMIZER`` block written into the generated
   ``HPOTARGET`` file tunes the later ``tadah hpo`` search over the
   fitted potential's parameters — a different optimiser entirely.  Both
   share the same ``ALGO`` / ``MAXEVAL`` / ``FTOL_REL`` grammar style.

Ladder trace (``refit_ladder_trace.dat``)
.........................................

Compact mode writes one row per ladder rung per function:

.. code-block:: none

   # function  n  curve_rmse  energy_metric(<=1==within <share>x tol)  seconds  kept

``energy_metric`` is normalised so that values ≤ 1 mean the function is
within its ``REFIT_COMPACT_SHARE`` share of the tolerance budget;
``kept`` marks the rungs that survived into the final fit.  Plot
``energy_metric`` against ``n`` to see where each ladder converged or
stalled.

.. _refit_hpo_handoff:

Handoff to HPO
--------------

The generated ``HPOTARGET`` file (default ``refit.hpotarget``) is a
complete, runnable starter configuration for :ref:`nested fitting
<nested_fitting>`:

.. code-block:: bash

   tadah hpo -c <pot+DBFILE> --hpotarget refit.hpotarget --validation <val>

where ``<pot+DBFILE>`` is a copy of the refit ``pot.tadah`` with a
``DBFILE <dataset>`` line appended (or a config that includes both),
and ``<val>`` is a file with ``DBFILE`` line(s) naming the validation
data.  Before running, **review the loss section**: ``PC_ERMSE``
measures the energy RMSE on the validation data; ``PC_LAMMPS`` lines
can score LAMMPS-computed observables instead.

What the file contains:

* a conservative optimiser block (``NLOPT:LN_BOBYQA``, derivative-free
  local polish of a smooth fit);
* ``LOSS L2`` with a ``PC_ERMSE`` line, plus a commented ``PC_FRMSE``
  (enable it only with a force-bearing dataset *and* a force-trained
  seed — the energy-only refit pot does not predict forces);
* ``OPTIM`` bounds of ±\ ``OPTIM_PCT`` % (default ``5.0``) around every
  fitted value.  By default only the **linear coefficients**
  (``OPTIM FIXWEIGHT``; the pot carries the ``FIXINDEX`` /
  ``FIXWEIGHT`` seed) are active.  Every descriptor-shape block
  (``RCUT2B``, ``CGRID*``, ``SGRID*``, ``AMPGRIDMB``, ``CEMBFUNC`` /
  ``SEMBFUNC``, …) is emitted *commented*, with its parameter count:
  uncomment exactly the subset you want to tune.  Every uncommented
  line adds one dimension to a derivative-free search — BOBYQA alone
  needs ``2n+1`` evaluations to build its initial model, so raise
  ``MAXEVAL`` accordingly (enabling *everything* yields roughly
  75–150 dimensions for the compact default, material-dependent, and
  many hundreds for dense mode — beyond any practical evaluation
  budget).  The density amplitudes ``AMPGRIDMB`` are
  a fully legitimate target: they are excluded from the *linear* weight
  solve by the EAM bilinearity, but ``OPTIM AMPGRIDMB`` searches them
  like any other shape parameter.

.. note::

   For the knot families no width ``OPTIM`` blocks exist at all (the
   potential carries no width keys — :math:`\eta` is a pure gauge);
   tune the ``CGRID*`` knot positions, ``AMPGRIDMB`` and
   ``FIXWEIGHT``.

Iteration-1 seed equality
.........................

The handoff is exact by construction: the ``OPTIM`` bounds are centred
on the fitted values, and HPO's implicit ``CONFIG`` seed *is* the
fitted parameter vector.  HPO's first (seed) evaluation therefore
reproduces the refit potential bit-for-bit — running ``tadah predict``
with the refit ``pot.tadah`` on your validation set gives exactly the
numbers HPO sees at iteration 1.  Any residual against reference data
at that point is the *representation error* of the refit itself, which
gives you a clean, quantified baseline the optimiser must improve on.

.. _refit_limits:

Known limitations
-----------------

* **Single-element setfl only.**  Multi-element ``eam/alloy`` /
  ``eam/fs`` tables are not supported.

* **Inner validity bound** ``r_lo`` **.**  The native fit covers
  ``r ≥ r_lo = REFIT_R_LO_FRAC × rcut`` (default 20 % of ``rcut``).
  Below ``r_lo`` the round-trip setfl carries the *original* table
  (offset-matched to the fit at ``r_lo``), but the native
  ``pot.tadah`` is outside its validity domain.  If your application
  compresses harder than the defaults anticipate, lower
  ``REFIT_R_LO_FRAC`` and/or ``REFIT_REACH_SMIN``.

* **Self-test coverage.**  The internal faithfulness judge runs only
  for ``bcc`` / ``fcc`` / ``hcp`` header lattices; anything else skips
  the self-test (and escalation) entirely.  Elastic constants for
  **hcp** elements have not been validated by the LAMMPS faithfulness
  battery (the measured :math:`C_{ij}` figures quoted above come from
  cubic reference EAMs) — verify hcp elastic properties externally.

* **Source-table domain pathologies.**  Some published tables leave no
  compression headroom (reachable density ≈ table ``rhomax``).  Under
  strong compression both the original *and* the refit then probe the
  table's own out-of-range extrapolation — neither is defined there.
  ``tadah refit`` warns when it detects this; treat results in that
  regime accordingly.

* **Compact mode requires the Ceres-enabled HPO build.**  Without it,
  compact mode falls back to dense fitting with a warning (and a
  defaulted ``Knot5`` basis falls back to ``Blip``, since the knot
  families have no dense path).  An *explicit* knot-family
  ``REFIT_BASIS`` on a Ceres-less build is a hard error.

See also
--------

* :ref:`nested_fitting` — the HPO module that consumes the generated
  target file.
* :ref:`ConfigSection` — general configuration-file syntax and the full
  key reference.
* ``tadah explain refit`` — the same key list from the command line.
