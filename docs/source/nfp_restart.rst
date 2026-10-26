.. _restart_decorator:

Stagnation restarts: RESTART
============================

An optional ``RESTART`` block inside ``STRATEGY`` relaunches the policy
when evaluations stop improving the run's best loss. Each relaunch starts
a fresh search within the current process.

.. code-block:: none

   RESTART
     AFTER_STAGNATION 100
     MAX 3
     METHOD LHS 4
     SEED 456
   ENDRESTART

Keys
----

.. list-table::
   :header-rows: 1
   :widths: 29 71

   * - Key
     - Values and effect
   * - ``AFTER_STAGNATION N``
     - Integer >= 1, required. Trigger after ``N`` evaluations without a
       better loss within the active window.
   * - ``MAX N``
     - Integer >= 1, required. Maximum number of additional policy
       launches.
   * - ``METHOD ...``
     - Repeatable starting-point methods, with the same syntax as
       :doc:`nfp_initialisation`.
   * - ``K_TOTAL K``
     - Integer >= 1 with the same meaning as in ``INIT``: the number of
       points of each restart, at most the points the chain gives.
   * - ``SEED n``
     - Unsigned 64-bit seed for the restart-point chain. Restart round
       ``r`` draws with ``n + r`` (unsigned 64-bit arithmetic).
   * - ``WARM_ON_MISMATCH policy``
     - ``warn`` (default), ``error`` or ``skip``; applies to this block's
       ``WARM`` entries.

A ``RESTART`` block without ``AFTER_STAGNATION`` or ``MAX`` is refused,
as is a value of 0: such a block would never restart the search. An
empty method chain uses ``CONFIG``. The initial launch
uses ``INIT``; the restart chain is used only after stagnation.

.. _what-restarts:

Restart scope
-------------

A restart relaunches the whole policy: a ``PLAIN`` search, the
``MULTISTART`` batch, or both ``PRESTAGE`` phases. Separate starts and
refinement seeds have separate stagnation windows. A stagnating
exploration hands its candidates to refinement before the enclosing
policy can restart.

Each relaunch gets fresh search budgets. Reaching an optimiser's own
stop normally ends the policy. Restarts are triggered by stagnation.
After ``MAX`` restarts, the next stagnation ends the run.

A chain that produces no point reuses the previous launch's starting
points and emits a warning. An exploration with its own ``INIT`` chain
keeps that chain; one sharing the main points receives the refreshed
points. Seeded optimiser streams also change between launches, and the
journal records the derived seeds. Without ``RESTART SEED``, a base seed
is drawn once and recorded when the first restart occurs.

Active restarts are refused in multi-rank HPO runs. Use a single rank for
this policy. See :doc:`nfp_stopping_settings` for search budgets and
:doc:`nfp_initialisation` for starting a new run from a saved potential.
