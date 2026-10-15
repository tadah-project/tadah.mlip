.. _lib-tadah-algo-anneal-classic-simulated-annealing:

.. _corana-adaptive-variant-opt-in:

.. _tadah_anneal_block:

Simulated annealing: ANNEAL
===========================

``LIB TADAH`` with ``ALGO ANNEAL`` runs one derivative-free search
chain. It proposes points inside the transformed ``OPTIM`` bounds and
uses a Metropolis acceptance test: improvements are accepted, while
uphill moves become less likely as the temperature falls.

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB TADAH
     ALGO ANNEAL
     MAXEVAL 1000
     SEED 123
     PARAM STEP_FRAC 0.1
   ENDSTRATEGY

Proposal kernels
----------------

The default CLASSIC kernel proposes all coordinates together with
Gaussian displacements. ``PARAM STEP_FRAC`` gives the initial standard
deviation as a fraction of each transformed interval's width.
``BOUNDARY_MODE REFLECT`` folds out-of-bounds coordinates back into the
box; ``REDRAW`` resamples them. These are different proposal rules, so
changing the boundary mode can change the search result.

.. _tadah_anneal_corana:

Corana kernel
-------------

With ``PARAM CORANA ON``, proposals move one coordinate at a time with
uniform displacements. Each coordinate's step width adapts from its
acceptance rate after ``NS`` sweeps. A sweep visits every dimension;
a temperature rung contains ``NT`` such adjustment cycles. Use ``NT``
instead of CLASSIC's ``CHAIN_LENGTH``.

Temperature and stopping
------------------------

``T_INIT`` sets the starting temperature. Otherwise, the driver
calibrates it with local loss probes; these probes count against
``MAXEVAL``. ``T0_PROBES`` must be at least ``2`` when calibration is
used. Explicit ``T_INIT`` makes ``T0_PROBES`` and ``T0_ACCEPT`` inactive,
so supplying them together is rejected.

The temperature is multiplied by ``RT`` after each rung. ``RT`` must be
in ``(0,1)``. ``T_MIN`` is positive and cannot exceed the resolved
starting temperature. Default ``CHAIN_LENGTH AUTO`` (CLASSIC) or
``NT AUTO`` (Corana) divides the remaining evaluation budget across the
schedule and therefore requires ``MAXEVAL``. Without that budget,
supply the appropriate length explicitly.

``REANNEAL ON`` can restart a completed cooling schedule from the best
point at a reduced starting temperature while budget remains. This is
part of the same optimiser search; it differs from the enclosing
:doc:`nfp_restart` policy, which relaunches a whole run policy.

Supported stops are ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` and positive
``FTOL_REL``/``FTOL_ABS`` plateau tolerances. The schedule also ends
when cooling and any permitted re-annealing are exhausted. The plateau
test compares rung-end losses over ``NEPS`` entries and against the best
loss. See :doc:`nfp_stopping_settings` for common budget semantics.

.. _anneal_keys:

Parameter reference
-------------------

Write each key below as ``PARAM KEY value`` in the optimiser block.
The tables are generated from the HPO parameter registry. Conditional
ranges described above are checked in addition to the table ranges.

.. include:: config/anneal_keys_serial.rst

Common settings, including ``SEED`` and ``THREADS``, are in
:doc:`nfp_optimizer_settings`. ``THREADS`` sizes evaluation work; the
search chain itself is serial. ``STEP``, ``XTOL_*``, ``POPULATION``,
``VECTOR_ARR`` and ``GRAD_STRATEGY`` are not accepted by this backend.
