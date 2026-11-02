Block and Key Directory
=======================

This page shows where each target-file key is documented. It assumes the
target-file syntax of :doc:`nfp_configuration`.

The target file has top-level keys and one ``OPTIMIZER`` block. This
directory groups the top-level keys by purpose. The :doc:`nfp_optimizer`
page covers the block, the blocks inside it and their keys. Accepted
values, defaults and restrictions appear in the linked reference pages.

Top-level keys
--------------

These keys appear outside ``OPTIMIZER``.

.. list-table::
   :header-rows: 1
   :widths: 48 52

   * - Keys
     - Purpose and value reference
   * - ``OPTIM``, ``OPTIM_SCALE``
     - Parameters, bounds and coordinates: :doc:`nfp_search_space`.
   * - ``LOSS``, ``FAILSCORE``, ``FAILSCORE_GRADING``
     - Loss function and failed evaluations: :doc:`nfp_losses`.
   * - ``PC_EMAE``, ``PC_ERMSE``, ``PC_ErRMSE``, ``PC_ERSQ``,
       ``PC_FRMSE``, ``PC_SRMSE_GPA``
     - Validation metrics: :doc:`nfp_validation_targets`.
   * - ``PC_LAMMPS``, ``PC_FAILPROBE``
     - Script-derived targets and failure probe: :doc:`nfp_lammps_targets`.
   * - ``PC_OBSERVABLE``
     - Built-in property targets: :doc:`nfp_observable_targets`.
   * - ``PC_SCRIPT_TIMEOUT``, ``PC_ISOLATION``, ``PC_WORKER_TIMEOUT``
     - Script execution controls: :doc:`nfp_lammps_targets`.
   * - ``BUMPGATE``, ``RHOGATE``, ``RHOBARGATE``
     - Candidate feasibility checks: :doc:`nfp_feasibility`.
   * - ``OUTPUT``, ``BOUTPUT``, ``QFLUSH``, ``DIGITS``, ``DUMP``, ``BDUMP``
     - Histories, precision and potential archives: :doc:`nfp_output_settings`.

As a top-level key, ``MAXTIME`` is another name for
``PC_SCRIPT_TIMEOUT``. Inside ``OPTIMIZER`` and the blocks inside it,
``MAXTIME`` sets a search time budget. The explicit top-level spelling
``PC_SCRIPT_TIMEOUT`` distinguishes the script limit from the search
budget.

The ``OPTIMIZER`` block
-----------------------

``OPTIMIZER`` configures the search. The blocks directly inside it are
``INIT``, ``NOISE`` and ``STRATEGY``. The staged-search blocks
``EXPLORE`` and ``REFINE`` belong inside ``STRATEGY``.
The :doc:`nfp_optimizer` page describes their nesting and their keys.
