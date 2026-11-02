Tempering replica initialisation
================================

This page describes how ``PARAM INIT_MODE`` places the replicas of
:doc:`anneal_pt` before the search starts. :doc:`nfp_initialisation`
describes the starting point of the search. The coldest replica starts at
that point. ``PARAM INIT_MODE`` controls how the remaining replicas are
placed.

Initialisation modes
--------------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Setting
     - Other replicas
   * - ``PARAM INIT_MODE UNIFORM``
     - Uniform draws over the transformed search box.
   * - ``PARAM INIT_MODE JITTER``
     - Gaussian displacement around the starting point, initially scaled by
       ``STEP_FRAC`` times the interval width; the default mode.
   * - ``PARAM INIT_MODE JITTER fraction``
     - Uniform displacement within plus/minus the specified fraction of
       each transformed interval width. The fraction is in ``(0,1]``.
   * - ``PARAM INIT_MODE JITTER AUTO p``
     - Uniform displacement using a scale measured separately for each
       coordinate from a positive relative loss tolerance ``p``.

Jitter is subject to the box's boundary rule and, by default, the ladder
scaling below. Each evaluated starting point other than the first
consumes the search budget.

.. _anneal_pt_init_jitter_ladder:

Ladder scaling
--------------

``INIT_JITTER_LADDER AUTO`` uses exponent ``gamma = 0.5``. For a replica
at temperature ``T``, its initial displacement scale is multiplied by
``(T/T_HOT)**gamma``, so colder replicas start closer to the starting
point.

A number in ``(0,4]`` selects another exponent. ``OFF`` uses the same
unscaled jitter widths for every replica but the first. Resolvable-coordinate
floors prevent extremely small scales from collapsing through rounding.
This setting applies to every ``JITTER`` variant and is rejected with
``INIT_MODE UNIFORM``.

Automatic jitter calibration
----------------------------

``JITTER AUTO`` probes one coordinate at a time around the starting point.
The resulting scales describe the tested single-coordinate displacements.
The loss under simultaneous displacement depends on their combined effect.

Its probes are outside ``MAXEVAL`` but are real loss evaluations.
``INIT_JITTER_MAXEVAL`` caps these probes separately. Its automatic cap
is ``INIT_JITTER_PROBES`` for each measured dimension, plus one probe
for each of up to ``INIT_JITTER_AUDIT`` measured dimensions that are
probed again at half their displacement.
``INIT_JITTER_SAMPLE`` can reduce the measured set. With an optimiser
``MAXTIME``, ``INIT_JITTER_MAXTIME_FRAC`` also bounds calibration time.
The evaluations of a search therefore comprise the ``MAXEVAL`` budget
and the jitter-calibration probes.

The ``INIT_JITTER_*`` calibration controls require ``JITTER AUTO``;
``INIT_JITTER_LADDER`` instead applies to all jitter modes. Resolved
scales and probe counts are reported in ``hpo_init_jitter.tsv`` and the
run journal.

Parameter reference
-------------------

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Replica initialisation
   :end-before: .. rubric:: Stopping
