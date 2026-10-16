.. _background:

.. _nested_fitting:

Overview
========

Nested fitting searches model settings against a configured loss.
The command is ``tadah hpo``; the names *Nested Fitting Procedure* (NFP)
and *Hyperparameter Optimisation* (HPO) refer to the same feature.

For each candidate, the outer optimiser changes the parameters selected
by ``OPTIM``. The inner fit solves the model's free coefficients using
the training configuration. The resulting potential is scored against
validation metrics or physical-property targets, and that loss guides
the next candidate.

.. figure:: images/nested_fitting.jpg
   :alt: Nested fitting workflow with an outer parameter search and inner coefficient fit.
   :align: center
   :width: 610px

   Nested fitting workflow illustrated with the DLIB GFS (MaxLIPO+TR)
   optimiser.

.. _what-you-configure:

Configuration components
------------------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Part of the run
     - Reference
   * - Input files and command
     - :doc:`nfp_quickstart`
   * - File format and nesting
     - :doc:`nfp_configuration`
   * - Parameters the search may vary
     - :doc:`nfp_search_space`
   * - Quantities each potential should reproduce
     - :doc:`nfp_objective`
   * - Algorithm, starting points and run policy
     - :doc:`nfp_optimisation`
   * - Saved potentials and histories
     - :doc:`nfp_output_settings`

For successful evaluations, the total loss is the arithmetic mean of
the weighted loss terms. A failed target can fail the whole evaluation.
Target units and scales determine suitable weights; the exact rules are
in :doc:`nfp_losses`.

Data used to choose hyperparameters is part of fitting. An independent
test set or set of properties provides an assessment of the final potential.
