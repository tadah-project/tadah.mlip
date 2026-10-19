.. _hpo_optimizer_common:

.. _dlib_param_keys:

.. _optimizer-block-common-attributes:

.. _choosing-the-optimizer:

.. _ceres-param-keys:

Common optimiser settings
=========================

An optimiser minimises the loss over the dimensions selected by ``OPTIM``.
Set ``LIB`` and ``ALGO`` together. The settings below belong to an
optimiser: the main ``STRATEGY``, its ``EXPLORE`` or ``REFINE`` phase, or
an eligible ``INNER`` search. Direct settings in ``OPTIMIZER`` are the
short form for the main optimiser.

``LIB`` and ``ALGO`` form a required pair of case-sensitive names;
:doc:`optimizer` lists the supported combinations.
Each driver accepts a subset of the settings below; unsupported settings
are diagnosed during setup.

Common keys
-----------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Meaning
   * - ``LIB``, ``ALGO``
     - Required backend and algorithm names.
   * - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
     - Evaluation budget, time budget and target loss. See
       :doc:`nfp_stopping_settings`.
   * - ``FTOL_REL``, ``FTOL_ABS``, ``XTOL_REL``, ``XTOL_ABS``
     - Backend-dependent convergence settings. See
       :doc:`nfp_stopping_settings`.
   * - ``POPULATION n``
     - Population size for ``TADAH:ANNEAL_PA`` and NLopt's CRS2, ISRES,
       ESCH, MLSL and ``GD_STOGO_RAND`` algorithms. Other backends refuse
       it. Defaults and valid sizes depend on the algorithm.
   * - ``VECTOR_ARR n``
     - NLopt stored-vector count for ``LD_LBFGS``, ``LD_VAR1``,
       ``LD_VAR2`` and the ``LD_TNEWTON`` family. Other backends refuse it.
   * - ``THREADS n``
     - Positive integer, default ``1``. The main optimiser's value sizes
       the shared worker pool for training, prediction and LAMMPS scripts.
       The pool retains this size during ``EXPLORE`` and ``INNER`` searches.
       ``CERES`` rejects an explicit ``THREADS`` setting.
   * - ``SEED n``
     - Unsigned 64-bit optimiser seed. See the seed rules below.
   * - ``STEP fraction``
     - Initial step for supported NLopt algorithms; see below.
   * - ``GRAD_STRATEGY mode``
     - Gradient calculation; see :doc:`nfp_gradients`.
   * - ``PARAM name value``
     - Algorithm-specific setting; see the parameter references below.

``POPULATION`` and ``VECTOR_ARR`` accept unsigned integers; zero has the
backend's own meaning. ``THREADS`` is in ``[1, 2147483647]``. Startup
checks each setting against the selected algorithm's capabilities.

Initial step: ``STEP``
----------------------

``STEP`` is a finite fraction in ``(0, 0.5]`` of each dimension's
transformed search width. It is supported by selected NLopt algorithms,
including ``LN_BOBYQA``, ``LN_COBYLA``, ``LN_SBPLX`` and
``LN_NELDERMEAD``.

Use a block-wide value, optionally with overrides::

   STEP 0.1
   STEP CGRID2B(1-3) 0.05

Overrides can select all optimised indices of a key (omit the index
list), or a list such as ``(1-4,7)``. Each selected index must belong to
the search. Without a block-wide ``STEP``, overrides must cover every
optimised dimension. The effective steps can be adjusted near bounds
and are reported in the run journal. An outer MLSL/AUGLAG ``STEP``
overrides the step of its ``INNER`` search.

For annealing, use its ``PARAM STEP_FRAC``. For ``TADAH:GRID``, use
``PARAM GRID_POINTS``. Finite-difference steps use ``PARAM fd_step``;
see :doc:`nfp_gradients`.

Seeds
-----

An optimiser's ``SEED`` controls its search stream. ``INIT SEED`` and
``RESTART SEED`` control their starting-point methods separately.
NLopt uses the enclosing optimiser's seed, falling back to its
``INIT`` seed when no optimiser seed is present. ``INNER`` has no seed
of its own.

A ``PRESTAGE`` exploration without a seed of its own takes the
``STRATEGY``-level ``SEED``. Its own seeds come first: the ``EXPLORE``
block's ``SEED`` and, for NLopt, its ``INIT`` seed. The refinement draws
other random numbers from the same ``SEED``, so one ``SEED`` repeats the
whole run. The journal names the seed of the exploration.

.. versionchanged:: 1.6.0
   In 1.5.0 such an exploration was not seeded, so a run did not repeat.

``TADAH:GRID``, ``DLIB:BFGS`` and ``CERES`` refuse ``SEED`` because they
draw no random numbers. ``DLIB:GFS`` ignores it with a warning. Every
NLopt algorithm accepts it, including deterministic ones. Set seeds
explicitly for stochastic search and sampling; see
:doc:`nfp_initialisation` and :doc:`nfp_restart`.

Algorithm parameters: ``PARAM``
-------------------------------

Keep parameter names in the case shown by their reference.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Backend
     - Accepted parameter reference
   * - ``TADAH:RANDOM``, ``DLIB:GFS``
     - No ``PARAM`` settings.
   * - ``TADAH:GRID``
     - ``PARAM GRID_POINTS n``: integer at least ``2``; default ``20``
       grid points per dimension.
   * - ``DLIB:BFGS``
     - Finite-difference settings in :doc:`nfp_gradients`, and
       ``tol_gradinf`` in :doc:`nfp_stopping_settings`.
   * - ``CERES:STEEPEST_DESCENT``
     - Finite-difference settings, ``tol_gradinf``, ``max_iter`` and
       ``max_ls_iter``; see :doc:`nfp_gradients` and
       :doc:`nfp_stopping_settings`.
   * - ``NLOPT``
     - Shared finite-difference settings plus numeric NLopt
       ``set_param`` settings. Their applicability belongs to the chosen
       NLopt algorithm and requires confirmation in that algorithm's
       parameter reference.
   * - ``TADAH:ANNEAL``, ``ANNEAL_PA``
     - :doc:`anneal` and :doc:`anneal_pa`.
   * - ``TADAH:ANNEAL_PT``
     - :doc:`anneal_pt`.
   * - ``ASA:ASA``
     - :doc:`asa`.

``LOG_HP`` and ``OPTIM_SCALE`` configure the search coordinates. See
:doc:`nfp_search_space`.
