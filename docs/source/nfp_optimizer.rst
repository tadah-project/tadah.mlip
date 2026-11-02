.. _nfp_optimizer_block:

The OPTIMIZER Block
===================

This page shows how the blocks inside the ``OPTIMIZER`` block nest and
what each one sets. It assumes the target-file syntax (see
:doc:`nfp_configuration`).

The ``OPTIMIZER`` block of the target file sets the search: its optimiser,
its starting points, the noise settings and the strategy. A target file
has one ``OPTIMIZER`` block. Settings for the coefficient fit belong to
the training configuration (see :ref:`ConfigSection`).

Block hierarchy
---------------

``INIT``, ``NOISE`` and ``STRATEGY`` are blocks directly inside
``OPTIMIZER``. The other blocks belong inside ``STRATEGY`` or inside one
of its blocks:

.. code-block:: none

   OPTIMIZER
   ├── INIT
   ├── NOISE
   └── STRATEGY
       ├── INIT
       ├── EXPLORE
       │   ├── INIT
       │   └── INNER
       ├── REFINE
       │   ├── INIT
       │   └── INNER
       ├── INNER
       └── RESTART

Which blocks are required depends on the strategy and the algorithm.
``STRATEGY TYPE PRESTAGE`` requires ``EXPLORE``; some NLopt algorithms
require an explicit ``INNER`` (see :doc:`nfp_inner`). The annotated
:doc:`example target file <nfp_configuration>` shows where each block
starts and ends, for example ``STRATEGY`` and ``ENDSTRATEGY``.

Shared keys
-----------

The following keys are accepted directly in ``OPTIMIZER`` and in its
``STRATEGY``, ``EXPLORE`` and ``REFINE`` blocks. Algorithm support
and value restrictions are specified in the linked references.

.. list-table::
   :header-rows: 1
   :widths: 48 52

   * - Keys
     - Purpose and value reference
   * - ``LIB``, ``ALGO``
     - Optimiser selection: :doc:`optimizer`.
   * - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``,
       ``XTOL_REL``, ``XTOL_ABS``
     - Budgets and convergence: :doc:`nfp_stopping_settings`.
   * - ``POPULATION``, ``STEP``, ``SEED``, ``VECTOR_ARR``, ``THREADS``
     - Optimiser settings: :doc:`nfp_optimizer_settings`.
   * - ``GRAD_STRATEGY``
     - Derivative calculation: :doc:`nfp_gradients`.
   * - ``PARAM``
     - Algorithm-specific settings: :doc:`nfp_optimizer_settings`.

``LOG_HP`` is accepted directly in ``OPTIMIZER`` or ``STRATEGY``.
It controls search-coordinate scaling; see :doc:`nfp_search_space`.

INIT block
----------

``INIT`` supplies starting points. Repeated ``METHOD`` lines form a
chain of starting points. Inside ``INIT``, a ``STRATEGY`` line is read as
a ``METHOD`` line. See :doc:`nfp_initialisation`. The keys it accepts are
listed in the entry :hpo:block:`INIT <BLOCK.INIT>` of the reference.

Its parent can be ``OPTIMIZER``, ``STRATEGY``, ``EXPLORE`` or
``REFINE``. An ``EXPLORE``-level ``INIT`` configures the exploration
starting points. The other placements configure the main starting points.
``INIT`` contains keys only.

NOISE block
-----------

``NOISE`` holds the noise settings of the ``STRATEGY``-level optimiser.
``AUTO`` derives the finite-difference step from the ``OPTIM`` boxes, and
``CALIBRATION`` measures the noise of the loss at the first starting
point. Its parent is ``OPTIMIZER``, and it contains keys only. See
:doc:`nfp_noise`.

.. versionchanged:: 1.6.0
   An optimiser key inside ``NOISE``, such as ``LIB``, is refused. 1.5.0
   applied it to the main search, as if it stood directly inside
   ``OPTIMIZER``.

STRATEGY block
--------------

``STRATEGY`` sets how the search runs its optimiser (``TYPE PLAIN``,
``MULTISTART`` or ``PRESTAGE``) and holds the main optimiser settings. Its
parent is ``OPTIMIZER``. A plain search can place the shared keys directly
in ``OPTIMIZER``; without a ``STRATEGY`` block the search is ``PLAIN``.
A ``STRATEGY`` block adds ``TYPE`` and its keys and may hold the
``INIT``, ``INNER`` and ``RESTART`` blocks, and under ``TYPE PRESTAGE``
the ``EXPLORE`` and ``REFINE`` blocks.

The keys it accepts are listed in the entry
:hpo:block:`STRATEGY <BLOCK.STRATEGY>` of the reference. Which of
``N_STARTS``, ``BUDGET_PER_START`` and ``TOPK`` applies depends on
``TYPE``: ``N_STARTS`` and ``BUDGET_PER_START`` apply to
``TYPE MULTISTART`` and ``TOPK`` to ``TYPE PRESTAGE``. See
:doc:`nfp_strategies`.

EXPLORE and REFINE blocks
~~~~~~~~~~~~~~~~~~~~~~~~~

Both are children of ``STRATEGY`` and require ``TYPE PRESTAGE`` before
their opening delimiter. Each accepts the shared keys and can contain
``INIT`` and ``INNER``.

``EXPLORE`` sets the optimiser of the exploration. ``REFINE`` groups the
refinement settings of the surrounding ``STRATEGY``. See
:doc:`nfp_strategies`.

INNER block
~~~~~~~~~~~

``INNER`` sets the local search that NLopt's MLSL and AUGLAG run inside
the search of the block around it. Its parent is ``STRATEGY``, ``EXPLORE``
or ``REFINE``. It contains keys only, listed in the entry
:hpo:block:`INNER <BLOCK.INNER>` of the reference. The enclosing search
supplies its random stream, gradient strategy, loss threshold and workers
(see :doc:`nfp_inner`).

RESTART block
~~~~~~~~~~~~~

``RESTART`` configures fresh search launches after stagnation. Its parent
is ``STRATEGY``. Its keys are listed in the entry
:hpo:block:`RESTART <BLOCK.RESTART>` of the reference. Inside
``RESTART``, a ``STRATEGY`` line is read as a ``METHOD`` line, as inside
``INIT``. It contains keys only. See :doc:`nfp_restart`.
