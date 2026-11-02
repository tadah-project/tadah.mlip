.. _nfp_targets_losses:

Targets and Losses
==================

The pages below describe how the targets and the loss of a nested fitting
search are written in the target file, and assume the file syntax of
:doc:`nfp_configuration`. Targets define the quantities that each candidate
potential must reproduce; loss functions and loss-term weights determine
their contributions to the loss. Validation metrics can accompany one
physical-property target family per run: LAMMPS targets or built-in
observables.

.. toctree::
   :maxdepth: 1

   nfp_losses
   nfp_validation_targets
   nfp_lammps_targets
   nfp_lammps_options
   nfp_observable_targets
   nfp_feasibility
