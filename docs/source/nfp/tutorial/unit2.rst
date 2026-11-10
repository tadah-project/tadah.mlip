.. _nfp_unit_2:

Unit 2: Forces, Stresses and Several Search Parameters
======================================================

This unit fits a potential with a pair term and a many-body term to the
energies, forces and stresses of solid MD snapshots. A local optimiser
varies three :term:`search parameters <search parameter>` at once. The unit
builds on :ref:`nfp_unit_1` and uses the solid MD data of
:ref:`the Ta data <nfp_ta_data>`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

What this unit adds
-------------------

The unit shows how one accuracy per quantity sets both the fit of each
:term:`evaluation` and the loss of the search, and how the number of search
parameters sets the budget. Compared with :ref:`nfp_unit_1`:

- the fit includes forces and stresses (:ref:`FORCE <force>`,
  :ref:`STRESS <stress>`), each quantity with its
  :term:`error scale` (:ref:`fit_objective`);
- a many-body term, ``DM_mBlipEnv``, adds an embedding energy of a
  density of the neighbours of each atom (:ref:`dm_mead_variants`);
- a penalty that the evidence approximation estimates in every fit
  (:ref:`LAMBDA` ``-1``, with :ref:`OALGO` ``3``);
- three :hpo:key:`OPTIM` lines, one of them tagged :hpo:key:`LOG`;
- three :term:`loss terms <loss term>`, :hpo:key:`PC_ERMSE`,
  :hpo:key:`PC_FRMSE` and :hpo:key:`PC_SRMSE_GPA`, one per quantity;
- a local optimiser, :hpo:opt:`NLOPT:LN_BOBYQA`, with a budget
  (:hpo:key:`MAXEVAL`) and a first step (:hpo:key:`STEP`).

Files
-----

The files lie under ``files/`` of the :ref:`archive <nfp_tutorial_files>`: the
three input files in ``tutorial/unit2``, the datasets
:download:`ta_md_train.tadahdb <../files/data/ta_md_train.tadahdb>` and
:download:`ta_md_val.tadahdb <../files/data/ta_md_val.tadahdb>` in ``data``.

:download:`config.train <../files/tutorial/unit2/config.train>`:

.. literalinclude:: ../files/tutorial/unit2/config.train
   :language: ini

The :term:`validation configuration` names the validation snapshots.

:download:`config.val <../files/tutorial/unit2/config.val>`:

.. literalinclude:: ../files/tutorial/unit2/config.val
   :language: ini

The :term:`target file` searches two cutoffs and the density width.

:download:`config.hpo <../files/tutorial/unit2/config.hpo>`:

.. literalinclude:: ../files/tutorial/unit2/config.hpo
   :language: none

Running the search
------------------

#. The search is started in ``tutorial/unit2`` with:

   .. code-block:: bash

      tadah hpo --config config.train --validation config.val \
          --hpotarget config.hpo

The search takes under a minute on one processor core.

The journal warns once per run, at the first evaluation, that some singular
directions of the design matrix, too weakly determined by the data, lie at or
below the rank threshold of :ref:`OALGO` ``3``, and that the evidence
approximation stopped at its iteration limit (its last penalty holds). The fit
leaves those directions out of the coefficients; each evaluation gives its loss.

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit2/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit2/expected.csv
   :header-rows: 1
   :widths: 60 40

Each best value lies inside its box. Compared with the training configuration,
the search lengthens both cutoffs and lowers :ref:`SGRIDMB`, so that the
density decays more slowly with distance. The ``RESULT`` section that ends
``hpo_run.log`` gives the validation errors of the best potential:

.. output: unit2 hpo_run.log tol=1e-2
.. code-block:: text

   Best total loss         : 1.4332e+01  (eval 75)
   Best ERMSE              : 10.89 meV/atom
   Best FRMSE              : 0.3380 eV/Ang
   Best SRMSE              : 0.6971 GPa

Reading the result
------------------

The loss of an evaluation is the mean of its three terms
(:ref:`nfp_targets_mean`). Each term compares a validation error with its
accuracy, a term of 1 being an error equal to it (:ref:`nfp_targets_losses`).
The ``RESULT`` part of the header of ``best_pot.tadah`` gives each error of the
:term:`best potential` and its term (:ref:`nfp_output_best`); ``loss.tadah``
holds the terms of every evaluation. The largest term, here the energy's, names
the quantity that limits the potential. The :term:`balance table` that ends the
run shows how the fit of the best potential shares its sum over the training
data among the quantities (:ref:`fit_balance`):

.. output: unit2 stdout tol=1e-2
.. code-block:: text

   quantity  dataset      n    RMSE  unit      r/sigma     share   gamma
   energy    all         88   15.50  meV/atom    7.748   0.03063   4.322
   force     all      14256  0.3405  eV/Å        3.405    0.9582   16.89
   stress    all        528  0.7165  GPa         1.433  0.009432   4.392

``params.tadah`` holds the search parameters of every evaluation
(:ref:`nfp_output_histories`). Its first row is the training configuration as
written. The next three take one :hpo:key:`STEP` up along each search parameter
in turn, and the three after them one :hpo:key:`!STEP` down in the same order.
:hpo:opt:`NLOPT:LN_BOBYQA` builds its first model of the loss from these seven
rows (:ref:`nfp_choosing_local`), and the later rows follow the model.

The best loss comes from evaluation 75, the last of the budget: the search
stops at :hpo:key:`MAXEVAL` while it still lowers the loss by small amounts, so
it has not finished (:ref:`nfp_choosing_budget`). :ref:`nfp_unit_3` starts a
search with more data from these values.

How the values were chosen
--------------------------

The unit chooses one accuracy for each quantity: 2 meV/atom for the energy per
atom, 0.1 eV/Å for a force component and 0.5 GPa for the stress. Each is the
error acceptable in the intended use of the potential, MD of the solid: a
choice, not a property of the data. An MD potential needs accurate forces, and
the stresses tie it to the pressure of each cell. The training configuration
gives these accuracies as the error scales of the fit, :ref:`ESIGMA`,
:ref:`FSIGMA` and :ref:`SSIGMA` (:ref:`fit_objective`).

The target file scores the validation errors with the same accuracies. The
:term:`loss-term weight` of each :hpo:key:`L2` term is one over the square of
its accuracy (:ref:`nfp_targets_losses`): 0.25 for :hpo:key:`PC_ERMSE`, 100 for
:hpo:key:`PC_FRMSE` and 4 for :hpo:key:`PC_SRMSE_GPA`. The fit sums over
observations, 14256 force components against 88 energies, and the loss gives
each quantity one term. The forces take most of the fit's sum, the energy most
of the loss (:ref:`nfp_targets_mean`). Each accuracy appears in both files.

Every evaluation fits the coefficients (:ref:`nfp_training_hpo_fit`); the
search covers values that no fit sets. The three search parameters are the pair
cutoff, :ref:`RCUT2B`, the cutoff of the density, :ref:`RCUTMB`, and the density
width, :ref:`SGRIDMB`. The grids of the blips stay fixed, and so does
:ref:`WATOM`: each further search parameter adds a dimension and its share of
the budget.

The boxes come from the geometry of the basis functions and of the body-centred
cubic crystal, as the comments of ``config.hpo`` state
(:ref:`nfp_search_space_bounds`). The box of :ref:`SGRIDMB` spans a factor of
20, and under the :hpo:key:`LOG` tag equal ratios of its value take equal parts
of the box (:ref:`nfp_search_space_transforms`).

The loss has no LAMMPS property, so :hpo:opt:`NLOPT:LN_BOBYQA` suits it
(:ref:`nfp_choosing_local`): it finds the minimum nearest the training
configuration with few evaluations. The start is a guess, so a global search
first is safer (:ref:`nfp_choosing_factors`), as in :ref:`nfp_unit_5`. The
optimiser draws no random numbers and refuses :hpo:key:`OPTIMIZER.SEED`.

:hpo:key:`MAXEVAL` ``75``, 25 per search parameter, leaves most of the budget
to the model steps after the first seven rows (:ref:`nfp_choosing_budget`). The
start is a guess, so :hpo:key:`STEP` stays at ``0.25``, the default of
:hpo:opt:`!NLOPT:LN_BOBYQA`: first steps of a quarter of each box, additive for
the cutoffs and multiplicative for :ref:`SGRIDMB` (:ref:`nfp_choosing_step`).

What to try next
----------------

Each variation runs in a copy of ``tutorial/unit2`` (:ref:`nfp_tutorial_files`).

- A smaller :hpo:key:`STEP`, such as ``0.1``, keeps the first evaluations
  closer to the start, as suits a start from an earlier search.
- :hpo:opt:`NLOPT:LN_SBPLX` in place of :hpo:opt:`NLOPT:LN_BOBYQA`, with the
  same budget, compares a simplex search with a model-based one on the same
  loss (:ref:`nfp_choosing_local`).
- :ref:`WATOM` as a fourth search parameter, an :hpo:key:`OPTIM` line with a
  :hpo:key:`LOG` box around its value, needs a larger budget: a
  :hpo:key:`MAXEVAL` of 100 keeps 25 evaluations per search parameter.
- A smaller energy accuracy, 1 meV/atom, changes both files: :ref:`ESIGMA`
  ``1`` in the training configuration and a loss-term weight of 1 on the
  :hpo:key:`PC_ERMSE` line. The fit and the search then both favour the
  energies.

Next: :ref:`nfp_unit_3`.
