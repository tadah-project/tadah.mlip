.. _hpo_fd_stencil:

.. _hpo_out_of_box:

.. _hpo_gradient_not_finite:

Gradients and finite differences
================================

``GRAD_STRATEGY`` controls how a gradient-based optimiser obtains the
derivative of the complete loss. It belongs to the optimiser requesting
that gradient: for NLopt MLSL/AUGLAG, the enclosing optimiser owns this
setting.

Gradient strategies
-------------------

.. list-table::
   :header-rows: 1
   :widths: 24 76

   * - Value
     - Meaning
   * - ``NUMERICAL``
     - Finite differences of complete loss evaluations; the usual
       default.
   * - ``ANALYTICAL``
     - Analytical derivatives only. Unsupported settings or selected
       parameters are refused.
   * - ``HYBRID``
     - Analytical derivatives where available, finite differences for
       the remaining parameters. If no analytical derivative can be
       formed, the numerical calculation covers every parameter.

With ``PC_OBSERVABLE``, an omitted strategy resolves to ``ANALYTICAL``
in phases that obtain gradients; explicit ``NUMERICAL`` is rejected
with that loss term. Supplying ``GRAD_STRATEGY`` to an
optimiser that never requests gradients is an error. The old value
``AUTODIFF`` is rejected.

Analytical availability depends on the model, fit, loss terms and chosen
optimiser. Setup validates derivative support for the selected combination. ``HYBRID`` is the explicit choice when numerical
fallback is acceptable.

The current analytical fit path requires a training ``DBFILE``,
``OALGO 4``, fixed ``LAMBDA >= 0``, and ``BF_Linear`` or ``Kern_Linear``.
It rejects ``FIXINDEX``, ``STRESS true``, ``NORM true`` and ``PC_SRMSE``.
Each selected descriptor parameter and observable must also supply a
supported derivative. These requirements apply to the analytical fit path.

Finite-difference keys
----------------------

These case-sensitive ``PARAM`` names apply to ``NUMERICAL`` and the
numerical part of ``HYBRID``:

.. list-table::
   :header-rows: 1
   :widths: 28 20 52

   * - Setting
     - Default
     - Values
   * - ``PARAM fd_step s``
     - ``1e-6``
     - Positive finite scalar controlling the step rule below.
   * - ``PARAM fd_kernel name``
     - Depends on mode and scaling; see below.
     - ``inhouse`` or ``ceres``; their all-capital forms are also
       accepted. Selects the finite-difference step rule.
   * - ``PARAM diff_method n``
     - ``0``
     - ``0`` for central differences; ``1`` for forward differences.
       Other values, including ``2`` (Ridders), are rejected.

Without ``PARAM fd_kernel``, the rule is ``inhouse`` for ``HYBRID`` or
normalised coordinates (``OPTIM_SCALE AUTO``), and ``ceres`` for
``NUMERICAL`` with ``OPTIM_SCALE NONE``. The ``fd_step`` default remains
``1e-6`` and the scheme remains central in either case. ``NOISE AUTO``
can derive a different ``fd_step``; see :doc:`nfp_noise`.

These keys are rejected where no finite differences are taken, including
``GRAD_STRATEGY ANALYTICAL`` and ``INNER`` blocks. ``STEP`` controls an
optimiser's initial search step and is a separate setting.

Steps and boundaries
--------------------

For internal optimiser coordinate ``u`` and configured ``s = fd_step``:

* ``inhouse`` uses ``h = s * max(abs(u), 1)``.
* ``ceres`` uses ``h = max(2**(-26), abs(u) * s)``.

With ``OPTIM_SCALE AUTO``, coordinates lie in the unit box, so the
in-house step is a fraction of each dimension's transformed width.
Without normalisation, it is absolute for ``abs(u) <= 1`` and relative
above that. The Ceres rule always has its absolute floor.

Central differences use ``u-h`` and ``u+h`` where possible. Near a bound,
they use a second-order, three-point one-sided stencil inside the box.
Forward differences use one extra point and reverse direction near the
upper bound. The configured step rule is preserved at the boundary.

Setup rejects a box in which the requested stencil cannot fit at every
point. With unit-box normalisation, the in-house limits are
``fd_step <= 1/3`` for central differences and ``<= 1/2`` for forward
differences. The Ceres-rule limits there are ``1/2`` and ``1``.
:doc:`nfp_noise` describes the optional automatic step heuristic.

Cost and unusable gradients
---------------------------

A central gradient normally needs two additional loss evaluations per
numerical dimension; forward differences need one. The loss at the
centre is reused when supplied by the optimiser. Analytical gradient
calls also count toward ``MAXEVAL``. A small budget can therefore end a
search before its first parameter step.

A failed loss at the point or unusable finite-difference stencil can
leave the search without a usable gradient. Failure grading can supply
numerical slopes within a graded failure region. Each stencil requires
losses from a consistent usable or failed region. This requirement also
applies to the numerical part of ``HYBRID``.

With ``PC_LAMMPS``, gradient-based NLopt and Ceres searches are refused;
``DLIB:BFGS`` is the supported gradient-based backend. With this loss
term, omit ``GRAD_STRATEGY``: every explicit value is rejected, including
on ``DLIB:BFGS``. MLSL/AUGLAG with
a derivative-free local search is allowed. ``PC_OBSERVABLE`` supplies
its own analytical derivative path and has separate compatibility rules.
