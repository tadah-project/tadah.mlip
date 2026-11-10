.. _nfp_example_anneal_pt_full:

A Full Ta Potential with Parallel Tempering
===========================================

This example scores a Ta potential on validation errors and six crystal
properties, and searches the fit's error scales with the three descriptor
parameters. :hpo:opt:`TADAH:ANNEAL_PT` searches the five search parameters on
four replicas. It builds on :ref:`nfp_unit_3`, :ref:`nfp_unit_4` and
:ref:`nfp_unit_6`, and needs a :ref:`Nested Fitting build <installation_nfp>`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When it applies
---------------

A potential for production use has to meet several kinds of target at once:
validation errors on the structures it is meant for, and properties that no
dataset holds. The loss here joins the validation datasets of
:ref:`nfp_unit_3` and the LAMMPS properties of :ref:`nfp_unit_4`. The search
changes the energy and stress :term:`error scales <error scale>` of the fit
inside each :term:`evaluation` (:ref:`nfp_training_hpo_candidate`), so that
energies and stresses get the share of the fit that suits this loss.

:hpo:opt:`TADAH:ANNEAL_PT` suits a loss with several basins and spare cores
(:ref:`nfp_annealing_choice`). It follows :ref:`nfp_annealing_recipe` with
``FIXED`` moves: a short run measures the hottest temperature for them.

Files
-----

The example reuses :download:`Unit 3's training configuration
<../files/tutorial/unit3/config.train>` and :download:`validation configuration
<../files/tutorial/unit3/config.val>` (:ref:`nfp_unit_3`). The index lists of
the target file count the three validation datasets in their order there. The
scripts are :download:`ecoh_a0.in <../files/lammps/ecoh_a0.in>`,
:download:`vacancy.in <../files/lammps/vacancy.in>` and :download:`elastic.in
<../files/lammps/elastic.in>` of :ref:`nfp_unit_4`, which describes them.

:download:`config.hpo <../files/examples/anneal_pt_full/config.hpo>` is the
target file of the search:

.. literalinclude:: ../files/examples/anneal_pt_full/config.hpo
   :language: none

:download:`calibrate.hpo <../files/examples/anneal_pt_full/calibrate.hpo>`
measures the hottest temperature. Above its :hpo:block:`OPTIMIZER` block it has
the lines of ``config.hpo`` without their comments: the calibration measures the
loss of the search. Its keys after :hpo:key:`ALGO <OPTIMIZER.ALGO>` are:

.. literalinclude:: ../files/examples/anneal_pt_full/calibrate.hpo
   :language: none
   :lines: 28-31,33-34,36-37
   :dedent: 4

Running the searches
--------------------

The commands run in ``examples/anneal_pt_full`` (:ref:`layout of the files
<nfp_tutorial_files>`). The calibration runs first:

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit3/config.train \
       --validation ../../tutorial/unit3/config.val --hpotarget calibrate.hpo

The calibration takes a few seconds on four processor cores.

Its journal, ``hpo_run.log``, gives the temperature T0 that the probes measure:

.. output: anneal_pt_full_short hpo_run.log tol=calibrated_t_hot

.. code-block:: text

   INFO      ANNEAL_PT: T0=1.300e+00 from 20 probes: median rise of the loss
       3.896e+00 over the 9 probes that raised it, T0 = median rise /
       ln(1/T0_ACCEPT) with T0_ACCEPT=5.000e-02  loss range=[1.616e+01, 1.109e+02]
       std=2.128e+01  anchor=seed

:hpo:param:`T_HOT` in ``config.hpo`` is that T0. The search replaces the
journal and the history files of the calibration (:ref:`nfp_output_files`),
so they are read first. The search evaluates the four replicas at once on
four workers (:hpo:key:`THREADS`):

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit3/config.train \
       --validation ../../tutorial/unit3/config.val --hpotarget config.hpo

The search takes about five minutes on four processor cores.

Expected result
---------------

:download:`expected.csv <../files/examples/anneal_pt_full/expected.csv>`:

.. csv-table::
   :file: ../files/examples/anneal_pt_full/expected.csv
   :header-rows: 1
   :widths: 60 40

The best loss of the calibration belongs to one of its probes. The search starts
again from the values of the training configuration, both error scales on their
upper bounds, and lowers the loss from 19.43 to 5.944. The journal warns when a
replica sits near an :hpo:key:`OPTIM` bound (:ref:`nfp_annealing_steps`):
:ref:`ESIGMA` early, later :ref:`RCUTMB`, each in one of the four slots:

.. output: anneal_pt_full hpo_run.log search=config.hpo tol=best_rcutmb
.. code-block:: text

   WARNING       dim                       side  bound        gap        tol
       slots   observed for
   WARNING       ESIGMA (ln)               HI    2            4.2e-02    5.0e-02
       1/4     5 epochs
   ...
   WARNING       RCUTMB                    HI    7            5.9e-04    5.0e-02
       1/4     15 epochs

The best values sit away from every bound, as the ``DIAGNOSTICS`` section
of the journal reports (:ref:`nfp_search_space_bounds`).

Some candidates relax to a lattice constant outside the range of
:hpo:key:`--hotvar <PC_LAMMPS.--hotvar>`. They fail
(:ref:`nfp_targets_failures`), and the replica that proposed each stays where
it was. The second rejection:

.. output: anneal_pt_full hpo_run.log search=config.hpo tol=best_a0

.. code-block:: text

   WARNING   eval 202 script ../../lammps/ecoh_a0.in: rejecting
       (a0=3.5756347686298473 outside [3.1, 3.5], total rejects=2)

Ecoh and the three elastic constants of the best potential lie within the
accuracies of their terms, 0.1 eV/atom and 10 GPa, and a0 within about its
accuracy, 0.01 Å. The vacancy formation energy Evf does not: at 2.14 eV it
lies 0.85 eV below the DFT value, well outside its accuracy of about 0.3 eV; its term is the largest of
the property terms (:ref:`nfp_targets_losses`).

Both error scales end well below their starting values of 2 meV/atom and
0.5 GPa: :ref:`ESIGMA` at 0.189 meV/atom and :ref:`SSIGMA` at 0.154 GPa. The
best fit weighs energies and stresses more heavily against the forces than
that of Unit 3. In ``outvar.tadah`` (:ref:`nfp_output_histories`), from the
first evaluation to the best, the validation energy error falls, and the force
and stress errors rise (table above): they pay for the properties.

Over the whole search, every pair of neighbouring slots exchanges often,
walkers cross the ladder, and every slot has held all four walkers
(:ref:`nfp_annealing_replicas`): the replicas work as one search.

.. output: anneal_pt_full hpo_run.log search=config.hpo

.. code-block:: text

   INFO      ANNEAL_PT swap acceptance per pair (cold->hot):  1:54%  2:49%  3:43%
   ...
   INFO      ANNEAL_PT ladder flow: traversals cold->hot=22  hot->cold=22  (~22
       round trip(s) completed)
   INFO      * ANNEAL_PT distinct walkers per slot over the whole run (4 = fully
       mixed):
   INFO        [1-4]          4          4          4          4

How the values were chosen
--------------------------

The loss terms
~~~~~~~~~~~~~~

The validation terms score the datasets of :ref:`nfp_unit_3` one by one, the
stresses included, each with the weight :math:`1/a^2` of its accuracy
:math:`a` (:ref:`nfp_targets_losses`):

.. literalinclude:: ../files/examples/anneal_pt_full/config.hpo
   :language: none
   :lines: 25-34

An indexed term is the mean over the three datasets
(:ref:`nfp_targets_datasets`). The forces of the compressed crystals of dataset
(3) are zero by symmetry: their :hpo:key:`PC_FRMSE` line has the loss-term
weight 0, so the force term counts for less than the energy term. Their energy
accuracy is coarser than in Unit 3: the elastic constants constrain the crystal
near equilibrium, and dataset (3) guards the behaviour under strong compression.

The property terms are the three :hpo:key:`PC_LAMMPS` lines of
:ref:`nfp_unit_4`, with its targets, weights and lattice constants. The loss
is the mean of nine terms: the three metrics and the six :hpo:key:`--varloss`
values (:ref:`nfp_targets_mean`).

The searched error scales
~~~~~~~~~~~~~~~~~~~~~~~~~

The training configuration fits with the error scales of :ref:`nfp_unit_2`.
The properties are energy differences (Ecoh, Evf) and stress responses (the
elastic constants), so energies and stresses may need a larger share:

.. literalinclude:: ../files/examples/anneal_pt_full/config.hpo
   :language: none
   :lines: 13-19

The ratios of the error scales set the balance of the fit
(:ref:`fit_objective`), so :ref:`FSIGMA` stays fixed and the two others move
against it. Each box reaches down from the value of the training configuration
by a factor of :math:`\sqrt{1000}`, which multiplies the contribution of the
energies (or stresses) to the fit by up to 1000. The loss-term weights stay
fixed (:ref:`nfp_targets_losses`).

The ladder
~~~~~~~~~~

Both files use the same moves: :hpo:param:`STEP_MODE` ``FIXED`` with a
:hpo:param:`STEP_FRAC` of 0.05. A fixed :hpo:param:`T_HOT` belongs to the
moves it was measured with (:ref:`nfp_annealing_steps`), and ``FIXED`` keeps
those moves, without a warm-up. The :hpo:block:`!OPTIMIZER` keys of
``config.hpo`` after :hpo:key:`ALGO <OPTIMIZER.ALGO>`, without comments:

.. literalinclude:: ../files/examples/anneal_pt_full/config.hpo
   :language: none
   :lines: 52,54-56,59-61,65
   :dedent: 4

:hpo:param:`REPLICAS` 4 is fixed, so that another :hpo:key:`THREADS` changes
the speed of the search, not its ladder (:ref:`nfp_annealing_replicas`).
:hpo:param:`T_COLD` 0.2 resolves loss differences of about 3 % of the best
loss (:ref:`nfp_annealing_temperatures`) and keeps every pair exchanging.
:hpo:key:`MAXEVAL` 1200 gives each of the five search parameters 240
evaluations (:ref:`nfp_choosing_budget`). :hpo:key:`SEED <OPTIMIZER.SEED>`
fixes the starts of the other replicas, the moves and the exchanges, so the
run repeats (:ref:`nfp_optimizer_block_threads`).

Variations
----------

:hpo:key:`THREADS` 1 or 2 in ``config.hpo`` gives the same evaluations, the
same history files and the same best potential, apart from the times they
record. The search takes longer: the replicas then share fewer workers.

A lower :hpo:param:`T_COLD`, such as 0.02, resolves smaller loss differences at
the cold end; its coldest pair of slots then exchanges less often, which the
journal's swap acceptance and ladder flow show (:ref:`nfp_annealing_replicas`).

Without the two :hpo:key:`OPTIM` lines of :ref:`ESIGMA` and :ref:`SSIGMA` in
both files, the fit keeps the balance of Unit 3, and the search moves the three
descriptor parameters only. Each move then changes three search parameters,
not five, so ``calibrate.hpo`` measures :hpo:param:`T_HOT` again first
(:ref:`nfp_annealing_steps`). The best loss of the search, set against 5.944,
measures what the searched error scales add for the properties.
