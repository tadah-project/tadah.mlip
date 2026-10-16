.. _tadah_anneal_pt_block:

Parallel tempering: ANNEAL_PT
=============================

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

Write algorithm-specific controls as ``PARAM KEY value``. Use
:doc:`nfp_optimizer_settings` for ``THREADS`` and ``SEED``,
:doc:`nfp_initialisation` for the seed point, and
:doc:`nfp_stopping_settings` for evaluation and time budgets.

Work and exchanges
------------------

An epoch advances every replica by one whole-vector proposal (CLASSIC)
or one sweep over the dimensions (Corana). Every ``SWAP_EVERY`` epochs,
adjacent pairs attempt exchanges, alternating even and odd pairings.
An exchange moves the parameter vector and its loss; the temperature
slot retains its proposal-step state. Exchange attempts need no new
loss evaluation.

``REPLICAS`` controls the number of temperature slots. ``THREADS``
controls local concurrency and defaults to ``1``. If ``REPLICAS`` is
``AUTO``, the count is ``max(THREADS,2)``; set it explicitly when changing
worker count should preserve the ladder size. MPI builds can distribute
replicas over ranks; their HPO driver accepts this backend with
``TYPE PLAIN`` and no active ``RESTART``.

Stopping
--------

Parallel tempering runs until a supported stopping condition is met.
It accepts ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` and positive
``FTOL_REL``/``FTOL_ABS``. An active stagnation restart can supply a stop
in a single-rank run.

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

This backend rejects serial cooling controls such as ``RT``,
``T_INIT`` and ``T_MIN``. It also rejects common ``STEP``, ``XTOL_*``,
``POPULATION``, ``VECTOR_ARR`` and ``GRAD_STRATEGY``.
