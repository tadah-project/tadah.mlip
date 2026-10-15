ASA stopping and temperature options
====================================

These are ``PARAM`` options for :doc:`ASA <asa>`. Names are
case-sensitive. Each option takes one finite value; rows marked
integer require an integral value.

Stopping and sampling
---------------------

.. list-table:: Limits and periodic checks
   :header-rows: 1
   :widths: 34 15 51

   * - Parameter
     - Default
     - Values and meaning
   * - ``Limit_Acceptances``
     - ``10000``
     - Integer ≥ 0. Maximum accepted states; ``0`` removes this limit.
   * - ``Limit_Generated``
     - ``99999``
     - Integer ≥ 0. Maximum generated states; ``0`` removes this limit.
   * - ``Limit_Invalid_Generated_States``
     - ``1000``
     - Integer ≥ -1. Invalid-state budget; ``0`` stops at the first
       invalid state, ``-1`` disables the check.
   * - ``Accepted_To_Generated_Ratio``
     - ``1e-6``
     - Positive. Ratio threshold triggering the periodic
       reannealing/termination checks.
   * - ``Cost_Precision``
     - ``1e-18``
     - Positive. ``abs(last_cost - best_cost)`` threshold in the
       repeated-cost plateau test.
   * - ``Maximum_Cost_Repeat``
     - ``5``
     - Integer ≥ 0. Plateau test cycles before termination;
       ``0`` disables the plateau stop.
   * - ``Number_Cost_Samples``
     - ``5``
     - Integer other than ``0`` or ``-1``. Positive values estimate
       initial cost temperature from mean absolute cost; values below
       ``-1`` select deviation sampling.
   * - ``Acceptance_Frequency_Modulus``
     - ``100``
     - Integer ≥ 0. Periodic-test cadence in accepted states;
       ``0`` disables this cadence.
   * - ``Generated_Frequency_Modulus``
     - ``10000``
     - Integer ≥ 0. Periodic-test cadence in generated states;
       ``0`` disables this cadence.

Temperature schedule
--------------------

.. list-table:: Scalar temperature settings
   :header-rows: 1
   :widths: 34 15 51

   * - Parameter
     - Default
     - Values and meaning
   * - ``Temperature_Ratio_Scale``
     - ``1e-5``
     - Positive. Sets the annealing-rate factor through its negative
       logarithm.
   * - ``Temperature_Anneal_Scale``
     - ``100``
     - Positive. Schedule scale used in the temperature exponent.
   * - ``Cost_Parameter_Scale_Ratio``
     - ``1``
     - Positive. Cost-temperature schedule scale relative to the
       parameter schedule.
   * - ``Initial_Parameter_Temperature``
     - ``1``
     - Positive. Initial parameter temperature, unless overridden
       per dimension.
   * - ``Reanneal_Cost``
     - ``1``
     - Integer other than ``-1``. ``0`` disables cost reannealing;
       ``1`` rescales from best/last costs; values above ``1`` or
       below ``-1`` select deviation-sample variants.
   * - ``Reanneal_Parameters``
     - ``1``
     - ``0`` or ``1``. Enables parameter-temperature rescaling from
       finite-difference sensitivities.

Starting point and probes
-------------------------

.. list-table:: State generation and sensitivity settings
   :header-rows: 1
   :widths: 34 15 51

   * - Parameter
     - Default
     - Values and meaning
   * - ``User_Initial_Parameters``
     - Automatic
     - ``1`` with a resolved ``INIT`` point; ``0`` without one.
       An explicit value contradicting the available seed is refused.
   * - ``Sequential_Parameters``
     - ``-1``
     - Integer less than the search dimension. Negative values disable
       sequential generation; a nonnegative value selects its initial
       zero-based dimension.
   * - ``Include_Integer_Parameters``
     - ``0``
     - ``0`` or ``1``. Includes integer-typed dimensions in
       derivative/reannealing calculations when enabled.
   * - ``Delta_X``
     - ``0.001``
     - Nonnegative tangent step, subject to :ref:`asa_probes_box`.
       ``0`` disables tangent probes unless their per-dimension
       values are overridden.
   * - ``User_Tangents``
     - ``0``
     - Only ``0`` is supported; HPO supplies no analytical ASA tangents.
   * - ``Curvature_0``
     - ``1``
     - ``1`` skips curvature; ``0`` computes it at exit; ``-1`` also
       computes it during reannealing. Curvature adds evaluations and
       imposes stricter probe-size bounds.

Per-dimension temperature overrides, multiple minima, caching and
block evaluation are listed on :doc:`asa_extended_options`.
