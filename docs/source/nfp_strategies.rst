.. _strategy_block:

.. _type-multistart:

.. _type-plain:

.. _type-prestage-staged-global-local:

Run policies: STRATEGY, EXPLORE and REFINE
==========================================

``STRATEGY`` selects how the optimiser runs. At most one such block is
allowed inside ``OPTIMIZER``. Without it, the main optimiser runs with
``TYPE PLAIN``. Common optimiser settings are described in
:doc:`nfp_optimizer_settings`.

Policy keys
-----------

.. list-table::
   :header-rows: 1
   :widths: 25 18 57

   * - Key
     - Default
     - Values and use
   * - ``TYPE``
     - ``PLAIN``
     - ``PLAIN``, ``MULTISTART`` or ``PRESTAGE``.
   * - ``N_STARTS``
     - ``1``
     - Positive integer; ``MULTISTART`` only.
   * - ``PARALLEL``
     - ``1``
     - Positive integer. Separate starts currently run sequentially;
       requesting more than ``1`` for a batch produces a warning and
       retains sequential execution.
   * - ``BUDGET_PER_START``
     - Unset
     - Integer in ``[1, 2147483647]``; per-start ``MAXEVAL`` override for
       ``MULTISTART``.
   * - ``TOPK``
     - ``1``
     - Positive integer; number of exploration candidates requested for
       ``PRESTAGE`` refinement.

Keys for another policy are rejected, even when their value equals its
default. ``RESTART`` is an optional child of any policy; see
:doc:`nfp_restart`. ``INNER`` is available for eligible NLopt backends;
see :doc:`nfp_inner`.

.. _one-search-plain:

Single search: PLAIN
--------------------

Run the selected optimiser once::

   STRATEGY
     TYPE PLAIN
     LIB NLOPT
     ALGO LN_BOBYQA
     MAXEVAL 200
   ENDSTRATEGY

.. _several-searches-multistart:

Multiple searches: MULTISTART
-----------------------------

``MULTISTART`` uses up to ``min(N_STARTS, available INIT points)``
separate starts. Supply enough points with :doc:`nfp_initialisation`.
It accepts local optimisers and backends that support multiple seeds.
A global backend that consumes only one seed is rejected for this policy.

If ``BUDGET_PER_START`` is absent, a main ``MAXEVAL M`` is divided among
``k`` actual starts as ``ceil(M/k)`` per start. Rounding can make the sum
larger than ``M``. A supplied ``BUDGET_PER_START`` replaces this division.
``MAXTIME`` applies separately to each search.

A backend that accepts all seeds in one run, currently ``DLIB:GFS``,
runs once with those seeds and rejects ``BUDGET_PER_START``. Separate
starts currently run sequentially, including when ``PARALLEL > 1``
requests concurrent starts. ``THREADS`` controls parallel evaluation
work within each search.

.. _explore-then-refine-prestage:

Staged search: PRESTAGE
-----------------------

Declare ``TYPE PRESTAGE`` before its phase blocks. ``EXPLORE`` is
required and has its own ``LIB`` and ``ALGO``. It runs once and collects
up to ``TOPK`` distinct candidates with usable losses.

``REFINE`` groups the main optimiser's settings. Values written there
configure the same optimiser as settings at ``STRATEGY`` level.

.. code-block:: none

   STRATEGY
     TYPE PRESTAGE
     TOPK 3
     EXPLORE
       LIB TADAH
       ALGO RANDOM
       SEED 123
       MAXEVAL 100
     ENDEXPLORE
     REFINE
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 300
       XTOL_REL 1e-5
     ENDREFINE
   ENDSTRATEGY

The refinement optimiser must use the starting point it is given.
``TOPK > 1`` additionally requires a local or multi-seed backend. The
refinement budget is divided among actual candidates by the same rule
as ``MULTISTART``; exploration has its separate budget.

When fewer than ``TOPK`` usable candidates exist, those candidates are
refined. When every exploration evaluation fails, the driver warns and
runs refinement from the main ``INIT`` points instead. An exploration
``INIT`` chain overrides the shared main starting points. A refinement
``INIT`` chain configures those main points. Usable exploration candidates
take precedence as refinement starting points.
