.. _hpo_feasibility_gates:

Candidate Feasibility Checks
============================

This page describes the checks that reject unsuitable candidates
before their coefficients are fitted. They are top-level keys of the
:doc:`target file <nfp_configuration>`. :hpo:key:`BUMPGATE` checks
compact support; the optional density checks :hpo:key:`RHOGATE` and
:hpo:key:`RHOBARGATE` check sampled density curves. A rejected candidate
receives a :doc:`failure score <nfp_losses>`.

BUMPGATE
--------

:hpo:key:`BUMPGATE` takes one of two values. The reference entry gives
the syntax and the default.

.. list-table:: Support modes
   :header-rows: 1
   :widths: 23 77

   * - Value
     - Behaviour
   * - ``CANDIDATE``
     - Refuses each candidate whose compact basis support exceeds the
       interaction radius. Startup refuses a box with no feasible
       position for a component.
   * - ``STRICT``
     - Requires the whole search box, including its worst corner, to
       satisfy the support bounds at startup.

In both modes the training configuration itself must satisfy the
support condition. The check applies to supported bump channels with
``Cut_Dummy``, and to ``DM_Embed`` density components with cutoff
``NONE``. Smooth spatial cutoffs permit the underlying basis support to
extend beyond the interaction radius. Embedding-envelope cutoffs act on
density values.

For a ``DM_Embed`` compact bump, :hpo:key:`!BUMPGATE` compares the right
support edge of the basis with the interaction radius. The support
half-width of each basis, the ``ETA`` and ``WIDTHS`` coordinates and the
cutoff requirements are described in :doc:`dm_embed`. The support of a
left-active density knot ends at its centre.

:hpo:key:`ENV.TADAH_BUMPGATE` overrides the file setting.

Density check coverage
----------------------

:hpo:key:`RHOGATE` and :hpo:key:`RHOBARGATE` are optional and may be
used together. They support a bare ``DM_REAM`` many-body term or a single
scalar ``DM_Embed`` component. They reject several density components
and joined many-body descriptors such as ``DM_mJoin``. Their scans use
the configured density, including its spatial cutoff.

Both checks require positive density pair weights. Their verdicts apply
to the sampled points of finite grids. Each key can appear only once;
unknown or repeated options are errors. The options and lattice names on
these two lines are case-insensitive.

.. _hpo_rhoform_positive:

RHOGATE
-------

:hpo:key:`RHOGATE` applies only to density form ``SIGNED``. It rejects a
sampled density with :math:`\rho_\min<-\mathrm{tol}\,\rho_\max`.
``NONNEG`` and ``MONOTONE`` forms already enforce nonnegative density,
so :hpo:key:`!RHOGATE` is refused for those forms. A tolerance of ``0``
rejects any sampled negative value.

.. code-block:: none

   RHOGATE 1e-5

The options :hpo:key:`R_LO <RHOGATE.R_LO>`,
:hpo:key:`NPTS <RHOGATE.NPTS>` and :hpo:key:`SEED <RHOGATE.SEED>` set
the scan window, the number of samples and the treatment of a failing
starting point. Their values and defaults are in the reference entries.

A sampled curve with no positive maximum fails, as does a non-finite
scan or an empty scan window.

RHOBARGATE
----------

:hpo:key:`RHOBARGATE` supports every density form. It sums the
neighbour-shell densities of the perfect monatomic lattice and requires
the total density to decrease strictly as the lattice constant
increases. A flat sampled step also fails. The hcp ratio is
:math:`c/a=\sqrt{8/3}`.

.. code-block:: none

   RHOBARGATE bcc 3.304 0.95 1.05

The scan spans the lattice constants from ``f_lo * a0`` through
``f_hi * a0``. The options :hpo:key:`NPTS <RHOBARGATE.NPTS>` and
:hpo:key:`SEED <RHOBARGATE.SEED>` follow the same rules as those of
:hpo:key:`RHOGATE`; their values and defaults are in the reference
entries.

The nearest shell at the largest lattice constant must lie at or within
the smallest reachable interaction radius. A non-finite scan or an
identically zero lattice density fails. ``MONOTONE`` guarantees only
non-increase, so it can still fail this stricter test on a plateau.
