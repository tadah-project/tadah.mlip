.. _glossary:

Glossary
========

.. glossary::

   NFP
   Nested Fitting Procedure
   HPO
   Hyperparameter Optimizer
      A two-level fitting procedure that optimises :term:`hyperparameters` and
      :term:`model coefficients<LP>` by iteratively retraining the model and validating
      it against :term:`performance constraints`.

   TC
   Training Configuration
   Config File
      A file that defines the initial model and training parameters for either training or the
      :term:`nested fitting procedure`. It completely specifies the initialise model.

   NFC
   Nested Fitting Configuration
   HPO Configuration
   HPO Targets
      A file that defines the settings used for :term:`nested fitting procedure`.

   HP
   Hyperparameter
      A model parameter chosen *before* regression (cutoff, number of Gaussians …).

   LP
   Learned Parameter
      Coefficient found by regression (:math:`\mathbf w`).

   PC
   Performance constraint
      A measurable (physical) property used as part of the loss.

   SSC
   Search Space Constraint
      A bound placed on a :term:`hyperparameter` via an ``OPTIM`` line.

