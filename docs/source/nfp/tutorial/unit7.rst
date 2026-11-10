.. _nfp_unit_7:

Unit 7: Extending the Data
==========================

This unit adds hot, liquid and high-pressure snapshots to the solid data of
:ref:`nfp_unit_2` and compares the potentials found without and with them.
It shows how the training data, their :term:`priorities <priority>` and the
validation data shape a search and its :term:`best potential`. It follows
:ref:`nfp_unit_6` but returns to the model and search of Unit 2, so that
only the data, their priorities and the :term:`loss terms <loss term>`
change. Several validation datasets and a test set are introduced in
:ref:`nfp_unit_3`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

What this unit adds
-------------------

- A :term:`training configuration` that includes another with
  :ref:`CONFIG` and adds a dataset (:ref:`config_file`).
- A priority per training dataset with :ref:`WDBFILE`.
- Per-dataset targets for energies, forces and stresses alike, with the
  accuracies of the training data (:ref:`nfp_targets_datasets`).
- Two potentials compared by ``tadah predict`` on the same three datasets.

Files
-----

The baseline reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>`,
:download:`validation configuration <../files/tutorial/unit2/config.val>`
and :download:`target file <../files/tutorial/unit2/config.hpo>`.
The datasets are :download:`ta_md_train.tadahdb
<../files/data/ta_md_train.tadahdb>`, :download:`ta_md_val.tadahdb
<../files/data/ta_md_val.tadahdb>`, :download:`ta_md_test.tadahdb
<../files/data/ta_md_test.tadahdb>`, :download:`ta_md_ext_train.tadahdb
<../files/data/ta_md_ext_train.tadahdb>` and :download:`ta_md_ext_val.tadahdb
<../files/data/ta_md_ext_val.tadahdb>` (:ref:`nfp_ta_data`).

:download:`config.train <../files/tutorial/unit7/config.train>`:

.. literalinclude:: ../files/tutorial/unit7/config.train
   :language: ini

The ``CONFIG`` line includes Unit 2's training configuration
(:ref:`CONFIG`), and the ``DBFILE`` line adds the extended snapshots as a
second training dataset (:ref:`DBFILE`). The second value of
:ref:`WDBFILE` gives them a priority of 0.01 (:ref:`fit_objective`).

:download:`config.val <../files/tutorial/unit7/config.val>`:

.. literalinclude:: ../files/tutorial/unit7/config.val
   :language: ini

The order of the two ``DBFILE`` lines numbers the datasets in the index
lists of the :hpo:key:`PC_ERMSE`, :hpo:key:`PC_FRMSE` and
:hpo:key:`PC_SRMSE_GPA` lines: ``(1)`` is the solid, ``(2)`` the added
snapshots.

:download:`config.hpo <../files/tutorial/unit7/config.hpo>`:

.. literalinclude:: ../files/tutorial/unit7/config.hpo
   :language: none

The search space, :hpo:opt:`NLOPT:LN_BOBYQA`, :hpo:key:`MAXEVAL` and
:hpo:key:`STEP` are those of Unit 2. :hpo:key:`!PC_ERMSE`,
:hpo:key:`!PC_FRMSE` and :hpo:key:`!PC_SRMSE_GPA` each give the two datasets
their own accuracy, and so their own :term:`loss-term weight`
(:ref:`nfp_targets_losses`).

Running the comparison
----------------------

The commands run in ``tutorial/unit7``, with the solid-data baseline first.

1. The first command repeats the search of Unit 2 in this directory; its
   ``best_pot.tadah`` is the baseline. The second predicts with it on the
   solid validation snapshots, the solid test set at pressures absent from
   the training data, and the added validation snapshots:

   .. code-block:: bash

      tadah hpo --config ../unit2/config.train --validation ../unit2/config.val \
          --hpotarget ../unit2/config.hpo
      tadah predict -p best_pot.tadah -F -S -A -d ../../data/ta_md_val.tadahdb \
          ../../data/ta_md_test.tadahdb ../../data/ta_md_ext_val.tadahdb

2. The next commands use the extended data. The search writes its output
   files over those of the first (:ref:`nfp_output_files`), and the prediction
   reads its ``best_pot.tadah`` on the same three datasets:

   .. code-block:: bash

      tadah hpo --config config.train --validation config.val --hpotarget config.hpo
      tadah predict -p best_pot.tadah -F -S -A -d ../../data/ta_md_val.tadahdb \
          ../../data/ta_md_test.tadahdb ../../data/ta_md_ext_val.tadahdb

The two searches take about three minutes in all on one processor core.

The ``tadah predict -A`` table has a column per ``-d`` dataset
(:ref:`prediction`). As in :ref:`nfp_unit_2`, the journal of each search warns
once of the singular directions under :ref:`OALGO` ``3`` and of the iteration
limit of the evidence approximation. Each search runs to its budget of
evaluations. The first ends with its status and the :term:`balance table` of the
fit of its best potential:

.. output: unit7,unit7_short stdout search=../unit2/config.hpo tol=3e-2
.. code-block:: text

   [tadah hpo] NLopt finished: MAXEVAL_REACHED -- maximum evaluations exhausted
       after 75 of MAXEVAL 75 evaluation(s)
   [tadah hpo] best_pot.tadah:
   Fit balance: LAMBDA -1 (evidence approximation: 0.130168);
       28 coefficients, rank 26

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit7/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit7/expected.csv
   :header-rows: 1
   :widths: 60 40

Each row names the target file of its search in its last brackets. The
RMSE rows come from the prediction after it, the others from the search's
output files (:ref:`nfp_output`).

Reading the result
------------------

The two best losses cannot be compared: they score different validation
data with different loss terms (:ref:`nfp_targets_mean`). The predictions
on the same three datasets compare the two potentials.

The baseline predicts the solid validation snapshots with errors of
10.9 meV/atom, 0.34 eV/Å and 0.70 GPa. On the added snapshots it fails:
5800 meV/atom, 11.7 eV/Å and 1560 GPa. These hot, liquid and strongly
compressed states lie outside its training data, and no term of its loss
measured them.

The extended potential predicts the added snapshots with 83 meV/atom,
0.95 eV/Å and 4.5 GPa: forces and stresses within their chosen
accuracies, the energy above its own. On the solid validation snapshots its
energy error rises from 10.9 to 12.7 meV/atom; its force and stress
errors, 0.34 eV/Å and 0.71 GPa, are about equal to those of the
baseline. On the test set, which neither search saw, its energy error falls
from 28 to 19.3 meV/atom and its stress error from 4.0 to 0.80 GPa.
Its force error is about equal, 0.32 and 0.34 eV/Å.

The best values change with the data. The pair cutoff :ref:`RCUT2B`
shortens from 6.60 to 6.04 Å, still inside its box. :ref:`SGRIDMB`
rises from 0.151 to 0.206 Å\ :sup:`-2`: the many-body density decays
faster with distance. The density cutoff :ref:`RCUTMB` stays near 5.3 Å.

How the values were chosen
--------------------------

The potential is meant for the solid MD of Units 2 to 6
(:ref:`nfp_ta_data`). The added snapshots extend its data to hotter states,
the liquid and a stronger compression than that of the solid MD. The
potential need not predict them as accurately, so their chosen accuracies
are ten times larger. A priority of 0.01 acts on each squared residual as an
error scale ten times larger (:ref:`fit_objective`). A dataset priority
below 1 is the recommended way to fit data of large residuals beside
near-equilibrium data (:ref:`fit_objective_wide_range`).

The target file scores each validation dataset with the accuracies that the
matching training data have in the fit: those of the error scales for the
solid, ten times larger for the added snapshots.

The search space, starting point, optimiser and budget are those of Unit 2.
Its smallest pair cutoff still reaches the seventh bcc shell of the most
compressed added cells, as the comment of the target file explains. Only the
data, their priorities and the loss terms differ, so the predictions show what
the data change. The ``CONFIG`` line keeps the model of Unit 2 in one file.

What to try next
----------------

- Equal priorities, ``WDBFILE 1 1``, hold the added snapshots to the
  accuracy of the solid in the fit; the predictions on the three datasets
  show the effect on each.
- Unit 2's training configuration with this unit's validation configuration
  and target file lets the added snapshots choose the potential without
  being fitted: it tests whether validation data can replace training data.
- This unit's training configuration with Unit 2's validation configuration
  and target file fits the added snapshots but scores only the solid: it
  separates the effect of the training data from that of the loss terms.

Next: :ref:`nfp_examples`.
