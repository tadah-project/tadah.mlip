.. _search_space_constraints:

.. _hpo_dimension_order:

.. _search_space_scaling:

.. _hpo_fixweight:

.. _optim-search-space-constraints:

.. _bound-transforms:

.. _pinning-and-searching-linear-coefficients:

Search parameters and bounds
============================

``OPTIM`` selects continuous values from the training configuration or
potential supplied with ``--config``. Each selected value becomes one
dimension of the outer search. Unselected values retain their configured
values, except model coefficients that the inner fit solves.

Explicit bounds
---------------

.. code-block:: none

   OPTIM <KEY> (indices) <low> <high> [LIN | LOG | ASINH <c>]

For example, with at least three pair-basis centres and scales:

.. code-block:: none

   OPTIM CGRID2B (1-3) 1.0 5.0
   OPTIM SGRID2B (1-3) 0.5 2.0 LOG

The bounds must be finite, with ``high > low``. The key after ``OPTIM``
is case-insensitive. Selectors are one-based: ``(2)``, ``(1,4,7)`` and
``(1-7:2)`` select one value, a list, and a strided range. Repeated
``OPTIM`` lines can select different indices of the same key; selecting
the same index twice is an error.

For keys containing species labels as well as numbers, indices select
the numeric values. ``FIXWEIGHT`` has its own indexing rule below.
Discrete choices, integer-only values, internal derived keys and
inactive parameters are rejected by ``OPTIM``. Examples include descriptor
families, density forms, ``FIXINDEX``, ``SBASIS`` and ``OALGO (1)``.

.. _bounds-from-the-configured-values:

Automatic bounds
----------------

.. code-block:: none

   OPTIM <KEY> (indices) AUTO [RATIO <f> | FRAC <p>] \
       [FLOOR <a>] [GROUP_FLOOR <r>] [LIN | LOG | ASINH <c>]

``AUTO`` derives each box from its value ``x0`` in ``--config``, including
runs that obtain their starting point through ``METHOD WARM``.

.. list-table:: AUTO options
   :header-rows: 1
   :widths: 25 75

   * - Option
     - Meaning
   * - ``RATIO f``
     - Bounds are the smaller and larger of ``x0/f`` and ``x0*f``.
       Requires ``f > 1`` and a nonzero seed. Default: ``RATIO 2``
       for positive scale keys, including component eta and widths.
   * - ``FRAC p``
     - Bounds are ``x0 - d`` and ``x0 + d``, where
       ``d = max(p*abs(x0), a, r*group_max)``. Requires ``p >= 0``.
       Default for other keys: ``FRAC 0.5``.
   * - ``FLOOR a``
     - Absolute minimum half-width for ``FRAC``; ``a >= 0``.
       Use a positive value when every selected seed is zero.
   * - ``GROUP_FLOOR r``
     - Relative minimum half-width for ``FRAC``; ``r >= 0``.
       ``group_max`` is the largest absolute seed selected by this line.

``RATIO`` and ``FRAC`` are mutually exclusive. A zero-width box is an
error. To give all selected centres the same absolute half-width, use
``AUTO FRAC 0 FLOOR 0.1``. ``FLOOR`` and ``GROUP_FLOOR`` apply to
``FRAC`` boxes only: with ``RATIO``, given or the default of a positive
scale key, they are refused.

.. versionchanged:: 1.6.0
   ``FLOOR`` and ``GROUP_FLOOR`` with ``RATIO`` are refused. 1.5.0
   accepted them and ignored them.

For nonnegative density amplitudes, ``FRAC`` bounds are clipped at zero.
Without either floor option these amplitudes receive
``GROUP_FLOOR 0.001``; if the whole group is zero, supply ``FLOOR`` or
explicit bounds. ``RATIO`` requires positive seeds for such amplitudes.
Legacy keys beginning with ``RCUT`` require explicit bounds.

Coordinate scaling
------------------

The trailing shape tag determines the coordinate used by the optimiser:

.. list-table:: Shape tags
   :header-rows: 1
   :widths: 25 75

   * - Tag
     - Coordinate
   * - ``LIN``
     - ``x``; the default shape.
   * - ``LOG``
     - ``ln(x)``; requires a positive lower bound.
   * - ``ASINH c``
     - ``asinh(x/c)``; requires finite ``c > 0`` and supports
       ranges that cross zero.

At the root level, ``OPTIM_SCALE AUTO`` maps these transformed bounds
to ``[0, 1]`` for each dimension. This is the default.
``OPTIM_SCALE NONE`` retains the transformed bounds at their original
scale, including any ``LOG`` or ``ASINH`` transform.

``LOG_HP ratio`` is an optional key inside ``OPTIMIZER`` or ``STRATEGY``.
It converts linear dimensions to logarithmic ones when their lower bound
is positive and ``high/low >= ratio``; ``ratio`` must be finite and at
least one. It converts only untagged dimensions: a dimension tagged
``LIN``, ``LOG`` or ``ASINH`` keeps the shape its tag gives, and
``hpo_optim_resolved.hpo`` keeps the tag.

The bounds and values written in ``params.tadah`` use the selected
physical coordinates. Scaling applies to the optimiser's coordinates;
reported parameter columns retain their physical units.

Component parameters
--------------------

A :doc:`DM_Embed <dm_embed>` block exposes named parameter paths. Replace
``ta`` below with the descriptor's ID:

.. list-table:: Searchable component paths
   :header-rows: 1
   :widths: 55 45

   * - Path
     - Availability
   * - ``ta.density.centres``
     - Every density basis.
   * - ``ta.density.eta`` or ``ta.density.widths``
     - The scale coordinate declared in a bump or Gaussian density
       component.
   * - ``ta.density.amplitudes``
     - Every density basis; bounds must be nonnegative for
       ``NONNEG`` and ``MONOTONE`` forms.
   * - ``ta.density.cutoff.radius``
     - A smooth spatial cutoff; unavailable for ``NONE``.
   * - ``ta.embedding.centres``
     - Every embedding basis.
   * - ``ta.embedding.eta`` or ``ta.embedding.widths``
     - The scale coordinate declared in a bump or Gaussian embedding
       component.
   * - ``ta.embedding.envelope.cutoff``
     - An enabled envelope; unavailable for ``OFF``.

Scale and cutoff bounds must be positive. ``WIDTHS`` remains the
authoritative coordinate when the component declares it: bounds,
derivatives and saved values use widths. An envelope cutoff is a density
value. Spatial neighbour lists use interaction radii.

.. code-block:: none

   OPTIM ta.density.centres (1-3) AUTO FRAC 0.1 FLOOR 0.02
   OPTIM ta.embedding.widths (1-4) AUTO RATIO 1.2
   OPTIM ta.density.cutoff.radius (1) 5.5 6.5

The example requires matching component arrays, ``WIDTHS`` in the
embedding component and a smooth density cutoff. Named paths are required
for component parameters; legacy many-body keys such as ``CGRIDMB`` apply
to legacy descriptors.

Pinned coefficients
-------------------

Pinned coefficients remain fixed during the inner fit. ``OPTIM FIXWEIGHT``
selects them for the outer search. With all coefficients pinned, evaluation
proceeds directly from the selected weights. The refit workflow supports
both pinned-coefficient searches and re-solving coefficients from training
data; see :ref:`DM_Embed refit workflows <dm_embed_refit>`.

``OPTIM WEIGHTS`` is rejected. To search model coefficients, pin them
with ``FIXINDEX`` and ``FIXWEIGHT`` in ``--config``, then use
``OPTIM FIXWEIGHT``. Its indices name model weights:

.. code-block:: none

   # In the training configuration:
   FIXINDEX 5 2
   FIXWEIGHT 0.3 -0.1

   # In the hpotarget:
   OPTIM FIXWEIGHT (5) -1.0 1.0

This searches weight 5, initially ``0.3``, labelled ``FIXWEIGHT[5]`` in
the result files. Weight 2 remains pinned at ``-0.1``. Each searched
weight must appear in ``FIXINDEX``.

.. _inspecting-the-resolved-space:

Resolved search space
---------------------

``hpo_search_space.tsv`` lists each dimension, its bounds, seed, shape
and optimiser coordinates. If any box uses ``AUTO``,
``hpo_optim_resolved.hpo`` also contains explicit ``OPTIM`` lines with
round-trip precision, suitable for reusing the resolved bounds.

Dimensions are ordered alphabetically by uppercase key, then by their
position within that key. For ``FIXWEIGHT`` the position is the slot in
``FIXINDEX``. This order is independent of ``OPTIM`` declaration order.
Use the TSV's ``dim`` column when an optimiser option takes a dimension
number.
