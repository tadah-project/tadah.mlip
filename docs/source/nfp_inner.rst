.. _hpo_inner_block:

.. _inner-block-nlopt-mlsl-auglag-sub-optimiser:

NLopt local searches: INNER
===========================

``INNER`` configures the local optimiser used inside NLopt's MLSL or
AUGLAG algorithms. It can appear inside ``STRATEGY``, ``EXPLORE`` or
``REFINE``, where the enclosing optimiser accepts it.

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB NLOPT
     ALGO G_MLSL_LDS
     MAXEVAL 500
     INNER
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 50
       XTOL_REL 1e-5
     ENDINNER
   ENDSTRATEGY

Keys and inheritance
--------------------

Set both ``LIB NLOPT`` and a local ``ALGO``. Global algorithms are
rejected. An ``INNER`` algorithm must run with a single level of local
search; algorithms requiring a further explicit ``INNER`` are rejected.

The following common optimiser keys are accepted at the grammar level;
algorithm-specific validation still applies:

* ``LIB``, ``ALGO``;
* ``MAXEVAL``, ``MAXTIME``, ``FTOL_REL``, ``FTOL_ABS``, ``XTOL_REL``,
  ``XTOL_ABS``;
* ``POPULATION``, ``STEP``, ``VECTOR_ARR``, ``PARAM``.

See :doc:`nfp_optimizer_settings` and :doc:`nfp_stopping_settings` for
values. Four keys belong to the enclosing optimiser instead:

* ``SEED``: the local search shares its random stream.
* ``GRAD_STRATEGY``: the enclosing optimiser obtains the gradient that
  the local search requests.
* ``STOPVAL``: NLopt replaces the local search's target with the
  enclosing target.
* ``THREADS``: the ``STRATEGY``-level optimiser's value sets the workers
  of the run, and ``INNER`` refuses it.

Finite-difference ``PARAM fd_step``, ``fd_kernel`` and ``diff_method``
also belong to the enclosing optimiser and are rejected in ``INNER``.

The local budget limits each local search; the enclosing budget limits
the whole run. An explicit ``INNER`` has independently configured
tolerances. A parent ``STEP`` overrides the local search's step. The main
optimiser's ``THREADS`` sizes the shared worker pool.

.. _when-inner-is-required:

Required and default local searches
-----------------------------------

``G_MLSL``, ``G_MLSL_LDS``, ``AUGLAG`` and ``AUGLAG_EQ`` require an
explicit ``INNER``. Their ``GN_MLSL*`` and ``LN_AUGLAG*`` variants use
``LN_COBYLA`` by default; ``GD_MLSL*`` and ``LD_AUGLAG*`` use ``LD_MMA``.
An explicit block replaces those defaults.

Whether an MLSL/AUGLAG run needs gradients is determined by this local
search. See :doc:`nfp_gradients` for gradient and loss-term compatibility.
