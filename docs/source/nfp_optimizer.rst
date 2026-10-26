Optimizer
=========

``OPTIMIZER`` is the root block for the outer search in ``config.hpo``.
It selects the search algorithm, stopping criteria and run policy.
Settings for the model's coefficient fit belong to the training
configuration.

Subblock hierarchy
------------------

``INIT``, ``NOISE`` and ``STRATEGY`` are immediate subblocks of
``OPTIMIZER``. The remaining subblocks belong to ``STRATEGY`` or one
of its stages:

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

Subblock requirements depend on the run policy and algorithm.
A staged search requires ``EXPLORE``; some NLopt composite algorithms
require an explicit ``INNER`` (see :doc:`nfp_inner`). The annotated
:doc:`configuration template <nfp_configuration>` shows the matching
opening and closing delimiters.

Shared keys
-----------

The following keys are accepted directly in ``OPTIMIZER`` and in its
``STRATEGY``, ``EXPLORE`` and ``REFINE`` subblocks. Algorithm support
and value restrictions are specified in the linked references.

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

``LOG_HP`` is accepted directly in ``OPTIMIZER`` or ``STRATEGY``.
It controls search-coordinate scaling; see :doc:`nfp_search_space`.

INIT subblock
-------------

``INIT`` supplies starting points. It accepts ``METHOD``, ``K_TOTAL``,
``SEED`` and ``WARM_ON_MISMATCH``. Repeated ``METHOD`` entries form a
seed chain. The legacy key ``STRATEGY`` inside ``INIT`` aliases
``METHOD``. See :doc:`nfp_initialisation`.

Its parent can be ``OPTIMIZER``, ``STRATEGY``, ``EXPLORE`` or
``REFINE``. An ``EXPLORE``-level ``INIT`` configures the exploration
starting points. The other placements configure the main starting points.
``INIT`` contains keys only.

NOISE subblock
--------------

``NOISE`` configures objective-noise detection and calibration. It accepts
``AUTO`` and ``CALIBRATION``. Its parent is ``OPTIMIZER``, and it contains
keys only. See :doc:`nfp_noise`.

.. versionchanged:: 1.6.0
   An optimiser key inside ``NOISE``, such as ``LIB``, is refused. 1.5.0
   applied it to the main search, as if it stood directly inside
   ``OPTIMIZER``.

STRATEGY subblock
-----------------

``STRATEGY`` groups the run policy and main optimiser settings. Its
parent is ``OPTIMIZER``. A plain search can place shared keys directly
in ``OPTIMIZER``; an explicit ``STRATEGY`` adds policy controls and
supports the nested search subblocks.

It accepts the shared keys and ``LOG_HP``, plus ``TYPE``, ``N_STARTS``,
``BUDGET_PER_START`` and ``TOPK``. Their applicability depends on the
policy. See :doc:`nfp_strategies`.

EXPLORE and REFINE subblocks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Both are children of ``STRATEGY`` and require ``TYPE PRESTAGE`` before
their opening delimiter. Each accepts the shared keys and can contain
``INIT`` and ``INNER``.

``EXPLORE`` configures the exploration driver. ``REFINE`` groups the
refinement settings of the surrounding ``STRATEGY``. See
:doc:`nfp_strategies`.

INNER subblock
~~~~~~~~~~~~~~~

``INNER`` configures the local sub-optimiser of an eligible NLopt
composite driver. Its parent is ``STRATEGY``, ``EXPLORE`` or ``REFINE``.
It accepts ``LIB``, ``ALGO``, ``MAXEVAL``, ``MAXTIME``, ``FTOL_REL``,
``FTOL_ABS``, ``XTOL_REL``, ``XTOL_ABS``, ``POPULATION``, ``STEP``,
``VECTOR_ARR`` and ``PARAM``. It contains keys only. The enclosing
search supplies the random stream, gradient strategy, loss threshold and
workers: ``THREADS`` is refused there. See :doc:`nfp_inner`.

RESTART subblock
~~~~~~~~~~~~~~~~

``RESTART`` configures fresh search launches after stagnation. Its parent
is ``STRATEGY``. It accepts ``AFTER_STAGNATION``, ``MAX`` and the
``INIT`` keys, including the legacy ``STRATEGY`` alias for ``METHOD``.
It contains keys only. See :doc:`nfp_restart`.
