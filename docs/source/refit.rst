.. _refit:

Refitting tabulated EAM potentials
==================================

``tadah refit`` converts a tabulated EAM potential (a *setfl* file in the
LAMMPS ``eam/alloy`` / ``eam/fs`` format) into a native Tadah! potential. The
three tabulated functions — pair :math:`\phi(r)`, electron density
:math:`\rho(r)` and embedding :math:`F(\rho)` — are re-expressed as small
basis-function expansions and fitted, producing a native ``pot.tadah``. Its
descriptors follow ``REFIT_BASIS``: a ``D2_*`` pair descriptor (e.g.
``D2_Knot5``, ``D2_Blip``) together with the ``DM_REAM`` many-body descriptor.
The result is usable with ``tadah predict``, the Tadah!LAMMPS plugin, and
:ref:`nested fitting <nested_fitting>`. ``refit`` also writes a round-trip
*setfl* (for direct LAMMPS comparison) and a ``tadah hpo`` starter target.

Typical workflow
----------------

#. Write a ``refit.cfg`` naming the input EAM, the density form and the basis
   (see `Quick start`_).
#. Run ``refit`` on a setfl EAM: ``tadah refit -c refit.cfg -v 2``.
#. Read the crystal self-test at the end of ``refit_opt.log`` (energy–volume
   accuracy of the fitted model vs the original table).
#. Use ``pot.tadah`` directly, or hand ``refit.hpotarget`` to ``tadah hpo``
   for nested fitting.

Quick start
-----------

Minimal config (``EAM`` and ``RHOFORM`` are required, all other settings
take their defaults):

.. code-block:: none

   EAM             Ti1.eam.fs
   RHOFORM         SIGNED
   REFIT_OBJECTIVE curve
   REFIT_BASIS     Knot5

Run it:

.. code-block:: bash

   tadah refit -c refit.cfg -v 2

A fuller curve-mode config (pinned window edges, explicit basis ceiling):

.. code-block:: none

   EAM             Ti1.eam.fs
   RHOFORM         SIGNED           # see "Density form"; MONOTONE needs Blip/Blip5
   OUTFILE         pot.tadah
   SETFL_OUT       refit.eam
   REFIT_OBJECTIVE curve
   REFIT_BASIS     Knot5            # Knot3 suits some elements (see REFIT_BASIS)
   REFIT_COMPACT_NMAX 35            # one value, or "phi rho F" e.g. 35 12 35

   INIT
     PHI XMIN PIN
     RHO XMIN PIN
     F   XMIN PIN
     F   XMAX PIN
   ENDINIT

The pipeline stages ``EXPLORE`` / ``REFINE`` / ``FUSE`` use built-in defaults
when omitted; see `How the fit works`_ for what each does and `Stage blocks`_
to override them.

How the fit works
-----------------

Each function is fitted by a basis-count **ladder**: start from a small seed,
then add one basis function per rung at the largest residual, until the
per-function ceiling ``REFIT_COMPACT_NMAX`` or a stall. Each rung is optimised
in stages:

* **EXPLORE** — a global seed then a per-rung local search that place the
  basis-function centres and widths. The optimiser at each stage is
  configurable (see `Stage blocks`_).
* **REFINE** — a high-precision local polish of centres, widths and
  coefficients (Ceres LM).
* **FUSE** — a final refinement: in ``curve`` mode a per-function polish of the
  weighted curve, in ``energy`` mode a joint fit of all three functions to the
  crystal energy–volume curve.

``REFIT_OBJECTIVE`` selects what every stage optimises. ``curve`` fits the
tabulated functions directly and concentrates accuracy near the equilibrium
structure (see ``REFIT_CURVE_WEIGHTING``); ``energy`` fits the crystal
energy–volume curve.

Reading the result
------------------

The self-test builds an ideal crystal from the **lattice type and constant in
the EAM header** (the ``<Z> <mass> <a0> <lattice>`` line). ``fcc``, ``bcc`` and
``hcp`` are supported (case-insensitive); ``hcp`` uses the ideal
:math:`c/a=\sqrt{8/3}`. If the header declares no supported lattice or a
non-positive constant (for example ``... 0.0 NONE``), ``refit`` stops with an
error — edit that line to the actual ground-state lattice and lattice constant
and rerun. The test compares the original and fitted models on the *same*
geometry, so the reported errors are exact even when the header constant is not
the potential's own energy minimum.

The end of ``refit_opt.log`` reports a crystal self-test over
:math:`a/a_0 \in [0.75, 1.10]`:

.. code-block:: none

   ambient    RMSE=0.05 meV/atom  max=0.10
   compressed RMSE=0.20 meV/atom  max=0.40
   cohesive: original=-5.34 refit=-5.34 delta=0.05 meV/atom

``ambient`` is :math:`a/a_0\,0.95\!-\!1.10`, ``compressed`` is
:math:`0.75\!-\!0.95`; ``REFIT_TOL_AMBIENT`` / ``REFIT_TOL_COMPRESSED`` are the
pass thresholds. ``cohesive delta`` is the equilibrium-energy error. A fit
that stays above them ends with a WARNING naming the keys of the fit that
ran: ``REFIT_COMPACT_NMAX`` or ``EXPLORE MAX_ADD`` (a larger basis) after a
compact fit, ``REFIT_MAX_ESCALATIONS`` or ``N2B``/``NRHO_BASIS``/``NF_BASIS``
after a dense one. Before 1.6.0 a compact fit got the dense fit's advice,
and its window report named ``REFIT_PLACEMENT`` and the dense fit's uniform
tail basis.

.. _refit_rhoform:

Density form (``RHOFORM``)
~~~~~~~~~~~~~~~~~~~~~~~~~~

The refit configuration must declare the :ref:`DM_REAM density form
<dm_ream_rhoform>` of the potential it writes: ``RHOFORM SIGNED``,
``NONNEG`` or ``MONOTONE``. There is no default; without the key ``refit``
stops before reading the table with ``tadah refit configuration: DM_REAM:
RHOFORM is required: ...``. The density is fitted, self-tested (the crystal
self-test and ``refit.eam`` use the fitted density in its form) and written
in the declared form, and the log states it (``tadah refit: RHOFORM
<form>: ...; the potential declares RHOFORM <form>.``).

.. list-table::
   :header-rows: 1
   :widths: 13 44 43

   * - Form
     - Density fit
     - Use
   * - ``SIGNED``
     - Signed amplitudes :math:`a_n` of the basis functions
       :math:`\psi_n`, as every tadah before 1.6.0 fitted them. In dense
       mode the key only adds the ``RHOFORM SIGNED`` line to
       ``pot.tadah``: the fit, ``refit.eam`` and ``refit.hpotarget`` are
       those of the density before the key existed; compact mode can differ
       where the earlier joint refinement left a bump past the cutoff (see
       *Compact mode* below).
     - Any table; no sign or shape guarantee.
   * - ``NONNEG``
     - The same columns with every :math:`a_n \ge 0`, by non-negative least
       squares.
     - :math:`\rho \ge 0` for every configuration the potential sees.
   * - ``MONOTONE``
     - The integrated-bump steps :math:`S_n` with every :math:`a_n \ge 0`,
       by non-negative least squares. Bump bases only (``Blip``,
       ``Blip5``, dense ``Gaussian``).
     - :math:`\rho \ge 0` and non-increasing, so :math:`\bar\rho(a)` falls
       monotonically under expansion on every lattice.

**Basis and mode.** Every form fits in both modes. ``MONOTONE`` is not
defined for the one-sided knot families (``Knot3``, ``Knot5``, ``KnotMix``),
and ``REFIT_MODE compact`` defaults to ``Knot5``, so a ``MONOTONE`` refit
must set ``REFIT_BASIS Blip`` or ``Blip5`` (compact) or ``Blip``, ``Blip5``
or ``Gaussian`` (dense); otherwise it stops with ``tadah refit: RHOFORM
MONOTONE is not defined for REFIT_BASIS Knot5 (the REFIT_MODE compact
default, a one-sided knot family): use a bump basis (Blip, Blip5) or RHOFORM
NONNEG ...``. ``MONOTONE`` also needs a non-negative, non-increasing
many-body cutoff; ``refit`` writes ``Cut_Dummy``, which is one.
``NONNEG`` with a knot family is allowed, but a knot density with every
amplitude :math:`\ge 0` is non-increasing and convex (knots :math:`\le`
``RCUTMB``, non-increasing cutoff), so the fit is locked to such shapes and
a table whose density is concave anywhere in the fit window (near the core,
say) is fitted poorly: the default compact ``Knot5`` fit of the Ravelo 2013
Ta setfl under ``NONNEG`` fails its self-test with errors above 100
meV/atom. A knot family set with ``REFIT_BASIS`` logs a HINT; the defaulted
compact ``Knot5`` logs a WARNING that recommends ``REFIT_BASIS Blip5`` (a
non-negative bump density without the convex lock) or ``RHOFORM SIGNED``.

**Table pre-checks.** Under ``NONNEG`` and ``MONOTONE`` the tabulated
density is checked on the radial fit window (the table nodes from the
window's lower edge, the ``fit_window_lo`` of ``refit_range_report.dat``,
to the cutoff; never below the inner fit bound, below which no fit samples
the table; with a steep wall the window starts above that bound, and the
table between the two, which the dense fit solves with a small tail basis,
is not checked) before any fitting, in both modes: a
node value below :math:`-\mathrm{tol}` is an error (a positive-form density
cannot be negative), and under ``MONOTONE`` so is a rise between
neighbouring nodes above :math:`\mathrm{tol}`, with
:math:`\mathrm{tol} = 8\,\varepsilon \max|\rho_{tab}|` over the window. The
message names the node, e.g. ``EAMRefit: RHOFORM MONOTONE cannot fit this
table: the tabulated density rises from rho = ... at r = ... A to ... at r
= ... A, inside the fit window [..., ...] A, and a MONOTONE density is
non-increasing. Use RHOFORM NONNEG or SIGNED.``

**Dense mode.** ``SIGNED`` solves each window block by the rank-revealing
least squares used before 1.6.0 (GELSD); ``NONNEG`` and ``MONOTONE`` solve it by
Lawson-Hanson non-negative least squares (Householder QR on the passive
columns, never the normal equations: the step columns of ``MONOTONE`` have
condition numbers up to about ``4e9``). ``LAMBDA`` applies in every form.
Under ``MONOTONE`` a step reaches down to :math:`r = 0`, so every step whose
outer edge lies inside the window takes part in the window block. The
non-negative solve costs more than GELSD and grows faster with the basis
size: with ``NRHO_BASIS 800`` a dense ``Blip5`` refit of a Ta table took
about 1.3 s under ``SIGNED``, 7 s under ``NONNEG`` and 33 s under
``MONOTONE`` on one machine (``NRHO_BASIS 400``: 0.4 s, 0.9 s and 2.3 s), and
basis escalations (``REFIT_MAX_ESCALATIONS``) repeat the fit. A solve that
stops before a Kuhn-Tucker point (at its iteration cap, ``3 n + 10``, or
because every column whose dual is above the tolerance was refused as
numerically dependent on the others) keeps its non-negative amplitudes and
logs a WARNING that names the reason and that they are not proven optimal.

**Compact mode** (the HPO build). The ``rho`` ladder solves its coefficients
by the same non-negative least squares, with the variable-projection
gradient taken over the passive set (the coefficients above 0). A
coefficient solve that stops before a Kuhn-Tucker point keeps its feasible
coefficients; the ladder counts such solves, and at its end the refit log
and a WARNING give the count and the reason (``RHOFORM NONNEG: 2 of 5311
non-negative least-squares coefficient solves stopped before a Kuhn-Tucker
point (the last: ...)``). REFINE and
FUSE hold every amplitude at :math:`\ge 0` with Ceres lower bounds (and
re-solve the final coefficients non-negatively). With a bump basis (``Blip``,
``Blip5``) an ``INIT PHI`` or ``INIT RHO`` anchor whose width (``MANUAL <mu>
<eta>``) is 0 or less is an error in every form (a radial bump spans :math:`[\mu - w/\eta, \mu + w/\eta]`, so a
width :math:`\le 0` has no support edge; under ``MONOTONE`` a step also needs
``SGRIDMB > 0``): ``tadah refit INIT [phi]: the basis at mu 5 (INIT PHI
anchor 1) has eta -1 <= 0; a radial bump needs a width eta > 0 ...``. Before
1.6.0 only ``MONOTONE`` refused it; in the other forms the anchor was fitted
as its mirror (the bump is even in :math:`\eta`) and written, or the refit
failed with ``zero entry in SGRIDMB``.

In every form the energy-mode joint refinement (FUSE) optimises the radial
bumps (``phi`` and ``rho``) in their outer support edges
:math:`\mu + w/\eta`, bounded at the hard cutoff, and keeps every free bump
width at or above the ladder's width floor (:math:`0.03\,\eta_0`). Every
free bump therefore ends inside the cutoff and the result is the joint
optimum of curves and energy–volume residuals. The bump-support projection
after it normally moves nothing; it still guards a pinned ``INIT`` anchor.
When it does move a bump and the curve-only re-solve of that channel leaves
the energy–volume residual above tolerance (``max |dE|/tol > 1``), the joint
refinement is re-run with the moved bumps held until nothing spills, and the
result with the smaller ``max |dE|/tol`` is kept; the ``joint:`` lines of
``refit_opt.log`` record it. The held re-run passes share the stage's
``MAXEVAL`` with the joint solve (the stage budgets below): each pass gets the
iterations that are left, and with none left the held re-run is skipped and
the linear re-solve result kept (``joint: ... FUSE held re-run skipped:
MAXEVAL 2000 spent by the joint solve, the re-solve result is kept``). Before
tadah 1.6.0 the joint refinement bounded only the bump centres, so a bump
could end past the cutoff, or with a negative width, which the projection
skipped (``tadah hpo`` 1.6.0 refuses such a width in a potential); and the
projection's curve-only re-solve could undo the energy–volume fit: compact
``SIGNED`` refits therefore differ from those of earlier versions wherever
that happened (the dense fit of the density is unchanged).

Under ``REFIT_OBJECTIVE curve`` FUSE refines each function against its own
curve (no energy–volume coupling). On the radial bump channels it does so in
the same outer support edges, bounded at the hard cutoff, with the same width
floor, so the bump-support projection after it moves nothing and FUSE never
ends worse than it started. Before 1.6.0 it bounded only the centres: a bump
pushed past the cutoff was moved back and the coefficients re-solved, which
could leave the curve score worse than before FUSE (0.0948 to 0.1020 on a
test curve).

``REFIT_MODE compact`` needs the compact fitter of the HPO build
(``TADAH_ENABLE_NFP`` with ``TADAH_ENABLE_CERES``). A ``tadah`` built
without it refuses compact mode, written or the default, before it reads
the table:

.. code-block:: none

   tadah refit: REFIT_MODE compact (the default) needs the compact fitter, which this tadah was built without (it needs TADAH_ENABLE_NFP and TADAH_ENABLE_CERES); set REFIT_MODE dense

Before 1.6.0 it fitted the table in dense mode after a WARNING (a defaulted
``Knot5`` basis became ``Blip``).

``refit.eam`` tabulates the fitted density from the inner fit bound up
(the ``full_lo`` of ``refit_range_report.dat``); below it, the original
table shifted by the constant that joins it to the fitted density there,
clamped at 0 under ``NONNEG`` and ``MONOTONE``. The form's guarantees are
those of the native density, from the inner fit bound out. ``tadah analysis bfunc`` plots the basis
functions :math:`\psi_n` of a potential, not the steps :math:`S_n` of a
``MONOTONE`` density.

Embedding reference (``EMBF0``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The native potential declares its :ref:`DM_REAM embedding reference
<dm_ream>`. Under the default ``EMBF0 ZERO`` both fitting modes fit the
zero-referenced embedding :math:`F(\rho) - F(0)` against the zero-referenced
table :math:`F_{tab}(\rho) - F_{tab}(0)` ("fit what you evaluate"), so the
native potential, ``refit.eam`` (its first ``F`` value is exactly 0) and the
self-test all have :math:`F(0) = 0` exactly. Forces, energy differences at
fixed atom count and the cohesive energy are those of the table; absolute energies are referenced
to the isolated atom, i.e. shifted by :math:`-F_{tab}(0)` per atom, which the
log reports (``table F(0) = X eV removed``; for most tables X = 0). Under
``ZERO`` the self-test's ``cohesive`` line is labelled relative to the
isolated atom.

Put ``EMBF0 FREE`` in the refit configuration to fit the raw embedding of
tadah 1.5.0 and earlier (dense mode then pins :math:`F(0)` softly to the
table value, ``REFIT_CONSTRAIN_F0``); the written potential says
``EMBF0 FREE``. The ``refit.hpotarget`` header notes that under ``ZERO``
absolute reference energies need ``tadah hpo --eshift-atom`` or
``--eshift-dbatom``, or an ``ESHIFT`` line in the configuration (the refit
potential with a ``DBFILE`` line: a toggle line of it is not read, since it
is a potential, :ref:`retrain_from_potential`), not ``BIAS true``: its
intercept columns, one per ``ATOM`` species, precede the weights that the
``FIXINDEX``/``FIXWEIGHT`` and ``OPTIM FIXWEIGHT`` lines index.

Configuration reference
-----------------------

The numbers of the configuration are read strictly, before the EAM file:
a real number in the number grammar of the hpotarget file (decimal, no
hexadecimal float, ``inf``, ``nan`` or number below the smallest normal
double) and an integer as decimal digits, each within the range its key
states in the tables below.  A configuration file must also pass the type
check of every Tadah! configuration key, which refuses ``.5`` in every
number and a sign (``+3``, ``-3``) in an integer (``+0.5`` is a valid real
number).  An error names the key and the token as written:

.. code-block:: none

   tadah refit: REFIT_TOL_AMBIENT must be a positive number, got '0'
   tadah refit: REFIT_COMPACT_JITTERS must be an integer from 0 to 2147483646, got '3000000000'

Before 1.6.0 they were read unchecked: ``REFIT_TOL_AMBIENT 0`` made the
energy metric infinite, ``OUTPUT_GRID_SCALE 0`` wrote a 2-point
``refit.eam`` (and so did any scale whose point counts rounded below 2 or
overflowed an ``int``, such as ``0.0001`` or ``1e7``),
``REFIT_COMPACT_OPT_EVERY 0`` and ``INSERT_TOPK 0`` became 1,
and ``REFIT_COMPACT_SEED 3000000000``, a valid seed, failed converting.

.. _refit_mode_keys:

**Keys of the other mode.** ``REFIT_MODE`` is ``compact`` (the default) or
``dense``, in any letter case. Each mode reads only some of the keys below,
and a key that only the other mode reads is an error, before the numbers
are checked, naming the key, the mode in effect and the mode that reads it:

.. code-block:: none

   tadah refit: N2B has no effect under REFIT_MODE compact (the default): only REFIT_MODE dense reads it; remove N2B, or set REFIT_MODE dense
   tadah refit: the REFINE block has no effect under REFIT_MODE dense: only REFIT_MODE compact reads it; remove the REFINE block, or set REFIT_MODE compact
   tadah refit: REFIT_CURVE_WEIGHTING has no effect under REFIT_OBJECTIVE energy (the default): only REFIT_OBJECTIVE curve reads it; remove REFIT_CURVE_WEIGHTING, or set REFIT_OBJECTIVE curve
   tadah refit: REFIT_CURVE_RELAX has no effect under REFIT_CURVE_WEIGHTING false: only REFIT_CURVE_WEIGHTING true (the default) reads it; remove REFIT_CURVE_RELAX, or set REFIT_CURVE_WEIGHTING true
   tadah refit: REFIT_COMPACT_MERGE has no effect under REFIT_BASIS Blip5: only the knot families (REFIT_BASIS Knot5, the compact default, Knot3 or KnotMix) read it; remove REFIT_COMPACT_MERGE, or set REFIT_BASIS Knot5

* Compact mode only: the keys of *Objective and curve weighting* (under
  ``REFIT_MODE dense`` ``REFIT_OBJECTIVE curve`` is an error and ``energy``
  is accepted) and of *Compact ladder*, and the *Stage blocks*.
  The ``REFIT_CURVE_*`` keys are read under ``REFIT_OBJECTIVE curve``
  only, so each is an error under ``REFIT_OBJECTIVE energy`` (written or
  the default), before its value is checked; and ``REFIT_CURVE_RELAX``, the floor of the curve
  weighting, is read only with ``REFIT_CURVE_WEIGHTING true`` (its default),
  so it is an error with ``REFIT_CURVE_WEIGHTING false``.
  ``REFIT_COMPACT_MERGE``, ``REFIT_COMPACT_OPT_EVERY`` and
  ``REFIT_COMPACT_INSERT_TOPK`` are read by the knot families only (the
  default ``Knot5``, ``Knot3``, ``KnotMix``), so each is an error with
  ``REFIT_BASIS Blip`` or ``Blip5`` (in any spelling ``refit`` takes,
  ``M6`` included).
* Dense mode only: the keys of :ref:`refit_dense` (``REFIT_LEVEL``,
  ``N2B``, ``NRHO_BASIS``, ``NF_BASIS``, ``REFIT_MAX_ESCALATIONS``,
  ``REFIT_PLACEMENT``, ``REFIT_CONSTRAIN_F0``). ``REFIT_CONSTRAIN_F0``,
  the :math:`F(0)` pin of an ``EMBF0 FREE`` fit, is read only under
  ``EMBF0 FREE``, so it is an error under ``EMBF0 ZERO`` (written or the
  default), true or false: ``tadah refit: REFIT_CONSTRAIN_F0 has no effect
  under EMBF0 ZERO (the default): only EMBF0 FREE reads it; remove
  REFIT_CONSTRAIN_F0, or set EMBF0 FREE``.
* Neither mode: a top-level ``OPTIMIZER`` block, the grammar before the
  stage blocks. It is refused first, in either mode, with a text that says
  which stage block each of its sub-blocks became (``tadah refit: a
  top-level OPTIMIZER block is no longer supported. ...``); under
  ``REFIT_MODE dense`` the text ends ``Under REFIT_MODE dense, which reads
  no stage block, remove the OPTIMIZER block.``
* Both modes: the other keys.

Only the keys the configuration sets count: the defaults ``tadah`` stores
for the keys it does not set (``REFIT_OBJECTIVE energy``,
``REFIT_CURVE_WEIGHTING true``, ``REFIT_COMPACT_MERGE auto``,
``REFIT_CONSTRAIN_F0 true``, ...) are not refused. Another ``REFIT_MODE``
is an error (``tadah refit: REFIT_MODE must be 'compact' (the default) or
'dense', got 'sparse'``), and so is, before the EAM file is read, a
``REFIT_BASIS`` that names no family (``tadah refit: REFIT_BASIS 'Blipp' is
no basis family of tadah refit: Blip (Blip, BLIP, blip), Blip5 (...), ...``;
``BLIP`` is ``Blip``, as ``BLIP5`` and ``KNOT5`` are theirs), a
``REFIT_LEVEL`` other than ``coarse``, ``balanced``, ``fine`` or
``accurate``, and a ``REFIT_PLACEMENT`` other than ``window`` or
``uniform``, both read in any letter case (``tadah refit: REFIT_LEVEL must
be 'coarse', 'balanced' (the default), 'fine' or 'accurate' (in any letter
case), got 'acurate'``). In 1.5.0 an unknown ``REFIT_BASIS``
(``Blipp``, ``BLIP``) fitted ``Blip`` while a WARNING called it the
experimental Gaussian basis, another ``REFIT_LEVEL`` (``Fine``,
``acurate``) the ``balanced`` preset, and another ``REFIT_PLACEMENT``
(``Uniform``) the window placement. Before 1.6.0 these keys were ignored: dense mode
never read the stage blocks (a ``REFINE`` block with ``THREADS 4`` and
``GTOL -1``, which compact mode refuses, ran), compact mode never read the
dense keys (``N2B 50``, ``REFIT_LEVEL accurate`` and
``REFIT_MAX_ESCALATIONS 9`` gave the same potential as without them),
``REFIT_CURVE_WEIGHTING`` did nothing under ``energy`` although the help
said it was rejected there, nor did ``REFIT_CURVE_RELAX`` and
``REFIT_CURVE_DERIV_WEIGHT`` (or ``REFIT_CURVE_RELAX`` with
``REFIT_CURVE_WEIGHTING false``), the knot-family keys did nothing with
a Blip basis, a top-level ``OPTIMIZER`` block was ignored in
dense mode and refused by the compact fitter only after it had read the
table, and any other ``REFIT_MODE`` (``Dense``, ``COMPACT`` included) ran
the dense fit after a WARNING.

General and output
~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 12 62

   * - Key
     - Default
     - Effect
   * - ``EAM`` (path)
     - —
     - Input setfl EAM file to refit (required), single-element, read and
       checked as the ``SETFL`` file of ``D2_EAM``/``DM_EAM`` is (a malformed
       file is refused, naming the line and the field; see :ref:`SETFL`).
   * - ``RHOFORM``
     - —
     - Density form of the written potential, ``SIGNED`` | ``NONNEG`` |
       ``MONOTONE`` (required; see :ref:`refit_rhoform`).
   * - ``OUTFILE`` (path)
     - ``pot.tadah``
     - Output native Tadah! potential.
   * - ``SETFL_OUT`` (path)
     - ``refit.eam``
     - Round-trip setfl written from the fitted model for LAMMPS comparison.
   * - ``HPOFILEOUT`` (path)
     - ``refit.hpotarget``
     - Generated ``tadah hpo`` starter target.
   * - ``REFIT_LOG`` (path)
     - ``refit_opt.log``
     - Live, line-flushed optimisation log; the self-test is at its end.
   * - ``OUTPUT_GRID_SCALE`` (float)
     - ``1.0``
     - Output setfl grid density relative to the input table; > 0, and the
       grid it gives (nr × scale and nrho × scale points, rounded) must have
       3 to 2147483647 points per table, checked once the EAM file is read,
       before any fitting (``tadah refit: OUTPUT_GRID_SCALE '0.0001' for
       refit.eam: EAMSetflIO: a grid scale of 0.0001 gives nrho = 1 and
       nr = 1 points ...``).
   * - ``OPTIM_PCT`` (float)
     - ``5.0``
     - ±% half-width of the OPTIM bounds written into the HPO target; > 0
       (also with ``--edit`` and ``--retarget``, checked before any file is
       written).

Mode and basis
~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 26 12 62

   * - Key
     - Default
     - Effect
   * - ``REFIT_MODE``
     - ``compact``
     - ``compact`` fits a small optimised basis via the ladder; ``dense`` fits fixed grids (see :ref:`refit_dense`). In any letter case; each mode refuses the keys only the other reads (above), and a ``tadah`` built without the compact fitter refuses ``compact``.
   * - ``REFIT_BASIS``
     - ``Knot5``
     - Basis family (compact): ``Knot5`` quintic knots, ``Knot3`` cubic knots, ``KnotMix`` per-knot cubic/quintic, ``Blip``, ``Blip5``. Knot5 is smoothest per knot; Knot3 captures sharper features. Best family is element-dependent.

Objective and curve weighting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Compact mode only (``REFIT_MODE dense`` refuses these keys, and accepts
``REFIT_OBJECTIVE energy``).

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Key
     - Default
     - Effect
   * - ``REFIT_OBJECTIVE``
     - ``energy``
     - ``curve`` fits the tabulated functions (value + derivatives) with pressure weighting; ``energy`` fits the crystal energy–volume curve. Use ``curve`` for a faithful crystal from a small basis.
   * - ``REFIT_CURVE_WEIGHTING``
     - ``true``
     - (curve) Weights :math:`\phi,\rho` fully between the equilibrium nearest-neighbour distance and the cutoff, and :math:`F` from zero to the equilibrium density, relaxing toward compression — tighter equilibrium energies for a given basis size. Refused under ``REFIT_OBJECTIVE energy``, written or the default (before 1.6.0 it was ignored there).
   * - ``REFIT_CURVE_RELAX``
     - ``0.05``
     - (curve, weighting on) Floor weight in the compressed region, in (0, 1]; smaller focuses the fit harder on the equilibrium region. Refused under ``REFIT_OBJECTIVE energy`` and with ``REFIT_CURVE_WEIGHTING false`` (before 1.6.0 it was ignored there).
   * - ``REFIT_CURVE_DERIV_WEIGHT``
     - ``0.0``
     - (curve) Weight of the 1st/2nd-derivative terms relative to value, >= 0. ``0`` fits values only (best crystal energies); raise to fit slope/curvature shape. Refused under ``REFIT_OBJECTIVE energy`` (before 1.6.0 it was ignored there).

Compact ladder
~~~~~~~~~~~~~~

Compact mode only (``REFIT_MODE dense`` refuses these keys).

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Key
     - Default
     - Effect
   * - ``REFIT_COMPACT_NMAX``
     - ``40``
     - Per-function basis ceiling: one integer from 1 (all functions) or three ``phi rho F`` (e.g. ``35 12 35``). Higher is more flexible and slower. The sample grid of at least 4×NMAX points must fit 2147483647, so the largest NMAX is at most 536870911 (checked with the other numbers, before the EAM file is read).
   * - ``REFIT_COMPACT_SAMPLES``
     - ``800``
     - Sample points per function for the fit and RMSE, from 1 (raised to at least 50 and 4×NMAX). More is finer and slower.
   * - ``REFIT_COMPACT_OPT_EVERY``
     - ``1``
     - Run the full per-rung optimisation every K-th rung (K from 1); intermediate rungs grow by insertion only. Higher is faster and coarser. Knot families only: a Blip fit optimises every rung, and ``REFIT_BASIS Blip``/``Blip5`` refuses the key.
   * - ``REFIT_COMPACT_STALL``
     - ``12``
     - Stop a function's ladder after this many rungs without improvement, from 1 (Blip families: 6).
   * - ``REFIT_COMPACT_INSERT_TOPK``
     - ``3``
     - Try this many top residual peaks per rung and keep the best insertion, from 1. Knot families only: a Blip fit inserts at the curve peak, and ``REFIT_BASIS Blip``/``Blip5`` refuses the key.
   * - ``REFIT_COMPACT_JITTERS``
     - ``2``
     - Randomised restarts per rung (jitters+1 candidates), from 0 to 2147483646; more explores the landscape harder.
   * - ``REFIT_COMPACT_SHARE``
     - ``0.5``
     - Per-function share of the tolerance budget used by the rung-selection metric; > 0.
   * - ``REFIT_COMPACT_SEED``
     - ``12345``
     - RNG seed for the jittered restarts, from 0 to 4294967295; fixes the run for reproducibility.
   * - ``REFIT_COMPACT_MERGE``
     - ``auto``
     - Knot coalescence guard: penalises near-coincident knot pairs during EXPLORE and merges them after REFINE, re-inserting a fresh knot at the worst residual. ``auto`` is off for ``curve`` and on for ``energy``; ``true``/``false`` force it. Knot families only: a Blip fit has no knots, and ``REFIT_BASIS Blip``/``Blip5`` refuses the key.

Window and self-test
~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Key
     - Default
     - Effect
   * - ``REFIT_TOL_AMBIENT`` (meV/atom)
     - ``0.1``
     - Pass threshold for the ambient band (:math:`a/a_0\,0.95\!-\!1.10`), > 0; also weights the energy-mode metric there.
   * - ``REFIT_TOL_COMPRESSED`` (meV/atom)
     - ``1.0``
     - Pass threshold for the compressed band (:math:`a/a_0\,0.75\!-\!0.95`), > 0.
   * - ``REFIT_REACH_SMIN``
     - ``0.75``
     - Smallest :math:`a/a_0` the fit must stay faithful at, from 0.1 to 1; sets the reachable :math:`r`/:math:`\rho` window and the span of the self-test and the E-V scans. Their neighbour lists grow as :math:`s_{\min}^{-3}` (0.01 took about a minute, 1e-6 did not finish), and the reachability probe lists neighbours only out to :math:`r_{cut}/0.1`.
   * - ``REFIT_REACH_MARGIN``
     - ``1.25``
     - Safety margin on the reachable :math:`F(\rho)` window; > 0.
   * - ``REFIT_R_LO_FRAC``
     - ``0.20``
     - Inner radial fit bound as a fraction of the cutoff, in (0, 1) (at least the table's dr; compact mode fits from max(0.8 r_reach, this bound)); also the lower edge of the :math:`\phi/\rho` weight ramp. Below the inner bound ``refit.eam`` carries the original table (*Output files*), and ``pot.tadah`` is not valid there.

Stage blocks
~~~~~~~~~~~~

Four optional multi-line blocks tune the pipeline of compact mode
(``REFIT_MODE dense`` refuses them); omit any to use defaults.

**INIT** declares basis placements (no optimisation), one per line:

.. code-block:: none

   INIT
     <PHI|RHO|F>  <placement>  [PIN]
   ENDINIT

``placement`` is ``MANUAL <mu> [<eta>]`` (fixed centre, plus width for Blip
families), ``XMIN`` / ``XMAX`` (window edges; ``XMAX`` applies to ``F`` only),
``YMIN`` / ``YMAX`` (target minimum/maximum location) or ``SMAX`` / ``CMAX``
(maximum slope/curvature location). ``PIN`` freezes that centre through all
later stages (its coefficient is still fitted). Pinning the window edges sets
the extrapolation behaviour.

``tadah explain refit.init`` and ``tadah explain refit.optimizer`` print
the keys of the INIT and OPTIMIZER blocks below in the terminal.

**EXPLORE / REFINE / FUSE** each accept ``ENABLED true|false`` (REFINE/FUSE;
``ENABLED false`` skips the stage) and a nested ``OPTIMIZER`` block, but not
both ``ENABLED false`` and an ``OPTIMIZER`` block, which would have no
effect: that is an error (before 1.6.0 the block was ignored with a
warning).  To keep a block and skip its stage, write ``ALGO NONE`` in it:

.. code-block:: none

   REFINE: ENABLED false skips the stage, so its OPTIMIZER block has no effect; remove the OPTIMIZER block, or write ALGO NONE in it instead of ENABLED false

EXPLORE
also accepts ``MAX_ADD`` (one value or three ``phi rho F``) capping the basis
functions added in that stage: each an integer from 0 to 2147483647.  Leave
``MAX_ADD`` out for no cap; a negative value, which used to mean no cap, is
an error (``EXPLORE MAX_ADD must be an integer from 0 to 2147483647, got
'-1'``).

.. code-block:: none

   EXPLORE
     MAX_ADD 45
     OPTIMIZER
       LIB NLOPT  ALGO GN_MLSL_LDS  MAXEVAL 1000000
       INNER
         LIB NLOPT  ALGO LN_SBPLX  MAXEVAL 50000  FTOL_REL 1e-7
       ENDINNER
     ENDOPTIMIZER
   ENDEXPLORE
   REFINE
     OPTIMIZER
       LIB CERES  ALGO LM  MAXEVAL 1000  FTOL_REL 1e-14  GTOL 1e-14
     ENDOPTIMIZER
   ENDREFINE
   FUSE
     OPTIMIZER
       LIB CERES  ALGO DOGLEG  MAXEVAL 2000  FTOL_REL 1e-14
     ENDOPTIMIZER
   ENDFUSE

Legal optimisers and the stop-criteria each one reads.  A block takes
only the stop-criteria of its ``ALGO``; a block with ``ALGO NONE`` or
``UNIFORM`` runs no optimiser and takes none, except those another stage
reads (below):

.. list-table::
   :header-rows: 1
   :widths: 18 48 34

   * - Stage
     - LIB / ALGO
     - Stop-criteria read
   * - EXPLORE outer
     - NLOPT ``GN_MLSL_LDS`` | ``GN_DIRECT_L`` | ``GN_CRS2_LM``; DLIB ``GLOBAL``
     - ``MAXEVAL``
   * - EXPLORE outer
     - ``UNIFORM``
     - none
   * - EXPLORE ``INNER``
     - NLOPT ``LN_SBPLX`` | ``LN_NELDERMEAD`` | ``LN_BOBYQA`` | ``LN_PRAXIS`` | ``LD_LBFGS``
     - ``MAXEVAL``, ``FTOL_REL``, ``XTOL_REL``
   * - EXPLORE ``INNER``
     - DLIB ``LBFGS``
     - ``MAXEVAL``, ``FTOL_ABS``
   * - EXPLORE ``INNER``
     - ``NONE``
     - none
   * - REFINE, FUSE
     - CERES ``LM`` | ``DOGLEG``
     - ``MAXEVAL`` (alias ``MAXITER``), ``FTOL_REL``, ``GTOL``
   * - REFINE
     - NLOPT ``LD_LBFGS``
     - ``MAXEVAL``, ``FTOL_REL``
   * - REFINE
     - DLIB ``LBFGS``
     - ``MAXEVAL``, ``FTOL_ABS``
   * - REFINE, FUSE
     - ``NONE``
     - none

Under the EXPLORE optimiser ``GN_MLSL_LDS`` an ``INNER`` block also takes
``MAXEVAL``, ``FTOL_REL`` and ``XTOL_REL`` whatever its ``ALGO``: they set
MLSL's local ``LN_SBPLX`` searches too.  ``FTOL_ABS`` is the absolute
change of the objective at which dlib's ``LBFGS`` stops (the name
``tadah hpo``'s ``DLIB:BFGS`` gives it); before 1.6.0 dlib took
``FTOL_REL`` for that absolute change, and under ``GN_MLSL_LDS`` an
``INNER`` dlib ``LBFGS`` block's ``FTOL_REL`` set both, so such a run
without ``FTOL_ABS`` gives different results now.  The defaults are
unchanged (1e-13 in ``INNER``, 1e-15 in REFINE).

``MAXEVAL`` counts objective evaluations, exactly, for every NLopt and
dlib algorithm: each call of the stage's objective, for its value or its
gradient, is one.  dlib asks for the value and the gradient of its
objective in separate calls, so its L-BFGS counts both, in the objective
wrapper; the call past ``MAXEVAL`` ends the search at its last iterate.
Ceres' ``LM`` and ``DOGLEG`` count their solver iterations instead
(``max_num_iterations``), each one Jacobian evaluation and one or more
residual evaluations; ``MAXITER``, the alias of ``MAXEVAL`` for ``LM`` and
``DOGLEG`` only, says so (with an L-BFGS REFINE, where it set the
evaluation budget before 1.6.0, it is an error naming ``MAXEVAL``).
Without ``MAXEVAL`` each optimiser takes its default budget:

.. list-table::
   :header-rows: 1
   :widths: 28 44 28

   * - Stage
     - ``MAXEVAL`` / ``MAXITER`` counts
     - Default
   * - EXPLORE (the seed)
     - evaluations of ``GN_MLSL_LDS`` (its local searches included),
       ``GN_DIRECT_L``, ``GN_CRS2_LM``; of dlib ``GLOBAL``
     - 60000; 3000 for ``GLOBAL``
   * - EXPLORE ``INNER``
     - evaluations of each candidate's local search (and of each local
       search of ``GN_MLSL_LDS``)
     - 20000
   * - REFINE
     - evaluations of ``LD_LBFGS`` and dlib ``LBFGS``; iterations of
       ``LM`` and ``DOGLEG``
     - 50000 evaluations; 1000 iterations
   * - FUSE
     - iterations of ``LM`` and ``DOGLEG``: in energy mode over the whole
       stage, the joint solve and the held re-run of *Compact mode* (a
       fallback, up to one more solve per radial bump) sharing it (before
       1.6.0 each held pass had the full budget); under ``REFIT_OBJECTIVE
       curve`` of each function's refine, so phi, rho and F each get the
       whole budget
     - 2000

Before 1.6.0 dlib ``GLOBAL`` took ``max(500, MAXEVAL/20)`` function calls,
dlib ``LBFGS`` capped its L-BFGS iterations (at ``MAXEVAL`` in EXPLORE
``INNER``, 50 × ``MAXEVAL`` in REFINE) rather than its evaluations, and
REFINE ``LD_LBFGS`` took 50 × ``MAXEVAL`` evaluations, so a refit with
one of them and an explicit ``MAXEVAL`` gives different results now.
Without ``MAXEVAL`` the defaults of ``GLOBAL`` (3000) and of REFINE
``LD_LBFGS`` (50000) are the budgets they had; dlib ``LBFGS`` keeps the
numbers of its old iteration caps (20000 in EXPLORE ``INNER``, 50000 in
REFINE), now counted in evaluations, at least two per L-BFGS iteration
(its value and its gradient), so a default dlib ``LBFGS`` run whose cap
binds stops sooner than before.
``FTOL_REL`` on the Ceres stages is Ceres' relative function tolerance,
and ``GTOL`` its gradient tolerance.  FUSE's settings are those of the
joint solve and, under ``REFIT_OBJECTIVE curve``, of each function's FUSE
refine.  Before 1.6.0 the curve FUSE refine ran with REFINE's
``MAXEVAL``/``MAXITER`` (an L-BFGS REFINE's evaluations read as Ceres
iterations), ``FTOL_REL`` and ``GTOL``, also with REFINE ``ENABLED
false``, and FUSE's own had no effect; FUSE ``GTOL`` was ignored, and the
joint solve's gradient tolerance was 1e-14, still the default.  Curve
refits that set those keys give different results now, and so does one
whose FUSE refine reached REFINE's default 1000 iterations (FUSE's
default is 2000).  The refit log states each stage's budget in its unit
and each stage's termination with the evaluations it spent
(``term=DLIB_GLOBAL:MAXEVAL_REACHED(3000ev)``), and in curve mode each
function's FUSE refine with its Ceres iterations
(``FUSE[phi] curve score 0.0304 -> 0.0287 (NO_CONVERGENCE(501it, maxiter))``).  A stage's
``OPTIMIZER`` block takes only the keys the stage reads — ``LIB``, ``ALGO``,
the stop-criteria of this table, and in EXPLORE the ``INNER`` block.  Any
other optimiser attribute of :ref:`tadah hpo <nfp_manual>` (``THREADS``,
``MAXTIME``, ``STOPVAL``, ``POPULATION``, ``STEP``, ``PARAM``, …) and an
``INNER`` block in REFINE or FUSE is an error that names the keys the stage
reads; before 1.6.0 they were accepted and did nothing:

.. code-block:: none

   REFINE: THREADS is not used by a tadah refit stage (it reads LIB, ALGO, MAXEVAL/MAXITER, FTOL_REL, FTOL_ABS, GTOL)
   EXPLORE INNER: MAXTIME is not used by a tadah refit stage (it reads LIB, ALGO, MAXEVAL, FTOL_REL, FTOL_ABS, XTOL_REL)

``SEED`` is rejected in every stage's ``OPTIMIZER`` block, ``INNER``
included, with a pointer to ``REFIT_COMPACT_SEED``, which seeds the refit's
random numbers; ``FTOL_ABS`` with a Ceres ``ALGO`` points to ``FTOL_REL``:

.. code-block:: none

   EXPLORE: SEED is not accepted in a tadah refit OPTIMIZER block: the refit's random numbers come from REFIT_COMPACT_SEED; set REFIT_COMPACT_SEED in the configuration instead.

Within a stage, a stop criterion that the block's ``ALGO`` does not read
(the table above) is an error, named as written with the ``ALGO`` and the
keys it reads; ``MAXITER`` with an ``ALGO`` that counts evaluations names
``MAXEVAL``, and ``FTOL_REL`` with dlib's ``LBFGS`` names ``FTOL_ABS``.
Before 1.6.0 they were accepted: ``XTOL_REL`` with an ``INNER`` dlib
``LBFGS`` had no effect (but under ``GN_MLSL_LDS``, whose local searches
it set); ``GTOL`` with an L-BFGS REFINE had none on REFINE, but under
``REFIT_OBJECTIVE curve`` it was the gradient tolerance of the FUSE curve
refine (above); ``MAXITER`` with an L-BFGS REFINE set its evaluation
budget; and ``FTOL_REL`` with dlib's ``LBFGS`` was dlib's absolute stop,
which ``FTOL_ABS`` sets now:

.. code-block:: none

   REFINE: GTOL has no effect with ALGO LD_LBFGS (it reads MAXEVAL, FTOL_REL); remove GTOL
   EXPLORE INNER: XTOL_REL has no effect with ALGO LBFGS (it reads MAXEVAL, FTOL_ABS); remove XTOL_REL
   REFINE: MAXITER counts Ceres iterations; ALGO LD_LBFGS counts objective evaluations: write MAXEVAL
   REFINE: FTOL_REL has no effect with ALGO LBFGS: dlib's L-BFGS stops on an absolute change of the objective; write FTOL_ABS

The ``optimiser:`` line of ``refit_opt.log`` gives each stage's settings
as its ``ALGO`` reads them, the MLSL seed with those of its local searches
(``EXPLORE.seed{algo=MLSL maxeval=60000 local=LN_SBPLX{maxeval=20000
ftol_rel=1e-13 xtol_rel=1e-11}}``).

Every ``OPTIMIZER`` block, and an EXPLORE ``INNER`` block, must set
``ALGO``.  To switch a search off, write it out: ``ALGO NONE`` in an
``INNER`` block (no local search), in REFINE or FUSE (the stage is
skipped), and ``ALGO UNIFORM`` (or ``NONE``) for the EXPLORE optimiser (no
global search).  A block switched off runs no optimiser, so a stop
criterion in it would have no effect: ``MAXEVAL``/``MAXITER``,
``FTOL_REL``, ``FTOL_ABS``, ``XTOL_REL`` and ``GTOL`` there are errors, named as
written, as ``LIB`` is (below).  An EXPLORE ``INNER`` block's
``MAXEVAL``, ``FTOL_REL`` and ``XTOL_REL`` stay accepted with ``ALGO
NONE`` under the EXPLORE optimiser ``GN_MLSL_LDS``, whose local searches
they also set, and another stop criterion there is an error that names
those searches as what reads them (``EXPLORE INNER: FTOL_ABS has no effect
with ALGO NONE (no local search runs; under ALGO GN_MLSL_LDS the block sets
MLSL's local LN_SBPLX searches, which read MAXEVAL, FTOL_REL, XTOL_REL);
remove FTOL_ABS``); before 1.6.0 REFINE's were accepted under
``REFIT_OBJECTIVE curve`` too (the FUSE curve refine ran with them).  EXPLORE's ``INNER``
block configures the per-rung local search whatever the EXPLORE
optimiser, ``UNIFORM`` included.  Before 1.6.0 these keys were accepted
and ignored:

.. code-block:: none

   REFINE: MAXITER has no effect with ALGO NONE (the stage is skipped); remove MAXITER
   EXPLORE: MAXEVAL has no effect with ALGO UNIFORM (no global search runs); remove MAXEVAL
   EXPLORE INNER: FTOL_REL has no effect with ALGO NONE (no local search runs); remove FTOL_REL

An ``INNER`` block without ``ALGO`` used to mean no local
search, while an EXPLORE ``OPTIMIZER`` block without an ``INNER`` block runs
the default one (``LN_SBPLX``); leaving a stage's whole ``OPTIMIZER`` block
out still runs the stage with its defaults:

.. code-block:: none

   EXPLORE INNER: ALGO is not set; set ALGO in the INNER block (NONE for no local search)

As in ``tadah hpo``, where every block, ``INNER`` included, sets both
``LIB`` and ``ALGO``, a block that runs an optimiser names the library of
its ``ALGO`` in ``LIB``:

.. list-table::
   :header-rows: 1
   :widths: 18 52 30

   * - Stage
     - ``ALGO`` (fitter name / NLopt name)
     - ``LIB``
   * - EXPLORE
     - ``GN_MLSL_LDS`` / ``MLSL``, ``GN_DIRECT_L`` / ``DIRECT``,
       ``GN_CRS2_LM`` / ``CRS2``
     - ``NLOPT``
   * - EXPLORE
     - ``GLOBAL`` / ``DLIB_GLOBAL``
     - ``DLIB``
   * - EXPLORE ``INNER``
     - ``LN_SBPLX`` / ``SBPLX``, ``LN_NELDERMEAD`` / ``NELDERMEAD``,
       ``LN_BOBYQA`` / ``BOBYQA``, ``LN_PRAXIS`` / ``PRAXIS``, ``LD_LBFGS``
       / ``NLOPT_LBFGS``
     - ``NLOPT``
   * - EXPLORE ``INNER``, REFINE
     - ``LBFGS`` / ``DLIB_LBFGS``
     - ``DLIB``
   * - REFINE
     - ``LD_LBFGS`` / ``NLOPT_LBFGS``
     - ``NLOPT``
   * - REFINE, FUSE
     - ``LM`` / ``CERES_LM``, ``DOGLEG`` / ``CERES_DOGLEG``
     - ``CERES``
   * - every stage
     - ``NONE``, and ``UNIFORM`` in EXPLORE (EXPLORE: no global search;
       ``INNER``: no local search; REFINE, FUSE: the stage is skipped)
     - none: ``LIB`` is an error

The ``ALGO`` names are the refit's own, not those of ``tadah hpo``: the
refit's dlib global search is ``GLOBAL`` (``tadah hpo``'s ``DLIB GFS``),
and ``LBFGS`` is dlib's L-BFGS (NLopt's is ``LD_LBFGS``; in ``tadah hpo``
``LBFGS`` is a ``CERES`` algorithm).

The refit's ``MLSL``, ``DIRECT`` and ``CRS2`` are NLopt's ``GN_MLSL_LDS``,
``GN_DIRECT_L`` and ``GN_CRS2_LM``.  ``GN_MLSL`` (MLSL with pseudo-random
starts) and ``GN_DIRECT`` (DIRECT without the local bias of ``DIRECT_L``)
are other NLopt algorithms, which ``tadah hpo`` runs as named, and
``GN_CRS2`` is no NLopt algorithm: the refit refuses all three, with any
``LIB``, naming the algorithm it runs.  Before 1.6.0 they ran
``GN_MLSL_LDS``, ``GN_DIRECT_L`` and ``GN_CRS2_LM``:

.. code-block:: none

   EXPLORE: ALGO GN_MLSL is not run by tadah refit (NLopt's MLSL with pseudo-random starts); its MLSL is GN_MLSL_LDS: set ALGO GN_MLSL_LDS

A missing ``LIB`` and one that does not match the ``ALGO`` are errors
naming the right one.  The ``LIB`` given may be the one meant, so when the
stage has algorithms of that library the message names them too.  A
``LIB`` in a block with ``ALGO NONE`` or ``UNIFORM``, which runs no
optimiser, would have no effect and is an error too:

.. code-block:: none

   REFINE: LIB is not set; set LIB NLOPT (ALGO LD_LBFGS is an NLopt algorithm)
   EXPLORE: LIB CERES does not match ALGO GN_MLSL_LDS, an NLopt algorithm; set LIB NLOPT
   REFINE: LIB NLOPT does not match ALGO LBFGS, a dlib algorithm; set LIB DLIB, or with LIB NLOPT set ALGO LD_LBFGS
   EXPLORE INNER: LIB has no effect with ALGO NONE (no local search runs); remove LIB
   EXPLORE: LIB has no effect with ALGO UNIFORM (no global search runs); remove LIB

Before 1.6.0 ``LIB`` could be left out where the ``ALGO`` named the
optimiser (``GN_MLSL_LDS``, ``LN_SBPLX``, ``LM``, ``DLIB_GLOBAL``, and
``LD_LBFGS`` in an EXPLORE ``INNER`` block) but not elsewhere
(``LD_LBFGS`` in REFINE), and a ``LIB`` that did not match the ``ALGO``
was ignored (``LIB CERES ALGO GN_MLSL_LDS`` ran MLSL), and ``UNIFORM``,
the refit's own uniform start, took any ``LIB`` (``LIB FOO`` included)
and ignored it.  The numbers are read as strictly as in
``tadah hpo`` (:ref:`nfp_manual`): ``MAXEVAL`` and ``MAXITER`` take
decimal digits only, from 1 to 2147483647 (the ``int`` budget the refit
hands NLopt, dlib and Ceres; ``MAXEVAL 0`` ran Ceres with no iteration and
NLopt without a limit), the tolerances and ``GTOL`` a finite number >= 0 in
decimal form (no hexadecimal float such as ``0x1p-40``; 0 switches the test
off).  Before 1.6.0 a negative tolerance was accepted: Ceres refused it and
returned without iterating, and the refit logged that as the stage's stop
term (``refine=FAILURE(0it, Invalid configuration...)``), so REFINE or FUSE
did nothing; NLopt switches a negative tolerance off.  Every error of a
stage's ``OPTIMIZER`` block names the block, an ``INNER`` block's as
``EXPLORE INNER``:

.. code-block:: none

   REFINE: FTOL_REL must be a finite number, got '0x1p-40'
   REFINE: GTOL must be a number >= 0, got '-1'
   EXPLORE INNER: MAXEVAL must be an integer from 1 to 2147483647, got '4294967299'

Output files
------------

* ``pot.tadah`` — fitted native potential.
* ``refit.eam`` (``SETFL_OUT``) — round-trip setfl for LAMMPS: the fitted
  functions from the inner fit bound up (the ``full_lo`` of
  ``refit_range_report.dat``: ``REFIT_R_LO_FRAC`` × rcut, at least the
  table's dr, in dense mode; max(0.8 × r_reach, that) in compact mode);
  below it the original table's :math:`\phi` and :math:`\rho`, each shifted
  by the constant that joins it to the fit there (:math:`\rho` clamped at 0
  under ``NONNEG`` and ``MONOTONE``). ``pot.tadah`` is valid from the inner
  fit bound out only.
* ``refit.hpotarget`` — ``tadah hpo`` starter target.  Its ``OPTIM`` groups
  are written as ``CGRID2B``, ``SGRID2B``, ``CGRIDMB``, …, ``FIXWEIGHT``;
  ``tadah hpo`` orders the dimensions by key whatever the order of the
  lines (:ref:`dimension order <hpo_dimension_order>`).
* ``refit_opt.log`` — optimisation log and crystal self-test.
* ``refit_basis_info.dat`` — final basis: function, centre, width, coefficient, degree.
* ``refit_curve_{phi,rho,F}.dat`` — fitted vs tabulated curves and derivatives.
* ``refit_compare.dat``, ``refit_ladder_trace.dat`` — energy–volume comparison, per-rung ladder trace.
* ``refit_range_report.dat`` — each function's range (``full_lo``,
  ``full_hi``) and the window the fit used (``fit_window_lo``,
  ``fit_window_hi``). For :math:`\phi` and :math:`\rho` the compact fit
  fits on its whole range, from ``full_lo`` = max(0.8 × r_reach,
  ``REFIT_R_LO_FRAC`` × rcut, the table's dr) to the cutoff, r_reach being
  the nearest-neighbour distance at ``REFIT_REACH_SMIN`` (the report's
  ``r_1nn-compressed``). The dense fit's ``full_lo`` is
  ``REFIT_R_LO_FRAC`` × rcut (at least dr); with ``REFIT_PLACEMENT window`` its window
  starts at the edge of a steep wall in the table, at most 0.8 × r_reach
  and never below ``full_lo`` (``full_lo`` for a table without such a
  wall), and a small uniform basis covers the rest down to ``full_lo``;
  with ``REFIT_PLACEMENT uniform`` it starts at ``full_lo``.
  ``refit_curve_{phi,rho,F}.dat`` flag the same windows (``in_window``).
  Before 1.6.0 ``fit_window_lo`` was 0.9 × r_reach in both modes (``full_lo``
  under ``REFIT_PLACEMENT uniform``), which is neither fit's window, and the
  curve files flagged r >= 0.9 × r_reach (under ``REFIT_PLACEMENT uniform``
  r >= 0.15 × rcut, every row of the files, whatever ``full_lo`` was).
  The note of its ``F`` row says what lies above the embedding window:
  after a compact fit ``refit.eam`` carries the original table there
  (offset-matched); the dense fit keeps a small uniform tail basis there
  (``REFIT_PLACEMENT window``) or fits one uniform basis on the full range
  (``REFIT_PLACEMENT uniform``). Before 1.6.0 every fit had the tail
  basis note.

.. _refit_dense:

Dense mode
----------

``REFIT_MODE dense`` fits fixed basis grids instead of the ladder; set the
grid sizes directly. The keys of this table are read by dense mode only:
``REFIT_MODE compact`` refuses them (:ref:`Keys of the other mode
<refit_mode_keys>`). Large dense grids reach very high accuracy, but every
basis function becomes a search dimension in :ref:`nested fitting
<nested_fitting>` — a high-count dense potential makes the downstream
``tadah hpo`` search much larger. Prefer compact mode (a small optimised basis)
when the refit will be tuned further.

.. list-table::
   :header-rows: 1
   :widths: 28 12 60

   * - Key
     - Default
     - Effect
   * - ``REFIT_LEVEL``
     - ``balanced``
     - Preset grid density: ``coarse`` | ``balanced`` | ``fine`` | ``accurate``,
       in any letter case; another value is an error.
   * - ``N2B``
     - —
     - :math:`\phi(r)` basis count, from 1 to 1024 (the escalation's cap).
   * - ``NRHO_BASIS``
     - —
     - :math:`\rho(r)` basis count, from 1 to 1024. Under ``RHOFORM NONNEG``/``MONOTONE``
       the density solve is non-negative least squares, whose cost grows
       faster with the count (see :ref:`refit_rhoform`).
   * - ``NF_BASIS``
     - —
     - :math:`F(\rho)` basis count, from 1 to 1024.
   * - ``REFIT_MAX_ESCALATIONS``
     - ``4``
     - Automatic basis-escalation rounds while above tolerance, from 0.
   * - ``REFIT_CONSTRAIN_F0``
     - ``true``
     - Under ``EMBF0 FREE``: pin the raw :math:`F(0)` softly to the table
       value. An error under the default ``EMBF0 ZERO``, where
       :math:`F(0) = 0` by construction.
   * - ``REFIT_PLACEMENT``
     - ``window``
     - ``window`` places the basis on the reachable window; ``uniform``
       spreads it over the full range; in any letter case, another value
       is an error. (The compact fit always fits on the reachable windows.)

Dense ``REFIT_BASIS`` families are ``Blip`` | ``Blip5`` | ``Gaussian``
(``Gaussian``, ``gaussian``, ``BP``, ``D2_BP``: experimental, with a
WARNING); a spelling of no family is an error.

.. _refit_edit:

Editing a refitted potential (``--edit``)
-----------------------------------------

``tadah refit --edit`` adds or removes ONE basis function of one curve of an
existing refit potential — no EAM input, no new fit::

   # insert a pair-function basis function at r = 2.75 A
   tadah refit --edit pot.tadah --func phi --add 2.75

   # remove the embedding basis function nearest rho = 0.9
   tadah refit --edit pot.tadah --func F --remove 0.9

``--func`` selects the curve: ``phi`` (pair, ``CGRID2B``), ``rho`` (density,
``CGRIDMB`` / ``AMPGRIDMB``) or ``F`` (embedding, ``CEMBFUNC``). Exactly one
of ``--add <centre>`` / ``--remove <centre>`` is required. Both knot
(``Knot5`` / ``Knot3``) and blip (``Blip`` / ``Blip5``) family pair
descriptors are supported; ``D2_mJoin`` and Gaussian potentials are not,
and ``BIAS`` / ``NORM`` must be false (their columns are WEIGHTS-aligned
and cannot survive an edit).

The many-body term is **optional**:

* *pure two-body potentials* (``TYPE2B`` only — e.g. hand-built knot pair
  potentials) are fully supported through ``--func phi``;
* *DM_REAM potentials* (as written by ``tadah refit``) expose all three
  curves;
* *any other many-body descriptor* restricts editing to ``--func phi`` —
  the many-body keys and their WEIGHTS block ride along untouched (the
  pair block is always the WEIGHTS prefix).

**Adding** inserts the function with coefficient 0, so the edited potential
predicts **bit-identically** to the input — value, every derivative, and the
extrapolation region. The new coefficient only adds capacity: it appears as
an extra ``OPTIM FIXWEIGHT`` line in the regenerated hpotarget, ready for a
``tadah hpo`` search. Knot families take an optional ``--deg 3|5`` (default
quintic; 3 materialises the per-knot ``KDEG*`` degree arrays) and reject
``--eta`` (the knot width is a pure gauge). Blip families take an optional
``--eta`` (default: the mean width of the two nearest neighbours); a blip
whose support lies entirely beyond ``RCUT`` is rejected.

**Removing** deletes the basis function whose centre is nearest the given
value (the choice is reported). Unless its coefficient is exactly 0, the two
remaining nearest-in-centre coefficients are least-squares adjusted against
the removed contribution's value, first and second derivative over the region
it controlled (per-derivative-order normalisation keeps the orders balanced).
Removal is inherently lossy — the log reports the residual before and after
compensation; expect observables to change and re-validate the potential.
Removing the outermost radial knot also shrinks the effective cutoff and the
uncovered tail cannot be compensated (warned).

``F`` edits act in the potential's ``EMBF0`` reference (the log states it). Under
``EMBF0 ZERO`` the ``F`` curve is :math:`F(\rho) - F(0)`: removing a function
with :math:`g(0) \neq 0`, or adjusting a compensating function with
:math:`g(0) \neq 0`, changes ``F`` by a constant over the whole ``F`` domain,
so the compensation region is then widened to :math:`[0,` largest support
:math:`]` (logged). A potential without an ``EMBF0`` line is read as ``ZERO``
(a WARNING says so; add ``EMBF0 FREE`` to the input to edit a tadah 1.5.0 fit
in its raw reference), and the edited potential always declares its mode.

.. _refit_output_mode_keys:

A configuration passed with ``-c`` to ``--edit`` or ``--retarget`` may not
set a key that only a refit of an EAM file reads: those modes fit nothing,
and such a key is an error, before any file is read or written, naming the
key and what reads it:

.. code-block:: none

   tadah refit --edit: N2B has no effect under --edit: only a refit of an EAM file in REFIT_MODE dense reads it; remove N2B
   tadah refit --edit: REFIT_TOL_AMBIENT has no effect under --edit: only a refit of an EAM file reads it; remove REFIT_TOL_AMBIENT
   tadah refit --retarget: the REFINE block has no effect under --retarget: only a refit of an EAM file in REFIT_MODE compact reads it; remove the REFINE block

The keys of either ``REFIT_MODE`` (``REFIT_MODE``, ``REFIT_BASIS``,
``REFIT_TOL_*``, ``REFIT_REACH_*``, ``REFIT_R_LO_FRAC``,
``REFIT_OBJECTIVE``, ``OUTPUT_GRID_SCALE``, ``SETFL_OUT``, ``REFIT_LOG``),
of one mode (:ref:`Keys of the other mode <refit_mode_keys>`) and the stage
blocks are refused this way, and so is a top-level ``OPTIMIZER`` block,
which no mode reads; ``--retarget`` also refuses ``HPOFILEOUT``
(``--hpofileout``), which ``--edit`` reads (see :ref:`refit_retarget`). The
defaults ``tadah`` stores for the keys the configuration does not set are
not refused. Before 1.6.0 these keys were ignored. ``NUMERIC``, the
precision of the files a command writes, is no key of the refit and is not
refused: ``--edit`` writes the potential with it, and ``--retarget`` writes
only an hpotarget, which it does not govern (below); a task file may set it
above its first ``TASK`` line, for every task.

``EMBF0`` and ``RHOFORM`` are properties of the potential. A configuration
passed with ``-c`` may set them only to the potential's own values: a
different value is refused before anything is written, as in ``tadah
predict`` (``RHOFORM conflict: pot.tadah declares RHOFORM NONNEG but the
task configuration sets RHOFORM MONOTONE. ...``; ``EMBF0 conflict: ...``),
and a ``RHOFORM`` set for a DM_REAM potential without one does not supply
it.

``rho`` edits act in the potential's :ref:`density form <dm_ream_rhoform>`
(the log states it), and the edited potential keeps its ``RHOFORM`` line.
The input must declare ``RHOFORM`` (a DM_REAM potential without it stops
with ``pot edit: DM_REAM: RHOFORM is required: ...``; add ``RHOFORM
SIGNED`` to a fit written before tadah 1.6.0), and under ``NONNEG`` and
``MONOTONE`` its density must pass the checks DM_REAM applies on load (the messages are
prefixed ``pot edit:``). **Adding** inserts a coefficient of 0, which is
valid in every form; under ``MONOTONE`` the new function is a step
:math:`S_k`, which is 1 below its bump, so only the rule on a support
*crossing* ``RCUT`` applies to it. **Removing** under ``NONNEG`` and
``MONOTONE`` solves the same compensation as a bounded least-squares
problem with every adjusted amplitude kept :math:`\ge 0` (exactly: each of
the at most two compensators is tried free and held at 0, and the best
feasible combination wins); a note names a compensator held at 0, a
compensator whose support misses the affected region is left unchanged, and
when none is left the removal is plain (logged). Under ``MONOTONE`` the
removed step contributed its full height everywhere below its bump, so the
affected region runs from the inner sampling bound to the removed step's
outer edge.

Outputs default to ``pot_edited.tadah`` and ``refit_edited.hpotarget`` — the
input file is never overwritten silently (an explicit ``-o`` /
``--hpofileout`` wins, on the command line or in a configuration or task
file, whatever its name: ``-o pot.tadah`` writes ``pot.tadah``; writing onto
the input is refused). In 1.5.0 a name equal to the default of a
refit (``pot.tadah``, ``refit.hpotarget``) counted as unset, and the edited
names were written instead, without a word. ``--retarget``
writes ``<input>_retargeted.hpotarget`` by default (below), so an
``--edit`` and a ``--retarget refit.hpotarget``, two tasks of one task file
or two commands, write different files. An output that
cannot be written (a missing directory, a directory in its place) stops the
command with an error naming the file, here as in every ``refit`` mode and
in ``tadah train``. All bookkeeping
(``TYPE2B`` / ``TYPEMB`` count tokens, ``WEIGHTS``, ``FIXINDEX``/
``FIXWEIGHT``, ``KDEG*``, ``RCUT2B``/``RCUTMB``) is regenerated consistently.

The :ref:`training record <training_record>` of the edited potential is
that of its input, after a line naming the input::

   # --- training record (no key is read from it) ---
   # edited by tadah refit --edit from pot.tadah
   # OUTFILE pot.tadah
   # --- end of training record ---

An edit fits nothing: its weights are the input's, edited, so the record of
the fit that produced them still describes them, and the edit's own ``-o``,
``-v`` and ``NUMERIC`` are not added (an edit of an edit lists both
``edited by`` lines, newest first). A ``DBFILE``, ``UNCERTAINTY``,
``VERBOSE``, ``NUMERIC`` (and so on) key line of an input an earlier
version wrote, or one of the keys that prepared its training data
(``ESHIFT_ATOM``, ``EFILTER``, ``WDBFILE``, ...), is not a key line of the
edit: the edit's record lists it.
What the input's record says is copied as it is, edits of yours included.
An input whose record is malformed is refused before anything is written,
naming the file and the lines: an opening line without its end line (the
refusal names the opening line and the first line after it that is not a
comment, the input's first key line when only the end line was deleted:
restore the end line before it), an end line without its opening line, or
a line inside the block that does not start with ``#``. The potential of a refit of an EAM file records the
refit's ``-o``, ``-v`` and ``NUMERIC`` when they were given (above, the
``OUTFILE`` of ``tadah refit -c config.refit -o pot.tadah``).

.. _refit_retarget:

Rewriting HPO search bounds (``--retarget``)
--------------------------------------------

The hpotarget written by ``refit`` uses ±\ ``OPTIM_PCT`` (default 5 %)
bounds. ``--retarget`` rewrites them without re-running anything::

   # widen every bound to +-10%, coefficients to +-2%
   tadah refit --retarget refit.hpotarget --pct 10 FIXWEIGHT=2

   # absolute bounds for one parameter, edit the file in place
   tadah refit --retarget refit.hpotarget --set 'CGRID2B(3)=2.1,2.4' --in-place

Every ``OPTIM`` line is rewritten — **active and commented alike, preserving
the comment state** — around the parameter values recovered from the existing
bounds; everything else in the file is preserved byte-for-byte. ``--pct``
takes a bare number (the global percentage) and/or ``KEY=N`` per-key
overrides; ``--set 'KEY(i)=lo,hi'`` (repeatable) is applied last. Output goes
to ``<input>_retargeted.hpotarget`` by default (``refit_retargeted.hpotarget``
for ``refit.hpotarget``; the extension and directory are the input's),
``-o <name>`` (any name, ``pot.tadah`` included), or back into the input
with ``--in-place``, which an ``OUTFILE`` set in a configuration or task file
contradicts as ``-o`` does (refused). Before 1.6.0 the
default was ``<input>_edited.hpotarget``: for ``refit.hpotarget`` that is
``refit_edited.hpotarget``, the hpotarget ``--edit`` writes, and of the two
runs the second overwrote the first one's file (the two are searches around
different potentials).

The numbers of ``--pct`` and ``--set`` are read with the number grammar of
the hpotarget file, which ``tadah hpo`` reads the rewritten bounds with
(:ref:`nfp_manual`): an optional sign, decimal digits with at most one
``.``, an optional exponent, a finite value that is zero or a normal double.
``5x``, ``0x10``, ``inf``, ``nan`` and ``1e-320`` are errors naming the
token. A percentage must be positive, and ``--pct KEY=N`` needs an ``OPTIM``
line on ``KEY`` in the file (active or commented; the error lists the keys
it has). A ``--set`` needs finite bounds with ``lo < hi``, and its ``lo``
must satisfy the rule the file's density form sets (below; ``tadah hpo``
refuses such a box at startup). The file takes no ``RHOFORM`` or ``EMBF0``
from a configuration (``-c``): an hpotarget carries no potential, so they
would have no effect, and either is an error (``RHOFORM has no effect on
--retarget ...``); the density form is ``--rhoform``, below. Nor does it
take the keys of a refit of an EAM file (:ref:`as --edit does not
<refit_output_mode_keys>`), or ``HPOFILEOUT``: the rewritten file is ``-o``
or, with ``--in-place``, the input (``tadah refit --retarget: HPOFILEOUT
has no effect under --retarget: only a refit of an EAM file and --edit read
it; remove HPOFILEOUT (the file --retarget writes is -o, OUTFILE)``).
Before 1.6.0 ``--hpofileout`` was ignored, and the file was written to
``<input>_edited.hpotarget``. ``NUMERIC`` is accepted and does not change
the file: its bounds are printed ``%.8e`` (9 significant digits), as in
every hpotarget ``tadah refit`` writes.

Retargeting a freshly-emitted file at the same percentage reproduces it
byte-for-byte, and any retargeted file is a fixed point of further
retargeting at the same settings. Bound recovery is exact at the printed
precision (``%.8e``, 9 significant digits); a hand-pruned group (deleted
lines) shifts the group's scale floor relative to a fresh emission
(warned). A line whose
lower bound was clamped (the width channels, ``RCUT``, the non-negative
amplitudes below) is recovered with the percentage of its own group comment
(``# KEY  (+-X%``, which a per-key ``--pct`` rewrites), so repeating a
per-key retarget such as ``--pct 5 SGRIDMB=50`` is a fixed point too. An
``RCUT2B``/``RCUTMB`` line has no group comment in a fresh file: a per-key
``--pct RCUT2B=50`` writes one above the line (``# RCUT2B  (+-50%, 1
params)``) and a retarget at the global percentage removes it again, so
``RCUT`` lines are fixed points as well.

**Density form.** Under ``RHOFORM NONNEG`` and ``MONOTONE`` the hpotarget
written by ``refit`` (and ``refit --edit``) keeps every ``AMPGRIDMB`` box
non-negative, :math:`lo = \max(0, v - d)` (a zero amplitude gets
:math:`[0, d]`), and records the form in a header line, because the
hpotarget does not carry the potential::

   # RHOFORM NONNEG: AMPGRIDMB bounds are non-negative (lo = max(0, v - d))

``--retarget`` parses that line: it recovers an ``AMPGRIDMB`` line clamped
at ``lo = 0`` from its upper bound without a warning, clamps every
rewritten ``AMPGRIDMB`` lower bound at 0, keeps the line, says so in the log
(``tadah refit --retarget: <file> declares RHOFORM <form>: ...``) and
refuses ``--set 'AMPGRIDMB(i)=lo,hi'`` with ``lo < 0`` (``tadah hpo``
refuses such a box under that form as well). A ``SIGNED`` hpotarget has no
such line and is written exactly as before. A header line that names no
density form is an error at its line.

``--rhoform SIGNED|NONNEG|MONOTONE`` states the form of the potential the
file searches around, for an hpotarget written without the line (by a
``SIGNED`` potential, or before the potential declared a positive form).
Under ``NONNEG`` or ``MONOTONE`` every rewritten ``AMPGRIDMB`` box is
non-negative by the writer's rule and the header line is added where the
writer puts it, so the result is the file ``refit`` writes for a potential
in that form. For example, an amplitude of ``2.5e-5`` in a group whose
bound floor is ``6e-5`` has the ``SIGNED`` box ``[-3.5e-5, 8.5e-5]``::

   tadah refit --retarget refit.hpotarget --rhoform NONNEG -o nonneg.hpotarget

makes it ``[0, 8.5e-5]`` and adds ``# RHOFORM NONNEG: AMPGRIDMB bounds are
non-negative (lo = max(0, v - d))``; later retargets read the form from the
line. ``--rhoform SIGNED`` adds nothing. A file whose header names another
form is refused, naming the line (``--rhoform MONOTONE conflicts with the
file: line 10 declares RHOFORM NONNEG ...``): an hpotarget searches around
one potential, whose form does not change.

.. _refit_rcut_contract:

Cutoffs: knot channels are derived, blip channels are fixed
-----------------------------------------------------------

The two basis families treat ``RCUT2B`` / ``RCUTMB`` differently:

.. list-table::
   :header-rows: 1
   :widths: 18 41 41

   * - Family
     - Effective cutoff
     - RCUT keys
   * - Knot (``Knot5``/``Knot3``)
     - The **largest knot position** — each knot function
       :math:`(t - r)_+^k` vanishes :math:`C^4`-smoothly at its own knot, so
       nothing exists beyond the largest one.
     - **Derived.** Every consumer (``refit``, ``--edit``, ``--retarget``,
       ``train``, ``predict``, ``hpo``) recomputes them from the largest
       knot; any differing stated value is replaced by it (a WARNING when
       the gap exceeds ``1e-9`` relative or the cutoff is smooth, INFO
       otherwise). Moving ``RCUT`` anywhere at or beyond the largest knot is
       prediction-exact only under a hard cutoff (``Cut_Dummy``): a smooth
       ``RCTYPE`` evaluates :math:`f_c(r)` with the ``RCUT`` value for
       every pair. ``pair_style tadah`` in LAMMPS reads the stated ``RCUT``
       and does not re-derive it (see below).
   * - Blip (``Blip``/``Blip5``)
     - The stated ``RCUT`` — blips near the cutoff overhang it and are
       hard-truncated there **by design** (the truncation was present during
       the fit).
     - **Authoritative.** Never auto-changed; warnings only (a centre beyond
       ``RCUT``, or OPTIM bounds that let a blip's support cross it).

Practical consequences for knot potentials:

* To change the effective cutoff, move the ``CGRID2B`` / ``CGRIDMB`` knot
  positions (``OPTIM CGRID2B`` in the hpotarget, or ``--edit --add`` beyond
  the current largest knot). Do **not** search ``OPTIM RCUT2B`` /
  ``RCUTMB`` — the keys are re-derived on every HPO evaluation, so that
  dimension is wasted (warned at startup).
* During ``tadah hpo`` the neighbour list is sized once for the largest
  cutoff the search can reach (including the upper bounds of OPTIM'd knot
  positions), and rebuilt for an evaluation outside the ``OPTIM`` box that
  reaches further (counted in ``DIAGNOSTICS``); the RCUT keys follow the
  moving knots automatically; the written ``best_pot.tadah`` is always
  self-consistent.
* Hand-editing knot positions in a ``pot.tadah`` is safe for ``tadah``: the
  stale RCUT is detected and replaced by whichever command reads the file
  next. ``pair_style tadah`` (LAMMPS) reads the stated ``RCUT`` as it is,
  so under a smooth cutoff LAMMPS and ``tadah train``/``predict``/``hpo``
  differ on such a file (for the ``~3e-13`` gap of knot potentials written
  by ``tadah`` 1.5.0 and earlier, about ``1e-13`` relative) until it is
  re-written: a potential written by ``tadah train``, ``tadah refit
  --edit`` or ``tadah hpo`` (``best_pot.tadah``) carries the derived value.
  The WARNING printed in that case says so.
