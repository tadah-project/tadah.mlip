Block and key directory
=======================

Use this page to find **where** a setting belongs. Follow the reference
links for accepted values, defaults and combinations. Block names and keys
are separate: ``METHOD`` is a key inside ``INIT``; ``INIT`` opens a block.

Root directives
---------------

These appear outside ``OPTIMIZER``.

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
       ``PC_FRMSE``, ``PC_SRMSE``
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

The legacy root key ``MAXTIME`` aliases ``PC_SCRIPT_TIMEOUT``. Inside an
optimiser block, ``MAXTIME`` has a different meaning: a search time budget.
Use ``PC_SCRIPT_TIMEOUT`` at root level to make the distinction clear.

Shared optimiser keys
---------------------

The following keys are accepted in ``OPTIMIZER``, ``STRATEGY``, ``EXPLORE``
and ``REFINE``. Each backend supports a subset of their settings; being in
the right scope does not make a setting valid for every algorithm.

.. list-table::
   :header-rows: 1
   :widths: 48 52

   * - Keys
     - Purpose and value reference
   * - ``LIB``, ``ALGO``
     - Driver selection: :doc:`optimizer`.
   * - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``,
       ``XTOL_REL``, ``XTOL_ABS``
     - Budgets and convergence: :doc:`nfp_stopping_settings`.
   * - ``POPULATION``, ``STEP``, ``SEED``, ``VECTOR_ARR``, ``THREADS``
     - Driver controls: :doc:`nfp_optimizer_settings`.
   * - ``GRAD_STRATEGY``
     - Derivative calculation: :doc:`nfp_gradients`.
   * - ``PARAM``
     - Algorithm-specific settings: :doc:`nfp_optimizer_settings`.

Block scopes
------------

``OPTIMIZER`` — the outer search
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains the shared optimiser keys and ``LOG_HP`` (search-coordinate
scaling; :doc:`nfp_search_space`). Child blocks: ``INIT``, ``NOISE`` and
``STRATEGY``. It is a container for the outer search, not the inner
regression that fits model coefficients.

``STRATEGY`` — how searches are arranged
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains the shared optimiser keys and ``LOG_HP``, plus ``TYPE``,
``N_STARTS``, ``PARALLEL``, ``BUDGET_PER_START`` and ``TOPK``.
Child blocks: ``INIT``, ``EXPLORE``, ``REFINE``, ``INNER`` and ``RESTART``.
The strategy determines which of these keys and blocks can be used.
See :doc:`nfp_strategies`.

``EXPLORE`` and ``REFINE`` — staged searches
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contain the shared optimiser keys. Child blocks: ``INIT`` and ``INNER``.
``EXPLORE`` configures the exploration driver. ``REFINE`` groups settings
for the strategy's refinement driver. Both require a staged strategy.
See :doc:`nfp_strategies`.

``INIT`` — starting points
~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains ``METHOD``, ``K_TOTAL``, ``SEED`` and ``WARM_ON_MISMATCH``; no
child blocks. It can be placed inside ``OPTIMIZER``, ``STRATEGY``,
``EXPLORE`` or ``REFINE``. Repeat ``METHOD`` to build a seed chain.
The old key ``STRATEGY`` inside this block aliases ``METHOD``.
See :doc:`nfp_initialisation`.

``NOISE`` — objective-noise calibration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains ``AUTO`` and ``CALIBRATION``; no child blocks. It is placed
directly inside ``OPTIMIZER``. See :doc:`nfp_noise`.

``INNER`` — an NLopt local sub-optimiser
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains the shared optimiser keys **except** ``SEED``, ``GRAD_STRATEGY``
and ``STOPVAL``; no child blocks. It is placed in ``STRATEGY``, ``EXPLORE``
or ``REFINE`` for a compatible NLopt composite driver.
It does not configure the model's inner regression. See :doc:`nfp_inner`.

``RESTART`` — launches after stagnation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Contains ``AFTER_STAGNATION``, ``MAX`` and the ``INIT`` keys (including
the legacy ``STRATEGY`` alias); no child blocks. It is placed inside
``STRATEGY``. Its method chain supplies fresh starts; it does not resume a
saved optimiser state. See :doc:`nfp_restart`.
