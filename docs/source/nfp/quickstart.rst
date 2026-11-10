.. _nfp_quickstart:

Quick Start
===========

This quick start runs a first nested fitting search: ``tadah hpo``
searches the pair cutoff of a Ta potential on a grid and keeps the
potential with the smallest validation energy error. It needs a
:ref:`Nested Fitting build <installation_nfp>`; the data are those of
:ref:`the Ta data <nfp_ta_data>`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Files
-----

The commands expect the layout of the :download:`files archive
<nfp-files.zip>` (:ref:`nfp_tutorial_files`). The potential is fitted to the
energies of
:download:`ta_eos_train.tadahdb <files/data/ta_eos_train.tadahdb>` and
validated on those of :download:`ta_eos_val.tadahdb
<files/data/ta_eos_val.tadahdb>`: body-centred cubic cells over a range of
lattice constants.

The :term:`training configuration` (``--config``, :ref:`config_file`)
names the training data, the pair term and the model, as for
``tadah train``. Its value of :ref:`RCUT2B` is the starting point of the
search (:ref:`nfp_training_hpo_start`).

:download:`config.train <files/tutorial/unit1/config.train>`:

.. literalinclude:: files/tutorial/unit1/config.train
   :language: ini

The :term:`validation configuration` (``--validation``) names the
validation data (:ref:`validation`).

:download:`config.val <files/tutorial/unit1/config.val>`:

.. literalinclude:: files/tutorial/unit1/config.val
   :language: ini

The :term:`target file` (``--hpotarget``, :ref:`nfp_target_file`) makes
:ref:`RCUT2B` a :term:`search parameter` (:hpo:key:`OPTIM`), scores each
candidate by its validation energy RMSE (:hpo:key:`PC_ERMSE`) and searches
on a grid (:hpo:opt:`TADAH:GRID`).

:download:`config.hpo <files/tutorial/unit1/config.hpo>`:

.. literalinclude:: files/tutorial/unit1/config.hpo
   :language: none

Running the search
------------------

The commands run in ``files/tutorial/unit1``. The first runs the search; the
second predicts the validation data with the :term:`best potential` and,
with ``-A``, prints its energy errors on them (:ref:`prediction`; options
in :ref:`cli_predict`):

.. code-block:: bash

   tadah hpo --config config.train --validation config.val \
       --hpotarget config.hpo
   tadah predict -p best_pot.tadah -d ../../data/ta_eos_val.tadahdb -A

The two commands take a few seconds on one processor core.

The search prints a few progress lines and ends with the
:term:`balance table` of the best potential: its errors on the training
data (:ref:`fit_balance`). The last progress lines and the first rows of
the table:

.. output: quickstart stdout tol=1e-2

.. code-block:: text

   ...
   [tadah hpo] TADAH:GRID   dim=1
   [tadah hpo] GRID search: 11 points in each dimension (PARAM GRID_POINTS; spacing
       = width/10), 1 dimension.
   [tadah hpo] TADAH:GRID finished: the INIT point and all 11 grid points evaluated
   [tadah hpo] best_pot.tadah:
   Fit balance: LAMBDA 0; 9 coefficients, rank 9
     error scales: ESIGMA 1 meV/atom
     quantity  dataset   n    RMSE  unit      r/sigma  share  gamma
     energy    all      40  0.5122  meV/atom   0.5122  1.000  9.000

Expected result
---------------

:download:`expected.csv <files/tutorial/unit1/expected.csv>`:

.. csv-table::
   :file: files/tutorial/unit1/expected.csv
   :header-rows: 1
   :widths: 60 40

The best loss, 0.2395, the square of the validation energy RMSE in
meV/atom (:ref:`nfp_unit_1`), is reached at an :ref:`RCUT2B` of 5.0 Å, the
value of the :ref:`RCUT2B` line of ``best_pot.tadah``.

The ``RESULT`` section of ``hpo_run.log`` gives the status of the
optimiser, the best loss and its evaluation, the validation energy RMSE of
the best potential and the file that holds it:

.. output: quickstart hpo_run.log tol=best_loss

.. code-block:: text

   -- RESULT --------------------------------------------------------------
     Optimiser status        : TADAH:GRID: all grid points evaluated
     Best total loss         : 2.3946e-01  (eval 6)
     Best ERMSE              : 0.49 meV/atom
     Best potential saved    : best_pot.tadah

``tadah predict -A`` prints the same validation energy RMSE, in meV/atom:

.. output: quickstart stdout command=2 tol=1e-2

.. code-block:: text

   Metric                              Mean            Val1
   ...
   Energy RMSE (meV/atom)      4.893484e-01    4.893484e-01

Where to look
-------------

The run writes its files into ``files/tutorial/unit1``
(:ref:`nfp_output_files`):

``best_pot.tadah``
   The best potential, for ``tadah predict`` (:ref:`prediction`) and for
   LAMMPS with the element ``Ta`` (:ref:`lammps_usage`); its header records
   the search (:ref:`nfp_output_best`).
``hpo_run.log``
   The journal: the settings, each improvement of the best loss and the
   result (:ref:`nfp_output_histories`).
``loss.tadah``
   The loss of every evaluation; ``params.tadah`` holds the cutoff of
   each (:ref:`nfp_output_histories`).

Next: :ref:`nfp_unit_1`, which runs the same input files and explains each
of their values.
