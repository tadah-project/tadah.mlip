.. _quick-start:

.. _hpo-quick-start:

.. _nfp_quick_start:

Quick start
===========

This example requires a ``tadah`` build with nested fitting enabled and
a working copy of ``Tadah.MLIP/examples/CLI/ex_2`` (also shown in
:ref:`cli_example_2`). The working copy keeps the generated files with
the experiment.

.. _the-three-inputs:

Input files
-----------

.. list-table::
   :header-rows: 1
   :widths: 23 22 55

   * - Input
     - Command option
     - Contents
   * - ``config.train``
     - ``--config``
     - Model definition, training datasets and regression settings.
   * - ``config.val``
     - ``--validation``
     - Validation datasets, listed with ``DBFILE``.
   * - ``config.hpo``
     - ``--hpotarget``
     - Search parameters, targets and optimiser settings.

Paths in the example are relative to the working directory. Run from
the directory containing those files and the two datasets.

.. _a-small-search:

Minimal search
--------------

For a short first run, replace the example's ``config.hpo`` with:

.. literalinclude:: nfp_examples/first_run.hpo
   :language: none

This evaluates the configured cutoff first, then five grid values from
4.5 to 5.5: six evaluations in total. It fits the model at each value and
minimises the squared validation energy RMSE.
The model, training data and validation data come from the example.

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotarget config.hpo

After a successful run, inspect ``best_pot.tadah``, ``best_loss.tadah``
and ``hpo_run.log``. :doc:`nfp_output_settings` explains output controls;
:doc:`nfp_search_space` describes parameter selection for other models.

.. _other-starting-points:

Alternative inputs
------------------

A run using validation metrics needs the validation input. A run using
only physical-property targets can omit it. Each run selects one property
target family: ``PC_OBSERVABLE`` or ``PC_LAMMPS``; see :doc:`nfp_objective`.

A model with all coefficients pinned can omit training ``DBFILE`` if
its element set is declared with ``ATOM``. For that workflow, follow
:ref:`dm_embed_refit` and the :doc:`gradient requirements <nfp_gradients>`.
