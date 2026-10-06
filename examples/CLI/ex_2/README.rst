Example 2 - Basics: Nested Fitting Procedure
--------------------------------------------

This example shows how to optimise a model’s hyper-parameters (HPs) with Tadah!MLIP.
The datasets ``tdata.db`` and ``vdata.db`` hold bcc Ta configurations from LAMMPS molecular dynamics with the Ta1_Ravelo_2013 EAM potential: energies in eV, forces in eV/Å and the virial (configurational) Cauchy stress in eV/Å³, positive under tension.  
The workflow is deliberately simple and is intended only as a minimal, self-contained
demonstration of the *nested fitting* (also called hyper-parameter optimisation, HPO)
capability.

Scope
.....

In this tutorial we optimise the HPs **only against a validation data set**.  
Doing so will generally lead to some degree of over-fitting to that validation set.
How to add physics-based constraints or extra test sets to mitigate over-fitting is
covered in a separate example.

Files provided
..............

* ``config.train`` – training-stage settings for the underlying potential.

.. literalinclude:: ../../examples/CLI/ex_2/config.train
   :language: ini

* ``config.val`` – list of configurations that form the validation data set.

.. literalinclude:: ../../examples/CLI/ex_2/config.val
   :language: ini

* ``config.hpo`` – instructions that tell Tadah!MLIP how to perform HPO
  (choice of optimiser, loss function, search space, logging policy, …).

.. literalinclude:: ../../examples/CLI/ex_2/config.hpo
   :language: ini

Running the example
...................

Invoke the nested fitting run with:

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotarget config.hpo

The program iterates until it reaches the maximum number of evaluations set in
``config.hpo`` or until you stop it manually (e.g. with <Ctrl-C>).  
Whenever it finds a parameter set that improves upon the current best loss,
it writes the corresponding potential to ``best_pot.tadah``.

Output files
............

The optimiser produces four main log files with one row per logged
iteration (``OUTPUT 10`` in this example: every 10th iteration).  Each row
starts with the iteration number and the slot:

* ``loss.tadah``   – value of each individual loss term and the total loss
  (the mean of the terms).
* ``params.tadah`` – the optimised hyperparameters, one column per
  dimension, labelled ``KEY[n]`` and ordered by key, then index
  (here ``CGRID2B[1..4]`` before ``SGRID2B[1..4]``).
* ``outvar.tadah`` – the evaluation wall time and the raw values behind the
  loss (here the validation energy RMSE).
* ``stats.tadah``  – each enabled validation metric per validation dataset
  and their average.

For convenience, each of these files has a *best* counterpart that gets a
row only when a new optimum is reached (``BOUTPUT 1``: every new best).
Its first column is the improvement ordinal ``k`` (1 for the first best,
2 for the second, …) instead of the iteration number:

* ``best_loss.tadah``
* ``best_params.tadah``
* ``best_outvar.tadah``
* ``best_stats.tadah``

These “best\_” logs allow you to track the progress of the optimisation without
having to parse the full history.

The ``DUMP``/``BDUMP`` lines additionally archive potentials:
``newPotsDir/pot_<iter>.tadah`` for every 100th iteration and
``newBestPotsDir/best_pot_<k>.tadah`` for every new best.
