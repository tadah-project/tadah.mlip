.. _quick-start:

.. _hpo-quick-start:

.. _nfp_quick_start:

First run
=========

Use a ``tadah`` build with nested fitting enabled. Start from the files
in ``Tadah.MLIP/examples/CLI/ex_2`` (also shown in :ref:`cli_example_2`).
Copy that directory to a working directory before running so that the
output files stay with your experiment.

The three inputs
----------------

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

A small search
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
:doc:`nfp_search_space` explains how to replace the single ``OPTIM`` line
with parameters of your own model.

Other starting points
---------------------

A run using validation metrics needs the validation input. A run using
only physical-property targets can omit it. ``PC_OBSERVABLE`` and
``PC_LAMMPS`` cannot be mixed in the same run; see :doc:`nfp_objective`.

A model with all coefficients pinned can omit training ``DBFILE`` if
its element set is declared with ``ATOM``. For that workflow, follow
:ref:`dm_embed_refit` and the :doc:`gradient requirements <nfp_gradients>`.
