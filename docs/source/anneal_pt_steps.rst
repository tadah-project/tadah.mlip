Tempering proposal steps
========================

These ``PARAM`` settings control how each replica of :doc:`anneal_pt`
proposes new coordinates. The steps are measured in transformed search
coordinates; see :doc:`nfp_search_space`.

CLASSIC step modes
------------------

``CORANA OFF`` uses whole-vector Gaussian proposals:

* ``STEP_MODE FIXED`` uses ``STEP_FRAC`` times each interval width.
* ``STEP_MODE FROZEN`` (default) calibrates per-slot scales and then
  keeps them fixed.
* ``STEP_MODE ADAPTIVE`` calibrates scales and continues adapting them
  during the search.

Calibration uses coordinate probes followed by a whole-vector pilot.
It consumes ``MAXEVAL``. When the hot temperature is also automatic,
a second temperature-calibration pass uses the calibrated production
kernel. An explicit ``T_HOT`` or ``STEP_MODE FIXED`` skips that second
pass.

An explicit ``STEP_WARMUP_SWEEPS`` is rejected with ``FIXED`` because
that mode performs no warm-up. ``STEP_TRUST_FACTOR`` is accepted only
with ``ADAPTIVE``. Set a number
greater than ``1`` to bound scales around their calibrated values, or
``OFF`` to disable that clamp. With a moving ladder, frozen proposal
scales remain those calibrated for the earlier slot temperatures; the
driver warns about this combination.

Corana steps
------------

``CORANA ON`` moves one coordinate at a time. Each slot adapts its step
widths after ``NS`` sweeps, using its own acceptance counts. CLASSIC
``STEP_MODE``, ``STEP_WARMUP_SWEEPS`` and ``STEP_TRUST_FACTOR`` settings
are rejected in Corana mode.

Both kernels support ``BOUNDARY_MODE REFLECT`` and ``REDRAW``.
``RESET_TO_BEST ON`` returns only the coldest slot to the global best
after an epoch; its default is ``OFF``.

Parameter reference
-------------------

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Proposal steps
   :end-before: .. rubric:: Replica initialisation
