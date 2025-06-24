 .. _nested_fitting:

Nested Fitting
==============

.. warning::

  This section is under construction.  The content is incomplete and may
  still contain errors.  We are working on it and will update it soon.

.. figure:: images/nested_fitting.jpg
   :alt: Nested Fitting Procedure
   :width: 80%
   :align: center

   Workflow of the :term:`nested fitting procedure`: the outer optimizer proposes
   new :term:`hyperparameters<hyperparameter>`, the model is retrained, and each trial potential is
   validated against :term:`performance constraints<performance constraint>` that feed back into the global
   loss.

Nested fitting is Tadah!MLIP’s automated, two-level fitting workflow:

* **inner loop** – ordinary regression that determines
  the :term:`learned parameters<learned parameter>` :math:`\mathbf w` for a fixed model definition;
* **outer loop** – a global optimizer that samples from
  the :term:`search space constraints<search space constraint>` (SSH),
  retrains the model, and judges the result against user-defined :term:`performance constraints<performance constraint>` (PC).

Letting a computer search the :term:`hyperparameter` space helps to

* escape the “good on the validation set, unstable in MD” trap;
* trade accuracy for speed (or vice versa) in a reproducible way;
* fold real-world performance constraints—elastic constants, phase
  stability, surface energies, …—directly into the loss function.

This page gives the necessary background and demonstrates how to set up
Tadah!MLIP for nested fitting.

Background
----------

A traditional MLIP workflow stops once regression converges on a
training/validation split.  Unfortunately, a potential that interpolates
perfectly may still

* disintegrate during an MD run
* produce an incorrect equation of state
* predict the wrong ground-state crystal
* extrapolate poorly beyond the training set

Usual remedies, such as manual hyperparameter tuning or enlarging the training
set, are both tedious and not guaranteed to succeed.

Nested fitting tackles the problem by *measuring* emergent properties
during the fit.  Each trial potential is dropped into a short LAMMPS run;
the resulting quantities are compared to their targets and the discrepancy
contributes to a **global loss**

.. math::

  L_{\text{total}} = \sum_{i} w_i\;
                     \mathcal L_i\!\bigl(|y_i-\hat y_i|\bigr),

where :math:`w_i` is the user-supplied weight, :math:`y_i` the target,
:math:`\hat y_i` the prediction, and :math:`\mathcal L_i` one of the built-in
loss functions (L1, L2, Huber, Tukey, …).

The outer optimizer explores the hyperparameter space :math:`\Theta`
declared with the ``OPTIM`` directive:

.. math::

  \theta^\star
  = \operatorname*{arg\,min}_{\theta\in\Theta} L_{\text{total}}(\theta).

Because the model is retrained for every candidate :math:`\theta`, the
procedure is computationally expensive but also powerful: it can expose
regions of hyperparameter space that yield stable, accurate, and fast
potentials. However, it is not a silver bullet:
**Success still depends on sensible choices of performance
constraints and search space limits**.

.. _hpo_quick_start:

Quick Start
-----------

Nested fitting requires a configuration file that defines the inital model (the same file as for regular training),
a validation file (simple list of datasets to validate against) and a hpo targets file that defines the outer-loop.
The command to run the nested fitting is:

::

  ``tadah hpo --config <FILE> --hpotarget <FILE> --validation <FILE>``

See :ref:`cli_example_2` for the simple example of nested fitting.

More examples will be added in the future. In the meantime, you can
contact us for help with setting up nested fitting for your specific use case.

.. _hpo_manual:

Manual
------

This manual covers the content of the configuration file format which is used to drive the nested fitting procedure.
As well as some general information about the nested fitting procedure.

In general the following steps are required to run the nested fitting procedure:

  #. Define the initial model in the :term:`training configuration` file.

  #. List validation dataset(s) in the validation file using :ref:`DBFILE` keyword.

  #. Define the :term:`nested fitting configuration` file with the following blocks:

     * ``LOSS`` to define the default loss function

     * ``OPTIMIZER`` block to define the outer optimizer

     * ``OPTIM`` line(s) to define the :term:`SSH<search space constraint>`

     * ``PC_<TYPE>`` line(s) to define the :term:`PC<performance constraint>`:

       * ``PC_ERMSE`` for validation RMSE of energies (see :ref:`performance_constraints` for more)

       * ``PC_LAMMPS`` line(s) to define the physics-informed performance constraints


Loss functions
..............

The keyword ``LOSS`` is used to define the default loss function for the nested fitting procedure.
Individual ``PC_LAMMPS`` simulations can override it.

The currently supported loss functions are:

================ =============== ==============================
Name             Extra params    Comment
================ =============== ==============================
``L1``           —               :math:`|x|`
``L2``           —               :math:`x^2`
``HUBER``        ``δ``           quadratically near zero, linear beyond δ
``TUKEY``        ``c``           redescending, zero influence for |x|>c
``LOG_COSH``     —               smooth alternative to L1
``RMSLE``        —               log-scaled L2 (non-negative targets)
================ =============== ==============================

Choosing the optimizer
.......................

The optimizer for the outer loop is specified in the :term:`nested fitting configuration` file.
The optimizer could be either global, local, or a hybrid of the two. The latter case is only supported by a handful of optimizers.

Tadah!MLIP supports optimizers from the following libraries:

* NLOPT [https://nlopt.readthedocs.io/](https://nlopt.readthedocs.io/)
  * We aim to support all NLOPT algorithms which do not require analytical
    derivatives, i.e. all algorithms which names begin with either ``GN_`` or ``LN_``.
    See the `NLOPT documentation <https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/>`_

* Dlib [http://dlib.net/](http://dlib.net/)
  * Local numerical optimizers (BFGS, LBFGS, CG, BOBYQA) [Work in progress...]
  * Global MaxLIPO+TR aka Global Function Search (GFS)

* Tadah!MLIP also provide basic random search and grid search optimizers.

The optimizer is specified in the OPTIMIZER block of the :term:`nested fitting configuration` file.

.. code-block:: none

   OPTIMIZER
     <key> <value> 
     ...
   ENDOPTIMIZER

List of optimizer block keys:
=================   =========================================================
``key``              ``value``
=================   =========================================================
``LIB``             name of the library, e.g. ``NLOPT``, ``DLIB``, ``TADAH``
``ALGO``            name of the algorithm, e.g. ``GN_DIRECT``, ``LD_COBYLA``, ``GFS``
``MAXEVAL``         maximum number of evaluations of the loss function
``MAXTIME``         maximum time in seconds to run the optimizer
``STOPVAL``         stop the optimization when the loss is below this value
``FTOL_REL``        relative tolerance for the loss function
``FTOL_ABS``        absolute tolerance for the loss function
``XTOL_REL``        relative tolerance for the hyperparameters
``XTOL_ABS``        absolute tolerance for the hyperparameters
``POPULATION``      number of individuals in the population (for population-based optimizers)
``STEP``            step size for the optimizer
``SEED``            random seed for the optimizer (default: current time)
``VECTOR_STORAGE``  required by some NLOPT algorithms
``THREADS``         number of threads for the Dlip::MaxLIPO+TR optimizer (default: 1)
``PARAM``           generic key for passing additional parameters to the optimizer
``LOGSCALE_THRESHHOLD``  
                   threshold for the log-scaling of the search space (experimental)

If optimizer is a hybrid of global and local, the nested ``LOCAL`` block must be specified.
The local block follows the same syntax as the global block, 
but it is used to configure the local optimizer that will be used for the local search.

.. code-block:: none

   OPTIMIZER
     <key> <value> ...
     ...
     LOCAL
       <key> <value> ...
       ...
     ENDLOCAL
   ENDOPTIMIZER

.. _search_space_constraints:

Search space constraints
........................

The search space constraints are defined in the :term:`nested fitting configuration` file using the ``OPTIM`` directive.
The ``OPTIM`` lines specify the hyperparameters that the outer optimizer will
explore.  Each ``OPTIM`` refers to a single KEY from a configuration file which defines the initial model,
(*config.train*), which is then treated as an optimisation variable.

Since KEY can contain multiple values, the
``OPTIM`` lines can also specify indices to select a subset of the values. The ``OPTIM`` line can be repeated for
the same KEY in case you want to supply different low and high bounds for different indices.

``OPTIM`` syntax:

.. code-block:: none

   OPTIM <KEY> (indices) <low> <high>

``indices`` must be specified in the brackets ``(indices...)`` and follow the order in the config file which defines the initial model.
Indices can be comma-separated lists, ranges ``a-b`` or strides ``a-b:s``  
(e.g. ``(1,4,7-10:2)``). Indices start from 1. ``<low`` and ``<high>`` are the floating point numbers for the lower and upper bounds.






















.. _performance_constraints:

Performance constraints
.......................

The simplest performance contrainst are MAE, RMSE, and R², which can be calculated for either energies, forces, or stresses.
These constrains are evaluated on the validation set.

The physics informed performance constraints are defined in the :term:`nested fitting configuration` file using the ``PC_LAMMPS`` directive.
LAMMPS is the work-horse that progressivly turns a potential into
numbers that matter.  Each ``PC_LAMMPS`` line in launches a separate simulation:

.. code-block:: none

   PC_LAMMPS --script in.elastic                 \
          --varloss C11sC22 0 100 TUKEY 10    \
          --varloss bulkmodulus 145 10 TUKEY 200 \
          --failscore 1e5

Required options
^^^^^^^^^^^^^^^^

``--script <file>``  
   LAMMPS input file.  Inside you must define *equal-style* variables whose
   values will be grabbed at the end of the run.

``--varloss <name> <target> <weight> [loss-type p₁ p₂…]``  
   Declare one variable to be included in :math:`L_{\text{total}}`.

Optional goodies
^^^^^^^^^^^^^^^^

``--invar <var> <value>`` injects a LAMMPS variable (like ``-var`` on the
command line), allowing the same script to be reused for many structures or
pressures.

``--outvar <var>`` records a variable in ``outvar.tadah`` *without*
contributing to the loss—handy for post-mortem plots.

``--failscore <val>`` overrides the global ``FAILSCORE``:

if LAMMPS crashes or loss exceeds this value the optimizer receives FAILSCORE

Output files
............

All artefacts land in the current directory.

====================== =====================================================
File / directory       Contents
====================== =====================================================
``loss.tadah``         
``outvar.tadah``
``params.tadah``
====================== =====================================================

Use ``OUTPUT N`` to write *loss.dat* every N iterations  
(default 10), ``BOUTPUT N`` for the best-only stream.

Performance & parallelism
.........................

The desktop version of Tadah!MLIP runs each LAMMPS simulation using a serial version of LAMMPS.
This allwos to run multiple ``PC_LAMMPS`` scripts in parallel. Use the
``OMP_NUM_THREADS`` environment variable to set the number of OpenMP threads. In case where optimizer is also parallelised, the number of threads times the number of OMP threads must not exceed the number of available cores otherwise this will result in significant downgrade in performance.

The MPI version of Tadah!MLIP runs each LAMMPS simulation using the MPI version of LAMMPS and allow to allocate specific number of cpu per LAMMPS simulation using ``--ncpu <value>`` option. **[work in progress...]**.
