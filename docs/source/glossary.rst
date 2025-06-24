.. _glossary:

Glossary
========

.. glossary::

   Nested Fitting Procedure, NFP, Hyperparameter Optimizer, HPO
      A two-level fitting procedure that optimises :term:`hyperparameters` and
      :term:`model coefficients<LP>` by iteratively retraining the model and validating
      it against :term:`performance constraints`.

   Training Configuration, TC, Config File
      A file that defines the initial model and training parameters for either training or the
      :term:`nested fitting procedure`. It completely specifies the initialise model.

   Nested Fitting Config, NFC, HPO Config, HPO Targets
      A file that defines the settings used for :term:`nested fitting procedure`.

   Hyperparameter, HP
      A model parameter chosen *before* regression (cutoff, number of Gaussians …).

   Learned Parameter, LP
      Coefficient found by regression (:math:`\mathbf w`).

   Performance Constraint, PC
      A measurable physical property used as part of the loss.

   Search-Space Constraint, SSC
      A bound placed on a :term:`hyperparameter` via an ``OPTIM`` line.

