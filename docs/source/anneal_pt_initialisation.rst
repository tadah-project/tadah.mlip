Tempering replica initialisation
================================

:doc:`nfp_initialisation` supplies the seed point to :doc:`anneal_pt`.
The coldest slot starts at that point. ``PARAM INIT_MODE`` controls how
the remaining replicas are placed.

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
     - Gaussian displacement around the seed, initially scaled by
       ``STEP_FRAC`` times the interval width; the default mode.
   * - ``PARAM INIT_MODE JITTER fraction``
     - Uniform displacement within plus/minus the specified fraction of
       each transformed interval width. The fraction is in ``(0,1]``.
   * - ``PARAM INIT_MODE JITTER AUTO p``
     - Uniform displacement using a scale measured separately for each
       coordinate from a positive relative loss tolerance ``p``.

Jitter is subject to the box's boundary rule and, by default, the ladder
scaling below. Each evaluated non-seed starting point consumes the
search budget.

.. _anneal_pt_init_jitter_ladder:

Ladder scaling
--------------

``INIT_JITTER_LADDER AUTO`` uses exponent ``gamma = 0.5``. For a slot at
temperature ``T``, its initial displacement scale is multiplied by
``(T/T_HOT)**gamma``, so colder slots start closer to the seed.

A number in ``(0,4]`` selects another exponent. ``OFF`` uses the same
unscaled jitter widths for every non-seed slot. Resolvable-coordinate
floors prevent extremely small scales from collapsing through rounding.
This setting applies to every ``JITTER`` variant and is rejected with
``INIT_MODE UNIFORM``.

Automatic jitter calibration
----------------------------

``JITTER AUTO`` probes one coordinate at a time around the seed.
The resulting scales describe the tested single-coordinate displacements.
The loss under simultaneous displacement depends on their combined effect.

Its probes are outside ``MAXEVAL`` but are real loss evaluations.
``INIT_JITTER_MAXEVAL`` caps these probes separately; its automatic cap
allows ``INIT_JITTER_PROBES`` for each measured dimension plus the
requested audit probes, up to the number of measured dimensions.
``INIT_JITTER_SAMPLE`` can reduce the measured set. With an optimiser
``MAXTIME``, ``INIT_JITTER_MAXTIME_FRAC`` also bounds calibration time.
Total startup cost includes the search and jitter-calibration budgets.

The ``INIT_JITTER_*`` calibration controls require ``JITTER AUTO``;
``INIT_JITTER_LADDER`` instead applies to all jitter modes. Resolved
scales and probe counts are reported in ``hpo_init_jitter.tsv`` and the
run journal.

Parameter reference
-------------------

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Replica initialisation
   :end-before: .. rubric:: Stopping
