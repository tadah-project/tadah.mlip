.. _asa_optimizer:

==================================
Adaptive Simulated Annealing (ASA)
==================================

``LIB ASA / ALGO ASA`` is Lester Ingber's Adaptive Simulated Annealing
(also known as *very fast simulated reannealing*), version ASA-30.53,
vendored verbatim into the HPO package under ``HPO/external/asa``.
The upstream code is distributed under the BSD-3-Clause license
(© 1987–2024 Lester Ingber); the license text ships with the vendored
source (``HPO/external/asa/ASA-LICENSE``) and the few build-integration
patches are inventoried in ``HPO/external/asa/TADAH-PATCHES.md``.
Upstream development has ended, which makes ASA a stable, battle-tested
vendoring target.

References:

1. Ingber, L.  *Very fast simulated re-annealing.*  Mathematical and
   Computer Modelling 12 (8), 967–973, 1989.
2. Ingber, L.  *Simulated annealing: Practice versus theory.*
   Mathematical and Computer Modelling 18 (11), 29–57, 1993.
3. Ingber, L.  *Adaptive simulated annealing (ASA): Lessons learned.*
   Control and Cybernetics 25 (1), 33–54, 1996 (arXiv:cs/0001018).

Background
----------

Classic SA (:ref:`tadah_anneal_block`) anneals one global temperature
and proposes fixed-shape steps.  ASA instead maintains a SEPARATE
temperature per search dimension and a separately scheduled *cost*
(acceptance) temperature:

.. math::

   T_i(k_i) \;=\; T_{0,i}\, \exp\!\bigl(-c_i\, k_i^{\,Q_i/D}\bigr),
   \qquad
   c_i \;=\; -\log(\mathrm{TRS})\,\exp\!\bigl(-\log(\mathrm{TAS})\,Q_i/D\bigr),

where :math:`D` is the search dimension, ``TRS`` =
``Temperature_Ratio_Scale``, ``TAS`` = ``Temperature_Anneal_Scale``,
:math:`Q_i` = ``User_Quench_Param_Scale[i]`` (1 unless quenching), and
the cost temperature uses :math:`c_{\mathrm{cost}} = c \cdot
\mathrm{Cost\_Parameter\_Scale\_Ratio}`.  Proposals are drawn from a
fat-tailed generating distribution that still produces occasional long
jumps at low temperature — this is what admits the proof of asymptotic
convergence at the exponentially fast schedule above.  Periodically ASA
*reanneals*: it measures per-dimension sensitivities as
finite-difference tangents (step ``Delta_X``) and rescales each
parameter temperature so stiff dimensions stay hot longer, and it
refreshes the cost temperature from the current best/last costs.

Like every HPO optimiser, ASA runs in the transformed (post-``LOG_HP``)
parameter space; for log-scaled dimensions the reanneal sensitivities
are therefore :math:`\partial L/\partial \log x` — usually what you
want.

.. _asa_probes_box:

ASA's finite-difference probes stay in the ``OPTIM`` box.  Its tangents —
at every reannealing and at the exit, whatever ``Reanneal_Parameters`` says
— evaluate :math:`(1 + \Delta) x`, or :math:`(1 - \Delta) x` when that
lies outside the box, and its exit curvature (``Curvature_0 0`` or ``-1``)
:math:`x \pm \Delta|x|`, or next to a bound :math:`x \mp \Delta|x|` and
:math:`x \mp 2\Delta|x|`, with :math:`\Delta` = ``Delta_X`` or
``User_Delta_Parameter[i]`` in internal coordinates.  A setting whose
probes would leave a box is refused at startup (and when ASA starts),
naming each dimension and the largest :math:`\Delta` that fits it.  Under
``OPTIM_SCALE AUTO`` (every box the unit interval) the tangents fit for
:math:`\Delta \le 1` and the curvature for :math:`\Delta \le 0.5`; under
``NONE`` a box narrower than about :math:`2\Delta|x|` does not fit:

.. code-block:: none

   the STRATEGY-level optimiser ASA:ASA: ASA's finite-difference probes -- the reannealing and exit tangents at (1 + Delta) x, or (1 - Delta) x when that lies outside the box -- would be evaluated outside the OPTIM box of these dimensions:
     - WATOM: OPTIM box [1, 1.000001], PARAM Delta_X 0.001: a probe of WATOM = 1.0000005 lies outside the box; Delta_X up to 4.99999e-07 fits it
     Lower the Delta; or use OPTIM_SCALE AUTO, where every box is the unit box (the tangents fit for Delta <= 1).

Before 1.6.0 such probes were evaluated outside the box.  The check uses
exact arithmetic; ASA computes the curvature's inner points as
:math:`(1 \pm \Delta) x` after testing :math:`x \pm \Delta|x|`, which can
differ in the last unit, so a probe landing exactly on a bound can come out
one unit in the last place beyond it — ``tadah hpo`` then counts it as an
evaluation outside the box and warns (:ref:`hpo_fd_stencil`).

Quick start
-----------

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB ASA
       ALGO ASA
       MAXEVAL 200
       SEED 1234
       PARAM Limit_Generated 200
       PARAM Temperature_Ratio_Scale 1e-4
     ENDSTRATEGY
   ENDOPTIMIZER

``PARAM`` keys use the ORIGINAL ASA option spellings, case-sensitively
(``PARAM Limit_Acceptances 10000``, not ``LIMIT_ACCEPTANCES``).
Per-dimension options additionally accept a 1-based indexed form
(marked ``[/i]`` in the tables below):
``PARAM User_Quench_Param_Scale 0.9`` fills every dimension,
``PARAM User_Quench_Param_Scale[3] 0.7`` overrides dimension 3.
The index is the dimension number the run itself prints — the ``dim``
column of ``hpo_search_space.tsv`` and the ``#`` column of the
``SEARCH SPACE`` table in ``hpo_run.log``, both of which start at 1.
Dimensions are numbered in the optimiser order — by ``OPTIM`` key, then
by index (:ref:`dimension order <hpo_dimension_order>`) — not in the
order of the ``OPTIM`` lines, so reordering those lines leaves every
``[i]`` on its dimension; adding or removing an ``OPTIM`` key moves the
dimensions of the keys that sort after it.

.. warning::

   The indexed form was **0-based** in Tadah 1.4 and earlier: ``[k]``
   meant the dimension printed as ``k+1``.  Add one to every index in
   an existing ASA config to keep addressing the same dimension
   (``[0]`` becomes ``[1]``).  Left unchanged, ``[k]`` now addresses
   the dimension one *earlier* than it used to — with ``D = 3``,
   ``PARAM User_Quench_Param_Scale[1] 0.7`` used to set the dimension
   printed as ``2`` and now sets the one printed as ``1``.  ``[0]`` is
   a hard error naming the change, but ``[1]``\ …\ ``[D-1]`` still
   parse, so check such configs against ``hpo_search_space.tsv``
   before rerunning.

Unknown keys are hard errors; option names belonging to features that
are compiled out get a tailored message naming the reason.

Stops honoured
--------------

``MAXEVAL`` (hard evaluation cap, including the ``Number_Cost_Samples``
initial cost-temperature probes; zero overshoot), ``MAXTIME``,
``STOPVAL``, and ``SEED`` are honoured.  ASA's own stops run alongside:
``Limit_Acceptances`` / ``Limit_Generated`` (NORMAL_EXIT),
``Accepted_To_Generated_Ratio``, the ``Cost_Precision`` ×
``Maximum_Cost_Repeat`` plateau (COST_REPEATING), the
temperature-underflow exits (P_TEMP_TOO_SMALL / C_TEMP_TOO_SMALL —
often acceptable convergence), and ``Limit_Invalid_Generated_States``
(TOO_MANY_INVALID_STATES; ``FAILSCORE``-level failures map to ASA's
invalid states).  ``FTOL_REL``/``FTOL_ABS`` and ``XTOL_*`` are rejected
with a pointer at ``Cost_Precision`` + ``Maximum_Cost_Repeat``;
``POPULATION``, ``STEP``, ``GRAD_STRATEGY``, ``VECTOR_ARR`` and
``INNER`` are rejected (dead knobs are errors, not silent no-ops).
These checks, and those of the ``PARAM`` keys, run at startup, before any
data is loaded, on every phase ASA runs in (:ref:`nfp_manual`).
When ``Gener_Block > 1``, ``MAXTIME``/``STOPVAL`` act at block
boundaries (``MAXEVAL`` stays exact).  A stop (``MAXEVAL``,
``MAXTIME``, ``STOPVAL``, a signal, or an error raised by an
evaluation) ends the run with ``IMMEDIATE_EXIT`` wherever ASA is,
including while it samples the initial cost temperature, a random
start (``User_Initial_Parameters 0``) or the ``Reanneal_Cost``
resampling: upstream ignores its exit request there, and TADAH patches
it to honour it (``TADAH-PATCHES.md``), so a stop is not held up by
``Limit_Invalid_Generated_States``.  Without ``MAXEVAL`` there is no
evaluation limit: ASA's own limits are its stops (``Limit_Generated``
99999 and ``Limit_Acceptances`` 10000 by default; set both to ``0`` for
no limit).

Seeding: ``SEED`` at OPTIMIZER/STRATEGY level seeds ASA's proposal
stream (unset → non-reproducible ``random_device``).  The first ``INIT``
point becomes ASA's starting point and sets ``User_Initial_Parameters
1``; without an ``INIT`` block that point is the configuration's values
(``CONFIG``), as for every optimiser.  Only when the ``INIT`` chain yields
no point (a ``WARM`` seed file that is missing under
``WARM_ON_MISMATCH warn`` or ``skip``) is ``User_Initial_Parameters`` 0:
ASA then draws its first state at random around the OPTIM box centre,
from its generating distribution at the initial parameter temperature,
redrawn until it is valid; the centre itself is not evaluated.  ``PARAM
User_Initial_Parameters 0`` with an ``INIT`` point, or ``1`` without
one, is an error.

Options
-------

The canonical 21 runtime options (defaults are the upstream ASA-README
defaults):

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - PARAM
     - Default
     - Effect
   * - ``Limit_Acceptances``
     - 10000
     - Maximum accepted states; 0 = no limit.
   * - ``Limit_Generated``
     - 99999
     - Maximum generated states; 0 = no limit.
   * - ``Limit_Invalid_Generated_States``
     - 1000
     - Invalid-state budget (failed evaluations); 0 = exit at the first
       invalid state, −1 = bypass the test (a stop still ends the run at
       once; see *Stops honoured*).
   * - ``Accepted_To_Generated_Ratio``
     - 1e-6
     - Minimum accepted/generated ratio; falling below it triggers the
       periodic reanneal/exit test.
   * - ``Cost_Precision``
     - 1e-18
     - Plateau threshold: ``|last - best|`` below it counts as
       repeating.
   * - ``Maximum_Cost_Repeat``
     - 5
     - Consecutive plateau test cycles before COST_REPEATING; 0 = off.
   * - ``Number_Cost_Samples``
     - 5
     - Initial cost-temperature sampling: >0 = mean ``|cost|`` over N
       probes; <−1 = deviation of a sample (0 and −1 are invalid).
   * - ``Temperature_Ratio_Scale``
     - 1e-5
     - THE primary annealing-rate knob, :math:`m = -\log(\mathrm{TRS})`.
       Raise toward 1 to slow annealing (recommended for large D).
   * - ``Cost_Parameter_Scale_Ratio``
     - 1.0
     - Cost-temperature schedule scale relative to the parameters.
   * - ``Temperature_Anneal_Scale``
     - 100.0
     - :math:`n = \log(\mathrm{TAS})`; keep > D for large problems.
   * - ``Include_Integer_Parameters``
     - 0
     - Include INTEGER-typed dimensions (from integer ``OPTIM`` keys)
       in the derivative/reanneal calculations.
   * - ``User_Initial_Parameters``
     - auto
     - 1 = start from the ``INIT`` seed (auto-set when one exists);
       0 = ASA draws a random start (contradicting an explicit ``INIT``
       seed is an error).
   * - ``Sequential_Parameters``
     - −1
     - ≥0 starts cyclic per-dimension generation at that index; any
       negative value keeps the default full-space sampling.
   * - ``Initial_Parameter_Temperature``
     - 1.0
     - Initial parameter temperature :math:`T_{0,i}`.
   * - ``Acceptance_Frequency_Modulus``
     - 100
     - Periodic-test cadence in accepted states (0 = off); also the
       cadence of the journal's ``ASA gen= acc= best=`` lines.
   * - ``Generated_Frequency_Modulus``
     - 10000
     - Periodic-test cadence in generated states (0 = off).
   * - ``Reanneal_Cost``
     - 1
     - 0 = off; 1 = rescale the cost temperature from best/last costs;
       >1 / <−1 = deviation-sample variants (−1 is invalid).
   * - ``Reanneal_Parameters``
     - 1
     - Rescale per-dimension temperatures by tangent sensitivities.
       Note: a ``FAILSCORE``-level failure DURING a tangent computation
       aborts the run with INVALID_COST_FUNCTION_DERIV (upstream
       semantics) — set 0 for intermittently failing objectives.
   * - ``Delta_X``
     - 0.001
     - Finite-difference step of the reanneal tangents; 0 disables
       tangents entirely.  Its probes must fit every ``OPTIM`` box
       (:ref:`the probes and the box <asa_probes_box>`).
   * - ``User_Tangents``
     - 0
     - Rejected if set: the HPO host has no analytic tangents.
   * - ``Curvature_0``
     - 1 *(deviation)*
     - 1 = skip the curvature array entirely; 0 (upstream default)
       computes a full D×D finite-difference curvature at exit —
       O(D²) LAMMPS-priced evaluations; −1 = recompute it every
       reanneal cycle.  See the deviations table below.

Quenching and per-dimension scales:

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - PARAM
     - Default
     - Effect
   * - ``User_Quench_Cost_Scale``
     - 1.0
     - Cost-temperature quench exponent Q; >1 quenches (does NOT void
       the sampling proof).
   * - ``User_Quench_Param_Scale`` [/i]
     - 1.0
     - Per-dimension quench exponent :math:`Q_i` in
       :math:`k^{Q_i/D}`.  >1 quenches — much faster, but VOIDS the
       statistical guarantee of sampling the whole space; <1
       deliberately slows annealing.  Think in units of
       :math:`Q_i/D`.
   * - ``User_Temperature_Ratio`` [/i]
     - 1.0
     - Per-dimension multiplier of :math:`-\log(\mathrm{TRS})`.
   * - ``User_Delta_Parameter`` [/i]
     - = ``Delta_X``
     - Per-dimension finite-difference tangent step; its probes must fit
       the dimension's ``OPTIM`` box (:ref:`the probes and the box <asa_probes_box>`).
   * - ``User_Parameter_Temperature`` [/i]
     - = ``Initial_Parameter_Temperature``
     - Per-dimension initial temperature.

Multiple minima and the evaluation cache:

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - PARAM
     - Default
     - Effect
   * - ``Multi_Number``
     - 1
     - Track the best N distinct minima; their costs are reported in
       the journal at the end of the run.
   * - ``Multi_Specify``
     - 1 *(deviation)*
     - 1 = a new minimum must be strictly better (upstream template
       uses 0 = ties count); see the deviations table.
   * - ``Multi_Grid`` [/i]
     - 0.0
     - Per-dimension distinctness grid of the multi-minima list.
   * - ``Queue_Size``
     - 0
     - FIFO evaluation-cache size; a generated state matching a cached
       one within ``Queue_Resolution`` reuses the cached cost instead
       of calling the objective.  0 = cache off.
   * - ``Queue_Resolution`` [/i]
     - 0.0
     - Per-dimension cache-hit resolution.

Block-parallel evaluation (see *Parallelism* below):

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - PARAM
     - Default
     - Effect
   * - ``Gener_Block``
     - 1
     - Number of trial states generated per iteration and evaluated
       concurrently, at the start of the run.  1 (with the default
       ``Gener_Block_Max`` 1) = canonical serial ASA sampling (proved
       bit-identical to the unmodified upstream build by a unit test);
       >1 changes the sampled sequence (upstream-documented caveat).
   * - ``Gener_Block_Max``
     - = ``Gener_Block``
     - Cap of the adaptive block resizing (buffers are sized once).
       At every periodic test (the ``Acceptance_Frequency_Modulus`` and
       ``Generated_Frequency_Modulus`` cadences, and whenever the
       accepted-to-generated ratio falls below
       ``Accepted_To_Generated_Ratio``) upstream's resizing sets the
       block to the average, over the last ``Gener_Mov_Avr`` tests, of
       the number of states generated since the last new best (or since
       the last test that found the ratio below
       ``Accepted_To_Generated_Ratio``), rounded down and kept between 1
       and this cap.  So the block never grows above the cap, but it can
       shrink below ``Gener_Block`` even when the two are equal; only
       ``Gener_Block_Max`` 1 or ``Gener_Mov_Avr`` 0 keeps it fixed.  The upstream
       README describes the average as one over the numbers of states
       needed to find each new best, but the code averages the running
       count at each test, which is small soon after a new best: on an
       objective that keeps improving the block tends towards 1.
   * - ``Gener_Mov_Avr``
     - 3
     - Moving-average window of the adaptive block resizing (≥0).
       ``0`` switches the resizing off, as the upstream README documents:
       every block holds ``Gener_Block`` states for the whole run, so
       ``Gener_Block 4`` with ``Gener_Block_Max 4`` keeps
       ``min(THREADS, 4)`` workers busy.  Upstream's own option check
       rejected 0; the vendored copy accepts it (TADAH-PATCH 7).

Parallelism
-----------

ASA is a single-chain algorithm, so — like serial ``TADAH:ANNEAL`` —
every evaluation's ``PC_LAMMPS`` fan-out already runs on the shared
``THREADS``-wide pool.  On top of that, ASA's own ``ASA_PARALLEL``
design generates a block of ``Gener_Block`` trial states per iteration
and evaluates them concurrently: the HPO integration runs the block on
a persistent worker pool of ``min(THREADS, Gener_Block_Max)`` threads
(the spawn region that upstream designates for user parallel code —
see ``TADAH-PATCHES.md``).  Because ASA sorts each block by cost before
consuming it sequentially, and the evaluation budget is reserved
per-slot in deterministic order, **results are invariant to THREADS**
(unit-tested at 1/2/4 workers).  ``Gener_Block 1`` (with its default
cap of 1) reproduces canonical serial ASA exactly; ``Gener_Block > 1`` is
a genuine sampling change
(more states per acceptance decision) that trades some serial
efficiency for wall-clock speed — the upstream README discusses this
trade-off.  ``Gener_Block`` is only the block size the run starts with:
the periodic tests resize the block to between 1 and
``Gener_Block_Max`` (see the ``Gener_Block_Max`` row above), so fewer
than ``min(THREADS, Gener_Block_Max)`` workers may be busy, for
example with ``Gener_Block 4``/``Gener_Block_Max 4`` on an objective
that keeps improving.  ``PARAM Gener_Mov_Avr 0`` keeps every block at
``Gener_Block`` states.

Concurrency contract: ``asa()`` is not re-entrant (upstream statics),
so one ASA run executes per process at a time; the ``RESTART`` decorator
re-runs it sequentially and works unchanged.  ASA is a single-seed
global optimiser, so ``STRATEGY TYPE MULTISTART`` rejects it, and so
does ``TYPE PRESTAGE`` with ``TOPK > 1`` when ASA is the refinement
optimiser; both are startup errors, raised before any data is loaded
(an ``EXPLORE`` block may run ASA).  Under MPI,
``ASA:ASA`` runs single-rank only (the multi-rank dispatch is reserved
for ``TADAH:ANNEAL_PT``/``_PA``).

Deviations from upstream defaults
---------------------------------

.. list-table::
   :header-rows: 1
   :widths: 26 12 12 50

   * - Option
     - Upstream
     - TADAH
     - Why
   * - ``Curvature_0``
     - 0
     - 1
     - Upstream computes a D×D finite-difference curvature array at
       exit; with LAMMPS-priced evaluations that is O(D²) extra
       evaluations (D≈96 ⇒ ~9000).  Set ``PARAM Curvature_0 0`` to
       restore upstream behaviour.
   * - ``Multi_Specify``
     - 0 *(template)*
     - 1
     - Strict ``<`` for new minima; the template's ``<=`` changes
       bookkeeping on cost ties.  Only relevant with
       ``Multi_Number > 1``.
   * - ``Limit_Acceptances`` / ``Accepted_To_Generated_Ratio``
     - 10000 / 1e-6
     - same
     - The canonical README defaults are used; note upstream's sample
       ``asa_opt``/``asa_usr.c`` ship illustrative values (1000 /
       1e-4).

Compiled-out upstream features (their option names give tailored
errors): ``ASA_RESOLUTION`` (``Coarse_Resolution`` grid snap has no
trajectory-neutral default), ``USER_INITIAL_COST_TEMP`` (replaces the
``Number_Cost_Samples`` sampling), the function-pointer options
(``USER_COST_SCHEDULE``, ``USER_GENERATING_FUNCTION``,
``USER_REANNEAL_*``, ``USER_ACCEPTANCE_TEST`` — the last does not even
compile upstream), the acceptance-law variants
(``USER_ACCEPT_ASYMP_EXP``/``THRESHOLD``), ``ASA_FUZZY``/``FITLOC``
(live in ASA's un-vendored user module), ``ASA_SAVE*`` (HPO runs are not
resumable), ``SELF_OPTIMIZE``, ``ASA_SAMPLE``, ``ASA_PIPE*``.
Every feature that IS compiled in is proven trajectory-neutral at its
default value by a golden unit test that compares the vendored build
bitwise against a pristine all-gates-off build.

Journal output
..............

.. code-block:: none

   [12:05:10]  INFO  ASA[Ingber ASA-30.53]: seed=1234 dim=2  Limit_Acceptances=10000  ...
   [12:05:10]  INFO  ASA: upstream journal -> asa_out (asa() truncates it per run)
   [12:05:12]  INFO  ASA gen=36 acc=20 best=6.622e-02 last=7.235e-02 Tcost=3.1e-01
   [12:05:14]  BEST  * eval=   173  loss=5.1035e-02  (52 ms)  [slot 2/4 Tcost=8.755e-02]
   [12:05:15]  INFO  ASA summary: generated=173 accepted=61 host_evals=200 best=4.621547e-02 exit=IMMEDIATE_EXIT  (at-best: generated=151 accepted=54)
   [12:05:15]  INFO  ASA stopped: MAXEVAL reached after 200 host eval(s) (asa exit: IMMEDIATE_EXIT)

The ``generated``/``accepted`` figures are live totals; the ``at-best``
pair repeats upstream ASA's exit convention of restoring its counters
to their values at the moment the best state was found (it reads
``0/0`` when the best never left the seed). A ``P_TEMP_TOO_SMALL``
stop line names the dimension whose temperature underflowed, e.g.
``... (often acceptable convergence) [dim 42: CGRID2B[10]] ...``.
``Tcost`` is ASA's live COST (acceptance) temperature — the single most
useful schedule-health number: it should start on the scale of the
initial loss spread and anneal downward; pinned near the ``FAILSCORE``
band means the calibration was poisoned by failures, and frozen high
means ``Reanneal_Cost`` keeps re-inflating it. (The per-dimension
parameter temperatures are internal to ASA and not reported; the
``[slot i/N]`` part names the ``Gener_Block`` lane that produced the
best, and both tags appear only in block-parallel mode.)

The ``ASA gen= acc=`` lines follow the ``Acceptance_Frequency_Modulus``
cadence.  ASA additionally writes its own upstream journal (options
dump, reanneal reports, new-optima lines) to ``asa_out`` in the run
directory (rank-suffixed under MPI; a numeric suffix is added when one
process runs ASA more than once, e.g. under ``MULTISTART``).  The
``loss.tadah`` / ``best_*.tadah`` artifact pipeline is identical to
every other optimiser.

Troubleshooting / tuning
------------------------

Ingber's recommended tuning order (ASA-README / ASA-NOTES):

1. ``Temperature_Ratio_Scale`` — the dominant knob.  For many
   parameters start LARGER than the default (slower annealing).
2. ``Temperature_Anneal_Scale`` — keep it above the search dimension.
3. ``Cost_Parameter_Scale_Ratio`` — then tune the cost temperature
   relative to the parameter schedule.  Parameter-temperature
   sensitivity is logarithmic; cost-temperature sensitivity is
   exponential — keep the cost temperature on the scale of
   ``best − last`` cost differences.
4. Only then quench (``User_Quench_*``) to speed up the end game —
   remembering that parameter quenching voids the sampling proof.

* Exit ``P_TEMP_TOO_SMALL`` after convergence is normal — a parameter
  temperature annealed below resolution (the stop line names the
  dimension).  If it fires too early, raise
  ``Temperature_Ratio_Scale`` (e.g. 1e-4 … 0.5) or set per-dimension
  ``User_Temperature_Ratio``.  Mind that quenching multiplies into the
  same exponent: ``User_Quench_Param_Scale`` Q with the default
  ``Temperature_Ratio_Scale 1e-5`` anneals to zero within a few
  thousand states — choose TRS and Q together
  (:math:`c = -\log(\mathrm{TRS})\exp(-\log(\mathrm{TAS})\,Q/D)`).
* Large D: set ``Curvature_0 1`` (default here), consider
  ``Reanneal_Parameters 0`` (saves D+1 evaluations per reanneal cycle),
  raise ``Temperature_Ratio_Scale`` and ``Cost_Parameter_Scale_Ratio``.
* Intermittently failing objectives (``FAILSCORE`` storms): failures
  during sampling are invalid states (bounded by
  ``Limit_Invalid_Generated_States``), but a failure inside a
  reanneal-tangent computation aborts with
  INVALID_COST_FUNCTION_DERIV — prefer ``Reanneal_Parameters 0`` or
  ``Delta_X 0`` there.
* Expensive re-visits (integer dimensions, coarse landscapes): enable
  the evaluation cache with ``Queue_Size`` + per-dimension
  ``Queue_Resolution``.

See also
--------

* :ref:`tadah_anneal_block` — the in-house classic SA and its Corana
  variant, for comparison.
* `Ingber's ASA site <https://www.ingber.com/#ASA>`_ — the upstream
  code and papers archive.
* ``HPO/external/asa/TADAH-PATCHES.md`` — vendoring notes, patch
  inventory, and the gate-set change procedure.
