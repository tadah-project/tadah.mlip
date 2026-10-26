.. _dm_embed:

Modular scalar embedding descriptor
===================================

``DM_Embed`` represents a scalar, rotationally invariant density (L = 0):

.. math::

   \rho_i=\sum_{j\ne i}W_{Z_iZ_j}f_c(r_{ij})\sum_n a_n T_n(r_{ij}),
   \qquad X_{im}=g_m(\rho_i)E(\rho_i)-g_m(0)E(0).

The last term applies under ``REFERENCE ZERO`` (the default). Under ``FREE``
the raw embedding is used. Model coefficients multiply the embedding outputs;
density amplitudes are nonlinear parameters. Density and embedding select their
bases independently. Existing ``DM_REAM`` and ``DM_mBlipEnv`` files retain their
existing evaluators and numerical conventions.

Configuration
-------------

.. code-block:: text

   DESCRIPTOR ta DM_Embed
     PAIRS Ta Ta
     DENSITY
       BASIS Blip5
       FORM NONNEG
       CENTRES 1.0 2.5 4.0
       ETA 1.5 1.5 1.5
       AMPLITUDES 0.4 0.2 0.1
       CUTOFF
         TYPE Cut_Poly
         RADIUS 6.0
       ENDCUTOFF
     ENDDENSITY
     EMBEDDING
       BASIS Gaussian
       CENTRES 0.0 0.5 1.0 1.5
       WIDTHS 0.5 0.5 0.5 0.5
       REFERENCE ZERO
       ENVELOPE
         TYPE Smoothstep2
         CUTOFF 2.0
       ENDENVELOPE
     ENDEMBEDDING
   ENDDESCRIPTOR

Blocks activate the many-body term; an explicitly disabled ``INITMB`` is an
error. IDs are unique without regard to case. Without ``TYPEMB``, blocks compose
in declaration order. A ``TYPEMB @ta`` line among the legacy ``TYPEMB``
declarations places that block at its position, ordering blocks together with
legacy descriptors. With ``TYPEMB``, each declared ID must appear exactly once;
any other count is an error. Legacy cutoff and grid lists continue
to describe the legacy terms only. ``CONFIG`` includes preserve block order,
boundaries and source locations: an included file's blocks stand at its
``CONFIG`` line, after the blocks above that line.

``FORM`` is required in each density: ``SIGNED`` permits signed amplitudes;
``NONNEG`` requires nonnegative amplitudes and positive pair weights;
``MONOTONE`` uses normalised integrated bumps with nonnegative amplitudes and
positive weights. Knot densities use ``NONNEG`` for their naturally decreasing
left-active powers; ``MONOTONE`` requires a bump family. ``REFERENCE`` defaults
to ``ZERO`` and an absent envelope defaults to ``OFF``. Duplicate, unknown,
inactive and nonfinite fields, mismatched arrays, and invalid scales are errors.
Numeric arrays accept the established ``LIN n lo hi`` and ``GEOM n lo hi`` grids.

Bases and coordinates
---------------------

With :math:`x=\eta(q-\mu)` for compact bumps, and :math:`u=|x|`:

.. list-table::
   :header-rows: 1

   * - Basis
     - Definition / support
     - Smoothness
   * - ``Blip``
     - Peak-normalised cubic B-spline, :math:`|x|<2`
     - C2
   * - ``Blip5``
     - Peak-normalised quintic B-spline, :math:`|x|<3`
     - C4
   * - ``Gaussian``
     - :math:`\exp[-\eta(q-\mu)^2]`, noncompact
     - Analytic
   * - ``Knot5``
     - Density :math:`(\mu-q)_+^k`; embedding :math:`(q-\mu)_+^k`
     - :math:`C^{k-1}`
   * - ``WendlandC2``
     - :math:`(1-u)_+^4(1+4u)`
     - C2
   * - ``WendlandC4``
     - :math:`(1-u)_+^6(1+6u+35u^2/3)`
     - C4

``Knot5`` accepts optional ``DEGREES`` entries 3 or 5, including mixed lists;
without the field all degrees are 5. It accepts neither ETA nor WIDTHS. The
embedding knots remain right-active at arbitrarily high density.

These are peak normalisations, not volume or orthogonality normalisations. The
cubic and quintic spline *degrees* are 3 and 5; their spline *orders* are 4 and
6. The cubic form agrees with `Alfe and Gillan
<https://www.homepages.ucl.ac.uk/~ucfbdxa/pubblicazioni/prb04blips.pdf>`_.
`Dehnen and Aly <https://arxiv.org/abs/1204.2471>`_ discuss quintic splines and
Wendland alternatives. The Wendland polynomials here have degrees 5 and 8.

``ETA`` retains the established inverse-scale definition and is the default
coordinate system. Alternatively, ``WIDTHS`` stores physical scales :math:`w`:
compact bumps use :math:`\eta=1/w`, Gaussians use :math:`\eta=1/w^2`. A Gaussian
therefore means :math:`\exp[-((q-\mu)/w)^2]`, with no factor of two. A blip's
support half-width is 2w or 3w; a Wendland's is w. ETA and WIDTHS are mutually
exclusive within each component.

For monotone densities, :math:`T_n=S_n` is the integral of the bump from q to
infinity, divided by its full one-dimensional area. Thus S is 1 below support,
1/2 at its centre, and 0 above support. The areas in x coordinates are 3/2,
20/11, 2/3 and 16/27 for Blip, Blip5, WendlandC2 and WendlandC4. The Gaussian
step is :math:`\tfrac12\operatorname{erfc}(\sqrt\eta(q-\mu))`.
For the Wendland steps, with :math:`t=1-x`, :math:`0\le x\le1`, the normalised
upper tails are :math:`S_{C2}(x)=t^5(3/2-t)` and
:math:`S_{C4}(x)=t^7(72-99t+35t^2)/16`. They extend by
:math:`S(-x)=1-S(x)`, with values 1 and 0 below and above support.


Cutoffs and envelopes
---------------------

Spatial cutoffs are ``Cut_Cos``, ``Cut_CosS``, ``Cut_Poly``, ``Cut_PolyS``,
``Cut_Tanh``, ``Cut_PT`` and ``NONE``. A Gaussian density always requires a real
smooth cutoff, even if its evaluated tail underflows. With NONE, every density
component must terminate smoothly at or before RADIUS. With a smooth cutoff,
centres, knot positions and radius remain independent. Envelope cutoffs have
density units and never contribute to spatial neighbour ranges.

Every embedding family supports ``OFF``, ``SinInv``, ``Smoothstep2`` or
``Smoothstep4``; products of envelopes are not accepted. For t = rho/C below 1,
these are respectively 1, sin(pi t/2), :math:`10t^3-15t^4+6t^5`, and
:math:`126t^5-420t^6+540t^7-315t^8+70t^9`; above 1 they equal 1. SinInv matches
with C1 smoothness at C; the smoothsteps match with C2 and C4. Enabled envelopes
explicitly reject negative total density. OFF permits signed density. There is
no high-density cutoff on the embedding basis.

.. _dm_embed_parameters:

Named nonlinear parameters
--------------------------

.. code-block:: text

   OPTIM ta.density.centres (1-3) AUTO FRAC 0.1 FLOOR 0.02
   OPTIM ta.density.eta (1-3) AUTO RATIO 1.2
   OPTIM ta.density.amplitudes (1-3) 0.0 1.0
   OPTIM ta.density.cutoff.radius (1) 5.5 6.5
   OPTIM ta.embedding.centres (1-4) AUTO FRAC 0.1 FLOOR 0.02
   OPTIM ta.embedding.widths (1-4) AUTO RATIO 1.2
   OPTIM ta.embedding.envelope.cutoff (1) 1.0 3.0

Paths, bounds, optimiser steps, differences, checkpoints and logs use the
declared coordinates. Width derivatives include -eta/w for compact bumps and
-2 eta/w for Gaussians. The selected coordinates remain authoritative; potential
serialisation preserves their full precision even at a lower NUMERIC setting.
Only the configured ETA or WIDTHS path exists. Knot degrees, basis choices,
reference modes, forms and envelope types are discrete and cannot be optimised.
Radius optimisation under NONE is refused. Repeated dimensions and inactive
aliases are errors. Existing flat OPTIM syntax remains available for legacy
terms; a legacy many-body key (``CGRIDMB``, ``SGRIDMB``, ``AMPGRIDMB``,
``CEMBFUNC``, ``SEMBFUNC``, ``RCUTMB``) of a potential whose many-body term is
DM_Embed, which has no such key, is refused naming the component path that
holds the parameter (``OPTIM CGRIDMB: the potential has no CGRIDMB key: its
many-body term is DM_Embed component(s) (DESCRIPTOR blocks), whose density
parameter is the named path TA.DENSITY.CENTRES; ...``). In a ``DM_mJoin`` the
analytical gradient (``GRAD_STRATEGY ANALYTICAL`` or ``HYBRID``) differentiates
the named paths of its ``DM_Embed`` children; a legacy key indexes the joined
lists, which no child reads against its own grid, so ``HYBRID`` takes it by
finite differences and ``ANALYTICAL`` names it as a parameter it cannot
differentiate, as before ``DM_Embed``. Scalar density gates accept a single
scalar component; mixed terms need an explicitly defined component selection
before such gates can be applied. A trainer of a DM_Embed potential (``tadah
train``, ``tadah hpo``'s ``best_pot.tadah``) writes no ``RCTYPEMB`` line: the
blocks' ``CUTOFF`` are the many-body cutoffs.

.. _dm_embed_refit:

Refit, editing and analysis
---------------------------

Every ``tadah refit`` writes resolved ``DM_Embed`` blocks (:ref:`refit`; the ID
``eam`` when the configuration declares no block). Without component blocks,
``REFIT_BASIS`` keeps its shared-family meaning, and ``RHOFORM`` and ``EMBF0``
become the written block's ``FORM`` and ``REFERENCE``. A dense ``REFIT_BASIS
Gaussian`` refit fits and writes the pair function and the density with the
smooth cutoff ``Cut_Poly`` at the table's rcut (``RCTYPE2B Cut_Poly``, the
density's ``CUTOFF TYPE Cut_Poly``), since a Gaussian density never vanishes and
needs a real smooth cutoff; its EXPERIMENTAL warning says so. With a block,
``REFIT_PAIR_BASIS`` (default Blip5) selects the existing pair family
separately; REFIT_BASIS and global many-body arrays,
RHOFORM and EMBF0 conflict with component ownership. REFIT_PAIR_BASIS without
a block is refused; REFIT_BASIS controls every legacy component.

A single-element refit accepts one block whose PAIRS match its source element.
``CENTRES AUTO [count]`` generates a grid; ``ETA AUTO`` or ``WIDTHS AUTO`` derives
its scales; ``AMPLITUDES AUTO`` fits amplitudes in the declared density form;
``RADIUS AUTO`` uses the source table radius. Explicit amplitudes remain fixed.
Resolved potentials contain numeric values. Explicit grids are fixed in dense
mode and are initial seeds in compact mode. Dense mode retains its knot
restrictions. Component compact mode polishes density and embedding shapes
with analytical variable-projection derivatives and Ceres; pair coefficients
are fitted on the pair grid. Legacy ladder stage settings are refused for this
path, which takes its seeds from the component grids.

A component fit has the basis its block declares and does not escalate it:
``REFIT_MAX_ESCALATIONS`` is refused with a block, and a fit above tolerance
names the component's counts and grids (``CENTRES AUTO <count>``, or a finer
explicit grid). Both modes fit the density on [max(wall edge, r_lo), RADIUS],
the window of the fit without blocks (never below r_lo), and the embedding on
the table's whole range, [0, rhomax] (the log's ``embedding fit window [0,
...]``). ``REFIT_COMPONENT_ITERATIONS`` (compact mode with a block only;
default 60) is the iteration limit of the compact fit: a solve that stops
there before converging is written with a WARNING naming the stop and the key.
The compact fit evaluates the configured grids before the solve, so a table
the declared ``FORM`` cannot follow is refused with its own message (``EAMRefit:
the density component CU's FORM NONNEG cannot fit this table: ... Use FORM
SIGNED in its DENSITY block.``); during the solve a trial point the component
cannot be evaluated at is infeasible, and an internal error stops the refit.
Its Ceres solve prints no progress lines whatever ``VERBOSE`` is (the INFO log
has its summary).

``refit --edit`` preserves component families, envelopes, reference and selected
coordinate system; additions have zero coefficient. Removal reports compensation residuals and refits the remaining
coefficients and is lossy. ``--eta`` specifies the new function's eta, converted
once to its selected coordinate when the component stores WIDTHS.

HPO starters (``refit.hpotarget``) list each component's parameters after the
pair section under a header of their own, ``# ---- TA component (DM_Embed)
density and embedding shape ----``, each group after a ``# COMPONENT_PARAMETER
<path> <rule>`` line (``NONNEG``, ``POSITIVE`` or ``FREE``), and note a zero
reference as ``REFERENCE ZERO``. The knot centres of a density with ``CUTOFF TYPE
NONE``, whose knots end within the fixed RADIUS, get boxes that end at RADIUS,
recorded in a ``# TA.DENSITY.CENTRES knot-support cap: RADIUS = 4 (...)``
comment. Retargeted starters retain named paths, their positivity rules and the
cap; ``--retarget`` rewrites the percentage of component group comments and
accepts ``--set 'TA.DENSITY.CENTRES(2)=1.8,2.2'``, and refuses ``--rhoform`` and
``--rcutmb``, which name DM_REAM keys, on a starter without ``AMPGRIDMB`` and
``CGRIDMB`` groups (the form is the block's ``FORM``, the radius the path
``TA.DENSITY.CUTOFF.RADIUS``).

``analysis bfunc --type MB DENSITY`` (or ``MB``) and ``MB EMBEDDING`` print
component curves with the meaning of the radial curves: the bare functions;
``--rescale`` tapers the density functions by the spatial cutoff (the product
rule under ``--derivative``) and is refused for the embedding, which has no
spatial cutoff; ``MB Y`` prints the bare density functions and then a block
with the cutoff (its derivative under ``--derivative``), in the fixed
``NUMERIC`` format of the two-body curves. A DM_REAM potential's density and
embedding are printed the same way, in the declared density form (the steps of
a ``MONOTONE`` density) and embedding reference. Generic and scalar EAM LAMMPS
evaluation share the same composed evaluator.

For HPO, a configuration either retains FIXINDEX/FIXWEIGHT and searches
selected coefficient pins, or removes both pin keys and supplies training data
so every shape candidate re-solves its linear coefficients. Validation data alone does not train them.

Further candidates
------------------

Compact support and adjustable smoothness motivate Wendland candidates; fitting
accuracy and conditioning must be measured at comparable fitting budgets.
`Chernih, Sloan and Womersley <https://arxiv.org/abs/1203.5696>`_ analyse Wendland
families. Fourier-Bessel bases (`DimeNet <https://arxiv.org/abs/2003.03123>`_) and
orthogonal polynomial constructions (`ACE analysis
<https://arxiv.org/abs/1911.03550>`_) are further candidates, outside this
implementation. Their normalisations and conditioning differ from these bumps.
