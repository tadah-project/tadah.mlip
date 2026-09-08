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
     - global (``G_MLSL_LDS``, ``DIRECT``, ...) and local
       (``LN_BOBYQA``, ...) algorithms
     - Broad third-party collection; see the OPTIMIZER-block table in
       :ref:`nested_fitting`.
   * - ``DLIB``
     - ``GFS``, ``BFGS``
     - Global function search and quasi-Newton refinement.
   * - ``CERES``
     - ``LBFGS``, ``WOLFE``, ``NLCG``, ``STEEPEST_DESCENT``
     - Gradient-based refinement (analytical gradients where available).

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
   * - ``NLOPT:LN_NEWUOA(_BOUND)`` / ``LN_PRAXIS``
     - collapsed to ``min|step|``
     - no in raw units; uniform in the unit box
     - ``LN_NEWUOA`` ignores the box entirely (startup WARNING)
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
       kernel); Ceres kernel under ``NONE``; CERES is unbounded
