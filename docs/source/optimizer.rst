.. _hpo_optimizer:

=========
Optimizer
=========

The nested-fitting driver (:ref:`tadah hpo <nested_fitting>`) treats the
whole potential-fitting pipeline as a single scalar **loss function** of
the hyperparameters selected with ``OPTIM``, and hands that function to an
optimization algorithm chosen in the :ref:`OPTIMIZER block <nfp_manual>`.
This section documents the optimizers themselves. The Monte-Carlo family
below is developed and validated in-tree; the external libraries are
mature third-party optimizers behind the same interface.

.. list-table::
   :header-rows: 1
   :widths: 18 30 52

   * - ``LIB``
     - Algorithms
     - Character
   * - ``TADAH``
     - ``ANNEAL`` (classical and Corana simulated annealing),
       ``ANNEAL_PT`` (parallel tempering), ``ANNEAL_PA``
       (population annealing), ``RANDOM``, ``GRID``
     - Derivative-free global search; robust to the FAILSCORE cliffs of
       physics-constrained losses; every run is reproducible from
       ``SEED``.
   * - ``ASA``
     - :ref:`Adaptive Simulated Annealing <asa_optimizer>`
     - Ingber's ASA with per-parameter temperatures and reannealing.
   * - ``NLOPT``
     - global (``G_MLSL_LDS``, ``GN_DIRECT``, ...) and local
       (``LN_BOBYQA``, ``LD_LBFGS``, ...) algorithms; ``LN_NEWUOA_BOUND``
       and ``NLOPT_LD_LBFGS_NOCEDAL`` are refused (:doc:`nfp_configuration`,
       the ``NLOPT`` row of *Algorithms by library*)
     - Broad third-party collection; see the OPTIMIZER-block table in
       :ref:`nfp_manual`.
   * - ``DLIB``
     - ``GFS``, ``BFGS``
     - Global function search (consumes every ``INIT`` point) and
       box-constrained quasi-Newton refinement.
   * - ``CERES``
     - ``LBFGS``, ``WOLFE``, ``NLCG``, ``STEEPEST_DESCENT``
     - Gradient-based local refinement with finite-difference gradients;
       no box constraints.

Any of these backends can be the outer optimiser, local or global; the
``STRATEGY`` block wraps it (``PLAIN``, ``MULTISTART``, ``PRESTAGE``,
optionally ``RESTART``; :ref:`strategy_block`).  Gradient-based backends
(``NLOPT`` ``LD_*``/``GD_*``, ``CERES``) obtain their gradient by finite
differences and are rejected when the configuration has ``PC_LAMMPS``
lines; ``DLIB:BFGS`` is the one gradient-based optimiser accepted there.
An NLopt ``MLSL`` or ``AUGLAG`` is judged by its local search, whatever
the ``D`` or ``N`` of its name: it evaluates its own points without a
gradient, so ``GD_MLSL``, ``GD_MLSL_LDS``, ``LD_AUGLAG`` and
``LD_AUGLAG_EQ`` with a derivative-free ``INNER`` block (``LN_BOBYQA``,
``LN_SBPLX``, ``LN_COBYLA``, …) are accepted with ``PC_LAMMPS``; without
one they run NLopt's default local optimiser, ``LD_MMA``, and are
rejected with a message naming it.  Before 1.6.0 they were rejected with a
derivative-free ``INNER`` block too.
A failed loss at the point, or a failed evaluation in the
finite-difference stencil of a usable point, leaves the point without a
usable gradient, whatever ``FAILSCORE`` is (at a graded failure at the
point under ``FAILSCORE_GRADING ON`` its finite differences are its
gradient), which can end such a search with its own status
(:ref:`hpo_gradient_not_finite`).
``MAXEVAL`` caps one search of a backend — the run of a ``PLAIN``
strategy, one ``RESTART`` launch, one ``MULTISTART`` start, the
``PRESTAGE`` exploration or one refinement, so a run can make more — and
every evaluation the search makes counts: each loss and, for a
gradient-based backend, each finite-difference point of its gradients
and each analytical gradient (an ``INNER`` block's ``MAXEVAL`` counts the
NLopt calls of each local search, :ref:`hpo_inner_block`; the ``NOISE``
block's ``CALIBRATION`` repeats and the ``INIT_JITTER AUTO`` calibration
of ``ANNEAL_PT`` are charged to no search).  Without it a backend runs
until its other stops or its own end it (convergence, a finite
temperature schedule or grid, ASA's limits).  A tolerance is a stop only
above 0, and an active ``RESTART`` (:ref:`restart_decorator`) is one
too, except for ``GD_STOGO`` and ``GD_STOGO_RAND``: once its ``MAX``
restarts are used, the next ``AFTER_STAGNATION`` evaluations without a
new best end the run.  The backends compare as
follows (:ref:`hpo_stop_criteria` has the whole table):

.. list-table::
   :header-rows: 1
   :widths: 30 34 36

   * - Backend
     - Ends on its own
     - Stops it honours
   * - ``TADAH:RANDOM``, ``DLIB:GFS``
     - no: one stop required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - ``TADAH:GRID``
     - after its grid
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - ``TADAH:ANNEAL``, ``ANNEAL_PA``
     - at the end of the schedule
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_*``
   * - ``TADAH:ANNEAL_PT``
     - no: one stop required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_*``
   * - ``ASA:ASA``
     - on its own limits
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - ``DLIB:BFGS``, ``CERES``
     - converge
     - ``MAXEVAL`` and their tolerances
   * - NLopt global searches (``GN_DIRECT*``, ``GN_CRS2_LM``,
       ``GN_ISRES``, ``GN_ESCH``, ``GN_AGS``, the MLSL family) and
       ``LD_MMA``, ``LD_CCSAQ``, ``LD_SLSQP``
     - no: one stop required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, and the tolerances of
       DIRECT (its ``XTOL`` only once every rectangle is that small),
       CRS2, ISRES, MMA, CCSAQ and SLSQP
   * - NLopt ``GD_STOGO``, ``GD_STOGO_RAND``
     - no: ``MAXEVAL`` required (it ignores the forced stop by which HPO
       ends a search)
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - NLopt ``GN_ORIG_DIRECT``, ``GN_ORIG_DIRECT_L``
     - after about 100000 evaluations
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``XTOL_REL``
   * - NLopt AUGLAG (``AUGLAG``, ``LN_AUGLAG``, ``LD_AUGLAG``, and their
       ``_EQ`` forms)
     - when its local search does
     - its local search's
   * - the other NLopt algorithms
     - converge
     - all

A backend that cannot end without a stop and has none is a startup
error before any data is loaded (``NLOPT:GN_DIRECT needs a stop: …``),
and so is a setting a backend rejects: an attribute its run never reads
— a tolerance it does not test (``TADAH:RANDOM does not support
FTOL_REL: …; stop it with MAXEVAL, MAXTIME or STOPVAL``), ``POPULATION``
or ``VECTOR_ARR`` where the algorithm has none, ``SEED`` on
``TADAH:GRID``, ``DLIB:BFGS`` and ``CERES``, which draw no random numbers
(NLopt takes ``SEED`` on every algorithm, and ``DLIB:GFS`` ignores it
with a warning), ``GRAD_STRATEGY`` on a phase that takes no gradient (a
derivative-free optimiser, or an MLSL or AUGLAG whose local search is
one), ``STOPVAL`` inside an ``INNER`` block —, an NLopt ``AUGLAG``,
``AUGLAG_EQ``, ``G_MLSL`` or ``G_MLSL_LDS`` without an ``INNER`` block,
or, for every backend but NLopt, a ``PARAM`` key it does not read, named
with the keys it takes (``TADAH:RANDOM: unknown PARAM 'FOO' (it takes
none)``; :ref:`nfp_manual`).  Before 1.6.0 ``TADAH:RANDOM``,
``TADAH:GRID``, ``DLIB:GFS`` and NLopt accepted such attributes and
ignored them, and ``CERES`` its ``SEED`` (the annealers, ``ASA`` and
``DLIB:BFGS`` already refused theirs), and NLopt refused such an
``AUGLAG`` or ``G_MLSL`` only once the run had started.

.. toctree::
   :maxdepth: 2

   anneal.rst
   anneal_pt.rst
   asa.rst

.. _anneal_vocabulary:

Shared vocabulary
=================

The annealing pages and the run journal (``hpo_run.log``) use the
following terms with exactly these meanings.

.. glossary::

   eval
      One evaluation of the loss function — one call into the fitting
      pipeline. ``MAXEVAL`` counts evals, including calibration probes.

   proposal
      One candidate point offered to the Metropolis rule. Every proposal
      costs one eval.

   sweep
      A unit of chain work touching every dimension once: for the
      CLASSIC kernel one whole-vector proposal, for the Corana kernel
      ``dim`` single-coordinate proposals.

   rung
      One temperature step of one serial chain: ``CHAIN_LENGTH``
      proposals (CLASSIC) or ``NT * NS * dim`` proposals (Corana) at a
      fixed temperature, after which the temperature cools.

   epoch
      One parallel-tempering round: every replica runs one sweep at its
      own temperature, then exchanges may be attempted.

   exchange round
      One deterministic-even-odd sweep over adjacent ladder pairs in
      which state swaps are attempted (every ``SWAP_EVERY`` epochs).

   pair
      Adjacent ladder slots ``p`` and ``p+1``; swap statistics are kept
      per pair.

   slot
      A fixed position on the temperature ladder, numbered from 0
      (coldest) to K-1 (hottest). Step calibration and the slot best are
      *slot-resident*: they stay with the temperature, not the state.

   replica
      The chain currently running at a given slot.

   chain
      One Metropolis random walk: a current point, its loss, and its
      per-dimension step state.

   walker
      A state lineage followed through exchanges: when a swap is
      accepted the two walkers change slots. Ladder transport (round
      trips) is a statement about walkers.

   window
      The diagnostics cadence of a parallel-tempering run:
      ``SWAP_WINDOW`` epochs. Windowed tables and the best-stall stop
      are evaluated per window.

.. _tadah_anneal_pa_block:

Population annealing (ANNEAL_PA)
================================

``LIB TADAH`` / ``ALGO ANNEAL_PA`` anneals a population of
``POPULATION`` walkers from ``T_HOT`` to ``T_MIN``: at each temperature
step the population is reweighted by :math:`\exp(-\Delta\beta\, f_i)`
and resampled (systematic resampling, one RNG draw), so low-loss walkers
multiply and FAILSCORE walkers die out; each walker then runs ``SWEEPS``
rungs of the shared annealing stepper at the new temperature.
``ESS_TARGET`` picks the temperature step adaptively so the effective
sample size stays at the target fraction of the feasible population
(``0`` selects the geometric ``RT`` schedule). Resampling copies walker
state including the per-dimension step vector; every slot keeps its own
RNG stream, so results are identical for any ``THREADS``.

Population annealing shares the stepper, calibration, and journal
conventions of :doc:`anneal`; its key set follows.

.. include:: config/anneal_keys_pa.rst


Scaling, steps and tolerances by backend
----------------------------------------

Every backend iterates in the search space's internal coordinates
(:ref:`search_space_scaling`); what differs is whether the algorithm's
own controls are invariant under a per-dimension affine rescale of the
box and how it consumes an initial step.

.. list-table::
   :header-rows: 1
   :widths: 22 16 30 32

   * - Backend
     - ``STEP``
     - Scale-invariant?
     - Notes
   * - ``NLOPT:LN_SBPLX`` / ``LN_NELDERMEAD``
     - per-dim vector (first simplex edges)
     - no (subspace partition and the single step-scale factor act in
       internal units)
     - the unit box (``OPTIM_SCALE AUTO``) is what makes them commensurate;
       ``XTOL_REL`` is relative to each coordinate's magnitude — set
       ``XTOL_ABS`` too
   * - ``NLOPT:LN_BOBYQA`` / ``LN_COBYLA``
     - per-dim vector → coordinate rescaling
     - yes once the step vector is per-dim
     - ``rhoend = XTOL_REL * rhobeg``; BOBYQA needs ``width >= 2*step``
       (``STEP 0.5`` is capped just below)
   * - ``NLOPT:LN_NEWUOA`` / ``LN_PRAXIS``
     - collapsed to ``min|step|``
     - no in raw units; uniform in the unit box
     - ``LN_NEWUOA`` ignores the box entirely (startup WARNING);
       ``LN_NEWUOA_BOUND`` is refused (:doc:`nfp_configuration`, the
       ``NLOPT`` row of *Algorithms by library*): use ``LN_BOBYQA``
   * - ``NLOPT:LD_MMA`` / ``LD_CCSAQ``
     - per-dim trust half-width (default ``0.5*width``)
     - default yes; an explicit ``STEP`` makes it a fraction of width
     -
   * - ``NLOPT:G_MLSL*`` / ``AUGLAG*``
     - forwarded to ``INNER`` (outer wins)
     - clustering radius is isotropic in internal units
     -
   * - ``NLOPT:GN_CRS2_LM`` / ``ISRES`` / ``ESCH`` / ``DIRECT*``
     - rejected (never read)
     - yes (box-relative by construction)
     - ``DIRECT_*_NOSCAL`` and ``STOGO`` split the longest raw side
   * - ``NLOPT:LD_LBFGS`` / ``VAR*`` / ``TNEWTON*`` / ``SLSQP``
     - rejected
     - no (raw-unit line search, absolute ``tolg``)
     -
   * - ``TADAH:ANNEAL*``
     - rejected → ``PARAM STEP_FRAC``
     - yes (``STEP_FRAC * width`` per dim)
     - ``hpo_step_scales.tsv`` reports sigma and sigma/width
   * - ``TADAH:GRID`` / ``RANDOM``
     - ``PARAM GRID_POINTS`` / none
     - yes
     -
   * - ``ASA:ASA``
     - rejected (per-dim temperatures)
     - generation yes; reannealing no (raw-unit tangents)
     -
   * - ``DLIB:GFS``
     - rejected
     - sampling yes; trust region no (Euclidean in internal units)
     - dlib's own hidden ln transform fires only under ``OPTIM_SCALE NONE``
   * - ``DLIB:BFGS`` / ``CERES:*``
     - rejected
     - no (H0 = I, ``|x|``-relative tolerances)
     - ``PARAM fd_step`` = fraction of width under ``AUTO`` (in-house
       rule); Ceres' rule under ``NONE``; either is one-sided within a
       step of a bound (:ref:`hpo_fd_stencil`); CERES iterates are
       unbounded
