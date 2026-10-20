Block and key directory
=======================

The outer-search file contains root directives and one ``OPTIMIZER``
block. This directory groups root keys by purpose. The :doc:`nfp_optimizer`
section covers the block, its subblocks and their keys. Accepted values,
defaults and restrictions appear in the linked reference pages.

Root directives
---------------

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

The legacy root key ``MAXTIME`` aliases ``PC_SCRIPT_TIMEOUT``. Inside
``OPTIMIZER`` and its search subblocks, ``MAXTIME`` sets a search time
budget. The explicit root spelling ``PC_SCRIPT_TIMEOUT`` distinguishes
the script limit from the search budget.

.. _shared-optimiser-keys:

.. _block-scopes:

.. _optimizer-the-outer-search:

.. _strategy-how-searches-are-arranged:

.. _explore-and-refine-staged-searches:

.. _init-starting-points:

.. _noise-objective-noise-calibration:

.. _inner-an-nlopt-local-sub-optimiser:

.. _restart-launches-after-stagnation:

Optimizer block
---------------

``OPTIMIZER`` configures the outer search. Its immediate subblocks are
``INIT``, ``NOISE`` and ``STRATEGY``. The staged-search subblocks
``EXPLORE`` and ``REFINE`` belong inside ``STRATEGY``.
See :doc:`nfp_optimizer` for their hierarchy and key directory.
