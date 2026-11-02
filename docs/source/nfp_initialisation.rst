.. _nfp_initialisation:

Starting Points: INIT
=====================

This page describes how an ``INIT`` block sets the starting points of a
search. It assumes a target file with an :hpo:block:`OPTIMIZER` block (see
:doc:`nfp_configuration` and :doc:`nfp_optimizer`).

``INIT`` supplies starting points, in the order written. It can appear
inside ``OPTIMIZER``, ``STRATEGY``, ``EXPLORE`` or ``REFINE``. The
``INIT`` blocks inside ``OPTIMIZER``, ``STRATEGY`` and ``REFINE`` set the
starting points of the main optimiser together, as one chain. The
``INIT`` block inside ``EXPLORE`` sets the starting points of the
exploration.

.. code-block:: none

   INIT
     METHOD CONFIG
     METHOD LHS 4
     K_TOTAL 3
     SEED 123
   ENDINIT

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _hpo_init_methods:

Methods
-------

.. _chain-entries:

Repeated ``METHOD`` lines form one chain, in the order of the file.

.. _init-block-seeding-strategies:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Entry
     - Points produced
   * - :hpo:key:`METHOD CONFIG <METHOD.CONFIG>`
     - One point from the selected parameter values in the training
       configuration. No arguments.
   * - :hpo:key:`METHOD RANDOM K <METHOD.RANDOM>`
     - ``K`` independent uniform samples in search coordinates; integer
       ``K >= 1``.
   * - :hpo:key:`METHOD LHS K <METHOD.LHS>`
     - ``K`` Latin-hypercube samples in search coordinates; integer
       ``K >= 1``.
   * - :hpo:key:`METHOD SOBOL K <METHOD.SOBOL>`
     - ``K`` Sobol sequence points mapped through the search transform;
       integer ``K >= 1``. The seed selects a block of ``K`` points that
       starts at a multiple of ``K``: a ``K`` that is a power of 2 puts
       one point in each 1/``K`` of every range, in search coordinates.
   * - :hpo:key:`METHOD WARM file <METHOD.WARM>`
     - One point read from the named potential; only selected ``OPTIM``
       values are taken. ``WARM_ON_MISMATCH`` sets the response to a
       mismatch (:ref:`reading-a-warm-seed`).

An absent ``INIT`` block, or an empty method chain, supplies one ``CONFIG``
point; inside ``EXPLORE`` it leaves the exploration the main starting
points. An explicit chain supplies only its declared methods. Near-duplicate
points in a chain are removed in search coordinates, retaining the
first occurrence.

.. _top-level-keys-inside-the-init-block:

Other keys
----------

:hpo:key:`K_TOTAL <INIT.K_TOTAL>`
   Caps a chain at its first ``K_TOTAL`` points, a single method
   included. The ``INIT`` blocks inside ``OPTIMIZER``, ``STRATEGY`` and
   ``REFINE`` make one chain, those inside ``EXPLORE`` another, and the
   ``RESTART`` blocks a third. A value at or above the points the methods
   give (the ``CONFIG`` lines give one point, and so do the ``WARM``
   lines of one file) caps nothing and is refused. An ``INIT`` block
   inside ``EXPLORE`` without a method refuses it too. Without the key,
   every point is kept.

:hpo:key:`SEED <INIT.SEED>`
   Seeds the sampling methods. Without it, a chain that draws its points
   draws them with a seed drawn once for the run, which standard output
   and the journal give: ``SEED`` with that value draws the same points
   again. This seed is separate from the ``SEED`` of the optimiser,
   except that an NLopt optimiser that draws random numbers and has no
   ``SEED`` of its own takes this one.

:hpo:key:`WARM_ON_MISMATCH <INIT.WARM_ON_MISMATCH>`
   Sets the response of the ``WARM`` lines to a missing selected
   parameter or a value outside the current bounds (see
   :ref:`reading-a-warm-seed`).

.. versionchanged:: 1.6.0
   In 1.5.0 ``K_TOTAL`` replaced the ``K`` of a single ``RANDOM``,
   ``LHS`` or ``SOBOL`` line, and a :hpo:key:`SOBOL <METHOD.SOBOL>` seed
   ``s`` skipped ``s`` points, so a block was evenly spread only when
   ``s + 1`` was a multiple of ``K``.

Inside ``INIT``, a ``STRATEGY`` line is read as a ``METHOD`` line.
``METHOD`` keeps a method line distinct from the
``STRATEGY ... ENDSTRATEGY`` block.

.. _reading-a-warm-seed:

Warm seeds
----------

``WARM`` reads the values selected by this run's ``OPTIM`` lines,
including named component paths (:ref:`dm_embed_parameters`). Other
model settings come from the
current training configuration. ``WARM`` starts a fresh optimiser search
from those parameter values.

With ``warn``, missing values use their current configuration values and
out-of-bounds values are clamped, with diagnostics. With ``error``,
either mismatch stops setup. With ``skip``, the affected seed point is
dropped. A missing or unreadable file always stops setup, regardless of
this policy. Descriptor-specific compatibility checks may also reject a
seed independently of the mismatch policy.

A ``WARM`` file that is the run's own ``best_pot.tadah`` gives a startup
warning. The run reads it at startup, then removes it before the first
evaluation and writes its best potential there: only a copy under
another name keeps the seed for a later run. Inside ``RESTART``, whose
``WARM`` points are read at each round, such a file gives each round the
best potential written so far. There a ``best_pot.tadah`` that is a
symbolic link to another file gives the warning too, as the run replaces
the link.

.. _how-many-seeds-does-the-optimiser-actually-consume:

Points a search evaluates
-------------------------

The number of points an optimiser uses depends on its capabilities and
:doc:`nfp_strategies`. For example, ``DLIB:GFS`` accepts several starting
points in one search; ``MULTISTART`` runs eligible optimisers from
separate points. Refinement normally starts from the exploration's
selected candidates.

Points that a search, a ``MULTISTART`` or a ``PRESTAGE`` exploration does
not evaluate give a startup warning that names the ``INIT`` block and the
setting that would evaluate them. An ``INIT`` block of the main optimiser
(inside ``OPTIMIZER``, ``STRATEGY`` or ``REFINE``) beside an ``INIT``
block of the ``EXPLORE`` block gives its points to the refinement only
when no evaluation of the exploration has a usable loss. A startup
warning says so.
