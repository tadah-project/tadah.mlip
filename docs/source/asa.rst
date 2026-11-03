.. _asa_optimizer:

Adaptive Simulated Annealing (ASA)
==================================

This page shows how to run a search with the Adaptive Simulated Annealing
(ASA) optimiser and lists its ``PARAM`` keys. It assumes a target file with
an ``OPTIMIZER`` block (see :doc:`nfp_optimizer`).

``LIB ASA`` and ``ALGO ASA`` select the bundled optimiser. It uses separate
parameter temperatures and a cost temperature. The block below runs it as
the optimiser of ``STRATEGY``:

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

At least one search parameter (``OPTIM`` line) is required. ASA starts at
the first starting point of the ``INIT`` block. Without an ``INIT`` block,
the values of the training configuration are the starting point. If no
starting point remains, ASA draws a random start. A starting point whose
loss ASA does not take (at or above :hpo:key:`FAILSCORE`, about 1e18 or more,
or not finite) stays ASA's first state, with that loss capped at about 1e18.
Every loss ASA takes is lower, so the first state that ASA generates after
it with a usable loss becomes ASA's best; ``hpo_run.log`` names the failed
start. ``SEED`` controls the random stream; without it, the seed is
non-deterministic.

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
search parameters regardless of line order. Indices are 1-based and match
the ``dim`` column of ``hpo_search_space.tsv``. ``Sequential_Parameters``
is a separate scalar option with a zero-based value.

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

Setup rejects ASA under ``TYPE MULTISTART`` or refinement of ``TOPK > 1``
candidates under ``TYPE PRESTAGE``. It can run as the exploration phase,
as a single refinement, or with a ``RESTART`` block. A run of ``tadah hpo``
on more than one MPI rank takes only ``TADAH:ANNEAL_PT`` or
``TADAH:ANNEAL_PA`` and no ``RESTART`` block, so ASA runs on one rank.

.. _asa_probes_box:

Probe bounds
------------

ASA checks tangent and curvature probes against the optimiser's
internal parameter bounds. A ``Delta_X`` or indexed
``User_Delta_Parameter`` whose probes can leave a box is refused at
startup, with the affected search parameter and a fitting step size.

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
``MAXTIME`` and ``STOPVAL`` act at block boundaries. ASA's periodic test,
in which it reanneals and checks whether the cost repeats, then follows
each block in which the count of accepted states reaches a multiple of
:hpo:param:`Acceptance_Frequency_Modulus <PARAM.ASA:ASA.Acceptance_Frequency_Modulus>`,
or the count of generated states a multiple of
:hpo:param:`Generated_Frequency_Modulus <PARAM.ASA:ASA.Generated_Frequency_Modulus>`.
:doc:`asa_extended_options` lists the block parameters and their
defaults, and :doc:`asa_options` the two moduli.

.. versionchanged:: 1.6.0
   In 1.5.0 a block ran a periodic test only when its last state reached
   such a multiple, so block runs made few of their periodic tests.
