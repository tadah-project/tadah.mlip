.. _nfp_unit_1:

Unit 1: A First Search
======================

This unit runs a first nested fitting search and explains each value of its
files. ``tadah hpo`` searches one :term:`search parameter`, the pair cutoff of a
Ta potential fitted to energies. The search runs on a grid and keeps the
potential with the smallest validation energy error. It needs a
:ref:`Nested Fitting build <installation_nfp>` and
:ref:`the Ta data <nfp_ta_data>`. The :ref:`nfp_quickstart` runs the same files.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Files
-----

The files lie under ``files/`` of the :ref:`archive <nfp_tutorial_files>`: the
three input files in ``tutorial/unit1``, the datasets in ``data``. The unit
fits the equation-of-state data :download:`ta_eos_train.tadahdb
<../files/data/ta_eos_train.tadahdb>` and validates on
:download:`ta_eos_val.tadahdb <../files/data/ta_eos_val.tadahdb>`.

The :term:`training configuration` (``--config``) sets the data, the
descriptor and the model, as for ``tadah train`` (:ref:`config_file`, keys in
:ref:`ConfigSection`). It has a pair term of blip functions
(:ref:`descriptors`) under a cosine taper (:ref:`cutoffs`) and a linear model:

:download:`config.train <../files/tutorial/unit1/config.train>`:

.. literalinclude:: ../files/tutorial/unit1/config.train
   :language: ini

Its value of :ref:`RCUT2B` is the start of the search
(:ref:`nfp_training_hpo_start`). Every other value is the same in every
:term:`evaluation` (:ref:`nfp_training_hpo_candidate`).

The :term:`validation configuration` (``--validation``) names the validation
data. They are never fitted, but they score every candidate
(:ref:`nfp_training_hpo_validation`):

:download:`config.val <../files/tutorial/unit1/config.val>`:

.. literalinclude:: ../files/tutorial/unit1/config.val
   :language: ini

The :term:`target file` (``--hpotarget``) states what to search, how to score
each evaluation and which optimiser runs the search
(:ref:`nfp_target_file_parts`):

:download:`config.hpo <../files/tutorial/unit1/config.hpo>`:

.. literalinclude:: ../files/tutorial/unit1/config.hpo
   :language: none

The :hpo:key:`OPTIM` line makes the first value of :ref:`RCUT2B`, index
``(1)``, a search parameter with a box from 4.0 to 6.5 Å
(:ref:`nfp_search_space_parameters`). The :hpo:key:`PC_ERMSE` line is the one
:term:`loss term`: the energy RMSE on the validation data, with target 0 and a
:term:`loss-term weight` of 1. The :hpo:block:`OPTIMIZER` block selects
:hpo:opt:`TADAH:GRID`, which evaluates the start, then :hpo:param:`GRID_POINTS`
values spread evenly over the box. :hpo:key:`THREADS` 1 gives the run one
worker, as the grid asks for one point at a time
(:ref:`nfp_optimizer_block_threads`).

Running the search
------------------

The commands run in ``tutorial/unit1``.

The commands take a few seconds on one processor core.

#. ``tadah hpo --check`` reads the three files and makes the checks that start a
   run, without evaluating a candidate (:ref:`nfp_target_file_check`):

   .. code-block:: bash

      tadah hpo --config config.train --validation config.val \
          --hpotarget config.hpo --check

#. The search is started with:

   .. code-block:: bash

      tadah hpo --config config.train --validation config.val \
          --hpotarget config.hpo

   The terminal shows a few progress lines and the :term:`balance table` of the
   best potential: its errors on the training data (:ref:`fit_balance`). The
   run writes its journal ``hpo_run.log``, its history files and the
   :term:`best potential` ``best_pot.tadah`` (:ref:`nfp_output_files`). Part
   of the ``DIAGNOSTICS`` section of the journal:

   .. output: unit1 hpo_run.log

   .. code-block:: text

      -- DIAGNOSTICS ---------------------------------------------------------
      ...
        [ok] Final best sits away from every OPTIM bound (edge 2% of width).

#. The best potential predicts the validation data; ``-A`` prints its errors
   (:ref:`prediction`):

   .. code-block:: bash

      tadah predict -p best_pot.tadah -d ../../data/ta_eos_val.tadahdb -A

Expected result
---------------

The run gives the values of :download:`expected.csv
<../files/tutorial/unit1/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit1/expected.csv
   :header-rows: 1
   :widths: 60 40

Reading the result
------------------

The loss of an evaluation is the mean of its loss terms
(:ref:`nfp_targets_mean`), here of the one :hpo:key:`PC_ERMSE` term. Under the
default loss function :hpo:key:`L2`, with target 0 and weight 1, that term is
the squared validation energy RMSE in meV/atom (:ref:`nfp_targets_losses`). The
square root of the best loss is thus the ``Best ERMSE`` of the journal and the
energy RMSE that ``tadah predict -A`` prints.

``params.tadah`` holds the cutoff of every evaluation, and ``outvar.tadah`` its
validation energy RMSE, row by row (:ref:`nfp_output_histories`). The first row
is the start; the rows after it follow the grid from the lower bound to the
upper one and trace the error over the whole box. ``best_params.tadah`` and
``best_loss.tadah`` hold one row per improvement of the best loss.

The best cutoff, 5.0 Å, lies inside the box (:ref:`nfp_search_space_bounds`),
as the ``[ok]`` line of the journal states. The key lines of ``best_pot.tadah``
hold it, and its header records the search (:ref:`nfp_output_best`).

How the values were chosen
--------------------------

**Energies only.** The equation-of-state data show how the energy of the
crystal changes with its volume, and the cutoff decides which neighbours the
pair term sees. :ref:`FORCE <force>` and :ref:`STRESS <stress>` keep their
default, off, so the fit uses the energies alone. :ref:`Unit 2 <nfp_unit_2>`
adds forces and stresses.

**The box.** Physics sets the bounds (:ref:`nfp_search_space_bounds`). The
lower bound, 4.0 Å, is the outermost blip centre, the last value of
:ref:`CGRID2B`. The upper bound, 6.5 Å, lies beyond the end of every blip (near
5.3 Å, as the pair term below shows), where a longer cutoff changes only the
taper.

**The pair term.** The blip centres run from below the shortest bond of the
data to the lower bound of the cutoff. The blips share one fixed width,
:ref:`SGRID2B` 1.5 per Å: each is 4/1.5 = 2.7 Å wide (:ref:`descriptors`), so
the outermost one, centred at 4.0 Å, ends near 5.3 Å.

**The grid.** One search parameter and a cheap fit of energies alone suit
:hpo:opt:`TADAH:GRID` (:ref:`nfp_choosing_baselines`). It costs the start
plus :hpo:param:`GRID_POINTS` evaluations and shows the whole curve. Over this
box, :hpo:param:`!GRID_POINTS` 11 puts the points 0.25 Å apart.

**The loss term.** :hpo:key:`PC_ERMSE` follows the accuracy rule of
:ref:`nfp_targets_losses`: a weight of :math:`1/a^2` for an accuracy
:math:`a`. An accuracy of 1 meV/atom, the default :ref:`ESIGMA` of the fit,
gives the weight 1, and the loss reads as the squared RMSE in meV/atom. With a
single term, the weight sets the scale of the loss, not which cutoff is best.

**The start.** The value of :ref:`RCUT2B` in ``config.train`` lies inside the
box. A grid visits the same points from any start; the start matters to the
local optimisers of the later units (:ref:`nfp_training_hpo_start`).

What to try next
----------------

A finer grid, such as ``PARAM GRID_POINTS 26`` for points 0.1 Å apart, costs
more evaluations and traces the curve around the best cutoff in more detail.

A local optimiser can replace the sweep: an :hpo:block:`OPTIMIZER` block with
:hpo:opt:`NLOPT:LN_BOBYQA`, the recommended local search for a smooth loss,
and a :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` stop
(:ref:`nfp_optimizer_block_budgets`) moves from the start towards lower losses
and traces no curve (:ref:`nfp_choosing_local`).

A box whose upper bound lies below the best cutoff puts the best point on that
bound, and the ``DIAGNOSTICS`` section of the journal lists :ref:`RCUT2B` as
``HI``. The start in ``config.train`` must then lie inside the smaller box.

Next: :ref:`nfp_unit_2`.
