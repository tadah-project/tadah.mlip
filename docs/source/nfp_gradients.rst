.. _hpo_fd_stencil:

.. _hpo_out_of_box:

.. _hpo_gradient_not_finite:

Gradients and finite differences
================================

``GRAD_STRATEGY`` controls how a gradient-based optimiser obtains the
derivative of the complete loss. It belongs to the optimiser requesting
that gradient: for NLopt MLSL/AUGLAG, the enclosing optimiser owns this
setting.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

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
has: the energy rows, the force rows of the fit with :ref:`FORCE` true,
and the forces that :hpo:key:`PC_FRMSE <ROOT.PC_FRMSE>` measures. Once
the data are read, ``ANALYTICAL`` refuses a search parameter without
one at the values of the training configuration. It does so before any
evaluation, and in ``tadah hpo --check`` too. The message names each
such :hpo:key:`OPTIM <ROOT.OPTIM>` line and why. Either no descriptor
differentiates it, or a descriptor differentiates it in the energy rows
only, or the data hold no pair of atoms where the descriptors depend on
it. :ref:`hpo_analytical_coverage` lists the search parameters each
descriptor differentiates.

.. _hpo_analytical_coverage:

Analytical coverage
-------------------

The analytical derivatives cover the parameters of the descriptors below
(:doc:`descriptors`), in the energy and the force rows unless the table
says otherwise. A key of the fit, such as :ref:`LAMBDA`, has none.

A descriptor differentiates a parameter through the pairs of atoms whose
terms depend on it, at the values of the training configuration: pairs
within its cutoff, of the species it pairs. The training structures, and the
validation structures when there are any, need such a pair.
:ref:`RCUT2B` and :ref:`RCUTMB` act through the cutoff function.
``Cut_PolyS`` and ``Cut_CosS`` depend on the cutoff only for a pair
within 1 Å of it, and ``Cut_Dummy`` does not depend on it.

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

A search parameter that a descriptor differentiates in the energy rows
only takes ``ANALYTICAL`` in a run without force rows: :ref:`FORCE`
false and no :hpo:key:`!PC_FRMSE <ROOT.PC_FRMSE>`. So does
:ref:`WATOM` beside ``DM_Blip``, ``DM_EAD``, ``DM_mRLR`` or
``DM_mSQRT``: a two-body and a many-body descriptor both read it, and
its force rows lack the many-body part. The analytical
gradient of ``DM_mJoin`` and of ``DESCRIPTOR`` blocks is described with
the blocks (:doc:`dm_embed`).

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
leave the search without a usable gradient. NLopt and Ceres reject such
a point and step back from it. A local search that has had no usable
gradient yet ends there, and :hpo:opt:`DLIB:BFGS` ends there at any
point.

Failure grading can supply numerical slopes within a graded failure
region. Each stencil requires losses from a consistent usable or failed
region. This requirement also applies to the numerical part of
``HYBRID``.

With ``PC_LAMMPS`` or ``PC_FAILPROBE``, every gradient-based optimiser
is refused, ``DLIB:BFGS`` included: the results of a LAMMPS script have
no gradient.
MLSL/AUGLAG with a derivative-free local search is allowed.
``PC_OBSERVABLE`` supplies its own analytical derivative path and has
separate compatibility rules.

.. versionchanged:: 1.6.0
   1.5.0 accepted ``DLIB:BFGS`` with ``PC_LAMMPS``, on finite
   differences of the LAMMPS loss terms.
