.. _nfp_example_restart:

Restarting a Stagnating Search
==============================

This example runs a local :term:`search` once without restarts and once
with a :hpo:block:`RESTART` block, on the same budget, and compares the
results. It uses the training and validation configurations of
:ref:`nfp_unit_2`; :ref:`nfp_optimizer_block_restart` covers the restart
settings.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When restarts help
------------------

A local search can stagnate long before its budget is spent. A
:hpo:block:`RESTART` block then starts new searches from other points once
a window of :hpo:key:`AFTER_STAGNATION` calls brings no new best loss of
the :term:`run` (:ref:`nfp_optimizer_block_restart`). The restarts cost
evaluations only after a search has stagnated.
:hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>` instead shares the budget
among its starts in advance (:ref:`nfp_example_multistart`).

Restarts are the recommended choice for an unattended local search from a
guessed start, when the search may stagnate far above the lowest loss in
the boxes. They do not help a search that keeps finding small gains: each new
best starts the window again. This example measures what three restarts
cost and gain for the search of :ref:`nfp_unit_2`.

Files
-----

The example reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`validation configuration <../files/tutorial/unit2/config.val>`,
with their :ref:`datasets <nfp_unit_2>`. Both target files keep the
:term:`search space` and the :term:`loss terms <loss term>` of
:ref:`nfp_unit_2`.

:download:`single.hpo <../files/examples/restart/single.hpo>` runs one
search of :hpo:opt:`NLOPT:LN_SBPLX` from the training configuration:

.. literalinclude:: ../files/examples/restart/single.hpo
   :language: none

:download:`config.hpo <../files/examples/restart/config.hpo>` runs the same
search with a :hpo:block:`RESTART` block:

.. literalinclude:: ../files/examples/restart/config.hpo
   :language: none

Running the searches
--------------------

The commands run in ``examples/restart`` (:ref:`layout of the files
<nfp_tutorial_files>`):

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget single.hpo

Each run starts afresh: the second replaces the journal, the history files
and ``best_pot.tadah`` of the first (:ref:`nfp_output_files`). The results
of ``single.hpo`` are therefore read, or copied, before the second search:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The two searches take about five minutes in all on one processor core.

The journal of each run has the fit warnings that :ref:`nfp_unit_2` describes.
On the terminal, an evaluation in either run can also warn that all
coefficients of the many-body descriptor are exactly zero; its loss is high,
and the search moves on (:ref:`nfp_unit_5`).

Expected result
---------------

:download:`expected.csv <../files/examples/restart/expected.csv>`:

.. csv-table::
   :file: ../files/examples/restart/expected.csv
   :header-rows: 1
   :widths: 60 40

Both runs end at a best loss of 14.19. The run with restarts makes 193
evaluations, the search without them about 195, on the same budget of calls
(below). A call at a point evaluated before is answered from the run's
record: it counts against the budget, but it is not an evaluation
(:ref:`nfp_optimizer_block_budgets`).

The journal of ``config.hpo`` counts the calls answered from the record, and
gives the status of the run, a stop on stagnation after the three restarts,
and the evaluation of the best loss:

.. output: restart,restart_short hpo_run.log search=config.hpo tol=best_loss

.. code-block:: text

   Repeated points reused  : 14 (points the run had evaluated that a search asked
       for, answered with the
   ...
   -- RESULT --------------------------------------------------------------
     Optimiser status        : STOPPED -- stagnation: no new best in 20 evaluations
         after 3 restart(s)
     Best total loss         : 1.4195e+01  (eval 119)

Reading the result
------------------

``params.tadah`` and ``loss.tadah`` of ``config.hpo`` hold the four searches
one after the other. The first search makes the same evaluations as
``single.hpo`` until its window fills. Each restart then starts at a point
of a Sobol sequence in the boxes. None of the three new searches beats the
best loss of the first within its window, so each one ends after one
window, and the :term:`best potential` comes from the first search.

The journal records where the window of each search fills, with the calls
in it that the run's record answered, and each restart, with the method
and the seed of its points:

.. output: restart,restart_short hpo_run.log search=config.hpo

.. code-block:: text

   INFO      the stagnation stop at a repeat of eval 137 answered from the run's
       record, after eval 138: no new best within AFTER_STAGNATION 20 evaluations,
       1 of them calls answered from the run's record; the search stops
   INFO      RESTART 1/3: new starting points from SOBOL; seeds of this round:
       RESTART SEED + 1 = 1
   ...
   INFO      RESTART: no new best within the stagnation window, and the MAX of 3
       restart(s) is used: the run stops

``single.hpo`` goes on from where the window of ``config.hpo`` ended the
first search. ``config.hpo`` spends those calls on the restarts instead,
whose new searches find nothing lower.

Here the first search ends at a low loss: each new search from another
point is still above it when its window fills, so no restart beats it. A
restart pays when a search stagnates well above the loss that a new search
reaches within one window.

Both best values of :ref:`RCUTMB` lie at the upper bound of its box, which
the journal notes, here that of ``config.hpo``:

.. output: restart,restart_short hpo_run.log search=config.hpo tol=best_rcutmb

.. code-block:: text

   Bound occupancy of the final best (eval 119) sits within 2% of an OPTIM limit
       on 1 dim(s) (gap = fraction of width):
   dim                     side  value         bound         gap/width  since eval
   RCUTMB                  HI    7.0000e+00    7.0000e+00    0.00e+00   7

Both files keep the boxes of :ref:`nfp_unit_2`, so that the example
measures restarts on the search of that unit; :ref:`nfp_search_space_bounds`
covers a best value on a bound.

How the values were chosen
--------------------------

The :hpo:block:`RESTART` block must stand inside :hpo:block:`STRATEGY`
(:ref:`nfp_target_file_blocks`); ``config.hpo`` puts the optimiser keys
beside it there:

.. literalinclude:: ../files/examples/restart/config.hpo
   :language: none
   :lines: 16-23
   :dedent: 8

- :hpo:opt:`NLOPT:LN_SBPLX` replaces :hpo:opt:`NLOPT:LN_BOBYQA` of
  :ref:`nfp_unit_2` (:ref:`nfp_choosing_local`). On this loss
  :hpo:opt:`!NLOPT:LN_BOBYQA` still finds a new best at the last of the 75
  evaluations of :ref:`nfp_unit_2`; :hpo:opt:`!NLOPT:LN_SBPLX` stagnates
  within its budget.
- :hpo:key:`MAXEVAL` bounds each search separately. A search that reaches
  it ends the run, restarts left or not
  (:ref:`nfp_optimizer_block_restart`), so the value leaves room for the
  first search to stagnate.
- :hpo:key:`STEP <OPTIMIZER.STEP>` 0.25 suits a guessed start, as in
  :ref:`nfp_unit_2` (:ref:`nfp_choosing_step`).

The window and the number of restarts:

.. literalinclude:: ../files/examples/restart/config.hpo
   :language: none
   :lines: 24-31
   :dedent: 8

- :hpo:key:`AFTER_STAGNATION` is both the window of a stagnating search
  and all that a restart gets to beat the best loss of the run. Its value
  comes from the pauses between new bests of ``single.hpo``. It is longer
  than each pause up to the best of the first search, and shorter than
  the next one, which ends that search. After that pause ``single.hpo``
  finds only the small gains described above.
- :hpo:key:`MAX` 3 bounds the extra cost. Restarts that beat nothing cost
  at most three windows; one that beats the best runs on until it stagnates
  or reaches :hpo:key:`!MAXEVAL`.

The starting points of the restarts:

.. literalinclude:: ../files/examples/restart/config.hpo
   :language: none
   :lines: 32-35
   :dedent: 8

- :hpo:key:`SOBOL` 1 gives each restart one point of a Sobol sequence,
  which spreads successive points over the boxes
  (:ref:`nfp_optimizer_block_initialisation`).
- :hpo:key:`SEED <INIT.SEED>` 0 fixes these points, so every run starts its
  restarts at the same three points and gives the same result.

The :hpo:key:`!MAXEVAL` of ``single.hpo`` is set to the number of calls
that the run of ``config.hpo`` makes in all, so that both runs have the
same budget.

Variations
----------

Without the :hpo:key:`METHOD <INIT.METHOD>` and
:hpo:key:`SEED <INIT.SEED>` lines of the :hpo:block:`RESTART` block, every
restart starts at the values of the training configuration
(:ref:`nfp_optimizer_block_restart`). :hpo:opt:`NLOPT:LN_SBPLX` draws no
random numbers, so each restart retraces the first search, and the run
answers its calls from its record.

A longer window, such as :hpo:key:`AFTER_STAGNATION` 30, lets the first
search run on. Once it reaches :hpo:key:`MAXEVAL`, the run ends without a
restart (:ref:`nfp_optimizer_block_restart`).

Without :hpo:key:`!SEED <INIT.SEED>`, the restarts start at other points in
each run; the journal names the seed drawn and the line that draws the same
points again (:ref:`nfp_optimizer_block_threads`).
