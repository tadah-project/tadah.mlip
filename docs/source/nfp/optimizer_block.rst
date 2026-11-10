.. _nfp_optimizer_block:

The OPTIMIZER Block
===================

This guide shows how the :hpo:block:`OPTIMIZER` block of the target file
organises a search: how its optimiser runs, from which starting points, on which
budget and with how many workers. It builds on the :ref:`nfp_quickstart`;
:ref:`nfp_choosing` covers the optimiser itself.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_optimizer_block_strategies:

Choosing a strategy
-------------------

A target file has one :hpo:block:`OPTIMIZER` block, which holds one
:term:`search` or several. Its optimiser keys name the optimiser
(:hpo:key:`LIB`, :hpo:key:`ALGO`) and set its stops and settings; the blocks
inside it decide how it runs (:ref:`nfp_target_file_blocks`). A run of one
search needs no :hpo:block:`STRATEGY` block, as in :ref:`nfp_unit_2`:

.. literalinclude:: files/tutorial/unit2/config.hpo
   :language: none
   :start-at: OPTIMIZER
   :end-at: ENDOPTIMIZER

The optimiser keys of :hpo:block:`!OPTIMIZER`, :hpo:block:`!STRATEGY` and a
:hpo:block:`REFINE` block set one optimiser, the STRATEGY-level optimiser, and
each is written once. :hpo:key:`TYPE` and the :hpo:block:`RESTART` and
:hpo:block:`INNER` blocks need a :hpo:block:`!STRATEGY` block; the recommended
layout then writes every optimiser key in it, beside the strategy, or under
:hpo:key:`PRESTAGE` in a :hpo:block:`!REFINE` block (:ref:`nfp_unit_5`).

:hpo:key:`TYPE` :hpo:key:`PLAIN`, the default, runs the optimiser once.
:hpo:key:`MULTISTART` runs a local optimiser once from each of
:hpo:key:`N_STARTS` starting points, which suits a loss with several minima. The
:hpo:block:`INIT` block supplies the points. Startup refuses fewer than
:hpo:key:`!N_STARTS` of them, and a global optimiser, which runs under
:hpo:key:`!PLAIN`. An :ref:`example <nfp_example_multistart>` runs six starts:

.. literalinclude:: files/examples/multistart/multistart.hpo
   :language: none
   :lines: 29-30,32-35,40
   :dedent: 4

:hpo:key:`!PRESTAGE` explores with the optimiser of the :hpo:block:`EXPLORE`
block, then refines the :hpo:key:`TOPK` best points it found with the
STRATEGY-level optimiser, whose keys a :hpo:block:`!REFINE` block may hold. The
exploration chooses the region, the refinement the point. Startup refuses a
refinement optimiser that ignores its starting point. When every evaluation of
the exploration fails, the refinement starts from the STRATEGY-level
:hpo:block:`!INIT` points, with a warning. :ref:`nfp_unit_5` explores with
:hpo:opt:`NLOPT:GN_DIRECT_L` and refines with :hpo:opt:`NLOPT:LN_BOBYQA`:

.. literalinclude:: files/tutorial/unit5/config.hpo
   :language: none
   :lines: 25,28,31-33,35-38,41
   :dedent: 8

.. _nfp_optimizer_block_initialisation:

Choosing starting points
------------------------

Without an :hpo:block:`INIT` block a search starts from the values of the
training configuration (:hpo:key:`CONFIG`). A first search keeps this default,
with values chosen from physics, as in :ref:`nfp_unit_2`.

Each :hpo:key:`METHOD <INIT.METHOD>` line adds points to a chain, in the order
of the file. :hpo:key:`RANDOM <METHOD.RANDOM>`, :hpo:key:`LHS` and
:hpo:key:`SOBOL` draw them inside the :hpo:key:`OPTIM` boxes. :hpo:key:`!LHS`
(Latin hypercube sampling), recommended for a few points, cuts each box into as
many equal slices as it draws points, equal ratios in a :hpo:key:`LOG` box, and
puts one point in each. A chain gives the points of its lines only, so a
:hpo:key:`!CONFIG` line keeps the start of the training configuration:

.. literalinclude:: files/examples/multistart/multistart.hpo
   :language: none
   :start-at: INIT
   :end-at: ENDINIT
   :dedent: 4

A local optimiser under :hpo:key:`PLAIN` starts from the first point,
:hpo:key:`MULTISTART` from each, and :hpo:opt:`DLIB:GFS` takes all of them. An
:hpo:block:`EXPLORE` block with a :hpo:key:`!METHOD <INIT.METHOD>` line in its
:hpo:block:`!INIT` block has a chain of its own; otherwise it takes this one.
Startup warns about points the search does not evaluate. It refuses the
:hpo:key:`!METHOD <INIT.METHOD>` lines of the :hpo:block:`!INIT` block of a
search whose optimiser ignores its start, such as :hpo:opt:`NLOPT:GN_DIRECT_L`.

:hpo:key:`WARM` reads one starting point, the values of the search parameters,
from a potential file such as the ``best_pot.tadah`` of an earlier run. The new
run starts there, with its training configuration for every other setting and
the ``# HOT`` values of the file for its LAMMPS scripts (:ref:`nfp_output_best`,
:ref:`nfp_example_warm_start`). :hpo:key:`WARM_ON_MISMATCH` error, recommended
when the search must start there, stops the run when a value is missing or
outside its box. :hpo:key:`!WARM` reads its file at startup. A run removes its
own ``best_pot.tadah`` before its first evaluation (:ref:`nfp_output_files`), so
a seed of that name survives only as a copy under another name.

.. _nfp_optimizer_block_restart:

Restarting a search
-------------------

A local search can stagnate in a shallow minimum long before its budget is
spent. A :hpo:block:`RESTART` block inside :hpo:block:`STRATEGY` restarts it.
The search stops after :hpo:key:`AFTER_STAGNATION` evaluations without a new
best loss of the run, calls answered from the record included
(:ref:`nfp_optimizer_block_budgets`). The remaining :hpo:key:`MULTISTART` starts
still run. When a search other than a :hpo:key:`PRESTAGE` exploration has
stagnated, the whole strategy then runs again from new starting points, at most
:hpo:key:`MAX` times. A stagnating exploration hands its points to the
refinement and restarts nothing by itself (:ref:`nfp_example_restart`):

.. literalinclude:: files/examples/restart/config.hpo
   :language: none
   :lines: 24,28,31-32,34-35
   :dedent: 8

The :hpo:key:`METHOD <INIT.METHOD>` lines of the block draw the points of each
restart, under :hpo:key:`!MULTISTART` at least :hpo:key:`N_STARTS` of them, and
a :hpo:key:`WARM` line reads its file again at each; the first round starts from
the :hpo:block:`INIT` points. Without them every restart starts from the values
of the training configuration, and an optimiser that draws no random numbers
then retraces an earlier round. Drawn points with a :hpo:key:`SEED <INIT.SEED>`
are recommended: each round starts elsewhere, and the run can be repeated. An
optimiser that ignores its start never evaluates them, and startup warns.

Only stagnation restarts a search: a round without stagnation ends the run, so
:hpo:key:`!AFTER_STAGNATION` stays well below the evaluations a search needs to
reach its stop. :hpo:opt:`TADAH:GRID` and runs on several MPI ranks refuse the
block (:hpo:topic:`RULE.restart_grid`, :hpo:topic:`RULE.multi_rank_restart`).

.. _nfp_optimizer_block_noise:

Accounting for noise
--------------------

A loss is noisy when the same point gives slightly different losses, as when a
LAMMPS script starts from the state an earlier evaluation left. A gain below the
spread of the loss may be noise. With a tolerance on the loss below that spread,
such gains keep a search going until another stop, usually :hpo:key:`MAXEVAL`.

:hpo:key:`CALIBRATION` in a :hpo:block:`NOISE` block, directly inside
:hpo:block:`OPTIMIZER`, evaluates the first starting point that many times
before the search, on top of :hpo:key:`!MAXEVAL`. The ``NOISE CALIBRATION``
section of ``hpo_run.log`` lists the losses, their relative spread,
``(max - min)/|mean|``, and a suggested :hpo:key:`FTOL_REL` of ten times that
spread, at least 1e-6. The suggestion is not applied: a short run with a small
:hpo:key:`!MAXEVAL` gives it for the long run. A loss with :hpo:key:`PC_LAMMPS`
lines calls for a calibration before an :hpo:key:`!FTOL_REL` is set; a search
that :hpo:key:`!MAXEVAL` alone stops, as in :ref:`nfp_unit_4`, needs none.

The best potential has the lowest single loss (:ref:`nfp_output_best`), which
with a noisy loss may be a low draw: a gain over the start below the absolute
spread of the calibration is within the noise. A low draw is also harder to
beat, so the stagnation of a :hpo:block:`RESTART` block comes sooner.

:hpo:key:`AUTO <NOISE.AUTO>` in the same block sets the finite-difference step
of the STRATEGY-level optimiser (:ref:`nfp_choosing_gradients`) from the size of
the search space; :hpo:param:`fd_step <PARAM.FD.fd_step>` sets it by hand.
Startup refuses :hpo:key:`!AUTO <NOISE.AUTO>` for an optimiser that takes no
finite-difference gradients, as in a run with :hpo:key:`!PC_LAMMPS` lines, which
takes :hpo:topic:`gradient-free optimisers <RULE.PC_LAMMPS_gradient_free>` only.

.. _nfp_optimizer_block_inner:

Choosing an inner optimiser
---------------------------

An NLopt multi-level single-linkage (MLSL) algorithm starts local searches from
points it samples; an augmented Lagrangian (AUGLAG) algorithm runs one. An
:hpo:block:`INNER` block names that local search, :hpo:key:`LIB` NLOPT and a
local :hpo:key:`ALGO`; without one, NLopt runs a default local optimiser where
it has one (:hpo:topic:`RULE.inner_block_required`).

An :hpo:block:`!INNER` block is recommended wherever it is allowed: it states
the local search and its stops, which the default leaves implicit. Its optimiser
follows the advice for local searches (:ref:`nfp_choosing_local`,
:ref:`nfp_choosing_gradients`). Its :hpo:key:`MAXEVAL` bounds each local search;
that of the enclosing block bounds the enclosing optimiser, its local searches
included, and each local search too. Its tolerances, such as
:hpo:key:`FTOL_REL`, are written in it. The seed, the gradient strategy and the
target loss belong to the enclosing block, the workers to the STRATEGY level.

.. _nfp_optimizer_block_budgets:

Setting budgets and stops
-------------------------

:hpo:key:`MAXEVAL` limits the evaluations of the optimiser. It is the
recommended stop: the cost of a run follows from the file, whatever the machine
load; :ref:`nfp_choosing_budget` says what a budget must cover. The starts of
:hpo:key:`MULTISTART` share :hpo:key:`!MAXEVAL`, each share rounded up;
:hpo:key:`BUDGET_PER_START` sets each start's budget instead. Under
:hpo:key:`PRESTAGE` the :hpo:block:`EXPLORE` block has its own
:hpo:key:`!MAXEVAL`, and the refinements share the STRATEGY-level one. Each
:hpo:block:`RESTART` round, at most :hpo:key:`MAX` after the first, has the
whole :hpo:key:`!MAXEVAL`; :hpo:key:`CALIBRATION` evaluations come on top.

An NLopt optimiser or :hpo:opt:`DLIB:BFGS` can ask again for a point that the
run has evaluated with a usable loss. Unless a term of the loss can give that
point another loss, such as a :hpo:key:`PC_LAMMPS` line, the run answers the
call from its record, without an evaluation. NLopt counts such a call against
its :hpo:key:`!MAXEVAL` all the same.

Other stops can end a search earlier: :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>`
(seconds), :hpo:key:`STOPVAL` (a target loss) and the tolerances, such as
:hpo:key:`FTOL_REL` and :hpo:key:`XTOL_REL`. An optimiser accepts the stops it
reads only (:ref:`nfp_ref_optimisers`), and one with no end of its own, such as
:hpo:opt:`TADAH:RANDOM`, needs a stop. :hpo:key:`!MAXTIME <OPTIMIZER.MAXTIME>`
in a block is not the top-level :hpo:key:`MAXTIME <ROOT.MAXTIME>`, another name
for :hpo:key:`PC_SCRIPT_TIMEOUT`. :ref:`nfp_output_end` covers how a run ends.

.. _nfp_optimizer_block_threads:

Threads and reproducibility
---------------------------

:hpo:key:`THREADS` of the STRATEGY-level optimiser sets the workers that run the
LAMMPS scripts and the training and prediction of each evaluation.
:hpo:opt:`DLIB:GFS`, :hpo:opt:`TADAH:ANNEAL_PT`, :hpo:opt:`TADAH:ANNEAL_PA` and
:hpo:opt:`ASA:ASA` with batches (:ref:`nfp_asa_batches`) run several evaluations
at once on these workers (:ref:`nfp_unit_6`). When one of them explores under
:hpo:key:`PRESTAGE`, the :hpo:key:`!THREADS` of the :hpo:block:`EXPLORE` block
(1 without one) sets how many it asks for at once. The starts of
:hpo:key:`MULTISTART` and the refinements of :hpo:key:`!PRESTAGE` run in turn.

:hpo:key:`SEED <OPTIMIZER.SEED>` in an optimiser block seeds the random numbers
of the optimiser, and of an :hpo:block:`!EXPLORE` block without a seed of its
own. An optimiser that draws no random numbers, such as
:hpo:opt:`NLOPT:LN_SBPLX`, refuses a seed that no :hpo:block:`!EXPLORE` block
takes, and so does :hpo:opt:`DLIB:GFS` (:ref:`nfp_choosing_global`).
:hpo:key:`SEED <INIT.SEED>` in an :hpo:block:`INIT` or :hpo:block:`RESTART`
block seeds the points its lines draw; that of :hpo:block:`!INIT` also seeds an
NLopt optimiser without a seed of its own. Without a seed, ``tadah hpo`` draws
one for the run and names it on the terminal and in the journal.

Setting every seed is recommended: the run can then be repeated, and two
searches compared on the same random numbers. A seeded run gives the same
results at any :hpo:key:`!THREADS` under the conditions of the
:hpo:key:`!THREADS` entry. A run on several MPI ranks takes
:hpo:opt:`!TADAH:ANNEAL_PT` or :hpo:opt:`!TADAH:ANNEAL_PA` under
:hpo:key:`PLAIN` only (:hpo:topic:`RULE.multi_rank_optimiser`).
