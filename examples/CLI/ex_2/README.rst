Example 2 - Basics: Nested Fitting Procedure
------------------------------------

This example demonstrates how to optimize model hyperparameters. It is trivial by design and is only meant to illustrate the use of the Tadah!MLIP framework for a nested fitting procedure.

Here we only cover optimisation of HP against a validation dataset. In general, such procedures will result in an overfitting of the hyperparameters to the validation dataset. The inclusion of the physics-based contraints will be covered elsewhere.

Files in this example:

* ``config.train``: defines the necessary parameters for the model

  .. literal_include:: config.train

* ``config.val``: simply lists the validation dataset

  .. literal_include:: config.val

* ``config.hpo`` defines the nested fitting (aka HPO) procedure.

  .. literal_include:: config.hpo

The ``tadah hpo`` command will run the nested fitting procedure:

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotargets config.hpo

The command will run number of iterations as defined in the ``config.hpo`` file or until the program is manually terminated. The opimized potential is saved to the ``best_pot.tadah`` file every time a new best potential is found.

The following log files are generated:

* loss.tadah: The loss function value for each individudal loss and the total loss.

* params.tadah: lists model parameters for iterations

* outvars.tadah: lists the output variables for each iteration. Be defauly each loss will be listed.

All three files have a twin file with a ``best_`` prefix, e.g. ``best_loss.tadah``, ``best_params.tadah``, and ``best_outvars.tadah``. These files are written only if the current model is better than the previous one.


Example 2 – Basics: Nested Fitting (Hyper-Parameter Optimisation)
-----------------------------------------------------------------

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

  .. literal_include:: config.train

* ``config.val`` – list of configurations that form the validation data set.

  .. literal_include:: config.val

* ``config.hpo`` – instructions that tell Tadah!MLIP how to perform HPO
  (choice of optimiser, loss function, search space, logging policy, …).

  .. literal_include:: config.hpo

Running the example
-------------------

Invoke the nested fitting run with:

.. code-block:: bash

   tadah hpo --config config.train --validation config.val --hpotargets config.hpo

The program iterates until it reaches the maximum number of evaluations set in
``config.hpo`` or until you stop it manually (e.g. with <Ctrl-C>).  
Whenever it finds a parameter set that improves upon the current best loss,
it writes the corresponding potential to ``best_pot.tadah``.

Output files
------------

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
