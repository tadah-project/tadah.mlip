.. _asa_optimizer:

Adaptive Simulated Annealing (ASA)
==================================

Select ``LIB ASA`` and ``ALGO ASA`` for the bundled Adaptive Simulated
Annealing optimiser. It uses separate parameter temperatures and a cost
temperature. Configure it in the phase that should run the search:

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB ASA
       ALGO ASA
       MAXEVAL 200
       SEED 1234
       PARAM Temperature_Ratio_Scale 1e-4
     ENDSTRATEGY
   ENDOPTIMIZER

At least one ``OPTIM`` dimension is required. ASA takes the first
resolved ``INIT`` point as its seed. The default ``CONFIG`` seed counts
as such a point. If the resolved chain has no point, ASA generates a
random start. ``SEED`` controls its random stream; omission uses a
non-deterministic seed.

Option reference
----------------

.. toctree::
   :maxdepth: 1

   asa_options
   asa_extended_options

``PARAM`` names are case-sensitive and unknown names are errors.
All values must be finite. Integer-valued ``PARAM`` options accept a
number with an integral value, including ``1e5``; this differs from
integer block keys such as ``MAXEVAL``, which require decimal digits.

Options marked **indexed** accept both a uniform value and overrides:

.. code-block:: none

   PARAM User_Quench_Param_Scale 0.9
   PARAM User_Quench_Param_Scale[3] 0.7

The uniform value is applied first; indexed values override individual
dimensions regardless of line order. Indices are 1-based and match the
``dim`` column of ``hpo_search_space.tsv``, not ``OPTIM`` declaration
order. ``Sequential_Parameters`` is a separate scalar option with a
zero-based value.

Search controls
---------------

ASA honours ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``SEED`` and
``THREADS``. The evaluation cap includes ASA's initial cost-temperature
sampling. ASA also has its own stopping parameters, listed on
:doc:`asa_options`. With no ``MAXEVAL``, those limits and any other
configured stop still apply.

``FTOL_REL``, ``FTOL_ABS``, ``XTOL_REL``, ``XTOL_ABS``, ``POPULATION``,
``STEP``, ``GRAD_STRATEGY``, ``VECTOR_ARR`` and ``INNER`` are unsupported.
ASA's tangent probes are controlled by its own ``Delta_X`` options,
independently of the common finite-difference gradient settings.

ASA cannot run under ``TYPE MULTISTART`` or refine ``TOPK > 1``
candidates under ``TYPE PRESTAGE``. It can run as the exploration phase,
as a single refinement, or under a ``RESTART`` decorator. Multi-rank
HPO launches support the PT/PA optimisers rather than ASA.

.. _asa_probes_box:

Probe bounds
------------

ASA checks tangent and curvature probes against the optimiser's
internal parameter bounds. A ``Delta_X`` or indexed
``User_Delta_Parameter`` whose probes can leave a box is refused at
startup, with the affected dimension and a fitting step size.

With ``OPTIM_SCALE AUTO``, tangent probes fit the unit box for
``Delta <= 1``. When ``Curvature_0`` is ``0`` or ``-1``, curvature probes
require ``Delta <= 0.5``. With ``OPTIM_SCALE NONE``, the permitted step
depends on each box and its distance from zero. These checks still
apply to exit tangents when ``Reanneal_Parameters`` is disabled.

Block evaluation
----------------

``Gener_Block`` sets the initial number of generated trial states per
block; ``Gener_Block_Max`` caps adaptive resizing. A block can use up
to ``min(THREADS, Gener_Block_Max)`` workers. Increasing the block size
changes ASA's sampled sequence. ``Gener_Mov_Avr 0`` keeps the block
size fixed; otherwise it can shrink below its initial value.

``MAXEVAL`` remains an exact evaluation cap. With multi-state blocks,
``MAXTIME`` and ``STOPVAL`` act at block boundaries. See
:doc:`asa_extended_options` for these parameters and their defaults.
