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
       ``LD_VAR2`` and the ``LD_TNEWTON_PRECOND`` algorithms. Other
       algorithms, ``LD_TNEWTON`` and ``LD_TNEWTON_RESTART`` included,
       refuse it.
   * - ``THREADS n``
     - Positive integer, default ``1``. The main optimiser's value sizes
       the shared worker pool for training, prediction and LAMMPS scripts.
       The pool retains this size during ``EXPLORE`` and ``INNER`` searches.
       In an ``EXPLORE`` block that names ``DLIB:GFS``, ``ASA:ASA``,
       ``TADAH:ANNEAL_PT`` or ``TADAH:ANNEAL_PA``, ``THREADS`` sets how
       many evaluations that exploration asks for at once, and the default
       of ``REPLICAS`` (``ANNEAL_PT``) and of ``POPULATION``
       (``ANNEAL_PA``). An ``EXPLORE`` block of another optimiser and
       ``CERES`` refuse it, and so does ``INNER`` (see :doc:`nfp_inner`).
       A seeded run writes the same files at any
       :hpo:key:`THREADS <OPTIMIZER.THREADS>` under the conditions of
       :ref:`nfp_evaluation_numbers`, except under :hpo:opt:`DLIB:GFS`.
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
optimised dimension. An outer MLSL/AUGLAG ``STEP`` overrides the step
of its ``INNER`` search. The effective steps are reported in the run
journal.

Without :hpo:key:`STEP <OPTIMIZER.STEP>`, :hpo:opt:`NLOPT:LN_BOBYQA` and
:hpo:opt:`NLOPT:LN_COBYLA` take 0.25 of the width of the box of each
search parameter as their first step, a scale they keep for the whole
search. The :hpo:key:`!STEP <OPTIMIZER.STEP>` entry gives their steps as
the local search of an MLSL or AUGLAG, and the starts that
:hpo:opt:`!LN_BOBYQA <NLOPT:LN_BOBYQA>` moves next to a bound.

With :hpo:key:`!STEP <OPTIMIZER.STEP>` set, :hpo:opt:`NLOPT:LN_SBPLX` and
:hpo:opt:`NLOPT:LN_NELDERMEAD` turn a step inward when the start lies
within one step of the upper bound. With :hpo:key:`!STEP <OPTIMIZER.STEP>`
unset, NLopt's own default sets the steps of
:hpo:opt:`!LN_SBPLX <NLOPT:LN_SBPLX>`,
:hpo:opt:`!LN_NELDERMEAD <NLOPT:LN_NELDERMEAD>` and
:hpo:opt:`NLOPT:LN_PRAXIS`: 0.25 of the width of the box of each search
parameter, or 0.75 of the distance to a bound closer than that.

.. versionchanged:: 1.6.0
   In 1.5.0 a start closer to a bound than one step shrank the step of
   that search parameter for the whole search: for
   :hpo:opt:`!LN_BOBYQA <NLOPT:LN_BOBYQA>`, and with
   :hpo:key:`!STEP <OPTIMIZER.STEP>` unset for
   :hpo:opt:`!LN_COBYLA <NLOPT:LN_COBYLA>` and for either as the local
   search of an MLSL or AUGLAG.

For annealing, use its ``PARAM STEP_FRAC``. For ``TADAH:GRID``, use
``PARAM GRID_POINTS``. Finite-difference steps use ``PARAM fd_step``;
see :doc:`nfp_gradients`.

Seeds
-----

An optimiser's :hpo:key:`SEED <OPTIMIZER.SEED>` controls its search
stream. ``INIT SEED`` and ``RESTART SEED`` control their starting-point
methods separately. An optimiser that draws random numbers takes the
:hpo:key:`!SEED <OPTIMIZER.SEED>` of its block; an NLopt algorithm next
takes the :hpo:key:`SEED <INIT.SEED>` of its
:hpo:block:`INIT <BLOCK.INIT>` block. :hpo:block:`INNER <BLOCK.INNER>`
has no seed of its own.

Without either seed, the optimiser draws its numbers with a seed drawn
once for the run. Standard output and the journal give that seed with the
:hpo:key:`!SEED <OPTIMIZER.SEED>` line that draws the same numbers again.
:hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>` starts, the
:hpo:key:`PRESTAGE <STRATEGY.TYPE.PRESTAGE>` refinement and
:hpo:block:`RESTART <BLOCK.RESTART>` rounds derive their seeds from that
seed as from a :hpo:key:`!SEED <OPTIMIZER.SEED>` line.

.. versionchanged:: 1.6.0
   In 1.5.0 :hpo:opt:`TADAH:RANDOM`, the annealers and :hpo:opt:`ASA:ASA`
   without :hpo:key:`SEED <OPTIMIZER.SEED>` drew a new seed each time they
   started, so a run of several starts or rounds could not be repeated.

A :hpo:key:`!PRESTAGE <STRATEGY.TYPE.PRESTAGE>` exploration whose
optimiser reads :hpo:key:`!SEED <OPTIMIZER.SEED>` and that has no seed of
its own takes the :hpo:key:`!SEED <OPTIMIZER.SEED>` line of the
:hpo:block:`STRATEGY <BLOCK.STRATEGY>`-level optimiser. The refinement
draws other random numbers from that :hpo:key:`!SEED <OPTIMIZER.SEED>`,
so the optimisers of both phases repeat. Without such a line, the
exploration draws a seed of its own, which standard output and the
journal give. Its own seeds come first: the
:hpo:block:`EXPLORE <BLOCK.EXPLORE>` block's
:hpo:key:`!SEED <OPTIMIZER.SEED>` and, for NLopt, its
:hpo:block:`!INIT <BLOCK.INIT>` seed. The journal names the seed of the
exploration.

.. versionchanged:: 1.6.0
   In 1.5.0 such an exploration was not seeded, so a run did not repeat.

Starting points that an :hpo:block:`!INIT <BLOCK.INIT>` or
:hpo:block:`!RESTART <BLOCK.RESTART>` block draws repeat only with that
block's :hpo:key:`!SEED <INIT.SEED>` (see :doc:`nfp_initialisation`).

``TADAH:GRID``, ``DLIB:GFS``, ``DLIB:BFGS`` and ``CERES`` refuse ``SEED``
because they draw no random numbers, except as a ``PRESTAGE`` refinement
whose ``SEED`` the exploration takes. An NLopt algorithm that draws no
random numbers, such as ``LN_SBPLX`` or ``GN_DIRECT_L``, refuses ``SEED``
too. The :ref:`optimiser reference <nfp_ref_optimisers>` says how each
optimiser reads ``SEED``. A stochastic search and its starting points
repeat only with ``SEED`` set; see :doc:`nfp_initialisation` and
:doc:`nfp_restart`.

.. versionchanged:: 1.6.0
   In 1.5.0 every NLopt algorithm accepted ``SEED``.

Algorithm parameters: ``PARAM``
-------------------------------

Parameter names keep the case shown by their reference. Each value is a
number, except the words of the finite-difference ``PARAM fd_kernel`` and
of the annealing keys; a word given to another key is refused.

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
     - The numeric ``PARAM`` keys NLopt reads for the chosen algorithm
       (``tolg`` of ``LD_LBFGS``, ``magic_eps`` of the ``GN_DIRECT``
       algorithms) and, when it takes finite-difference gradients, the
       finite-difference settings of :doc:`nfp_gradients`;
       :ref:`the optimiser reference <nfp_ref_optimisers>` names them for
       each algorithm, and any other name is refused.
   * - ``TADAH:ANNEAL``, ``ANNEAL_PA``
     - :doc:`anneal` and :doc:`anneal_pa`.
   * - ``TADAH:ANNEAL_PT``
     - :doc:`anneal_pt`.
   * - ``ASA:ASA``
     - :doc:`asa`.

``LOG_HP`` and ``OPTIM_SCALE`` configure the search coordinates. See
:doc:`nfp_search_space`.
