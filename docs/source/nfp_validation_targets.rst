.. _performance_constraints:

.. _hpo_validation_follows_metrics:

.. _energy-force-stress-validation-metrics:

.. _individual-validation-targets-and-weights:

.. _pc-performance-constraints:

.. _per-metric-bad-pot-pre-filter:

.. _validation-set-sanitisation:

Validation Targets
==================

This page describes the validation metrics of a nested fitting search and
the datasets that they measure. Validation targets compare predictions
with the datasets that :ref:`DBFILE` lists in the validation configuration,
the file given with ``--validation`` (see :doc:`cli/cli`). The metrics are
top-level keys of the :doc:`target file <nfp_configuration>`.

.. code-block:: none

   LOSS L2
   PC_ERMSE 0 1
   PC_FRMSE 0 10

A validation configuration is required whenever any validation metric is
enabled. :ref:`FORCE` and :ref:`STRESS` in the training configuration
control training; validation computes forces for
:hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>` and stresses for
:hpo:key:`PC_SRMSE_GPA <ROOT.PC_SRMSE_GPA>` independently. The validation
configuration lists the validation datasets and their filters, and refuses
the keys that set a fit (see :ref:`validation`).

Metric keys
-----------

A metric line has the form:

.. code-block:: none

   PC_<METRIC> <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]

:hpo:key:`PC_ERSQ <ROOT.PC_ERSQ>` takes neither option
(`Early-rejection options`_). Targets must be finite; loss-term weights
must be finite and nonnegative. A metric has one global line: a second one
is refused, and the message names the line of the first.

.. versionchanged:: 1.6.0
   A repeated ``PC_EMAE``, ``PC_ErRMSE`` or ``PC_ERSQ`` line is refused
   (1.5.0 used the last one). ``PC_SRMSE_GPA`` replaces ``PC_SRMSE`` (six
   entries in eV/Å³), which is refused with the conversion.

:hpo:key:`PC_EMAE <ROOT.PC_EMAE>`
   Mean absolute energy error per atom, in meV/atom.

:hpo:key:`PC_ERMSE <ROOT.PC_ERMSE>`
   Root-mean-square energy error per atom, in meV/atom.

:hpo:key:`PC_ErRMSE <ROOT.PC_ErRMSE>`
   Relative energy RMSE, multiplied by 1000, in per mille.

:hpo:key:`PC_ERSQ <ROOT.PC_ERSQ>`
   Energy coefficient of determination :math:`R^2`, dimensionless.

:hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>`
   Force-component RMSE, in eV/Å.

:hpo:key:`PC_SRMSE_GPA <ROOT.PC_SRMSE_GPA>`
   Stress RMSE over the nine entries of each tensor, in GPa.

Top-level keys are case-sensitive: ``PC_ErRMSE`` keeps its spelling. For
the global form, ``tadah hpo`` takes the unweighted arithmetic mean of the
metric across datasets, then applies the top-level
:hpo:key:`LOSS <ROOT.LOSS>` function (see :doc:`nfp_losses`), the target
and the loss-term weight.

Dataset-specific targets
------------------------

Only :hpo:key:`PC_ERMSE <ROOT.PC_ERMSE>`, :hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>`
and :hpo:key:`PC_SRMSE_GPA <ROOT.PC_SRMSE_GPA>` support indexed
declarations:

.. code-block:: none

   PC_ERMSE (1,2) 5.0 1.0
   PC_ERMSE (3)  10.0 0.5
   PC_FRMSE (1-3) 0.05 2.0

Indices start at 1 and follow the :ref:`DBFILE` order in the validation
configuration. Lists, ranges and strides are supported: ``(1,4,7-10:2)``.
Each metric takes either one global declaration or disjoint indexed
declarations that cover every validation dataset. Mixing these forms,
overlapping assignments and missing assignments are errors.

For measured dataset metrics :math:`x_i`, targets :math:`t_i`, loss-term
weights :math:`w_i` and :math:`N` datasets, the indexed contribution is

.. math::

   C = \frac{1}{N}\sum_i w_i\,\ell(x_i,t_i).

It still counts as one term in the total loss. A loss-term weight of 0
contributes zero and retains that dataset in the denominator. Loss-term
weights enter the expression at their configured values. With nonlinear losses, taking
the mean of dataset losses differs from taking the loss of the mean metric.

Missing reference data
----------------------

A structure with energy :term:`priority` 0 (``eweight`` 0) is left out
of the energy metrics (:hpo:key:`PC_EMAE <ROOT.PC_EMAE>`,
:hpo:key:`PC_ERMSE <ROOT.PC_ERMSE>`,
:hpo:key:`PC_ErRMSE <ROOT.PC_ErRMSE>` and
:hpo:key:`PC_ERSQ <ROOT.PC_ERSQ>`), one with force priority 0
(``fweight`` 0) of :hpo:key:`!PC_FRMSE <ROOT.PC_FRMSE>`, and one with
stress priority 0 (``sweight`` 0) of
:hpo:key:`!PC_SRMSE_GPA <ROOT.PC_SRMSE_GPA>` (:ref:`dataset`). A dataset without a structure that has the quantity is
not measured by its metrics: its metric columns hold ``nan``.

The global metrics of every quantity average only the measured datasets.
For indexed :hpo:key:`!PC_ERMSE <ROOT.PC_ERMSE>`,
:hpo:key:`!PC_FRMSE <ROOT.PC_FRMSE>` and
:hpo:key:`!PC_SRMSE_GPA <ROOT.PC_SRMSE_GPA>`, an unmeasured dataset needs
a loss-term weight of 0: it contributes no loss, and the indexed average
still divides by the number of validation datasets. Startup refuses a
metric with no measured dataset. It also refuses an indexed metric with a
loss-term weight above 0 on an unmeasured dataset. A validation dataset
that the filters leave empty is refused too.

The :math:`R^2` of :hpo:key:`!PC_ERSQ <ROOT.PC_ERSQ>` has no value for a
dataset whose energies per atom are all equal, such as a dataset of one
structure. Startup refuses :hpo:key:`!PC_ERSQ <ROOT.PC_ERSQ>` when a
measured validation dataset is such a dataset.

Early-rejection options
-----------------------

.. list-table:: Options of a metric line other than ``PC_ERSQ``
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Effect
   * - ``--skip-above <cap>``
     - Reject when the metric exceeds this finite positive value, in
       the metric's units (listed above).
   * - ``--skip-rel <ratio>``
     - Reject when the metric exceeds this finite positive multiple
       of its best previously accepted value. No relative comparison
       is made before such a value exists. Evaluations run at once
       compare with the best value recorded before all of them
       (:ref:`nfp_evaluation_numbers`).

:hpo:key:`PC_ERSQ <ROOT.PC_ERSQ>` takes neither option. Its
:math:`R^2` is better when higher, and an option rejects an evaluation
whose metric is above its threshold: it would reject the better fits.

.. versionchanged:: 1.6.0
   1.5.0 accepted both options on :hpo:key:`!PC_ERSQ <ROOT.PC_ERSQ>`,
   where they rejected the better fits.

These checks happen after validation and before the physical-property
targets. In the indexed form they apply separately to the selected
datasets; in the global form they apply to the aggregate metric.
When both options are present, either can reject the candidate. Rejected
candidates receive a failure score. Each option can appear only once on
a declaration.
