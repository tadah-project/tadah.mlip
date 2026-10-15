.. _performance_constraints:

.. _hpo_validation_follows_metrics:

.. _energy-force-stress-validation-metrics:

.. _individual-validation-targets-and-weights:

.. _pc-performance-constraints:

.. _per-metric-bad-pot-pre-filter:

.. _validation-set-sanitisation:

Validation targets
==================

Validation targets compare predictions with the datasets listed by
``DBFILE`` in the file passed through ``--validation``. Declare them at
the root of the :doc:`target file <nfp_configuration>`.

.. code-block:: none

   LOSS L2
   PC_ERMSE 0 1
   PC_FRMSE 0 10

A validation file is required whenever any validation metric is enabled.
``FORCE`` and ``STRESS`` in the training configuration control training;
validation computes forces for ``PC_FRMSE`` and stresses for
``PC_SRMSE`` independently. Those two training switches are refused in
the validation file.

Metric keys
-----------

Syntax: ``PC_<METRIC> <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]``.
Targets must be finite; weights must be finite and nonnegative.

.. list-table:: Metrics and units
   :header-rows: 1
   :widths: 22 54 24

   * - Key
     - Measured quantity
     - Unit
   * - ``PC_EMAE``
     - Mean absolute energy error per atom
     - meV/atom
   * - ``PC_ERMSE``
     - Root-mean-square energy error per atom
     - meV/atom
   * - ``PC_ErRMSE``
     - Relative energy RMSE, multiplied by 1000
     - Per mille
   * - ``PC_ERSQ``
     - Energy coefficient of determination, :math:`R^2`
     - Dimensionless
   * - ``PC_FRMSE``
     - Force-component RMSE
     - eV/Å
   * - ``PC_SRMSE``
     - Stress-component RMSE
     - eV/Å³

Keep the spelling ``PC_ErRMSE``: root keywords are case-sensitive.
For the global form, HPO averages the metric across datasets and then
applies the root :doc:`LOSS <nfp_losses>`, target and weight.
Dataset sizes do not weight that average.

Dataset-specific targets
------------------------

Only ``PC_ERMSE`` and ``PC_FRMSE`` support indexed declarations:

.. code-block:: none

   PC_ERMSE (1,2) 5.0 1.0
   PC_ERMSE (3)  10.0 0.5
   PC_FRMSE (1-3) 0.05 2.0

Indices start at 1 and follow ``DBFILE`` order in the validation file.
Lists, ranges and strides are supported: ``(1,4,7-10:2)``. For each
metric, use either one global declaration or disjoint indexed
declarations covering every validation dataset. Mixing these forms,
overlapping assignments and missing assignments are errors.

For measured dataset metrics :math:`x_i`, targets :math:`t_i`, weights
:math:`w_i` and :math:`N` datasets, the indexed contribution is

.. math::

   C = \frac{1}{N}\sum_i w_i\,\ell(x_i,t_i).

It still counts as one term in the total loss. A zero weight contributes
zero but retains that dataset in the denominator. Weights are not
normalised. With nonlinear losses, taking the mean of dataset losses
differs from taking the loss of the mean metric.

Missing reference data
----------------------

A structure with force weight zero is excluded from the force metric;
one with stress weight zero is excluded from the stress metric. A
dataset with no such reference data is unmeasured for that metric and
has ``nan`` in its metric columns.

Global force and stress metrics average only the measured datasets.
For indexed ``PC_FRMSE``, an unmeasured dataset needs an explicit zero
weight: it contributes no loss, but the indexed average still divides
by the total number of validation datasets. Startup rejects a metric
with no measured dataset, or a positive indexed force weight assigned
to an unmeasured dataset. Empty validation datasets are also rejected.

Early-rejection options
-----------------------

.. list-table:: Optional flags on any metric declaration
   :header-rows: 1
   :widths: 30 70

   * - Flag
     - Effect
   * - ``--skip-above <cap>``
     - Reject when the metric exceeds this finite positive value.
       Use the metric's units from the table above.
   * - ``--skip-rel <ratio>``
     - Reject when the metric exceeds this finite positive multiple
       of its best previously accepted value. No relative comparison
       is made before such a value exists.

These checks happen after validation and before the physical-property
targets. In the indexed form they apply separately to the selected
datasets; in the global form they apply to the aggregate metric.
When both flags are present, either can reject the candidate. Rejected
candidates receive a failure score. Each flag can appear only once on
a declaration.
