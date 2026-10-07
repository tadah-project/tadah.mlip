.. _tadah_anneal_block:

============================
Simulated Annealing (ANNEAL)
============================

``LIB TADAH`` / ``ALGO ANNEAL`` is the serial simulated-annealing
optimizer of the nested-fitting driver. Simulated annealing minimizes a
function it can only *evaluate* — no derivatives, no smoothness
assumptions — by performing a random walk whose willingness to accept
bad moves is controlled by a temperature that is gradually lowered. It
is a good first choice when the loss surface has many local minima, and
it tolerates the FAILSCORE cliffs of physics-constrained losses by
construction.

This page first explains the classical algorithm and every quantity it
uses, then the Corana variant that builds on it, then the key reference
and the run journal. Terms such as *eval*, *proposal*, *sweep* and
*rung* are defined in the :ref:`shared vocabulary <anneal_vocabulary>`.

The classical algorithm
=======================

State and objective
-------------------

The optimizer walks a single **chain**: a current point
:math:`x \in \mathbb{R}^n` inside the box given by the ``OPTIM``
bounds, together with its loss :math:`f(x)`. Here :math:`n` is the
number of optimizable dimensions and every coordinate lives in its
interval :math:`[l_i, h_i]` of **width** :math:`w_i = h_i - l_i`.
(When ``LOG_HP`` is set, qualifying dimensions are optimized in
:math:`\ln x_i`; the chain then walks the transformed interval and the
loss is always evaluated at the back-transformed point. Nothing is
transformed by default — see :ref:`search_space_constraints`.)

The proposal
------------

Each step draws a whole-vector Gaussian **proposal** — a candidate point

.. math::

   x'_i \;=\; x_i \;+\; \sigma_i\, \xi_i ,
   \qquad \xi_i \sim \mathcal{N}(0, 1)
   \quad \text{for every dimension } i,

where the per-dimension scale is set by :ref:`STEP_FRAC <anneal_keys>`:

.. math::

   \sigma_i \;=\; \texttt{STEP\_FRAC} \cdot w_i .

A coordinate that lands outside its interval is folded back by exact
mirror reflection (``BOUNDARY_MODE REFLECT``, the default), which is an
involution and therefore keeps the proposal symmetric.
``BOUNDARY_MODE REDRAW`` instead redraws the offending coordinate until it
lands inside; that is *not* a symmetric proposal and no
Metropolis–Hastings correction is applied, so sampling near a bound is
slightly biased away from the edge — the reported optimum is
unaffected, but prefer ``0`` unless you have a reason not to.

The acceptance rule
-------------------

The proposal is accepted or rejected by the **Metropolis rule**
(Metropolis *et al.* 1953): with the current loss :math:`f` and the
proposed loss :math:`f'`,

.. math::

   P(\text{accept}) \;=\;
   \begin{cases}
     1 & f' \le f \\[2pt]
     \exp\!\big(-(f' - f)/T\big) & f' > f ,
   \end{cases}

where :math:`T > 0` is the **temperature**: the loss scale on which
uphill moves are still routinely accepted. At large :math:`T` almost
everything is accepted and the chain explores; as :math:`T \to 0` only
improvements survive and the chain settles.

.. plot::

   import numpy as np, matplotlib.pyplot as plt
   dE = np.linspace(0.0, 5.0, 400)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   for T in (2.0, 1.0, 0.25):
       ax.plot(dE, np.exp(-dE / T), label=f"T = {T}")
   ax.set_xlabel(r"uphill move  $\Delta f = f' - f$")
   ax.set_ylabel("acceptance probability")
   ax.legend(frameon=False)
   ax.set_ylim(0, 1.02)
   fig.tight_layout()

A proposal whose loss is non-finite or equal to ``FAILSCORE`` is always
rejected; with :ref:`graded failures <failscore_grading>` enabled,
values inside the graded band are ordinary (large) losses to the rule
above, which is what lets a chain walk home from an infeasible start.

The cooling schedule
--------------------

Annealing proper (Kirkpatrick *et al.* 1983) lowers the temperature
geometrically. The chain runs one **rung** — ``CHAIN_LENGTH``
proposals — at each temperature, then cools:

.. math::

   T_{k+1} \;=\; \texttt{RT} \cdot T_k ,
   \qquad k = 0, 1, 2, \dots

from the starting temperature :math:`T_0` down to the floor ``T_MIN``
(by default :math:`T_0 \cdot 10^{-6}`). The default cooling factor
``RT`` = 0.85 follows Corana *et al.* (1987); values closer to 1 cool
slower and explore longer.

.. plot::

   import numpy as np, matplotlib.pyplot as plt
   k = np.arange(0, 130)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   for rt in (0.80, 0.85, 0.95):
       ax.semilogy(k, rt ** k, label=f"RT = {rt}")
   ax.axhline(1e-6, color="0.5", lw=0.8, ls="--")
   ax.text(2, 1.6e-6, "auto T_MIN = T0 * 1e-6", fontsize=8, color="0.35")
   ax.set_xlabel("rung  k")
   ax.set_ylabel(r"$T_k / T_0$")
   ax.legend(frameon=False)
   fig.tight_layout()

The starting temperature
------------------------

Unless ``T_INIT`` is set explicitly, :math:`T_0` is **calibrated**: the
seed point is evaluated, ``T0_PROBES`` proposals are drawn around it
with the proposal kernel above, and the temperature is chosen so that
the *median uphill* loss change would be accepted with probability
``T0_ACCEPT``:

.. math::

   T_0 \;=\;
   \frac{\operatorname{median}\big(\Delta f_{\text{uphill}}\big)}
        {\ln\!\big(1 / \texttt{T0\_ACCEPT}\big)} .

The median makes the estimate robust against FAILSCORE cliffs among the
probes (cliff probes are excluded from the statistics); if every probe
is downhill, the median absolute change is used instead. All probes
count against ``MAXEVAL``.

Automatic chain length
----------------------

``CHAIN_LENGTH`` defaults to **auto**: once :math:`T_0` is known the
schedule length is fixed,

.. math::

   M \;=\; \Big\lfloor \ln(T_{\min}/T_0) \,/\, \ln(\texttt{RT})
   \Big\rfloor + 1
   \quad\text{rungs},

and the remaining ``MAXEVAL`` budget is spread evenly over it,
:math:`\texttt{CHAIN\_LENGTH} = B / M`. The derivation is journaled
(:ref:`below <tadah_anneal_journal>`). Because auto needs a budget to
divide, running with auto lengths and no ``MAXEVAL`` is a configuration
error, reported at startup before any data is loaded (``TADAH:ANNEAL:
CHAIN_LENGTH AUTO needs MAXEVAL …``) — either set ``MAXEVAL`` or choose
``CHAIN_LENGTH`` yourself.

Re-annealing
------------

If the schedule reaches ``T_MIN`` with budget left, ``REANNEAL``
(default on) snaps the chain to the best point found and restarts the
schedule at a reduced starting temperature
:math:`T_0 \cdot \texttt{REANNEAL\_SCALE}^m` for re-anneal :math:`m`,
so leftover budget refines the region around the best point instead of expiring in a
frozen chain. Re-annealing stops when the next start temperature could
not complete two rungs above ``T_MIN``, and the stop line then says
``T < T_MIN``; a ``MAXTIME`` that has passed by then ends the search
instead, ``MAXTIME exceeded`` (until 2026-10-07 such a search reported
``T < T_MIN`` and ``T_MIN reached``).

Stops
-----

The run ends at the first of: ``MAXEVAL`` (hard cap on evals, probes
included), ``MAXTIME`` (wall clock), ``STOPVAL`` (any *feasible* eval at
or below the target), :math:`T < \texttt{T\_MIN}` with re-annealing
exhausted or disabled, or the **plateau rule**: when ``FTOL_REL`` and/or
``FTOL_ABS`` is set to a positive value, the run stops once the
end-of-rung loss agrees with the previous ``NEPS`` end-of-rung values
and with the running best to within

.. math::

   \varepsilon \;=\; \max\big(\texttt{FTOL\_ABS},\;
   \texttt{FTOL\_REL}\cdot |f|\big)

(Corana *et al.* 1987, §4). A tolerance of 0 disables the plateau. The
final ``ANNEAL stopped:`` journal line always names the reason, and the
run's status says it too (``TADAH:ANNEAL: FTOL plateau``;
:ref:`hpo_end_status`).

.. _tadah_anneal_corana:

The Corana variant
==================

``PARAM CORANA ON`` replaces the whole-vector Gaussian kernel with the
adaptive single-coordinate kernel of Corana *et al.* (1987). Everything
else — temperature calibration, cooling, re-annealing, stops — is the
classical machinery above. The implementation is validated against the
paper's own published experiments: the test suite re-runs the paper's
multiminima protocol (its Table II) and reproduces the reported final
losses and evaluation counts to within a few percent.

One coordinate at a time
------------------------

Instead of moving every dimension at once, the Corana kernel cycles
through the dimensions and moves one per proposal, uniformly:

.. math::

   x'_i \;=\; x_i \;+\; u\, V_i ,
   \qquad u \sim \mathcal{U}(-1, 1),

where :math:`V_i` is the per-dimension **step half-width**. One pass
over all dimensions is a *sweep*; the rung structure is ``NT``
step-adjustment cycles of ``NS`` sweeps each, so a Corana rung costs
``NT * NS * n`` evals.

The step adaptation
-------------------

After each block of ``NS`` sweeps the acceptance ratio
:math:`r_i` of every dimension (accepted / proposed moves of that
coordinate) adjusts its half-width toward the 40–60 % dead band:

.. math::

   V_i \;\leftarrow\;
   \begin{cases}
     V_i \,\Big(1 + c\,\dfrac{r_i - 0.6}{0.4}\Big) & r_i > 0.6
     \quad\text{(too easy — grow)} \\[6pt]
     V_i \,\Big(1 + c\,\dfrac{0.4 - r_i}{0.4}\Big)^{-1} & r_i < 0.4
     \quad\text{(too hard — shrink)} \\[6pt]
     V_i & \text{otherwise,}
   \end{cases}

with the gain :math:`c` = ``CORANA_C`` (paper value 2). The half-width
is clamped to :math:`[\texttt{V\_FLOOR\_FRAC} \cdot w_i,\; w_i]`; the
floor keeps every coordinate able to move, and dimensions that sit at
it while hugging a bound are what the :ref:`pinned-dimension audit
<tadah_anneal_journal>` reports.

.. plot::

   import numpy as np, matplotlib.pyplot as plt
   r = np.linspace(0, 1, 400)
   fig, ax = plt.subplots(figsize=(5.6, 3.2))
   for c in (1.0, 2.0):
       g = np.where(r > 0.6, 1 + c * (r - 0.6) / 0.4,
           np.where(r < 0.4, 1.0 / (1 + c * (0.4 - r) / 0.4), 1.0))
       ax.plot(r, g, label=f"CORANA_C = {c}")
   ax.axvspan(0.4, 0.6, color="0.92")
   ax.text(0.5, 2.6, "dead band", ha="center", fontsize=8, color="0.35")
   ax.set_xlabel(r"per-dimension acceptance ratio  $r_i$")
   ax.set_ylabel(r"step multiplier  $V_i' / V_i$")
   ax.legend(frameon=False)
   fig.tight_layout()

Cycle lengths and the return to the best
----------------------------------------

``NS`` defaults to the paper's 20. ``NT`` defaults to **auto** on the
same budget-spreading rule as ``CHAIN_LENGTH``, additionally capped at
the paper's structure :math:`\max(100, 5n)`. On every cooling step the
chain returns to the best point found so far (``RESET_TO_BEST``,
default on under Corana — the paper's step 7), so each colder rung
refines around the incumbent with the freshly adapted step vector.

When to prefer which kernel
---------------------------

The classical kernel costs one eval per whole-vector move and is the
cheaper explorer in low dimension. The Corana kernel pays :math:`n`
evals per sweep but learns an anisotropic step vector, which matters
when the loss is orders of magnitude more sensitive in some dimensions
than others — the common case for potential hyperparameters. Both are
exactly reproducible from ``SEED``.

.. _anneal_keys:

Key reference
=============

Keys are grouped by function, most important first. The tables are
generated from the same registry that validates the configuration, so
they cannot drift from the code. Retired key names are hard errors that
name their replacement.

.. include:: config/anneal_keys_serial.rst

The generic ``OPTIMIZER``/``STRATEGY`` attributes (``MAXEVAL``,
``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``, ``SEED``,
``THREADS``) are documented with the block grammar in
:ref:`nested_fitting`. ``THREADS`` sizes the shared evaluation pool
(the chain itself is serial); ``STEP``, ``XTOL_*``, ``POPULATION``,
``VECTOR_ARR`` and ``GRAD_STRATEGY`` are rejected with pointers to the
right control.

.. _tadah_anneal_journal:

Reading hpo_run.log
===================

The annealer writes a structured journal into the run log's PROGRESS
section. All lines use the :ref:`shared vocabulary
<anneal_vocabulary>`; values below are illustrative.

Banner and layout::

   ANNEAL[CLASSIC]: seed=1234  RT=0.85  BOUNDARY_MODE=REFLECT  T_MIN=auto(T0*1e-6)  CHAIN_LENGTH=auto  STEP_FRAC=0.1 (CLASSIC Gaussian sigma/width)  REANNEAL=ON
   ANNEAL: T0=2.412e+00 from 20 probes,  median|dE|=7.230e+00  loss range=[1.020e+00, 4.410e+01]  std=9.870e+00  target p_accept=5.000e-02  anchor=seed
   ANNEAL auto CHAIN_LENGTH: remaining budget 179 over 86 scheduled rung(s) (T0 -> T_MIN at RT=0.85) -> CHAIN_LENGTH=2
   ANNEAL layout: rung = CHAIN_LENGTH = 2 evals;  MAXEVAL=200 -> <= 100 rung(s) by budget (T_MIN schedule / plateau may end earlier, REANNEAL restarts extend rung count)

The banner echoes every knob that shapes the trajectory; ``auto``
values are resolved in the lines that follow, so the journal always
records the numbers the run actually used.

Per-rung line (one per rung; Corana adds the step-vector state)::

   ANNEAL rung=12/86  T=4.213e-01  proposed=2  accepted=1 (50.0%)  oob=0 (0.0%)  f_x=3.412e-01  best=2.108e-01
   ANNEAL[CORANA] rung=12/86  T=4.213e-01  proposed=120  accepted=55 (45.8%)  oob=3 (2.5%)  f_x=3.412e-01  best=2.108e-01  V=[1.200e-03, 3.100e-01]  V_floor=1/2  V_cap=0/2

``proposed``/``accepted`` count this rung's proposals and acceptances;
``oob`` counts proposals that needed boundary handling; ``f_x`` is the
chain's current loss and ``best`` the run best. Under Corana,
``V=[min,max]`` is the range of the step half-widths and
``V_floor``/``V_cap`` count dimensions clamped at the floor and at the
full width — a rising ``V_floor`` count means the kernel is running out
of room to shrink.

Watch acceptance: healthy runs start near 50–80 % and fall as the
schedule cools; an early collapse to a few percent means the start
temperature or step scale is too small for the landscape
(``T0_ACCEPT``/``STEP_FRAC`` up), while acceptance pinned near 100 %
to the end means the schedule never got cold enough to exploit
(``MAXEVAL`` up, or ``RT`` down).

Events and audits::

   ANNEAL re-anneal #1: T_start=6.030e-01  from best=2.108e-01
   WARNING   * 2 OPTIM dim(s) near a bound -- x sits within one proposal step (tol) of an OPTIM limit (under CORANA the step has also reached its floor, so no further shrinkage is available): ...

The pinned-dimension table lists, per dimension and side, the gap to
the bound, the audit tolerance (the dimension's actual proposal scale)
and for how many rungs the mark has persisted. Persistent marks are a
hint of ``OPTIM`` bound pressure: inspect the trace, and widen or
re-centre the interval only when the admissible domain permits it.

Summary and stop::

   ANNEAL summary: rungs=93  proposed=186  accepted=61 (32.8%)  oob=2 (1.1%)  T0=2.412e+00  T_final=1.512e-05  reanneals=1  best=1.981e-01
   ANNEAL accept-rate trajectory: min=4.2%  max=78.1%
   ANNEAL stopped: MAXEVAL exhausted after 200 host eval(s) (93 rung(s) completed, 1 re-anneal(s))

Every run ends with exactly one ``stopped:`` line naming the reason,
which is also the run's status (``TADAH:ANNEAL: MAXEVAL reached``; it
read ``COMPLETED`` before 1.6.0); budget accounting is exact (probes and
seed included).

MPI
===

Serial annealing runs on one rank (the shared evaluation pool still
parallelizes each eval's fitting work across ``THREADS``). Multi-rank
``tadah hpo`` runs are supported for the parallel drivers and reproduce
the same evaluated set for any rank count — see
:ref:`tadah_anneal_mpi`.

See also
========

:doc:`anneal_pt` runs many of these chains at once and couples them;
:doc:`asa` is an independent annealer with per-parameter temperatures;
:ref:`hpo_examples` contains ready-to-run configurations.
