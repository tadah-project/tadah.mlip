Example 2 - Basics: Nested Fitting Procedure
--------------------------------------------

This example shows how to optimise a model’s hyper-parameters (HPs) with Tadah!MLIP.  
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

  .. literal_include:: ../../../examples/CLI/ex_2/config.train

* ``config.val`` – list of configurations that form the validation data set.

  .. literal_include:: ../../../examples/CLI/ex_2/config.val

* ``config.hpo`` – instructions that tell Tadah!MLIP how to perform HPO
  (choice of optimiser, loss function, search space, logging policy, …).

  .. literal_include:: ../../../examples/CLI/ex_2/config.hpo

Running the example
...................

Invoke the nested fitting run with:

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotargets config.hpo

The program iterates until it reaches the maximum number of evaluations set in
``config.hpo`` or until you stop it manually (e.g. with <Ctrl-C>).  
Whenever it finds a parameter set that improves upon the current best loss,
it writes the corresponding potential to ``best_pot.tadah``.

Output files
............

The optimiser produces three main log files that record *every* iteration

* ``loss.tadah``   – value of each individual loss term and the total loss.  
* ``params.tadah`` – hyper-parameter vector for the current iteration.  
* ``outvars.tadah`` – any additional output variables (by default each loss term).

For convenience, each of these files has a *best* counterpart that is updated
only when a new optimum is reached:

* ``best_loss.tadah``  
* ``best_params.tadah``  
* ``best_outvars.tadah``  

These “best\_” logs allow you to track the progress of the optimisation without
having to parse the full history.
