.. _nfp_unit_3:

Unit 3: Several Validation Datasets
===================================

This unit adds training data and scores every candidate on three validation
datasets, with an energy and a force target and :term:`weight <loss-term
weight>` for each. It rejects candidates with large energy errors on the
vacancy snapshots and searches one :term:`search parameter` in a box relative
to its start. It also tests the :term:`best potential` on data that neither the
fit nor the search saw. The model, its error scales and the search parameters
are those of :ref:`nfp_unit_2`; the unit uses :ref:`the Ta data <nfp_ta_data>`.

The search of this unit takes about two minutes on one processor core.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Files
-----

The datasets are :download:`ta_md_train.tadahdb
<../files/data/ta_md_train.tadahdb>`, :download:`ta_vac_train.tadahdb
<../files/data/ta_vac_train.tadahdb>`, :download:`ta_md_val.tadahdb
<../files/data/ta_md_val.tadahdb>`, :download:`ta_vac_val.tadahdb
<../files/data/ta_vac_val.tadahdb>`, :download:`ta_strain_max20.tadahdb
<../files/data/ta_strain_max20.tadahdb>` and :download:`ta_md_test.tadahdb
<../files/data/ta_md_test.tadahdb>`; ``data`` lies beside ``tutorial``.

The :term:`training configuration` is Unit 2's, with the vacancy MD on a
second :ref:`DBFILE <dbfile>` line and Unit 2's best values as the start.

:download:`config.train <../files/tutorial/unit3/config.train>`:

.. literalinclude:: ../files/tutorial/unit3/config.train
   :language: ini

The :term:`validation configuration` lists solid MD (index 1), vacancy MD
(2) and crystals compressed along one axis (3), a deformation the MD lacks.

:download:`config.val <../files/tutorial/unit3/config.val>`:

.. literalinclude:: ../files/tutorial/unit3/config.val
   :language: ini

:download:`config.hpo <../files/tutorial/unit3/config.hpo>`:

.. literalinclude:: ../files/tutorial/unit3/config.hpo
   :language: none

Running the search and prediction
---------------------------------

The commands run in ``tutorial/unit3`` (:ref:`nfp_tutorial_files`).

1. The search is started with:

   .. code-block:: bash

      tadah hpo --config config.train --validation config.val \
          --hpotarget config.hpo

2. The best potential is tested on the MD snapshots of ``ta_md_test.tadahdb``,
   at pressures absent from the MD of the training and validation data
   (:ref:`nfp_ta_data`). The prediction reports energy, force and stress errors
   (:ref:`prediction`; options in :ref:`cli_predict`):

   .. code-block:: bash

      tadah predict -p best_pot.tadah -d ../../data/ta_md_test.tadahdb -F -S -A

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit3/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit3/expected.csv
   :header-rows: 1
   :widths: 60 40

The best pair cutoff lies near the upper bound of its box, 8.0 Å, so the
minimum may lie beyond it (:ref:`nfp_search_space_bounds`). The loss is flat
near the best: a build of Tadah! that rounds otherwise can end the search at
another density cutoff and width, at about the same loss.

Reading the result
------------------

The ``RESULT`` part of the header of ``best_pot.tadah`` gives each metric on
each validation dataset with its loss (:ref:`nfp_output_best`). A loss of 1 is
an error equal to the accuracy of its dataset (:ref:`nfp_targets_losses`): the
losses show which datasets lie furthest from their accuracies.

``hpo_optim_resolved.hpo`` holds the :hpo:key:`AUTO <OPTIM.AUTO>` box of
:ref:`SGRIDMB` as an explicit :hpo:key:`OPTIM` line (:ref:`nfp_output_files`).

In this run the cap on the vacancy snapshots rejects the candidate of
evaluation 7, with an :ref:`SGRIDMB` below the start (a broader density). Its
row of ``loss.tadah`` keeps the loss of each term, its total is the
:hpo:key:`FAILSCORE` value, and ``stats.tadah`` keeps its errors
(:ref:`nfp_output_histories`). The journal
warns once of the singular directions under :ref:`OALGO` ``3`` and of the
iteration limit of the evidence approximation, as in :ref:`Unit 2 <nfp_unit_2>`,
and of the rejection with its RMSE and the cap:

.. output: unit3 hpo_run.log tol=1e-2
.. code-block:: text

   WARNING   eval 7: PC_ERMSE of validation dataset 2
       (../../data/ta_vac_val.tadahdb) 40.47561044677067 meV/atom is above its
       --skip-above 40: the evaluation fails (1 evaluation rejected by --skip-above
       or --skip-rel so far)

The test-set rows of the table, set beside the errors on validation dataset 1
in the ``RESULT`` part of ``best_pot.tadah``, show how the potential carries
over to pressures absent from its training data.

How the values were chosen
--------------------------

The vacancy snapshots give the fit an environment that the solid MD lacks. The
error scales stay: :ref:`ESIGMA` is an error per atom, so it treats the 53-atom
vacancy cells and the 54-atom MD cells alike (:ref:`fit_objective`). The start
takes all the digits of Unit 2's best values, so the search continues where
Unit 2 ended. :ref:`LAMBDA` ``-1`` stays as well: the evidence approximation
finds the penalty in every fit, so it is no search parameter, and an
:hpo:key:`OPTIM LAMBDA <OPTIM.TARGET.LAMBDA>` line is refused under it.

Each :hpo:key:`PC_ERMSE` line sets the accuracy of its dataset, and its weight
is the inverse square of that accuracy (:ref:`nfp_targets_losses`). The MD and
vacancy snapshots take 2 meV/atom, the :ref:`ESIGMA` of the fit, and the
compressed crystals 10 meV/atom, about 1 % of the energy that the largest
compression, 20 %, adds. :hpo:key:`PC_FRMSE` takes 0.1 eV/Å, the :ref:`FSIGMA`,
and a weight of 0 for the compressed crystals, whose forces are zero by
symmetry. The energy and force terms are each the mean over the three datasets
(:ref:`nfp_targets_datasets`), so in the MD and vacancy datasets 0.1 eV/Å
counts as much as 2 meV/atom. :hpo:key:`PC_SRMSE_GPA` has no index list: it
scores the mean stress error, at the 0.5 GPa of :ref:`SSIGMA`.

An energy error of 40 meV/atom is 2.12 eV on a 53-atom vacancy cell, 71 % of
the vacancy formation energy (:ref:`nfp_ta_reference`). Such a potential cannot
place the energy of a vacancy, so :hpo:key:`--skip-above` ``40`` rejects it.
Here the cap saves no time, as the validation errors are computed already; with
:hpo:key:`PC_LAMMPS` terms it also skips their scripts
(:ref:`nfp_targets_rejection`). :hpo:key:`FAILSCORE` ``1000`` lies well above
the losses of usable candidates and keeps the totals in ``loss.tadah`` readable
(:ref:`nfp_targets_failures`). The optimiser receives a finite stand-in for a
failed loss, as the :hpo:key:`!FAILSCORE` entry describes.

The cutoffs take explicit bounds (:ref:`nfp_search_space_bounds`). Every pair
blip ends 1.0 Å past its centre, so by 7.0 Å. Above that, a larger
:ref:`RCUT2B` adds no neighbour to the pair term and only flattens the
``Cut_Cos`` taper. The upper bound 8.0 Å, above Unit 2's 7.5 Å, lets the
search also try flatter tapers. :ref:`RCUTMB` keeps Unit 2's bounds.

:ref:`SGRIDMB` takes :hpo:key:`RATIO <OPTIM.AUTO.RATIO>` ``2``, a box from half
to twice the start, the default box for this key (:hpo:key:`OPTIM.AUTO`),
stated so that the file shows it. The line adds :hpo:key:`LOG` itself, as the
width acts by factors (:ref:`nfp_search_space_transforms`). At the starting
:ref:`RCUTMB`, an :ref:`SGRIDMB` well below the start can raise densities of
the 250 GPa MD cells above 12, past the last embedding blip (:ref:`CEMBFUNC`,
:ref:`SEMBFUNC`), where the embedding function is zero.

:hpo:key:`MAXEVAL` ``75`` keeps Unit 2's 25 evaluations per search parameter. A
start from an earlier search usually takes a small :hpo:key:`STEP`
(:ref:`nfp_choosing_step`). Here the vacancy and compressed-crystal data move
the best values, so :hpo:key:`!STEP` ``0.25`` keeps the first steps at a
quarter of each box.

What to try next
----------------

The index list ``(1-3)`` on the :hpo:key:`PC_SRMSE_GPA` line gives the stress
error of each dataset a loss of its own, so one poor dataset weighs more
(:ref:`nfp_targets_datasets`). Its loss belongs to another objective and cannot
be compared with this one. Without the :hpo:key:`FAILSCORE` line, the rejected
row records the largest floating-point number, and the search is unchanged.

With an upper bound of 7.5 Å for :ref:`RCUT2B`, as in Unit 2, the first
:hpo:key:`STEP` along it is shorter and the local search takes another path
(:ref:`nfp_search_space_bounds`). A global search (:ref:`nfp_unit_5`) explores
wider boxes of :ref:`RCUTMB` and :ref:`SGRIDMB`.

A fixed :ref:`LAMBDA` such as ``1e-4`` in ``config.train`` and :hpo:key:`OPTIM
LAMBDA <OPTIM.TARGET.LAMBDA>` ``(1) 1e-6 1e-2 LOG`` make the penalty a fourth
search parameter; :hpo:key:`MAXEVAL` ``100`` keeps 25 evaluations per
search parameter.

Next: :ref:`nfp_unit_4`.
