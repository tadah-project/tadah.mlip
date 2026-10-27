.. _glossary:

Glossary and Abbreviations
==========================

.. glossary::

   Command Line Interface
      A way to interact with the program by typing commands into a terminal or console.

   Nested Fitting Procedure
   Hyperparameter Optimization
      A two-level fitting procedure that optimises :term:`hyperparameters<hyperparameter>` and
      :term:`learned parameter<learned parameter>` by iteratively retraining the model and validating
      it against :term:`performance constraint<performance constraint>`.

   Training Configuration
      A file that defines the initial model and training parameters for either training or the
      :term:`nested fitting procedure`. It completely specifies the initial model.

   Nested Fitting Configuration
   HPO Configuration
      A file that defines the settings used for :term:`nested fitting procedure`.

   Hyperparameter
      A model parameter chosen *before* regression (cutoff, widths of Gaussians ...).

   Learned Parameter
      Coefficient found by regression (:math:`\mathbf w`).

   Error scale
      The residual of an observation that costs one priority unit in a fit,
      in the unit of the metric that measures it: ``ESIGMA`` (meV/atom;
      meV per √atom under ``ENORM SQRT``), ``FSIGMA`` (eV/Å), ``SSIGMA``
      (GPa) (:ref:`fit_objective`).

   Priority
      A number >= 0 that multiplies the cost of an observation in a fit: the
      ``eweight``, ``fweight`` or ``sweight`` of a structure times the
      ``WDBFILE`` value of its dataset (:ref:`fit_objective`).

   Balance table
      The table of a fit, printed by ``tadah train`` and, for its best
      potential, by ``tadah hpo``, and kept in the training record of the
      potential: per quantity and dataset, the observations, the residuals,
      the share of the minimised sum and the effective parameters of each
      (:ref:`fit_balance`).

   Effective parameters
      How many of the coefficients a quantity, a dataset or the prior
      determines (:ref:`fit_balance`).

   Performance constraint
      A measurable (physical) property used as part of the loss.

   Loss term
      One contribution to the nested-fitting loss: an enabled validation
      metric, a ``PC_LAMMPS --varloss`` variable or a ``PC_OBSERVABLE``
      line.  The loss of an evaluation is the mean of its weighted terms,
      clipped to ``FAILSCORE`` (:ref:`nested_fitting`).

   Batch
      The evaluations that an optimiser runs at once on its workers, such
      as an epoch of :hpo:opt:`TADAH:ANNEAL_PT`
      (:ref:`nfp_evaluation_numbers`).

   Search Space Constraint
      A bound placed on a :term:`hyperparameter` via an ``OPTIM`` line.

Abbreviations
=============

.. glossary::

   API
      Application Programming Interface

   CLI
      Command Line Interface

   NFP
      Nested Fitting Procedure

   HPO
      Hyperparameter Optimisation
