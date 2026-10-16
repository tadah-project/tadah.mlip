.. _hpo_feasibility_gates:

.. _hpo_rhoform_positive:

Candidate feasibility gates
===========================

These root keys reject unsuitable candidates before fitting their
weights. Add them to the :doc:`target file <nfp_configuration>`.
``BUMPGATE`` checks compact support; the optional density gates check
sampled density curves. Rejections use a :doc:`failure score <nfp_losses>`.

BUMPGATE
--------

Syntax: ``BUMPGATE CANDIDATE|STRICT``; default ``CANDIDATE``.

.. list-table:: Support policies
   :header-rows: 1
   :widths: 23 77

   * - Value
     - Behaviour
   * - ``CANDIDATE``
     - Reject individual candidates whose compact basis support
       exceeds the interaction radius. Startup rejects a box with no
       feasible position for a component.
   * - ``STRICT``
     - Require the whole search box to satisfy support bounds at
       startup, including its worst corner.

The seed must satisfy its support condition in both modes. The check
applies to supported legacy bump channels with ``Cut_Dummy``, and to
``DM_Embed`` density components with cutoff ``NONE``. Smooth spatial
cutoffs permit the underlying basis support to extend beyond the
interaction radius. Embedding-envelope cutoffs act on density values.

For a ``DM_Embed`` compact bump of centre :math:`c` and physical width
:math:`w`, its right support edge is :math:`c+s w`, where :math:`s` is
``2`` for Blip, ``3`` for Blip5 and ``1`` for either Wendland family.
With ``ETA``, replace :math:`w` by :math:`1/\eta`. A left-active density
knot ends at its centre. Gaussian density requires a smooth cutoff and
rejects ``NONE``. Interaction-radius optimisation requires a smooth
spatial cutoff.

``TADAH_BUMPGATE=CANDIDATE|STRICT`` overrides the file setting.

Density-gate scope
------------------

``RHOGATE`` and ``RHOBARGATE`` are optional and may be used together.
They support a bare legacy ``DM_REAM`` many-body term or a single scalar
``DM_Embed`` component. They reject multiple component densities and
legacy joined many-body descriptors. Their scans use the configured
density, including its spatial cutoff.

Both gates require positive density pair weights. Their verdicts apply
to the sampled points of finite grids. Each key can
appear only once; unknown or repeated options are errors. The options
and lattice names on these two lines are case-insensitive.

RHOGATE
-------

.. code-block:: none

   RHOGATE <tol> [R_LO <radius>] [NPTS <count>] [SEED ERROR|WARN]

This gate applies only to density form ``SIGNED``. It rejects a sampled
density with :math:`\rho_\min<-\mathrm{tol}\,\rho_\max`.
``NONNEG`` and ``MONOTONE`` forms already enforce nonnegative density,
so ``RHOGATE`` is refused for those forms.

.. list-table:: Sign-gate options
   :header-rows: 1
   :widths: 21 27 52

   * - Argument
     - Default
     - Rule
   * - ``tol``
     - Required
     - Finite and nonnegative; ``0`` rejects any sampled negative value.
   * - ``R_LO``
     - ``0.15`` times the candidate radius
     - Finite, nonnegative lower scan radius in Å. An explicit value
       must be below the smallest reachable interaction radius.
   * - ``NPTS``
     - ``20001``
     - Integer from 2 to 10000000; equally spaced samples through the
       candidate's interaction radius.
   * - ``SEED``
     - ``ERROR``
     - ``ERROR`` refuses a failing initial seed. ``WARN`` permits
       evaluation of that seed as a rejected starting point.

A sampled curve with no positive maximum fails, as does a non-finite
scan or an empty scan window.

RHOBARGATE
----------

.. code-block:: none

   RHOBARGATE <lattice> <a0> <f_lo> <f_hi> [NPTS <count>] [SEED ERROR|WARN]

This gate supports every density form. It sums the perfect monatomic
lattice's neighbour-shell densities and requires total density to
decrease strictly as the lattice constant increases. A flat sampled
step also fails.

.. list-table:: Lattice-density gate options
   :header-rows: 1
   :widths: 21 27 52

   * - Argument
     - Default
     - Rule
   * - ``lattice``
     - Required
     - ``bcc``, ``fcc`` or ``hcp``. The hcp ratio is
       :math:`c/a=\sqrt{8/3}`.
   * - ``a0``
     - Required
     - Finite positive reference lattice constant in Å; the in-plane
       constant for hcp.
   * - ``f_lo``, ``f_hi``
     - Required
     - Finite factors satisfying ``0 < f_lo < f_hi``. The scan spans
       lattice constants from ``f_lo * a0`` through ``f_hi * a0``.
   * - ``NPTS``
     - ``1001``
     - Integer from 3 to 10000000; equally spaced lattice constants.
   * - ``SEED``
     - ``ERROR``
     - Same initial-seed policy as ``RHOGATE``.

The nearest shell at the largest lattice constant must lie at or within
the smallest reachable interaction radius. A non-finite scan or an
identically zero lattice density fails. ``MONOTONE`` guarantees only
non-increase, so it can still fail this stricter test on a plateau.
