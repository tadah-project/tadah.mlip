Tempering temperatures and exchanges
====================================

These ``PARAM`` settings configure the temperature ladder and exchange
cadence of :doc:`anneal_pt`.

Temperature endpoints
---------------------

``T_HOT`` is positive. Omit it to calibrate the hot endpoint from loss
probes; ``T0_PROBES`` must then be at least ``2``. If ``T_HOT`` is
explicit, the calibration settings ``T0_PROBES`` and ``T0_ACCEPT``
are inactive and rejected.

``T_COLD`` is positive and must be below the resolved hot endpoint. If
omitted, it is derived from ``T_HOT / LADDER_SPAN``. ``T_COLD`` and
``LADDER_SPAN`` are mutually exclusive.

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Temperature
   :end-before: .. rubric:: Ladder

Ladder size and adaptation
--------------------------

At least two replicas are required; ``LADDER_ADAPT ON`` requires at
least three. ``LADDER_POWER 1`` gives a geometric ladder. Values below
one place more slots towards the hot end; values above one place more
towards the cold end.

Adaptation changes interior log-temperature gaps while preserving the
endpoints. It uses only nondegenerate exchange attempts as adaptation
evidence. ``LADDER_FREEZE`` can
stop further adaptation after the acceptance rates have equalised
within its statistical test.

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Ladder
   :end-before: .. rubric:: Exchange and diagnostics cadence

Exchange cadence
----------------

``SWAP_EVERY`` sets the epochs between exchange rounds.
``SWAP_WINDOW`` controls periodic diagnostics and the tolerance-stop
window. Its ``OFF`` value suppresses periodic diagnostic windows;
exchanges and the tolerance test remain active.

.. include:: config/anneal_keys_pt.rst
   :start-after: .. rubric:: Exchange and diagnostics cadence
   :end-before: .. rubric:: Proposal steps
