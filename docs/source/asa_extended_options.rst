ASA Dimension, Cache and Batch PARAM Keys
=========================================

This page lists the Adaptive Simulated Annealing (ASA) ``PARAM`` keys that
set values per dimension, track several minima, reuse evaluations and
control the batches of generated states. It builds on :doc:`ASA <asa>`
and the scalar keys of :doc:`asa_options`.
All values must be finite. **Indexed** keys accept ``Name[i]`` for
dimension ``i`` (1-based), or plain ``Name`` to fill every dimension.
The index follows the complete dimension order in ``hpo_search_space.tsv``.
Each entry gives the default first.

Temperature and probe scales
----------------------------

``User_Quench_Cost_Scale``
   Default ``1``. Positive scalar cost-temperature quench exponent.

``User_Quench_Param_Scale``
   Default ``1``. Positive; **indexed**. Parameter-temperature quench
   exponent.

``User_Temperature_Ratio``
   Default ``1``. Positive; **indexed**. Multiplier of the negative
   logarithm of ``Temperature_Ratio_Scale`` for each dimension.

``User_Delta_Parameter``
   Default ``Delta_X``. Positive when explicitly set; **indexed**.
   Overrides the dimension's tangent step, subject to
   :ref:`asa_probes_box`.

``User_Parameter_Temperature``
   Default ``Initial_Parameter_Temperature``. Positive; **indexed**.
   Initial temperature of each dimension.

``Delta_X 0`` disables the default tangent steps; an explicit
``User_Delta_Parameter`` must be positive.

Multiple minima and cache
-------------------------

``Multi_Number``
   Default ``1``. Integer ≥ 1. Number of distinct minima to track.

``Multi_Specify``
   Default ``1``. ``0`` or ``1``. ``1`` requires a strictly lower cost for
   a new minimum; ``0`` also permits ties.

``Multi_Grid``
   Default ``0``. Nonnegative; **indexed**. Distinctness scale for tracked
   minima; zero uses ASA's ``EPS_DOUBLE`` threshold.

``Queue_Size``
   Default ``0``. Integer ≥ 0. FIFO cost-cache capacity; ``0`` disables the
   cache.

``Queue_Resolution``
   Default ``0``. Nonnegative; **indexed**. Coordinate tolerance for a
   cache hit. With every resolution zero, only near-exact revisits are
   reused.

Batches of generated states
---------------------------

``Gener_Block``
   Default ``1``. Integer ≥ 1. Initial number of generated states per
   batch.

``Gener_Block_Max``
   Default ``Gener_Block``. Integer at least ``Gener_Block``. Upper bound
   on the adaptive batch size; buffers are allocated for this cap.

``Gener_Mov_Avr``
   Default ``3``. Integer ≥ 0. Moving-average window for adaptive
   resizing. ``0`` keeps every batch at ``Gener_Block`` states.

With adaptive resizing, the batch size remains between 1 and
``Gener_Block_Max`` and may drop below ``Gener_Block``. Batches larger
than one change the sampled sequence. ``THREADS`` bounds concurrent
workers. The batch size is set by ``Gener_Block``, ``Gener_Block_Max`` and
``Gener_Mov_Avr``.

.. _unavailable-options:

Interface scope
---------------

The entries on this page and :doc:`asa_options` cover all 32 ``PARAM``
keys that ``tadah hpo`` takes for ASA. ``tadah hpo`` leaves out these
features of ASA: the coarse resolution of generated values
(``Coarse_Resolution``), a given initial cost temperature
(``User_Cost_Temperature``), ASA's recursive optimisation of its own
options and the asymptotic form of the acceptance test. It also leaves
out the local fit and the fuzzy tuning of ASA's user module, importance
sampling and the saving of ASA's state.

A ``PARAM`` key of one of these features is refused at startup with its
reason. So are two further ``PARAM`` keys that ``tadah hpo`` sets itself:
``User_Initial_Parameters`` (see :doc:`asa` for where ASA starts) and
``Include_Integer_Parameters`` (every search parameter takes real
values).
