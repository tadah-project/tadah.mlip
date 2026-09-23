.. _tadah_anneal_pt_block:

==============================
Parallel Tempering (ANNEAL_PT)
==============================

``LIB TADAH`` / ``ALGO ANNEAL_PT`` runs :math:`K` simulated-annealing
chains — **replicas** — concurrently, each at its own fixed temperature
on a ladder from ``T_COLD`` to ``T_HOT``, and periodically lets
neighbouring replicas **exchange states**. Hot replicas cross loss
barriers that would trap a single cooling chain; exchanges carry their
discoveries down to the cold end, where they are refined. Unlike serial
annealing there is no cooling schedule: the ladder is the whole design.

Part A below is the reference — the algorithm, every equation, the key
tables, and the run journal. Part B is a step-by-step setup guide.
Read :doc:`anneal` first for the Metropolis rule, the proposal kernels,
and the T0 calibration, all of which are shared; the
:ref:`shared vocabulary <anneal_vocabulary>` defines *sweep*, *epoch*,
*slot*, *pair*, *walker* and *window*.

.. _anneal_pt_reference:

Part A — Reference
==================

The tempering loop
------------------

Each **epoch**, every replica runs one *sweep* of its kernel at its
slot's temperature :math:`T_k` — one whole-vector proposal (CLASSIC) or
:math:`n` single-coordinate proposals (Corana). Keeping the unit of
work between exchanges this small is deliberate: exchange attempts are
free (no loss evaluation), and frequent swapping maximizes the flow of
states along the ladder (Syed *et al.* 2022).

Every ``SWAP_EVERY`` epochs an **exchange round** attempts swaps on
adjacent pairs with **deterministic even–odd pairing** (DEO): rounds
alternate between the even pairs :math:`(0,1), (2,3), \dots` and the
odd pairs :math:`(1,2), (3,4), \dots`. A swap of the pair with
temperatures :math:`T_a < T_b` holding losses :math:`f_a, f_b` is
accepted with the replica-exchange probability

.. math::

   P_{\text{swap}} \;=\; \min\!\left(1,\;
   \exp\!\Big[-\big(f_b - f_a\big)
   \Big(\tfrac{1}{T_a} - \tfrac{1}{T_b}\Big)\Big]\right),

so moving the *lower* loss toward the *colder* slot is always accepted,
and the reverse move is accepted with Boltzmann probability. Only the
states :math:`(x, f)` move; each slot keeps its calibrated step state
and its slot best. DEO makes the swap dynamics *non-reversible*: a
walker that starts moving up the ladder tends to keep moving up, which
shortens **round trips** (cold → hot → cold traversals) — the single
best indicator that tempering is actually working.

A note on *degenerate* exchanges: when both sides hold exactly equal
losses the rule above accepts with probability 1, which is correct but
carries no information about the ladder. The journal counts these
separately and the ladder adaptation ignores them (see
:ref:`the journal <anneal_pt_journal>`).

.. plot::

   # Illustration of DEO walker motion: an abstract index process with a
   # fixed swap probability per attempted pair (deterministic seed).
   import numpy as np, matplotlib.pyplot as plt
   rng = np.random.default_rng(7)
   K, rounds, p = 8, 120, 0.7
   pos = np.arange(K)
   hist = [pos.copy()]
   for r in range(rounds):
       for i in range(r % 2, K - 1, 2):
           if rng.random() < p:
               a = np.where(pos == i)[0][0]
               b = np.where(pos == i + 1)[0][0]
               pos[a], pos[b] = i + 1, i
       hist.append(pos.copy())
   hist = np.array(hist)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   for w in range(K):
       ax.plot(hist[:, w] + 1, lw=1)          # slots are 1-based for readers
   ax.set_xlabel("exchange round")
   ax.set_ylabel("slot occupied by walker   (1 = coldest)")
   ax.set_title("DEO walker trajectories: persistent, ladder-crossing motion",
                fontsize=9)
   fig.tight_layout()

The temperature ladder
----------------------

The ladder interpolates in log-temperature between the endpoints:

.. math::

   \ln T_k \;=\; \ln T_{\text{cold}} \;+\;
   \Big(\tfrac{k}{K-1}\Big)^{p}\,
   \ln\!\frac{T_{\text{hot}}}{T_{\text{cold}}},
   \qquad k = 0, \dots, K-1,

with :math:`p` = ``LADDER_POWER``. :math:`p = 1` (default) is the
**geometric ladder** — equal temperature *ratios* between neighbours —
which equalizes pair swap rates for energy distributions whose width
scales with temperature (Kofke 2002; Predescu *et al.* 2004).
:math:`p < 1` crowds slots toward the hot end, :math:`p > 1` toward the
cold end.

.. plot::

   import numpy as np, matplotlib.pyplot as plt
   K, span = 8, 1e3
   k = np.arange(K)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   for p in (0.5, 1.0, 2.0):
       T = 1e-3 * span ** ((k / (K - 1)) ** p)
       ax.semilogy(k + 1, T, "o-", label=f"LADDER_POWER = {p}")
   ax.set_xlabel("slot  k   (1 = coldest)")
   ax.set_ylabel(r"$T_k$")
   ax.legend(frameon=False)
   fig.tight_layout()

``T_HOT`` is calibrated like the serial :math:`T_0` unless set
explicitly; ``T_COLD`` defaults to ``T_HOT / LADDER_SPAN``. The hot end
should accept typical uphill moves freely; the cold end sets the
resolution to which the best minimum is refined.

How close neighbouring temperatures must be is a trade-off the ladder
geometry makes explicit: with :math:`K` replicas the :math:`K-1` rungs
share the fixed total span, so each geometric-rung ratio is
:math:`T_k/T_{k+1} = \text{LADDER\_SPAN}^{-1/(K-1)}`. Pair swap
acceptance depends on that ratio alone (for width-scaling energy
distributions), and it falls off quickly as the ratio widens. For a
2-D harmonic model the dependence has a closed form,
:math:`P_{\text{swap}} = 2R/(1+R)` with :math:`R = T_k/T_{k+1} < 1`
(the same expression the statistical test suite validates the swap
kernel against):

.. plot::

   # Analytic pair swap acceptance for the 2-D harmonic model:
   # P = 2R/(1+R), R = T_cold/T_hot of the pair. The 23% line is the
   # optimal-allocation reference of Kone & Kofke (2005).
   import numpy as np, matplotlib.pyplot as plt
   R = np.linspace(1e-3, 1.0, 400)
   P = 2 * R / (1 + R)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   ax.plot(R, P, lw=1.5)
   ax.axhline(0.23, ls="--", lw=1, color="tab:red")
   Rstar = 0.23 / (2 - 0.23)
   ax.axvline(Rstar, ls=":", lw=1, color="tab:red")
   ax.annotate(f"23% target\n(ratio $\\approx$ {1/Rstar:.1f} per rung)",
               xy=(Rstar, 0.23), xytext=(0.3, 0.12), fontsize=8,
               arrowprops=dict(arrowstyle="->", lw=0.8))
   ax.set_xlabel(r"temperature ratio of the pair   $R = T_k/T_{k+1}$")
   ax.set_ylabel(r"$P_{\mathrm{swap}}$")
   ax.set_ylim(0, 1.05)
   fig.tight_layout()

Real losses are not harmonic, and higher-dimensional problems need
tighter rungs than this curve suggests — but the shape carries over:
acceptance collapses once rungs are too far apart, and adding replicas
(or shrinking ``LADDER_SPAN``) is the only cure. Read the measured
per-pair acceptances from :ref:`the journal <anneal_pt_journal>` rather
than predicting them.

Ladder adaptation
-----------------

With ``LADDER_ADAPT ON`` the *interior* temperatures adapt toward equal
pair swap acceptance while both endpoints stay fixed. After every pair
has accumulated ``LADDER_ADAPT_EVERY`` (non-degenerate) attempts, each
interior log-gap is reweighted by its measured acceptance
:math:`a_p` relative to the mean :math:`\bar a`:

.. math::

   g_p \;\leftarrow\; g_p \cdot
   \exp\!\big[\operatorname{clamp}\big(\gamma_m\,(a_p - \bar a),
   \pm\ln 2\big)\big],
   \qquad
   \gamma_m = \texttt{LADDER\_ADAPT\_GAIN} \cdot
   m^{-\texttt{LADDER\_ADAPT\_DECAY}}

for update number :math:`m`, then the gaps are renormalized to the
fixed total span with every gap floored at
``LADDER_MIN_GAP`` :math:`\times` the mean gap. High-acceptance gaps
widen, low-acceptance gaps narrow; the diminishing gain makes the
controller settle. Equal pair acceptance is the standard equalization
target, with :math:`\sim 23\,\%` the optimal-allocation reference value
(Kone & Kofke 2005). Once every pair's pooled acceptance is
statistically indistinguishable from equalized (``LADDER_FREEZE``,
default on), adaptation freezes and the ladder is final; every update
is appended to ``hpo_temperature_ladder.tsv``.

Step modes
----------

``STEP_MODE`` selects how the CLASSIC per-dimension Gaussian scales
:math:`\sigma_i` are obtained (per slot — a hot replica needs larger
steps than a cold one):

``FIXED``
   :math:`\sigma_i = \texttt{STEP\_FRAC}\cdot w_i` for every slot. No
   calibration evals; use when you already know the scale.

``FROZEN`` (default)
   Two-phase automatic calibration per slot, then frozen. The
   *coordinate warm-up* bisects each dimension's scale until its
   single-coordinate acceptance sits near the one-dimensional optimum
   0.44 (Gelman, Roberts & Gilks 1996), spending between
   ``STEP_WARMUP_SWEEPS`` and twice that many attempts per coordinate.
   The *full-vector pilot* then rescales the whole vector
   (:math:`\sigma_i / \sqrt{n}` base) by one common multiplier until
   the whole-vector acceptance sits near the high-dimensional optimum
   0.234 (Roberts, Gelman & Gilks 1997). Calibrated scales and
   per-dimension diagnoses go to ``hpo_step_scales.tsv``; dimensions
   the warm-up could not settle are flagged (``insensitive`` /
   ``hypersensitive``) and continue at their best tested scale.

``ADAPTIVE``
   As ``FROZEN``, then the scales keep adapting during production: a
   robust adaptive-Metropolis update nudges :math:`\ln \sigma_i` after
   *every* proposal with a diminishing gain
   :math:`\eta_j = j^{-0.6}` toward the 0.234 target (Andrieu & Thoms
   2008), split into a common-scale term and a shape term. Scales are
   confined to a trust region of factor ``STEP_TRUST_FACTOR`` around
   the calibrated baseline so they can track a moving ladder without
   drifting without bound. Choose this together with
   ``LADDER_ADAPT ON``.

Under ``CORANA ON`` the per-slot step vector adapts by the Corana rule
instead (every ``NS`` completed sweeps, on that slot's own acceptance
counts); the ``STEP_MODE`` machinery does not apply and is rejected.

Two-pass temperature calibration
--------------------------------

When ``T_HOT`` is auto-calibrated *and* a step warm-up runs, the first
:math:`T_0` estimate was necessarily made with the generic
``STEP_FRAC`` kernel — the only kernel that exists before calibration.
A temperature derived for a kernel the run never uses would anchor the
whole ladder wrong, so after the warm-up the driver re-estimates
``T_HOT`` from the same seed anchor using the hottest slot's calibrated
kernel and rebuilds the ladder. Both passes and their ratio are
journaled; explicit ``T_HOT``, ``STEP_MODE FIXED`` and ``CORANA`` runs
skip the second pass because their first kernel is already the
production kernel.

Replica initialisation
----------------------

Slot 1 always starts exactly at the seed point. With ``INIT_MODE
UNIFORM`` the other replicas draw uniformly from the ``OPTIM`` box —
prefer it when the seed may sit in a narrow, unrepresentative well.
With ``INIT_MODE JITTER`` (default) they are displaced around the seed,
and two knobs decide by how much: the **magnitude** of the displacement
and its **shape along the ladder**. Each non-seed start costs one eval.

**Magnitude** — three forms, in increasing order of how much they know
about your problem:

``JITTER``
   sigma = ``STEP_FRAC`` × width, drawn from a Gaussian. The 1.4.x
   behaviour, kept as the default spelling.

``JITTER <fraction>``
   sigma = ``fraction`` × width, drawn uniformly on ±sigma. Use this
   when you want large production steps on a narrow ladder without
   scattering the initial population: the two were previously the same
   number, and on a wide box a "jittered" replica could be most of the
   way to a full randomisation from the seed.

``JITTER AUTO <p>``
   sigma is **measured** per dimension: the largest displacement
   :math:`\sigma_d` for which
   :math:`|L(\sigma_d) - L_0| \le p\,|L_0|`, found with a two-sign
   screen at the per-side cap followed by a log-bisection against your
   own ``--hpotarget`` objective, then drawn uniformly on ±sigma. The
   result is a per-dimension bound in the units the loss actually
   responds to, rather than a single fraction applied to boxes of very
   different sensitivity.

``FIXED`` and ``AUTO`` draw uniformly rather than from a Gaussian
because their sigma is a *bound*, and a Gaussian would place about 32 %
of coordinates outside the bound it was given.

The ``AUTO`` calibration does **not** consume ``MAXEVAL``, and its
probes are invisible to the stagnation counter, the restart policy and
``best_pot.tadah`` publication. It is bounded instead by
``INIT_JITTER_MAXEVAL`` (``AUTO`` = the protocol's own worst case,
dimensions × ``INIT_JITTER_PROBES``) and by ``INIT_JITTER_MAXTIME_FRAC``
(0.25 of ``MAXTIME``). On a large search space, ``INIT_JITTER_SAMPLE
<n>`` measures only *n* dimensions — chosen round-robin across
parameter families — and lets the rest inherit their family's median
sigma/width; on an 816-dimension space that turns 2 888 probes into
256. ``INIT_JITTER_AUDIT <n>`` re-probes sampled dimensions at
sigma/2, because sigma is a certificate about the points that were
tested, not a proof that the loss is monotone in displacement. Every
measured sigma, its status and the per-rung initial loss are journaled,
and the full table goes to ``hpo_init_jitter.tsv``.

.. _anneal_pt_init_jitter_ladder:

**Shape along the ladder** — ``INIT_JITTER_LADDER`` (``AUTO`` by
default) scales slot *k*'s displacement by

.. math::

   \sigma_k = \sigma_d \left(\frac{T_k}{T_\mathrm{HOT}}\right)^{\gamma}

so the cold slots start near the seed and the hot ones explore. Without
it every non-seed slot is displaced by the *same* sigma, and the
replica whose job is to refine the seed starts as far from it as the
hottest explorer — a warm start that survives only at slot 1.

``AUTO`` resolves to :math:`\gamma = 0.5`. That is equipartition, not a
tuned constant: about a locally quadratic minimum the equilibrium coordinate
spread goes as :math:`\sqrt{T}`. It is deliberately **not** a function
of ``LADDER_SPAN`` — the span already enters through
:math:`T_k/T_\mathrm{HOT}`, so making gamma depend on it as well would
count the same quantity twice. Scaled sigmas are floored at the
resolvable coordinate scale, so a very wide ladder cannot quietly
collapse the coldest slots onto the seed; the clamped count is reported
with a warning naming the remedy.

.. warning::

   This is on by default from 1.5.0 and **changes the starting
   population of every existing ANNEAL_PT run at a fixed ``SEED``**.
   ``PARAM INIT_JITTER_LADDER OFF`` reproduces the 1.4.x placement
   exactly; a number sets gamma directly.

Whether it helps depends on the seed. Measured on a warm start
(``ellipsoid6``, ``LADDER_SPAN 1e6``, seed at 2 % of the default start)
the default is 4× better on median best and wins 38 of 40 paired seeds
— that is the Tadah workflow, where ``OPTIM_INIT`` is a fitted
potential and the best point anyone knows. From a *mediocre* seed on a
curved landscape it can lose: ``rosenbrock6`` under the same ladder is
1.22× worse on 40 of 40 seeds, because that is exactly the case where
you want the cold slots to spread out and hunt. Use ``OFF`` there.

Stopping
--------

``MAXEVAL`` and ``MAXTIME`` are hard caps and ``STOPVAL`` fires on any
feasible eval at or below the target, as in serial annealing. The
tolerance stop is a **best-stall** rule: with a positive ``FTOL_REL``
and/or ``FTOL_ABS``, the run stops when the global best has failed to
improve by more than
:math:`\varepsilon = \max(\texttt{FTOL\_ABS},
\texttt{FTOL\_REL}\cdot|f_{\text{best}}|)` over ``NEPS`` consecutive
diagnostics windows of ``SWAP_WINDOW`` epochs each. (The occupant of
the coldest slot changes at every exchange, so a single-trajectory
plateau has no clean meaning in tempering; the run best is the quantity
a stopping decision is actually about.)

Determinism
-----------

Every source of randomness draws from its own stream keyed by
``SEED`` — chains by slot, swap coins by (epoch, pair), calibration
probes and warm-up separately — so with ``REPLICAS`` fixed and a
deterministic loss the evaluated set is identical for any ``THREADS``
and any MPI rank count, and calibration never shifts the production
proposal sequences.

Key reference
-------------

Grouped by function, most important first; generated from the
validation registry, so the tables cannot drift from the code. Retired
names (including every cooling-schedule key) are hard errors that
explain themselves.

.. include:: config/anneal_keys_pt.rst

The generic attributes (``MAXEVAL``, ``MAXTIME``, ``STOPVAL``,
``FTOL_*``, ``SEED``, ``THREADS``) are documented in
:ref:`nested_fitting`. ``THREADS`` is both the worker-pool width and
the ``REPLICAS`` default.

.. _anneal_pt_journal:

Reading hpo_run.log
-------------------

Startup (banner, layout, diagnostics contract, ladder)::

   ANNEAL_PT[CLASSIC[FROZEN]]: seed=1234  REPLICAS=8  THREADS=8  SWAP_EVERY=1  LADDER_SPAN=1000  STEP_WARMUP_SWEEPS=12  STEP_FRAC=0.1 (CLASSIC Gaussian sigma/width)  BOUNDARY_MODE=REFLECT  INIT_MODE=JITTER (slot 1 = seed; others Gaussian, sigma=STEP_FRAC*width=0.1) x (T_k/T_hot)^0.5 [INIT_JITTER_LADDER AUTO]
   ANNEAL_PT layout: sweep = 1 proposal eval(s)/replica;  epoch = REPLICAS*sweep = 8*1 = 8 evals;  exchange attempt every 1 epoch(s);  reports every 20 epoch(s)
   ANNEAL_PT diagnostics: step-health acceptance band [5%, 50%] per SWAP_WINDOW, warning after 2 consecutive windows;  transport window = 100*(REPLICAS-1) exchange rounds;  ladder-freeze evidence >= 100 pooled attempts/pair;  degenerate (equal-loss) exchanges are reported separately and excluded from ladder adaptation
   ANNEAL_PT ladder: T_HOT=2.412e+00  T_COLD=2.412e-03 (T_HOT/LADDER_SPAN)  geometric over 8 slots, ratio=2.683 per rung  [static ladder]
   ANNEAL_PT ladder T per slot (1=coldest .. 8=hottest): ...

The diagnostics line states every fixed threshold that can later fire
a WARNING, so each warning can be read against its rule.

Initial placement (``INIT_MODE JITTER``)::

   ANNEAL_PT initial-jitter ladder scaling: sigma_k = sigma_d * (T_k/T_hot)^gamma with gamma=0.500 (AUTO: equipartition, spread ~ sqrt(T)).  Slot 1 is the seed exactly; slot 2 draws 21.54% of sigma_d and slot 8 100%.
   ANNEAL_PT initial-jitter factor per slot (% of sigma_d; the coldest slot is the seed itself and draws none):
   [1-8]       seed     21.544     46.416 ...

A WARNING here naming ``(slot, dimension)`` pairs clamped to the
resolvable coordinate scale means the ladder is wider than the search
space can express: reduce ``LADDER_SPAN``, or lower gamma.

Calibration::

   ANNEAL_PT step calibration slot=3 T=4.6e-02  coordinate_acc=41.2% ...  pilot_multiplier=0.62 (moved)  pilot_status=ok  sigma=[3.1e-04, 2.2e-02]
   ANNEAL_PT step calibration summary: dimensions=12 ok=11 ... unconverged=1 (insensitive=1 ...)
   ANNEAL_PT calibration pass 2: T_HOT=9.115e-02 from the calibrated production kernel (pass 1 used the generic STEP_FRAC kernel; pass2/pass1 ratio=0.04); ladder rebuilt
   ANNEAL_PT ladder T per slot after calibration pass 2 (1=coldest .. 8=hottest): ...

A large pass-2 ratio is *normal* — it is the point of the second pass:
the generic kernel and the calibrated kernel probe very different loss
changes. The run continues on the rebuilt ladder.

Per-window report (every ``SWAP_WINDOW`` epochs, and on the final
epoch)::

   ANNEAL_PT epoch=140/2000  T=[2.400e-03, 9.100e-02]  proposed=160  accepted=23.8%  oob=1 (0.6%)  swap_acc=31.4%  f_cold=1.980e-01  best=1.980e-01  epoch_wall=12ms
   ANNEAL_PT swap acc% per pair (cumulative, att/pair 70; pair p = slots p<->p+1):  ...
   ANNEAL_PT DEGENERATE% (equal-loss; cumulative):  ...        (only when nonzero)
   ANNEAL_PT exchange this epoch (x=swapped, .=rejected, blank=other parity): ...
   ANNEAL_PT f per slot (pre-swap): ...
   ANNEAL_PT walker per slot (post-swap = next epoch's lineup; ^=up walker (last touched cold end), v=down walker (last touched hot end), .=no extreme yet): ...
   ANNEAL_PT ladder flow: traversals cold->hot=6  hot->cold=5  (~5 round trip(s) so far)

Under ``CORANA ON`` the same cadence also reports what the per-dimension
stepper is doing::

   ANNEAL_PT Corana median per-dim acceptance % per slot (epochs 121-140; per-slot, not per-walker): ...
   ANNEAL_PT Corana dims with ZERO accepted moves per slot (of 12; epochs 121-140): ...

How to act on these:

* **Pair acceptance** far below ~20 % on one pair while its neighbours
  are healthy = a gap that is too wide there: enable ``LADDER_ADAPT``,
  or add replicas, or shrink ``LADDER_SPAN``.
* **DEGENERATE%** dominating a pair (with its WARNING) = both replicas
  hold identical losses, typically a FAILSCORE plateau: the acceptance
  number above it is meaningless there; fix feasibility (seed, graded
  failures) before tuning the ladder.
* **Ladder flow / round trips** is the ground truth of tempering. Zero
  round trips over a transport window triggers a WARNING that the
  ladder is severed — more replicas, a smaller span, or more budget.
  Swap acceptance can look healthy while flow is zero; trust flow.
* **Step health** WARNINGs (production acceptance outside the band for
  consecutive windows under ``ADAPTIVE``) point at a slot whose kernel
  lost track — usually a moving ladder with too tight a trust region.
* **Corana frozen dimensions** growing at the *hot* end is the signal
  to act on. At the cold end a coordinate that accepts nothing is
  ordinary; a coordinate frozen at every temperature never moved
  anywhere, and the end-of-run HINT names those explicitly.

Final summary: the cumulative and last-window pair tables, new-best
counts per slot, total ladder flow, the final ladder (with the number
of adaptation updates and whether it froze), the whole-run Corana
acceptance census under ``CORANA ON``, and exactly one
``ANNEAL_PT stopped: <reason> after N host eval(s) (E epoch(s), R
exchange round(s))`` line.

The Corana census is a per-slot distribution, because 816 dimensions ×
16 slots cannot go in a journal::

    slot          T    dims  visited  frozen   satur    min%    p10%    p25%    p50%    p75%    p90%    max%
       1  1.000e-03       2        2       0       0    40.6    40.6    40.6    40.6    53.1    53.1    53.1
       4  1.000e-01       2        2       0       0    93.8    93.8    93.8    93.8    93.8    93.8    93.8

Rows are per **slot** (temperature), *not* per walker: an exchange moves
``x``/``f`` between slots and leaves the step vector and these counters
behind, so a row aggregates every walker that visited that temperature.

Sidecar files hold the full histories:
``hpo_temperature_ladder.tsv`` (every ladder update),
``hpo_step_scales.tsv`` (calibrated scales and diagnoses per slot and
dimension), ``hpo_init_jitter.tsv`` (measured jitter sigma per
dimension under ``JITTER AUTO``), and ``hpo_corana_acceptance.tsv``
(per-``(slot, dimension)`` acceptance and adapted sigma under
``CORANA ON``).

.. note::

   Every slot, pair and walker index printed by the run — in the
   journal, in the sidecar TSVs and in ``best_pot.tadah``'s
   ``# HPO_SOURCE_SLOT`` header — is **1-based** from 1.5.0. Scripts
   written against 1.4.x output must subtract one.

.. _anneal_pt_guide:

Part B — Setting up a run
=========================

1. **Budget first.** Set ``MAXEVAL`` from what you can afford; PT needs
   enough epochs for ladder traversal. A useful floor: at least
   :math:`2(K-1)/0.25` exchange rounds (the journal's plan line checks
   this for you and warns when a round trip is impossible).
2. **Width.** Set ``THREADS`` to the cores you can use; leave
   ``REPLICAS`` at its default (= ``THREADS``, min 2) unless you need a
   longer ladder than you have cores — 8–16 replicas cover most
   problems.
3. **Temperatures.** Start with automatic calibration (set neither
   ``T_HOT`` nor ``T_COLD``) and the default ``LADDER_SPAN``. Pin
   ``T_HOT``/``T_COLD`` explicitly only after a first run shows the
   scales.
4. **Step policy.** Keep ``STEP_MODE FROZEN``. Switch to ``ADAPTIVE``
   when you enable ``LADDER_ADAPT`` (moving temperatures want moving
   steps); use ``FIXED`` only for controlled experiments. Use
   ``CORANA ON`` when dimensions differ wildly in sensitivity and you
   can afford :math:`n` evals per replica per epoch.
5. **Ladder adaptation.** After a first run, if the per-pair table
   shows very uneven acceptance, set ``LADDER_ADAPT ON`` (and
   ``STEP_MODE ADAPTIVE``); the defaults for the gain schedule and the
   freeze need no tuning.
6. **Stops.** Add ``STOPVAL`` when you know a good-enough loss;
   ``FTOL_REL`` (e.g. ``1e-4``) with the default ``NEPS`` gives a
   sensible best-stall stop for open-ended runs.
7. **Read the journal** against the checklist in
   :ref:`anneal_pt_journal` — in particular ladder flow — before
   touching any advanced knob.

A minimal configuration::

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB TADAH
       ALGO ANNEAL_PT
       THREADS 8
       SEED 1234
       MAXEVAL 20000
       PARAM LADDER_SPAN 1000
     ENDSTRATEGY
   ENDOPTIMIZER

See also
========

:doc:`anneal` for the shared Metropolis/kernel/calibration machinery;
:doc:`asa` for an annealer with per-parameter temperatures;
:ref:`hpo_examples` for a runnable PT example;
:ref:`tadah_anneal_pa_block` for population annealing.
