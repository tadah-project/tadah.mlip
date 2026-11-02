.. _restart_decorator:

Stagnation Restarts: RESTART
============================

This page shows how a search restarts when evaluations stop improving the
best loss. It assumes the target-file syntax (see :doc:`nfp_configuration`)
and the :hpo:block:`STRATEGY <BLOCK.STRATEGY>` block (see :doc:`nfp_strategies`).

An optional :hpo:block:`RESTART <BLOCK.RESTART>` block inside
:hpo:block:`STRATEGY <BLOCK.STRATEGY>` restarts the search when evaluations stop improving the best loss of the
run. Each restart starts a fresh search within the current process.

.. code-block:: none

   RESTART
     AFTER_STAGNATION 100
     MAX 3
     METHOD LHS 4
     SEED 456
   ENDRESTART

Keys
----

:hpo:key:`AFTER_STAGNATION <RESTART.AFTER_STAGNATION>` is the number of
evaluations without a better loss that triggers a restart, and
:hpo:key:`MAX <RESTART.MAX>` is the largest number of restarts. The
:hpo:key:`METHOD <INIT.METHOD>`, :hpo:key:`K_TOTAL <INIT.K_TOTAL>`,
:hpo:key:`SEED <INIT.SEED>` and
:hpo:key:`WARM_ON_MISMATCH <INIT.WARM_ON_MISMATCH>` keys draw the starting
points of each restart, with the syntax of :doc:`nfp_initialisation`. The
syntax, values and defaults of all six keys are in their reference
entries.

A :hpo:block:`!RESTART <BLOCK.RESTART>` block without
:hpo:key:`!AFTER_STAGNATION <RESTART.AFTER_STAGNATION>` or
:hpo:key:`!MAX <RESTART.MAX>` is refused, as is a value of 0 for either.
A block without a :hpo:key:`!METHOD <INIT.METHOD>` line uses
``METHOD CONFIG``, one point from the training configuration. The initial
launch uses :hpo:block:`INIT <BLOCK.INIT>`; the restart chain is used only
after stagnation.

.. _what-restarts:

Restart scope
-------------

A restart runs the whole :hpo:block:`!STRATEGY <BLOCK.STRATEGY>` block
again: a ``PLAIN`` search, all the ``MULTISTART`` starts, or both
``PRESTAGE`` phases. Each ``MULTISTART`` start and each point that an
exploration hands to refinement has its own stagnation window. A
stagnating exploration hands its candidates to refinement before the
enclosing strategy can restart.

Under :hpo:key:`TYPE MULTISTART <STRATEGY.TYPE.MULTISTART>`, each
restart runs :hpo:key:`N_STARTS <STRATEGY.N_STARTS>` starts from the
restart chain. A :hpo:block:`RESTART <BLOCK.RESTART>` block is refused
on a :hpo:opt:`TADAH:GRID` search, whose restarts would evaluate the
first nodes of the grid again (:hpo:topic:`rule <RULE.restart_grid>`).

.. versionchanged:: 1.6.0
   1.5.0 accepted a :hpo:block:`!RESTART <BLOCK.RESTART>` block on
   :hpo:opt:`!TADAH:GRID`, and ran fewer starts than
   :hpo:key:`!N_STARTS <STRATEGY.N_STARTS>` from a restart chain of fewer
   points.

Each restart gets fresh search budgets. Reaching an optimiser's own
stop normally ends the strategy. Restarts are triggered by stagnation.
After :hpo:key:`MAX <RESTART.MAX>` restarts, the next stagnation ends the
search.

:hpo:opt:`TADAH:ANNEAL_PT`, :hpo:opt:`TADAH:ANNEAL_PA` and
:hpo:opt:`ASA:ASA` with
:hpo:param:`Gener_Block_Max <PARAM.ASA:ASA.Gener_Block_Max>` above 1
evaluate several points at once. They count the window in the order of
the evaluation numbers, in which the run records the evaluations of a
:term:`batch` (see :ref:`nfp_evaluation_numbers`). The window therefore
closes at the same evaluation whatever the timing of the workers. It does
so at any :hpo:key:`THREADS <OPTIMIZER.THREADS>` under the conditions of
that section. When it closes during a batch, the later evaluations of the
batch still run and are recorded, and the restart follows the batch.

.. versionchanged:: 1.6.0
   In 1.5.0 the window closed at the evaluation that ended first past it,
   and the other replicas or walkers of an annealer stopped at their next
   proposal.

A chain that produces no point reuses the previous launch's starting
points and emits a warning. An exploration with its own ``INIT`` chain
keeps that chain; one sharing the main points receives the refreshed
points. Each restart also derives new seeds for the random numbers of the
optimisers, and ``hpo_run.log`` records them (see
:doc:`nfp_output_settings`). Without ``RESTART SEED``, a base seed
is drawn once and recorded when the first restart occurs.

A run on several MPI ranks refuses ``RESTART``. Search budgets are
described in :doc:`nfp_stopping_settings`, and starting a new run from a
saved potential in :doc:`nfp_initialisation`.
