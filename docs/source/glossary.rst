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
      :term:`nested fitting procedure`. It completely specifies the initialise model.

   Nested Fitting Configuration
   HPO Configuration
      A file that defines the settings used for :term:`nested fitting procedure`.

   Hyperparameter
      A model parameter chosen *before* regression (cutoff, widths of Gaussians ...).

   Learned Parameter
      Coefficient found by regression (:math:`\mathbf w`).

   Performance constraint
      A measurable (physical) property used as part of the loss.

   Search Space Constraint
      A bound placed on a :term:`hyperparameter` via an ``OPTIM`` line.

.. abbreviations::

   API
      Application Programming Interface

   CLI
      Command Line Interface

   NFP
      Nested Fitting Procedure

   HPO
      Hyperparameter Optimization
