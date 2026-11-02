.. _tadah_anneal_pt_block:

Parallel tempering: ANNEAL_PT
=============================

This page shows how a parallel-tempering search is set up, how its
replicas exchange states and when it stops. It needs a target file with a
search space and an ``OPTIMIZER`` block (:doc:`nfp_configuration`,
:doc:`nfp_optimizer`).

``LIB TADAH`` with ``ALGO ANNEAL_PT`` runs several derivative-free
search chains, called replicas, at different temperatures. The replicas
exchange states between neighbouring temperature slots. The ladder
endpoints stay fixed after calibration, while optional ladder adaptation
changes interior temperatures.

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB TADAH
     ALGO ANNEAL_PT
     MAXEVAL 5000
     THREADS 4
     SEED 123
     PARAM REPLICAS 8
   ENDSTRATEGY

.. _anneal_pt_reference:

Controls by purpose
-------------------

.. toctree::
   :maxdepth: 1

   anneal_pt_ladder
   anneal_pt_steps
   anneal_pt_initialisation

Algorithm-specific controls are written as ``PARAM KEY value``.
:doc:`nfp_optimizer_settings` describes ``THREADS`` and ``SEED``,
:doc:`nfp_initialisation` the starting point, and
:doc:`nfp_stopping_settings` the evaluation and time budgets.

Work and exchanges
------------------

An epoch advances every replica by one whole-vector proposal (CLASSIC)
or one sweep over the dimensions (Corana). ``PARAM CORANA`` selects the
kernel; :doc:`anneal_pt_steps` describes both. Every ``SWAP_EVERY`` epochs,
adjacent pairs attempt exchanges, alternating even and odd pairings.
An exchange moves the parameter vector and its loss; the temperature
slot retains its proposal-step state. Exchange attempts need no new
loss evaluation.

``REPLICAS`` controls the number of temperature slots. ``THREADS``
controls local concurrency and defaults to ``1``. If ``REPLICAS`` is
``AUTO``, the count is ``max(THREADS,2)``; an explicit count keeps the
ladder size the same when ``THREADS`` changes. MPI builds can distribute
replicas over ranks; there, ``tadah hpo`` accepts this optimiser with
``TYPE PLAIN`` and no ``RESTART`` block.

Stopping
--------

Parallel tempering runs until a supported stopping condition is met.
It accepts ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` and positive
``FTOL_REL``/``FTOL_ABS``. In a single-rank run, an active
:hpo:block:`RESTART` block also ends the search: once its ``MAX``
restarts are used, ``AFTER_STAGNATION`` evaluations without a better loss
stop it. This counts as the stop that the search requires.

A replica that reaches :hpo:key:`STOPVAL <OPTIMIZER.STOPVAL>` makes no
further evaluations. The search ends after the phase in which a replica
reaches it: the evaluation of the starting points, or the current epoch.
The other replicas complete their evaluations of that phase.
Where the search ends therefore does not depend on the timing of the
workers. The evaluations that the replica did not make have no row
(:ref:`nfp_evaluation_numbers`).

The step calibration (:doc:`anneal_pt_steps`) runs in rounds of one
evaluation for each replica, and the search ends after the round in which
a replica reaches :hpo:key:`!STOPVAL <OPTIMIZER.STOPVAL>`. In a run of
several MPI ranks only the rank of that replica ends its calibration
there; the other ranks first complete their calibration phase.

.. versionchanged:: 1.6.0
   In 1.5.0 the other replicas stopped at their next proposal, at a point
   that depended on the order of the replicas and the timing of the
   workers.

The tolerance test tracks the global best over ``NEPS`` consecutive
windows. It stops when improvement stays within the configured loss
tolerance. A window has ``SWAP_WINDOW`` epochs; with
``SWAP_WINDOW OFF``, the stop test still uses the default 20-epoch
window. This differs from serial annealing's rung-end plateau test.

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Stopping

Ordinary temperature calibration, step warm-up and replica-start
losses consume the search budget. ``INIT_MODE JITTER AUTO`` calibration
has its own additional budget, described in
:doc:`anneal_pt_initialisation`. A time limit allows evaluations already
in progress to finish.

``ANNEAL_PT`` refuses serial cooling controls such as ``RT``,
``T_INIT`` and ``T_MIN``. It also refuses the ``OPTIMIZER`` keys ``STEP``,
``XTOL_*``, ``POPULATION``, ``VECTOR_ARR`` and ``GRAD_STRATEGY``.
