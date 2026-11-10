.. _nfp_example_gfs_descriptor:

A Global Search Without Tuning
==============================

This example searches the three descriptor parameters of
:ref:`Unit 5 <nfp_unit_5>` with :hpo:opt:`DLIB:GFS`, a global optimiser that
needs only the bounds, a budget and its starting points. It runs the search
from two sets of starting points and tests the :term:`best potential` on
data that no search read. It builds on Unit 5 and uses the configurations of
:ref:`Unit 2 <nfp_unit_2>` and the :ref:`Ta data <nfp_ta_data>`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When it applies
---------------

A global search suits a search in which no starting point is trusted
(:ref:`nfp_choosing_global`). :hpo:opt:`DLIB:GFS` then needs no setting
besides the bounds, :hpo:key:`MAXEVAL` and its starting points. The
:hpo:key:`PRESTAGE` search of :ref:`Unit 5 <nfp_unit_5>` has more settings
to choose: the split of the budget between its exploration and its local
searches, the number of local searches and their step.

With :ref:`SGRIDMB` fixed, the same model on the same data has several
minima along :ref:`RCUTMB` (:ref:`nfp_example_multistart_minima`);
:ref:`nfp_choosing_staged` gives when a :hpo:key:`!PRESTAGE` search is the
recommended choice for such a loss. This example shows what a search
without tuning gives on the same loss, and how its result depends on its
starting points.

Files
-----

The searches reuse :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`validation configuration <../files/tutorial/unit2/config.val>`,
with their :ref:`datasets <nfp_unit_2>`. The predictions read
:download:`ta_md_test.tadahdb <../files/data/ta_md_test.tadahdb>` and
:download:`ta_strain.tadahdb <../files/data/ta_strain.tadahdb>` from the
:ref:`Ta data <nfp_ta_data>`.

:download:`config.hpo <../files/examples/gfs_descriptor/config.hpo>` has
the search parameters, bounds and loss terms of Unit 5:

.. literalinclude:: ../files/examples/gfs_descriptor/config.hpo
   :language: none

:download:`seed2.hpo <../files/examples/gfs_descriptor/seed2.hpo>` is the
same search from other starting points:

.. literalinclude:: ../files/examples/gfs_descriptor/seed2.hpo
   :language: none

Running the searches and predictions
------------------------------------

The commands run in ``examples/gfs_descriptor`` (:ref:`layout of the files
<nfp_tutorial_files>`), the search of ``seed2.hpo`` first:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget seed2.hpo

The journal of each search, ``hpo_run.log``, records the number of starting
points, the end at :hpo:key:`MAXEVAL` and the number of evaluations. For
``seed2.hpo``:

.. output: gfs_descriptor,gfs_descriptor_short hpo_run.log search=seed2.hpo

.. code-block:: text

   INFO      DLIB:GFS seeding with 8 starting points
   ...
   INFO      DLIB:GFS stopped: MAXEVAL reached after 177 evaluation(s)
   ...
     Total evaluations       : 177

The second run writes its files over those of the first
(:ref:`nfp_output_files`), so the best loss of ``seed2.hpo`` is read from the
``RESULT`` section of ``hpo_run.log`` first, and its best parameters from the
last row of ``best_params.tadah``. The search of ``config.hpo`` follows:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The two searches take about five minutes in all on one processor core.

The journal of the run has the fit warnings that :ref:`nfp_unit_2` describes,
and on the terminal a few evaluations warn that all coefficients of the
many-body descriptor are zero (:ref:`nfp_unit_5`).

The directory ends with the files of ``config.hpo`` and its best potential.

The best potential then predicts two datasets that no search read: solid
molecular dynamics at other pressures, and crystals compressed along one axis
(:ref:`nfp_ta_data`). The DFT forces of the compressed crystals are zero by
symmetry, so the second command predicts energies and stresses only:

.. code-block:: bash

   tadah predict -p best_pot.tadah -A -F -S -d ../../data/ta_md_test.tadahdb \
       -o test_energy.pred test_forces.pred test_stress.pred
   tadah predict -p best_pot.tadah -A -S -d ../../data/ta_strain.tadahdb \
       -o strain_energy.pred strain_stress.pred

The ``-o`` names keep the predictions of the two datasets apart
(:ref:`cli_predict`).

The search without tuning
-------------------------

Without the comments, the :hpo:block:`OPTIMIZER` block of ``config.hpo`` is:

.. literalinclude:: ../files/examples/gfs_descriptor/config.hpo
   :language: none
   :lines: 16-18,21,24-25,29,32-34

The block has no single starting point and no
:hpo:key:`STEP <OPTIMIZER.STEP>`: :hpo:opt:`DLIB:GFS` sizes its own trust
region and refuses a step. It takes no :hpo:key:`PARAM <OPTIMIZER.PARAM>`
key, and :hpo:key:`FTOL_ABS <OPTIMIZER.FTOL_ABS>`, the precision of its
solver, keeps its default.

The :hpo:block:`INIT` block takes the place of a single starting point.
:hpo:key:`METHOD <INIT.METHOD>` :hpo:key:`SOBOL` draws eight points spread
over the whole box. A chain gives the points of its lines only
(:ref:`nfp_optimizer_block_initialisation`), so the values of the training
configuration are not evaluated. :hpo:opt:`!DLIB:GFS` evaluates the eight
points first, then chooses every later point itself. The eight points are
evaluations of the search, so :hpo:key:`MAXEVAL` counts them.

:hpo:key:`SEED <INIT.SEED>` belongs in the :hpo:block:`!INIT` block, where it
selects the points that :hpo:key:`!SOBOL` draws. :hpo:opt:`!DLIB:GFS` refuses
a :hpo:key:`SEED <OPTIMIZER.SEED>` in the :hpo:block:`!OPTIMIZER` block
(:ref:`nfp_choosing_global`).

Expected result
---------------

:download:`expected.csv <../files/examples/gfs_descriptor/expected.csv>`:

.. csv-table::
   :file: ../files/examples/gfs_descriptor/expected.csv
   :header-rows: 1
   :widths: 60 40

Each search makes 177 evaluations, its whole budget. Both end with the
density cutoff :ref:`RCUTMB` near 3.59 Å, at different losses:
``config.hpo`` lower than ``seed2.hpo``. :hpo:opt:`DLIB:GFS` chooses each
point from the losses of the points before it, so other starting points lead
it along another path and, here, to another end point. A result of
:hpo:opt:`!DLIB:GFS` is therefore checked with a second set of starting
points: a second :hpo:key:`SEED <INIT.SEED>` in the :hpo:block:`INIT` block.

The losses are comparable with those of :ref:`Unit 5 <nfp_unit_5>`, whose
searches have the same search space, loss terms and number of evaluations.
Both searches here end lower than the random search of Unit 5 and higher
than its :hpo:key:`PRESTAGE` search.

``tadah predict -A`` prints the errors of the potential on each dataset, and
the ``.pred`` files hold the predicted values (:ref:`prediction`). The
molecular dynamics test set checks the potential at pressures that no search
read. The compressed crystals reach strains beyond the training data: their
energy error is about 30 times, and their stress error about 18 times, that
on the test set.
:ref:`nfp_output` describes the files of the searches.

How the values were chosen
--------------------------

- The search parameters, their bounds and the loss-term weights are those of
  :ref:`Unit 5 <nfp_unit_5>`, line for line, so that the losses are
  comparable; the comments of Unit 5's target file give their reasons. The
  training and validation configurations are those of
  :ref:`Unit 2 <nfp_unit_2>`, as in Unit 5.
- :hpo:key:`MAXEVAL` 177 is the number of evaluations of each of Unit 5's
  searches, 59 per search parameter. The eight starting points use under
  5 % of it.
- :hpo:key:`METHOD <INIT.METHOD>` :hpo:key:`SOBOL` 8 spreads the starting
  points over the whole box, as no starting point is trusted. Eight is a
  power of two; the :hpo:key:`!SOBOL` entry gives how such a block covers each
  box.
- :hpo:key:`SEED <INIT.SEED>` 1 and 2 are two consecutive values, not chosen
  for their results. The two blocks share no point.
- :hpo:key:`THREADS` 1 makes each search repeat for the same files on the
  same build. With more workers :hpo:opt:`DLIB:GFS` chooses each point from
  the evaluations that have ended by then, which depends on their timing, so
  a run does not repeat (the :hpo:key:`!THREADS` entry).

Variations
----------

A third :hpo:key:`SEED <INIT.SEED>` in the :hpo:block:`INIT` block draws a
third set of starting points, and so checks once more where
:hpo:opt:`DLIB:GFS` ends on this loss.

A :hpo:key:`METHOD <INIT.METHOD>` :hpo:key:`CONFIG` line after the
:hpo:key:`SOBOL` line adds the values of the training configuration to the
eight starting points, which suits values from an earlier search. Without an
:hpo:block:`!INIT` block the search starts from those values alone, and
:hpo:opt:`!DLIB:GFS` chooses every other point.

A larger :hpo:key:`MAXEVAL` gives :hpo:opt:`!DLIB:GFS` more points to
choose; the evaluations of this run come first, in the same order.

:hpo:key:`THREADS` above 1 runs several evaluations at once
(:ref:`nfp_optimizer_block_threads`), at the cost of a run that does not
repeat.
