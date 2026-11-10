.. _nfp_annealing:

Annealing Optimisers
====================

This guide shows how to choose an annealing optimiser, its temperatures, moves,
replicas, starting points and stop. It builds on :ref:`nfp_choosing` and
:ref:`nfp_optimizer_block`. :ref:`nfp_unit_6` runs the searches behind the
fragment; :ref:`nfp_ref_anneal_params` lists every annealing
:hpo:key:`PARAM <OPTIMIZER.PARAM>` key.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_annealing_choice:

Choosing an annealer
--------------------

An annealer evaluates a proposal near its current point in the :hpo:key:`OPTIM`
box. A lower or equal loss is always accepted, a higher one with a probability
that falls as the rise grows relative to the temperature. A high temperature
lets the search leave a basin; a low one keeps it near its best point. A
proposal scoring :hpo:key:`FAILSCORE` is never accepted; a graded failure
(:hpo:key:`FAILSCORE_GRADING`) counts as a large loss, so a search can leave a
failed region (:ref:`nfp_targets_failures`).

:hpo:opt:`TADAH:ANNEAL` is the recommended first choice. It runs one chain,
measures its starting temperature and spreads :hpo:key:`MAXEVAL` over its
cooling, so a first search sets only :hpo:key:`!MAXEVAL`, and
:hpo:key:`OPTIMIZER.SEED` to repeat it. The replicas of
:hpo:opt:`TADAH:ANNEAL_PT` (parallel tempering) run on a ladder of temperatures
that does not cool, and exchange their points. It suits several cores and a
loss with several deep basins, where one cooling chain settles in the first.

:hpo:opt:`TADAH:ANNEAL_PA` (population annealing) cools a population of
walkers: after each temperature step, walkers with a low loss are copied and
failed ones dropped. It suits a loss with failed regions and a large budget.
A fixed :hpo:key:`POPULATION <OPTIMIZER.POPULATION>`, the number of walkers, is
recommended, as for :hpo:param:`REPLICAS` (:ref:`nfp_annealing_replicas`).

.. _nfp_annealing_terms:

Terms
~~~~~

The run journal (:ref:`nfp_output_histories`) counts work in these units:

proposal
   A point offered to the acceptance rule: one evaluation, accepted or not.
temperature step
   The work at one temperature: a ``rung`` of a chain, or
   :hpo:param:`SWEEPS` rungs of each walker of :hpo:opt:`!TADAH:ANNEAL_PA`.
replica, slot
   A chain of :hpo:opt:`!TADAH:ANNEAL_PT`, and its place on the temperature
   ladder, numbered from the coldest. A walker of :hpo:opt:`!TADAH:ANNEAL_PA`
   and a point of an :hpo:opt:`ASA:ASA` batch also have a slot (``SLOT``).
sweep, epoch
   One step of a replica: one proposal, or one per search parameter under the
   Corana kernel. An epoch is one sweep of every replica.
exchange round
   Attempts to exchange the points of neighbouring slots (pairs); every
   other pair takes part, alternating from round to round.
walker, window
   A walker is a point followed through the exchanges, or a population member
   of :hpo:opt:`!TADAH:ANNEAL_PA`. A window is :hpo:param:`SWAP_WINDOW` epochs.

.. _nfp_annealing_temperatures:

Choosing temperatures
---------------------

A temperature has the unit of the loss (:ref:`nfp_targets_losses`): a rise much
smaller than it is almost always accepted, one much larger almost never.

The first temperature is :hpo:param:`T_INIT` (:hpo:param:`T_HOT` of the other
annealers) when the target file sets it. Otherwise the annealer derives it from
the rises of :hpo:param:`T0_PROBES` proposals around the starting point
(:hpo:param:`T0_ACCEPT`). After a failed start, the search moves to the best
of as many points drawn from the box and probes there; when all of these score
:hpo:key:`FAILSCORE`, the search stops with an error. When every probe around a
usable point scores :hpo:key:`!FAILSCORE`, or none raises the loss and most
leave it unchanged, a WARNING says so and the temperature of that calibration
falls back to 1.0. The default :hpo:param:`!T0_ACCEPT` starts cold, near the
starting point; a larger value starts hotter and explores further.

:hpo:opt:`TADAH:ANNEAL` cools by :hpo:param:`RT` after each temperature step,
down to :hpo:param:`T_MIN`. By default, :hpo:param:`CHAIN_LENGTH`
(:hpo:param:`NT` under the Corana kernel) spreads the evaluations left after the
probes over the steps of that schedule; with too few evaluations, the search
ends above :hpo:param:`!T_MIN`. The default :hpo:param:`!T_MIN` lies far
below most loss differences that matter; at the smallest difference worth
resolving (``anneal.hpo`` of Unit 6), it shortens the schedule and gives each
temperature more proposals. When the schedule reaches :hpo:param:`!T_MIN` with
evaluations left, :hpo:param:`REANNEAL` starts it again from the best point and
a lower temperature. :hpo:opt:`TADAH:ANNEAL_PA` cools from :hpo:param:`!T_HOT`
to :hpo:param:`!T_MIN`, by steps that :hpo:param:`ESS_TARGET` sizes from the
walkers' losses, never faster than :hpo:param:`!RT`.

:hpo:opt:`TADAH:ANNEAL_PT` keeps a ladder from :hpo:param:`!T_HOT` down to
:hpo:param:`T_COLD` (by default :hpo:param:`!T_HOT` over
:hpo:param:`LADDER_SPAN`). Under the default moves, a measured
:hpo:param:`!T_HOT` takes two passes: with the :hpo:param:`STEP_FRAC` moves,
then with those of the hottest slot after the step warm-up
(:ref:`nfp_annealing_steps`). The recommended practice fixes the ladder:
:hpo:param:`!T_HOT` from the ``calibration pass 2`` line of a short run without
it, :hpo:param:`!T_COLD` at the smallest loss difference worth resolving. Fixed
temperatures cost no probes and do not depend on :hpo:key:`MAXEVAL`, as a
measured ladder does when the warm-up stops at its share of the budget. In
Unit 6, ``anneal_pt.hpo`` fixes the ladder after ``anneal_pt_calibrate.hpo``:

.. literalinclude:: ../files/tutorial/unit6/anneal_pt.hpo
   :language: none
   :start-at: PARAM    T_HOT
   :end-at: PARAM    T_COLD

.. _nfp_annealing_steps:

Choosing moves
--------------

Moves are made in the coordinates of the search
(:ref:`nfp_search_space_transforms`). The default CLASSIC kernel changes every
search parameter at once, by a Gaussian step of :hpo:param:`STEP_FRAC` times the
width of its box; an annealer refuses :hpo:key:`STEP`. :hpo:param:`CORANA`
``ON`` moves one search parameter at a time and adapts the step of each from its
acceptance (:hpo:param:`NS`, :hpo:param:`CORANA_C`). It suits search parameters
of very different sensitivity, with a budget for at least :hpo:param:`!NS`
evaluations per search parameter at each temperature. The CLASSIC kernel is the
recommended first choice: each evaluation moves every search parameter.

A move that leaves the box is folded back into it by :hpo:param:`BOUNDARY_MODE`
``REFLECT``, the default and recommended choice. ``REDRAW`` draws the coordinate
again: the moves are then not symmetric, and the chain visits the region near a
bound less often than it should. At each ``SUMMARY`` report the journal warns
when a chain or, in a run on one MPI rank, a replica or a walker lies within one
proposal step (``tol``) of an :hpo:key:`OPTIM` bound; under :hpo:param:`!CORANA`
``ON``, once that step is at its :hpo:param:`V_FLOOR_FRAC` floor. A warning that
persists suggests a wider or re-centred box (:ref:`nfp_search_space_bounds`).

Under the CLASSIC kernel, :hpo:opt:`TADAH:ANNEAL_PT` sets the steps of each slot
by :hpo:param:`STEP_MODE`. The recommended ``FROZEN`` measures them in a
warm-up, writes them to ``hpo_step_scales.tsv`` (:ref:`nfp_output_files`), then
keeps them. ``ADAPTIVE`` keeps adapting them, for a ladder that moves
(:hpo:param:`LADDER_ADAPT`). ``FIXED`` applies one :hpo:param:`!STEP_FRAC` to
every slot and search parameter, without a warm-up or a second temperature pass.

A fixed :hpo:param:`T_HOT` belongs to the moves it was measured with. ``FROZEN``
measures the steps again at the fixed temperatures. ``FIXED`` keeps the moves of
a ``FIXED`` measuring run with the same :hpo:param:`!STEP_FRAC`
(:ref:`nfp_example_anneal_pt_full`), or of a run whose ``hpo_step_scales.tsv``
holds one sigma/width value, set as :hpo:param:`!STEP_FRAC` (Unit 6).

.. _nfp_annealing_replicas:

Replicas and exchanges
----------------------

:hpo:param:`REPLICAS` sets the number of temperatures, :hpo:key:`THREADS` the
workers that evaluate them; without :hpo:key:`!THREADS`, the replicas run one
at a time. The default of :hpo:param:`!REPLICAS` follows :hpo:key:`!THREADS`; a
fixed value is recommended, so that a change of :hpo:key:`!THREADS` changes the
speed of the search, not its ladder (:ref:`nfp_optimizer_block_threads`).

Every :hpo:param:`SWAP_EVERY` epochs, neighbouring slots try to exchange their
points. A lower loss always moves to the colder slot; a higher one moves with a
probability that falls with the loss difference and the gap between the
temperatures. An exchange costs no evaluation, and each slot keeps its steps.
The default, a round after every epoch, is recommended: frequent exchanges
carry what the hot replicas find to the cold end fastest. The ladder is
geometric by default (:hpo:param:`LADDER_POWER`); :hpo:param:`LADDER_ADAPT`
``ON`` moves the inner temperatures until every pair exchanges equally often;
:hpo:param:`STEP_MODE` ``ADAPTIVE`` then keeps the steps of each slot matched
to its moving temperature.

In a run on one MPI rank, the journal reports, every window, the exchange rate
of each pair, the place of each walker and the ladder flow: how often a walker
has crossed the ladder from end to end, in each direction, and the round trips
this makes. The flow shows whether the method works: exchange rates can look
healthy while no walker crosses the ladder, and the replicas then run as
independent annealers. A WARNING reports a stretch of exchange rounds without a
crossing. Under :hpo:param:`SWAP_WINDOW` ``OFF`` the journal reports these for
the whole run, at the end, and gives no such WARNING.

A pair with a low rate has temperatures too far apart: more replicas, a
narrower ladder or :hpo:param:`!LADDER_ADAPT` close the gap. Many exchanges
between equal losses usually mean failed evaluations on both sides.

.. _nfp_annealing_initialisation:

Choosing starting points
------------------------

An annealer starts at the point the search is given
(:ref:`nfp_optimizer_block_initialisation`). :hpo:opt:`TADAH:ANNEAL_PT` starts
its coldest slot there, :hpo:opt:`TADAH:ANNEAL_PA` its first walker, and
:hpo:param:`INIT_MODE` places the others, at one evaluation each. ``JITTER``,
the default, places them around the start with the first proposal steps, and
suits a trusted start; ``JITTER <fraction>`` bounds each displacement by a
fraction of the box. Under :hpo:opt:`!TADAH:ANNEAL_PT`,
:hpo:param:`INIT_JITTER_LADDER` by default shrinks these displacements towards
the cold end, so that colder replicas start nearer the start. ``UNIFORM`` draws
them from the box, for a start that may lie in a narrow, untypical basin.

``JITTER AUTO <p>`` of :hpo:opt:`!TADAH:ANNEAL_PT` measures, for each search
parameter, how far a displacement keeps the rise of the loss within a relative
tolerance *p*, a fraction of the starting loss. It suits search parameters of
very different sensitivity; :hpo:param:`INIT_JITTER_SAMPLE` limits its cost for
many of them. Its probes are evaluations outside :hpo:key:`MAXEVAL`: they take
time, and a probe with a better loss becomes the best potential. It writes the
measured displacements to ``hpo_init_jitter.tsv`` (:ref:`nfp_output_files`).

.. _nfp_annealing_stopping:

Choosing a stop
---------------

:hpo:key:`MAXEVAL`, the recommended stop, counts the starting points, the
temperature probes, the step warm-up and every proposal. Without it,
:hpo:opt:`TADAH:ANNEAL_PA` ends at :hpo:param:`T_MIN`; :hpo:opt:`TADAH:ANNEAL`
ends there once reannealing is spent, if :hpo:param:`CHAIN_LENGTH` or
:hpo:param:`NT` gives a number; :hpo:opt:`TADAH:ANNEAL_PT` needs another stop.

:hpo:key:`OPTIMIZER.STOPVAL` ends the search at a usable loss at or below a
known good-enough value. :hpo:opt:`!TADAH:ANNEAL_PT` and
:hpo:opt:`!TADAH:ANNEAL_PA` end after the epoch or temperature step in which it
is reached, so their end does not depend on the timing of the evaluations.
:hpo:key:`FTOL_REL` or :hpo:key:`FTOL_ABS` above 0 ends a search whose loss
stops improving over :hpo:param:`NEPS` temperature steps, or windows of
:hpo:opt:`!TADAH:ANNEAL_PT`; it suits an open-ended :hpo:opt:`!TADAH:ANNEAL_PT`
run. :hpo:key:`OPTIMIZER.MAXTIME` bounds the time.

.. _nfp_annealing_recipe:

Setting up a parallel-tempering search
--------------------------------------

A :hpo:opt:`TADAH:ANNEAL_PT` search is set up in this order:

#. :hpo:key:`MAXEVAL` as the run can afford; the journal's plan gives the
   epochs and exchange rounds left after the probes and the warm-up, with a
   WARNING or a HINT when they are too few for one round trip.
#. :hpo:key:`THREADS` up to the cores, :hpo:param:`REPLICAS` fixed.
#. :hpo:param:`T_HOT` from a first run, then it and :hpo:param:`T_COLD` fixed.
#. :hpo:param:`STEP_MODE` as :ref:`nfp_annealing_steps` recommends.
#. :hpo:param:`LADDER_ADAPT` ``ON`` when the pairs exchange very unevenly.
#. :hpo:key:`OPTIMIZER.STOPVAL` or :hpo:key:`FTOL_REL` as an extra stop.
#. The ladder flow in the journal is read first, before any setting changes.
