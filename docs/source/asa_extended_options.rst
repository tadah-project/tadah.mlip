ASA dimension, cache and block options
======================================

These ``PARAM`` options extend the :doc:`ASA <asa>` scalar settings.
All values must be finite. **Indexed** options accept ``Name[i]`` for
dimension ``i`` (1-based), or plain ``Name`` to fill every dimension.
The index follows the complete dimension order in ``hpo_search_space.tsv``.

Temperature and probe scales
----------------------------

.. list-table:: Schedule overrides
   :header-rows: 1
   :widths: 34 22 44

   * - Parameter
     - Default
     - Values and meaning
   * - ``User_Quench_Cost_Scale``
     - ``1``
     - Positive scalar cost-temperature quench exponent.
   * - ``User_Quench_Param_Scale``
     - ``1``
     - Positive; **indexed**. Parameter-temperature quench exponent.
   * - ``User_Temperature_Ratio``
     - ``1``
     - Positive; **indexed**. Multiplier of the negative logarithm of
       ``Temperature_Ratio_Scale`` for each dimension.
   * - ``User_Delta_Parameter``
     - ``Delta_X``
     - Positive when explicitly set; **indexed**. Overrides the
       dimension's tangent step, subject to :ref:`asa_probes_box`.
   * - ``User_Parameter_Temperature``
     - ``Initial_Parameter_Temperature``
     - Positive; **indexed**. Initial temperature of each dimension.

Use ``Delta_X 0`` to disable default tangent steps; an explicit
``User_Delta_Parameter`` must be positive.

Multiple minima and cache
-------------------------

.. list-table:: Minima tracking and evaluation reuse
   :header-rows: 1
   :widths: 34 15 51

   * - Parameter
     - Default
     - Values and meaning
   * - ``Multi_Number``
     - ``1``
     - Integer ≥ 1. Number of distinct minima to track.
   * - ``Multi_Specify``
     - ``1``
     - ``0`` or ``1``. ``1`` requires a strictly lower cost for a new
       minimum; ``0`` also permits ties.
   * - ``Multi_Grid``
     - ``0``
     - Nonnegative; **indexed**. Distinctness scale for tracked
       minima; zero uses ASA's ``EPS_DOUBLE`` threshold.
   * - ``Queue_Size``
     - ``0``
     - Integer ≥ 0. FIFO cost-cache capacity; ``0`` disables the cache.
   * - ``Queue_Resolution``
     - ``0``
     - Nonnegative; **indexed**. Coordinate tolerance for a cache hit.
       With every resolution zero, only near-exact revisits are reused.

Generated-state blocks
----------------------

.. list-table:: Concurrent evaluation controls
   :header-rows: 1
   :widths: 34 20 46

   * - Parameter
     - Default
     - Values and meaning
   * - ``Gener_Block``
     - ``1``
     - Integer ≥ 1. Initial number of generated states per block.
   * - ``Gener_Block_Max``
     - ``Gener_Block``
     - Integer at least ``Gener_Block``. Upper bound on adaptive
       block size; buffers are allocated for this cap.
   * - ``Gener_Mov_Avr``
     - ``3``
     - Integer ≥ 0. Moving-average window for adaptive resizing.
       ``0`` keeps every block at ``Gener_Block`` states.

With adaptive resizing, block size remains between 1 and
``Gener_Block_Max`` and may drop below ``Gener_Block``. Blocks larger
than one change the sampled sequence. ``THREADS`` bounds concurrent
workers. Generated-state block size is set by the block parameters.

.. _unavailable-options:

Interface scope
---------------

The tables on this page and :doc:`asa_options` cover all 32 accepted
``PARAM`` names exposed through HPO. ``Coarse_Resolution``,
``User_Cost_Temperature``, recursive ASA, local-fit options, importance
sampling and ASA checkpointing options are excluded. Their named
parameters produce a startup error. So do two options ``tadah hpo`` sets
itself: ``User_Initial_Parameters`` (see :doc:`asa` for where ASA starts)
and ``Include_Integer_Parameters`` (every search parameter takes real
values).
