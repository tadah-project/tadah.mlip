.. _nfp_example_asa:

Global Search with ASA
======================

This example searches four :term:`search parameters <search parameter>` of
the potential of :ref:`nfp_unit_2` with Adaptive Simulated Annealing (ASA,
:hpo:opt:`ASA:ASA`), serially and in batches of four. It builds on
:ref:`nfp_unit_2`, whose training and validation configurations it reuses;
:ref:`nfp_asa` covers the settings of ASA.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When it applies
---------------

A global search suits a loss with several minima or jumps, and starting points
that are guesses (:ref:`nfp_asa_when` compares it with the alternatives).
The example adds the centre of the many-body density, :ref:`CGRIDMB`, to the
three search parameters of :ref:`nfp_unit_2`. The
:term:`training configuration` puts that centre at 0, its lower bound: a
guess, not a trusted start.

A global search needs a larger budget than a local one, and more with each
search parameter (:ref:`nfp_choosing_budget`). The example gives ASA 100
:term:`evaluations <evaluation>` per search parameter, four times the budget
per search parameter of the local search of :ref:`nfp_unit_2`.

Files
-----

The example reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`validation configuration <../files/tutorial/unit2/config.val>`,
with their :ref:`datasets <nfp_unit_2>`. The two target files below have the
same search space, loss and budget; ``threads.hpo`` evaluates the points in
:term:`batches <Batch>` of four.

:download:`config.hpo <../files/examples/asa/config.hpo>`:

.. literalinclude:: ../files/examples/asa/config.hpo
   :language: none

:download:`threads.hpo <../files/examples/asa/threads.hpo>`:

.. literalinclude:: ../files/examples/asa/threads.hpo
   :language: none

Running the searches
--------------------

The commands run in ``examples/asa`` (:ref:`layout of the files
<nfp_tutorial_files>`). The first runs the serial search of ``config.hpo``:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The search takes about four minutes on one processor core.

After the starting points (:ref:`nfp_target_file_check`), the terminal
names the algorithm and the number of search parameters (``dim=4``), then
the settings of ASA:

.. output: asa stdout search=config.hpo

.. code-block:: text

   [tadah hpo] ASA:ASA   dim=4
   [tadah hpo] ASA[Ingber ASA-30.53]: seed=1 dim=4  Limit_Acceptances=10000
       Limit_Generated=99999  Temperature_Ratio_Scale=1e-05
       Temperature_Anneal_Scale=100  Cost_Parameter_Scale_Ratio=1
       Reanneal_Parameters=1  Reanneal_Cost=1  Curvature_0=1
       User_Initial_Parameters=1

The journal has the fit warnings that :ref:`nfp_unit_2` describes, and the
terminal the warnings of single fits that :ref:`nfp_unit_5` describes. The run
ends with the :term:`balance table` of the best potential (:ref:`fit_balance`).

The second command runs the batch search of ``threads.hpo`` (`Four
evaluations at a time`_). It rewrites the output files of the first search,
``best_pot.tadah`` included (:ref:`nfp_output_files`), so the results of
``config.hpo`` are read, or the directory copied, before it runs:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget threads.hpo

The batch search takes about two minutes on four processor cores.

Expected result
---------------

:download:`expected.csv <../files/examples/asa/expected.csv>`:

.. csv-table::
   :file: ../files/examples/asa/expected.csv
   :header-rows: 1
   :widths: 60 40

The serial search ends at a loss of 13.75, with :ref:`RCUT2B` 6.27 Å
and :ref:`RCUTMB` 4.68 Å. The batch search ends at 14.34, with
6.30 Å and 4.65 Å. Both density cutoffs lie between the second and
third neighbour shells of the crystal at a0 (3.32 and 4.70 Å, from the
comment of ``config.hpo``), close to the third. At that lattice constant
the density counts the first two shells.

The two searches end at different centres and widths of the density:
:ref:`CGRIDMB` 1.7 Å and :ref:`SGRIDMB` 0.99 Å⁻² for the serial
search, 0.34 Å and 0.145 Å⁻² for the batch search. With the same
budget, the result of a global search depends on the sequence of points it
tries.

The best :ref:`SGRIDMB` of the serial search, 0.99 Å⁻², lies next to the
upper bound of its box, 1.0 Å⁻². The journal of the serial search counts
its 60 accepted points and lists that width under the bound occupancy of
the final best: the minimum may lie outside the box
(:ref:`nfp_search_space_bounds`).

.. output: asa hpo_run.log search=config.hpo tol=abs:0.02

.. code-block:: text

   INFO      ASA summary: generated=374 accepted=60 host_evals=400
       best=1.374872e+01 exit=IMMEDIATE_EXIT  (at-best: generated=371 accepted=60)
   ...
   Bound occupancy of the final best (eval 397) sits within 2% of an OPTIM limit on
       1 dim(s) (gap = fraction of width):
   dim                     side  value         bound         gap/width  since eval
   SGRIDMB                 HI    9.9181e-01    1.0000e+00    2.75e-03   84

The first rows of ``loss.tadah`` are ASA's samples of the initial
temperature of the loss (:ref:`nfp_asa_minimal`). Both searches end at
:hpo:key:`MAXEVAL`, the batch search with these lines of its journal:

.. output: asa,asa_short hpo_run.log search=threads.hpo tol=best_loss

.. code-block:: text

   INFO      ASA summary: generated=374 accepted=56 host_evals=400
       best=1.433760e+01 exit=IMMEDIATE_EXIT  (at-best: generated=361 accepted=54)
   INFO      ASA stopped: MAXEVAL reached after 400 evaluation(s) (asa exit:
       IMMEDIATE_EXIT)
   ...
   Optimiser status        : ASA: MAXEVAL reached

``host_evals`` counts every evaluation, the samples, the start and the
probes of the periodic tests included (:ref:`nfp_asa_stopping`);
``generated`` counts only the points ASA generates. ``at-best`` gives both
counts when the best was found, and ``IMMEDIATE_EXIT`` is ASA's exit code
when the run stops it. :ref:`nfp_output` describes the output files.

How the values were chosen
--------------------------

The search space is that of :ref:`nfp_unit_2`, with its bounds and
reasons, plus :ref:`CGRIDMB`, the centre of the Gaussian from which the
density is built. A centre below the shortest distance in the training data
keeps the Gaussian falling at every neighbour distance of the training data,
as the density of an embedded-atom potential does:

.. literalinclude:: ../files/examples/asa/config.hpo
   :language: none
   :start-at: # The radial factor
   :end-at: OPTIM      CGRIDMB

The loss is that of :ref:`nfp_unit_2`, with the same accuracies.
The block of the optimiser sets the budget, the seed and the cadence of
ASA's periodic tests:

.. literalinclude:: ../files/examples/asa/config.hpo
   :language: none
   :start-at: MAXEVAL
   :end-at: Generated_Frequency_Modulus

- ASA draws random numbers: with :hpo:key:`SEED <OPTIMIZER.SEED>` the run
  repeats exactly.
- At each periodic test ASA rescales the temperature of each search
  parameter to the sensitivity of the loss (:ref:`nfp_asa_tuning`). The
  default cadences, :hpo:param:`Acceptance_Frequency_Modulus` and
  :hpo:param:`Generated_Frequency_Modulus`, come later than this budget
  reaches. A test every 80 generated points brings the tests within the
  budget, at a cost of evaluations (:ref:`nfp_asa_probes`). The value sets
  when ASA adapts; the example claims no better result from it.

The schedule of the temperatures keeps its defaults, the recommended choice
for a first run; :ref:`nfp_asa_tuning` gives the order in which to change it
when a search stagnates. ASA takes no :hpo:key:`STEP` (``tadah hpo`` refuses
one): its temperatures set the width of its steps.

Four evaluations at a time
--------------------------

Three settings of the block of ``threads.hpo`` differ from ``config.hpo``:

.. literalinclude:: ../files/examples/asa/threads.hpo
   :language: none
   :lines: 26,31-32

:hpo:param:`Gener_Block` makes ASA generate four points together, and
:hpo:key:`THREADS` evaluates them on four workers, one per point.
:hpo:key:`!THREADS` alone gives ASA
no concurrent evaluations, and :hpo:param:`Gener_Mov_Avr` 0 keeps the
batches at four (:ref:`nfp_asa_batches`). With batches of four, each
multiple of 80 generated points falls at the end of a batch. The periodic
tests then come at the same counts as in ``config.hpo``.

The batch search takes less wall time when four cores are free. It follows
another sequence of points than the serial search, and so ends elsewhere
(the rows of ``threads.hpo`` in the table above). For a given batch size,
its result does not depend on :hpo:key:`!THREADS`
(:ref:`nfp_asa_batches`): the same file with ``THREADS 1`` evaluates the same
points, one at a time.

Variations
----------

Another :hpo:key:`SEED <OPTIMIZER.SEED>` makes ASA try another sequence of
points, which can end at another point with another loss. The result of a
global search with a budget is one sample; two or three seeds show its
spread.

A further search widens the upper bound of :ref:`SGRIDMB`, where the best of
the serial search lies. With the centre of the density away from zero, the
same density width gives a slower fall between neighbour shells, as the
comment of ``config.hpo`` shows. That search can start from the best
potential of the serial search, in a copy of the directory made before the
second command (:ref:`nfp_example_warm_start`).

Without :hpo:param:`Generated_Frequency_Modulus`, the search ends before
its first periodic test, and ASA never adapts its temperatures to the
loss.

A local search from the same start, such as :hpo:opt:`NLOPT:LN_BOBYQA` of
:ref:`nfp_unit_2` with the four search parameters of this example, gives the
baseline that shows what the global search adds.
