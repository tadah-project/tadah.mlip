.. _tadah_anneal_pa_block:

Population annealing
====================

This page shows how a population-annealing search is set up, budgeted and
stopped. It needs a target file with a search space and an ``OPTIMIZER``
block (:doc:`nfp_configuration`, :doc:`nfp_optimizer`).

``LIB TADAH`` with ``ALGO ANNEAL_PA`` cools a population of walkers from
``T_HOT`` to ``T_MIN``. After an initial sampling phase at ``T_HOT``,
each temperature change reweights and resamples the current population.
Walkers with lower loss receive more descendants. Each resulting walker
then runs ``SWEEPS`` temperature rungs at the new temperature.

``SWEEPS`` counts temperature rungs. One rung costs ``CHAIN_LENGTH``
proposals with the default kernel, or ``NT * NS * dimensions`` proposals
with ``CORANA ON``. Budget limits can truncate this work. These units are
defined in :doc:`nfp_annealing_terms`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Cooling and population size
---------------------------

:hpo:key:`POPULATION <OPTIMIZER.POPULATION>` is a key of the
``OPTIMIZER`` block, written without ``PARAM``. Its default depends on
``THREADS``. An explicit value keeps the number of walkers fixed when runs
with different thread counts are compared.

:hpo:param:`ESS_TARGET` selects an adaptive temperature step using the
population's effective sample size; ``OFF`` uses geometric cooling.
:hpo:param:`RT` limits the cooling of each step. A population with insufficient variation for the
adaptive calculation also uses the geometric step.

Resampling uses weights proportional to
``exp(-delta_beta * loss)``. Unusable walkers receive zero weight; if
none has a usable loss, resampling is skipped and cooling continues.
Children inherit the parent's point, loss and proposal-step vector.
Random streams belong to population slots, so descendants placed in
different slots obtain different proposal sequences.

Example
-------

This ``OPTIMIZER`` block needs top-level :hpo:key:`OPTIM <ROOT.OPTIM>`
lines and target lines that define the search space and the loss:

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

Stopping
--------

:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` counts the evaluation of the
starting point, the temperature-calibration probes, population
initialisation and production proposals. The search also stops at its
lowest temperature or on the stopping keys of the ``OPTIMIZER`` block
(:doc:`nfp_stopping_settings`).

A walker that reaches :hpo:key:`STOPVAL <OPTIMIZER.STOPVAL>` ends its own
sweeps, and the search ends after the starting points or the temperature
step in which it is reached. The other walkers complete their evaluations
of those starting points or of that temperature step, so where the search
ends does not depend on the timing of the
workers. The evaluations that the walker did not make have no row
(:ref:`nfp_evaluation_numbers`).

.. versionchanged:: 1.6.0
   In 1.5.0 the other walkers stopped at their next proposal, at a point
   that depended on the order of the walkers and the timing of the
   workers.

Parameter reference
-------------------

The keys below are written as ``PARAM <key> <value>``. Keys of the
``OPTIMIZER`` block such as ``POPULATION``, ``THREADS`` and ``MAXEVAL`` are
documented separately in
:doc:`nfp_optimizer_settings` and :doc:`nfp_stopping_settings`.

.. note::

   ``TADAH:ANNEAL_PA`` refuses the ``INIT_JITTER_*`` keys of the
   ``JITTER AUTO`` calibration of :doc:`anneal_pt`.

.. include:: config/anneal_keys_pa.rst
