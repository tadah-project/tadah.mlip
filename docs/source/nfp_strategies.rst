.. _strategy_block:

The STRATEGY, EXPLORE and REFINE Blocks
=======================================

This page shows how a search runs its optimiser: once, from several
starting points, or as an exploration followed by a refinement. It
assumes familiarity with the target file (:doc:`nfp_configuration`) and
the ``OPTIMIZER`` block (:doc:`nfp_optimizer_settings`).

:hpo:block:`STRATEGY <BLOCK.STRATEGY>` selects how the optimiser runs.
At most one such block is allowed inside
:hpo:block:`OPTIMIZER <BLOCK.OPTIMIZER>`. Without it, the main optimiser
runs with :hpo:key:`TYPE PLAIN <STRATEGY.TYPE.PLAIN>`. Common optimiser
settings are described in :doc:`nfp_optimizer_settings`.

Keys of the STRATEGY block
--------------------------

The block has the keys :hpo:key:`TYPE <STRATEGY.TYPE>`,
:hpo:key:`N_STARTS <STRATEGY.N_STARTS>`,
:hpo:key:`BUDGET_PER_START <STRATEGY.BUDGET_PER_START>` and
:hpo:key:`TOPK <STRATEGY.TOPK>`. :hpo:key:`!TYPE <STRATEGY.TYPE>` selects
how the optimiser runs; each of the other keys belongs to one type and is
described in the section of that type below. The reference entry of each
key gives its values, range and default.

A key of another :hpo:key:`!TYPE <STRATEGY.TYPE>` is refused, even when
its value equals its default. A :hpo:block:`RESTART <BLOCK.RESTART>`
block is optional inside :hpo:block:`!STRATEGY <BLOCK.STRATEGY>` for
every :hpo:key:`!TYPE <STRATEGY.TYPE>` (see :doc:`nfp_restart`). An
:hpo:block:`INNER <BLOCK.INNER>` block sets the local search that
NLopt's MLSL and AUGLAG algorithms run (see :doc:`nfp_inner`).

.. _type-plain:

Single search: PLAIN
--------------------

:hpo:key:`TYPE PLAIN <STRATEGY.TYPE.PLAIN>` runs the selected optimiser
once:

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB NLOPT
     ALGO LN_BOBYQA
     MAXEVAL 200
   ENDSTRATEGY

.. _type-multistart:

Multiple searches: MULTISTART
-----------------------------

:hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>` runs
:hpo:key:`N_STARTS <STRATEGY.N_STARTS>` separate starts, one from each
of the first :hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` points of the
:hpo:block:`INIT <BLOCK.INIT>` chain; :doc:`nfp_initialisation` supplies
the points. :hpo:key:`!MULTISTART <STRATEGY.TYPE.MULTISTART>` runs a
local optimiser from each start. It refuses a global optimiser that
takes one starting point. It also refuses :hpo:opt:`DLIB:GFS`, which
takes every :hpo:block:`!INIT <BLOCK.INIT>` point in one run and so would
run once whatever :hpo:key:`!N_STARTS <STRATEGY.N_STARTS>`.
:hpo:key:`TYPE PLAIN <STRATEGY.TYPE.PLAIN>` runs each of these
optimisers once; :hpo:opt:`!DLIB:GFS` then takes every
:hpo:block:`!INIT <BLOCK.INIT>` point.

An :hpo:block:`!INIT <BLOCK.INIT>` chain that gives fewer points than
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` is refused at startup
(``tadah hpo --check`` too), and so is a
:hpo:block:`RESTART <BLOCK.RESTART>` chain of fewer points
(:ref:`what-restarts`). The entry of
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` says how the points are
counted, and when a chain drawn with fewer points runs fewer starts. The
points of an :hpo:block:`!INIT <BLOCK.INIT>` chain after the first
:hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` are not evaluated, and a
startup warning says so.

.. versionchanged:: 1.6.0
   1.5.0 accepted :hpo:opt:`!DLIB:GFS` under ``MULTISTART`` and ran it
   once, and ran fewer starts than :hpo:key:`!N_STARTS <STRATEGY.N_STARTS>`
   from a chain of fewer points.

If :hpo:key:`BUDGET_PER_START <STRATEGY.BUDGET_PER_START>` is absent, a
main :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` ``M`` is divided among ``k``
actual starts as ``ceil(M/k)`` per start. Rounding can make the sum
larger than ``M``. A supplied
:hpo:key:`!BUDGET_PER_START <STRATEGY.BUDGET_PER_START>` replaces this
division. :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>` applies separately to
each search.

Separate starts run one after another.
:hpo:key:`THREADS <OPTIMIZER.THREADS>` sets the workers that run the
evaluation work within each search.

.. _type-prestage-staged-global-local:

Staged search: PRESTAGE
-----------------------

:hpo:key:`TYPE PRESTAGE <STRATEGY.TYPE.PRESTAGE>` must come before the
:hpo:block:`EXPLORE <BLOCK.EXPLORE>` and
:hpo:block:`REFINE <BLOCK.REFINE>` blocks.
:hpo:block:`!EXPLORE <BLOCK.EXPLORE>` is required and has its own
:hpo:key:`LIB <OPTIMIZER.LIB>` and :hpo:key:`ALGO <OPTIMIZER.ALGO>`. It
runs once and collects up to :hpo:key:`TOPK <STRATEGY.TOPK>` distinct
candidates with usable losses.

:hpo:block:`!REFINE <BLOCK.REFINE>` groups the main optimiser's
settings. Values written there configure the same optimiser as settings
at :hpo:block:`!STRATEGY <BLOCK.STRATEGY>` level.

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
:hpo:key:`!TOPK <STRATEGY.TOPK>` above 1 requires an optimiser that runs
from each start, or one that takes several starting points in one run.
The refinement budget is divided among actual candidates by the same
rule as :hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>`; exploration
has its separate budget.

When fewer than :hpo:key:`!TOPK <STRATEGY.TOPK>` usable candidates exist,
those candidates are refined. When every exploration evaluation fails, a
warning is written and refinement runs from the main
:hpo:block:`INIT <BLOCK.INIT>` points instead. An exploration
:hpo:block:`!INIT <BLOCK.INIT>` chain overrides the shared main starting
points. A refinement :hpo:block:`!INIT <BLOCK.INIT>` chain configures
those main points. Usable exploration candidates take precedence as
refinement starting points.
