.. _inner-block-nlopt-mlsl-auglag-sub-optimiser:

NLopt Local Searches: INNER
===========================

This page shows how the local search run by NLopt's multi-level
single-linkage (MLSL) and augmented Lagrangian (AUGLAG) algorithms is
configured. It assumes the ``OPTIMIZER`` block
(:doc:`nfp_optimizer_settings`) and the ``STRATEGY`` block
(:doc:`nfp_strategies`).

``INNER`` configures that local optimiser. It can appear inside
``STRATEGY``, ``EXPLORE`` or ``REFINE``, where the enclosing optimiser
accepts it.

.. code-block:: none

   STRATEGY
     LIB NLOPT
     ALGO G_MLSL_LDS
     MAXEVAL 500
     INNER
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 50
     ENDINNER
   ENDSTRATEGY

Keys and inheritance
--------------------

An ``INNER`` block needs both ``LIB NLOPT`` and a local ``ALGO``. Global
algorithms are refused. An ``INNER`` algorithm must run with a single
level of local search; algorithms requiring a further explicit ``INNER``
are refused.

The following common optimiser keys are accepted; algorithm-specific
validation still applies:

* ``LIB``, ``ALGO``;
* ``MAXEVAL``, ``MAXTIME``, ``FTOL_REL``, ``FTOL_ABS``, ``XTOL_REL``,
  ``XTOL_ABS``;
* ``POPULATION``, ``STEP``, ``VECTOR_ARR``, ``PARAM``.

The values of these keys are described in
:doc:`nfp_optimizer_settings` and :doc:`nfp_stopping_settings`. Four
keys belong to the enclosing optimiser instead:

* ``SEED``: the local search shares its random stream.
* ``GRAD_STRATEGY``: the enclosing optimiser obtains the gradient that
  the local search requests.
* ``STOPVAL``: NLopt replaces the local search's target with the
  enclosing target.
* ``THREADS``: the ``STRATEGY``-level optimiser's value sets the workers
  of the run, and ``INNER`` refuses it.

Finite-difference ``PARAM fd_step``, ``fd_kernel`` and ``diff_method``
also belong to the enclosing optimiser and are refused in ``INNER``.

The local budget limits each local search; the enclosing budget limits
the whole search of the enclosing optimiser. An explicit ``INNER`` has
independently configured tolerances. A parent ``STEP`` overrides the
local search's step.

.. _when-inner-is-required:

Required and default local searches
-----------------------------------

``G_MLSL``, ``G_MLSL_LDS``, ``AUGLAG`` and ``AUGLAG_EQ`` require an
explicit ``INNER``. Their ``GN_MLSL*`` and ``LN_AUGLAG*`` variants use
``LN_COBYLA`` by default; ``GD_MLSL*`` and ``LD_AUGLAG*`` use ``LD_MMA``.
An explicit block replaces those defaults.

Whether an MLSL or AUGLAG search needs gradients depends on this local
search. Gradient and loss-term compatibility is described in
:doc:`nfp_gradients`.
