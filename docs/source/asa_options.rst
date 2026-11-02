ASA Stopping and Temperature PARAM Keys
=======================================

These are ``PARAM`` keys for :doc:`Adaptive Simulated Annealing (ASA)
<asa>`. Names are case-sensitive. Each key takes one finite value; keys
marked integer require an integral value. Each entry gives the default
first.

Stopping and sampling
---------------------

``Limit_Acceptances``
   Default ``10000``. Integer ≥ 0. Maximum accepted states; ``0`` removes
   this limit.

``Limit_Generated``
   Default ``99999``. Integer ≥ 0. Maximum generated states; ``0`` removes
   this limit.

``Limit_Invalid_Generated_States``
   Default ``1000``. Integer ≥ -1. Invalid-state budget; ``0`` stops at the
   first invalid state, ``-1`` disables the check.

``Accepted_To_Generated_Ratio``
   Default ``1e-6``. Positive. Ratio threshold triggering the periodic
   reannealing/termination checks.

``Cost_Precision``
   Default ``1e-18``. Positive. ``abs(last_cost - best_cost)`` threshold in
   the repeated-cost plateau test.

``Maximum_Cost_Repeat``
   Default ``5``. Integer ≥ 0. Plateau test cycles before termination;
   ``0`` disables the plateau stop.

``Number_Cost_Samples``
   Default ``5``. Integer other than ``0`` or ``-1``. Positive values
   estimate initial cost temperature from mean absolute cost; values below
   ``-1`` select deviation sampling.

``Acceptance_Frequency_Modulus``
   Default ``100``. Integer ≥ 0. A periodic test follows each state, or
   each batch of states, that brings the count of accepted states to a
   multiple of this number; ``0`` switches this off.

``Generated_Frequency_Modulus``
   Default ``10000``. Integer ≥ 0. A periodic test follows each state, or
   each batch of states, that brings the count of generated states to a
   multiple of this number; ``0`` switches this off.

Temperature schedule
--------------------

``Temperature_Ratio_Scale``
   Default ``1e-5``. Positive. Sets the annealing-rate factor through its
   negative logarithm.

``Temperature_Anneal_Scale``
   Default ``100``. Positive. Schedule scale used in the temperature
   exponent.

``Cost_Parameter_Scale_Ratio``
   Default ``1``. Positive. Cost-temperature schedule scale relative to the
   parameter schedule.

``Initial_Parameter_Temperature``
   Default ``1``. Positive. Initial parameter temperature, unless
   overridden per dimension.

``Reanneal_Cost``
   Default ``1``. Integer other than ``-1``. ``0`` disables cost
   reannealing; ``1`` rescales from best/last costs; values above ``1`` or
   below ``-1`` select deviation-sample variants.

``Reanneal_Parameters``
   Default ``1``. ``0`` or ``1``. Enables parameter-temperature rescaling
   from finite-difference sensitivities.

State generation and probes
---------------------------

``Sequential_Parameters``
   Default ``-1``. Integer less than the search dimension. Negative values
   disable sequential generation; a nonnegative value selects its initial
   zero-based dimension.

``Delta_X``
   Default ``0.001``. Nonnegative tangent step, subject to
   :ref:`asa_probes_box`. ``0`` disables tangent probes unless their
   per-dimension values are overridden.

``User_Tangents``
   Default ``0``. Only ``0`` is supported; ASA computes its tangents
   numerically.

``Curvature_0``
   Default ``1``. ``1`` skips curvature; ``0`` computes it at exit; ``-1``
   also computes it during reannealing. Curvature adds evaluations and
   imposes stricter probe-size bounds.

Per-dimension temperature overrides, multiple minima, caching and
batch evaluation are listed on :doc:`asa_extended_options`.
