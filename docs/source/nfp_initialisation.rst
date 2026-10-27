.. _hpo_init_methods:

.. _init-block-seeding-strategies:

.. _chain-entries:

.. _how-many-seeds-does-the-optimiser-actually-consume:

.. _top-level-keys-inside-the-init-block:

Starting points: INIT
=====================

``INIT`` supplies starting points, in the order written. It can appear
inside ``OPTIMIZER``, ``STRATEGY``, ``EXPLORE`` or ``REFINE``. The first,
second and fourth locations configure the main optimiser's same
initialisation settings. ``EXPLORE`` has its own settings.

.. code-block:: none

   INIT
     METHOD CONFIG
     METHOD LHS 4
     K_TOTAL 3
     SEED 123
   ENDINIT

Methods
-------

Repeat ``METHOD`` to concatenate methods into one chain.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Entry
     - Points produced
   * - ``METHOD CONFIG``
     - One point from the selected parameter values in the training
       configuration. No arguments.
   * - ``METHOD RANDOM K``
     - ``K`` independent uniform samples in transformed search
       coordinates; integer ``K >= 1``.
   * - ``METHOD LHS K``
     - ``K`` Latin-hypercube samples in transformed coordinates;
       integer ``K >= 1``.
   * - ``METHOD SOBOL K``
     - ``K`` Sobol sequence points mapped through the search transform;
       integer ``K >= 1``. The seed selects a block of ``K`` points that
       starts at a multiple of ``K``: a ``K`` that is a power of 2 puts
       one point in each 1/``K`` of every range, in search coordinates.
   * - ``METHOD WARM file``
     - One point read from the named potential; only selected ``OPTIM``
       values are taken. See the mismatch policy below.

An absent ``INIT`` block, or an empty method chain, supplies one ``CONFIG``
point; inside ``EXPLORE`` it leaves the exploration the main starting
points. An explicit chain supplies only its declared methods. Near-duplicate
points in a chain are removed in transformed coordinates, retaining the
first occurrence. ``METHOD NOISE_AUTO`` is reserved and rejected.

Other keys
----------

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Key
     - Values and effect
   * - ``K_TOTAL K``
     - Integer >= 1. The ``INIT`` blocks inside ``OPTIMIZER``,
       ``STRATEGY`` and ``REFINE`` make one chain, and those inside
       ``EXPLORE`` another; ``K_TOTAL`` keeps the first ``K`` points of
       its chain, a single method included. A value at or above the
       points the methods give (``CONFIG`` and ``WARM`` give one each)
       caps nothing and is refused. An ``INIT`` block inside ``EXPLORE``
       without a method refuses it. Omission keeps every point.
   * - ``SEED n``
     - Unsigned 64-bit seed for the sampling methods. Omission draws a
       seed from ``std::random_device``. This is separate from the
       optimiser's seed, apart from NLopt's documented fallback.
   * - ``WARM_ON_MISMATCH policy``
     - ``warn`` (default), ``error`` or ``skip``, lower case. Applies to
       missing selected parameters and values outside the current bounds.

.. versionchanged:: 1.6.0
   In 1.5.0 ``K_TOTAL`` replaced the ``K`` of a single ``RANDOM``,
   ``LHS`` or ``SOBOL`` line, and a :hpo:key:`SOBOL <METHOD.SOBOL>` seed
   ``s`` skipped ``s`` points, so a block was evenly spread only when
   ``s + 1`` was a multiple of ``K``.

``STRATEGY`` remains an accepted alias for ``METHOD`` inside ``INIT``;
use ``METHOD`` in new files to distinguish a method line from the
``STRATEGY ... ENDSTRATEGY`` block.

.. _reading-a-warm-seed:

Warm seeds
----------

``WARM`` reads the values selected by this run's ``OPTIM`` lines,
including named component paths. Other model settings come from the
current training configuration. ``WARM`` starts a fresh optimiser search
from those parameter values.

With ``warn``, missing values use their current configuration values and
out-of-bounds values are clamped, with diagnostics. With ``error``,
either mismatch stops setup. With ``skip``, the affected seed point is
dropped. A missing or unreadable file always stops setup, regardless of
this policy. Descriptor-specific compatibility checks may also reject a
seed independently of the mismatch policy.

The number of points an optimiser uses depends on its capabilities and
:doc:`nfp_strategies`. For example, ``DLIB:GFS`` accepts several seeds in
one search; ``MULTISTART`` runs eligible optimisers from separate points.
Refinement normally starts from the exploration's selected candidates.
