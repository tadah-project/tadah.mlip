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

#. Write a ``refit.cfg`` naming the input EAM and the basis (see `Quick start`_).
#. Run ``refit`` on a setfl EAM: ``tadah refit -c refit.cfg -v 2``.
#. Read the crystal self-test at the end of ``refit_opt.log`` (energy–volume
   accuracy of the fitted model vs the original table).
#. Use ``pot.tadah`` directly, or hand ``refit.hpotarget`` to ``tadah hpo``
   for nested fitting.

Quick start
-----------

Minimal config (all other settings take their defaults):

.. code-block:: none

   EAM             Ti1.eam.fs
   REFIT_OBJECTIVE curve
   REFIT_BASIS     Knot5

Run it:

.. code-block:: bash

   tadah refit -c refit.cfg -v 2

A fuller curve-mode config (pinned window edges, explicit basis ceiling):

.. code-block:: none

   EAM             Ti1.eam.fs
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
pass thresholds. ``cohesive delta`` is the equilibrium-energy error.

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
absolute reference energies need ``ESHIFT_ATOM``/``ESHIFT_DBATOM``/
``ESHIFT`` (not ``BIAS true``: its intercept column precedes the weights that
the ``FIXINDEX``/``FIXWEIGHT`` and ``OPTIM FIXWEIGHT`` lines index).

Configuration reference
-----------------------

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
     - Input setfl EAM file to refit (required).
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
     - Output setfl grid density relative to the input table.
   * - ``OPTIM_PCT`` (float)
     - ``5.0``
     - ±% half-width of the OPTIM bounds written into the HPO target.

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
     - ``compact`` fits a small optimised basis via the ladder; ``dense`` fits fixed grids (see :ref:`refit_dense`).
   * - ``REFIT_BASIS``
     - ``Knot5``
     - Basis family (compact): ``Knot5`` quintic knots, ``Knot3`` cubic knots, ``KnotMix`` per-knot cubic/quintic, ``Blip``, ``Blip5``. Knot5 is smoothest per knot; Knot3 captures sharper features. Best family is element-dependent.

Objective and curve weighting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
     - (curve) Weights :math:`\phi,\rho` fully between the equilibrium nearest-neighbour distance and the cutoff, and :math:`F` from zero to the equilibrium density, relaxing toward compression — tighter equilibrium energies for a given basis size.
   * - ``REFIT_CURVE_RELAX``
     - ``0.05``
     - (curve, weighting on) Floor weight in the compressed region, in (0, 1]; smaller focuses the fit harder on the equilibrium region.
   * - ``REFIT_CURVE_DERIV_WEIGHT``
     - ``0.0``
     - (curve) Weight of the 1st/2nd-derivative terms relative to value. ``0`` fits values only (best crystal energies); raise to fit slope/curvature shape.

Compact ladder
~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Key
     - Default
     - Effect
   * - ``REFIT_COMPACT_NMAX``
     - ``40``
     - Per-function basis ceiling. One value (all functions) or three ``phi rho F`` (e.g. ``35 12 35``). Higher is more flexible and slower.
   * - ``REFIT_COMPACT_SAMPLES``
     - ``800``
     - Sample points per function for the fit and RMSE (raised to at least 4×NMAX). More is finer and slower.
   * - ``REFIT_COMPACT_OPT_EVERY``
     - ``1``
     - Run the full per-rung optimisation every K-th rung; intermediate rungs grow by insertion only. Higher is faster and coarser.
   * - ``REFIT_COMPACT_STALL``
     - ``12``
     - Stop a function's ladder after this many rungs without improvement (Blip families: 6).
   * - ``REFIT_COMPACT_INSERT_TOPK``
     - ``3``
     - Try this many top residual peaks per rung and keep the best insertion (Blip families: 1).
   * - ``REFIT_COMPACT_JITTERS``
     - ``2``
     - Randomised restarts per rung (jitters+1 candidates); more explores the landscape harder.
   * - ``REFIT_COMPACT_SHARE``
     - ``0.5``
     - Per-function share of the tolerance budget used by the rung-selection metric.
   * - ``REFIT_COMPACT_SEED``
     - ``12345``
     - RNG seed for the jittered restarts; fixes the run for reproducibility.
   * - ``REFIT_COMPACT_MERGE``
     - ``auto``
     - Knot coalescence guard: penalises near-coincident knot pairs during EXPLORE and merges them after REFINE, re-inserting a fresh knot at the worst residual. ``auto`` is off for ``curve`` and on for ``energy``; ``true``/``false`` force it. Knot families only.

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
     - Pass threshold for the ambient band (:math:`a/a_0\,0.95\!-\!1.10`); also weights the energy-mode metric there.
   * - ``REFIT_TOL_COMPRESSED`` (meV/atom)
     - ``1.0``
     - Pass threshold for the compressed band (:math:`a/a_0\,0.75\!-\!0.95`).
   * - ``REFIT_REACH_SMIN``
     - ``0.75``
     - Smallest :math:`a/a_0` the fit must stay faithful at; sets the reachable :math:`r`/:math:`\rho` window.
   * - ``REFIT_REACH_MARGIN``
     - ``1.25``
     - Safety margin on the reachable :math:`F(\rho)` window.
   * - ``REFIT_R_LO_FRAC``
     - ``0.20``
     - Inner radial fit bound as a fraction of the cutoff; also the lower edge of the :math:`\phi/\rho` weight ramp.
   * - ``REFIT_PLACEMENT``
     - ``window``
     - ``window`` places the basis on the reachable window; ``uniform`` spreads it over the full range.

Stage blocks
~~~~~~~~~~~~

Four optional multi-line blocks tune the pipeline; omit any to use defaults.

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

**EXPLORE / REFINE / FUSE** each accept ``ENABLED true|false`` (REFINE/FUSE;
``ENABLED false`` skips the stage) and a nested ``OPTIMIZER`` block. EXPLORE
also accepts ``MAX_ADD`` (one value or three ``phi rho F``) capping the basis
functions added in that stage.

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

Legal optimisers and the stop-criteria each stage consumes:

.. list-table::
   :header-rows: 1
   :widths: 18 48 34

   * - Stage
     - LIB / ALGO
     - Stop-criteria used
   * - EXPLORE outer
     - NLOPT ``GN_MLSL_LDS`` | ``GN_DIRECT_L`` | ``GN_CRS2_LM``; DLIB ``GLOBAL``; ``UNIFORM``
     - ``MAXEVAL``
   * - EXPLORE ``INNER``
     - NLOPT ``LN_SBPLX`` | ``LN_NELDERMEAD`` | ``LN_BOBYQA`` | ``LN_PRAXIS`` | ``LD_LBFGS``; DLIB ``LBFGS``; ``NONE``
     - ``MAXEVAL``, ``FTOL_REL``, ``XTOL_REL``
   * - REFINE
     - CERES ``LM`` | ``DOGLEG``; NLOPT ``LD_LBFGS``; DLIB ``LBFGS``; ``NONE``
     - ``MAXEVAL`` (alias ``MAXITER``), ``FTOL_REL``, ``GTOL``
   * - FUSE
     - CERES ``LM`` | ``DOGLEG``; ``NONE``
     - ``MAXEVAL`` (alias ``MAXITER``), ``FTOL_REL``

``MAXEVAL`` is the NLopt evaluation budget / Ceres iteration cap; ``FTOL_REL``
on the Ceres stages is Ceres' relative function tolerance.

Output files
------------

* ``pot.tadah`` — fitted native potential.
* ``refit.eam`` — round-trip setfl for LAMMPS.
* ``refit.hpotarget`` — ``tadah hpo`` starter target.
* ``refit_opt.log`` — optimisation log and crystal self-test.
* ``refit_basis_info.dat`` — final basis: function, centre, width, coefficient, degree.
* ``refit_curve_{phi,rho,F}.dat`` — fitted vs tabulated curves and derivatives.
* ``refit_compare.dat``, ``refit_range_report.dat``, ``refit_ladder_trace.dat`` — energy–volume comparison, reachable window, per-rung ladder trace.

.. _refit_dense:

Dense mode
----------

``REFIT_MODE dense`` fits fixed basis grids instead of the ladder; set the
grid sizes directly. Large dense grids reach very high accuracy, but every
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
     - Preset grid density: ``coarse`` | ``balanced`` | ``fine`` | ``accurate``.
   * - ``N2B``
     - —
     - :math:`\phi(r)` basis count.
   * - ``NRHO_BASIS``
     - —
     - :math:`\rho(r)` basis count.
   * - ``NF_BASIS``
     - —
     - :math:`F(\rho)` basis count.
   * - ``REFIT_MAX_ESCALATIONS``
     - ``4``
     - Automatic basis-escalation rounds while above tolerance.
   * - ``REFIT_CONSTRAIN_F0``
     - ``true``
     - Under ``EMBF0 FREE``: pin the raw :math:`F(0)` softly to the table
       value. No effect under the default ``EMBF0 ZERO`` (exact by
       construction).

Dense ``REFIT_BASIS`` families are ``Blip`` | ``Blip5`` | ``Gaussian``.

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

Outputs default to ``pot_edited.tadah`` and ``refit_edited.hpotarget`` — the
input file is never overwritten silently (an explicit ``-o`` /
``--hpofileout`` wins; writing onto the input is refused). All bookkeeping
(``TYPE2B`` / ``TYPEMB`` count tokens, ``WEIGHTS``, ``FIXINDEX``/
``FIXWEIGHT``, ``KDEG*``, ``RCUT2B``/``RCUTMB``) is regenerated consistently.

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
to ``<input>_edited.hpotarget`` by default, ``-o <name>``, or back into the
input with ``--in-place``.

Retargeting a freshly-emitted file at the same percentage reproduces it
byte-for-byte, and any retargeted file is a fixed point of further
retargeting at the same settings. Bound recovery is exact at the printed
precision (8 significant digits); a hand-pruned group (deleted lines) shifts
the group's scale floor relative to a fresh emission (warned).

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
  positions), and the RCUT keys follow the moving knots automatically; the
  written ``best_pot.tadah`` is always self-consistent.
* Hand-editing knot positions in a ``pot.tadah`` is safe for ``tadah``: the
  stale RCUT is detected and replaced by whichever command reads the file
  next. ``pair_style tadah`` (LAMMPS) reads the stated ``RCUT`` as it is,
  so under a smooth cutoff LAMMPS and ``tadah train``/``predict``/``hpo``
  differ on such a file (for the ``~3e-13`` gap of knot potentials written
  by ``tadah`` 1.5.0 and earlier, about ``1e-13`` relative) until it is
  re-written: a potential written by ``tadah train``, ``tadah refit
  --edit`` or ``tadah hpo`` (``best_pot.tadah``) carries the derived value.
  The WARNING printed in that case says so.
