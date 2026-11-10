.. _nfp_choosing:

Choosing an Optimiser
=====================

This guide shows how to choose the optimiser of a search, its first step
and its budget. It builds on the :ref:`nfp_quickstart`;
:ref:`nfp_optimizer_block` describes the block that holds these settings,
and :ref:`nfp_ref_optimisers` lists every optimiser with what it reads.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_choosing_factors:

What the choice depends on
--------------------------

A search repeats one step: the optimiser proposes values of the search
parameters, and ``tadah hpo`` trains a potential with them and computes its
loss. Optimisers differ in how they spend these evaluations. Five
properties of the search decide which one spends them best:

Number of search parameters
   A grid grows as a power of it, a finite-difference gradient with it.
Cost of an evaluation
   Expensive evaluations favour an optimiser that learns from each one.
Noise and failures
   Both favour gradient-free optimisers over gradient-based ones.
Gradients
   An analytical gradient is cheap; a :hpo:key:`PC_LAMMPS` or
   :hpo:key:`PC_FAILPROBE` line allows none.
Several minima
   A local search ends in the minimum nearest its start.

A noisy loss (:ref:`nfp_optimizer_block_noise`) comes from the loss terms,
failed evaluations (:ref:`nfp_targets_failures`) also from candidates that
cannot be trained. The first row that matches the search gives the recommended
first choice; a random search with the same budget shows whether it pays off.

.. list-table::
   :header-rows: 1
   :widths: 55 45

   * - Search
     - Recommended first choice
   * - One or two search parameters, cheap evaluations
     - :hpo:opt:`TADAH:GRID`
   * - A noisy loss, or LAMMPS properties in the loss
     - :hpo:opt:`NLOPT:LN_SBPLX`
   * - Analytical gradients, a smooth loss and a start near the minimum
     - :hpo:opt:`DLIB:BFGS`
   * - A rugged loss with failures, and a large budget
     - An annealer (:ref:`nfp_choosing_annealing`) or :hpo:opt:`ASA:ASA`
   * - Several minima, or wide boxes around a guessed start
     - :hpo:key:`PRESTAGE`: global, then local
   * - A smooth loss and a reasonable start
     - :hpo:opt:`NLOPT:LN_BOBYQA`

.. _nfp_choosing_baselines:

Grid and random baselines
-------------------------

:hpo:opt:`TADAH:GRID` evaluates the start, then every node of a grid that is
regular in the coordinates of the search (:ref:`nfp_search_space_transforms`),
with :hpo:param:`GRID_POINTS` values per search parameter. It shows the whole
loss curve of one search parameter, as in :ref:`nfp_unit_1`, or the surface of
two; each further one multiplies the cost by :hpo:param:`!GRID_POINTS`.

:hpo:opt:`TADAH:RANDOM` evaluates the start, then points drawn uniformly from
the box in the coordinates of the search. Random search is the recommended
baseline: an optimiser earns its place only if it beats random search with the
same budget, as in :ref:`nfp_unit_5`. It needs a stop, usually
:hpo:key:`OPTIMIZER.MAXEVAL`, and :hpo:key:`OPTIMIZER.SEED` repeats its points.

.. _nfp_choosing_local:

Local gradient-free searches
----------------------------

:hpo:opt:`NLOPT:LN_BOBYQA` is the recommended local search for a smooth loss.
It evaluates the start and one step on each side of it along every search
parameter. It fits a quadratic model of the loss to these evaluations and
moves towards the minimum of the model within the box. It uses every evaluation,
so it suits expensive evaluations and a start near a minimum. :ref:`nfp_unit_2`
runs it:

.. literalinclude:: ../files/tutorial/unit2/config.hpo
   :language: none
   :start-at: OPTIMIZER
   :end-at: ENDOPTIMIZER

:hpo:opt:`NLOPT:LN_SBPLX` suits a noisy loss better. It compares the losses at
the corners of a simplex instead of fitting a model to them, so it tolerates
small random changes in the loss. :ref:`nfp_unit_4` runs it with LAMMPS
properties in the loss, and :ref:`nfp_example_restart` restarts it when it
stagnates. Several starts (:ref:`nfp_example_multistart`) cover more of the
search space than one.

.. _nfp_choosing_global:

Global searches
---------------

A global optimiser spreads its evaluations over the whole box and concentrates
them where the loss is low. It needs no good start, but more evaluations than a
local search, and most global optimisers are refused without a stop such as
:hpo:key:`OPTIMIZER.MAXEVAL`.

- :hpo:opt:`DLIB:GFS` suits expensive evaluations among the global optimisers.
  It chooses each point from a lower bound of the loss fitted to every earlier
  evaluation, and alternates with a local search around the best point. It reads
  no :hpo:key:`PARAM <OPTIMIZER.PARAM>` key. Its random numbers come from a
  fixed internal seed, so with :hpo:key:`THREADS <OPTIMIZER.THREADS>` 1 it
  repeats its points; it refuses :hpo:key:`OPTIMIZER.SEED`. An :hpo:block:`INIT`
  block gives its first evaluations, as in :ref:`nfp_example_gfs_descriptor`.
- :hpo:opt:`NLOPT:GN_DIRECT_L` divides the box into rectangles from its
  centre, draws no random numbers and never evaluates its start.
- :hpo:opt:`NLOPT:GN_CRS2_LM`, :hpo:opt:`NLOPT:GN_ISRES` and
  :hpo:opt:`NLOPT:GN_ESCH` evolve a population of random points that
  includes the start; they read :hpo:key:`!OPTIMIZER.SEED` and suit a large
  budget.

.. _nfp_choosing_staged:

Global, then local
~~~~~~~~~~~~~~~~~~

:hpo:key:`PRESTAGE` runs a global exploration, then local searches from its
best points (:ref:`nfp_optimizer_block_strategies`). It is the recommended
choice when the loss may have several minima and a long global search costs too
much. :ref:`nfp_unit_5` explores with :hpo:opt:`!NLOPT:GN_DIRECT_L`, then
refines with :hpo:opt:`NLOPT:LN_BOBYQA`.

.. _nfp_choosing_annealing:

Annealing and ASA
~~~~~~~~~~~~~~~~~

The annealers (:hpo:opt:`TADAH:ANNEAL`, :hpo:opt:`TADAH:ANNEAL_PT`,
:hpo:opt:`TADAH:ANNEAL_PA`) and :hpo:opt:`ASA:ASA` accept a worse point from
time to time, so a search can leave a local minimum. They suit a rugged loss
with several minima or failures, and a large budget:

.. toctree::
   :maxdepth: 1

   annealing
   asa

.. _nfp_choosing_gradients:

Gradient-based searches
-----------------------

A gradient-based optimiser follows the slope of the loss. It pays off for
a smooth loss, a start near the minimum and a cheap gradient.
:hpo:opt:`DLIB:BFGS`, the recommended one, is a quasi-Newton search inside
the box (:ref:`nfp_example_gradient_observable`). :hpo:key:`GRAD_STRATEGY`
decides what a gradient costs:

- ``NUMERICAL``, the default without :hpo:key:`PC_OBSERVABLE`, which needs no
  setting of the analytical gradient: finite differences, two evaluations per
  search parameter, or one with :hpo:param:`PARAM.FD.diff_method` 1 (forward
  differences), so its gradient is cheap only for few search parameters;
- ``ANALYTICAL``, the default with :hpo:key:`!PC_OBSERVABLE`, which refuses
  ``NUMERICAL``: the derivative of the loss through the fit, one evaluation
  whatever the number of search parameters; startup refuses a setup without
  the settings that the :hpo:key:`!GRAD_STRATEGY` entry lists, or a search
  parameter without a derivative;
- ``HYBRID``: the analytical derivative where one exists
  (:ref:`descriptor_analytical_derivatives`), finite differences for the
  other search parameters.

Near failed evaluations a gradient-based search can end early: a finite
difference never mixes a usable loss with a failed one.
:hpo:opt:`NLOPT:LD_LBFGS` also keeps to the box, and
:hpo:opt:`CERES:STEEPEST_DESCENT` does not (its entry says what a point outside
costs). An NLopt MLSL algorithm ends on any error of a local search, such as a
failure of an :hpo:block:`INNER` :hpo:opt:`!NLOPT:LD_LBFGS`,
:hpo:opt:`NLOPT:LD_TNEWTON` form, :hpo:opt:`NLOPT:LD_VAR1` or
:hpo:opt:`NLOPT:LD_VAR2` next to failed evaluations.

With a :hpo:key:`PC_LAMMPS` or :hpo:key:`PC_FAILPROBE` line, every optimiser
:hpo:topic:`runs without gradients <RULE.PC_LAMMPS_gradient_free>`: the
results of a LAMMPS script have no gradient.

:hpo:opt:`!DLIB:BFGS` is refused beside a :hpo:key:`!PC_LAMMPS` or
:hpo:key:`!PC_FAILPROBE` line, as every gradient-based optimiser is.

.. _nfp_choosing_step:

Choosing the first step
-----------------------

:hpo:key:`OPTIMIZER.STEP` sets the first step of the NLopt local searches that
read one, as a fraction of the width of each box in the coordinates of the
search. On a log-scaled box (:ref:`nfp_search_space_transforms`) a step
multiplies the value by the ratio of the bounds to the power of the fraction.
:hpo:opt:`NLOPT:LN_BOBYQA` takes it as the radius of its first trust region,
:hpo:opt:`NLOPT:LN_SBPLX` as the edges of its first simplex.

The recommended step follows from the trust in the start. A large step suits
a guess, as in :ref:`nfp_unit_2`. A small one suits a start that an earlier
search found, as in :ref:`nfp_unit_5` and :ref:`nfp_example_warm_start`. A
:hpo:key:`!OPTIMIZER.STEP` line that names an :hpo:key:`OPTIM <ROOT.OPTIM>` key
overrides the block-wide step for it.

:hpo:opt:`!NLOPT:LN_BOBYQA` moves a start within one step of a bound by at most
half a step. With :hpo:key:`!OPTIMIZER.STEP` set, :hpo:opt:`!NLOPT:LN_SBPLX` and
:hpo:opt:`NLOPT:LN_NELDERMEAD` turn the first step inward from a start within
one step of the upper bound and not of the lower one, with a warning in the
journal. An optimiser that reads no :hpo:key:`!OPTIMIZER.STEP` refuses it; its
entry in :ref:`nfp_ref_optimisers` says what sets its first step.

.. _nfp_choosing_budget:

Choosing the budget
-------------------

:hpo:key:`OPTIMIZER.MAXEVAL` limits the evaluations of a search,
finite-difference ones included. The budget follows from the number of
search parameters, *n*, and from what the optimiser spends before it
improves on the start:

- :hpo:opt:`TADAH:GRID`: the start, then :hpo:param:`GRID_POINTS` to the
  power *n*; a smaller budget stops the sweep in grid order.
- :hpo:opt:`NLOPT:LN_BOBYQA`: 2\ *n* + 1 evaluations before its first model
  step, so a budget of several times that, fewer from a trusted start
  (:ref:`nfp_example_warm_start`).
- A gradient-based search: the start and one gradient before its first step.
  ``tadah hpo`` warns at startup when the budget cannot pay for that step.
- A global search: more than a local one, and more with each search
  parameter.
- :hpo:key:`PRESTAGE` and :hpo:key:`MULTISTART` split a run into several
  searches, each with a budget of its own or a share of one
  (:ref:`nfp_optimizer_block_budgets`, which also covers the other stops).

The tutorial gives first budgets per search parameter for a local search
(:ref:`nfp_unit_2`) and a :hpo:key:`!PRESTAGE` search (:ref:`nfp_unit_5`).
``hpo_run.log`` names the evaluation that gave the best loss
(:ref:`nfp_output_histories`). A best near the end of the budget means that the
search has not finished. It goes further in a new run with a larger budget, or
in a new search from the best potential with :hpo:key:`WARM <METHOD.WARM>`
(:ref:`nfp_optimizer_block_initialisation`). Optimisers that draw random numbers
are compared over a few seeds.
