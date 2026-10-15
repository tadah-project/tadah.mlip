.. _background:

.. _nested_fitting:

Overview
========

Nested fitting searches model settings against a loss that you define.
The command is ``tadah hpo``; the names *Nested Fitting Procedure* (NFP)
and *Hyperparameter Optimisation* (HPO) refer to the same feature.

For each candidate, the outer optimiser changes the parameters selected
by ``OPTIM``. The inner fit solves the model's free coefficients using
the training configuration. The resulting potential is scored against
validation metrics or physical-property targets, and that loss guides
the next candidate.

Pinned coefficients remain fixed during the inner fit. They can instead
be searched directly with ``OPTIM FIXWEIGHT``. When all coefficients
are pinned, the least-squares stage is unnecessary. The refit workflow
supports both pinned-coefficient searches and re-solving coefficients
from training data; see :ref:`DM_Embed refit workflows <dm_embed_refit>`.

What you configure
------------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Part of the run
     - Where to start
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

For successful evaluations, the total loss is the mean of the weighted
loss terms, not a sum normalised by the weights. A failed target can
fail the whole evaluation. Choose targets and weights with their units
and scales in mind; the exact rules are in :doc:`nfp_losses`.

Data used to choose hyperparameters is part of fitting. Keep an independent
test set or set of properties to assess the final potential.
