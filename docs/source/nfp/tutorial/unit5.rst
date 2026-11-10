.. _nfp_unit_5:

Unit 5: A Global Search
=======================

This unit searches the three search parameters of :ref:`nfp_unit_2` over
wider bounds, without a trusted starting point. A global exploration finds the
region of low loss, and local searches from its best points finish it. A
random search with the same number of evaluations shows whether the
exploration pays off. The unit follows :ref:`nfp_unit_4`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

What this unit adds
-------------------

- A :hpo:block:`STRATEGY` block with :hpo:key:`TYPE <STRATEGY.TYPE>`
  :hpo:key:`PRESTAGE`: an exploration in an :hpo:block:`EXPLORE` block, then
  local searches from its :hpo:key:`TOPK` best points with the optimiser of
  the :hpo:block:`REFINE` block (:ref:`nfp_optimizer_block_strategies`).
- A global optimiser, :hpo:opt:`NLOPT:GN_DIRECT_L`, which needs no starting
  point (:ref:`nfp_choosing_global`).
- A random baseline, :hpo:opt:`TADAH:RANDOM`, with a
  :hpo:key:`SEED <OPTIMIZER.SEED>` (:ref:`nfp_choosing_baselines`).
- Two runs of ``tadah hpo``, one after the other in one directory.

Files
-----

The search reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`Unit 2's validation configuration
<../files/tutorial/unit2/config.val>`, with their :ref:`datasets <nfp_unit_2>`.
The two target files below have the same search parameters, bounds and loss
terms; only their :hpo:block:`OPTIMIZER` blocks differ.

:download:`random.hpo <../files/tutorial/unit5/random.hpo>`, the baseline:

.. literalinclude:: ../files/tutorial/unit5/random.hpo
   :language: none

:download:`config.hpo <../files/tutorial/unit5/config.hpo>`, the global search:

.. literalinclude:: ../files/tutorial/unit5/config.hpo
   :language: none

Running the searches
--------------------

The commands run in ``tutorial/unit5``.

1. The random baseline is started with:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train \
          --validation ../unit2/config.val --hpotarget random.hpo

   It evaluates the values of the training configuration first, then
   random points in the box (:ref:`nfp_choosing_baselines`).

The global search writes its files over those of the baseline
(:ref:`nfp_output_files`), so the best loss of the baseline is read from the
``RESULT`` section of ``hpo_run.log`` first. Its ``best_pot.tadah`` is copied
to another name, such as ``random_pot.tadah``, to keep it.

2. The global search is started with:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train \
          --validation ../unit2/config.val --hpotarget config.hpo

   The exploration evaluates the centre of the box first, then divides the
   box into rectangles (:ref:`nfp_choosing_global`); the three local
   searches follow.

The two searches take about five minutes in all on one processor core.

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit5/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit5/expected.csv
   :header-rows: 1
   :widths: 60 40

Both runs make 177 evaluations. The global search ends at a loss of
13.55, the random search at 14.00. With as many evaluations, the
exploration and its local searches find a lower loss than the random points
of this seed. The best :ref:`RCUTMB`, 3.92 Å, lies below the lower bound of
the box of Unit 2, where the search of Unit 2 could not go. ``hpo_run.log``
reports the best point away from every bound, as in :ref:`nfp_unit_1`
(:ref:`nfp_search_space_bounds`), and gives why the global search ended and
the validation errors of its best potential:

.. output: unit5,unit5_short hpo_run.log search=config.hpo tol=2e-2
.. code-block:: text

   [ok] Final best sits away from every OPTIM bound (edge 2% of width).
   ...
   Optimiser status        : MAXEVAL_REACHED -- maximum evaluations exhausted
   Best total loss         : 1.3545e+01  (eval 119)
   Best ERMSE              : 10.41 meV/atom
   Best FRMSE              : 0.3331 eV/Ang
   Best SRMSE              : 0.7829 GPa

Reading the result
------------------

``hpo_run.log`` of the global search reports the exploration, then each
local search (:ref:`nfp_output_histories`). After the exploration it lists
the losses of the points handed to the local searches. Each local search has
30 calls of its optimiser.

The first call asks for its starting point, a point of the exploration: the
run answers it from its record (:ref:`nfp_optimizer_block_budgets`), and the
journal says so. Each local search thus adds one evaluation fewer than its
share, and the run makes 177 evaluations in all. The journal ends each local
search with its evaluations and whether it improved the best loss of the run,
as for the first and the last:

.. output: unit5,unit5_short hpo_run.log search=config.hpo tol=best_loss
.. code-block:: text

   INFO      NLopt finished: MAXEVAL_REACHED -- maximum evaluations exhausted after
       29 of MAXEVAL 30 evaluation(s); NLopt counts its calls, and 1 of them asked
       for points the run had evaluated and were answered without an evaluation
   INFO      REFINEMENT start 1/3 done: 29 evaluation(s) consumed, improved global
       best 1.3591e+01 -> 1.3545e+01
   ...
   INFO      REFINEMENT start 3/3 done: 29 evaluation(s) consumed, no global
       improvement (best stays at 1.3545e+01)

``params.tadah`` and ``loss.tadah`` give every evaluation in order: the
exploration first, then each local search (:ref:`nfp_output_histories`). Here
the three starts lie in one valley (:hpo:key:`TOPK` drops only near-copies),
and the second and third local searches find no lower loss.

Points far from the valley of low loss give large losses. A density cutoff
and width that place every density of the data past the last embedding
function leave the many-body term, descriptor 2, with only zero coefficients.
The terminal warns at the first such evaluation and, under the default
:ref:`HEALTH_LOG`, at most once per 100 evaluations after it. Such a point is
a valid evaluation with a high loss, expected in a box this wide.

How the values were chosen
--------------------------

The training and validation configurations are those of Unit 2, unchanged:
the unit is about the search, not the model. The search parameters are
Unit 2's three, so the results are comparable. The bounds come from the
model and the physics, not from a starting point, as the comments of the
:hpo:key:`OPTIM` lines explain (:ref:`nfp_search_space_bounds`). The box of
:ref:`RCUTMB` reaches below that of Unit 2, and that of :ref:`SGRIDMB` spans
a larger ratio.

:hpo:opt:`NLOPT:GN_DIRECT_L` explores; it draws no random numbers, so the run
repeats without a seed (:ref:`nfp_choosing_global`). It needs a stop: 90
evaluations, 30 per search parameter. It evaluates no starting point, so
``config.hpo`` has no :hpo:block:`INIT` block: the local searches start from
the points of the exploration (:ref:`nfp_optimizer_block_strategies`).

:hpo:key:`TOPK` 3 starts three local searches. The best point of the
exploration may lie in a shallower valley than the second or the third; one
local search per point guards against that. The :hpo:key:`MAXEVAL` of the
:hpo:block:`REFINE` block, 90, is shared among them: 30 each, or 10 per
search parameter (:ref:`nfp_optimizer_block_budgets`). That covers the
first model of :hpo:opt:`NLOPT:LN_BOBYQA` and several steps after it
(:ref:`nfp_choosing_budget`). :hpo:key:`STEP <OPTIMIZER.STEP>` 0.05 keeps
the first steps short: each local search starts at a good point, where
Unit 2 starts from a guess (:ref:`nfp_choosing_step`).

The baseline draws random numbers, so it has a
:hpo:key:`SEED <OPTIMIZER.SEED>`: every run evaluates the same points. Its
:hpo:key:`!MAXEVAL` equals the evaluations of the global search, so the two
are compared at the same cost. It runs first, so that the directory ends with
the best potential of the global search.

What to try next
----------------

- Another :hpo:key:`SEED <OPTIMIZER.SEED>` in ``random.hpo``: a random
  search ends at another loss with each seed, so a fair comparison takes a
  few seeds (:ref:`nfp_choosing_budget`).
- :hpo:key:`TOPK` 1, with the whole :hpo:key:`MAXEVAL` of the
  :hpo:block:`REFINE` block for one local search.
- Another split of the budget: a longer exploration and shorter local
  searches, or the reverse, with the same total.
- :hpo:opt:`NLOPT:GN_CRS2_LM` in the :hpo:block:`EXPLORE` block. It evolves
  a population of random points, so a :hpo:key:`!SEED <OPTIMIZER.SEED>` in
  the :hpo:block:`!EXPLORE` or :hpo:block:`STRATEGY` block makes the run
  repeat (:ref:`nfp_optimizer_block_threads`).

Next: :ref:`nfp_unit_6`.
