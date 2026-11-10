.. _nfp_asa:

Adaptive Simulated Annealing
============================

This guide shows when Adaptive Simulated Annealing (ASA,
:hpo:opt:`ASA:ASA`) suits a search, and how its schedule, its batches of
evaluations and its stop are chosen. It builds on :ref:`nfp_choosing`;
:ref:`nfp_optimizer_block` describes the block that holds these settings.
The :ref:`nfp_example_asa` example runs two ASA searches with the
training configuration of :ref:`nfp_unit_2`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_asa_when:

When ASA suits a search
-----------------------

ASA is a gradient-free global optimiser. It draws each new point around the last
accepted one, within a width set by a temperature of each search parameter. It
accepts a worse point with a probability set by a temperature of the loss. Both
temperatures fall as the search goes on, and at periodic tests ASA rescales the
temperature of each search parameter to the sensitivity of the loss along it.

ASA suits a loss with several minima or jumps, starting points that
are guesses, and search parameters that change the loss by very different
amounts. Each search parameter gets its own step width, and there is no
:hpo:key:`STEP` to choose. Like every global search, it needs a larger budget
than a local one (:ref:`nfp_choosing_budget`).

The alternatives:

- a smooth loss and a trusted start: a local search
  (:ref:`nfp_choosing_local`), with a fraction of the budget;
- many workers, or a run on several MPI ranks: :hpo:opt:`TADAH:ANNEAL_PT`
  (:ref:`nfp_annealing`). ASA evaluates points concurrently only in batches
  (:ref:`nfp_asa_batches`), and ``tadah hpo`` refuses it on several MPI ranks
  (:hpo:topic:`RULE.multi_rank_optimiser`).

For a loss whose evaluations often fail, ASA replaces a failed point that it
drew by another one, but a failure at one of its probes ends the search;
:ref:`nfp_asa_probes` gives the setting that avoids it.

ASA runs once per search: under :hpo:key:`PLAIN`, or under :hpo:key:`PRESTAGE`
as the exploration or as the refinement of the best candidate (:hpo:key:`TOPK`
1); the :hpo:key:`MULTISTART` entry says why it is refused there.

.. _nfp_asa_minimal:

A minimal block
---------------

The block below, from the :ref:`nfp_example_asa` example, sets the
algorithm, a budget, a seed and a :hpo:param:`Generated_Frequency_Modulus`
that brings the periodic tests within the budget (:ref:`nfp_asa_tuning`).

.. literalinclude:: ../files/examples/asa/config.hpo
   :language: none
   :lines: 23-27,33,35

:hpo:key:`MAXEVAL` is the recommended stop (:ref:`nfp_asa_stopping`).
:hpo:key:`SEED <OPTIMIZER.SEED>` makes the random numbers of a run repeat
(:ref:`nfp_optimizer_block_threads`).

ASA starts at the first starting point of the :hpo:block:`INIT` block, and
without one at the values of the training configuration
(:ref:`nfp_training_hpo_start`). Before the start it evaluates a sample of
points (:hpo:param:`Number_Cost_Samples`), drawn at the initial temperatures, so
they can lie anywhere in the box. Their losses set the initial temperature of
the loss. The first rows of ``loss.tadah`` are these samples, and the start
follows them.

A start whose loss ASA does not take (a loss at or above :hpo:key:`FAILSCORE` or
about 1e18, or one that is not finite) stays ASA's first state, its loss capped
at about 1e18. The first state ASA generates after it with a usable loss becomes
ASA's best state; ``hpo_run.log`` names the failed start.

.. _nfp_asa_tuning:

Choosing the schedule
---------------------

The default schedule is the recommended choice for a first run. Without
reannealing, the temperature of each search parameter falls with the number
of generated points: :hpo:param:`Temperature_Ratio_Scale` is the ratio of
that temperature to its initial value after
:hpo:param:`Temperature_Anneal_Scale` generated points. A larger ratio
cools more slowly. Past that point, the temperatures fall more slowly the
more search parameters there are (formula in the
:hpo:param:`!Temperature_Ratio_Scale` entry).

The chance of a long step falls only logarithmically with the temperature:
far below its initial value, a temperature still gives occasional long jumps.

When a search stagnates, or ends far from a point known to be better, the
ASA documentation recommends changing one schedule at a time, in order:

#. The schedule of the search parameters, through
   :hpo:param:`!Temperature_Ratio_Scale`, for the middle of the search. With
   many search parameters, the ASA documentation advises a value larger than
   the default, which cools more slowly. :hpo:param:`User_Temperature_Ratio`
   multiplies its logarithm for one search parameter: above 1 that parameter
   cools faster, below 1 more slowly. The documentation also advises, at
   first, a :hpo:param:`!Temperature_Anneal_Scale` above the number of search
   parameters; this setting has less effect than
   :hpo:param:`!Temperature_Ratio_Scale`.
#. The schedule of the loss, through :hpo:param:`Cost_Parameter_Scale_Ratio`.
   ASA accepts a worse point with the probability exp(-r/T), with r the rise of
   the loss and T this temperature; T works best on the scale of the difference
   between the best loss and the loss of the last accepted point.
#. Quenching, for the end of the search. :hpo:param:`User_Quench_Param_Scale`
   above 1 cools the search parameters faster once
   :hpo:param:`!Temperature_Anneal_Scale` points have been generated, and
   gives up ASA's statistical guarantee of finding the global minimum: it
   suits a search known to be near the best minimum.
   :hpo:param:`User_Quench_Cost_Scale` quenches the temperature of the loss
   and keeps that guarantee.

ASA adapts its temperatures at periodic tests: after every
:hpo:param:`Acceptance_Frequency_Modulus` accepted points, every
:hpo:param:`Generated_Frequency_Modulus` generated points, and whenever the
recent share of accepted points falls below
:hpo:param:`Accepted_To_Generated_Ratio`. Each test multiplies the temperature
of each search parameter by the ratio of the largest slope of the loss to the
slope along that parameter. The most sensitive search parameter keeps its
temperature; the others are heated, at most to their initial temperature, so
the search keeps exploring the less sensitive directions. A search parameter
at 0 (its lower bound under :hpo:key:`OPTIM_SCALE` ``AUTO``) keeps its
temperature: the probe step is relative, so its probe is the best point
itself and measures no slope. :hpo:param:`Reanneal_Cost` (on by default) also
lowers the initial temperature of the loss to the size of the current losses
when they are smaller, and keeps the current one at or below it.

At the default :hpo:param:`!Accepted_To_Generated_Ratio`, a search that
accepts fewer points than :hpo:param:`!Acceptance_Frequency_Modulus` and
generates fewer than :hpo:param:`!Generated_Frequency_Modulus` makes no
periodic test, and never adapts its temperatures. A smaller
:hpo:param:`!Generated_Frequency_Modulus` brings the tests within the budget;
the :ref:`nfp_example_asa` example sets one for that reason. Each test has a
cost in evaluations (:ref:`nfp_asa_probes`).

.. _nfp_asa_probes:

Probes and the box
------------------

At a periodic test, ASA measures the slope of the loss along each search
parameter at the best point. It evaluates the best point again, then one probe
per search parameter, a small relative step away (:hpo:param:`Delta_X`). So,
at the default settings, a test costs one evaluation more than the number of
search parameters. These evaluations count against :hpo:key:`MAXEVAL`, and
their rows in ``loss.tadah`` lie between those of the generated points.

When ASA ends by its limits, its temperatures or its plateau test
(:ref:`nfp_asa_stopping`), it measures the slopes once more. The recommended
:hpo:param:`Curvature_0` is the default, which skips curvatures. 0 adds them
at the end, and -1 also at every periodic test, each time at a cost that grows
with the square of the number of search parameters.

Every probe lies inside the :hpo:key:`OPTIM` box: next to a bound, a probe
steps the other way. ``tadah hpo`` checks this when it starts, and refuses a
step whose probes would leave the box. The :hpo:param:`!Delta_X` entry gives
the steps that fit; the default fits every box under :hpo:key:`OPTIM_SCALE`
``AUTO``. The refusal of the line below names each search parameter concerned,
its box and the largest step that fits it:

.. hpo:refusal::

   PARAM Delta_X 1.5

Under the default :hpo:key:`FAILSCORE_GRADING` ``OFF``, ASA treats a failed
evaluation (:ref:`nfp_targets_failures`) of a sample or a generated point as
an invalid point and draws another one
(:hpo:param:`Limit_Invalid_Generated_States`). At a probe it has no
alternative: a failure ends the search, which keeps its best point. The stop
line of the journal names this cause and the settings that avoid it. For a
loss whose evaluations can fail, such as one with a LAMMPS script that does
not always converge, :hpo:param:`Reanneal_Parameters` 0 removes the probes of
the periodic tests. The search then gives up the adaptation of its step
widths, and a search that ends at :hpo:key:`!MAXEVAL`,
:hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>` or :hpo:key:`STOPVAL` makes no probe at
all. Under ``ON``, with a :hpo:key:`FAILSCORE` that ASA accepts (the
:hpo:key:`!FAILSCORE_GRADING` entry), a failure has a graded loss, which ASA
takes as any other.

.. _nfp_asa_batches:

Evaluating batches
------------------

By default ASA generates and evaluates one point at a time.
:hpo:key:`THREADS` alone then gives it no concurrent evaluations: the workers
serve only the work inside each evaluation
(:ref:`nfp_optimizer_block_threads`), as a ``HINT`` line of the journal says.

:hpo:param:`Gener_Block` above 1 makes ASA generate a batch of points from
the same current point. It evaluates them on up to :hpo:key:`!THREADS`
workers at once and tests them in order of their losses. The recommended
setting is :hpo:param:`!Gener_Block` equal to :hpo:key:`!THREADS`, with
:hpo:param:`Gener_Mov_Avr` 0, as in the example below. ASA otherwise resizes
the batch at its periodic tests and can shrink it to one point, which leaves
workers idle.

.. literalinclude:: ../files/examples/asa/threads.hpo
   :language: none
   :lines: 15-19,26,31-33

A batch runs on at most :hpo:param:`Gener_Block_Max` workers, which is
:hpo:param:`!Gener_Block` by default. A batch search follows another sequence
of points than a serial search with the same :hpo:key:`SEED <OPTIMIZER.SEED>`,
so its result differs; the :ref:`nfp_example_asa` example runs both. For a
given batch size, the result does not depend on :hpo:key:`!THREADS`, and
``tadah hpo`` numbers and records the evaluations of a batch in the order of
the batch (:ref:`nfp_output_numbers`), so the records match too.

.. _nfp_asa_stopping:

Choosing a stop
---------------

:hpo:key:`MAXEVAL` is the recommended stop. It counts every evaluation: the
samples of the initial temperature of the loss, the start, the generated
points and the probes. The search ends exactly there, also in the middle of a
batch. :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>` and :hpo:key:`STOPVAL` end the
search too; in a batch search they act after the batch, whose evaluations all
complete. The stagnation stop of a :hpo:block:`RESTART` block ends an ASA
search as any other (:ref:`nfp_optimizer_block_restart`).

ASA also ends by tests of its own, each with a reference entry:

- the limits :hpo:param:`Limit_Acceptances` and :hpo:param:`Limit_Generated`;
- the temperature of a search parameter, or of the loss, below the
  smallest temperature ASA distinguishes from zero;
- the plateau test: the last and the best losses within
  :hpo:param:`Cost_Precision` of each other at
  :hpo:param:`Maximum_Cost_Repeat` periodic tests in a row;
- :hpo:param:`Limit_Invalid_Generated_States` failed evaluations in a row.

Without :hpo:key:`!MAXEVAL` or another stop, only these tests end ASA. With
several search parameters the temperatures fall slowly (:ref:`nfp_asa_tuning`):
these stops come after far more evaluations than a search usually has.

The journal line ``ASA stopped:`` names the stop and the number of
evaluations, ``ASA summary:`` the counts of generated and accepted points
(both quoted in :ref:`nfp_example_asa`); ASA writes a file of its own too
(:ref:`nfp_output`).
