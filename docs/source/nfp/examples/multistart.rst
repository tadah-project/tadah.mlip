.. _nfp_example_multistart:

Local Searches from Several Starts
==================================

This example runs a local :term:`search` once from the
:term:`training configuration` and then from six starts, five of them spread
over the :term:`search space`, with the same budget, and compares the
results. It uses the training configuration of :ref:`nfp_unit_2` and an
:hpo:block:`INIT` block of starting points; :ref:`nfp_optimizer_block` covers
the strategies, starting points and budgets it uses.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_example_multistart_minima:

When several starts help
------------------------

A local optimiser, such as :hpo:opt:`NLOPT:LN_BOBYQA`, ends in one minimum,
and which one depends on its start.
:hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>` runs the same local search
from each of the first :hpo:key:`N_STARTS` points of the :hpo:block:`INIT`
block and keeps the :term:`best potential` of all of them. Here
:hpo:key:`LHS <METHOD.LHS>` spreads the points over the boxes. A global
optimiser is the alternative (:ref:`nfp_choosing_global`).

The loss of this example has several minima, most of them along
:ref:`RCUTMB`. In the model of the training configuration, the many-body
energy of an atom is a sum of embedding blips of its density
(:ref:`CEMBFUNC`), each with a fitted coefficient. A larger :ref:`RCUTMB`
raises the densities of the atoms, so the densities of the data move across
the blips, and the loss rises and falls as they do. With the blips at other
densities, the minima lie at other cutoffs: they belong to the model on these
data, not to the search.

Files
-----

The example reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`validation configuration <../files/tutorial/unit2/config.val>`,
with their :ref:`datasets <nfp_unit_2>`.

:download:`single.hpo <../files/examples/multistart/single.hpo>` runs one
search of :hpo:opt:`NLOPT:LN_BOBYQA` from the training configuration:

.. literalinclude:: ../files/examples/multistart/single.hpo
   :language: none

:download:`multistart.hpo <../files/examples/multistart/multistart.hpo>` runs
the same search, with the same boxes and :term:`loss terms <loss term>`, from
six starts:

.. literalinclude:: ../files/examples/multistart/multistart.hpo
   :language: none

Running the searches
--------------------

The commands run in ``examples/multistart`` (:ref:`layout of the files
<nfp_tutorial_files>`). ``single.hpo`` runs first:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget single.hpo

Each :term:`run` starts afresh: the second replaces the journal, the history
files and ``best_pot.tadah`` of the first (:ref:`nfp_output_files`). A copy
of ``hpo_run.log`` made before the second search keeps the journal of
``single.hpo``, which the next section quotes:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget multistart.hpo

The two searches take about five minutes in all on one processor core.

Expected result
---------------

:download:`expected.csv <../files/examples/multistart/expected.csv>`:

.. csv-table::
   :file: ../files/examples/multistart/expected.csv
   :header-rows: 1
   :widths: 60 40

Both searches end at the same best loss, 14.31, at an :ref:`RCUT2B` of
6.63 Å and an :ref:`RCUTMB` of 6.97 Å. The single search stops early, and
the rest of its budget is not used: NLopt ends the search of
:hpo:opt:`NLOPT:LN_BOBYQA` itself when floating-point round-off blocks
further progress. The six starts spend all 300. The journal of
``single.hpo`` gives the status:

.. output: multistart,multistart_short hpo_run.log search=single.hpo
.. code-block:: text

   -- RESULT --------------------------------------------------------------
     Optimiser status        : STOPPED -- NLopt ended the search itself (nlopt
         roundoff-limited); the best evaluation so far is kept

The evaluation at which round-off stops the search, and with it the count of
evaluations, depends on the rounding of the build: the status is the same.

Start 1 begins at the training configuration, the start of ``single.hpo``, so
its 50 evaluations are the first 50 of the single search. Start 5 ends in the
same minimum. The other drawn starts end in other minima, with best losses
from 14.46 (start 6) to 14.84 (start 2). Starts 2 and 4 end at the
same :ref:`RCUTMB`, 4.85 Å, in two minima along :ref:`RCUT2B`, at 6.83 and
6.41 Å.

Here the training configuration is a good start: no drawn start reaches a
lower minimum, and the six starts add no gain over the single search. Their
value is protection against a poor start: a search that starts near a worse
minimum, such as start 2, ends there.

Both best values of :ref:`RCUTMB` lie close to its upper bound. The journal
of ``single.hpo`` notes the best point near a bound and suggests a wider
bound on that side:

.. output: multistart,multistart_short hpo_run.log search=single.hpo tol=best_rcutmb
.. code-block:: text

   dim                     side  value         bound         gap/width  since eval
   RCUTMB                  HI    6.9750e+00    7.0000e+00    8.34e-03   7
   (advisory: a best that rides a bound suggests bound pressure; a wider bound on
    that side and a rerun with INIT METHOD WARM test it; the run never changes
    OPTIM bounds by itself)

``loss.tadah`` and ``params.tadah`` hold the starts one after the other. The
standard output and the journal give the evaluations of each start and
whether it improved the best loss of the run:

.. output: multistart hpo_run.log search=multistart.hpo tol=best_loss
.. code-block:: text

   INFO      MULTISTART start 1/6 done: 50 evaluation(s) consumed, the run's first
       best 1.4309e+01
   ...
   INFO      MULTISTART start 2/6 done: 50 evaluation(s) consumed, no global
       improvement (best stays at 1.4309e+01)
   ...
   INFO      MULTISTART start 5/6 done: 50 evaluation(s) consumed, improved global
       best 1.4309e+01 -> 1.4309e+01

Start 5 improves on start 1 in the same minimum, by less than the printed
digits.

How the values were chosen
--------------------------

The boxes of :ref:`RCUT2B` and :ref:`RCUTMB` are those of :ref:`nfp_unit_2`,
for the reasons the comments give. :ref:`SGRIDMB` is not searched. Every
start is drawn from the whole box. At the value of :ref:`SGRIDMB` in the
training configuration, the densities of the data stay on the embedding blips
for every :ref:`RCUTMB` in its box. With :ref:`SGRIDMB` searched as well, a
drawn start can put the densities beyond every blip, where the many-body term
contributes nothing.

The :hpo:block:`INIT` block gives the six starting points:

.. literalinclude:: ../files/examples/multistart/multistart.hpo
   :language: none
   :lines: 22,26-27
   :dedent: 8

- :hpo:key:`CONFIG <METHOD.CONFIG>` comes first, so start 1 is the start of
  ``single.hpo``.
- :hpo:key:`LHS <METHOD.LHS>` 5 draws five more points, one in each fifth of
  either box (:ref:`nfp_optimizer_block_initialisation`). With the
  :hpo:key:`!CONFIG <METHOD.CONFIG>` point they make six searches of 50 in the
  budget of 300.
- :hpo:key:`SEED <INIT.SEED>` in the :hpo:block:`!INIT` block fixes the drawn
  points, so every run starts from the same six
  (:ref:`nfp_optimizer_block_threads`). :hpo:opt:`NLOPT:LN_BOBYQA` draws no
  random numbers, so the drawn points are the only random part of the run.

The :hpo:block:`STRATEGY` block runs the local search from each point:

.. literalinclude:: ../files/examples/multistart/multistart.hpo
   :language: none
   :lines: 30,32-33,38
   :dedent: 8

- :hpo:key:`N_STARTS` 6 runs a start from every point of the chain.
- :hpo:key:`BUDGET_PER_START` 50 gives each start 25 evaluations per
  :term:`search parameter`, the budget of :ref:`nfp_unit_2`. The run makes at
  most 300 evaluations in all (:ref:`nfp_optimizer_block_budgets`).
- :hpo:key:`STEP <OPTIMIZER.STEP>` 0.25 suits starts that are guesses, as in
  :ref:`nfp_unit_2`. :hpo:opt:`!NLOPT:LN_BOBYQA` moves a drawn start that lies
  within one step of a bound, so that its first step keeps this size
  (:ref:`nfp_choosing_step`).

The journal of ``multistart.hpo`` gives the search parameter, the moved and
the drawn value of each moved start, as for start 2:

.. output: multistart hpo_run.log search=multistart.hpo tol=abs:0.01
.. code-block:: text

   INFO      MULTISTART start 2/6: NLOPT:LN_BOBYQA
   INFO      NLOPT:LN_BOBYQA: RCUT2B starts at 7.125 instead of 7.27337324, one
       step from its upper bound: BOBYQA keeps its full first step only from a
       start on a bound or at least one step from it, and a start within half a
       step of a bound moves onto it

``single.hpo`` has :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` 300, the most the
six starts can spend, so both runs have the same budget.

Variations
----------

Another :hpo:key:`SEED <INIT.SEED>` draws other starts. They can end in other
minima, a lower one among them. With :hpo:key:`CONFIG <METHOD.CONFIG>` in
the chain, the result is never worse than that of the first 50 evaluations
of the single search.

More starts, such as :hpo:key:`LHS <METHOD.LHS>` 9 and :hpo:key:`N_STARTS` 10,
cover the boxes more densely, at a cost that grows with the number of starts.
:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` 500 in place of
:hpo:key:`BUDGET_PER_START` gives each of the ten starts the same share of 50
(:ref:`nfp_optimizer_block_budgets`).

:hpo:opt:`DLIB:GFS` under :hpo:key:`PLAIN <STRATEGY.TYPE.PLAIN>`, with
:hpo:key:`!MAXEVAL <OPTIMIZER.MAXEVAL>` 300 in place of
:hpo:key:`!N_STARTS`, :hpo:key:`!BUDGET_PER_START` and
:hpo:key:`STEP <OPTIMIZER.STEP>`, takes all six points as the first
evaluations of one global search (:ref:`nfp_example_gfs_descriptor`).
:hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>` refuses it.

A finer embedding basis (:ref:`CEMBFUNC`) moves the minima along
:ref:`RCUTMB` and changes their depths; that is a change of the model in the
training configuration, not of the search.
