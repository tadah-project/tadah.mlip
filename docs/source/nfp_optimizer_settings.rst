.. _optimizer-block-common-attributes:

Common Optimiser Settings
=========================

This page describes the settings that every optimiser of a target file
shares. It assumes a target file with an
:hpo:block:`OPTIMIZER <BLOCK.OPTIMIZER>` block (see
:doc:`nfp_configuration` and :doc:`nfp_optimizer`).

An optimiser minimises the loss over the search parameters that the
:hpo:key:`OPTIM <ROOT.OPTIM>` lines define. The settings below belong to
an optimiser: the main :hpo:block:`STRATEGY <BLOCK.STRATEGY>`, its
:hpo:block:`EXPLORE <BLOCK.EXPLORE>` or
:hpo:block:`REFINE <BLOCK.REFINE>` phase, or the local search of an
:hpo:block:`INNER <BLOCK.INNER>` block. Settings written directly in
:hpo:block:`!OPTIMIZER <BLOCK.OPTIMIZER>` configure the main optimiser.

:hpo:key:`LIB <OPTIMIZER.LIB>` and :hpo:key:`ALGO <OPTIMIZER.ALGO>` are
always given together. They form a required pair of case-sensitive
names; :doc:`optimizer` lists the supported combinations. Each optimiser
accepts a subset of the settings below; unsupported settings are refused
when ``tadah hpo`` starts.

.. _choosing-the-optimizer:

Common keys
-----------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key
     - Meaning
   * - :hpo:key:`LIB <OPTIMIZER.LIB>`, :hpo:key:`ALGO <OPTIMIZER.ALGO>`
     - Required library and algorithm names.
   * - :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>`,
       :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>`,
       :hpo:key:`STOPVAL <OPTIMIZER.STOPVAL>`
     - Evaluation budget, time budget and target loss. See
       :doc:`nfp_stopping_settings`.
   * - :hpo:key:`FTOL_REL <OPTIMIZER.FTOL_REL>`,
       :hpo:key:`FTOL_ABS <OPTIMIZER.FTOL_ABS>`,
       :hpo:key:`XTOL_REL <OPTIMIZER.XTOL_REL>`,
       :hpo:key:`XTOL_ABS <OPTIMIZER.XTOL_ABS>`
     - Library-dependent convergence settings. See
       :doc:`nfp_stopping_settings`.
   * - :hpo:key:`POPULATION <OPTIMIZER.POPULATION>`
     - Population size for :hpo:opt:`TADAH:ANNEAL_PA` and NLopt's CRS2,
       ISRES, ESCH, MLSL and :hpo:opt:`NLOPT:GD_STOGO_RAND` algorithms.
       Other optimisers refuse it. Defaults and valid sizes depend on
       the algorithm.
   * - :hpo:key:`VECTOR_ARR <OPTIMIZER.VECTOR_ARR>`
     - NLopt stored-vector count for :hpo:opt:`NLOPT:LD_LBFGS`,
       :hpo:opt:`NLOPT:LD_VAR1`, :hpo:opt:`NLOPT:LD_VAR2` and the
       ``LD_TNEWTON_PRECOND`` algorithms. Other algorithms,
       ``LD_TNEWTON`` and ``LD_TNEWTON_RESTART`` included, refuse it.
   * - :hpo:key:`THREADS <OPTIMIZER.THREADS>`
     - The main optimiser's value sizes the shared worker pool for
       training, prediction and LAMMPS scripts. The pool retains this
       size during :hpo:block:`EXPLORE <BLOCK.EXPLORE>` and
       :hpo:block:`INNER <BLOCK.INNER>` searches. In an
       :hpo:block:`!EXPLORE <BLOCK.EXPLORE>` block that names
       :hpo:opt:`DLIB:GFS`, :hpo:opt:`ASA:ASA`,
       :hpo:opt:`TADAH:ANNEAL_PT` or :hpo:opt:`TADAH:ANNEAL_PA`,
       :hpo:key:`!THREADS <OPTIMIZER.THREADS>` sets how many evaluations
       that exploration asks for at once, and the default of
       :hpo:param:`REPLICAS <PARAM.TADAH:ANNEAL.REPLICAS>`
       (:hpo:opt:`!TADAH:ANNEAL_PT`) and of
       :hpo:key:`!POPULATION <OPTIMIZER.POPULATION>`
       (:hpo:opt:`!TADAH:ANNEAL_PA`).
       An :hpo:block:`!EXPLORE <BLOCK.EXPLORE>` block of another
       optimiser refuses it, and so does
       :hpo:block:`!INNER <BLOCK.INNER>` (see :doc:`nfp_inner`).
       A seeded run writes the same files at any
       :hpo:key:`!THREADS <OPTIMIZER.THREADS>` under the conditions of
       :ref:`nfp_evaluation_numbers`, except under :hpo:opt:`!DLIB:GFS`.
   * - :hpo:key:`SEED <OPTIMIZER.SEED>`
     - Seed of the optimiser's random numbers. See the seed rules below.
   * - :hpo:key:`STEP <OPTIMIZER.STEP>`
     - Initial step for supported NLopt algorithms; see below.
   * - :hpo:key:`GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>`
     - Gradient calculation; see :doc:`nfp_gradients`.
   * - :hpo:key:`PARAM <OPTIMIZER.PARAM>`
     - Algorithm-specific setting; see the parameter references below.

Startup checks each setting against the selected algorithm's
capabilities. The reference entry of each key gives its values, range
and default.

Initial step: ``STEP``
----------------------

:hpo:key:`STEP <OPTIMIZER.STEP>` is a fraction of the width of the box
of each search parameter, in the coordinates of the search. Selected
NLopt algorithms support it, including
:hpo:opt:`NLOPT:LN_BOBYQA`, :hpo:opt:`NLOPT:LN_COBYLA`,
:hpo:opt:`NLOPT:LN_SBPLX` and :hpo:opt:`NLOPT:LN_NELDERMEAD`.

A block-wide value can be combined with overrides:

.. code-block:: none

   STEP 0.1
   STEP CGRID2B(1-3) 0.05

An override without an index list selects all optimised indices of a
key; a list such as ``(1-4,7)`` selects those indices. Each selected
index must belong to the search. Without a block-wide
:hpo:key:`!STEP <OPTIMIZER.STEP>`, overrides must cover every search
parameter. An outer MLSL or AUGLAG
:hpo:key:`!STEP <OPTIMIZER.STEP>` overrides the step of its
:hpo:block:`INNER <BLOCK.INNER>` search. The effective steps are
reported in the run journal.

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
   that search parameter for the whole search. This applied to
   :hpo:opt:`!LN_BOBYQA <NLOPT:LN_BOBYQA>`. With
   :hpo:key:`!STEP <OPTIMIZER.STEP>` unset, it applied also to
   :hpo:opt:`!LN_COBYLA <NLOPT:LN_COBYLA>` and to either algorithm as
   the local search of an MLSL or AUGLAG.

Annealing sets its first proposal scale with
:hpo:param:`STEP_FRAC <PARAM.TADAH:ANNEAL.STEP_FRAC>`.
:hpo:opt:`TADAH:GRID` takes
:hpo:param:`GRID_POINTS <PARAM.TADAH:GRID.GRID_POINTS>` instead of a
step. The finite-difference step is set with
:hpo:param:`fd_step <PARAM.FD.fd_step>` (see :doc:`nfp_gradients`).

Seeds
-----

An optimiser's :hpo:key:`SEED <OPTIMIZER.SEED>` seeds the random
numbers of its search. :hpo:key:`SEED <INIT.SEED>` in
:hpo:block:`INIT <BLOCK.INIT>` and in
:hpo:block:`RESTART <BLOCK.RESTART>` seeds their starting-point methods
separately. An optimiser that draws random numbers takes the
:hpo:key:`!SEED <OPTIMIZER.SEED>` of its block; an NLopt algorithm next
takes the :hpo:key:`!SEED <INIT.SEED>` of its
:hpo:block:`!INIT <BLOCK.INIT>` block. :hpo:block:`INNER <BLOCK.INNER>`
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
   without :hpo:key:`!SEED <OPTIMIZER.SEED>` drew a new seed each time they
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

:hpo:opt:`TADAH:GRID`, :hpo:opt:`DLIB:BFGS` and
:hpo:opt:`CERES:STEEPEST_DESCENT` refuse :hpo:key:`!SEED <OPTIMIZER.SEED>`
because they draw no random numbers. :hpo:opt:`DLIB:GFS` refuses it because
dlib draws its random points from a fixed internal seed, which ``tadah hpo``
does not set. As a :hpo:key:`!PRESTAGE <STRATEGY.TYPE.PRESTAGE>` refinement,
each of them takes a :hpo:key:`!SEED <OPTIMIZER.SEED>` that the exploration
takes. An NLopt
algorithm that draws no random numbers, such as
:hpo:opt:`NLOPT:LN_SBPLX` or :hpo:opt:`NLOPT:GN_DIRECT_L`, refuses
:hpo:key:`!SEED <OPTIMIZER.SEED>` too. The
:ref:`optimiser reference <nfp_ref_optimisers>` says how each optimiser
reads :hpo:key:`!SEED <OPTIMIZER.SEED>`. A stochastic search and its
starting points repeat only with
:hpo:key:`!SEED <OPTIMIZER.SEED>` set; see :doc:`nfp_initialisation`
and :doc:`nfp_restart`.

.. versionchanged:: 1.6.0
   In 1.5.0 every NLopt algorithm accepted
   :hpo:key:`!SEED <OPTIMIZER.SEED>`.

.. _ceres-param-keys:

Algorithm parameters: ``PARAM``
-------------------------------

Parameter names keep the case shown by their reference. Each value is a
number, except the words of the finite-difference
:hpo:param:`fd_kernel <PARAM.FD.fd_kernel>` and of the annealing keys; a
word given to another key is refused.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Optimiser
     - Accepted parameter reference
   * - :hpo:opt:`TADAH:RANDOM`, :hpo:opt:`DLIB:GFS`
     - No :hpo:key:`PARAM <OPTIMIZER.PARAM>` settings.
   * - :hpo:opt:`TADAH:GRID`
     - :hpo:param:`GRID_POINTS <PARAM.TADAH:GRID.GRID_POINTS>`: the
       number of grid points of each search parameter.
   * - :hpo:opt:`DLIB:BFGS`
     - Finite-difference settings in :doc:`nfp_gradients`, and
       ``tol_gradinf`` in :doc:`nfp_stopping_settings`.
   * - :hpo:opt:`CERES:STEEPEST_DESCENT`
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
   * - :hpo:opt:`TADAH:ANNEAL`, :hpo:opt:`TADAH:ANNEAL_PA`
     - :doc:`anneal` and :doc:`anneal_pa`.
   * - :hpo:opt:`TADAH:ANNEAL_PT`
     - :doc:`anneal_pt`.
   * - :hpo:opt:`ASA:ASA`
     - :doc:`asa`.

:hpo:key:`LOG_HP <OPTIMIZER.LOG_HP>` and
:hpo:key:`OPTIM_SCALE <ROOT.OPTIM_SCALE>` configure the search
coordinates (see :doc:`nfp_search_space`).
