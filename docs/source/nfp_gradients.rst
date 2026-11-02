.. _nfp_gradients:

Gradients and Finite Differences
================================

This page describes how a gradient-based optimiser of ``tadah hpo``
obtains the gradient of the loss and how the finite-difference steps are
set. It assumes a target file with an :hpo:block:`OPTIMIZER` block (see
:doc:`nfp_optimizer`).

``GRAD_STRATEGY`` controls how a gradient-based optimiser obtains the
derivative of the complete loss. It belongs to the optimiser requesting
that gradient: for the NLopt multi-level single-linkage (MLSL) and
augmented Lagrangian (AUGLAG) optimisers, the enclosing optimiser owns
this setting.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Gradient strategies
-------------------

The values of :hpo:key:`GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>` and its
default are listed in its reference entry. ``ANALYTICAL`` takes
analytical derivatives only and refuses unsupported settings or selected
parameters. ``HYBRID`` takes analytical derivatives where available and
finite differences for the remaining parameters. If no analytical
derivative can be formed, the finite differences cover every parameter.

With ``PC_OBSERVABLE``, an omitted strategy resolves to ``ANALYTICAL``
in phases that obtain gradients; explicit ``NUMERICAL`` is rejected
with that loss term. Supplying ``GRAD_STRATEGY`` to an
optimiser that never requests gradients is an error.

Analytical availability depends on the model, fit, loss terms, search
parameters and chosen optimiser. Setup validates derivative support for
the selected combination. ``HYBRID`` is the explicit choice when
numerical fallback is acceptable.

The analytical fit path needs the settings listed under
:hpo:key:`GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>`: startup refuses a
setup without them, naming the first setting it lacks. Each observable
must also supply a supported derivative.

Each search parameter needs an analytical derivative in the rows the run
has, one per energy and one per force component (:ref:`fit_objective`):
the energy rows, the force rows of the fit with :ref:`FORCE` true, and
the forces that :hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>` measures in the
validation structures. ``ANALYTICAL`` refuses a search parameter without
one at startup (:hpo:key:`!GRAD_STRATEGY <OPTIMIZER.GRAD_STRATEGY>`).
:ref:`nfp_analytical_coverage` lists the search parameters each
descriptor differentiates.

.. versionchanged:: 1.6.0
   1.5.0 stopped an ``ANALYTICAL`` run at the gradient of such a
   parameter, or of one whose structures hold no pair of atoms that
   depends on it. In some of the :ref:`WATOM` setups below it ran on
   instead, with a gradient that lacked the part of the descriptor
   without that derivative.

.. _nfp_analytical_coverage:

Analytical coverage
-------------------

The analytical derivatives cover the parameters of the descriptors below
(:doc:`descriptors`), in the energy and the force rows unless the table
says otherwise. A key of the fit, such as :ref:`LAMBDA`, has none.
:ref:`RCUT2B` and :ref:`RCUTMB` act through the cutoff function, and
``Cut_Dummy`` gives no derivative with respect to them.

.. list-table::
   :header-rows: 1
   :widths: 32 68

   * - Descriptor
     - Search parameters with an analytical derivative
   * - ``D2_Blip``, ``D2_Blip5``, ``D2_BP``
     - :ref:`CGRID2B`, :ref:`SGRID2B`, :ref:`RCUT2B`, :ref:`WATOM`
   * - ``D2_Knot5``
     - :ref:`CGRID2B`, :ref:`RCUT2B`, :ref:`WATOM`
   * - ``D2_LJ``, ``D2_MIE``, ``D2_ZBL``
     - :ref:`RCUT2B`, :ref:`WATOM`
   * - ``DM_mBlip``
     - :ref:`CGRIDMB`, :ref:`SGRIDMB`, :ref:`RCUTMB`, :ref:`WATOM`,
       :ref:`CEMBFUNC`, :ref:`SEMBFUNC`
   * - ``DM_mBlipEnv``
     - those of ``DM_mBlip`` and :ref:`RCUTENV`
   * - ``DM_Blip``, ``DM_EAD``, ``DM_mRLR``, ``DM_mSQRT``
     - :ref:`CGRIDMB`, :ref:`SGRIDMB`, :ref:`RCUTMB`, :ref:`WATOM`, in
       the energy rows only
   * - ``D2_mJoin`` and every other descriptor but ``DM_mJoin`` and
       ``DESCRIPTOR`` blocks
     - none

A search parameter has an analytical derivative only in the rows that
every descriptor that depends on it differentiates. One that a descriptor
differentiates only in the energy rows is accepted under ``ANALYTICAL``
solely in a run without force rows: :ref:`FORCE` false and no
:hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>`. The analytical gradient of
``DM_mJoin`` and of ``DESCRIPTOR`` blocks is described with the blocks
(:doc:`dm_embed`).

Most two-body and many-body descriptors read :ref:`WATOM` (the key names
those that do not), so both descriptors of a configuration usually depend
on it. ``DM_REAM``, the ``DM_Poly`` descriptors, ``DESCRIPTOR`` blocks,
``DM_mJoin`` and ``D2_mJoin`` depend on ``WATOM`` and give no derivative
with respect to it. A ``WATOM`` value that weighs their pairs has no
analytical derivative, whatever the other descriptor gives. ``HYBRID``
takes it by finite differences, and ``ANALYTICAL`` refuses it at
startup. Beside ``DM_Blip``, ``DM_EAD``, ``DM_mRLR`` or ``DM_mSQRT`` it
has one in the energy rows only.

Finite-difference keys
----------------------

These case-sensitive ``PARAM`` keys apply to ``NUMERICAL`` and the
numerical part of ``HYBRID``:

:hpo:key:`PARAM <OPTIMIZER.PARAM>` :hpo:param:`fd_step <PARAM.FD.fd_step>` ``s``
   A positive finite scalar that controls the step rule below. The
   default is ``1e-6``.

:hpo:key:`!PARAM <OPTIMIZER.PARAM>` :hpo:param:`fd_kernel <PARAM.FD.fd_kernel>` ``name``
   ``inhouse`` or ``ceres``; their all-capital forms are also accepted.
   Selects the finite-difference step rule. The default depends on
   ``GRAD_STRATEGY`` and ``OPTIM_SCALE``; see below.

:hpo:key:`!PARAM <OPTIMIZER.PARAM>` :hpo:param:`diff_method <PARAM.FD.diff_method>` ``n``
   ``0`` for central differences, ``1`` for forward differences. Other
   values, including ``2`` (Ridders), are rejected. The default is ``0``.

Without ``PARAM fd_kernel``, the rule is ``inhouse`` for ``HYBRID`` or
normalised coordinates (``OPTIM_SCALE AUTO``), and ``ceres`` for
``NUMERICAL`` with ``OPTIM_SCALE NONE``. The ``fd_step`` default remains
``1e-6`` and the scheme remains central in either case. ``NOISE AUTO``
can derive a different ``fd_step``; see :doc:`nfp_noise`.

These keys are rejected where no finite differences are taken, including
``GRAD_STRATEGY ANALYTICAL`` and ``INNER`` blocks. ``STEP`` controls an
optimiser's initial search step and is a separate setting.

.. _hpo_fd_stencil:

Steps and boundaries
--------------------

For internal optimiser coordinate ``u`` and configured ``s = fd_step``:

* ``inhouse`` uses ``h = s * max(abs(u), 1)``.
* ``ceres`` uses ``h = max(2**(-26), abs(u) * s)``.

With ``OPTIM_SCALE AUTO``, coordinates lie in the unit box, so the
``inhouse`` step is a fraction of the transformed width of each
dimension. Without normalisation, it is absolute for ``abs(u) <= 1`` and
relative above that. The ``ceres`` rule always has its absolute floor.

.. _hpo_out_of_box:

Stencils near a bound
~~~~~~~~~~~~~~~~~~~~~

Central differences use ``u-h`` and ``u+h`` where possible. Near a bound,
they use a second-order, three-point one-sided stencil inside the box.
Forward differences use one extra point and reverse direction near the
upper bound. The configured step rule also applies at the boundary.

Setup rejects a box in which the requested stencil cannot fit at every
point. With unit-box normalisation, the ``inhouse`` limits are
``fd_step <= 1/3`` for central differences and ``<= 1/2`` for forward
differences. The ``ceres`` limits there are ``1/2`` and ``1``.
:doc:`nfp_noise` describes the optional automatic step heuristic.

.. _hpo_gradient_not_finite:

Cost and unusable gradients
---------------------------

A central gradient normally needs two additional loss evaluations per
numerical dimension; forward differences need one. The loss at the
centre is reused when supplied by the optimiser. Analytical gradient
calls also count towards ``MAXEVAL``. A small budget can therefore end a
search before its first parameter step.

A failed loss at the current point, or an unusable finite-difference
stencil, can leave the search without a usable gradient. NLopt and Ceres reject such
a point and step back from it. A local search that has had no usable
gradient yet ends there, and :hpo:opt:`DLIB:BFGS` ends there at any
point. Under the NLopt MLSL forms (:hpo:opt:`NLOPT:G_MLSL`,
:hpo:opt:`NLOPT:GD_MLSL` and their ``_LDS`` forms), a local search of
:hpo:opt:`NLOPT:LD_LBFGS`, of one of the four ``LD_TNEWTON`` algorithms
(:hpo:opt:`NLOPT:LD_TNEWTON` among them), of :hpo:opt:`NLOPT:LD_VAR1` or of
:hpo:opt:`NLOPT:LD_VAR2` that returns an NLopt failure at the edge of the
failure region ends the whole search, because NLopt's MLSL ends on any
error of a local search. The run says ``STOPPED -- no usable gradient``.

Failure grading can supply numerical slopes within a graded failure
region. Each stencil requires losses from a consistent usable or failed
region. This requirement also applies to the numerical part of
``HYBRID``.

With ``PC_LAMMPS`` or ``PC_FAILPROBE``, every gradient-based optimiser
is refused, ``DLIB:BFGS`` included: the results of a LAMMPS script have
no gradient.
MLSL/AUGLAG with a derivative-free local search is allowed.
``PC_OBSERVABLE`` supplies its own analytical derivative path; the rules
for ``GRAD_STRATEGY`` beside it are under :ref:`nfp_observable_gradients`.

.. versionchanged:: 1.6.0
   1.5.0 accepted ``DLIB:BFGS`` with ``PC_LAMMPS``, on finite
   differences of the LAMMPS loss terms.
