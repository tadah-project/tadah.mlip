.. _tadah_anneal_pa_block:

Population annealing
====================

``LIB TADAH`` with ``ALGO ANNEAL_PA`` cools a population of walkers from
``T_HOT`` to ``T_MIN``. After an initial sampling phase at ``T_HOT``,
each temperature change reweights and resamples the current population.
Walkers with lower loss receive more descendants. Each resulting walker
then runs ``SWEEPS`` temperature rungs at the new temperature.

``SWEEPS`` counts temperature rungs. One rung costs ``CHAIN_LENGTH`` proposals with the default kernel, or
``NT * NS * dimensions`` proposals with ``CORANA ON``. Budget limits can
truncate this work. See :doc:`nfp_annealing_terms` for these units.

Cooling and population size
---------------------------

Set ``POPULATION`` as an optimiser attribute, without ``PARAM``.
Its default is ``max(4 * THREADS, 16)``. Set it explicitly when comparing
different thread counts, so that the number of walkers stays fixed.

``PARAM ESS_TARGET`` selects an adaptive temperature step using the
population's effective sample size. Its default is ``0.5``; ``OFF`` uses
geometric cooling. ``PARAM RT`` limits cooling to a factor of ``0.90``
per step by default. A population with insufficient variation for the
adaptive calculation also uses the geometric step.

Resampling uses weights proportional to
``exp(-delta_beta * loss)``. Unusable walkers receive zero weight; if
none has a usable loss, resampling is skipped and cooling continues.
Children inherit the parent's point, loss and proposal-step vector.
Random streams belong to population slots, so descendants placed in
different slots obtain different proposal sequences.

Example
-------

This optimiser block requires root ``OPTIM`` and target directives
to define the search space and loss:

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB TADAH
       ALGO ANNEAL_PA
       MAXEVAL 20000
       THREADS 4
       POPULATION 16
       SEED 1234
       PARAM CHAIN_LENGTH 20
       PARAM SWEEPS 1
       PARAM ESS_TARGET 0.5
     ENDSTRATEGY
   ENDOPTIMIZER

``MAXEVAL`` includes the seed, temperature-calibration probes,
population initialisation and production proposals. The run can also
stop on its temperature floor or configured stopping criteria.

Parameter reference
-------------------

Write the keys below as ``PARAM <key> <value>``. The table is generated
from the parser's registry. Optimiser attributes such as ``POPULATION``,
``THREADS`` and ``MAXEVAL`` are documented separately in
:doc:`nfp_optimizer_settings` and :doc:`nfp_stopping_settings`.

.. note::

   ``TADAH:ANNEAL_PA`` refuses the ``INIT_JITTER_*`` keys of the
   ``JITTER AUTO`` calibration of :doc:`anneal_pt`.

.. include:: config/anneal_keys_pa.rst
