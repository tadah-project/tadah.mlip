.. _nfp_unit_6:

Unit 6: Annealing
=================

This unit searches the three search parameters of :ref:`nfp_unit_2` with two
annealing optimisers and sets their temperatures from the loss. A short run
measures the hottest temperature, and a longer run fixes it. It follows
:ref:`nfp_unit_5`; :ref:`nfp_annealing` describes these optimisers.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

What this unit adds
-------------------

- A cooling :term:`search` with :hpo:opt:`TADAH:ANNEAL`
  (:ref:`nfp_annealing_choice`).
- A short calibration followed by a four-replica search with
  :hpo:opt:`TADAH:ANNEAL_PT` (:ref:`nfp_annealing_recipe`).
- A comparison with the local search of Unit 2 on the same boxes and loss.

Files
-----

The search reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and :download:`Unit 2's validation
configuration <../files/tutorial/unit2/config.val>`, with their :ref:`datasets
<nfp_unit_2>`. The :term:`target files <target file>` keep the boxes and loss
terms of Unit 2.

:download:`anneal.hpo <../files/tutorial/unit6/anneal.hpo>`:

.. literalinclude:: ../files/tutorial/unit6/anneal.hpo
   :language: none

:download:`anneal_pt_calibrate.hpo
<../files/tutorial/unit6/anneal_pt_calibrate.hpo>`:

.. literalinclude:: ../files/tutorial/unit6/anneal_pt_calibrate.hpo
   :language: none

:download:`anneal_pt.hpo <../files/tutorial/unit6/anneal_pt.hpo>`:

.. literalinclude:: ../files/tutorial/unit6/anneal_pt.hpo
   :language: none

Running the searches
--------------------

The commands run in ``tutorial/unit6``. Each run rewrites the journal and
the history files of the one before (:ref:`nfp_output_files`).

The three searches take about eight minutes in all on two processor cores.

#. The first search cools a single chain:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train --validation ../unit2/config.val \
          --hpotarget anneal.hpo

   The journal reports the starting temperature from the :hpo:param:`T0_PROBES`
   probes (:ref:`nfp_annealing_temperatures`), then one line per temperature
   step (``rung``) with the share of moves accepted, which falls as the chain
   cools. When the schedule ends at :hpo:param:`T_MIN` with :term:`evaluations
   <evaluation>` left, :hpo:param:`REANNEAL` restarts it from the best point,
   at a lower temperature:

   .. output: unit6 hpo_run.log search=anneal.hpo tol=calibrated_t_hot
   .. code-block:: text

      INFO      ANNEAL rung=29/29  T=1.018e-02  proposed=9  accepted=0 (0.0%)  oob=0
          (0.0%)  f_x=1.427e+01  best=1.427e+01
      INFO      ANNEAL re-anneal #1: T_start=2.411e-01  from best=1.427e+01

#. The second search measures the temperatures of a parallel-tempering
   search with four replicas:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train --validation ../unit2/config.val \
          --hpotarget anneal_pt_calibrate.hpo

   The journal reports two measurements of :hpo:param:`T_HOT`, the hottest
   temperature, the second after a warm-up that sets the steps of each slot.
   With this short budget the warm-up stops at a quarter of :hpo:key:`MAXEVAL`
   (:hpo:param:`STEP_MODE`) and warns about it. The second measurement reports:

   .. output: unit6,unit6_short hpo_run.log search=anneal_pt_calibrate.hpo tol=calibrated_t_hot
   .. code-block:: text

      INFO      ANNEAL_PT calibration pass 2: T_HOT=3.088e-01 from probes with the
          proposal scales of the hottest slot after the step warm-up, which stopped at
          its budget (pass 1 used the STEP_FRAC scales; T_HOT of pass 2 / pass 1 =
          0.32); the ladder is rebuilt

   ``hpo_step_scales.tsv`` records the steps that the search keeps, as
   fractions of the box (``sigma/width``): for every slot and search parameter,
   the default :hpo:param:`STEP_FRAC` over the square root of the number of
   search parameters, as the warm-up tested none (``status`` ``interrupted``).
   Its ``temperature`` column holds the first ladder. A hint says that, at a
   typical exchange rate, the planned exchange rounds fall short of one round
   trip of the ladder. A last warning says that the coldest slot never
   exchanged its state: the ladder is cut next to it.

#. The third search fixes the hottest temperature that the second one
   measured, a coldest temperature chosen from the loss, and the moves that
   the second one ended with:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train --validation ../unit2/config.val \
          --hpotarget anneal_pt.hpo

   It spends no evaluations on probes or a warm-up. The journal reports the
   temperature of each slot and, every :hpo:param:`SWAP_WINDOW` epochs, the
   share of accepted exchanges of each pair of neighbouring slots and the
   ladder flow (:ref:`nfp_annealing_replicas`). The summary at the end gives
   both over the whole search, and how many walkers each slot has held:

   .. output: unit6 hpo_run.log search=anneal_pt.hpo
   .. code-block:: text

      INFO      ANNEAL_PT swap acceptance per pair (cold->hot):  1:11%  2:37%  3:34%

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit6/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit6/expected.csv
   :header-rows: 1
   :widths: 60 40

Reading the result
------------------

``anneal.hpo`` and ``anneal_pt.hpo`` both end below the best loss of Unit 2:
``anneal.hpo`` at 14.27 and ``anneal_pt.hpo`` at 14.26. The search of
``anneal_pt.hpo`` ends in another region of the box than the local search of
Unit 2. Its pair cutoff :ref:`RCUT2B` is 6.30 Å, its density cutoff
:ref:`RCUTMB` 4.16 Å and the density decays more slowly, with :ref:`SGRIDMB`
0.0653 Å⁻². Losses this close at such different points suggest a loss with
several minima or a long flat valley, along which an annealer can move.

The best density cutoff lies within one proposal step of its lower bound, where
the journal warns (:ref:`nfp_annealing_steps`), but more than 2 % of the width
away, so the final diagnostics omit it (:ref:`nfp_search_space_bounds`).

The calibration run ends at 14.58, at a probe of the first measurement: most
of its budget measures the temperatures and the steps.

In the production run every pair of neighbouring slots exchanges, the coldest
pair least often (the summary quoted in step 3).

How the values were chosen
--------------------------

The temperatures are in units of the loss. :hpo:param:`T_MIN` of ``anneal.hpo``
and :hpo:param:`T_COLD` of ``anneal_pt.hpo`` are both 0.01, the smallest rise
of the loss worth resolving. A colder end would spend evaluations on
differences that do not matter.

``anneal_pt.hpo`` fixes :hpo:param:`T_HOT` at the calibrated value,
0.309. A temperature belongs to the moves it was measured with
(:ref:`nfp_annealing_steps`), so the file fixes those moves too:
:hpo:param:`STEP_MODE` ``FIXED`` with the :hpo:param:`STEP_FRAC` that the
``sigma/width`` column of ``hpo_step_scales.tsv`` holds for every slot and
search parameter. With :hpo:param:`!STEP_MODE` ``FIXED`` and the larger
:hpo:param:`!STEP_FRAC` of ``anneal.hpo``, the moves raise the loss more, and
this temperature would rarely accept a typical rise.

The replicas share :hpo:key:`MAXEVAL`: each epoch costs one evaluation per
replica, as the comment of ``anneal_pt.hpo`` counts. A
:hpo:param:`SWAP_WINDOW` of 40 epochs gives each pair 20 exchange attempts
(:ref:`nfp_annealing_terms`), enough to read its exchange rate.

Both parallel-tempering files set :hpo:param:`REPLICAS` to 4, so that a change
of :hpo:key:`THREADS` changes the speed of the search, not its ladder
(:ref:`nfp_annealing_replicas`). With one or four workers, the seeded run writes
the same history files and best potential, apart from the times they record.

What to try next
----------------

- :hpo:param:`CORANA` ``ON`` and a larger :hpo:key:`MAXEVAL` in ``anneal.hpo``:
  one search parameter moves at a time, by a step adapted to its acceptance.
- Another :hpo:key:`SEED <OPTIMIZER.SEED>`: a second search on other random
  numbers shows how much the result depends on them.
- In ``anneal_pt.hpo``, :hpo:param:`STEP_MODE` ``FROZEN`` with a
  :hpo:key:`!MAXEVAL` four times the complete warm-up the second search warns
  of, or ``ADAPTIVE`` with :hpo:param:`LADDER_ADAPT` ``ON`` for uneven pairs.
- A local search from the best point of ``anneal_pt.hpo``
  (:ref:`nfp_optimizer_block_initialisation`).

Next: :ref:`nfp_unit_7`.
