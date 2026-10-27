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
   :widths: 25 75

   * - Key
     - Values and use
   * - ``TYPE``
     - ``PLAIN`` (default), ``MULTISTART`` or ``PRESTAGE``.
   * - ``N_STARTS``
     - Integer >= 2, no default; ``MULTISTART`` only, which requires it
       (one start is ``PLAIN``).
   * - ``BUDGET_PER_START``
     - Integer in ``[1, 2147483647]``, no default; per-start ``MAXEVAL``
       override for ``MULTISTART``.
   * - ``TOPK``
     - Positive integer, default ``1``; number of exploration candidates
       requested for ``PRESTAGE`` refinement.

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

``MULTISTART`` runs :hpo:key:`N_STARTS <STRATEGY.N_STARTS>` separate
starts, one from each of the first
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` points of the
:hpo:block:`INIT <BLOCK.INIT>` chain; :doc:`nfp_initialisation` supplies
the points. ``MULTISTART`` runs a local optimiser from each start. It
refuses a global optimiser that takes one starting point, and an
optimiser that takes every :hpo:block:`!INIT <BLOCK.INIT>` point in one
run, :hpo:opt:`DLIB:GFS`, which it would run once whatever
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>`.
:hpo:key:`TYPE PLAIN <STRATEGY.TYPE.PLAIN>` runs either once;
:hpo:opt:`!DLIB:GFS` then takes every :hpo:block:`!INIT <BLOCK.INIT>`
point.

An :hpo:block:`!INIT <BLOCK.INIT>` chain that gives fewer points than
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` is refused at startup
(``tadah hpo --check`` too), and so is a ``RESTART`` chain of fewer
points (:ref:`what-restarts`). The entry of
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` says how the points are
counted, and when a chain drawn with fewer points runs fewer starts. The
points of an :hpo:block:`!INIT <BLOCK.INIT>` chain after the first
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` are not evaluated, and a
startup warning says so. A restart runs from the first
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` points of its chain.

.. versionchanged:: 1.6.0
   1.5.0 accepted :hpo:opt:`!DLIB:GFS` under ``MULTISTART`` and ran it
   once, and ran fewer starts than :hpo:key:`!N_STARTS <STRATEGY.N_STARTS>`
   from a chain of fewer points.

If ``BUDGET_PER_START`` is absent, a main ``MAXEVAL M`` is divided among
``k`` actual starts as ``ceil(M/k)`` per start. Rounding can make the sum
larger than ``M``. A supplied ``BUDGET_PER_START`` replaces this division.
``MAXTIME`` applies separately to each search.

Separate starts run one after another. ``THREADS`` sets the workers that
run the evaluation work within each search.

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
