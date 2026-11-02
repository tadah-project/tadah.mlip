.. _quick-start:

.. _hpo-quick-start:

.. _nfp_quick_start:

Quick Start
===========

This page runs the shortest real nested fitting search, which ends with a
best potential. The search needs a ``tadah`` build with nested fitting
enabled (see :ref:`installation_nfp`) and the files of the example in
:ref:`cli_example_2`. A working copy of that example keeps the files that
the run writes apart from the shipped ones.

.. _the-three-inputs:

Input files
-----------

``config.train`` (``--config``)
   The training configuration: the model, the training datasets and the
   regression settings (see :ref:`config_file`).

``config.val`` (``--validation``)
   The validation configuration: the validation datasets, listed with
   :ref:`DBFILE`.

``config.hpo`` (``--hpotarget``)
   The target file: the search parameters, the targets and the optimiser
   settings (see :doc:`nfp_configuration`).

Paths in the example are relative to the working directory, the directory
that contains these files and the two datasets.

.. _a-small-search:

Minimal search
--------------

For a short first run, the target file below takes the place of the
example's ``config.hpo``:

:download:`first_run.hpo <nfp_examples/first_run.hpo>`

.. literalinclude:: nfp_examples/first_run.hpo
   :language: none

This evaluates the configured cutoff first, then five grid values from
4.5 to 5.5: six evaluations in total. It fits the model at each value and
minimises the squared validation energy RMSE.
The model, training data and validation data come from the example.

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotarget config.hpo

After a successful run, the results are in ``best_pot.tadah``,
``best_loss.tadah`` and ``hpo_run.log``. :doc:`nfp_output_settings`
explains output controls; :doc:`nfp_search_space` describes parameter
selection for other models.

.. _other-starting-points:

Alternative inputs
------------------

A run using validation metrics needs the validation input. A run using
only physical-property targets can omit it. Each run selects one property
target family: ``PC_OBSERVABLE`` or ``PC_LAMMPS``; see :doc:`nfp_objective`.

A model whose coefficients are all fixed with :ref:`FIXINDEX` and
:ref:`FIXWEIGHT` can omit :ref:`DBFILE` from the training configuration
if its element set is declared with :ref:`ATOM`. :ref:`dm_embed_refit` and
the :doc:`gradient requirements <nfp_gradients>` describe that workflow.
