.. _nfp_manual:

Configuration File
==================


This section is the reference for the HPO configuration file.  The
annealing-family key tables live on the :doc:`optimizer` pages
(generated from the validation registry); ``tadah hpo`` prints the same
information per algorithm.

Block hierarchy
---------------

The HPO configuration is a sequence of *root-level* keywords (``LOSS``,
``OPTIM``, ``PC_*``, ``OUTPUT``, …) plus a single ``OPTIMIZER``
``ENDOPTIMIZER`` block.  The optimiser block has nested blocks, listed
in the order they normally appear:

.. code-block:: none

   OPTIMIZER
     INIT       ... ENDINIT          # optional: seeding strategy chain
     NOISE      ... ENDNOISE         # optional: objective-noise calibration
     STRATEGY   ... ENDSTRATEGY      # required: TYPE + driver + decorators
       EXPLORE  ... ENDEXPLORE       # required for TYPE PRESTAGE
       REFINE   ... ENDREFINE        # optional for TYPE PRESTAGE
       INNER    ... ENDINNER         # optional: NLopt MLSL/AUGLAG inner
       RESTART  ... ENDRESTART       # optional decorator
   ENDOPTIMIZER

Block keywords are case-sensitive.  Comments start with ``#``; long
lines may be continued with a trailing ``\``.  An unknown key is an
error naming the line, and so is a malformed number, a missing value or
a stray value after a key (``LIB``, ``MAXEVAL``, ``PARAM`` …).  Numbers
are read strictly: an integer key takes decimal digits only (no sign,
fraction or exponent — ``MAXEVAL 1e5`` is an error, write ``100000``)
within its range, and a real-valued key one whole finite number in the
decimal form LAMMPS reads — an optional sign, digits with at most one
decimal point, and an optional exponent (``5``, ``-0.5``, ``.5``,
``+1e-3``, ``2.5E+07``).  Anything else is an error: ``inf``, ``nan``,
a hexadecimal float such as ``0x1p4``, trailing characters such as
``5x``, a number that overflows a double (``1e400``), and one below the
smallest normal double (``1e-320``, or ``1e-400``, which underflows to
0).  LAMMPS accepts the last kind; HPO does not.  A number in this
grammar but outside its key's range is an error that names it as well.
Every key reads numbers this way, the options of the ``PC_LAMMPS``,
``PC_FAILPROBE`` and ``PC_OBSERVABLE`` lines and the ``tadah refit``
stage blocks included, and ``tadah refit --retarget`` reads its ``--pct``
and ``--set`` numbers, which it writes into an hpotarget, the same way:

.. code-block:: none

   Error on line 6: MAXEVAL must be an integer from 1 to 2147483647, got '1e5'
   Line: MAXEVAL 1e5

   Error on line 9: FAILSCORE must be a positive finite number, got '0x1p40'
   Line: FAILSCORE 0x1p40

Root-level keys are read top to bottom, and **order matters** for three
of them: every ``PC_LAMMPS`` and ``PC_FAILPROBE`` line captures the
``LOSS``, ``FAILSCORE`` and ``PC_SCRIPT_TIMEOUT`` values in force *when
the line is read*.  Put those three keys before the first ``PC_LAMMPS``
line; a ``LOSS`` written after it does not apply to it.

A short form without a ``STRATEGY`` block — ``LIB``/``ALGO``/``MAXEVAL``
written directly inside ``OPTIMIZER`` — is still accepted and behaves
exactly like ``STRATEGY { TYPE PLAIN ... }``.  It is fine for short
single-shot runs (an ``INIT`` block may still precede it) but cannot
express ``MULTISTART``, ``PRESTAGE`` or the ``RESTART`` decorator; those
features require the explicit grammar.

.. _loss_functions:

LOSS — global loss function
---------------------------

::

   LOSS <Name> [<param>]

The default loss applied to every loss term that does not override it
(default ``L2``).  A ``PC_LAMMPS --varloss`` variable may pick a
different loss locally, and a ``PC_OBSERVABLE`` line takes its own
``--loss``.  Every term is the loss of the measured value :math:`y`
against its target :math:`t` (residual :math:`r = y - t`), multiplied by
the term's weight; the loss of an evaluation is the mean of its terms
(:ref:`nested_fitting`), unless one of its ``PC_LAMMPS`` simulations or
``PC_OBSERVABLE`` terms failed: such an evaluation scores ``FAILSCORE``
(see ``--failscore``, :ref:`pc_lammps_options`, and
:ref:`pc_observable`).

================ ================= ==========================================================
Name             Parameter         Term (before the weight)
================ ================= ==========================================================
``L1``           —                 :math:`|r|`
``L2``           —                 :math:`r^2`
``HUBER``        ``δ`` (default    :math:`r^2/2` for :math:`|r| \le δ`, else
                 ``50``)           :math:`δ\,(|r| - δ/2)` — quadratic near zero, linear beyond
``TUKEY``        ``c`` (default    :math:`1 - (1 - (r/c)^2)^3` for :math:`|r| \le c`, else ``1``
                 ``4.685``)        — redescending, zero influence beyond ``c``
``LOG_COSH``     —                 :math:`\log\cosh r` — smooth alternative to L1
``MAPE``         —                 :math:`|r| / |y|` — relative to the *measured* value
``RMSLE``        —                 :math:`(\ln y - \ln t)^2` — log-scaled L2 (positive values)
================ ================= ==========================================================

``HUBER`` and ``TUKEY`` take at most one parameter; when it is omitted
the default above is used.  An explicit parameter must be a finite
number greater than zero.  The other losses take no parameter, and a
value after them is an error.  The ``--varloss`` override uses the same
defaults and validation.

``LOSS`` must precede the ``PC_LAMMPS`` lines it is meant for: each
``PC_LAMMPS`` line captures the ``LOSS`` in force when it is read.

Root-level loss control:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Key (type)
     - Description
   * - ``FAILSCORE`` (positive finite float)
     - Ceiling of the evaluation loss: a total above it, or a non-finite
       total, is clipped to ``FAILSCORE`` (default: the largest double).
       The value must be a positive finite number, else it is an error
       at its line (``FAILSCORE must be a positive finite number, got
       '0'``).  ``FAILSCORE abc`` used to set it to ``0``, so every
       failure scored as the best loss; and before 1.6.0 a value of
       ``0`` or below was accepted and left no failure band: every
       finite loss, ``FAILSCORE`` itself included, counted as usable,
       so a run all of whose evaluations failed ended as a completed
       one with a ``best_pot.tadah`` that held a failed evaluation
       (:ref:`hpo_stopping`).  Negative losses (negative-weight
       ``--varloss`` terms) are usable under any positive
       ``FAILSCORE``: the failures sit above every loss.
       It is also the value of a failed ``PC_OBSERVABLE`` term in
       ``loss.tadah``, the loss of an evaluation in which such a term
       (:ref:`pc_observable`) or a ``PC_LAMMPS`` simulation failed
       (``--failscore``, :ref:`pc_lammps_options`), and the reference
       for graded failures
       (:ref:`failscore_grading`).  ``PC_LAMMPS``
       lines capture the value in force when they are read; a line's
       own ``--failscore`` changes only the value substituted for that
       line's variables when its simulation fails.
       ``DLIB:GFS`` hands dlib a finite stand-in for a failed
       evaluation instead (:ref:`failed evaluations under DLIB:GFS
       <hpo_dlib_gfs_failures>`); the history files and the journal
       keep the loss the evaluation reported (``FAILSCORE``, or a
       graded failure).  At a usable point a failed evaluation in the
       finite-difference stencil fails the gradient, whatever
       ``FAILSCORE`` is, and a failed loss at the point fails it at
       once under every ``GRAD_STRATEGY``, unless it is a graded failure
       under ``FAILSCORE_GRADING ON``, whose finite differences (or
       analytical gradient) are its gradient; such a point can end a
       gradient-based search (:ref:`hpo_gradient_not_finite`).
   * - ``FAILSCORE_GRADING`` (``ON``/``OFF``)
     - Graded failure losses instead of the constant ``FAILSCORE``
       (default ``OFF``); see :ref:`failscore_grading`.
   * - ``BUMPGATE`` (``CANDIDATE``/``STRICT``)
     - Bump support inside a hard cutoff: reject a violating candidate
       before training (``CANDIDATE``, default) or require a feasible
       box at startup (``STRICT``); see :ref:`hpo_feasibility_gates`.
   * - ``RHOGATE <tol> [R_LO <r>] [NPTS <n>] [SEED ERROR|WARN]``
     - The sign of a ``RHOFORM SIGNED`` DM_REAM density: reject a
       candidate whose :math:`\rho(r)` dips below
       :math:`-\mathrm{tol}\,\rho_{max}` (off by default;
       ``tol``, ``R_LO`` finite numbers, ``NPTS`` an integer from 2 to
       10000000); see :ref:`hpo_feasibility_gates`.
   * - ``RHOBARGATE <bcc|fcc|hcp> <a0> <f_lo> <f_hi> [NPTS <n>] [SEED ERROR|WARN]``
     - A DM_REAM lattice density that falls under expansion: reject a
       candidate whose :math:`\bar\rho(a)` is not strictly decreasing
       on :math:`[f_{lo} a_0, f_{hi} a_0]` (off by default; ``a0``,
       ``f_lo``, ``f_hi`` finite numbers, ``NPTS`` an integer from 3 to
       10000000); see :ref:`hpo_feasibility_gates`.

.. _search_space_constraints:

OPTIM — search space constraints
--------------------------------

The ``OPTIM`` directive declares which configuration keys the outer
optimiser may vary, and over what bounds:

.. code-block:: none

   OPTIM <KEY> (indices) <low> <high>

* ``<KEY>`` — any optimisable key from the training configuration.
  A key or value that is not a search target is refused, at its line or
  at startup, naming it (*Special cases* below: e.g. ``RCUT2B`` with
  ``D2_EAM``, whose cutoff is its setfl file's).  Keys are
  case-insensitive, as in the
  configuration: the key is upper-cased when the line is read, so every
  rule below applies whatever its case (``OPTIM weights`` is refused like
  ``OPTIM WEIGHTS``, ``OPTIM fixweight`` maps its indices through
  ``FIXINDEX``, ``OPTIM cgrid2b`` arms ``BUMPGATE``), and the labels of
  ``params.tadah``, the journal and the sidecars are upper-case
  (``OPTIM ampgridmb (1) 0 1`` gives ``AMPGRIDMB[1]``).  Before 1.6.0 a
  key in lower case was searched under its own spelling and missed every
  rule keyed on the upper-case name.
* ``(indices)`` — selects a subset of the values bound to ``<KEY>``.
  Indices follow the order in the training configuration and start
  from **1**.  Comma-separated lists, ranges ``a-b``, and strides
  ``a-b:s`` are all accepted (e.g. ``(1,4,7-10:2)``).  The same index
  is the one every label the run prints carries -- ``KEY[n]`` in
  ``hpo_run.log``, ``params.tadah``, ``hpo_step_scales.tsv`` and the
  search-space sidecars (single-value keys carry no index).
  Empty or malformed selectors, duplicate indices within a selector,
  descending ranges, and zero strides are errors.
* ``<low>``, ``<high>`` — floating-point bounds.  ``high`` must be
  strictly greater than ``low``.
* an optional shape tag ``LIN`` (default), ``LOG`` (requires
  ``low > 0``; the optimiser works in ``ln x``) or ``ASINH <c>``
  (``asinh(x/c)``: linear with slope ``1/c`` near zero, logarithmic
  beyond ``|x| ~ c``; the tool for sign-crossing ranges that span
  several decades).  See :ref:`search_space_scaling`.  Any other trailing
  token is an error (stray tokens used to be silently ignored).

``OPTIM`` lines may be repeated for the same key with different indices,
in any case (``OPTIM cgridmb (1) ...`` and ``OPTIM CGRIDMB (3) ...`` are
lines of one key).  An index an earlier line of the same key already
selects is an error at the second line, naming the first
(``OPTIM CGRIDMB (1): the index is already selected by the OPTIM CGRIDMB
line on line 1``); it used to replace the first box without a word.

.. _hpo_dimension_order:

Dimension order.  The optimiser's parameter vector is ordered by key —
alphabetically, byte-wise, so ``CGRID2B`` comes before ``RCUT2B`` and
``SGRID2B`` — and then by position within the key: by index for every
key but ``FIXWEIGHT``, whose dimensions follow the slot order of
``FIXINDEX`` (with ``FIXINDEX 5 2 9`` and all three weights optimised,
``FIXWEIGHT[5]``, ``FIXWEIGHT[2]``, ``FIXWEIGHT[9]``).  The order of the
``OPTIM`` lines plays no part.  That order defines the columns of
``params.tadah`` / ``best_params.tadah``, the rows of
``hpo_search_space.tsv``, the ``dim`` numbers in ``hpo_run.log``, and
the index ``i`` of ASA's per-dimension ``PARAM <Name>[i]``, which is the
``dim`` column of ``hpo_search_space.tsv`` (:ref:`asa_optimizer`).  For
example ``OPTIM SGRID2B (2) …`` followed by ``OPTIM CGRID2B (2) …`` and
``OPTIM CGRID2B (1) …`` gives the order ``CGRID2B[1]``, ``CGRID2B[2]``,
``SGRID2B[2]``.  The hpotarget written by ``tadah refit`` follows the
same rule: its ``OPTIM`` groups are listed as ``CGRID2B``, ``SGRID2B``,
``CGRIDMB``, …, ``FIXWEIGHT``, and the run orders them by key.  This
order is a documented contract, kept from release to release: reordering
the ``OPTIM`` lines changes neither the columns nor an ASA index, while
adding or removing a key moves the dimensions of the keys that sort
after it.

Seed-relative boxes.  When the right range is unknown, let the CONFIG
seed size the box:

.. code-block:: none

   OPTIM <KEY> (indices) AUTO [RATIO <f> | FRAC <p>] [FLOOR <a>] [GROUP_FLOOR <r>] [LIN | LOG | ASINH <c>]

* ``RATIO f`` — geometric box ``[x0/f, x0*f]`` (mirrored for a negative
  seed); the default for positive-only keys (``SGRID2B``, ``SGRIDMB``,
  ``SEMBFUNC``; ``f = 2``).  A zero seed is an error under ``RATIO``.
* ``FRAC p`` — linear box ``x0 ± max(p·|x0|, FLOOR, GROUP_FLOOR·max|x0|)``
  where ``max|x0|`` runs over the indices of that ``OPTIM`` line; the
  default otherwise (``p = 0.5``).  ``FLOOR``/``GROUP_FLOOR`` keep a
  zero or tiny seed searchable (``FIXWEIGHT`` coefficients that vanish
  at long range).  ``FRAC 0`` with ``FLOOR a`` gives every index the
  same absolute half-width ``a`` -- the natural box for grid centres
  (``CGRID*``, ``CEMBFUNC``), whose unit is the grid spacing rather
  than the value.
* cutoff keys (``RCUT*``) cannot be ``AUTO`` — the neighbour list is
  sized from an explicit high bound.
* under ``RHOFORM NONNEG``/``MONOTONE`` the ``AMPGRIDMB`` boxes are clamped
  at 0 and marked ``AUTO FRAC>=0`` (see :ref:`hpo_rhoform_positive`).
  Under ``MONOTONE`` an amplitude is a step height: a change to an outer
  amplitude shifts the density by the same amount (times :math:`W f_c`)
  all the way in to the core (see :ref:`density form <dm_ream_rhoform>`),
  so size those boxes more tightly than under ``NONNEG``, where an amplitude
  changes only its own bump.

The resolved boxes are written to ``hpo_optim_resolved.hpo`` (a
paste-ready explicit ``OPTIM`` block) and every run writes
``hpo_search_space.tsv`` (one row per dimension: label, real bounds,
origin, seed, scale, optimiser-side bounds and the search-space version;
a comment header explains every column, and the columns are space-padded
to equal width and tab-separated, so the file reads aligned in an editor
and still parses as TSV once whitespace is stripped).  For data-driven
ranges of descriptor shape keys use ``tadah analysis suggest_bounds``.

Bound pressure.  The run never changes ``OPTIM`` bounds by itself; it
tells you when the running best rides one: every milestone ``SUMMARY``
lists the dimensions whose best sits within 2 % of a limit (in internal
units, so a ``LOG`` dimension is judged in ``ln``), with the value, the
bound, the gap as a fraction of the width and the evaluation since which
the mark has persisted, and the ``DIAGNOSTICS`` section repeats it for
the final best.  A best that keeps riding a bound suggests widening that
side (where the physics permits) and rerunning from the best potential
with ``METHOD WARM best_pot.tadah`` in the ``INIT`` block.
The exception is a lower bound the potential itself imposes: an
``AMPGRIDMB`` amplitude at 0 under ``RHOFORM NONNEG``/``MONOTONE`` is an
admissible optimum, marked ``LO*`` and given no such advice (see
:ref:`hpo_rhoform_positive`).

Special cases:

* ``OPTIM FIXWEIGHT`` — the ``(indices)`` name **model weights** (the
  weights ``FIXINDEX`` pins), *not* positions in the ``FIXWEIGHT`` value
  list.  ``OPTIM FIXWEIGHT (5) -1 1`` searches model weight 5 and it is
  an error if 5 is not listed in ``FIXINDEX``.  See :ref:`hpo_fixweight`.
* ``OPTIM WEIGHTS`` — **rejected**.  The linear ``WEIGHTS`` are re-fitted
  by least squares on every evaluation, so optimising them directly
  would be a silent no-op; pin them with ``FIXINDEX``/``FIXWEIGHT``
  instead.
* ``OPTIM EMBF0`` — **rejected** at its line.  ``EMBF0`` (``ZERO``/``FREE``)
  selects the :ref:`DM_REAM embedding reference <dm_ream>`, a discrete
  property of the potential; set it in the ``-c`` potential or
  configuration.  A top-level ``EMBF0`` line in the hpotarget file is an
  unknown key.
* ``OPTIM`` on structure data — ``RHOFORM``, ``TYPE2B``, ``TYPEMB``,
  ``RCTYPE2B``, ``RCTYPEMB``, ``KDEG2B``, ``KDEGMB``, ``KDEGEMB``,
  ``DIMER`` (the rigid-dimer layout of the training data, its bond length
  included), ``FIXINDEX`` (which weights are pinned: search their values
  with ``OPTIM FIXWEIGHT``) and ``MODEL`` (the model, its basis function
  or kernel and its integer option) — is **rejected** at its line: these
  are discrete properties of the potential or of the data, not continuous
  parameters (a searched value was truncated to an integer, broke
  ``FIXINDEX`` or moved the data's bond length).  So is ``OPTIM OALGO
  (1)``, the training algorithm; ``OPTIM OALGO (2)``, the ``rcond`` *M*
  of options 1 and 3, is continuous and stays a search target.
* ``OPTIM`` on a value its key takes only as an integer (every format of
  the key gives it ``INT`` or ``UINT``: ``SBASIS``, the number of basis
  vectors) is **rejected** at its line (``OPTIM SBASIS (1) is not a valid
  search target: value 1 of SBASIS is an integer ...``); a candidate such
  as 14.47 was trained as 14.  So is ``OPTIM`` on an internal key
  (``DSIZE``, ``RCUTMAX``, ``ESTDEV``, ...), a value tadah computes itself
  (``OPTIM ESTDEV`` failed every evaluation).
* ``OPTIM MPARAMS`` is checked against the model's basis function or
  kernel at startup (:ref:`nonlinear_models`): ``OPTIM MPARAMS (i)`` is
  **rejected** on a function that reads fewer than *i* values (``BF_Linear``,
  ``BF_Polynomial2``, ``Kern_Linear``, ``Kern_Quadratic`` and ``Kern_LQ``
  read none; the search changed nothing), on the degree of
  ``Kern_Polynomial`` (``MPARAMS (1)``, an integer) and on the
  :math:`\sigma` of ``Kern_RBF`` with a box that is not positive.
* ``OPTIM RCUT2B`` or ``OPTIM RCUTMB`` on the cutoff of a ``D2_EAM`` or
  ``DM_EAM`` — inside a ``D2_mJoin`` or ``DM_mJoin``, that descriptor's
  own value — is **rejected** at startup: the descriptor takes the cutoff
  of its setfl file (``SETFL``) and refuses another (``OPTIM RCUT2B (1)
  is not a valid search target: D2_EAM takes the cutoff 5.58190742024 of
  its setfl file 'Ta.eam.alloy' (SETFL) and refuses another value ...``),
  so every candidate but the ``-c`` value failed its evaluation.  The
  values of the other descriptors of an mJoin stay search targets.  The
  pre-flight audit and ``tadah analysis suggest_bounds`` do not propose
  this cutoff either.
* A radial bump width — ``SGRID2B`` of a ``D2_Blip``/``D2_Blip5`` pair
  term, ``SGRIDMB`` of a DM_REAM ``Blip``/``Blip5`` density — must be
  :math:`> 0` in every ``RHOFORM``.  A bump spans :math:`[c - w/\eta,
  c + w/\eta]`; a width :math:`\le 0` has no support edge (the bump is even
  in :math:`\eta`, so it evaluates as its mirror) and the bump-support gate
  cannot hold it inside the cutoff.  An ``OPTIM`` box whose low bound on
  such a width, explicit or ``AUTO``, is :math:`\le 0` is an error at its
  line (``OPTIM SGRID2B (2): the low bound 0 is not > 0, but the D2_Blip
  pair basis requires every SGRID2B width > 0 ...``), and so is a width
  :math:`\le 0` in the ``-c`` potential at startup, naming each key, bump
  and value (``SGRID2B (2) = -1: the D2_Blip pair basis requires ...``).
  ``tadah analysis check_bounds`` reports such corners too.

.. _hpo_embf0:

DM_REAM potentials and the embedding reference (EMBF0)
......................................................

Under the default ``EMBF0 ZERO`` the DM_REAM embedding is zero-referenced,
:math:`F(\rho) = \sum_m w_m [g_m(\rho) - g_m(0)]`, so the isolated-atom
energy is 0 by construction for **every candidate** — no penalty term is
needed to hold :math:`F(0)` in place.  :math:`g_m(0)` is recomputed for each
candidate from the ``CEMBFUNC``/``SEMBFUNC`` values it evaluates, so the
constraint also holds when only the embedding shape is searched; the search
space, ``hpo_search_space.tsv``, ``hpo_optim_resolved.hpo`` and
``params.tadah`` are unchanged (no dimension is derived or removed), and
``FIXWEIGHT``/``WEIGHTS``/``FIXINDEX`` keep their meaning.  Every potential
the run writes (``best_pot.tadah``, ``BDUMP``/``DUMP`` archives, the
potential handed to ``PC_LAMMPS``) carries the ``EMBF0`` line.
``EMBF0`` takes ``ZERO`` or ``FREE`` only (in any case); another value is an
error at its line of the configuration or potential file.

``hpo_run.log`` states the reference in the POTENTIAL SETUP section
(``Embedding F(0)``, and the raw :math:`F(0)=\sum_m w_m g_m(0)` of the
``-c`` potential: from its ``FIXWEIGHT`` when every weight is pinned, else
from its stored ``WEIGHTS``, else "determined per evaluation"), and the
RESULT section prints the raw :math:`F(0)` of the first and of the best
evaluation (under ``ZERO``: the per-atom constant the zero reference
removes).  Both values are reported for models linear in the descriptor
(``BF_Linear``, ``Kern_Linear``) only.  A ``WARM`` seed whose ``EMBF0``
differs from the run's triggers a WARNING: ``WARM`` takes only the
``OPTIM`` values from the seed and evaluates them unchanged in the run's
reference (no conversion), so the seed's recorded loss need not be
reproduced.  A HINT is printed when a DM_REAM model under ``ZERO`` is
trained on data without ``BIAS`` or ``ESHIFT`` (see
:ref:`hpo_load_transforms`); prefer ``ESHIFT*``, because ``BIAS true``
inserts intercept columns, one per ``ATOM`` species, before the descriptor
weights (``FIXINDEX``/``FIXWEIGHT`` indices shift) and disables ``EAMFAST``.  Under ``ZERO`` a
combination of embedding coefficients that only shifts :math:`F` by a
constant (available when the bumps nearly sum to a constant) no longer
changes any energy, so the coefficients may drift along it; the raw
:math:`F(0)` of the stored ``WEIGHTS`` can then grow, which matters only to
tools that evaluate the weights without honouring ``EMBF0``.  Example
``27_dm_ream_embf0`` (:doc:`hpo_examples`) searches the embedding knots of
a ``tadah refit`` seed and checks that the isolated atom stays at exactly
0 for every candidate.

.. _hpo_rhoform:

DM_REAM potentials and the density form (RHOFORM)
.................................................

The ``-c`` configuration of a DM_REAM model must declare its
:ref:`density form <dm_ream_rhoform>` (``RHOFORM SIGNED | NONNEG |
MONOTONE``; startup stops without it, and another value is an error at its
line), and every potential the run writes carries it. Under ``SIGNED``
nothing changes for the search. Under ``NONNEG`` and ``MONOTONE`` every
candidate must remain a valid density: the ``AMPGRIDMB`` boxes are
non-negative and every evaluated candidate is projected onto them (see
:ref:`hpo_rhoform_positive`). Whatever the form, two gates can keep the
density physical during the search, ``RHOGATE`` (the sign of a ``SIGNED``
density) and ``RHOBARGATE`` (a lattice density that falls under
expansion), and ``hpo_run.log`` reports the density of the ``-c`` potential
and of the best (see :ref:`hpo_feasibility_gates`). ``WARM`` seeds must
declare the run's form when density keys are searched (see the same
section). Example ``27_dm_ream_embf0`` (:doc:`hpo_examples`) declares
``RHOFORM SIGNED``.

.. _hpo_knot_rcut:

Cutoffs of knot-family refit potentials
---------------------------------------

For potentials written by ``tadah refit`` with the knot basis families
(``D2_Knot5`` two-body, ``DM_REAM`` knot density), the effective cutoff
**is the largest knot position**: each knot function :math:`(t-r)_+^k`
vanishes smoothly at its own knot, so ``RCUT2B``/``RCUTMB`` are *derived*
quantities (see :ref:`refit_rcut_contract`).  ``tadah hpo`` enforces this
automatically:

* at startup, a stated ``RCUT`` that differs from the largest knot is
  replaced by it before anything is built (a WARNING when the gap exceeds
  ``1e-9`` relative or the channel's cutoff is smooth, INFO otherwise --
  see :ref:`refit_rcut_contract` for why LAMMPS can then differ);
* on **every evaluation**, the ``RCUT`` keys are re-derived from the
  current (possibly ``OPTIM``'d) knot positions, so searching
  ``OPTIM CGRID2B`` / ``CGRIDMB`` moves the effective cutoff with the
  knots;
* the neighbour list is sized **once** for the largest cutoff the search
  can reach — including the upper bounds of any ``OPTIM`` block on the
  knot grids. An evaluation beyond it (a point outside the ``OPTIM`` box,
  from an optimiser that ignores the bounds) is evaluated with lists
  rebuilt for it, and ``DIAGNOSTICS`` counts it (``Neighbour lists
  rebuilt``), so no evaluation silently outruns the list.

Consequently ``OPTIM RCUT2B`` / ``OPTIM RCUTMB`` on a knot-family
potential is a *wasted search dimension* (the value is overridden each
evaluation; a startup warning says so).  To search the effective cutoff,
put an ``OPTIM`` block on the knot positions instead — e.g. uncomment the
``OPTIM CGRID2B`` line of the outermost knot in the refit-generated
hpotarget and widen its upper bound.

Blip-family potentials are unaffected: their hard truncation at the
stated ``RCUT`` is part of the fitted model, so the key remains
authoritative; under a hard cutoff ``BUMPGATE`` keeps every candidate's
bumps inside it.

Bump support on hard cutoffs (``BUMPGATE``).  On a hard-cutoff
(``Cut_Dummy``) channel every compact bump must end inside the cutoff,
``c + w/η ≤ RCUT``; otherwise the truncated overhang is an energy and
force step at the cutoff.  The root key ``BUMPGATE`` chooses how this is
enforced over the ``OPTIM`` boxes of the bump centres and widths:
``CANDIDATE`` (default) rejects a violating candidate before any training
work, and ``STRICT`` requires the worst box corner to be feasible at
startup.  It is one of the feasibility gates, with ``RHOGATE`` and
``RHOBARGATE``: see :ref:`hpo_feasibility_gates` for the rules, the
journal lines and an example.

.. _hpo_fixweight:

Pinning and searching linear coefficients
-----------------------------------------

By default the model's linear ``WEIGHTS`` are re-fitted by least squares
inside **every** objective evaluation, while ``OPTIM`` searches the
nonlinear hyperparameters.  ``FIXINDEX``/``FIXWEIGHT`` in the *training
configuration* carve weights out of that LS fit: pinned columns are
removed from the design matrix, their contribution is subtracted from
the target, and the remaining weights are fitted against the residual.
The i-th ``FIXWEIGHT`` value pins the weight named by the i-th
``FIXINDEX`` index; a lone key is an error, and ``NORM true`` is
incompatible with pinning (the values would live in normalized feature
space).  ``FIXINDEX`` lists 1-based model weights, single indices, ranges
``a-b`` and intervals ``a-b:step``, separated by commas or spaces
(``FIXINDEX 4 5`` and ``FIXINDEX 4,5`` are the same list), and names as
many weights as ``FIXWEIGHT`` has values, each weight once: a descending
range (``5-4``), a weight named twice (``4,4,5``, with two ``FIXWEIGHT``
values or three), index 0 and a weight past the model's are refused at
startup, naming the text (``FIXINDEX 4,4,5 names weight 4 twice; ...``), as
``tadah train`` refuses them.  Earlier versions
dropped ``FIXINDEX`` given as separate values and expanded a descending
range to nothing, so the *Mixed mode* example below was refused with
``OPTIM FIXWEIGHT (5): weight 5 is not pinned by FIXINDEX (4 5)``, and
removed a repeated weight with a warning (``4,4,5`` with two ``FIXWEIGHT``
values was accepted).  With ``BIAS true`` and a linear model weights 1 to :math:`N` are
the intercepts of the :math:`N` ``ATOM`` species and the descriptor weights
start at :math:`N + 1` (:ref:`the bias term <bias_term>`, also for the
other models); startup prints a WARNING naming the model's intercept
weights when ``FIXINDEX`` is used with two or more species (before 1.6.0
a linear model's descriptor weights started at index 2).  These rules need the model's weight count, so startup builds the
model of the ``-c`` configuration once, as every evaluation builds it; it
does so for every run.  A configuration whose model does not build (a
``DIMER`` potential whose ``TYPE2B`` names element pairs, say) is refused
with the reason (``the -c configuration does not build a model: DIMER
evaluates every atom as Z=1; ... Every evaluation would fail; fix the -c
potential/configuration.``).  Before 1.6.0 startup printed a WARNING
(with ``FIXINDEX``: "FIXINDEX bounds/coverage checks are deferred to the
first training evaluation") or nothing, every evaluation failed, and the
run ended "no evaluation produced a usable loss".
The weights of ``M_KRR`` with a non-linear kernel belong to its ``SBASIS``
basis vectors (weights 1 to :math:`n` to the intercept vectors
:math:`e_0 \dots e_{n-1}`, one per ``ATOM`` species with ``BIAS true`` and
:math:`e_0` alone otherwise, the others to descriptors drawn from the
training data, see :ref:`Non-linear models <nonlinear_models>`): startup
checks ``FIXINDEX`` against ``SBASIS``, refuses an ``SBASIS`` below
:math:`n + 1` (every fit would refuse it), requires a training ``DBFILE``,
and every evaluation trains, also when ``FIXINDEX`` pins every weight. The
basis is drawn anew in every fit: the ``BASIS`` and ``WEIGHTS`` of a ``-c``
potential are not read.

**Mixed mode** — LS-fit most weights, hold one, search another.  With a
5-weight model: weights 1–3 LS-fitted, weight 4 held at 0.75, weight 5
searched by the optimiser:

.. code-block:: none

   # training configuration (config.train)
   DBFILE    train.tadahdb
   FIXINDEX  4 5
   FIXWEIGHT 0.75 1.20        # 0.75 pins w4; 1.20 seeds w5

   # config.hpo
   OPTIM FIXWEIGHT (5) -2.0 2.0    # (5) = model weight 5

**All-fixed polish** — the ``tadah refit`` workload.  ``FIXINDEX`` covers
every weight, so the LS solve is vacuous and skipped (including the
per-evaluation design-matrix build); the optimiser searches the
coefficients directly:

.. code-block:: none

   FIXINDEX  1-28
   FIXWEIGHT <28 fitted values>
   # config.hpo: one OPTIM FIXWEIGHT (i) line per coefficient
   # (this is exactly what `tadah refit` emits as refit.hpotarget)

**No training data** — with every weight pinned, the training ``DBFILE``
may be omitted entirely.  Declare the element set with ``ATOM`` and keep
at least one loss source (``--validation`` metrics, ``PC_OBSERVABLE``,
or ``PC_LAMMPS``).  With ``PC_OBSERVABLE`` and a gradient-based backend,
set ``GRAD_STRATEGY HYBRID`` or use a derivative-free backend: an unset
``GRAD_STRATEGY`` resolves to ``ANALYTICAL`` (:ref:`pc_observable`),
which needs training data, and startup refuses it.

.. code-block:: none

   # config.train: a fitted potential, no DBFILE line
   ATOM      Ta
   FIXINDEX  1-28
   FIXWEIGHT <28 values>

   # config.hpo
   PC_LAMMPS --script Ecoh.in --hotvar hot_a0 2.5 4.5 3.302 --varloss Ecoh 8.10 800
   OPTIM FIXWEIGHT (1-28) ...

   # command line: --validation may be omitted
   tadah hpo -c config.train --hpotarget config.hpo

Misconfigurations (lone keys, size mismatches, out-of-range indices,
missing loss sources, incomplete pin coverage without a ``DBFILE``) are
rejected at startup with actionable messages.

With ``FIXINDEX`` present there is no analytical gradient: the
analytical chain assumes the unconstrained LS solution map, which
pinning invalidates.  ``GRAD_STRATEGY ANALYTICAL`` — written out, or
resolved from an unset ``GRAD_STRATEGY`` with ``PC_OBSERVABLE`` — is
refused at startup (``… cannot form: FIXINDEX pins model weights …``),
and ``HYBRID`` uses finite differences for every parameter.  Before
1.6.0 ``ANALYTICAL`` passed startup and stopped the run at its first
gradient.  Derivative-free algorithms (e.g. ``NLOPT LN_BOBYQA``) are a
natural choice for coefficient searches.

.. _performance_constraints:

PC_* — performance constraints
------------------------------

Energy / force / stress validation metrics
..........................................

Validation-set metrics are evaluated on every ``DBFILE`` listed in the
``--validation`` file.  The global form takes one target and weight and
applies the loss to the arithmetic average of that metric across datasets:

.. code-block:: none

   PC_<METRIC> <target> <weight>

================== ======================================================= ==============
Key                Quantity                                                Unit
================== ======================================================= ==============
``PC_EMAE``        Energy mean absolute error (per atom)                   meV/atom
``PC_ERMSE``       Energy root-mean-square error (per atom)                meV/atom
``PC_ErRMSE``      Energy *relative* RMSE                                  ‰ (× 1000)
``PC_ERSQ``        Coefficient of determination (R²) for energies          —
``PC_FRMSE``       Force component RMSE                                    eV/Å
``PC_SRMSE``       Stress component RMSE                                   eV/Å³
================== ======================================================= ==============

These constraints use the global ``LOSS`` selected above.

**Structures without forces or a stress tensor.** A validation structure
stored with force weight 0 (``fweight``, on the dataset's weights line) is
left out of ``PC_FRMSE``, and one with stress weight 0 (``sweight``) out of
``PC_SRMSE``: neither its components nor their count enter the metric (nor,
for ``PC_FRMSE``, its analytical gradient).  ``tadah data convert`` stores a
structure without a stress tensor that way, and a CASTEP ``.castep``
structure without forces, with zeros in their place, so there is nothing to
compare; a weight of 0 set by hand has the same effect.
A validation dataset in which every structure has that weight 0 is *not
measured*: its value and its loss term are ``nan`` in ``stats.tadah``,
``loss.tadah`` and the ``best_pot.tadah`` result table, it is left out of
the average (``FRMSE[avg]``, ``SRMSE[avg]``) and of its gradient, and one
line of the journal names it at startup.  For
example, with ``DBFILE md.tadahdb`` (forces) and ``DBFILE relax.tadahdb``
(a relaxation converted without forces), ``PC_FRMSE 0 1`` gives ``FRMSE[2]``
``nan`` and ``FRMSE[avg]`` equal to ``FRMSE[1]``, and so does ``PC_FRMSE (1)
0 1`` with ``PC_FRMSE (2) 0 0``.  The run is refused at startup, naming the
file, when no validation dataset has the data, or when an indexed
``PC_FRMSE`` gives such a dataset a weight above 0.  ``tadah predict -A``
leaves the same structures and datasets out of its force and stress metrics
(``nan``, left out of the mean column, with a note) and refuses a run in
which no dataset has the data.  Before 1.6.0 the metrics compared the
prediction with those zeros.

.. _hpo_validation_follows_metrics:

**What validation predicts.** ``FORCE`` and ``STRESS`` (in the ``-c``
configuration file, or ``-F``/``-S`` on the command line, which take
precedence) decide only what *training* fits, exactly as in
``tadah train``.  Validation predicts what its metrics measure: forces
whenever ``PC_FRMSE`` is set, stresses whenever ``PC_SRMSE`` is set,
whatever the potential was fitted with.  An energy-only fit
(``FORCE false``, or no ``FORCE`` key) with ``PC_FRMSE`` therefore searches
for hyperparameters whose energy-only fit also gives good forces, and
``best_pot.tadah`` keeps ``FORCE false``, so ``tadah train`` with the best
hyperparameters reproduces it.  The journal's POTENTIAL SETUP states both
sides, for example::

   Fit forces (FORCE)    : no  (configuration file config.tadah)
   Fit stresses (STRESS) : no  (default)
   Validation predicts   : energies, forces (PC_FRMSE)

and, with ``-F`` on the command line,
``yes  (command line -F/--force; overrides FORCE false in config.tadah)``.
``FORCE`` and ``STRESS`` are refused in the ``--validation`` file, at their
line: it has no fit, so they would have no effect there.  When no metric
asks for them, validation predicts neither forces nor stresses, whatever
training fitted.  Before 1.6.0 ``PC_FRMSE`` with ``FORCE false`` (or no
``FORCE`` key) stopped the run with a segmentation fault at the first
evaluation, ``MODEL M_KRR`` did so even with ``FORCE true``, and
``PC_SRMSE`` compared the reference stresses with zero when ``STRESS`` was
false, and with ``MODEL M_KRR`` also when it was true; an ``M_KRR``
``best_pot.tadah`` did not record ``FORCE`` and ``STRESS``.

``ErRMSE`` is a relative error times 1000, so the journal and the
``best_pot.tadah`` headers give it in *per mille*; ``SRMSE`` is in the
dataset's stress unit, eV/Å³, written ``eV/A^3``.  Before 1.6.0 they were
labelled ``meV/atom`` and ``GPa``, with the same values.

**Two runs in one process.** A ``tadah --task`` file with two ``hpo``
tasks, or a program that calls ``hpo_run`` twice, runs two model
searches. Each evaluation builds its model from its own run's
configuration and candidate (the second run used to train the first run's
model, with the first run's ``FORCE``, ``STRESS``, ``LAMBDA``, row weights
and every other key it does not search), and each run starts its
``PC_LAMMPS --hotvar`` values from their ``<initial>`` values or its
``WARM`` seeds (the second run used to start each hot script from the
value the first run's last evaluation converged to). Run twice from one
task file, example ``01_minimal`` (at ``MAXEVAL 3``) writes for its
second run the ``loss.tadah``, ``outvar.tadah`` and ``best_pot.tadah`` of
the same run made alone, but for the evaluation numbers. Each run builds
its own pool of ``THREADS`` worker threads, which runs the ``PC_LAMMPS``
scripts and the training kernels, and under ``PC_ISOLATION PROCESS`` its
own worker processes, and stops them when it ends, also when it fails;
the ``PC_ISOLATION THREAD`` LAMMPS instances end with the threads, so
the next run creates its own. Every run used to run on the pool of the
first run's ``THREADS`` while its journal reported its own value (under
``PC_ISOLATION PROCESS`` a later run with a smaller ``THREADS`` failed
its evaluations: ``PcWorkerPool::execute: bad slot``; under
``PC_ISOLATION THREAD`` a script of the same file name and LAMMPS
options ran on the first run's LAMMPS instance, as the first run had
read it, so a script rewritten between the runs was not read again),
and a program that called ``hpo_run`` again after a run that threw
found the first run's worker processes still running
(``pc_worker_pool_init: pool already initialized``). One thing carries over from the first run, the
evaluation counter: the second run's ``ITERATION`` numbers and ``DUMP``
file names continue where the first run's stopped (4 to 6 after a run of
3 evaluations). Run each ``tadah hpo`` in a process of its own when its
files must number the evaluations from 1.

Individual validation targets and weights
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``PC_ERMSE`` and ``PC_FRMSE`` also accept indexed declarations:

.. code-block:: none

   PC_ERMSE (indices) <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]
   PC_FRMSE (indices) <target> <weight> [--skip-above <cap>] [--skip-rel <ratio>]

Indices start from **1** and refer to ``DBFILE`` order in the validation
configuration, not the training configuration. Selectors use the same syntax
as ``OPTIM``: lists ``(1,2,3)``, inclusive ranges ``(1-3)``, strides
``(1-5:2)``, and combinations with whitespace ``(1, 3-7:2)``.
Malformed selectors, zero indices or strides, descending ranges, duplicate
indices, and indices beyond the validation list are errors.

For example, with three validation datasets:

.. code-block:: none

   # config.val
   DBFILE bulk.db
   DBFILE surfaces.db
   DBFILE liquid.db

   # config.hpo
   LOSS L2
   PC_ERMSE (1,2) 5.0  1.0
   PC_ERMSE (3)   10.0 0.5
   PC_FRMSE (1)   0.05 2.0
   PC_FRMSE (2-3) 0.10 1.0

ERMSE targets are in **meV/atom**; FRMSE targets are in **eV/Å**. Targets
must be finite, and weights must be finite and nonnegative. An explicit
zero weight satisfies coverage and contributes zero loss; its dataset still
counts in the average. Weights are not normalized by their sum.

For each enabled metric, choose either one global line or disjoint indexed
lines covering **every** validation dataset. Mixing forms for the same
metric, repeated global lines, overlapping selections, and missing targets
or weights are errors. ERMSE and FRMSE may choose different forms, and either
metric may be omitted entirely. Empty validation datasets, including those
emptied by validation filters, are rejected without renumbering later files.
Indexed declarations are not supported for the other validation metrics.

For measured dataset metrics :math:`x_i` and :math:`N` validation datasets,
the metric contribution is:

.. math::

   C_{\mathrm{global}} = \operatorname{LOSS}\!\left(
      \frac{1}{N}\sum_i x_i,\ t,\ w\right),
   \qquad
   C_{\mathrm{indexed}} = \frac{1}{N}\sum_i
      \operatorname{LOSS}(x_i,\ t_i,\ w_i).

Each metric contributes **one term** to the existing total-loss average,
alongside the other enabled constraints. Dataset sizes do not change this
averaging. Even identical indexed targets and weights can differ from the
global form for nonlinear losses such as L2: averaging losses differs from
taking the loss of an average. The forms agree for a single dataset.

The run journal and potential header identify the form and the resolved
indexed assignments. In ``loss.tadah`` and ``best_loss.tadah``, indexed
``L(ERMSE)[avg]`` / ``L(FRMSE)[avg]`` columns are the mean of the corresponding
dataset losses. For the global form they retain the loss of the average
metric. Raw metric averages in ``stats.tadah`` and ``outvar.tadah`` are
unchanged.

Physics-informed constraints (PC_LAMMPS)
........................................

``PC_LAMMPS`` runs a regular LAMMPS script against each trial potential
and feeds one or more LAMMPS *equal-style* variables back into the
loss:

.. code-block:: none

   PC_LAMMPS --script in.mysim \
             --varloss myVar       0   100 \
             --varloss myOtherVar 145    10

A long ``PC_LAMMPS`` line may be split with a trailing back-slash
(``\``).  Tadah!MLIP creates one ``LAMMPS_NS::LAMMPS`` instance per
script per worker slot — inside a persistent worker process under the
default ``PC_ISOLATION PROCESS`` — and reuses it across iterations (the
script's own ``clear`` directive resets simulation state between runs,
so existing scripts work unmodified).  Each ``--varloss`` variable is one
term of the loss (:ref:`nested_fitting`).

``PC_LAMMPS`` and ``PC_OBSERVABLE`` are mutually exclusive: an HPO
configuration may use one or the other, not both.  ``PC_LAMMPS`` also
rejects every gradient-based optimiser except ``DLIB:BFGS``
(:ref:`strategy_block`), and any ``GRAD_STRATEGY`` line, in every block
that configures an optimiser: the ``STRATEGY`` level (or ``REFINE``),
``EXPLORE``, and their ``INNER`` blocks.

Each line captures the root ``LOSS``, ``FAILSCORE`` and
``PC_SCRIPT_TIMEOUT`` in force when it is read, so declare those keys
first.

Required options
~~~~~~~~~~~~~~~~

``--script <file>``
   LAMMPS input file containing the variable definitions.

``--varloss <name> <target> <weight> [<loss> [<param>]]``
   The variable ``<name>`` (defined inside the LAMMPS script) is read
   at the end of the run and combined with ``<target>`` and
   ``<weight>`` via the root ``LOSS``.  An optional loss name (and, for
   ``HUBER``/``TUKEY``, an optional parameter) overrides the loss for
   this variable only, with the same defaults and validation as the
   root ``LOSS`` (:ref:`loss_functions`).  ``<target>``, ``<weight>``
   and ``<param>`` must be finite numbers; a negative target or weight
   is accepted.  A malformed value is an error naming the line:

   .. code-block:: none

      Error on line 9: --varloss Ecoh <target> must be a finite number, got '8.1x'
      Line: PC_LAMMPS --script Ecoh.in --varloss Ecoh 8.1x 800

.. _pc_lammps_options:

Additional options
~~~~~~~~~~~~~~~~~~

``--invar <name> <value>``
   Inject a variable (equivalent to LAMMPS' ``-var`` flag), letting the
   same script be reused for several structures or pressures.

``--outvar <name>``
   Record the variable in ``outvar.tadah`` without contributing to the
   loss.  Useful for diagnostic plots.  A diagnostic that cannot be read
   back after the script, or has no number for its value
   (:ref:`pc_lammps_readback`), is ``NaN`` in its column, and the
   evaluation keeps its loss.  (The ``S`` of the
   ``PC_FAILPROBE`` line is not a diagnostic: one that cannot be read
   fails the probe.)

``--failscore <value>``
   The value substituted for each of this script's ``--varloss`` /
   ``--outvar`` variables when the simulation fails (crash, timeout,
   out-of-window hot value, a hot value LAMMPS refuses to inject, a
   ``--varloss``, ``--hotvar`` or ``--failvar`` variable that cannot be
   read back or has no number for its value, …): their value in
   ``outvar.tadah``, which passes through the loss against the target
   into their terms in ``loss.tadah`` like a measured one.  A finite
   number (it may be negative or ``0``: it is a variable's value, not a
   failure score).  Default: the root ``FAILSCORE`` in force
   when the line is read.  The evaluation is a failed one whatever that
   value and the line's loss: its loss is the root ``FAILSCORE`` (under
   ``FAILSCORE_GRADING ON`` the graded failure loss,
   :ref:`failscore_grading`).  Before 1.6.0 it was the mean of its
   terms, which reached ``0.999 × FAILSCORE`` only when the line's loss
   carried the substituted value past ``FAILSCORE``, as ``L2`` does with
   the default value: under ``L1`` or the bounded ``TUKEY``, or with a
   small ``--failscore``, a failed evaluation counted as feasible and
   could become the best.
   On the ``PC_FAILPROBE`` line it is a failure score: the probe's
   ``S`` reads it when the probe itself fails, and an ``S`` at or above
   it counts as a failed probe.  There it must be a positive finite
   number, as ``FAILSCORE`` (``PC_FAILPROBE --failscore must be a
   positive finite number, got '0'``); before 1.6.0 a value of ``0`` or
   below was accepted, and every probe read as failed.

``--maxtime <time>|off``
   Per-script override of the root ``PC_SCRIPT_TIMEOUT``, with the same
   values: ``off``, ``unlimited`` or a positive time (see
   :ref:`pc_lammps_execution`).

``--ncpu`` (removed)
   Gave the number of MPI ranks for this script in the legacy MPI build,
   removed in 1.6.0.  A line that contains ``--ncpu`` is rejected at
   startup, with or without a value:

   .. code-block:: none

      Error on line 12: --ncpu was removed in 1.6.0 (legacy MPI build); parallelism over PC_LAMMPS scripts is THREADS / PC_ISOLATION. Delete --ncpu from this line.
      Line: PC_LAMMPS --script in.elastic --varloss C11 250 1 --ncpu 8

   Scripts run in parallel over the ``THREADS`` worker slots, each slot's
   LAMMPS instances in their own worker process under the default
   ``PC_ISOLATION PROCESS`` (:ref:`pc_lammps_execution`,
   :ref:`performance_parallelism`).  ``PC_LAMMPS`` lines keep their
   configuration order everywhere: in the ``outvar.tadah`` and
   ``loss.tadah`` columns, in the journal and in the potential header.

A token that starts with a single ``-`` is a LAMMPS option, passed to
LAMMPS as a command-line argument together with the values that follow
it: for example ``-log run.log``, ``-screen none`` or ``-var x 1 2`` (see
:ref:`pc_lammps_result_contract` for how HPO renames ``-log`` /
``-screen`` files per worker); LAMMPS judges these itself, when the line
is read: HPO starts a LAMMPS instance with the line's arguments to check
the script's variables, and one LAMMPS refuses — a typo, or an option
this LAMMPS build cannot honour, such as ``-sf omp`` without the
``OPENMP`` package — is an error at its line, with LAMMPS' message:

.. code-block:: none

   Error on line 9: PC_LAMMPS: LAMMPS cannot start with this line's command-line options: ERROR: Invalid command-line argument: -sff (src/lammps.cpp:490)
   Line: PC_LAMMPS --script Ecoh.in --varloss Ecoh 8.10 800 -sff omp

Before 1.6.0 the line was accepted and every evaluation failed to create
its LAMMPS instance, with nothing in the journal.  Every LAMMPS
command-line option starts with a single ``-``, so an option that starts
with ``--`` and is none of the HPO options of this section is an error at
its line, naming HPO's options and, for a near miss, the closest one:

.. code-block:: none

   Error on line 9: PC_LAMMPS: unknown option '--hotvars' (did you mean '--hotvar'?): HPO's options are --script, --varloss, --invar, --outvar, --failscore, --maxtime, --hotvar and --failvar; LAMMPS command-line options start with a single '-'
   Line: PC_LAMMPS --script Ecoh.in --hotvars hot_a0 2.5 4.5 3.302 --varloss Ecoh 8.10 800

On a ``PC_FAILPROBE`` line the list and the closest option are those
that line takes: ``--script``, ``--invar``, ``--outvar``,
``--failscore``, ``--maxtime``, ``--gate`` and ``--s0``
(:ref:`failscore_grading`), without the ``--varloss``, ``--hotvar`` and
``--failvar`` it refuses; on a ``PC_LAMMPS`` line the message names
``--gate`` and ``--s0`` as ``PC_FAILPROBE`` options.  Before 1.6.0 such an option was passed to
LAMMPS with its values: every evaluation failed (``Invalid command-line
argument: --hotvars``), and the run spent its whole budget before it
ended with no usable loss.

An HPO option takes only its own values: a value after them is an error
naming the option, and so is a value before any option.  Before 1.6.0 it
was passed to LAMMPS, and every evaluation failed on it:

.. code-block:: none

   Error on line 9: --maxtime takes a single value (unexpected '00')
   Line: PC_LAMMPS --script Ecoh.in --varloss Ecoh 8.10 800 --maxtime 1:30 00

The LAMMPS variable ``maxtime`` is reserved: HPO sets it from
``PC_SCRIPT_TIMEOUT`` or ``--maxtime`` for the injected ``timer timeout
${maxtime}`` (:ref:`pc_lammps_execution`), and ``-var maxtime``, ``-v
maxtime`` or ``--invar maxtime`` on the line is an error.  Before 1.6.0
such a value replaced the checked, reported limit.

``--hotvar <name> <min> <max> [<initial>]``
   Declare this ``PC_LAMMPS`` line as *hot*: a two-phase evaluator
   for parameters that other scripts depend on. Hot scripts run in
   **phase A** of each iteration (in parallel with other hot scripts
   and with the cold scripts that use no hot value), redefine ``variable <name> equal …`` at the end of
   the LAMMPS script (see :ref:`warmstart_protocol` for the
   ``equal``-style readback), and the returned value is validated
   against ``[min, max]``. Validated values are cached per
   ``(script, name)`` and propagated as ``${<name>}`` into
   the cold (no-``--hotvar``) ``PC_LAMMPS`` scripts that use it, which
   run in **phase B**.  HPO injects the value as ``variable <name> equal
   <value>`` before each cold script that reads it (``${<name>}`` or
   ``v_<name>`` in its text, ``include`` files included; a cold script
   gets no hot value it does not read), and the warm-start seed before
   the hot script.  LAMMPS refuses that when the name is already
   another style in the script's LAMMPS instance — ``--invar <name>`` or
   ``-var <name>`` on the line makes it an index-style variable — and
   then the script does not run: it fails as a failed run does, journalled
   with the LAMMPS message:

   .. code-block:: none

      PC_LAMMPS task failed (cold.in, occurrence 1): injecting `variable hot_a0 equal 3.25` before the script: ERROR: Cannot redefine variable hot_a0 with a different style (src/variable.cpp:478)

   Before 1.6.0 the refusal was ignored and the script ran on the value
   it had.  A cold script that reads a hot value its hot script did not
   produce — that script's LAMMPS instance could not be created, a
   failure that does not abort the evaluation (:ref:`pc_lammps_readback`)
   — does not run either, and its variables take its ``--failscore``; it
   used to run without the value, or in a cached instance on an earlier
   evaluation's.
   ``<initial>`` is optional; when omitted the midpoint of
   ``[<min>, <max>]`` seeds the first iteration.  ``<min>``, ``<max>``
   and ``<initial>`` must be finite numbers: a token after ``<max>``
   that is not an option is ``<initial>``, and a malformed one is an
   error (it used to be passed on to LAMMPS). If a hot value
   lands outside ``[min, max]`` HPO fail-scores the iteration and
   skips phase B (a *graded* failure when ``FAILSCORE_GRADING ON``
   — see :ref:`failscore_grading`); the journal WARNING ``eval N script
   <script>: rejecting (hot_a0=4.500000000000001 outside [2.5, 4.5],
   total rejects=K)`` prints the value and the window in the shortest
   form that reads back exactly (six decimals before 1.6.0, which printed
   a value just above ``<max>`` as ``<max>`` itself).

``--failvar <name> [<scale>]``
   Declare a *violation variable*: a nonnegative LAMMPS ``equal``-style
   variable that is ``0`` when the simulation vouches for itself and
   ``> 0`` when it failed, with the magnitude measuring *how badly*
   (natural units; ``<scale>`` converts to the dimensionless failure
   measure, default 1; a finite number ``> 0``, and like ``--hotvar``'s
   ``<initial>`` a token that is not an option is it). With
   ``FAILSCORE_GRADING ON``, when it reads ``> 0`` the sim is treated as
   failed, its outvars are excluded from the fit, and ``scale × value``
   grades the failure loss so the optimiser is pulled toward feasibility
   instead of hitting a constant; with ``OFF`` its value is not used.  In
   both, a failvar that cannot be read back, or whose value is not a
   finite number, fails the simulation (:ref:`pc_lammps_readback`).
   Define it **early** in the script (right after
   the risky command), so it is still readable when a later section
   times out. Like ``--varloss``/``--outvar`` names, the variable must
   be defined in the script as an ``equal``-style variable — a typo'd
   name is a startup error, not a silently-disabled check. Replaces
   the old “sentinel value” idiom (returning ``-1000`` as fake data).
   See :ref:`failscore_grading`.

Per-metric bad-pot pre-filter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``PC_<METRIC> --skip-above <cap>`` and ``PC_<METRIC> --skip-rel <ratio>``
attach a per-metric pre-filter on the dataset error counters
(``PC_EMAE``, ``PC_ERMSE``, ``PC_ErRMSE``, ``PC_ERSQ``, ``PC_FRMSE``,
``PC_SRMSE``).  Both gates fire **before** any ``PC_LAMMPS`` work for
the iteration:

* ``--skip-above <cap>`` fail-scores the iteration when the metric
  exceeds the absolute ``<cap>``.
* ``--skip-rel <ratio>`` fail-scores when
  ``metric > ratio × running_best`` for that same metric.

Both filters can be applied to the same metric line.  They short-cut
obviously bad potentials so wall-time is not spent on LAMMPS phases
that would never compete.

On indexed ``PC_ERMSE`` / ``PC_FRMSE`` lines, the flags apply independently
to each selected dataset; any triggered dataset skips downstream observable
and LAMMPS work. ``--skip-rel`` compares against that dataset's own best
observed value for the metric. A zero loss weight does not disable a filter.
On global lines the filters continue to act on the average metric. Histories
are shared between evaluation threads within a run and start empty for every
run (HPO runs are not resumable; see :ref:`hpo_potential_archives`).
Diagnostics name the triggering metric, dataset index, and filename.

.. _pc_lammps_result_contract:

Result contract for user scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LAMMPS scripts driven by HPO must communicate their results back
*only* through ``--varloss`` / ``--outvar`` (LAMMPS ``equal``-style
variables).  They should avoid writing files (dumps, restarts,
``write_data``, custom logs) unless filenames are made unique, because
all shared-pool workers (and isolated worker processes) use the same
working directory.

If ``-log <file>`` or ``-screen <file>`` is supplied explicitly inside
a ``PC_LAMMPS`` entry, HPO appends ``.t<worker_slot>`` per shared-pool
slot (and the MPI rank suffix when applicable), so for example
``-log run.log`` becomes ``run.log.t0``, ``run.log.t1``, … on one rank.
``-log none`` and ``-screen none`` are left untouched.

.. _pc_lammps_readback:

Reading the variables back
~~~~~~~~~~~~~~~~~~~~~~~~~~

HPO reads every ``--varloss``, ``--outvar``, ``--hotvar`` and
``--failvar`` variable after the script has run, by evaluating its
formula in LAMMPS (:ref:`hot_value_precision`).  That evaluation can raise
a LAMMPS error the script itself never met, because the script did not use
the variable: a thermo keyword the script's ``thermo_style`` does not
compute (LAMMPS computes a thermo keyword only when thermo output uses it),
a divide by zero for one candidate, a compute that is not current.  Each
variable is read on its own, and the error is handled by the variable's
role, the same way under ``PC_ISOLATION THREAD`` and ``PROCESS``:

* a ``--varloss``, ``--hotvar`` or ``--failvar`` variable that cannot be
  read fails its simulation, as a failed run does: the line's variables
  take its ``--failscore``, the evaluation scores ``FAILSCORE`` (under
  ``FAILSCORE_GRADING`` a graded loss, the failvars still grading the
  failure: :ref:`failscore_grading`), and a hot script's cold scripts do
  not run.  The journal says, with the variable and the LAMMPS message:

  .. code-block:: none

     PC_LAMMPS task failed (press.in, occurrence 1): reading --varloss E after the script: ERROR: Thermo keyword press in variable requires thermo to use/init press (src/thermo.cpp:1331)

  So does the ``S`` of the ``PC_FAILPROBE`` line (its first ``--outvar``),
  which grades every failure of the evaluation: an ``S`` that cannot be
  read is a failed probe, as when the probe's script fails, so the
  evaluation is aborted (``FAILSCORE_GRADING OFF`` or ``ON``) and, under
  ``FAILSCORE_GRADING ON``, graded band D with ``S`` unavailable
  (``PC_LAMMPS task failed (probe.in, occurrence 1): reading PC_FAILPROBE
  --outvar S after the script: …``).

* a variable declared only with ``--outvar`` (a diagnostic) that cannot
  be read is ``NaN`` in ``outvar.tadah``, and the evaluation keeps its
  loss:

  .. code-block:: none

     PC_LAMMPS --outvar is NaN (press.in, occurrence 1): reading --outvar Pend after the script: ERROR: Thermo keyword press in variable requires thermo to use/init press (src/thermo.cpp:1331)

A variable can also be read and have no number for its value: it is not
defined after the script (the script deleted it, or defined it in a branch
it did not take), has no single value, or is text (redefined as a
string).  That is handled the same way, by role: a ``--varloss``,
``--hotvar`` or ``--failvar`` variable or the ``PC_FAILPROBE`` ``S`` fails
its simulation, and a diagnostic is ``NaN`` with its journal line.  A
``--failvar`` must also be finite: one that is ``inf`` or ``nan`` fails the
simulation (``exp(1000)`` is ``inf`` in LAMMPS), whatever
``FAILSCORE_GRADING``.  The message says which:

.. code-block:: none

   PC_LAMMPS task failed (relax.in, occurrence 1): reading --varloss F after the script: no value (the variable is not defined, or has no single value)
   PC_LAMMPS task failed (hot.in, occurrence 1): reading --hotvar hot_a0 after the script: its value 'abc' is not a number
   PC_LAMMPS task failed (relax.in, occurrence 1): reading --failvar viol after the script: its value 'inf' is not a finite number

Before 1.6.0 such a value was skipped: a ``--varloss`` one added
``FAILSCORE`` to the mean of the evaluation's terms, which with other terms
fell below ``0.999 × FAILSCORE`` and counted as a feasible loss; a hot
value had no range check or warm start, and the cold scripts ran without it;
a failvar counted as no violation; a diagnostic was ``NaN`` with nothing in
the journal.  A startup check already refuses a ``--varloss``, ``--outvar``
or ``--failvar`` name the script never defines (as an ``equal``-style
variable); this covers what only the run shows.

A variable that reads one that cannot be read (``variable Pdiff equal
v_Pend-1.0``) raises that one's LAMMPS error, so the journal gives the cause
whichever of them fails the simulation.  The LAMMPS message may go on with
``Last input line:`` and the script's last command, which ran before the
variable was read.  A LAMMPS error inside the
script is journalled the same way, with its own message (``PC_LAMMPS task
failed (bad.in, occurrence 1): ERROR: Unknown command: bogus_command 1 2
…``); a script that times out has no message.  So is a task that fails
before its script runs, for a reason that is not the candidate's: a LAMMPS
instance that cannot be created (``PC_LAMMPS task failed (relax.in,
occurrence 1): creating the LAMMPS instance: ERROR: …``) or a script that
cannot be rendered to its temporary file (``… rendering the script to a
temporary file: …``, e.g. when ``TMPDIR`` names no directory).  Such a
failure gives that script's variables their ``--failscore`` and the
evaluation the loss ``FAILSCORE`` (under ``FAILSCORE_GRADING`` a constant
failure loss, not graded: :ref:`failscore_grading`), but does not abort
the evaluation: the other scripts run, but for a cold script that reads a
hot value the failed script did not produce (``--hotvar``); before 1.6.0
it left nothing in the journal.  Each kind of line is
written for the occurrences 1, 2, 4, 8, … of a run.  The LAMMPS instance
that raised the error is discarded and created afresh for the next
evaluation.  To read a thermo keyword after the run, list it in
``thermo_style`` (``thermo_style custom step pe press``), or read it
through a compute defined before the run (``compute P all pressure
thermo_temp``, ``variable Pend equal c_P``).

Before 1.6.0 such a read-back error of a ``--varloss`` or ``--outvar``
variable ended a ``PC_ISOLATION THREAD`` run (the only mode of the MPI
build) with a segmentation fault, and under ``PROCESS`` failed the whole
script, a diagnostic included, with a journal line that did not name the
variable; a ``--hotvar`` or ``--failvar`` that could not be read was taken
as absent (no range check or warm start, no violation), and an error
inside the script left no journal line.

.. _pc_lammps_execution:

Execution, isolation and timeouts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Four root keys control how ``PC_LAMMPS`` scripts are executed:

``PC_ISOLATION THREAD|PROCESS`` (default ``PROCESS``)
   ``PROCESS`` runs each worker slot's LAMMPS instances in a persistent
   child process, so concurrent instances never share LAMMPS' process
   globals; model training and prediction stay in the main process.
   ``THREAD`` runs the instances in-process; it is a deprecated fallback
   and logs a warning.  The worker processes ignore ``SIGINT``,
   ``SIGTERM`` and ``SIGUSR2``: ``tadah`` stops the run and ends them
   (:ref:`hpo_stopping`).  The MPI-parallel build (``TADAH_HPO_MPI``,
   :ref:`tadah_anneal_mpi`) cannot use worker processes: there the default
   becomes ``THREAD`` (noted in the journal), and an explicit
   ``PC_ISOLATION PROCESS`` is an error.  Environment override:
   ``TADAH_PC_ISOLATION``.  A worker process that dies — killed at its
   deadline (``PC_WORKER_TIMEOUT``), by the kernel's out-of-memory killer
   or by a crash inside LAMMPS — leaves the script it was running not
   completed (band C under ``FAILSCORE_GRADING``), and a new worker
   takes the slot's next task.  The journal names the slot and the
   worker's pid, the pid an out-of-memory line of the kernel log names.
   A worker killed inside its script, by the out-of-memory killer for
   example, gives:

   .. code-block:: none

      WARNING   pc-worker 2 (pid 41237) killed/died: RESULT recv failed: pcwire: peer closed (EOF); wait: signal 9 (Killed); consecutive deaths: 1

   The text after ``killed/died:`` says how ``tadah`` found the death: a
   worker killed at its deadline gives ``task deadline expired (<N> ms)``,
   and one that died between two tasks the error of the next task's
   exchange (``RESULT recv failed: pcwire: recv failed: Connection reset
   by peer``, for example).

``PC_SCRIPT_TIMEOUT <time>|off`` (default ``off``; legacy alias: root ``MAXTIME``)
   A soft wall-clock limit per script, injected as LAMMPS
   ``timer timeout`` after every ``clear`` (a script that issues its
   own ``timer`` command is left alone).  The value is a LAMMPS time
   spec: ``off`` or ``unlimited`` (no limit), or a positive
   ``[[HH:]MM:]SS`` of one to three non-empty parts, each a number of the
   grammar above — ``90``, ``1:30`` and ``00:01:30`` are all 90 s, the
   seconds ``timer timeout`` makes of them.  That is a strict subset of
   what LAMMPS reads, which also takes an empty or a fourth part
   (``1::30``, ``1:2:3:4``) and the forms of ``std::stod`` (``0x10``,
   ``inf``).  Anything else is an error naming the line
   (``PC_SCRIPT_TIMEOUT must be 'off', 'unlimited' or a time
   [[HH:]MM:]SS, got 'bad'``).  A limit of ``0`` or less is rejected too,
   although LAMMPS accepts it: ``0`` stops every script at its first
   timer check, and with ``0`` or a negative limit ``$(timeremain)`` is
   0, so the section guard stops every multi-section script; the
   evaluations would score ``FAILSCORE``.  Write ``off`` or ``unlimited``
   for no limit:

   .. code-block:: none

      Error on line 9: PC_SCRIPT_TIMEOUT must be a positive time, 'off' or 'unlimited' (a limit <= 0 stops every script), got '0'
      Line: PC_SCRIPT_TIMEOUT 0

   ``--maxtime`` follows the same rule.  A limit whose ``PROCESS``
   worker deadline (below) does not fit in 64-bit milliseconds — above
   about 3.07·10\ :sup:`15` s — is an error too (``PC_SCRIPT_TIMEOUT is
   too large for the worker deadline, got '4e15' (…)``).  A timed-out
   script counts as failed.  Like ``LOSS``, the value is captured by each ``PC_LAMMPS``
   line when it is read; ``--maxtime`` overrides it for one line.  If
   both spellings are given the later line wins and a warning is
   logged.  Do not confuse it with ``MAXTIME`` *inside* the
   ``OPTIMIZER`` block, which limits the whole optimiser run.

``PC_WORKER_TIMEOUT <seconds>`` (default ``0``; ``PROCESS`` mode only)
   The hard deadline after which a worker process running one script is
   killed.  ``0`` derives it from the script's own timeout
   ``t`` (in seconds, from its time spec) as
   :math:`\max(3t,\ t + 120\,\mathrm{s})`, and applies no deadline to
   scripts whose timeout is ``off`` or ``unlimited``; a positive value is
   used for every script.  Must be a single finite number ``≥ 0``, at
   most about 9.2·10\ :sup:`15` s (the deadline is kept in 64-bit
   milliseconds).  Environment override: ``TADAH_PC_WORKER_TIMEOUT``.
   (Before 1.6.0 the derived deadline read only the leading number of the
   time spec: ``2:00:00`` gave 122 s and ``00:05:00`` no deadline; and a
   deadline above 2\ :sup:`31` − 1 ms, about 24.8 days, wrapped, so
   the worker was killed early or not at all.)

``BUMPGATE CANDIDATE|STRICT``, ``RHOGATE``, ``RHOBARGATE``
   Not ``PC_LAMMPS`` settings as such, but they decide whether a
   candidate reaches the simulations at all; see
   :ref:`hpo_feasibility_gates`.

The startup journal prints the effective soft and hard limits.

.. _pc_observable:

Typed observables (PC_OBSERVABLE)
.................................

``PC_OBSERVABLE`` evaluates a physical property *in process*, without
LAMMPS, with a closed-form derivative with respect to the model
weights.  Each line is one loss term.  It cannot be combined with
``PC_LAMMPS`` in the same configuration.  The only kind at present is
``cohesive_energy``:

.. code-block:: none

   PC_OBSERVABLE cohesive_energy --species Ta --target 8.10 --weight 100 \
                 --hotvar a0 2.8 3.8 3.30

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Flag
     - Meaning
   * - ``--species <X>`` (required)
     - Element of the bcc crystal.
   * - ``--target <E>`` (required)
     - Target cohesive energy in eV/atom, positive for a bound crystal
       (:math:`E_\mathrm{coh} = E_\mathrm{isolated} - E_\mathrm{bulk}/N`).
   * - ``--weight <w>`` (required)
     - Weight of the term.
   * - ``--hotvar a0 <lo> <hi> [<init>]`` (required)
     - Lattice-constant window of the relaxation, and its optional
       starting value (default: the midpoint).  Only ``a0`` is supported.
   * - ``--lattice bcc``
     - Crystal structure; only ``bcc`` is supported (default).
   * - ``--supercell <nx> <ny> <nz>``
     - Supercell size (default ``4 4 4``): integers from 1 to 2147483647.
       The neighbour list holds every periodic image, so any size gives the
       bulk energy of the perfect crystal (``1 1 1`` too); a larger one
       costs time.
   * - ``--loss <name>``
     - Loss of this term (default ``L2``; ``HUBER``/``TUKEY`` use their
       default parameter).  ``MAPE`` and ``RMSLE`` have no analytical
       derivative with respect to the value, so with a gradient-based
       backend they need ``GRAD_STRATEGY HYBRID`` (see *Gradients*
       below).
   * - ``--maxiter <n>``
     - Relaxation iteration cap (default ``5000``): an integer from 0 to
       2147483647.
   * - ``--maxeval <n>``
     - Cap on energy/force evaluations of the relaxation (default
       ``5000``): an integer from 0 to 2147483647.
   * - ``--etol <x>``
     - Energy convergence tolerance (default ``0``: force-only convergence).
   * - ``--max-wall-time <s>``
     - Wall-clock cap per relaxation in seconds (default ``0``: none).
   * - ``--fmax-converged <f>``
     - Largest residual force (eV/Å) accepted as converged (default ``0.1``).
   * - ``--unconverged const <V>`` / ``--unconverged force-scaled <slope>``
     - Value reported when the relaxation does not converge: the constant
       ``V`` (default ``const 100``; its gradient is zero), or
       ``target + slope × fmax``, which keeps a slope toward convergence
       that only finite differences can follow: with a gradient-based
       backend ``force-scaled`` needs ``GRAD_STRATEGY HYBRID`` (see
       *Gradients* below).

A term whose evaluation fails — a relaxed lattice constant outside the
``--hotvar a0`` window, a divergent relaxation, a value or a loss that is
not finite — is ``FAILSCORE`` in ``loss.tadah`` (its value ``NaN`` in
``outvar.tadah``), and its evaluation is a failed one: the loss is
``FAILSCORE`` whatever the other terms, and under ``FAILSCORE_GRADING ON``
a graded band-B failure measured by the failed fraction of the
``PC_OBSERVABLE`` terms (:ref:`failscore_grading`).  Before 1.6.0 the
loss was the mean of ``FAILSCORE`` and the other terms under either
setting, about ``FAILSCORE`` / *n* for one failed term of *n* ≥ 2: below
``0.999 × FAILSCORE``, a usable loss, which could become the best or
reach ``STOPVAL``; under the default ``FAILSCORE`` the other terms
vanished in it, every finite difference across the region where the term
failed was 0, and a gradient search that started there converged on it.
(An evaluation with no usable term, or one whose failed terms summed
past the largest double, scored ``FAILSCORE`` then too.)  Such a search
now stops at its first gradient: under ``FAILSCORE_GRADING OFF`` at
once, under ``ON`` when every point of its finite-difference stencil
fails with the same failed fraction (a flat failure region; an
analytical gradient at a graded failure is kept;
:ref:`hpo_gradient_not_finite`).

Columns.  The *k*-th ``PC_OBSERVABLE`` line (1-based, in configuration
order) writes the ``loss.tadah`` column ``L(<tag>)[k]`` and the
``outvar.tadah`` column ``<tag>[k]``, where the tag of
``cohesive_energy`` is ``Ecoh``: ``L(Ecoh)[1]`` and ``Ecoh[1]`` for the
first line.  The journal names the term ``cohesive_energy[k]`` (the
loss section and the ``top:`` breakdown of a new best).  These labels
fit every ``DIGITS`` setting up to ``k = 99``; past that the tag is
shortened and the index kept (``L(Eco*[100]``), and the journal's loss
section names the columns as the files write them.  Before 1.6.0 the
index was 0-based and the loss column ``L(cohesive_energy[0])``, which
every ``DIGITS`` setting cut to ``L(cohesive*``.

Gradients.  ``PC_OBSERVABLE`` is built for the analytical gradient
(``GRAD_STRATEGY``, :ref:`hpo_optimizer_common`), which differentiates
each term through the refitted weights and through its own structures
(the relaxed crystal and the isolated atom) at fixed weights; the
relaxation itself adds nothing at the relaxed geometry.  In every block
that runs an optimiser and obtains gradients — the ``STRATEGY``-level
block and ``EXPLORE``, with a gradient-based backend (``DLIB:BFGS``,
``NLOPT`` ``LD_*``/``GD_*``, ``CERES``) or an ``MLSL``/``AUGLAG`` whose
local search is one (for ``MLSL`` and ``AUGLAG`` the local search alone
decides, whatever the ``D`` or ``N`` of their name: ``GD_MLSL_LDS``
with an ``INNER`` ``LN_BOBYQA`` takes no gradient, and before 1.6.0 an
unset ``GRAD_STRATEGY`` resolved to ``ANALYTICAL`` there):

* an unset ``GRAD_STRATEGY`` resolves to ``ANALYTICAL``, and the
  journal's OPTIMISER section says so (``GRAD_STRATEGY : ANALYTICAL
  (resolved for PC_OBSERVABLE)``); derivative-free blocks are left
  alone;
* ``HYBRID`` and ``ANALYTICAL`` written out are kept;
* ``GRAD_STRATEGY NUMERICAL`` is rejected.

An ``INNER`` block is not resolved, and takes no ``GRAD_STRATEGY`` of
its own: ``MLSL`` and ``AUGLAG`` hand their ``INNER`` search the
gradient of their own objective, so the ``GRAD_STRATEGY`` of the block
they belong to decides how it is obtained.  Set it in that block; one
written in the ``INNER`` block is a startup error at its line
(:ref:`hpo_inner_block`).

``ANALYTICAL`` needs a setup the analytical gradient covers — see the
``GRAD_STRATEGY`` row of :ref:`hpo_optimizer_common` — and
``PC_OBSERVABLE`` lines whose values it can differentiate: no
``--loss MAPE`` or ``RMSLE`` and no ``--unconverged force-scaled``.
Startup refuses a setup that falls short, naming the setting or the
line; the shared example model, for instance, uses ``OALGO 1``:

.. code-block:: none

   HPOEngine error: OPTIMIZER block: GRAD_STRATEGY ANALYTICAL (resolved for PC_OBSERVABLE: the block sets no GRAD_STRATEGY) needs the analytical gradient of the loss, which this setup cannot form: OALGO is 1 (it needs OALGO 4, the Cholesky normal equations). Change that setting, or set GRAD_STRATEGY HYBRID to use finite differences where no analytical gradient is available.

and a ``force-scaled`` line (a ``MAPE`` or ``RMSLE`` one alike):

.. code-block:: none

   HPOEngine error: OPTIMIZER block: GRAD_STRATEGY ANALYTICAL (resolved for PC_OBSERVABLE: the block sets no GRAD_STRATEGY) needs the analytical gradient of the loss, which this setup cannot form: the PC_OBSERVABLE term cohesive_energy[1]: --unconverged force-scaled reports target + slope x fmax for a relaxation that does not converge, a value without an analytical derivative. Change that setting, or set GRAD_STRATEGY HYBRID to use finite differences where no analytical gradient is available.

``HYBRID`` takes finite differences only where the analytical gradient
is missing: at an unconverged ``force-scaled`` relaxation, or for every
gradient with ``MAPE``/``RMSLE``.  The constant ``--unconverged const``
penalty contributes a zero gradient and needs neither.

Before 1.6.0 an unset ``GRAD_STRATEGY`` ran finite differences with
``PC_OBSERVABLE``, and the analytical gradient left out the term's own
dependence on the descriptor parameters and the averaging over the loss
terms, so it could point the wrong way.  Example ``22_pc_observable``
(:ref:`hpo_examples`) runs this configuration.

.. _failscore_grading:

Failure handling — graded FAILSCORE
...................................

What problem this solves
~~~~~~~~~~~~~~~~~~~~~~~~

When a candidate potential makes a simulation fail — a relaxation that
does not converge, a hot value outside its window, a hung run that had
to be killed — the iteration traditionally returns the constant
``FAILSCORE``.  A constant is a *flat* landscape: inside a failing
region every proposal looks equally bad, so annealers random-walk or
freeze (their step-size adaptation has two absorbing states on a flat
or cliff-edged landscape), parallel-tempering ladders die on the
plateau, and gradient methods see exactly zero slope.  The optimiser
receives no hint of *which way is out*.

``FAILSCORE_GRADING ON`` replaces the constant with a **graded** value:
each gradable failure returns a loss inside the reserved band
``[0.999 × FAILSCORE, FAILSCORE)`` that *decreases as the candidate
gets closer to working*.  Feasible results always win (they are orders
of magnitude below the band).  The annealing family ranks finite values
inside the band in its Metropolis decisions and chain/walker personal-best
updates, so their ordering supplies a downhill path toward feasibility.
``DLIB:GFS`` keeps that order only among the stand-ins it hands dlib in
the same state of its search: a stand-in is not revised, so a failure
before the search's first usable loss stays far above the later ones
(:ref:`failed evaluations under DLIB:GFS <hpo_dlib_gfs_failures>`).
Operations that require a genuinely feasible sample remain stricter:
``STOPVAL``, the Corana ``FTOL`` plateau, T0 calibration, and PA population
weighting/resampling classify the entire reserved band as failed.  Thus a
failure value cannot report convergence; PA gives a graded walker zero
resampling weight whenever a feasible walker exists, while an all-failed
population skips resampling and continues cooling.  A gradient search
at a graded failure takes its finite differences as they are, a direction
out of the failure region, and its gradient fails there only when every
difference is 0, as in a band that is constant.  Next to the usable side
a difference spans the jump from the band to a usable loss: it is as
steep as that jump, not the band's own slope, and a search that starts
there can evaluate a bound again and again, stop at once as converged, or
leave the box (:ref:`hpo_gradient_not_finite`).  A usable point whose
finite-difference stencil reaches a failure, graded or not, has no
gradient.  ``GRAD_STRATEGY ANALYTICAL`` keeps its analytical gradient at
a graded failure, which does not see the grading.

The knobs, most important first
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``FAILSCORE <value>`` (root)
   The failure ceiling, a positive finite number as always — but with
   grading ON it **must** also be below ``1e300`` (e.g. ``1e9``) and
   large enough that all four grading bands are strictly representable;
   the plain ``DBL_MAX`` default and unrepresentable tiny values are
   rejected at startup.  Choose it a few orders of magnitude above your
   worst plausible *real* loss.

``FAILSCORE_GRADING ON|OFF`` (root, default ``OFF``)
   The master switch.  ``OFF`` reproduces the ungraded
   behaviour: a failed evaluation scores the constant ``FAILSCORE``.
   What follows grades or gates only when this is ``ON``; under ``OFF`` a
   simulation that fails still fails its evaluation, and so does one
   whose ``--failvar`` cannot be read back or is not a finite number, and
   a ``PC_FAILPROBE`` whose script fails or whose ``S`` cannot be read or
   has no number.  The startup warning for a probe line under ``OFF``
   says so; it said that the probe ran "as a plain diagnostic sim", but a
   probe whose script failed failed the evaluation then too.
   Environment override: ``TADAH_FAILSCORE_GRADING=ON|OFF``.

``--failvar <name> [<scale>]`` (per ``PC_LAMMPS`` line)
   The per-script violation channel (see the option reference above and
   the examples below).  Costs one ``variable`` line per script and
   upgrades that script's failures from “counted” to “measured”.

``PC_FAILPROBE --script <f> --outvar <S> [--gate <T>] [--s0 <v0>]`` (root)
   A tiny, minimiser-free *sanity probe* executed at the front of every
   iteration's parallel batch.  Its first ``--outvar`` is a single
   nonnegative scalar ``S`` measuring how unphysical the candidate is
   (0 = sane).  ``S`` becomes the failure measure for the failure modes
   that cannot measure themselves — hung runs that were killed, crashed
   potential builds — and, optionally, a **gate**: when ``S > <T>`` the
   iteration is short-circuited before wasting the full simulation
   budget.  ``<v0>`` must be finite and strictly positive; it sets the
   saturation scale of the grading curve for ``S`` (default 1; a good
   choice is the seed potential's ``S`` times a small factor).  At most
   one probe line is allowed; ``--varloss``, ``--hotvar`` and
   ``--failvar`` on it are rejected (the probe grades failures — it must
   not shape the feasible objective).
   An ``S`` that cannot be read back after the probe's
   script, or has no number for its value (:ref:`pc_lammps_readback`), is
   a failed probe, as when the script fails: the evaluation is aborted
   (under ``OFF`` too) and, under ``FAILSCORE_GRADING``, graded band D
   with ``S`` unavailable.

``--gate <T>`` (on ``PC_FAILPROBE``)
   **The only knob that needs real care.**  If the gate can fire on a
   candidate that would actually have succeeded, you have changed the
   objective, not just saved time.  Calibrate it from a short run:
   journal the ``S`` statistics of accepted iterations, then set ``T``
   far above anything a feasible candidate produces (same philosophy as
   the ``--skip-above`` caps).  Gate firings are journaled and
   rate-limited; watch the counter.

``PC_WORKER_TIMEOUT`` / ``PC_SCRIPT_TIMEOUT`` / per-sim ``--maxtime``
   Unchanged (:ref:`pc_lammps_execution`), but they *interact*: a killed
   hung run is graded by ``S`` (band C below), so with a probe defined
   even timeouts become directional instead of flat.

Order of firing within one iteration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

   feasibility gates (BUMPGATE, RHOGATE, RHOBARGATE)  →  [any fails?  → graded band D, stop]
   train/validate  →  [skip caps?  → graded band D, stop]
   PC_OBSERVABLE terms, in place of PC_LAMMPS  →  [a term failed?  → graded band B
                                                   by the failed fraction, stop]
   dispatch batch:  FAILPROBE first (cheapest → returns first),
                    hot scripts + independent cold scripts alongside
     ├─ probe returns S
     │    ├─ S ≤ gate (or no gate): S is stored for later grading
     │    └─ S > gate: iteration short-circuited → band D graded by S;
     │       queued sims are cancelled; in PROCESS mode this
     │       iteration's in-flight sims are killed (other concurrent
     │       iterations on the shared pool are never touched)
     ├─ each sim finishes → failvars read (0 ⇒ OK; >0 ⇒ band B, graded)
     │    hot values validated → out of window ⇒ band A, graded by the
     │    normalized distance to [min, max]
     ├─ a sim times out in-script (timer timeout) → instance still
     │    alive → failvars still readable → band B
     └─ a sim hangs → parent deadline kill (PROCESS mode) → nothing
          readable → band C, graded by S
   dependent cold wave → loss assembly:
     no failure  → normal loss (identical to grading OFF)
     any failure → L_fail in [0.999·FS, FS), see the band table

The severity bands (worst on top) encode the natural *repair path* —
make the potential sane, then runs complete, then outputs valid, then
windows satisfied, then feasible:

.. list-table::
   :header-rows: 1
   :widths: 8 46 46

   * - Band
     - Failure modes
     - Graded by
   * - D
     - probe gate fired; potential build crashed; skip caps; a
       :ref:`feasibility gate <hpo_feasibility_gates>` rejected the
       candidate before training
     - ``S`` (probe) / metric excess (caps) / the gate's violation
       measure ``v``
   * - C
     - run hung and was killed; worker died
     - ``S`` (probe)
   * - B
     - run completed but failed: in-script timeout, ``--failvar > 0``,
       unreadable/NaN outputs; a failed ``PC_OBSERVABLE`` term
     - failvar sum + failed fraction (of the scripts; of the
       ``PC_OBSERVABLE`` terms)
   * - A
     - hot value outside its window
     - normalized distance to the window

Cross-band ordering is fixed (every ``A`` value beats every ``B``
value, etc.), within a band less violation is strictly better, and one
iteration gets exactly one band (the worst one that occurred).
Failures that are *not the potential's fault* — a LAMMPS instance that
could not be created, worker crash-loops — are deliberately **not**
graded (a directionless constant), so the optimiser never chases
infrastructure noise.

Designing ``failprobe.in``
~~~~~~~~~~~~~~~~~~~~~~~~~~

Four rules make a good probe:

1. **It must not be able to fail or hang.**  Single-point evaluations
   only: fixed cells, ``run 0`` — no ``minimize``, no ``box/relax``,
   no dynamics.
2. **It must be smooth in the potential parameters.**  Build ``S`` from
   hinge/softplus penalties of energies, forces and stresses — these
   are smooth functions of the parameters, so ``S`` gives a descent
   direction.
3. **It must be cheap** — a few small single-point cells cost
   milliseconds and disappear into the parallel batch.
4. **No ``label``/``jump`` loops.**  Scripts are executed
   command-by-command through the LAMMPS library interface, where the
   file-rewind machinery behind ``jump`` does not work reliably (it
   can half-work in ways that depend on leftover loop variables from
   the *previous* evaluation in the cached instance).  Unroll the few
   cells you probe — it is also easier to read.

Good ingredients (all already proven in the ``pen_*`` observables):
equation-of-state sanity (energy at a handful of fixed volumes around
equilibrium: hinge on non-monotonic compression branches), near-zero
forces on the ideal lattice, finite and positive stress response.  A
ready-to-adapt example:

.. code-block:: bash

   # failprobe.in — minimiser-free sanity scalar S for bcc metal.
   # Contract: NEVER minimize/box-relax/run>0; define S even on the
   # weirdest potential; keep it O(ms); no label/jump loops (rule 4) —
   # the three volumes are unrolled. `variable X equal $( ... )`
   # captures the value IMMEDIATELY, and equal-style redefinition
   # replaces the old value, so the captures stay fresh in the cached
   # instance on every evaluation.

   # -- E(V) monotonicity: 2-atom bcc cells at 0.90/1.00/1.10 a0 -----
   # (a0 = 3.30 reference lattice constant, baked into the lattice
   #  commands below)
   clear                    # scripts run in a CACHED LAMMPS instance:
                            # always start with clear (variables survive)
   units metal
   dimension 3
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc $(3.30*0.90)
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_m10 equal $(pe/atoms)     # captured now (immediate $)

   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc 3.30
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_0 equal $(pe/atoms)

   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc $(3.30*1.10)
   region box block 0 1 0 1 0 1
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   run 0 post no
   variable e_p10 equal $(pe/atoms)

   # compressed branch must rise toward small volume; expanded branch
   # must rise toward large volume (bound state). Hinge each violation.
   variable h_comp equal $(ternary(v_e_m10<v_e_0, v_e_0-v_e_m10, 0.0))
   variable h_exp  equal $(ternary(v_e_p10<v_e_0, v_e_0-v_e_p10, 0.0))

   # -- force sanity: rattle a cell DETERMINISTICALLY (fixed seed) and
   # hinge on absurd forces. Do NOT probe fmax on the ideal lattice --
   # there it is zero by symmetry for ANY potential, sick or sane.
   clear
   units metal
   boundary p p p
   atom_style atomic
   atom_modify map array
   lattice bcc 3.30
   region box block 0 2 0 2 0 2
   create_box 1 box
   create_atoms 1 region box
   mass 1 180.95
   pair_style tadah
   pair_coeff * * EXTERN Ta
   displace_atoms all random 0.05 0.05 0.05 12345 units box
   run 0 post no
   variable h_frc equal $(ternary(fmax>20.0, fmax-20.0, 0.0))

   # -- compose S: bounded, nonnegative, 0 for a sane potential ------
   variable S equal $(v_h_comp + v_h_exp + v_h_frc)

with the configuration line

.. code-block:: none

   FAILSCORE_GRADING ON
   PC_FAILPROBE --script pc_lammps/failprobe.in --outvar S --s0 0.5
   # add "--gate 50" ONLY after calibrating S statistics on your seed

(Adapt the element, ``a0``, volumes and thresholds to your system; add
Born-stability hinges via finite-difference stress probes on fixed
strained cells if shear stability matters for your fits.)  The HPO
examples ship this probe as ``examples/common/lammps/failprobe.in``;
example ``25_failprobe_grading`` (:ref:`hpo_examples`) runs it with
``FAILSCORE_GRADING ON`` and a lattice window that grades six band-A
failures.

``--failvar`` recipes for typical scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Pressure-target relaxation** (``box/relax`` at pressure ``P``): the
convergence check already computes the violation — export it:

.. code-block:: bash

   fix relax all box/relax iso ${P_bar} vmax 0.003
   minimize 1e-10 1e-10 2000 20000
   unfix relax
   variable Pres equal $(press)*1.0e-4
   # 0 when converged within 1 GPa; the residual (GPa) otherwise:
   variable fail_press equal $(ternary(abs(v_Pres-v_P_GPa)<1.0, 0.0, abs(v_Pres-v_P_GPa)))
   # ... only now compute the real observables ...

.. code-block:: none

   PC_LAMMPS --script pc_lammps/elastic_P_single.in --invar P 300 \
       --varloss Cp_P300 232.8 7.6 --varloss C44_P300 332.9 3.9 \
       --failvar fail_press 0.01

**Minimisation that must actually converge** (vacancy, surface,
stacking-fault energies): grade by the residual force instead of
returning a sentinel:

.. code-block:: bash

   minimize 1e-10 1e-10 2000 20000
   # 0 when converged; the residual max force (eV/Å) otherwise:
   variable fail_fmax equal $(ternary(fmax<1.0e-8, 0.0, fmax))

.. code-block:: none

   PC_LAMMPS --script pc_lammps/vac_bcc_P_fast.in ... --failvar fail_fmax 10

Both patterns follow the same contract: **define the failvar
immediately after the risky command** (it survives a later in-script
timeout), keep it ``0`` on success, and make its magnitude the natural
physical distance from success.  Where your scripts currently return
sentinel values (``-1000``) as fake observables, move that logic into a
``--failvar`` — the sentinel stops polluting the fitted region and its
information starts steering the optimiser instead.

Setup effort, honestly
~~~~~~~~~~~~~~~~~~~~~~

Level 0 (just ``FAILSCORE_GRADING ON``): hot-window rejections and
partial failures become graded automatically — no script changes, and
the worst optimiser pathologies (plateau death, step-size collapse)
disappear.  Level 1 (one ``--failvar`` line per script that has a
convergence check): minutes per script.  Level 2 (the probe): one
script, once, assembled from the same hinge idioms as the ``pen_*``
observables.  The only knob that can *change your objective* if set
carelessly is the probe ``--gate`` — leave it off until you have
calibrated ``S``.

.. _hpo_feasibility_gates:

Feasibility gates
~~~~~~~~~~~~~~~~~

Three root-level keys reject a candidate **before any training work** when
its model is invalid in a way the parameters alone decide:

.. list-table::
   :header-rows: 1
   :widths: 34 16 50

   * - Key
     - Default
     - Rejects a candidate whose
   * - ``BUMPGATE CANDIDATE|STRICT``
     - ``CANDIDATE`` (always on)
     - compact bump reaches past a hard cutoff, :math:`c + w/\eta > r_c`
   * - ``RHOGATE <tol> ...``
     - off
     - DM_REAM pair density :math:`\rho(r)` dips below
       :math:`-\mathrm{tol}\,\rho_{max}` (``RHOFORM SIGNED`` only)
   * - ``RHOBARGATE <lattice> <a0> <f_lo> <f_hi> ...``
     - off
     - lattice density :math:`\bar\rho(a)` is not strictly decreasing on
       :math:`a \in [f_{lo} a_0, f_{hi} a_0]`

**What a reject is.** The candidate gets a :ref:`band-D failure
<failscore_grading>` without being trained, predicted or run through
LAMMPS. With ``FAILSCORE_GRADING ON`` the loss is graded by the gate's
violation measure :math:`v \ge 0`,

.. math::

   L = 0.999\,\mathrm{FS} + 0.001\,\mathrm{FS}\,\Big(0.75 + 0.25\,\frac{v}{1+v}\Big),

so :math:`v = 0` sits at the band-D floor :math:`0.99975\,\mathrm{FS}`,
a smaller violation is always a lower loss, and a *saturated* measure
(:math:`v` = ``DBL_MAX``, for a density gate whose density is not finite,
has no positive value, or has a pair weight :math:`\le 0`; see below) sits
at the top of band D. With
grading ``OFF`` the loss is the flat ``FAILSCORE``. A reject is otherwise a
complete evaluation: it takes an evaluation number and a ``MAXEVAL`` slot
(at almost no wall time, so budget ``MAXEVAL`` for them or stop on
``MAXTIME``), its rows are written, it counts toward stagnation, and it can
never become the best; its ``DUMP`` archive is header-only (no potential
was built). The verdicts are pure functions of the candidate and the
settings (the same at any ``THREADS``), and a gate never changes the
candidate, so a candidate that passes evaluates bit for bit as without the
gate.

**Journal.** The first five rejects of each gate, then every power of two,
are ``INFO`` lines ``eval N: <gate> reject (...); total rejects=K``, where
``K`` counts that gate's rejects. The ``SUMMARY`` failure census gains
``bumpgate=N``, ``rhogate=N`` and ``rhobargate=N`` (each when it is not 0)
after the band counts, and ``DIAGNOSTICS`` breaks the band-D count down::

   SUMMARY ... failed=86 (71.7%; A=0 B=0 C=0 D=86) rhogate=57 rhobargate=29

     Graded failures         : 86  (71.7%)
       A=0 (hot-var out of range)  B=0 (script or observable failed)  C=0 (did not complete)  D=86 (pot build / gated)
       of which rho-sign gate rejects: 57 (rho_min < -tol*rho_max on the RHOGATE window, rejected before training)
       of which lattice-rhobar gate rejects: 29 (rhobar(a) not strictly decreasing, rejected before training)

(``of which bump-support gate rejects: N (c + w/eta past RCUT, rejected
before training)`` for ``BUMPGATE``.)

BUMPGATE — bump support inside a hard cutoff
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   BUMPGATE CANDIDATE | STRICT        # default CANDIDATE

On a channel whose basis is a compact bump family (``D2_Blip`` or
``D2_Blip5`` pair channels, the ``Blip``/``Blip5`` density of ``DM_REAM``)
and whose cutoff is hard (``Cut_Dummy``, also when ``RCTYPE*`` is absent),
every bump must end at or before the cutoff: :math:`c + w/\eta \le r_c`,
with :math:`c` the ``CGRID*`` centre, :math:`\eta` the ``SGRID*`` width,
:math:`w = 2` (Blip) or 3 (Blip5) and :math:`r_c` the ``RCUT*`` value (to a
relative tolerance of ``1e-6``). A bump that overhangs is cut off at
:math:`r_c`: an energy and force step whenever a neighbour crosses the
cutoff. Under a smooth cutoff the overhang is damped to zero and there is no
gate.

* **Startup, both modes.** The ``-c`` potential's own bumps must fit its
  stated cutoff: a centre beyond it, or a support past it, is an error that
  says how to fix it (the ``SGRID*`` value that fits, moving the centre with
  ``tadah refit --edit``, or a larger ``RCUT*``).
* **CANDIDATE** (default). An ``OPTIM`` box is refused only when it holds no
  feasible point at all (even its most favourable corner, lowest centre and
  largest :math:`\eta`, reaches past the largest cutoff a candidate can
  have). A box that is only partly feasible is accepted at full width, and
  an ``INFO`` message per bump (``tadah`` log, verbosity 2) says that the
  gate is armed for it. Every armed bump is then checked on every candidate,
  against the candidate's own sampled cutoff when ``RCUT*`` is ``OPTIM``'d,
  with the measure

  .. math::

     v = \sum_{\text{violating bumps}} \frac{(c + w/\eta - r_c)\,\eta}{w},

  the summed overhang in units of the half-width, which grows with every
  violating coordinate. A width :math:`\eta \le 0`, which only an
  optimiser that leaves the ``OPTIM`` box evaluates (the box's low bound
  on a width is :math:`> 0`), has no support edge and is a violation too:
  it counts :math:`1 - \eta\, r_c / w`, the continuation of the measure
  (which tends to 1 as :math:`\eta \to 0^+`), and a NaN width the top of
  band D; it was skipped before 1.6.0. The reject line reads ``eval N:
  bump-support gate reject (c + w/eta past the cutoff, violation=<v>,
  graded band D); total rejects=K``, with :math:`v` in the shortest text
  that reads back as the same double (six decimals before 1.6.0, so a
  :math:`v` below :math:`5\times10^{-7}` read ``0.000000``). A
  configuration whose boxes cannot violate arms nothing and evaluates
  exactly as without the gate. A bump channel whose ``CGRID``/``SGRID``
  grids or per-pair ``RCUT`` list cannot be read, or whose grids do not
  pair up, is an error naming the key (its descriptor cannot be built),
  not a channel the gate skips.
* **STRICT** (the behaviour of 1.4.0, before ``CANDIDATE`` became the
  default). The worst corner of every box
  (``CGRID*`` high bound, ``SGRID*`` low bound, the lowest ``OPTIM RCUT*``
  bound) must fit, or startup stops naming the bound to change; no candidate
  is rejected at run time. Use it for strict ``MAXEVAL`` budgeting or to
  reproduce an older run.

The environment variable ``TADAH_BUMPGATE=CANDIDATE|STRICT`` overrides the
key. ``OPTIM RCUT*`` on a bump channel is usually a wasted dimension (a
feasible bump vanishes inside the cutoff, so the sampled value only moves
the feasible region; startup warns).

*Example.* A ``D2_Blip5`` pair channel with ``RCTYPE2B Cut_Dummy`` and
``RCUT2B 5.5`` whose fourth bump sits at ``CGRID2B`` 4.4, ``SGRID2B`` 3
(reach :math:`4.4 + 3/3 = 5.4`), searched with

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM CGRID2B (4) 4.0 5.0
   OPTIM SGRID2B (4) 2.0 4.0

The most favourable corner (4.0, 4.0) reaches 4.75 and the worst (5.0, 2.0)
reaches 6.5, so ``CANDIDATE`` accepts the box and arms the gate for bump 4::

   bump-support gate (CANDIDATE): armed for CGRID2B bump 4 — candidates with c + 3/eta > RCUT2B = 5.5
   are rejected before training as graded band-D failures (worst box corner reaches 6.5). BUMPGATE
   STRICT restores the startup box-corner error.

A candidate at :math:`c = 4.8`, :math:`\eta = 2.5` reaches 6.0: it is
rejected with :math:`v = 0.5 \cdot 2.5/3 \approx 0.41667` (``eval N:
bump-support gate reject (c + w/eta past the cutoff,
violation=0.4166666666666667, graded band D); total rejects=K``) and a loss of about ``9.99824e8``, while
candidates at (4.4, 3.0) or (4.2, 2.5) reach 5.4 and are trained as usual.
``STRICT`` refuses this box at startup: its worst corner needs an ``SGRID2B
(4)`` low bound of at least :math:`w/(r_c - c_{hi}) = 3/(5.5 - 5.0) = 6`.

RHOGATE — the sign of a SIGNED density
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   RHOGATE <tol> [R_LO <r>] [NPTS <n>] [SEED ERROR|WARN]

======== =========================== ==========================================================
Token    Default                     Rule
======== =========================== ==========================================================
``tol``  — (required)                finite, ``>= 0``
``R_LO`` ``0.15`` × the candidate's  finite, ``>= 0``, below the smallest cutoff a candidate can
         ``RCUTMB``                  have (startup error otherwise)
``NPTS`` ``20001``                   integer from ``2`` to ``10000000``
``SEED`` ``ERROR``                   ``ERROR`` or ``WARN``: what a failing seed does (below)
======== =========================== ==========================================================

The numbers follow the number grammar of every key (:ref:`nfp_manual`):
``tol`` and ``R_LO`` are finite decimal numbers and ``NPTS`` an integer in
decimal digits, so ``NPTS 2e3`` or ``NPTS +100`` is an error at its line.
Keywords and their values are case-insensitive; a second ``RHOGATE`` line, an
unknown or repeated keyword and a keyword without a value are errors. There
is no environment override.
``RHOGATE`` needs a bare DM_REAM many-body term (``INITMB true``, ``TYPEMB
DM_REAM``; ``DM_mJoin`` is refused) with ``RHOFORM SIGNED``: ``NONNEG`` and
``MONOTONE`` guarantee :math:`\rho \ge 0` by construction, and startup says
so.

The gate scans :math:`\rho_{raw}(r) = f_c(r)\sum_n a_n\psi_n(r)`, the pair
density without the pair weight, as DM_REAM trains with it, on ``NPTS``
equally spaced points of :math:`[r_{lo}, r_{hi}]`: :math:`r_{hi}` is the
candidate's ``RCUTMB`` (for a knot density its largest knot) and
:math:`r_{lo}` is ``R_LO`` or :math:`0.15\,r_{hi}`. The pair weights
:math:`W` scale every pair alike, so the verdict does not depend on them,
but each must be finite and :math:`> 0`. The candidate **fails** when

.. math::

   \rho_{min} < -\mathrm{tol}\cdot\rho_{max}

(``tol 0`` rejects any negative value; a small ``tol`` tolerates a small
relative dip), with the measure

.. math::

   v = \min\!\big(10^8,\ v_{peak} + v_{int}\big),\qquad
   v_{peak} = \max\!\Big(0, \frac{-\rho_{min} - \mathrm{tol}\,\rho_{max}}{\rho_{max}}\Big),\qquad
   v_{int} = \sum_i \frac{\max(0, -\rho_i - \mathrm{tol}\,\rho_{max})\,w_i}{\rho_{max}\,(r_{hi} - r_{lo})}

(:math:`w_i` trapezoid weights): the depth of the worst dip plus the mean
depth over the window, both relative to :math:`\rho_{max}`. The measure is
saturated when :math:`\rho_{max} \le 0` on the window, a value is not
finite, a pair weight is :math:`\le 0` or not finite, or the window is
empty. A candidate whose density DM_REAM would refuse (for instance a zero
``SGRIDMB`` entry) gets no verdict: it fails in training exactly as it would
without the gate. The reject line is ``eval N: rho-sign gate reject
(rho_min/rho_max = <rel> at r = <r> A, tol <tol>, v = <v>); total
rejects=K``, or ``(<reason>, v = saturated)``. The scan costs about 0.7 ms
per candidate at 20001 points with 20 Blip5 bumps.

*Example.* A ``SIGNED`` DM_REAM density of three Blip5 bumps (``TYPEMB``
basis code 3) and a search over the sign of the outermost amplitude:

.. code-block:: none

   # -c potential (excerpt)
   CGRIDMB    2.8 3.6 4.4
   SGRIDMB    3.0 3.0 3.0
   AMPGRIDMB  0.5 0.3 0.1
   RHOFORM    SIGNED
   RCTYPEMB   Cut_Dummy
   RCUTMB     5.5

   # config.hpo
   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM AMPGRIDMB (3) -0.5 0.5
   RHOGATE 1e-5

At startup the journal states the gate and the seeds' verdicts::

   INFO  RHOGATE: rho(r) sign gate, tol 1.0000e-05, window [0.15 RCUTMB, RCUTMB] of each candidate,
         20001-point grid, SEED ERROR; armed per candidate on AMPGRIDMB[3]: candidates with
         rho_min < -tol*rho_max are rejected before training as graded band-D failures;
         seeds: -c pot (INIT CONFIG) PASS

(one line in the file). The seed passes (:math:`\rho \ge 0`, ``rel
0.0000e+00``). A candidate with ``AMPGRIDMB(3) = -0.2`` has a dip to
:math:`-0.19966` at :math:`r = 4.4025` Å against :math:`\rho_{max} =
0.50036`, on the window :math:`[0.825, 5.5]`::

   INFO  eval 7: rho-sign gate reject (rho_min/rho_max = -3.9903e-01 at r = 4.4025 A, tol 1.0000e-05,
         v = 4.4515e-01); total rejects=1

and its loss is about ``9.99827e8``. Had the ``-c`` potential itself carried
``AMPGRIDMB 0.5 0.3 -0.2``, startup would stop (``SEED ERROR``)::

   RHOGATE: seed -c pot (INIT CONFIG) fails the rho(r) sign gate: rho_min -1.9966e-01 at r 4.4025 A,
   rho_max 5.0036e-01, rel -3.9903e-01 < -tol = -1.0000e-05 on [0.8250, 5.5000] A (20001 points),
   v = 4.4515e-01. Repair the seed's density (for a WARM seed, its OPTIM'd density values: the
   rest comes from the -c potential) with tadah refit --edit, relax the gate, or add SEED WARN to
   evaluate it as a rejected start.

RHOBARGATE — the lattice density decreases under expansion
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

   RHOBARGATE <bcc|fcc|hcp> <a0> <f_lo> <f_hi> [NPTS <n>] [SEED ERROR|WARN]

The lattice is case-insensitive (``hcp`` takes the ideal
:math:`c/a = \sqrt{8/3}`, with ``a0`` its in-plane constant); ``a0`` is
finite and :math:`> 0`; :math:`0 < f_{lo} < f_{hi}`, both finite; ``NPTS``
is an integer from 3 to 10000000, default ``1001``; ``SEED`` and the
grammar rules are those of ``RHOGATE``. It works under **every**
``RHOFORM`` and needs a bare DM_REAM many-body term.

For every lattice constant :math:`a_k = a_0\,(f_{lo} + k\,(f_{hi} -
f_{lo})/(\mathrm{NPTS}-1))` (the last point exactly :math:`a_0 f_{hi}`) the
gate sums the density an atom of the perfect monatomic crystal receives,

.. math::

   \bar\rho(a_k) = \sum_s m_s\,\rho_{raw}(d_s\,a_k),

over the neighbour shells :math:`s` (distance :math:`d_s a`, multiplicity
:math:`m_s`) inside the candidate's ``RCUTMB``. The candidate **fails**
when :math:`\bar\rho` does not decrease strictly on the whole grid, i.e.
when :math:`\bar\rho(a_{k+1}) \ge \bar\rho(a_k)` for some :math:`k`, with
the measure

.. math::

   v = \min\!\Big(10^8,\ \frac{\sum_k \max\big(0,\ \bar\rho(a_{k+1}) - \bar\rho(a_k)\big)}{\max_k |\bar\rho(a_k)|}\Big),

the summed rise relative to the largest density. A flat step (a rise of
exactly 0) fails with :math:`v = 0`, at the band-D floor. The measure is
saturated when :math:`\bar\rho = 0` on the whole grid, a value is not
finite, or a pair weight is :math:`\le 0`. Under ``RHOFORM MONOTONE``
:math:`\bar\rho(a)` is non-increasing by construction, but the gate asks
for a strict decrease: a flat stretch still fails (under ``Cut_Dummy``,
every shell on the plateau below the innermost bump, where :math:`\rho` is
constant), and so does a rise at the rounding level. At startup the
first shell at :math:`a = f_{hi} a_0` must lie inside the smallest cutoff a
candidate can have (``RHOBARGATE: the first bcc shell at a = f_hi a0 = ...
A lies at r = ... A, beyond the smallest cutoff a candidate can have ...``);
the shells are enumerated once, for the largest cutoff a candidate can
reach. The reject line is ``eval N: lattice-rhobar gate reject (<K> rises
over a/a0 [<f_lo>, <f_hi>] on <lattice>, first at a/a0 = <f>, v = <v>);
total rejects=K`` (the first rising step, not the worst). The scan costs
about 0.3 ms per candidate at 1001 points (0.9 ms at 3501).

*Example.* The potential of the ``RHOGATE`` example, searched over two
amplitudes with both density gates (bcc Ta, :math:`a_0 = 3.304` Å):

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   OPTIM AMPGRIDMB (2) 0.0 2.0
   OPTIM AMPGRIDMB (3) -0.5 0.5
   RHOGATE 1e-5
   RHOBARGATE bcc 3.304 0.98 1.10

The startup line reads ``RHOBARGATE: lattice rhobar(a) gate, bcc (a0 3.304
A, a/a0 [0.98, 1.1], 1001 points), 4 shells within the largest reachable
RCUTMB, SEED ERROR; armed per candidate on AMPGRIDMB[2], AMPGRIDMB[3]: ...;
seeds: -c pot (INIT CONFIG) PASS``. In a 120-candidate ``TADAH:RANDOM``
search of this box, 57 candidates failed the sign gate and 29 more only the
lattice gate, for instance::

   INFO  eval 21: lattice-rhobar gate reject (577 rises over a/a0 [0.98, 1.1] on bcc,
         first at a/a0 = 1.0255, v = 3.3868e-02); total rejects=4

Combined grading and attribution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``BUMPGATE`` is checked first, on the raw parameters; ``RHOGATE`` and
``RHOBARGATE`` right after the candidate's configuration is built (still
before training and before any MPI traffic). When any configured gate fails,
every configured gate is evaluated and the candidate is graded by the sum
of the failing gates' measures, ``v = min(DBL_MAX, sum of their v)``,
so a move that shrinks any one violation is downhill even while another
gate still fails (a saturated measure stays saturated). The reject is
counted, and journalled, for the **first** failing gate in the order
``BUMPGATE``, ``RHOGATE``, ``RHOBARGATE``; its line names the others. From
the example above::

   INFO  eval 2: rho-sign gate reject (rho_min/rho_max = -2.3527e-01 at r = 4.4068 A, tol 1.0000e-05,
         v = 2.6067e-01); also failing: lattice-rhobar gate (v = 7.2300e-01); graded by the summed
         v = 9.8367e-01; total rejects=1

A lone failure keeps its own line, so without ``RHOGATE`` and ``RHOBARGATE``
every evaluation, ``BUMPGATE`` line and count is exactly as before.

**Arming and seeds** (``RHOGATE``, ``RHOBARGATE``). A density gate is
*armed*, i.e. evaluated for every candidate (its density pair weights
included), by an ``OPTIM`` line (in any case, anywhere in the file) on
``AMPGRIDMB``, ``CGRIDMB``, ``SGRIDMB`` or ``RCUTMB``, the keys that change
:math:`\rho(r)` or its window. A gate without one cannot act: every
candidate would have the ``-c`` density and its one verdict (an ``OPTIM
WATOM`` only scales the density, and the verdicts are scale-invariant), so
it is an error at the gate's line naming the keys that would arm it
(``Error on line 12: RHOGATE cannot act: no OPTIM line searches a key that
changes the DM_REAM density or its window (AMPGRIDMB, CGRIDMB, SGRIDMB or
RCUTMB), ...``). Before 1.6.0 such a gate was accepted with an INFO line
("not armed per candidate") and the ``-c`` verdict reused. A pair weight no
candidate can make positive (``WATOM`` fixed at :math:`\le 0`, or an
``OPTIM WATOM`` box that does not reach above 0) is a startup error under
either ``SEED`` mode, since every evaluation would be rejected. At startup
every *seed* is evaluated as a candidate: the ``-c`` potential and every
``CONFIG`` or ``WARM`` point of the resolved ``INIT`` chains (``OUTER``,
``PRESTAGE`` and ``PRESTAGE.INNER``) and of the ``RESTART`` block's
``METHOD`` chain (a restart round starts from them; ``RESTART WARM
<file>`` in the messages). A
failing seed that the optimiser may start from stops
the run under ``SEED ERROR`` (one message listing every failing seed with
its origin, the extremes, the window and ``v``, or for ``RHOBARGATE`` the
rises, the first :math:`a/a_0` and ``v``) and is a WARNING under ``SEED
WARN`` (``SEED WARN: the run proceeds; if the optimiser starts from this
seed, that evaluation is rejected before training (band D).``). A failing
``-c`` potential that is not an ``INIT`` start (a ``WARM``- or ``LHS``-only
chain) gets a WARNING. ``RANDOM``, ``LHS`` and ``SOBOL`` points are ordinary
candidates. Each configured gate logs one startup ``INFO`` line: its
settings, the dimensions that arm it and every seed's verdict.

.. _hpo_density_report:

The density report
^^^^^^^^^^^^^^^^^^

Every run whose many-body term is a bare DM_REAM, gated or not, reports the
density of the ``-c`` potential in the POTENTIAL SETUP section of
``hpo_run.log``, and of the best in RESULT. Example 27::

   Density form          : RHOFORM SIGNED -- rho = W f_c sum a_n psi_n, signed amplitudes (no sign guarantee)
   Density rho(r), -c pot: [0.8226, 5.4840] A, 20001 pts: min 0.0000e+00 (r 5.4840) max 5.5617e+00 (r 0.8226)
                           rel 0.0000e+00; max rho' +4.61e-03 (r 2.1430); non-increasing on window: no; W = 1

(one line in the file). The window is ``RHOGATE``'s (``R_LO``, ``NPTS``)
when it is configured, else :math:`[0.15\,r_c, r_c]` with 20001 points,
:math:`r_c` the ``RCUTMB`` of the potential. The row gives the extremes of
:math:`\rho_{raw}` and where they occur, ``rel`` = :math:`\rho_{min} /
\rho_{max}` (``n/a`` when :math:`\rho_{max} \le 0`), the largest
forward-difference slope ``max rho'`` and where, whether :math:`\rho_{raw}`
is non-increasing on the window, the pair weights (``W = <w>`` or
``W in [<lo>, <hi>]``; ``tadah hpo`` inserts ``WATOM`` = Z, the atomic
number, when the potential has none, so a potential without a ``WATOM``
line reports ``W = Z``) and, with ``RHOGATE``, its verdict.
*Non-increasing* means :math:`\max_i(\rho_{i+1} - \rho_i) \le
8\,\varepsilon \max_i \sum_n |a_n T_n(r_i) f_c(r_i)|`: the tolerance is
relative to the size of the terms, not to :math:`\rho`, because
``MONOTONE`` is non-increasing to rounding only. A ``SIGNED`` density often
reads ``no`` (example 27: ``max rho' +4.61e-03``), which is information, not
an error. With ``RHOBARGATE`` a ``Density rhobar`` row follows
(``strictly decreasing (0 rising steps)`` or ``rises at <K> of <NPTS-1>
steps, first at a/a0 = ... (v = ...): RHOBARGATE FAIL``). Each distinct
``WARM`` seed adds ``Density rho(r), WARM <k>`` rows, evaluated as the run
evaluates it (its ``OPTIM`` dimensions from the seed, the rest from
``-c``). RESULT prints the same rows for the best (``Density rho(r) : best
(eval N) ...``), or ``n/a (not derivable)`` when the density could not be
built at startup. A density that cannot be evaluated prints ``not derivable:
<reason>``; the report never stops a run.

.. _hpo_rhoform_positive:

Positive density forms: projection and non-negative boxes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A DM_REAM ``-c`` configuration must declare :ref:`RHOFORM
<dm_ream_rhoform>` (without it startup stops with ``tadah hpo: DM_REAM:
RHOFORM is required: ...``), and its density is validated for its form at
startup: a density DM_REAM would refuse (a negative amplitude under
``NONNEG``, a ``WATOM`` :math:`\le 0` under a positive form, ...) is a
startup error rather than a failure of every evaluation. Under ``NONNEG``
and ``MONOTONE`` every candidate must stay a valid density, so the search
space is restricted:

* ``OPTIM AMPGRIDMB ... AUTO`` (a linear rule; ``FRAC 0.5`` by default)
  clamps the box at 0, :math:`lo = \max(0, x_0 - d)`, and without ``FLOOR``
  or ``GROUP_FLOOR`` it takes an implicit ``GROUP_FLOOR 1e-3`` (the ``tadah
  refit`` writer's group floor), so a zero amplitude, which non-negative
  fits produce, gets the box :math:`[0, 10^{-3}\max|x_0|]` of its line.
  Such boxes carry the origin ``AUTO FRAC>=0`` in the SEARCH SPACE section,
  ``hpo_search_space.tsv`` and ``hpo_optim_resolved.hpo``. ``RATIO`` is
  allowed only when every seed on the line is :math:`> 0`, a negative seed
  is an error, and a line whose seeds are all 0 needs ``FLOOR <a>`` (the box
  :math:`[0, a]`) or explicit bounds. ``LOG`` needs :math:`lo > 0`, so a box
  clamped at 0 cannot be ``LOG``.
* An explicit ``AMPGRIDMB`` low bound below 0 is an error (0 is allowed), a
  ``WATOM`` low bound :math:`\le 0` is an error (explicit or ``AUTO``), and
  so is an ``SGRIDMB`` low bound :math:`\le 0` under ``MONOTONE`` and, for
  a Gaussian density (basis code 1), under ``NONNEG``: these are the width
  rules DM_REAM applies to the density (``OPTIM SGRIDMB (1) -0.5 2.0`` on a
  Gaussian ``NONNEG`` seed stops startup with ``... is not > 0, but RHOFORM
  NONNEG requires every SGRIDMB width > 0 for the Gaussian basis ...``). In a
  ``DM_mJoin`` with a Gaussian DM_REAM child the Gaussian rule holds every
  ``SGRIDMB`` entry (a box does not say whose entries it holds). A ``Blip``
  or ``Blip5`` density needs an ``SGRIDMB`` low bound :math:`> 0` in every
  form, as every radial bump width does (:ref:`search_space_constraints`).
* **Projection.** Each evaluation evaluates a copy of the candidate with
  every negative ``AMPGRIDMB`` coordinate set to 0 (a NaN is left for
  DM_REAM to refuse), before the gates and the model build, and that
  projected point is what is recorded (``params.tadah``, the potentials),
  so every recorded point is a valid potential; the optimiser is given the
  projection's loss for its own point. A search that keeps to the box has
  nothing projected: finite differences, with either step rule, are
  one-sided within a step of a bound and never evaluate outside the box
  (:ref:`hpo_fd_stencil`), so the derivative at a zero amplitude is the
  full one-sided slope (Ceres' step rule, the default under
  ``OPTIM_SCALE NONE``, stepped below 0 and was projected, which halved
  it). Only an optimiser that ignores the bounds still reaches below 0; the
  host counts that evaluation as one outside the box before projecting it.
  The first projection is an ``INFO`` line (``eval 8: RHOFORM NONNEG:
  AMPGRIDMB[3] = -5.0000e-10 below 0 is evaluated and recorded as 0: the
  candidate is projected onto AMPGRIDMB >= 0 (a point outside the OPTIM
  box, which only an optimiser that ignores the bounds evaluates);
  DIAGNOSTICS counts every projection``), and ``DIAGNOSTICS`` prints
  ``Positive-form projection: N evaluation(s) with an AMPGRIDMB amplitude <
  0, ...`` when there was one. A phase whose optimiser ignores the bounds
  gets one startup WARNING when ``AMPGRIDMB`` is ``OPTIM``'d, naming each
  such phase, as the other startup checks do (``CERES``
  ``LBFGS``/``WOLFE``/``NLCG``/``STEEPEST_DESCENT``, ``NLOPT LN_NEWUOA``:
  ``RHOFORM NONNEG: the PRESTAGE refinement CERES:LBFGS (ignores the OPTIM
  bounds) can evaluate AMPGRIDMB amplitudes outside the non-negative OPTIM
  box ...``). It also named a phase that took ``NUMERICAL`` gradients with
  Ceres' finite-difference kernel; that kernel keeps to the box now. A zero
  amplitude seed sits on its box's lower bound, and ``PRAXIS`` and
  ``BOBYQA`` may move a start that lies on a bound (see
  :ref:`search_space_scaling`). Under ``SIGNED`` nothing is projected.
* **Hard lower bounds.** An ``AMPGRIDMB`` lower bound of exactly 0 is
  *hard*: when the best rides it, the bound-occupancy audit shows ``LO*``
  instead of ``LO``, with the note ``(* a hard lower bound: RHOFORM NONNEG/MONOTONE require AMPGRIDMB
  >= 0, so an amplitude at 0 is an admissible optimum, not bound
  pressure)``; the advice to widen that side (and its HINT) is printed only
  when another dimension rides a bound. An ``AUTO`` box whose lower bound is
  above 0 can still be widened down to 0 and keeps the normal advice.

WARM seeds and structure keys
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A ``WARM`` seed whose ``RHOFORM`` is absent, invalid or different from the
run's (both being DM_REAM potentials) is an **error** when an ``OPTIM`` key
is ``AMPGRIDMB``, ``CGRIDMB`` or ``SGRIDMB``: density amplitudes mean
something only in the form they were fitted in. A potential written before
tadah 1.6.0 is ``SIGNED``: add ``RHOFORM SIGNED`` to it for a ``SIGNED`` run; a
``NONNEG`` or ``MONOTONE`` run needs a seed fitted in its form. Otherwise
(no density key searched) it is a WARNING: the seed's density keys are not
read. The rule is independent of ``WARM_ON_MISMATCH``, and it applies to the
``WARM`` seeds of the ``INIT`` chains and of an active ``RESTART`` block's
``METHOD`` chain at startup (:ref:`restart_decorator`), and again at each
restart.

``OPTIM`` on structure data is an error at its line, case-insensitively:
``RHOFORM``, ``TYPE2B``, ``TYPEMB``, ``RCTYPE2B``, ``RCTYPEMB``,
``KDEG2B``, ``KDEGMB``, ``KDEGEMB``, ``DIMER``, ``FIXINDEX``, ``MODEL``
and the first ``OALGO`` value (``OPTIM <KEY> is not a valid search
target: ..., structure data rather than a continuous parameter; set it in
the -c potential/configuration.``); ``OPTIM EMBF0`` has its own message.
So is ``OPTIM`` on an integer value (``SBASIS``) or an internal key;
``OPTIM MPARAMS`` on a value the model's function does not read as a
continuous one, and ``OPTIM RCUT2B``/``RCUTMB`` on the setfl cutoff of a
``D2_EAM``/``DM_EAM``, are errors at startup (see *Special cases* above).

.. _hpo_optimizer_common:

OPTIMIZER block — common attributes
-----------------------------------

The driver-attribute keys below are accepted wherever an optimiser is
being configured: directly inside ``OPTIMIZER`` (short form), at the top
level of ``STRATEGY`` (for ``TYPE PRESTAGE`` this is the refinement
optimiser), and inside ``EXPLORE``, ``REFINE`` and ``INNER`` sub-blocks
(``INNER`` takes every key but ``SEED`` and ``GRAD_STRATEGY``,
:ref:`hpo_inner_block`).
What the file alone settles is checked at startup, before any data is
loaded or any evaluation runs: each attribute's grammar and range, a
missing ``LIB`` or ``ALGO``, an unknown optimiser, whether the optimiser
uses ``STEP``, the ``INNER`` and ``STRATEGY`` rules, the ``PC_LAMMPS`` /
``PC_OBSERVABLE`` gradient rules, each backend's own checks of its
attributes and ``PARAM`` keys, and whether a backend that never ends on
its own has a stop (:ref:`hpo_stop_criteria`).  Every backend but NLopt
rejects a ``PARAM`` key it does not read, and ``TADAH:RANDOM``,
``TADAH:GRID`` and ``DLIB:GFS`` name the ones they take
(``TADAH:RANDOM: unknown PARAM 'FOO' (it takes none)``, ``TADAH:GRID:
unknown PARAM 'FOO' (it takes GRID_POINTS)``; before 1.6.0 ``RANDOM``
and ``GFS`` accepted any ``PARAM`` and ignored it).  NLopt passes every
``PARAM`` to NLopt, where an algorithm reads only the names it knows.
The backend checks run on the
settings each phase runs with — the ``STRATEGY``-level optimiser, each
``MULTISTART`` start (with its ``BUDGET_PER_START`` or its share of
``MAXEVAL``), the ``PRESTAGE`` exploration and refinement, and every
``INNER`` block — and the message names the phase and the key:

.. code-block:: none

   HPOEngine error: the PRESTAGE refinement: DLIB:BFGS does not support MAXTIME

Before 1.6.0 these backend checks ran when that optimiser started: after
the data were loaded and any ``NOISE CALIBRATION`` evaluations, for the
``PRESTAGE`` refinement only after the whole exploration, and under
``MULTISTART`` each start failed with a warning, so the run ended without
an evaluation.  Each optimiser still applies them when it starts.  Every
numeric attribute takes exactly one value (``STEP``'s
per-key form and ``PARAM`` take a name first): an *integer* one decimal
digits only (``SEED`` up to 2\ :sup:`64` − 1, ``POPULATION`` and
``VECTOR_ARR`` up to 2\ :sup:`32` − 1, ``MAXEVAL`` from 1 to
2\ :sup:`31` − 1, ``THREADS`` from 1 to 2\ :sup:`31` − 1), a *float*
one a finite number (``MAXTIME`` a positive one).

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Key (type)
     - Description
   * - ``LIB`` (string)
     - Optimisation library: ``TADAH``, ``ASA``, ``NLOPT``, ``DLIB``,
       ``CERES``.  Required, with ``ALGO``, for the ``STRATEGY``-level
       optimiser (under ``TYPE PRESTAGE`` the refinement, written at
       ``STRATEGY`` level or in a ``REFINE`` block) and for every
       ``EXPLORE`` and ``INNER`` block; an ``INNER`` block's ``LIB`` is
       ``NLOPT`` (:ref:`hpo_inner_block`).  A missing ``LIB`` or ``ALGO`` is
       an error naming the block, reported at the line that closes it
       (``ENDOPTIMIZER``, ``ENDEXPLORE``, ``ENDINNER``), e.g. ``Error on
       line 8: the OPTIMIZER block names no optimiser: LIB and ALGO are not
       set``.
   * - ``ALGO`` (string)
     - Algorithm (see table below).  ``NLOPT:LN_NEWUOA_BOUND`` and
       ``NLOPT:NLOPT_LD_LBFGS_NOCEDAL`` are refused, at the same lines,
       naming the phase and what to use instead (the ``NLOPT`` row of the
       table below).
   * - ``MAXEVAL`` (integer, 1 to 2\ :sup:`31` − 1)
     - Maximum number of evaluations: an integer from 1 to 2147483647,
       the ``int`` NLopt counts evaluations in.  ``0`` or a larger value
       is an error at its line (``MAXEVAL must be an integer from 1 to
       2147483647, got '0'``).  Before 1.6.0 a larger value wrapped in
       NLopt, so ``MAXEVAL 4294967299`` ran 3 evaluations, and
       ``MAXEVAL 0`` meant something different to every backend (no
       limit to NLopt, no evaluation to ``TADAH:RANDOM``, the annealers
       and ASA, the first starting point to ``DLIB:GFS``, an error when
       ``DLIB:BFGS`` and ``CERES`` started, nothing to ``TADAH:GRID``).

       ``MAXEVAL`` bounds one optimiser search, and every evaluation the
       search makes counts against it: each loss and, for a
       gradient-based optimiser, each finite-difference point of its
       gradients and each analytical gradient (``ANALYTICAL``,
       ``HYBRID``), one evaluation each.  A search is the run of a
       ``PLAIN`` strategy, or one ``RESTART`` launch (each has its own
       ``MAXEVAL``, :ref:`restart_decorator`), one ``MULTISTART`` start
       (its ``BUDGET_PER_START`` or its share of ``MAXEVAL``, ceil
       division), the ``PRESTAGE`` exploration (its ``EXPLORE`` block's
       ``MAXEVAL``) or one ``PRESTAGE`` refinement (its share), so a run
       can make more than ``MAXEVAL`` evaluations.  Evaluations outside
       a search are charged to no ``MAXEVAL`` and add to the run's total
       too: the ``NOISE`` block's ``CALIBRATION`` repeats
       (:ref:`hpo_noise_block`) and the ``INIT_JITTER AUTO`` calibration
       of ``TADAH:ANNEAL_PT`` (:ref:`tadah_anneal_pt_block`).  An NLopt
       search that its budget ends has made exactly ``MAXEVAL``
       evaluations and reports ``MAXEVAL_REACHED`` (``STOPVAL_REACHED``
       when the budget cut short the gradient of a loss at or below
       ``STOPVAL``, :ref:`hpo_gradient_not_finite`); the journal says
       ``NLopt finished: MAXEVAL_REACHED -- maximum evaluations exhausted
       after 60 of MAXEVAL 60 evaluation(s)``.  Before 1.6.0 NLopt counted its
       objective calls, each a loss and a whole gradient, while the
       gradients' evaluations had a budget of their own: an NLopt
       gradient search made up to about twice ``MAXEVAL`` evaluations
       (``LD_MMA``: 120 for ``MAXEVAL 60``), was given ``+inf`` for the
       points it evaluated after that budget, and could end as if it had
       converged (``STOPPED -- nlopt failure (algorithm converged …)``)
       or with ``SUCCESS``; ``GN_CRS2_LM``, ``GN_ORIG_DIRECT``,
       ``GN_ORIG_DIRECT_L`` and ``AUGLAG`` made a few evaluations more
       than ``MAXEVAL``.  The same ``MAXEVAL`` now gives an NLopt
       gradient search fewer steps (an ``ANALYTICAL`` one about half the
       losses); ``DLIB:BFGS`` and ``CERES`` counted this way already.  An
       ``INNER`` block's ``MAXEVAL`` counts NLopt calls
       (:ref:`hpo_inner_block`).

       Leave the key out for no evaluation limit.  The run then ends on
       the optimiser's other stops (``MAXTIME``, ``STOPVAL``, a
       tolerance), or on its own: NLopt's local algorithms, ``DLIB:BFGS``
       and ``CERES`` when they converge, ``TADAH:ANNEAL`` and
       ``ANNEAL_PA`` at the end of their temperature schedule, ASA on its
       own ``Limit_Generated`` / ``Limit_Acceptances`` and plateau tests,
       ``TADAH:GRID`` after its whole grid.  A backend with no convergence
       test and no finite schedule — ``TADAH:RANDOM``, ``DLIB:GFS``,
       ``ANNEAL_PT``, NLopt's global searches and three of its gradient
       algorithms — needs a stop: ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``,
       a tolerance it honours set above 0, or an active ``RESTART``
       (``GD_STOGO`` and ``GD_STOGO_RAND``: ``MAXEVAL``), a
       startup error otherwise; the table of :ref:`hpo_stop_criteria` says
       which backend needs one and which stops it honours.  ``TADAH:ANNEAL`` needs ``MAXEVAL`` itself for
       its default ``CHAIN_LENGTH AUTO`` / ``NT AUTO`` (set them to run
       without it):

       .. code-block:: none

          HPOEngine error: the STRATEGY-level optimiser: NLOPT:GN_DIRECT needs a stop: NLopt's global search has no convergence test of its own and runs until it is interrupted; set MAXEVAL, MAXTIME, STOPVAL or a positive tolerance it honours (FTOL_REL, FTOL_ABS, XTOL_REL, XTOL_ABS); DIRECT's XTOL_REL and XTOL_ABS end it only once every rectangle is that small, after roughly (1/XTOL_REL)^n evaluations in n dimensions

       ``TADAH:GRID`` stops after ``MAXEVAL`` points in the order it
       visits them (the ``INIT`` point, then the grid nodes, the first
       dimension of the :ref:`dimension order <hpo_dimension_order>`
       varying fastest), and the journal says so
       (``GRID: MAXEVAL 5 < 28 grid points (the INIT point, then 27 = 3^3
       grid nodes); evaluating the first 5 in grid order``); before 1.6.0
       it ignored ``MAXEVAL`` and evaluated its whole grid.
   * - ``MAXTIME`` (float > 0)
     - Whole-optimiser wall-time in seconds for this active driver, a
       positive number; leave the key out for no limit.  0 or a negative
       value is an error at its line (``MAXTIME must be a positive number
       of seconds, got '-5'``); before 1.6.0 it meant no limit to NLopt and
       ``TADAH`` and was rejected at startup by ASA and the annealers.
       This is distinct from root-level ``PC_SCRIPT_TIMEOUT`` (whose alias
       is root-level ``MAXTIME``, a LAMMPS time spec).  No evaluation
       starts once that many seconds have passed since the optimiser
       started; the evaluations already running finish.
       ``DLIB:BFGS`` and ``CERES`` reject it.  ``TADAH:RANDOM``,
       ``TADAH:GRID`` and ``DLIB:GFS`` accepted it and ignored it before
       1.6.0.
   * - ``STOPVAL`` (float)
     - Stop at the first loss that reaches this value.  ``TADAH:RANDOM``,
       ``TADAH:GRID``, ``DLIB:GFS`` and the ``TADAH:ANNEAL`` family stop
       at the first feasible loss at or below it (a ``FAILSCORE`` loss,
       graded or not, never reaches it).  NLopt and ASA apply their own
       test (NLopt: below it, or at or below it for some algorithms; ASA:
       at or below it) to every loss, a failed evaluation's included, so
       keep ``STOPVAL`` below ``FAILSCORE`` with them.  ``DLIB:BFGS`` and
       ``CERES`` reject it.  ``TADAH:RANDOM``, ``TADAH:GRID`` and ``DLIB:GFS``
       accepted it and ignored it before 1.6.0; with ``THREADS`` > 1 the
       ``DLIB:GFS`` evaluations already running when one reaches it
       finish.  ``STOPVAL`` inside an ``INNER`` block is an error at its
       line: NLopt replaces it with the enclosing block's
       (:ref:`hpo_inner_block`).
   * - ``FTOL_REL`` (float)
     - Relative tolerance on the loss.  A tolerance of 0 or below switches
       the test off (NLopt; the annealers' plateau rule arms only above
       0), so it is no stop (:ref:`hpo_stop_criteria`).  A backend whose
       run never tests a tolerance refuses it at startup, naming the stops
       it honours; the table of :ref:`hpo_stop_criteria` lists which
       tolerances each one tests.
   * - ``FTOL_ABS`` (float)
     - Absolute tolerance on the loss (0 or below: off, as ``FTOL_REL``).
   * - ``XTOL_REL`` (float)
     - Relative tolerance on hyperparameters (0 or below: off).
   * - ``XTOL_ABS`` (float)
     - Absolute tolerance on hyperparameters (0 or below: off).
   * - ``POPULATION`` (integer)
     - Population size of the algorithms that have one: ``TADAH:ANNEAL_PA``
       (its walkers) and NLopt's ``GN_CRS2_LM``, ``GN_ISRES``,
       ``GN_ESCH``, ``GD_STOGO_RAND`` and the six MLSL forms.  Every other
       backend refuses it at startup (``NLOPT:LN_BOBYQA does not use
       POPULATION; remove it (it is read by NLOPT:GD_MLSL, …,
       TADAH:ANNEAL_PA)``); before 1.6.0 ``TADAH:RANDOM``, ``TADAH:GRID``,
       ``DLIB:GFS`` and NLopt's other algorithms accepted it and ignored
       it.
   * - ``STEP`` (float)
     - Initial step for the NLopt derivative-free algorithms, as a
       **fraction of each OPTIM dimension's (transformed) width**,
       ``0 < STEP <= 0.5`` (the same meaning as the annealers'
       ``PARAM STEP_FRAC``).  NLopt receives one step per dimension:
       SBPLX/NELDERMEAD use them as the first simplex's edges,
       BOBYQA/COBYLA as their per-dimension coordinate rescaling,
       NEWUOA/PRAXIS collapse them to one radius, MLSL/AUGLAG forward
       them to ``INNER`` (an outer ``STEP`` overrides an ``INNER`` one).
       Per-key/per-index overrides: ``STEP <KEY>[(indices)] <fraction>``
       (repeatable); the index list has the grammar of the ``OPTIM``
       lines (``1-4,7``, indices from 1).  Every fraction must be a
       finite number, and a malformed value or index list is an error
       naming the line (``STEP must be a finite number, got 'inf'``,
       ``STEP override 'CGRID2B(3x)': bad index list (…)``); before 1.6.0
       ``STEP inf`` failed only when the run started, and an index that
       did not parse was dropped, so ``CGRID2B(3x)`` set every index.  Near a bound the step is adjusted per dimension
       (BOBYQA: shrunk to 0.75 of the slack so the warm start is not
       relocated; SBPLX: pointed inward).  Unset: NLopt's own
       per-dimension default (25 % of the width, or 75 % of the distance
       to a nearer bound).  Every other backend rejects ``STEP`` at
       startup with a hint (annealing: ``PARAM STEP_FRAC``; GRID:
       ``PARAM GRID_POINTS``; GFS/RANDOM/BFGS/CERES/ASA have no initial
       step).  The effective per-dimension step and the algorithm's
       consumption mode are printed in ``hpo_run.log``.
   * - ``SEED`` (integer ≥ 0)
     - Random seed of stochastic optimisers (default: drawn from
       ``std::random_device``, so a run without ``SEED`` is not
       reproducible).  ``DLIB:BFGS``, ``CERES`` and ``TADAH:GRID``, which
       draw no random numbers, reject it (``CERES`` and ``GRID`` accepted
       it and ignored it before 1.6.0); ``DLIB:GFS`` is deterministic and
       ignores it with a warning.  Every NLopt algorithm takes it, whether
       or not it draws random numbers.  ``MULTISTART`` and
       the ``PRESTAGE`` refinement run start *i* with ``SEED + i``, and
       each ``RESTART`` round runs with a seed derived from it
       (:ref:`restart_decorator`).  Inside an ``INIT`` block the same key
       seeds the stochastic init methods instead; of the optimisers, only
       NLopt reads it, and only when its block has no ``SEED`` of its
       own.  Inside a ``RESTART`` block it seeds the ``METHOD`` chain
       that draws each restart's points (restart *r* with ``SEED + r``).
       An ``INNER`` block takes no ``SEED`` (startup error): NLopt
       has one random stream per run, seeded by the enclosing block, and
       MLSL/AUGLAG draw their ``INNER`` search from it
       (:ref:`hpo_inner_block`).
   * - ``VECTOR_ARR`` (integer)
     - Number of stored vectors of NLopt's limited-memory algorithms
       ``LD_LBFGS``, ``LD_VAR1``, ``LD_VAR2`` and the four
       ``LD_TNEWTON`` forms.  Every other backend refuses it at startup
       (``… does not use VECTOR_ARR; remove it (it is read by …)``);
       before 1.6.0 ``TADAH:RANDOM``, ``TADAH:GRID``, ``DLIB:GFS`` and
       NLopt's other algorithms accepted it and ignored it.
   * - ``THREADS`` (integer ≥ 1)
     - Width of the shared per-rank worker pool that runs the
       ``PC_LAMMPS`` fan-out and MLIP train/predict for every backend
       (default ``1``; ``ANNEAL_PT``/``ANNEAL_PA`` also cap concurrent
       replicas by it).  The value of the ``STRATEGY``-level optimiser
       sizes the pool: at the ``OPTIMIZER`` / ``STRATEGY`` level, or in a
       ``REFINE`` block, which configures the same optimiser.
       ``THREADS`` inside ``EXPLORE`` or ``INNER`` does not resize it.  ``THREADS 0`` is an
       error at its line in every block (``THREADS must be an integer
       from 1 to 2147483647, got '0'``); before 1.6.0 it was accepted,
       and the pool quietly ran one worker.  Each run builds its pool
       and stops it when it ends, so every run of a process has a pool
       of its own ``THREADS`` (see *Two runs in one process*).
   * - ``GRAD_STRATEGY`` (string)
     - How gradient-based backends obtain gradients: ``NUMERICAL``
       (default; finite differences, see ``PARAM fd_step`` /
       ``fd_kernel`` / ``diff_method`` and :ref:`hpo_fd_stencil`),
       ``ANALYTICAL`` or ``HYBRID`` (analytical where a derivative is
       registered, finite differences elsewhere — for every parameter
       when no analytical derivative can be formed at the point; before
       1.6.0 it returned a zero gradient there).  An analytical gradient
       counts as one evaluation toward ``MAXEVAL``, with every backend
       (NLopt counted only its calls before 1.6.0, so an NLopt
       ``ANALYTICAL`` search made up to ``MAXEVAL`` losses and as many
       gradients).  A
       failed loss at the point leaves it without a usable gradient
       under every strategy, but for a graded failure at the point under
       ``FAILSCORE_GRADING ON``, and so does a failed evaluation, graded
       or not, in the finite-difference stencil of a usable point
       (``NUMERICAL``, the differences of ``HYBRID``); such a point can
       end the search, with its own status
       (:ref:`hpo_gradient_not_finite`).  The
       analytical modes are experimental.  Not accepted inside an
       ``INNER`` block (a startup error at its line): ``MLSL`` and
       ``AUGLAG`` hand their ``INNER`` search the gradient of their own
       objective, which the enclosing block's ``GRAD_STRATEGY`` obtains
       (:ref:`hpo_inner_block`).  A phase whose optimiser takes no
       gradient refuses it at startup, as the annealers and ASA did
       (``the STRATEGY-level optimiser NLOPT:LN_SBPLX: GRAD_STRATEGY never
       takes effect: NLOPT:LN_SBPLX takes no gradient; remove it.``);
       before 1.6.0 a derivative-free NLopt algorithm, ``TADAH:RANDOM``,
       ``TADAH:GRID`` and ``DLIB:GFS`` accepted it and ignored it.  An
       MLSL or AUGLAG takes it when its local search is gradient-based —
       a gradient-based ``INNER`` block, or without one the ``LD_MMA``
       that NLopt runs for the ``GD_`` and ``LD_`` forms — and refuses it
       otherwise, whatever the ``D`` or ``N`` of its name: MLSL and AUGLAG
       evaluate their own points without a gradient, and an ``INNER``
       block replaces NLopt's default local optimiser
       (``… NLOPT:GD_MLSL_LDS takes no gradient: NLopt's MLSL and AUGLAG
       take a gradient only for their local search, and the INNER block's
       NLOPT:LN_BOBYQA takes none; remove it.``).  Before 1.6.0
       ``GD_MLSL``, ``GD_MLSL_LDS``, ``LD_AUGLAG`` and ``LD_AUGLAG_EQ``
       with a derivative-free ``INNER`` block accepted it and ignored it,
       as they did ``PARAM fd_step``, ``fd_kernel`` and ``diff_method``
       and ``NOISE`` ``AUTO``.
       Rejected when the configuration has ``PC_LAMMPS`` lines.  With
       ``PC_OBSERVABLE`` ``NUMERICAL`` is
       rejected in every block, ``EXPLORE`` included, and an unset
       value resolves to ``ANALYTICAL`` in each block that runs an
       optimiser and obtains gradients: the ``STRATEGY``-level block and
       ``EXPLORE`` (:ref:`pc_observable`).
       ``ANALYTICAL`` (written out or resolved) differentiates the
       least-squares fit through its Cholesky factor, so it needs
       training data with every weight fitted (no ``FIXINDEX``),
       ``OALGO 4`` with a fixed ``LAMBDA`` ≥ 0 (not ``-1``),
       ``NORM false``, a linear basis (``MODEL … BF_Linear`` or
       ``Kern_Linear``), and ``STRESS false`` and no ``PC_SRMSE``
       (``PC_FRMSE`` is covered with ``FORCE`` true or false; before
       1.6.0 it needed ``FORCE true``); it also needs loss terms it can
       differentiate: no ``PC_OBSERVABLE`` ``--loss MAPE`` or
       ``RMSLE``, and no ``--unconverged force-scaled``.  Startup
       refuses a block that sets it without them, naming the setting or
       the line.  It still stops the run at a gradient it cannot form —
       a parameter without an analytical derivative, a candidate
       rejected before training, an ill-conditioned fit — where
       ``HYBRID`` uses finite
       differences instead.  Before 1.6.0 such a setup passed startup
       and stopped at the first gradient with ``no registered
       analytical derivative``.
   * - ``PARAM`` (string value)
     - Algorithm-specific key/value (e.g. ``PARAM fd_step 1e-3``).  A
       value that starts like a number must be a finite number
       (``PARAM fd_step inf`` and ``PARAM fd_step 5x`` are errors naming
       the line); a word (``AUTO``, ``inhouse``) is passed to the
       backend as it is.  Keys
       understood by the finite-difference gradient of every
       gradient-based backend (``GRAD_STRATEGY NUMERICAL`` and
       ``HYBRID``; :ref:`hpo_fd_stencil`): ``fd_step`` (the step,
       default ``1e-6``), ``fd_kernel`` (the step rule: ``inhouse``,
       ``fd_step * max(|u|, 1)`` in the internal coordinate ``u``, the
       default under ``OPTIM_SCALE AUTO`` and for ``HYBRID``, or
       ``ceres``, Ceres' rule ``max(2^-26, fd_step * |u|)``, evaluated
       by Tadah, the default of ``NUMERICAL`` under ``OPTIM_SCALE
       NONE``) and ``diff_method`` (``0`` CENTRAL, the default, or
       ``1`` FORWARD; ``2``, RIDDERS, is refused).  With either rule no
       finite-difference point leaves the ``OPTIM`` box: next to a bound
       the stencil turns one-sided into it.  They are checked at startup
       on each phase that takes finite differences, and refused where
       they never take effect (an ``INNER`` block, ``GRAD_STRATEGY
       ANALYTICAL``, a derivative-free optimiser).  ``fd_kernel``
       overrides the default under either ``OPTIM_SCALE``; before 1.6.0
       ``inhouse`` was ignored under ``NONE``, and ``fd_kernel ceres``
       needed a build with ``TADAH_ENABLE_CERES``.

.. list-table:: Algorithms by library
   :header-rows: 1
   :widths: 12 88

   * - LIB
     - ALGO (algorithms from the selected library)
   * - ``TADAH``
     - ``RANDOM`` (uniform random search: the ``INIT`` point, then
       uniform draws until ``MAXEVAL``, ``MAXTIME`` or ``STOPVAL`` ends
       it — one of them is required; no ``PARAM``),
       ``GRID`` (the ``INIT`` point, then every node of a
       ``PARAM GRID_POINTS``\ :sup:`dim` grid, default 20 points per
       dimension, the first dimension of the :ref:`dimension order
       <hpo_dimension_order>` varying fastest; ``MAXEVAL``, ``MAXTIME``
       and ``STOPVAL`` stop the sweep earlier, still in grid order),
       ``ANNEAL`` (classic
       Kirkpatrick/Metropolis Simulated Annealing — see
       :ref:`tadah_anneal_block` below), ``ANNEAL_PT`` (parallel
       tempering, :ref:`tadah_anneal_pt_block`), ``ANNEAL_PA``
       (population annealing, :ref:`tadah_anneal_pa_block`).
   * - ``ASA``
     - ``ASA`` — Lester Ingber's Adaptive Simulated Annealing
       (ASA-30.53, vendored, BSD-3-Clause). Fat-tailed per-dimension
       generating distribution, sensitivity-driven reannealing,
       optional quenching and block-parallel evaluation. Documented on
       its own page: :ref:`asa_optimizer`.
   * - ``NLOPT``
     - The NLopt catalogue — globals (``GN_*``, ``G_MLSL*``, …) and
       locals (``LN_*``, ``LD_*``); see the `NLOpt algorithm catalogue
       <https://nlopt.readthedocs.io/en/latest/NLopt_Algorithms/>`_ for
       the full list.  Gradient-using algorithms (``GD_*`` / ``LD_*``)
       obtain their gradient from the shared finite-difference seam also
       used by ``DLIB:BFGS`` and the Ceres drivers; like every
       gradient-based backend except ``DLIB:BFGS`` they are rejected
       when the configuration has ``PC_LAMMPS`` lines.

       ``LN_NEWUOA_BOUND`` is refused in every phase (the
       ``STRATEGY``-level optimiser, ``MULTISTART``, the ``PRESTAGE``
       exploration and refinement, ``INNER``): at ``ENDOPTIMIZER`` for
       the ``STRATEGY``-level optimiser, whether the ``STRATEGY`` block,
       a ``REFINE`` block or the ``OPTIMIZER`` block itself names it, at
       ``ENDEXPLORE`` for the exploration and at ``ENDINNER`` for an
       ``INNER`` block.  NLopt 2.10 plans each of its steps with a
       nested ``LD_MMA`` run that has no time limit and sees no stop
       request, and once a non-finite value reaches its model each step
       takes about 10–60 s without evaluating the loss.  That happens
       after every stop ``tadah hpo`` makes through the objective
       (Ctrl-C, SIGTERM, a ``RESTART`` stagnation stop, an error that ends
       the run: the objective then returns ``HUGE_VAL``, NLopt's
       convention for a forced stop) from a run's (2n+1)-th evaluation
       on, n being the number of ``OPTIM`` dimensions (NEWUOA first
       evaluates 2n+1 starting points, and a stop on one of the first 2n
       returns at once), and when the loss barely changes over the trust
       region; Ctrl-C, ``MAXTIME`` and ``RESTART`` wait that long.
       Before 1.6.0 it was accepted.  Use ``LN_BOBYQA``, Powell's
       bound-constrained successor of NEWUOA, whose ``STEP``
       rescales each dimension instead of setting one radius
       (:ref:`search_space_scaling`), or ``LN_NEWUOA`` where the search
       may leave the ``OPTIM`` box (it ignores the bounds):

       .. code-block:: none

          HPOEngine error: Error on line 10: the STRATEGY-level optimiser: NLOPT:LN_NEWUOA_BOUND is refused: NLopt 2.10 plans each LN_NEWUOA_BOUND step with a nested LD_MMA run that has no time limit and sees no stop request, ... Use NLOPT:LN_BOBYQA, Powell's bound-constrained successor of NEWUOA (its STEP rescales each dimension instead of setting one radius), or NLOPT:LN_NEWUOA where the search may leave the OPTIM box
          Line: ENDOPTIMIZER

       ``NLOPT_LD_LBFGS_NOCEDAL`` (NLopt's name for it) is refused in
       every phase as well, at the same lines: NLopt 2.10 names it but
       does not implement it, and ``nlopt_optimize()`` returns
       ``NLOPT_INVALID_ARGS`` for it, so every run of it ended ``FAILED --
       nlopt invalid argument`` once its setup was done.  Before 1.6.0 it
       was accepted.  Use ``LD_LBFGS``, NLopt's L-BFGS:

       .. code-block:: none

          HPOEngine error: Error on line 10: the STRATEGY-level optimiser: NLOPT:NLOPT_LD_LBFGS_NOCEDAL is refused: NLopt 2.10 does not implement it (NLopt names it, but its optimiser rejects it as an invalid argument, so every run of it failed once its setup was done). Use NLOPT:LD_LBFGS, NLopt's L-BFGS
          Line: ENDOPTIMIZER

       .. note::

          With the default ``GRAD_STRATEGY NUMERICAL`` every
          gradient-using algorithm (``LD_*``, ``GD_*``, ``DLIB:BFGS``,
          all CERES drivers) obtains its gradient by finite differences
          (``PARAM fd_step``), and every evaluation counts toward
          ``MAXEVAL``: the loss and each difference point (an analytical
          gradient counts as one).  Before 1.6.0 NLopt counted only its
          calls, the losses (see ``MAXEVAL`` above).  A
          failed evaluation at the point or in the stencil fails the
          gradient, whatever ``FAILSCORE`` is (but for a graded failure
          at the point under ``FAILSCORE_GRADING ON``), and can end the
          search (:ref:`hpo_gradient_not_finite`).  Analytical gradients (``GRAD_STRATEGY
          ANALYTICAL``/``HYBRID``) are experimental; with
          ``PC_OBSERVABLE`` they are the default (:ref:`pc_observable`).
   * - ``DLIB``
     - ``BFGS`` — local, box-constrained quasi-Newton with a
       finite-difference gradient; the only gradient-based optimiser
       accepted with ``PC_LAMMPS``.  ``GFS`` — the dlib MaxLIPO+TR
       global *Global Function Search*; consumes every ``INIT`` point.
       See :ref:`dlib_param_keys`.
   * - ``CERES``
     - ``LBFGS`` (limited-memory BFGS, Wolfe line search), ``WOLFE``
       (full BFGS, Wolfe line search), ``NLCG`` (nonlinear conjugate
       gradient), ``STEEPEST_DESCENT`` (Armijo line search). All four
       use Ceres' ``GradientProblemSolver`` with a finite-difference
       gradient (``PARAM fd_step``, ``fd_kernel``, ``diff_method``;
       :ref:`hpo_fd_stencil`).  They need a build with ``TADAH_ENABLE_CERES``
       (the default) and are rejected with ``PC_LAMMPS``.  The Ceres NLLS
       family (``LEVENBERG_MARQUARDT``, ``DOGLEG``, ``SUBSPACE_DOGLEG``)
       is **deferred** — mathematically degenerate over a scalar HPO
       loss; it will be enabled in a later phase together with
       residual-mode loss exposure. Ceres optimisers do **not** enforce
       box constraints: their iterates can leave the ``OPTIM`` box and be
       evaluated there — warned about at startup and at the first such
       evaluation, and counted in ``DIAGNOSTICS``; such a point can become
       the best (:ref:`hpo_fd_stencil`).  Use ``DLIB:BFGS`` or
       ``NLOPT:LD_LBFGS`` for bounds-enforced gradient optimisation.

CERES PARAM keys
................

The four CERES algorithms accept these algorithm-specific ``PARAM``
entries (all keys are optional unless noted):

================== =================================================================================
Key                Effect
================== =================================================================================
``fd_step``        Finite-difference step (default ``1e-6``): ``fd_step * max(|u|, 1)`` in internal
                   coordinates ``u`` with ``fd_kernel inhouse`` (a fraction of the box width under
                   ``OPTIM_SCALE AUTO``), ``max(2^-26, fd_step * |u|)`` with ``ceres``.  Central, or
                   one-sided within a step of a bound, so that every evaluation stays in the
                   ``OPTIM`` box; a box too narrow for the step is refused at startup
                   (:ref:`hpo_fd_stencil`).
``fd_kernel``      The step rule: ``inhouse`` (default under ``OPTIM_SCALE AUTO``) or ``ceres``
                   (default under ``OPTIM_SCALE NONE``; Ceres' rule, evaluated by Tadah and kept in
                   the box); see the ``PARAM`` row of the attribute table.
``diff_method``    ``0`` = CENTRAL (default), ``1`` = FORWARD (backward at the upper bound; first
                   order, ``n`` evaluations per gradient after the loss at the point), with either
                   rule.  ``2`` (RIDDERS) is refused: it ignored ``fd_step`` and started outside
                   most boxes.
``tol_gradinf``    Convergence tolerance on the gradient's largest component, its infinity norm
                   (Ceres ``gradient_tolerance``, default ``1e-10``): ``CERES:<ALGO>: tol_gradinf
                   reached``.
``max_iter``       Override on Ceres' ``max_num_iterations``, an integer from ``0`` to ``2147483647``
                   (``0``: the start is evaluated and the solve ends) (``CERES:<ALGO>: max_iter
                   reached``).  By default the iteration cap is set to a large sentinel and
                   ``MAXEVAL`` is enforced externally on host evaluations.
``max_ls_iter``    Maximum line-search step-size iterations, an integer from ``1`` to ``2147483647``.
``max_lbfgs_rank`` (LBFGS only) limited-memory rank, an integer from ``1`` to ``2147483647``, default
                   ``20``.
``nlcg_type``      (NLCG only) variant: ``0`` = FLETCHER_REEVES (default), ``1`` = POLAK_RIBIERE,
                   ``2`` = HESTENES_STIEFEL.
================== =================================================================================

A value outside these ranges, a fraction included, is refused at
startup, naming it (``CERES:LBFGS requires PARAM max_ls_iter to be an
integer from 1 to 2147483647 (Ceres' max_num_line_search_step_size_iterations),
got 0``).  Before 1.6.0 a fraction was cut to an integer (``2.5`` ran as
``2``), and Ceres refused ``max_iter`` below ``0`` and ``max_ls_iter``
``0`` when the search started: the search ended at once, before any
evaluation.

The ``MAXEVAL`` cap is enforced at the host-evaluation level by an
external counter shared with the ``GradientProvider``.  Ceres'
internal iteration counting is intentionally given a high sentinel so
that one Ceres iteration's worth of finite-diff samples (2 × N under
CENTRAL) is correctly accounted for against the user budget.

Any other ``PARAM`` key is rejected.  CERES also rejects ``STEP``,
``MAXTIME``, ``STOPVAL``, ``FTOL_ABS``/``XTOL_ABS`` (use ``FTOL_REL`` →
``function_tolerance`` and ``XTOL_REL`` → ``parameter_tolerance``),
``POPULATION``, ``VECTOR_ARR``, ``SEED`` (accepted and ignored before
1.6.0), ``THREADS``, integer dimensions and ``INNER`` blocks, and
``FTOL_REL`` or ``XTOL_REL`` below ``0`` (``CERES:LBFGS requires
FTOL_REL >= 0 (Ceres' function_tolerance), got -1``), which Ceres
refused, before 1.6.0, when the search started.  Options that Ceres
refuses all the same are an internal error before any evaluation
(``internal error: CERES:<ALGO>: Ceres refuses the options built from a
validated spec: …``; please report it).

.. _dlib_param_keys:

DLIB:BFGS PARAM keys
....................

``DLIB:BFGS`` runs dlib's box-constrained BFGS with the same
finite-difference gradient seam as the CERES drivers; gradient
evaluations count toward ``MAXEVAL`` through a shared counter.

================== =================================================================================
Key                Effect
================== =================================================================================
``fd_step``        Finite-difference step (default ``1e-6``), as for CERES above.
``fd_kernel``      The step rule, ``inhouse`` or ``ceres``, as for CERES above.
``diff_method``    ``0`` = CENTRAL (default) or ``1`` = FORWARD, as for CERES above (refused
                   before 1.6.0).
``tol_gradinf``    Stop when the L2 norm of the gradient (dlib's ``length(g)``) falls below this
                   value (> 0): ``DLIB:BFGS: tol_gradinf reached``.  Mutually exclusive with
                   ``FTOL_ABS``; without either the run stops when the loss changes by less than
                   ``FTOL_ABS = 1e-7`` from one iteration to the next: ``DLIB:BFGS: FTOL_ABS
                   reached`` (:ref:`hpo_end_status`).  Before 1.6.0 this page and the ``ALGO``
                   help called it the gradient's infinity norm, which is not what dlib tests.
================== =================================================================================

Any other ``PARAM`` key is rejected, and so are ``FTOL_REL``, ``XTOL_REL``,
``XTOL_ABS``, ``SEED``, ``VECTOR_ARR``, ``STEP``, ``MAXTIME``,
``STOPVAL``, ``POPULATION``, integer dimensions and ``INNER`` blocks.
``MAXEVAL`` is enforced by HPO (dlib's own stop rules count iterations).

``DLIB:GFS`` (dlib's ``find_min_global``) takes ``MAXEVAL``, ``MAXTIME``
and ``STOPVAL`` — dlib's call budget, run-time limit and stop condition,
the ``INIT`` points' evaluations counted; the run-time limit is at most
dlib's own no-limit, 200 years (a ``MAXTIME`` beyond about 292 years, the
nanoseconds dlib counts it in, ended the search after its ``INIT``
points before 1.6.0) — and ``FTOL_ABS``, dlib's
solver epsilon (default ``1e-3``), which ends no run.  It takes no
``PARAM`` and rejects ``FTOL_REL``, ``XTOL_REL`` and ``XTOL_ABS``, which
it cannot apply (``DLIB:GFS does not support FTOL_REL: dlib's global
search has no convergence test to apply it to; stop it with MAXEVAL,
MAXTIME or STOPVAL``), and ``POPULATION`` and ``VECTOR_ARR``, which it
does not use.  It has no convergence test, so one of ``MAXEVAL``,
``MAXTIME`` or ``STOPVAL`` is required (``DLIB:GFS needs a stop: …``).
It is deterministic, so ``SEED`` has no effect (a warning says so when
the run starts).  Before 1.6.0 it read only ``MAXEVAL``, which it needed,
and ignored ``MAXTIME``, ``STOPVAL``, the other tolerances,
``POPULATION``, ``VECTOR_ARR`` and any ``PARAM``.

.. _hpo_dlib_gfs_failures:

A failed evaluation (one without a usable loss: ``FAILSCORE``, a graded
failure, a non-finite loss) is not handed to dlib as it is.  dlib is
given a finite stand-in, fixed when the evaluation returns:

* once the search has usable losses, the worst of them so far plus a
  margin, the larger of the usable losses' spread and the worst one's
  magnitude: twice the margin for an ungraded failure, and between one
  and two margins for a graded one, by its place in the graded-failure
  band;
* before the search's first usable loss, ``FAILSCORE`` capped at
  ``1e100``, and a graded failure at its place in the band below it —
  with a ``FAILSCORE`` up to ``1e100``, exactly the loss the evaluation
  reported.

"So far" counts the evaluations of this search only: each time a run
starts ``DLIB:GFS`` (again in a ``RESTART`` round, for instance), dlib
starts a new model and the stand-ins start afresh.  Graded failures keep
their order among the stand-ins handed over in the same state — all
before the first usable loss, or all while the same worst and best usable
losses hold — but not across states, since a stand-in is never revised:
a failure before the first usable loss keeps its value near
``FAILSCORE`` (or ``1e100``) for the rest of the search, far above a
later ungraded failure's, and an ungraded failure handed over early can rank below a
graded one handed over after a worse usable loss has raised the
stand-ins.
No value dlib sees exceeds ``1e100`` in magnitude.  ``loss.tadah``, the
journal and ``best_pot.tadah`` keep the loss the evaluation reported, and
the journal says once per search, at its first failed evaluation, that
the stand-in is in use (``DLIB:GFS: dlib is given a finite stand-in for
each failed evaluation of this search …``).  Before 1.6.0 dlib was given
``FAILSCORE``: with the default, the largest double, a run whose first
*dims* + 2 evaluations had all failed crashed (SIGSEGV) in dlib's
trust-region step, even when later evaluations would have produced
usable losses.

.. _hpo_fd_stencil:

Finite-difference gradients and the OPTIM box
.............................................

``GRAD_STRATEGY NUMERICAL`` (the default) takes the whole gradient by
finite differences; ``HYBRID`` takes the components without an analytical
derivative — every component when none can be formed at the point (a
setup the analytical gradient does not cover, a candidate rejected before
training, a failed ``PC_LAMMPS`` chain) — and ``ANALYTICAL`` takes none.
No finite-difference point is ever evaluated outside the ``OPTIM`` box,
with either step rule, in every mode; there is no opt-out.  Three
``PARAM`` keys of the block that takes the gradient set the differences:

.. list-table::
   :header-rows: 1
   :widths: 14 86

   * - Key
     - Effect
   * - ``fd_kernel``
     - The step rule along the internal coordinate :math:`u` the optimiser
       iterates on.  ``inhouse``: :math:`h = \mathrm{fd\_step}\cdot
       \max(|u|, 1)` — ``fd_step`` of the box width under
       ``OPTIM_SCALE AUTO``, where every box is the unit interval; under
       ``NONE`` absolute below :math:`|u| = 1` and relative above.
       ``ceres``: :math:`h = \max(2^{-26}, \mathrm{fd\_step}\cdot|u|)`,
       the rule of Ceres' numeric differentiation, which Tadah evaluates
       itself (no Ceres build needed); inside the box it gives Ceres'
       gradient bit for bit.  Default: ``inhouse`` under ``OPTIM_SCALE
       AUTO`` and for ``HYBRID``, ``ceres`` for ``NUMERICAL`` under
       ``OPTIM_SCALE NONE`` (its floor of :math:`2^{-26}` next to
       :math:`u = 0` makes it a poor choice for the unit box).
   * - ``fd_step``
     - The step, default ``1e-6``; a positive finite number.  The
       ``NOISE`` block's ``AUTO`` can derive it (:ref:`hpo_noise_block`).
   * - ``diff_method``
     - ``0``: ``CENTRAL`` (default); ``1``: ``FORWARD``, with either rule.
       ``2`` (``RIDDERS``) is refused: Ridders' extrapolation ignored
       ``fd_step``, started from a step of :math:`0.32\max(|u|, 1)` —
       outside most boxes — and took up to 20 evaluations per parameter.

They are validated at startup on every phase that takes finite-difference
gradients: the ``STRATEGY``-level optimiser (each ``MULTISTART`` start),
the ``PRESTAGE`` exploration and refinement, and an ``MLSL`` / ``AUGLAG``
block whose local search takes gradients — NLopt differentiates that
block's objective, so its ``INNER`` search takes the enclosing block's
settings; ``GD_MLSL*`` and ``LD_AUGLAG*`` with a derivative-free ``INNER``
block take none (:ref:`GRAD_STRATEGY <hpo_optimizer_common>`).  An invalid value is an error there, with the same message on
every backend (``PARAM fd_kernel must be inhouse or ceres (got 1)``), and so
is one that can never take effect (``… PARAM fd_step never takes effect:
NLOPT:LN_SBPLX takes no gradient; remove it.``): ``fd_step``, ``fd_kernel``
or ``diff_method`` in an ``INNER`` block, on a derivative-free optimiser, or
under ``GRAD_STRATEGY ANALYTICAL``.  The ``NOISE`` block's ``AUTO``, which
derives ``fd_step``, follows the same rule (:ref:`hpo_noise_block`).  Before
1.6.0 such settings were accepted and ignored, and so were ``diff_method``
with the in-house kernel and ``fd_kernel`` and ``diff_method`` under
``HYBRID``; an invalid ``fd_kernel`` under ``HYBRID`` was accepted too.

**The stencil.**  The step :math:`h` is never shrunk or grown; only its
direction changes.  Along each coordinate, with the box :math:`[a, b]` in
internal coordinates (the conditions are tested on the points as computed,
so a chosen stencil never leaves the box):

* ``CENTRAL``: :math:`u \pm h` when both lie in the box, and the difference
  :math:`(f(u+h) - f(u-h)) / 2h` of each rule's old kernel, bit for bit;
  otherwise one-sided into the box, towards the side with room, at
  :math:`u + s` and :math:`u + 2s` (:math:`s = \pm h`):
  :math:`(-3 f(u) + 4 f(u+s) - f(u+2s)) / 2s`;
* ``FORWARD``: :math:`(f(u+h) - f(u)) / h`, or backward,
  :math:`(f(u-h) - f(u)) / (-h)`, when :math:`u + h` lies above the box.

**Accuracy and cost.**  On :math:`f = e^{3u}` at :math:`u = 0` (relative
errors; :math:`\sigma` is the noise of one loss):

.. list-table::
   :header-rows: 1
   :widths: 26 8 18 14 16 18

   * - Stencil
     - Order
     - Truncation error
     - Noise
     - Error at :math:`h = 10^{-3}`
     - With :math:`\sigma = 10^{-6}`
   * - Central
     - :math:`h^2`
     - :math:`h^2 |f'''| / 6`
     - :math:`0.71\,\sigma/h`
     - 1.5e-6
     - 2.4e-4
   * - One-sided 3-point
     - :math:`h^2`
     - :math:`h^2 |f'''| / 3`
     - :math:`2.55\,\sigma/h`
     - 3.0e-6
     - 8.4e-4
   * - Forward (``diff_method 1``)
     - :math:`h`
     - :math:`h |f''| / 2`
     - :math:`1.41\,\sigma/h`
     - 1.5e-3
     - 1.6e-3

The one-sided difference is second order like the central one, with twice
its error constant, and amplifies the noise of the losses 3.6 times as much
(:math:`\sqrt{26}/2` against :math:`1/\sqrt{2}`): with a noisy loss (a
``PC_LAMMPS`` relaxation, a hot-variable warm start) a gradient next to a
bound is noisier than one inside the box at the same step.  It costs the
same:

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - Evaluations per gradient, after the loss at the point (*n* parameters, *m* differenced by ``HYBRID``)
     - Inside the box
     - Next to a bound
   * - ``NUMERICAL``, ``CENTRAL``, either rule
     - 2 *n*, plus 1 when the loss at the point is not passed
     - 2 *n*, plus 1 when the loss at the point is not passed
   * - ``NUMERICAL``, ``FORWARD``
     - *n*, plus 1 when the loss at the point is not passed
     - *n*, plus 1 when the loss at the point is not passed
   * - ``HYBRID``, ``CENTRAL``
     - 1 + 2 *m*
     - 1 + 2 *m*, plus 1 when the loss at the point is not passed
   * - ``HYBRID``, ``FORWARD``
     - 1 + *m*, plus 1 when the loss at the point is not passed
     - 1 + *m*, plus 1 when the loss at the point is not passed

A gradient takes the loss the optimiser has just evaluated at exactly
its point, and does not evaluate the point again: a one-sided or forward
difference uses that loss, a central one does not (``NUMERICAL`` only
checks that it is finite), and ``HYBRID``'s analytical pass (the 1)
evaluates no loss.  All three gradient families pass it with every
gradient they ask for: the NLopt ``LD_*`` searches and the ``CERES``
drivers evaluate the loss and the gradient together, and ``DLIB:BFGS`` asks
for a gradient at its start, right after the loss there, and after each
line search at the point the search ended on, the last point it evaluated
(its line search evaluates the loss alone).  A gradient evaluates the
point itself only when it is asked for without that loss, once per
gradient (``DIAGNOSTICS`` counts ``HYBRID``'s: ``HYBRID centre losses``).
With a hot-variable warm start the loss the optimiser was given is the
one used.  Every evaluation counts toward ``MAXEVAL``.  Before 1.6.0
``NUMERICAL`` evaluated the loss at the point itself first (1 + 2 *n*
and 1 + *n* evaluations per gradient), a second evaluation of a point
the optimiser had just evaluated; central differences are the same bit
for bit without it.

**A box too narrow for the step** — one in which some point has no
stencil — is refused at startup on each phase that takes finite
differences, and by the gradient before it evaluates anything.  Under
``OPTIM_SCALE AUTO`` that is ``fd_step`` above 1/3 (``CENTRAL``) or 1/2
(``FORWARD``) with the in-house rule, and above 1/2 (``CENTRAL``) or 1
(``FORWARD``) with Ceres' rule, whose step shrinks towards the lower bound;
under ``NONE`` the check runs over the whole box.  The check is sound: a
box it accepts never has a point evaluated outside it.  The limits above
are exact.  It bounds the step as computed — exactly for a power-of-two
``fd_step``, else within an ulp of it — but takes :math:`u \pm h` as exact
numbers, where the stencil tests them rounded, so in a box only hundreds or
thousands of ulps wide (possible under ``NONE``) it can refuse an
``fd_step`` that every point of the box would accept, by up to a few per
cent.  The message names the phase,
each such dimension with its box and a point without a stencil, the step and
its source (``PARAM``, the default, or ``NOISE`` ``AUTO``), the largest
``fd_step`` that fits, and what else would do:

.. code-block:: none

   the STRATEGY-level optimiser DLIB:BFGS: PARAM fd_step 0.4 does not fit these OPTIM boxes: no finite-difference point is evaluated outside the box, and next to a bound the CENTRAL stencil turns one-sided and needs two steps of h = 0.4 of each box width (in-house rule) inside it; the step is not shrunk:
     - WATOM: OPTIM box [0.1, 5] (internal [0, 1]); no stencil fits at WATOM = 1.57; fd_step up to 0.333333 fits it
     Set PARAM fd_step to at most 0.333333.

   the STRATEGY-level optimiser DLIB:BFGS: the default fd_step 1e-06 does not fit these OPTIM boxes: … needs two steps of h = max(2^-26, 1e-06 x |u|) in internal units (Ceres' rule) inside it; the step is not shrunk:
     - WATOM: OPTIM box [1, 1.000001]; no stencil fits at WATOM = 1.0000005; fd_step up to 3.33333e-07 fits it
     Set PARAM fd_step to at most 3.33333e-07; or use OPTIM_SCALE AUTO (fd_step is then a fraction of each box width).

The step is not shrunk for such a dimension: that would change what
``fd_step`` means there, and the noise it was chosen to step over (``NOISE``
``AUTO``); a step that large says little about the slope anyway.  Under
``NONE`` a ``LOG`` box suits a positive parameter that spans decades: its
internal width is then its number of e-foldings.

**A point outside the box** is evaluated only when an optimiser that ignores
the bounds — the ``CERES`` drivers and NLopt ``LN_NEWUOA`` — puts one there,
or hands one to a ``PRESTAGE`` refinement.  Finite differences never do, nor,
with the bundled ASA, do ASA's probes (:ref:`asa_optimizer`).  A gradient at
such a point is differenced towards the box, one-sided (``FORWARD``: the one
step towards it), so no finite-difference point lies further outside than
the point itself; ``DIAGNOSTICS`` counts such gradients.  ``HPO_Host`` counts
every evaluation outside the box, whatever asked for it, before the
``RHOFORM`` projection (:ref:`hpo_rhoform_positive`), and warns at the
first:

.. code-block:: none

   WARNING   eval 5: CGRID2B[2] = 3 lies outside its OPTIM box [1, 2] and is evaluated there: finite differences keep to the box, and a point proposed within rounding of a bound is evaluated on it, so an optimiser that ignores the bounds (CERES:*, NLopt LN_NEWUOA), or a start it handed on, stepped outside it; the loss of such a point can become the best. DIAGNOSTICS counts every evaluation outside the box

**A point within rounding of a bound.** A coordinate proposed outside its
box by no more than rounding is evaluated on the bound instead, recorded
there (``params.tadah``, the potentials, the best) and counted in
``DIAGNOSTICS`` (``Snapped onto the box``), with no warning.  Rounding means
at most 8 units in the last place from the bound, in real coordinates — of
:math:`\max(|lo|, |hi|)` on a linear dimension, of the bound itself on a
``LOG`` or ``ASINH`` one — or in the internal coordinates the optimiser
works in.  Under ``OPTIM_SCALE NONE`` NLopt's own arithmetic rounds points on
a bound up to 2 such units past it (``LN_COBYLA``, ``LN_PRAXIS``, whose map
of the box is ``mid + width·tanh``, ``LN_BOBYQA``, the unscaled
``GN_DIRECT*_NOSCAL`` variants, and ``MLSL`` and ``AUGLAG`` with such a
local search; none of them does in the unit box of ``OPTIM_SCALE AUTO``):
such points were evaluated outside the box and warned about.

An optimiser that ignores the bounds gets one startup ``WARNING`` in every run that uses it
(``CERES:LBFGS (OUTER): ignores the OPTIM box: Ceres'
GradientProblemSolver has no bounds, …``); keeping its own iterates in the
box is a separate matter.  The ends of a box map to exactly its ``OPTIM``
bounds, and a point inside it into them: ``LOG`` and ``ASINH`` boxes gave
back an end one unit in the last place outside (``OPTIM X 0.01 10 LOG``
pressed against its upper bound recorded ``X = 10.000000000000002``).

**The journal.**  The ``OPTIMISER`` section has a line per phase that
takes finite differences, e.g.

.. code-block:: none

   Finite differences    : CENTRAL, h = 1e-06 of each box width (default fd_step; in-house rule); next to a bound: one-sided 3-point into the box, same h, same cost

The first one-sided difference of each dimension is an ``INFO`` line
(``finite differences: RCUT2B = 6 lies within a step of its upper bound
(OPTIM box [4, 6]): one-sided 3-point difference, at RCUT2B = 5.999998 and
RCUT2B = 5.999996 with the loss at the point itself; …``), and
``DIAGNOSTICS`` counts, each only when it is not zero, the one-sided
differences per dimension, the points snapped onto the box, the evaluations
and the gradients outside the box, ``HYBRID``'s own evaluations of the
point, and the evaluations whose cutoff (``RCUT2B``/``RCUTMB``, or a knot
beyond the box) exceeds the neighbour lists, which ``tadah hpo`` builds
once for the ``OPTIM`` box and rebuilds for such an evaluation (1.6.0;
before, the short lists were used silently):

.. code-block:: none

   Snapped onto the box    : 4 (points proposed outside the OPTIM box within
     rounding, 8 units in the last place, and evaluated on the bound)
   Out-of-box evaluations  : 3 (points outside the OPTIM box, evaluated there: an
     optimiser that ignores the bounds; see the first such WARNING)
   Out-of-box gradients    : 2 (at points outside the OPTIM box, each finite
     difference stepped towards the box)
   Neighbour lists rebuilt : 2 evaluation(s) with a cutoff above the one-time neighbour
     lists (sized for the OPTIM box), evaluated with lists rebuilt for them;
     see the first such WARNING
   One-sided differences   : 1 (a coordinate within a step of its bound, or outside
     the box, differenced into it): RCUT2B 1
   HYBRID centre losses    : 1 (evaluations of a gradient's point for its
     one-sided or forward differences, where the optimiser's own loss could not
     be reused)

A run whose gradients never come within a step of a bound keeps its
``DIAGNOSTICS``.

**What changed.**  Before 1.6.0 every stencil was central: within a step of
a bound one of its points was evaluated outside the box (examples 01, 05,
09, 13, 23 and 26 recorded ``RCUT2B = 6.000002``, outside ``[4, 6]``, as
their best), or, for a ``RHOFORM NONNEG``/``MONOTONE`` amplitude projected
onto its bound, the derivative at a zero amplitude came out as
:math:`(f(h) - f(0)) / 2h`, half the slope.  Ceres' rule, the default under
``NONE``, ran inside Ceres, which has no bounds.  Runs whose gradients never
come within a step of a bound are unchanged bit for bit, with either rule
and with ``HYBRID`` (only the ``OPTIMISER`` line is new), and so are
derivative-free, ``ANALYTICAL``, annealing, ``ASA``, ``GRID`` and
``RANDOM`` runs — except where a run evaluates an end of a ``LOG`` or
``ASINH`` box (a ``GRID`` over one always does) or, rarely, of a linear box
under ``OPTIM_SCALE AUTO``: that end is now exactly the ``OPTIM`` bound,
where it could come out one unit in the last place outside; and where an
NLopt search under ``OPTIM_SCALE NONE`` proposes a point within rounding
outside the box, which is now evaluated on the bound.  A search whose gradient reaches a bound can take a
different path from there; so can a ``NONNEG``/``MONOTONE`` run at a zero
amplitude, a ``CERES`` run whose iterates leave the box, a ``LOG`` or
``ASINH`` run at a bound (at the ulp level), an NLopt run under
``OPTIM_SCALE NONE`` that rounded a point past a bound (evaluated on it
now), a run with a formerly ignored
``diff_method`` or ``fd_kernel``, and a ``HYBRID`` run without an
analytical derivative, which took no finite differences and returned a zero
gradient, so that a local search stopped at its start.  Refused at startup:
an ``fd_step`` too large for a box (above 1/3 under ``AUTO``), ``RIDDERS``,
settings that never take effect, the ``NOISE`` block's ``AUTO`` where no
slope is measured or beside a written ``PARAM fd_step`` (it was ignored
with a note on stderr), and ``ASA``
settings whose probes cannot fit a box (:ref:`asa_optimizer`).  ``NOISE``
``AUTO`` now derives ``fd_step`` for an ``MLSL`` or ``AUGLAG`` block whose
local search takes gradients too, which it left without.

.. _hpo_gradient_not_finite:

Points without a usable gradient
................................

The gradient-based backends — ``DLIB:BFGS``, NLopt's ``LD_*`` and
``GD_*`` algorithms (for ``MLSL`` and ``AUGLAG``, their local search
when it is one) and every CERES driver — take a finite-difference gradient (``GRAD_STRATEGY
NUMERICAL``, the default, and the finite-difference part of ``HYBRID``)
from the losses at a stencil of points around the current one (``PARAM
fd_step``, ``fd_kernel``).  A failed evaluation — its loss is
``FAILSCORE`` or a graded failure (at least ``0.999 × FAILSCORE``), or it
is not finite — gives no slope, whatever ``FAILSCORE`` is: the difference
of two failures is an exact 0, and the difference with a usable loss is as
large as the jump to ``FAILSCORE`` (it overflows under the default, the
largest double; under a finite ``FAILSCORE`` such as ``1e9`` it is a
steep, finite slope).  The loss at the point decides what the gradient
does with a failed evaluation:

* At a usable loss, the first failed evaluation of the stencil fails the
  gradient; the stencil points after it are not evaluated.
* At a failed loss, the gradient fails at once, before any stencil point:
  with ``FAILSCORE_GRADING OFF``, at a loss equal to ``FAILSCORE`` (an
  ungraded failure) and at a loss that is not finite.  A local gradient
  search that starts inside the failure region — ``DLIB:BFGS``, an
  ``LD_*`` algorithm or an ``AUGLAG`` whose local search is one, a CERES
  driver — therefore stops at its first gradient (below); it used to take
  the zero difference of two failures for convergence.  ``MLSL`` and the
  global ``GD_*`` searches reject such a start and go on to other points.
* At a graded failure under ``FAILSCORE_GRADING ON``
  (:ref:`failscore_grading`) the stencil is evaluated and its differences
  are taken as they are, a direction out of the failure region.  The
  gradient fails only when every difference is 0 (``… and every difference
  at it is 0: a flat failure region``): a saturated measure, or a band that
  is constant without ``PC_FAILPROBE``.  Inside the band the differences
  are its graded slope; next to the usable side a difference spans the
  jump from the band to a usable neighbour and is as steep as that jump,
  which can mislead the searches.  In the example below under ``FAILSCORE 1e9``
  with ``FAILSCORE_GRADING ON``, from 1.4 (a graded failure whose stencil
  point 1.6 is usable) ``DLIB:BFGS`` evaluates the upper bound 2 23 times
  in a row, 31 evaluations in all; ``LD_SLSQP`` stops after 3 evaluations
  with ``STOPPED -- bug: more than iter SQP iterations (algorithm
  converged; …)``, its best the stencil point 1.6, short of the optimum at
  2; ``CERES:LBFGS`` leaves the box (2.4).  From 1.25, where the whole
  stencil is graded, ``DLIB:BFGS`` reaches the bound at its 4th
  evaluation.

A one-sided stencil near a bound, and a ``FORWARD`` one
(:ref:`hpo_fd_stencil`), take the loss at the point itself.  ``HYBRID``
reads the loss the optimiser has just evaluated at the point; without it
(``DLIB:BFGS`` asking for the gradient of another point), it evaluates the
point for a one-sided or forward stencil, and under
``FAILSCORE_GRADING ON`` also when a central stencil point failed, to tell
a graded failure at the point from a usable loss (counted under ``HYBRID
centre losses`` in the journal's DIAGNOSTICS); under ``OFF`` a failed
central stencil point fails its gradient at once.

``GRAD_STRATEGY ANALYTICAL``, and ``HYBRID`` before its analytical pass,
read the loss the optimiser has just evaluated at the point too: a failed
one — ``FAILSCORE_GRADING OFF``, ``FAILSCORE`` itself, a loss that is not
finite — fails the gradient at once, ``a failed evaluation at the point``,
and no analytical gradient is formed.  The analytical gradient at a failed
point is that of the loss before the failure (under a skip cap the
uncapped loss), small but not 0, while the optimiser sees ``FAILSCORE``;
a search that started inside the failure region took it for a slope and
converged on ``FAILSCORE``, under ``HYBRID`` with nothing differenced —
the default under ``PC_OBSERVABLE`` (:ref:`pc_observable`) — as under
``ANALYTICAL``.  At a graded failure under ``FAILSCORE_GRADING ON`` the
analytical gradient is kept; it does not see the grading.

An evaluation with a failed term is a failed evaluation — a failed
``PC_LAMMPS`` simulation (``--failscore``, :ref:`pc_lammps_options`) or a
failed ``PC_OBSERVABLE`` term (:ref:`pc_observable`) —, and the rule
reads it as one.  Under ``FAILSCORE_GRADING OFF`` a search that starts
where a ``PC_OBSERVABLE`` term fails stops at its first gradient; under
``ON`` its finite differences fail when the failed fraction that grades
the failure is the same at every point of the stencil (``… a flat failure
region``), and an analytical gradient is kept (above).  When the search
so stopped is the run's only one, the run ends without a usable loss
(status 1).  Under a ``FAILSCORE`` that does not absorb the usable terms (``1e16``
next to terms of about 1e5) such a search used to end with a best and
status 0 under either setting: most backends followed the gradient of
the usable terms out of the region, and ``LD_SLSQP`` stopped at once
(roundoff-limited) with a failed evaluation as its best.  Before 1.6.0 a
failed ``PC_OBSERVABLE`` term, and under ``FAILSCORE_GRADING OFF`` a failed
``PC_LAMMPS`` simulation, was averaged with the evaluation's other terms
(under ``ON`` a failed simulation was graded), and a mean below ``0.999 ×
FAILSCORE`` was a usable loss: with one failed observable among *n* ≥ 2
terms it was about ``FAILSCORE`` / *n*, and with a failed simulation it
was under ``L1``, the bounded ``TUKEY`` or a small ``--failscore``.  An
evaluation with no usable term — a target whose only term is the failed
observable — scored ``FAILSCORE`` then too, and so did one whose failed
terms summed past the largest double (two of them under the default
``FAILSCORE``).

The rule has limits.  A usable point whose stencil reaches a failure has
no gradient, so a search cannot settle within one step (``PARAM
fd_step``) of the failure region.  A graded failure next to the usable
side differences the jump out of the band (above).  And the analytical
gradient at a graded failure does not see the grading.

A point without a usable gradient ends the search where the backend cannot
go on without it:

* ``DLIB:BFGS`` cannot reject a point: the search ends at the first such
  gradient.
* A local NLopt gradient search ends at such a point before it has had a
  usable gradient — at its starting point, where it has no point to step
  back to.  That is an ``LD_*`` algorithm without an ``INNER`` block, and an
  ``AUGLAG`` whose local search is one — ``LD_AUGLAG``'s default
  ``LD_MMA``, or an ``LD_*`` ``INNER`` block: HPO sets ``AUGLAG`` no
  constraint, so it runs its local search once, from the starting point.
* Otherwise NLopt and the CERES drivers reject a point without a usable
  gradient (NLopt sees ``+inf``, Ceres a failed evaluation) and step
  back, as they do for a failed evaluation, and the search goes on.  A
  search that ends right after such a rejection ended because of it.  For
  NLopt that means no evaluation after the rejection — under ``AUGLAG``,
  none but the one ``AUGLAG`` makes of the point its local search
  returned — and neither ``MAXEVAL`` (every evaluation counted),
  ``MAXTIME`` nor ``STOPVAL`` reached, nor, under ``AUGLAG``, the
  ``MAXEVAL`` (its ``INNER`` block's, in NLopt calls) or ``MAXTIME`` of
  its local search (which ``AUGLAG`` reports as ``FTOL_REACHED``).  NLopt's
  own ending is then a success code, ``roundoff-limited``, ``nlopt
  failure`` or a tolerance: an ``XTOL_REACHED`` or ``FTOL_REACHED`` there
  is the rejection's ending, reached on the steps the search took back
  from the rejected point.  For Ceres it means ``FAILURE`` (``Initial
  cost and jacobian evaluation failed``, ``Numerical failure in line
  search``) right after the rejection.
* ``MLSL`` and the global ``GD_*`` searches (``GD_STOGO``) are never
  stopped at a rejection: a rejected local start or point leaves them the
  rest of their search.

Such an ending is the search's own.  The journal has a warning that names
the backend and the failed evaluation — the stencil point and its loss, or
the loss at the point — and the ``Optimiser status`` is ``STOPPED -- no
usable gradient: a failed evaluation in the finite-difference stencil``
(``… a failed evaluation at the point`` when ``ANALYTICAL`` or ``HYBRID``
read a failed loss before their analytical pass).
For a one-dimensional search (``OPTIM CGRID2B (2) 1.0 2.0``, ``PARAM
fd_step 0.2``) whose evaluations fail below about 1.47 (a skip cap), from
1.6, next to the failure region, from 1.25, inside it, and from 1.25 under
``GRAD_STRATEGY ANALYTICAL`` (``OALGO 4``):

.. code-block:: none

   WARNING   DLIB:BFGS stopped: no usable gradient: a failed evaluation in the finite-difference stencil (at CGRID2B[2] = 1.6, the stencil point CGRID2B[2] = 1.4 failed: loss 1.797693135e+308); best-so-far preserved
   WARNING   DLIB:BFGS stopped: no usable gradient: a failed evaluation in the finite-difference stencil (the loss at CGRID2B[2] = 1.25 is 1.797693135e+308); best-so-far preserved
   WARNING   DLIB:BFGS stopped: no usable gradient: a failed evaluation at the point (the loss at CGRID2B[2] = 1.25 is 1.797693135e+308); best-so-far preserved

The first run makes three evaluations (1.6, then the stencil 1.8 and 1.4)
under the default ``FAILSCORE`` and under ``FAILSCORE 1e9`` alike, and
ends with status ``0``; the other two make one, and end without a usable
loss.  A gradient that is not finite for another reason ends the search
the same way, with its own cause: ``the finite-difference quotient overflowed``
(two usable losses too far apart for the step), ``the analytical gradient
could not be formed at this point`` or ``the analytical gradient is not
finite at this point`` (``ANALYTICAL``, ``HYBRID``).  The evaluations are
the ones the backend made: nothing stands in for the failed one, and the
search does not go on past the point where it ends.  The run then ends
like any run whose optimiser ended on its own (:ref:`hpo_stopping`): with
status ``0`` when an evaluation produced a usable loss — the stencil's
other points normally did — and as a run without a usable loss otherwise.
The search's ending does not end the strategy: the other ``MULTISTART``
starts, and the ``PRESTAGE`` refinement after its exploration, still run,
and the trailer is the status of the last search that ran, as for any
search that ends on its own (under ``MULTISTART`` the last start's: a
start that such a gradient ended shows only in its journal warning when a
later start runs to its own end).  A signal or an error decides the ending
first.

On v1-dev until 2026-10-07 only a loss that was not finite, or a
difference that overflowed, failed a finite-difference gradient, the
analytical strategies did not read the loss, and the status read
``STOPPED -- gradient not finite: …``.  The run clamps every failed
evaluation to a finite ``FAILSCORE``, so a start inside the failure region
got the zero difference of two failures: from 1.25 in the example above
``DLIB:BFGS`` made 8 evaluations (1.25, then 1.25, 1.45 and 1.05 for its
gradient, twice: each gradient also evaluated its point again) and ended
as converged, ``LD_LBFGS``, ``LD_TNEWTON`` and ``LD_VAR2`` ended
``SUCCESS -- generic success`` after 4, ``LD_MMA`` and ``LD_CCSAQ`` asked
for the same point and its stencil until ``MAXEVAL``, and the CERES
drivers reported ``CONVERGENCE`` at their first iteration; such a
``MULTISTART`` start ended without a word after 8, and as the last start
left the trailer ``COMPLETED``.  Under ``ANALYTICAL`` (or ``HYBRID`` with
nothing differenced) such searches converged on ``FAILSCORE`` the same
way: ``DLIB:BFGS`` and the ``MULTISTART`` start ended as converged, Ceres
with ``CONVERGENCE``, ``LD_LBFGS`` with ``nlopt failure -- algorithm
converged``.  Under ``FAILSCORE 1e9`` the steep slope from 1.6 led
``DLIB:BFGS`` to evaluate the upper bound 28 times in a row, 60
evaluations in all, and the CERES drivers out of the box.

Before 1.6.0 these endings were reported as ``MAXEVAL=<n> reached`` and
``COMPLETED`` (``DLIB:BFGS``), ``SUCCESS -- generic success``
(``LD_LBFGS``, ``LD_TNEWTON``, ``LD_VAR1``, ``LD_VAR2``),
``roundoff-limited`` (``LD_SLSQP``) or ``COMPLETED`` (the CERES drivers).
``LD_MMA`` and ``LD_CCSAQ`` did not stop at a starting point without a
finite gradient: they asked for the same point again and again until
``MAXEVAL`` (120 evaluations of three points for ``MAXEVAL 60``), and so
did ``LD_AUGLAG``'s default local search and an ``INNER`` ``LD_MMA`` or
``LD_CCSAQ``; ``AUGLAG`` with an ``INNER`` ``LD_LBFGS`` or ``LD_TNEWTON``
ended at once and reported ``FTOL_REACHED``.  They now stop there too;
later in a search ``LD_MMA`` and ``LD_CCSAQ`` reject such a point and
shrink their trust region, as before.

A usable loss at or below ``STOPVAL`` comes before all of these: NLopt
is given ``+inf`` for a point without a usable gradient, or whose
gradient ``MAXEVAL`` cut short, and so never tests that point's loss
against ``STOPVAL``, as it does every loss it is given.  An NLopt search
whose withheld loss is usable (not a failure under ``FAILSCORE``) and at
or below ``STOPVAL`` ends there instead of stopping or going on as
above: its status is ``STOPVAL_REACHED``, not ``STOPPED -- no usable
gradient``, it makes no evaluation after that point (``GD_STOGO``, which
ignores the stop, calls on until its ``MAXEVAL``, each call refused; see
:ref:`hpo_stop_criteria`), and the journal has no warning but an
``INFO`` line that says which loss and why:

.. code-block:: none

   INFO      NLopt finished: STOPVAL_REACHED -- objective below stopval threshold after 5 evaluation(s): the loss 24.2 is at or below STOPVAL 100, and NLopt was not given it: its point has no usable gradient: a failed evaluation in the finite-difference stencil (at …)

Before 1.6.0 the search went on past that point, or ended there with
another status.

.. _hpo_stop_criteria:

Stop criteria per backend
.........................

Each backend ends a run on the criteria it honours — ``MAXEVAL``,
``MAXTIME``, ``STOPVAL`` and the tolerances its stop tests read, set
above 0 — and most also end on their own.  A tolerance that ends a
gradient-based search right after it rejected a point without a usable
gradient is reported as that rejection's ending
(:ref:`hpo_gradient_not_finite`).  A backend with no convergence
test and no finite schedule runs until it is interrupted when it has
none of them, so at least one is required: a startup error names the
backend and the stops it honours.  An active ``RESTART``
(:ref:`restart_decorator`) counts as one: once its ``MAX`` restarts are
used, the next ``AFTER_STAGNATION`` evaluations without a new best end
the run, whatever the phase — but not for ``GD_STOGO`` and
``GD_STOGO_RAND``, which need ``MAXEVAL`` (below).  Which backend is which was measured: each
ran without a stop, on the HPO example data and on synthetic objectives,
until it was killed or ended.

.. list-table::
   :header-rows: 1
   :widths: 30 26 44

   * - Backend
     - Without a stop
     - Stops that end a run
   * - ``TADAH:RANDOM``
     - never ends: one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - ``TADAH:GRID``
     - ends after its grid
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - ``TADAH:ANNEAL``, ``ANNEAL_PA``
     - ends at the end of its temperature schedule
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``
       (the plateau rule)
   * - ``TADAH:ANNEAL_PT``
     - never ends (the ladder keeps its temperatures): one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``
       (the best-stall rule)
   * - ``ASA:ASA``
     - ends on its own limits
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` (and ASA's own options)
   * - ``DLIB:GFS``
     - never ends: one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` (``FTOL_ABS`` is its solver
       epsilon)
   * - ``DLIB:BFGS``
     - converges
     - ``MAXEVAL``, ``FTOL_ABS`` (or ``PARAM tol_gradinf``)
   * - ``CERES`` (all four)
     - converges
     - ``MAXEVAL``, ``FTOL_REL``, ``XTOL_REL`` (and ``PARAM tol_gradinf``)
   * - NLopt ``GN_DIRECT``, ``GN_DIRECT_L``, ``GN_DIRECT_L_RAND`` and
       their ``_NOSCAL`` forms, ``GN_CRS2_LM``, ``GN_ISRES``
     - never ends: one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``,
       ``XTOL_REL``, ``XTOL_ABS``
   * - NLopt ``GN_ESCH``, ``GN_AGS``
     - never ends: one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - NLopt ``GD_STOGO``, ``GD_STOGO_RAND``
     - never ends, and ignores NLopt's forced stop: ``MAXEVAL``
       required (below)
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``
   * - NLopt ``GN_MLSL``, ``GN_MLSL_LDS``, ``GD_MLSL``, ``GD_MLSL_LDS``,
       ``G_MLSL``, ``G_MLSL_LDS``
     - never ends: one required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL`` (the block's tolerances reach
       only NLopt's default local optimiser, so they are accepted without
       an ``INNER`` block and refused with one: set them in ``INNER``)
   * - NLopt ``LD_MMA``, ``LD_CCSAQ``, ``LD_SLSQP``
     - never ends (the tolerances are its only convergence test): one
       required
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``FTOL_REL``, ``FTOL_ABS``,
       ``XTOL_REL``, ``XTOL_ABS``
   * - NLopt ``GN_ORIG_DIRECT``, ``GN_ORIG_DIRECT_L``
     - ends after about 100000 evaluations of its own (the cap is
       checked once per iteration)
     - ``MAXEVAL``, ``MAXTIME``, ``STOPVAL``, ``XTOL_REL``
   * - NLopt ``AUGLAG``, ``AUGLAG_EQ``, ``LN_AUGLAG``, ``LD_AUGLAG`` and
       their ``_EQ`` forms
     - ends when its local search ends
     - its local search's (below; the block's tolerances, as for MLSL,
       only without an ``INNER`` block)
   * - every other NLopt algorithm (``LN_*``, ``LD_LBFGS``, …)
     - converges
     - all seven

Each of these ends the run only when it is met: a ``STOPVAL`` below
every loss the search finds, or a tolerance too tight to be reached,
leaves it running until it is interrupted, so ``MAXEVAL`` or ``MAXTIME``
is the stop that always ends it.  A tolerance of 0 or below is no stop
at all: NLopt switches it off, and ``ANNEAL_PT``'s best-stall rule arms
only above 0, so ``FTOL_REL 0`` alone is a startup error for a backend
that needs a stop (it used to run until it was killed).  DIRECT tests
``XTOL`` on every rectangle it divides, and it divides the largest one
in every sweep, so ``XTOL`` ends it only once every rectangle is that
small: roughly (1/``XTOL_REL``)\ :sup:`n` evaluations for *n*
dimensions (``XTOL_REL 1e-3`` in two dimensions ran past 20000); it
counts as a stop, and the message says what it costs.  ``CRS2`` and
``ISRES`` test the tolerances when the best improves.  ``GD_STOGO`` and
``GD_STOGO_RAND`` need ``MAXEVAL``, in their own block or, as an
``INNER`` search, in the enclosing one: NLopt's StoGO ignores the forced
stop by which HPO ends a search on a signal, an error, the ``RESTART``
stagnation stop or a loss at or below ``STOPVAL`` that NLopt was not
given (:ref:`hpo_gradient_not_finite`).  From then on StoGO calls on, and
HPO refuses each call without evaluating it, a few microseconds each,
until NLopt's own evaluation count ends it; its clock would end it only
once the rest of ``MAXTIME`` had passed.  ``MAXTIME``, ``STOPVAL`` and an
active ``RESTART`` still end such a run when they are met, but none of
them is enough as its stop:

.. code-block:: none

   HPOEngine error: the STRATEGY-level optimiser: NLOPT:GD_STOGO needs a stop: NLopt's StoGO has no convergence test of its own and ignores NLopt's forced stop, by which HPO ends a search (on a signal, an error, the RESTART stagnation stop, or a loss at or below STOPVAL that NLopt was not given): it then calls on, each call refused, and only NLopt's own evaluation count ends it promptly (MAXTIME only once the rest of its time has passed); set MAXEVAL (it honours no tolerance)

Before 1.6.0 ``MAXTIME``, ``STOPVAL`` or an active ``RESTART`` was
accepted as its stop: without ``MAXEVAL``, once ``Ctrl-C``, an error or
the stagnation stop had stopped it, such a run never ended, or, with
``MAXTIME``, waited out the rest of ``MAXTIME`` at 100% CPU before it
reported.

A tolerance the backend's run never tests is refused at startup, naming
the phase and the stops the backend honours: every tolerance of
``TADAH:RANDOM``, ``TADAH:GRID``, ``GN_ESCH``, ``GN_AGS`` and
``GD_STOGO*``, all but ``XTOL_REL`` of ``GN_ORIG_DIRECT*``,
``DLIB:GFS``'s ``FTOL_REL``, ``XTOL_REL`` and ``XTOL_ABS``, and every
tolerance of an MLSL or AUGLAG block with an ``INNER`` block (NLopt
hands them only to the default local optimiser it creates without one;
``auglag.c`` tests them only to name the ending its ``INNER`` search
decides):

.. code-block:: none

   HPOEngine error: the STRATEGY-level optimiser: TADAH:RANDOM does not support FTOL_REL: uniform random search has no convergence test to apply it to; stop it with MAXEVAL, MAXTIME or STOPVAL
   HPOEngine error: the STRATEGY-level optimiser: NLOPT:G_MLSL_LDS does not support FTOL_REL with an INNER block: MLSL's multistart loop tests no tolerance, and NLopt passes the block's tolerances to no INNER search; set it in the INNER block, whose local search tests it

The second sends the tolerance into the ``INNER`` block only when that
block's algorithm tests it; otherwise it names the stops that search
honours (``…; the INNER block's NLOPT:GN_ESCH does not test it either:
stop that search with MAXEVAL or MAXTIME in the INNER block, or MAXEVAL,
MAXTIME or STOPVAL in the NLOPT:AUGLAG block, which NLopt hands on to
it``).

Before 1.6.0 they were accepted and did nothing: ``TADAH:RANDOM`` and
``TADAH:GRID`` accepted every tolerance (and ``POPULATION`` and
``VECTOR_ARR``; ``GRID`` also ``SEED``, which are refused now too), the
journal listed them among the stop criteria, and an ``AUGLAG``
tolerance with an ``INNER`` block relabelled a run that spent its
``MAXEVAL`` ``XTOL_REACHED``.  The local search of NLopt's MLSL
and AUGLAG (an ``INNER`` block, or without one NLopt's default local
optimiser, ``LN_COBYLA`` or ``LD_MMA``, which takes the block's
tolerances) must end too: it runs with the enclosing block's ``MAXEVAL``,
``MAXTIME`` and ``STOPVAL`` as well as its own ``MAXEVAL``, ``MAXTIME``
and tolerances (:ref:`hpo_inner_block`; its own ``MAXEVAL`` counts NLopt
calls, the enclosing block's every evaluation), so an ``INNER`` global search
under ``AUGLAG``, or ``LD_AUGLAG`` without an ``INNER`` block, needs one
of those:

.. code-block:: none

   HPOEngine error: the STRATEGY-level optimiser: the local search of NLOPT:LD_AUGLAG (NLopt's default NLOPT:LD_MMA, as the block has no INNER block) needs a stop: without a tolerance it has no convergence test and runs until it is interrupted; set MAXEVAL, MAXTIME, STOPVAL or a positive tolerance it honours (FTOL_REL, FTOL_ABS, XTOL_REL, XTOL_ABS) in the NLOPT:LD_AUGLAG block, which NLopt hands on to it

The rule holds in every phase (each ``MULTISTART`` start, the
``PRESTAGE`` exploration and refinement), and the message names it; an
active ``RESTART`` bounds every phase, the ``PRESTAGE`` exploration
included (it ends at its first stagnation and hands its best points to
the refinement).
Before 1.6.0 these configurations were accepted and ran until
interrupted (``Ctrl-C`` leaves a valid ``best_pot.tadah``,
:ref:`hpo_stopping`), and ``TADAH:RANDOM`` and ``DLIB:GFS`` needed
``MAXEVAL`` itself.  The journal says which stop ended a ``RANDOM``,
``GRID`` or ``GFS`` search (``TADAH:RANDOM stopped: STOPVAL reached after
7 evaluation(s)``), and the stop is the run's status
(``TADAH:RANDOM: STOPVAL reached``; :ref:`hpo_end_status`).  ``MAXTIME``
counts only while another evaluation would start: a search whose last
evaluation spends its ``MAXEVAL`` or its grid ends on that
(``TADAH:GRID stopped: MAXEVAL reached after 5 of 28 grid points``,
status ``TADAH:GRID: MAXEVAL reached``; it was ``COMPLETED`` before
1.6.0), however long that evaluation took.  The journal's stop criteria say what ends a
run that sets none of its own: its own end (``(none set -- TADAH:GRID
ends on its own: …)``), each ``MULTISTART`` start's
``BUDGET_PER_START``, or the ``RESTART`` stagnation stop (``(none set --
the RESTART stagnation stop ends NLOPT:GN_DIRECT_L: once its MAX 1
restart(s) are used, AFTER_STAGNATION 60 evaluations without a new
best)``).

.. _hpo_end_status:

How each backend reports its ending
...................................

Every optimiser run records why it ended, and the journal trailer of a
run that ends on its own is that status (``Optimiser status`` under
``RESULT``; :ref:`hpo_stopping`).  The TADAH, DLIB and CERES drivers
write ``<LIB>:<ALGO>: <reason>`` with a fixed reason text, so a script
or a fingerprint can match it; the journal's stop line gives the
numbers: the evaluations made, and for a tolerance its value, with
``(default)`` when its key was not set.  NLopt keeps its own result
strings and ASA ``ASA: <reason>``; a point without a usable gradient
ends any gradient-based search with ``STOPPED -- no usable gradient: …``
(:ref:`hpo_gradient_not_finite`), and the ``RESTART`` budget a run with
``STOPPED -- stagnation: …``.

A Ceres ending that has no such reason is named by Ceres' termination
type and the head of Ceres' message, its words before the first ``.``,
``,`` or ``:``, a fixed text as well: a failure that stopped the search,
the best result standing, reads ``STOPPED -- CERES:<ALGO>: FAILURE
(<head>)``, e.g. ``STOPPED -- CERES:STEEPEST_DESCENT: FAILURE (Numerical
failure in line search)``, and the stop line and a WARNING give Ceres'
whole message, its numbers included.

.. list-table::
   :header-rows: 1
   :widths: 18 40 42

   * - Backend
     - Ending → status
     - Journal stop line
   * - ``TADAH:RANDOM``
     - ``MAXEVAL reached``, ``MAXTIME exceeded``, ``STOPVAL reached``
       (``TADAH:RANDOM: MAXEVAL reached``)
     - ``TADAH:RANDOM stopped: <reason> after N evaluation(s)``
   * - ``TADAH:GRID``
     - the same, and the whole grid: ``all grid points evaluated``
     - ``TADAH:GRID stopped: <reason> after N of M grid points``;
       ``TADAH:GRID finished: all M grid points evaluated``
   * - ``TADAH:ANNEAL``, ``ANNEAL_PT``, ``ANNEAL_PA``
     - ``MAXEVAL reached`` (also during the T0 probes), ``MAXTIME
       exceeded``, ``STOPVAL reached`` (also on a probe), ``T_MIN
       reached`` (``ANNEAL``, ``ANNEAL_PA``), ``FTOL plateau`` (the
       plateau rule; ``ANNEAL_PT``'s best-stall rule), ``no optimisable
       dimensions``; a signal: ``SHUTDOWN (<signal>)``
     - ``ANNEAL stopped: <reason> after N host eval(s) (…)``, and
       ``ANNEAL_PT stopped: …``, ``ANNEAL_PA stopped: …``
   * - ``DLIB:GFS``
     - ``MAXEVAL reached``, ``MAXTIME exceeded``, ``STOPVAL reached``
     - ``DLIB:GFS stopped: <reason> after N evaluation(s)``
   * - ``DLIB:BFGS``
     - ``MAXEVAL reached``; ``FTOL_ABS reached``: the loss changed by
       less than ``FTOL_ABS`` (``1e-7`` by default) from one iteration
       to the next; ``tol_gradinf reached``: the L2 norm of the
       gradient fell below ``PARAM tol_gradinf``
     - ``DLIB:BFGS -- MAXEVAL=N reached; preserving best-so-far``;
       ``DLIB:BFGS stopped: FTOL_ABS reached after 51 evaluation(s):
       the loss changed by less than FTOL_ABS = 1e-07 (default) from one
       iteration to the next``
   * - ``CERES`` (all four)
     - Ceres' own reason when it converged: ``FTOL_REL reached``
       (``function_tolerance``, ``1e-6`` by default), ``XTOL_REL
       reached`` (``parameter_tolerance``, ``1e-8``), ``tol_gradinf
       reached`` (``gradient_tolerance``, ``1e-10``), or ran out of
       iterations: ``max_iter reached``; ``MAXEVAL reached`` when an
       evaluation refused for the budget made Ceres fail; any other
       failure: ``STOPPED -- CERES:<ALGO>: FAILURE (<head>)``, the head
       of Ceres' message (``Numerical failure in line search`` when a
       line search finds no valid step), the best result standing
     - ``CERES:LBFGS stopped: FTOL_REL reached after 45 evaluation(s):
       FTOL_REL = 1e-06 (default); Ceres: Function tolerance reached. …``
       (Ceres' message); a failure: ``CERES:STEEPEST_DESCENT stopped:
       FAILURE after N evaluation(s); Ceres: Numerical failure in line
       search, failed to find a valid step size, …``, and the WARNING
       ``CERES:<ALGO> -- non-convergent: <message>``; the
       ``termination:`` line before it
   * - NLopt
     - NLopt's result string: ``MAXEVAL_REACHED -- maximum evaluations
       exhausted``, ``XTOL_REACHED -- …``, …; an algorithm that
       terminated itself: ``STOPPED -- <what> (algorithm converged; …)``,
       e.g. ``STOPPED -- nlopt roundoff-limited (…)``; a result that
       masked an NLopt diagnostic: ``STOPPED -- masked <result>:
       <diagnostic>``
     - ``NLopt finished: <result> …`` for a result NLopt returned
       (``NLopt finished: MAXEVAL_REACHED -- maximum evaluations
       exhausted after N of MAXEVAL M evaluation(s)``).  An algorithm
       that terminated itself writes no such line: the WARNING ``<what>
       -- algorithm converged; best result preserved`` (stdout ``NLopt
       self-terminated (<what>) -- best result preserved.``); a masked
       diagnostic the WARNING ``<ALGO>: NLopt internal diagnostic was
       set after N evaluation(s) -- '<diagnostic>'. …``
   * - ``ASA:ASA``
     - ``ASA: <reason>`` (``ASA: MAXEVAL reached``, …)
     - ``ASA stopped: <reason> after N host eval(s) (asa exit: …)``

Ceres' reason is read from its message (Ceres 2.2: ``Function tolerance
reached``, ``Parameter tolerance reached``, ``Gradient tolerance
reached``, ``Maximum number of iterations reached.``); a Ceres version
that words it otherwise gives ``CERES:<ALGO>: CONVERGENCE (<head>)``
or ``NO_CONVERGENCE (<head>)``, with the head of Ceres' message, and
the unit tests that pin the wording fail.  Under ``MULTISTART`` the trailer
is the last start's status and under ``PRESTAGE`` the refinement's: its
``MAXEVAL reached`` is that start's budget (``BUDGET_PER_START``, or its
share of ``MAXEVAL``), not the ``STRATEGY``'s total.  A normal end
without a recorded status is an internal error: the run ends ``FAILED
-- internal error: the optimiser ended without recording why it ended``
with exit status ``1`` (please report it).

Before 1.6.0 these drivers recorded no status for most of their endings
and the trailer read ``COMPLETED``: ``DLIB:BFGS`` for a spent
``MAXEVAL`` and for its convergence test, which wrote no line at all,
``DLIB:GFS``, ``TADAH:RANDOM`` and ``TADAH:GRID`` for a spent
``MAXEVAL`` and the whole grid, the annealers for every stop
(``STOPVAL`` and ``MAXTIME`` included), and the CERES drivers for every
ending but a rejected gradient's, a ``FAILURE`` such as
``STEEPEST_DESCENT``'s ``Numerical failure in line search`` included.

.. _hpo_init_methods:

INIT block — seeding strategies
-------------------------------

Outside an ``INIT { ... } ENDINIT`` block the optimiser starts at the
single point taken from the values next to your ``OPTIM`` lines (the
implicit ``CONFIG`` seed).  Inside an ``INIT`` block you may declare a
*chain* of strategies that produces multiple starting points.

Chain entries
.............

Each ``METHOD <method> [args]`` line appends one entry to the chain.
``STRATEGY <method>`` is accepted as an alias (older configurations use
it), but ``METHOD`` is the canonical spelling: at the ``OPTIMIZER``
level ``STRATEGY`` opens a block, so reusing it inside ``INIT`` reads
ambiguously.  The chain is resolved into a flat list of starting points
before the optimiser runs.

.. code-block:: none

   OPTIMIZER
     INIT
       METHOD CONFIG
       METHOD LHS 8
       SEED 42
     ENDINIT
     STRATEGY
       ...
     ENDSTRATEGY
   ENDOPTIMIZER

================ ============== ============= ===========================================================
Method           Args           Default K     Effect
================ ============== ============= ===========================================================
``CONFIG``       (none)         1             Take the value next to each ``OPTIM`` key.
``RANDOM K``     ``K`` (int≥0)  ``K``         ``K`` uniform samples within (transformed) bounds.
``LHS K``        ``K`` (int≥1)  ``K``         Latin-hypercube sample (one per stratum per dim).
``SOBOL K``      ``K`` (int≥1)  ``K``         Sobol low-discrepancy sequence: ``SEED`` *s* draws block *s*
                                              of *K* points (below).
``WARM <file>``  path           1             Read a previous ``pot.tadah`` and pull values for
                                              every ``OPTIM`` key.
================ ============== ============= ===========================================================

``SOBOL`` draws whole blocks of the Sobol sequence: with seed *s* it takes
block *s* of *K* points, the points *s·K* … *s·K + K − 1* (the seed's 16
low bits select the block: *s* mod 65536).  Different blocks share no
point, so these draw disjoint points: the seeds ``SEED + r`` of
consecutive ``RESTART`` rounds, and an ``INIT`` block and a ``RESTART``
block with the same ``SEED`` and *K* (:ref:`restart_decorator`).  *K* is the number of points
the method draws: its ``K``, or ``K_TOTAL`` when ``SOBOL`` is the only
method.  ``SOBOL 1`` draws point *s*.  Without ``SEED`` the block is
chosen by ``std::random_device``; in a chain, each method after the first
draws with a seed derived from ``SEED``.  A block that would run past the
end of the sequence (2\ :sup:`64` − 1 points) is an error.  Before 1.6.0 a
seed skipped *s* points, so the seeds *s* and *s + 1* shared *K − 1* of
their *K* points.

Top-level keys inside the ``INIT`` block
........................................

.. list-table::
   :header-rows: 1
   :widths: 22 16 22 40

   * - Key
     - Type
     - Default
     - Effect
   * - ``METHOD`` (alias ``STRATEGY``)
     - repeatable
     - single ``CONFIG`` point when no ``METHOD`` line is given
     - Append an entry to the chain (see table above).
   * - ``K_TOTAL``
     - integer ≥ 0
     - sum of strategies' default K
     - Cap on total starting points produced.
   * - ``SEED``
     - integer ≥ 0
     - ``std::random_device``
     - Seed for the stochastic methods (RANDOM / LHS / SOBOL).  An NLopt
       optimiser without a ``SEED`` of its own also takes its random seed
       from it, and so does the search of its ``INNER`` block; its own
       ``SEED`` takes precedence.  Every other optimiser is seeded only by
       the ``SEED`` of its own block (``OPTIMIZER`` in the short form,
       ``STRATEGY``, ``EXPLORE``, ``REFINE``).
   * - ``WARM_ON_MISMATCH``
     - ``warn`` / ``error`` / ``skip``
     - ``warn``
     - What to do when ``WARM`` cannot fill every ``OPTIM`` key.

Each ``METHOD`` line is checked where it is written — a known method,
its argument count and ``K`` — so an error names that line (``Error on
line 3: INIT METHOD RANDOM K must be an integer >= 0, got '3x'``).

There is no ``INCLUDE_CONFIG`` keyword.  The in-config point is part of
the chain only when ``METHOD CONFIG`` is listed explicitly; an
``INIT`` block with no ``METHOD`` line (or no ``INIT`` block at all)
resolves to a single ``CONFIG`` point.  The removed ``INCLUDE_CONFIG``
and ``INIT_INCLUDE_CONFIG`` keywords are rejected with a message
pointing at ``METHOD CONFIG``.

Two implementation details affect reproducibility:

* The chain is resolved in the exact order it is written — no entry is
  reordered.  To make the warm point iter 0, list ``METHOD WARM``
  first in the ``INIT`` block.
* When the writer that produced the ``WARM`` file dumped
  ``# HOT_A0 <script> <a0>`` lines, ``WARM`` reads them and primes the
  warm-start cache (see :ref:`warmstart_protocol`).

How many seeds does the optimiser actually consume?
...................................................

A chain that produces more starting points than the optimiser can use
emits a warning at config time naming how many are discarded.

================================ ===================================================
Optimiser                        Seeds consumed
================================ ===================================================
``DLIB:GFS``                     All K (forwarded as ``initial_function_evals``).
``DLIB:BFGS``                    First only (``optim_init.front()``).
Any ``NLOPT`` algorithm          First only.
``TADAH:RANDOM``, ``TADAH:GRID`` First only — used as eval 0, then sweep.
``TADAH:ANNEAL``                 First only — used as the SA chain's starting state after T0 calibration.
``TADAH:ANNEAL_PT``/``_PA``      First only — the coldest slot starts there and the
                                 rest are displaced around it under
                                 ``INIT_MODE JITTER`` (default; sigma from
                                 ``STEP_FRAC``, an explicit fraction, or measured
                                 per dimension by ``JITTER AUTO <p>``). ANNEAL_PT
                                 additionally scales the displacement along the
                                 ladder — see
                                 :ref:`ladder-scaled initial jitter
                                 <anneal_pt_init_jitter_ladder>`.
``ASA:ASA``                      First only — becomes ASA's
                                 ``parameter_initial_final`` and auto-sets
                                 ``User_Initial_Parameters 1``
                                 (:ref:`asa_optimizer`).
================================ ===================================================

``STRATEGY TYPE MULTISTART`` runs the optimiser once per starting point,
``min(N_STARTS, number of points)`` times.  ``DLIB:GFS`` collapses to a
single call because GFS already takes every seed natively; ``TYPE
PRESTAGE`` feeds its ``TOPK`` candidates to the refinement the same
way.

.. _search_space_scaling:

Search-space scaling
....................

Every ``OPTIM`` dimension has a *real* box ``[lo, hi]`` (what the
configuration and the model see) and an *internal* coordinate ``u`` that
the optimiser iterates on.  One object, the search space, owns the map:

.. code-block:: none

   t = T(x)                                   per-dimension shape: LIN  t = x
                                                                   LOG  t = ln x
                                                                   ASINH c  t = asinh(x / c)
   u = (t - T(lo)) / (T(hi) - T(lo))          OPTIM_SCALE AUTO  (unit box [0,1])
   u = t                                      OPTIM_SCALE NONE  (raw units; ln for LOG dims)

``OPTIM_SCALE AUTO | NONE`` is a root-level key (next to the ``OPTIM``
lines).  Under ``AUTO`` every dimension spans ``[0, 1]``, so the
optimiser-level controls ``STEP``, ``XTOL_ABS`` and ``PARAM fd_step``
mean the same fraction of every box regardless of the keys' units --
the point of the transform: real searches mix box widths across five to
nine decades (knot positions in Angstrom next to amplitude coefficients
of order ``1e-3`` and pinned linear weights of order ``1e4``), and every
derivative-free algorithm assumes its variables are commensurate.
Under ``NONE`` the internal coordinates are the pre-1.5 ones (raw
units, ``ln`` for ``LOG`` dims) -- the escape hatch for A/B checks of the
coordinate system, also reachable through the environment override
``TADAH_OPTIM_SCALE=NONE``.  Runs are not byte-identical to those of
earlier releases, however: numbers are no longer rounded on their way
through the code (grid values, for example), which shifts losses at
about the ``1e-13`` relative level, and starting points are evaluated at
exactly their loaded values (see below; this matters for ``LOG``/``ASINH``
dimensions, whose ``exp``/``log`` round trip is not exact, and for
optimisers that round their start point).  The default is ``AUTO``.

``LOG_HP <ratio>`` (a finite ratio ``>= 1``, declared once at the
``OPTIMIZER`` or ``STRATEGY`` level; rejected inside
``EXPLORE``/``REFINE``/``INNER``/``INIT`` blocks) tags every untagged
dimension with ``low > 0`` and ``high/low >= ratio`` as ``LOG``; an
explicit tag on the ``OPTIM`` line always wins.  Default: no
auto-tagging.

All internal arithmetic happens in ``u``; the model is always evaluated
at the real point; ``best_*.tadah``, ``params.tadah`` and the WARM seed
files are real-space.  The ``SEARCH SPACE`` section of ``hpo_run.log``
prints the policy, the real width ratio across dimensions, and one row
per dimension with its scale, box origin, real and internal bounds and
the starting point in both spaces.

Starting points are evaluated exactly.  The round trip ``x -> u -> x``
is not the identity in floating point: ``lo + u*(hi - lo)`` rounds (in a
``[-300, 300]`` box a weight of ``-2.87486722631648e-4`` comes back
with a relative error of ``3e-11`` and ``1.2e-14`` comes back as ``0``),
and ``LOG``/``ASINH`` add ``exp``/``log`` rounding.  Every optimiser run
therefore maps the internal coordinate of each of its starting points
back to exactly the real value it came from, dimension by dimension:
the ``CONFIG`` and ``WARM`` values, every other ``INIT`` chain point,
``MULTISTART`` starts, ``RESTART`` refreshes and ``PRESTAGE`` top-K
seeds.  Whenever the optimiser evaluates one of its starting points
(most do so first; ``ASA`` after its initial cost-temperature samples),
the model sees the loaded doubles bit for bit, whatever the box
(explicit, ``AUTO`` ``FRAC``/``RATIO``/``FLOOR``/``GROUP_FLOOR``,
``LIN``/``LOG``/``ASINH``, either ``OPTIM_SCALE``; one exception under
``NONE`` below), and a coordinate the optimiser has not moved keeps its
seed value in later evaluations.  This includes optimisers that round the
start point before evaluating it (NLopt's ``BOBYQA`` and ``COBYLA``
rescale it when the initial steps differ between dimensions, ``PRAXIS``
maps it through its bound transform), whose first evaluation lands a few
units in the last place away from the seed's ``u``: any ``u`` within 4
units in the last place of the internal box's largest magnitude is
evaluated at the seed.  The window is absolute -- ``4 x 2.2e-16`` of the
box width under ``AUTO``, capped at ``2^-27`` (``7.5e-9``) but never below
2 units in the last place of the seed's own ``u`` -- so it replaces only
a move smaller than itself away from a seed, which the plain map resolves
where it is finer than the box (a ``[0, W]`` box near ``0``, or any
linear dimension under ``NONE``).  No optimiser moves that little on
purpose: steps and finite-difference steps are far larger (with the
default ``PARAM fd_step`` of ``1e-6``, at least ``1e-6`` in internal
units, or ``1.5e-8`` with Ceres' step rule, whose smallest step is what
caps the window), and an annealing proposal that small evaluates the
seed again; only a ``PARAM fd_step`` set by hand below the window (under
``NONE`` with ``fd_kernel inhouse`` on a linear box reaching ``1.7e7``:
below about ``7.5e-9``) gives a zero derivative at the seed.  The cap
matters only under ``NONE`` for a linear box reaching beyond ``1.7e7``:
there ``PRAXIS``'s start can be evaluated a few units in the last place
of the box off a seed of smaller magnitude than the box (the exception
above).  ``PRAXIS`` rounds its start by a few units in the last place of
the box's midpoint and half-width, while the capped window is only
``2^-27`` or 2 units in the last place of the seed; measured with NLopt
2.10 this happens for seeds up to about 0.3 of ``max(|lo|, |hi|)`` on a
``[0, W]`` box and up to about 0.6 (rarely 0.8) on a box straddling
``0``, never from 0.9 of it on, and only on boxes of ``2^25``
(``3.4e7``) or more.  Any other point maps as before.  Two limits: when
two seeds of one run are closer in a dimension than the box resolves
(e.g. ``0`` and ``1e-20`` in ``[-300, 300]``), the first one listed wins;
and a start point an optimiser moves on purpose is evaluated where it
was moved to (``PRAXIS`` moves a start that lies on a bound ``1e-12`` of
the half-width inside; ``BOBYQA`` moves a start closer to a bound than
its initial step onto the bound or one step away from it).

.. _strategy_block:

STRATEGY block
--------------

The ``STRATEGY { ... } ENDSTRATEGY`` block is the run-policy hub.  Its
``TYPE`` selects the dispatch shape:

================== =================================================================
``TYPE``           Effect
================== =================================================================
``PLAIN``          Single-shot run of the driver named by ``LIB``/``ALGO``.
``MULTISTART``     Run the driver ``N_STARTS`` independent times, each from a
                   different point of the ``INIT`` chain.
``PRESTAGE``       Two-stage: an ``EXPLORE { ... }`` block runs first, the top-K
                   candidates are then refined by the STRATEGY-level driver
                   (optionally written as a ``REFINE { ... }`` block).
================== =================================================================

Any ``TYPE`` can be decorated with ``RESTART`` (:ref:`restart_decorator`).
The optimiser named by ``LIB``/``ALGO`` may be local or global; the
``TYPE`` only decides how often, and from which points, it runs.

.. list-table:: STRATEGY-level keys besides the driver attributes
   :header-rows: 1
   :widths: 22 78

   * - Key
     - Effect
   * - ``TYPE``
     - ``PLAIN`` (default), ``MULTISTART`` or ``PRESTAGE``.
   * - ``LOG_HP <ratio>``
     - Automatic ``LOG`` tagging of wide positive dimensions
       (:ref:`search_space_scaling`).
   * - ``N_STARTS``, ``PARALLEL``, ``BUDGET_PER_START``
     - ``MULTISTART`` knobs (below); rejected under another ``TYPE``.
   * - ``TOPK``
     - ``PRESTAGE`` knob (below); rejected under another ``TYPE``.
   * - ``EXPLORE``, ``REFINE``, ``INNER``, ``RESTART``
     - Nested blocks (below).

A knob of one ``TYPE`` would have no effect under another, so it is an
error naming its own line.  ``TYPE`` may come before or after the knob;
the ``TYPE`` in force when the block ends decides, and a block without a
``TYPE`` line is ``PLAIN``:

.. code-block:: none

   Error on line 6: N_STARTS is not accepted with STRATEGY TYPE PLAIN: only TYPE MULTISTART runs N_STARTS starts, so it would have no effect; set TYPE MULTISTART or remove N_STARTS
   Line:     N_STARTS 3

The same holds for the ``EXPLORE`` and ``REFINE`` blocks: they need
``TYPE PRESTAGE`` before them, and a later ``TYPE`` line of the block
that changes it is an error at the ``EXPLORE`` (``REFINE``) line
(``EXPLORE block is only valid with TYPE PRESTAGE, but the STRATEGY block
ends with TYPE PLAIN (the last TYPE line decides); …``).  Before 1.6.0
the run silently became ``PLAIN``, without the exploration.  An
``OPTIMIZER`` block has one ``STRATEGY`` block: a second one is an error
at its line (``STRATEGY block given more than once (the first opens on
line 4); write every STRATEGY key in one block``); it reopened the first
and could change the ``TYPE`` its keys had been checked against.

TYPE PLAIN
..........

The simplest dispatch: a single run of ``LIB``/``ALGO``.  The driver
attribute table above lists every accepted key.  No nested blocks
other than ``RESTART`` and ``INNER`` (for NLopt MLSL/AUGLAG) are
relevant.

TYPE MULTISTART
...............

Runs the inner driver ``N_STARTS`` independent times.  The host's
best-tracker keeps the global minimum across all runs.

.. list-table::
   :header-rows: 1
   :widths: 22 16 14 48

   * - Key
     - Args
     - Default
     - Effect
   * - ``N_STARTS``
     - int ≥ 1
     - ``1``
     - Number of independent starts.
   * - ``PARALLEL``
     - int ≥ 1
     - ``1``
     - Concurrent starts (currently parsed and validated; runs
       sequentially with a stderr warning if ``PARALLEL > 1``).
   * - ``BUDGET_PER_START``
     - int, 1 to 2\ :sup:`31` − 1
     - derived
     - Per-start ``MAXEVAL`` override, in ``MAXEVAL``'s range: ``0`` is
       an error at its line (``BUDGET_PER_START must be an integer from 1
       to 2147483647, got '0'``).  When unset HPO splits the outer
       ``MAXEVAL`` over the starts that run (ceil division); with
       neither, the starts run without an evaluation limit.  A multi-seed
       optimiser (``DLIB:GFS``) runs once from every ``INIT`` point with
       the outer ``MAXEVAL``, so ``BUDGET_PER_START`` would have no effect
       there: it is a startup error (``BUDGET_PER_START has no effect
       with DLIB:GFS, which runs every start in one optimiser; use
       MAXEVAL``); before 1.6.0 it was ignored.  The startup checks see
       each start's budget: ``MULTISTART`` over ``NLOPT:LN_SBPLX`` with
       ``BUDGET_PER_START 5`` and no ``MAXEVAL`` is valid.

The number of starts is ``min(N_STARTS, number of INIT points)``; when the
``INIT`` chain yields fewer points than ``N_STARTS`` a warning says so,
so pair ``MULTISTART`` with an ``INIT`` chain of at least ``N_STARTS``
points (e.g. ``METHOD LHS 8``).  With ``SEED`` set, start ``i`` uses
``SEED + i`` so stochastic optimisers do not repeat one another; after
restart *r* of a ``RESTART``, ``SEED + r·N_STARTS + i``
(:ref:`restart_decorator`).  An NLopt start draws from that seed even
when the ``INIT`` block has a ``SEED``, which draws the start points, and
so does the ``INNER`` search of an AUGLAG start (:ref:`hpo_inner_block`).

A start that fails on its own (a rejected setup, an optimiser error) is
reported as ``MULTISTART start i/K failed: <error>`` — a ``WARNING`` in
``hpo_run.log`` and on stderr — and the next start runs; the final status
then notes ``(N of M MULTISTART starts failed; see the WARNINGs)``, counted
over the whole run: after a :ref:`RESTART <restart_decorator>` relaunch it
reads ``(N of M MULTISTART starts failed across L launches; ...)``, and it
is also appended to RESTART's ``STOPPED -- stagnation: ...`` status.  When
every start of a launch fails, the run fails like a single run would
(``FAILED -- <first error>``, exit status ``1``).  Two stops end the whole
``MULTISTART`` at once, because they concern the run rather than the
start: a signal (:ref:`hpo_stopping`) and a failure to save
``best_pot.tadah`` (:ref:`hpo_potential_archives`).  Under ``RESTART``
each start runs in a stagnation window of its own: a start that runs out
of it ends (``MULTISTART start i/K done: ...; stopped on stagnation``) and
the next start runs (:ref:`restart_decorator`).

``MULTISTART`` rejects — for any ``N_STARTS``, including ``1`` — every
*global* backend that consumes a single starting point: every NLopt
algorithm whose name starts with ``G`` (``GN_*``, ``GD_*``,
``G_MLSL*``), ``TADAH:RANDOM``, ``TADAH:GRID``, the ``TADAH:ANNEAL``
family and ``ASA:ASA``.  Local optimisers (``NLOPT:LN_*``/``LD_*``,
``DLIB:BFGS``, CERES; not the refused ``LN_NEWUOA_BOUND`` and
``NLOPT_LD_LBFGS_NOCEDAL``) and the
multi-seed global ``DLIB:GFS`` are accepted; ``DLIB:GFS`` consumes all points in one run.  Use ``TYPE
PLAIN`` to run a single-seed global once.  The check is a startup
error, raised before any data is loaded or any evaluation runs.  The
message lists the first accepted backends in alphabetical order (the
list depends on the libraries the build includes):

.. code-block:: none

   HPOEngine error: STRATEGY TYPE MULTISTART cannot run TADAH:ANNEAL: it is a global optimiser that consumes a single starting point, so restarting it from each INIT point is rejected for any N_STARTS. Use a local or multi-seed optimiser (CERES:LBFGS, CERES:NLCG, CERES:STEEPEST_DESCENT, CERES:WOLFE, DLIB:BFGS, DLIB:GFS, ... (23 more)), or STRATEGY TYPE PLAIN to run TADAH:ANNEAL once.

TYPE PRESTAGE — staged global → local
.....................................

A self-contained ``EXPLORE { ... } ENDEXPLORE`` sub-block describes the
*first* (exploration) phase; the STRATEGY-level ``LIB``/``ALGO`` drives
the *second* (refinement) phase over the top-K candidates harvested by
the EXPLORE pass.  The refinement optimiser may equally be written as a
``REFINE { ... } ENDREFINE`` block — it is optional and configures the
same STRATEGY-level optimiser.

.. code-block:: none

   STRATEGY
     TYPE PRESTAGE
     LIB DLIB
     ALGO BFGS              # local refinement, runs SECOND
     MAXEVAL 60
     TOPK 3
     EXPLORE
       LIB DLIB
       ALGO GFS             # global exploration, runs FIRST
       MAXEVAL 200
     ENDEXPLORE
   ENDSTRATEGY

================ ============= =============================================================
Key              Args          Effect
================ ============= =============================================================
``TOPK``         int ≥ 1       Number of best (loss, params) pairs the EXPLORE phase
                 (default 1)   forwards to refinement.  With one candidate the refinement
                               runs once from it; with more it is a multistart over the
                               candidates, so the refinement optimiser must be one
                               ``MULTISTART`` accepts, per-start ``MAXEVAL`` is split
                               from the STRATEGY-level ``MAXEVAL`` by the same rule, and
                               candidate ``i`` runs with ``SEED + i``.  A ``TOPK`` above
                               1 with a single-seed global refinement optimiser is a
                               startup error, whatever number of candidates the
                               exploration would harvest (``TOPK 1`` refines the best
                               one once).  Only valid with ``TYPE PRESTAGE``.
================ ============= =============================================================

The ``EXPLORE`` block accepts the full driver-attribute set
(``LIB``/``ALGO``/``MAXEVAL`` / …) and an optional ``INIT`` block; it
inherits the search space automatically and, without its own ``INIT``
block, starts from the OPTIMIZER-level ``INIT`` points.  Candidates are
deduplicated so the ``TOPK`` seeds are distinct.  If the exploration
yields no candidate, the refinement runs from the OPTIMIZER-level
``INIT`` points instead.

With ``RESTART``, stagnation during the exploration ends it early and
hands its top-K to the refinement, whose seeds each run in a stagnation
window of its own; when a refinement seed stagnated, the whole
``PRESTAGE`` restarts after the refinement's last seed, exploration first,
whether or not the refinement improved the global best.  The restarted exploration starts from
the restart's fresh points, unless the ``EXPLORE`` block has an ``INIT``
block of its own.  In restart *r* a seeded exploration draws from
``SEED + r`` and the refinement's candidate ``i`` from ``SEED + r·TOPK + i``
of the STRATEGY-level ``SEED`` (:ref:`restart_decorator`).

LIB TADAH — the annealing family
................................

The Monte-Carlo optimizers developed in-tree have their own reference
pages under :doc:`optimizer`:

* :ref:`tadah_anneal_block` — serial simulated annealing, classical and
  Corana-adaptive, with automatic temperature and chain-length
  calibration;
* :ref:`tadah_anneal_pt_block` — parallel tempering (replica exchange),
  with a reference manual and a step-by-step setup guide;
* :ref:`tadah_anneal_pa_block` — population annealing;
* :ref:`asa_optimizer` — Ingber's Adaptive Simulated Annealing.

The :ref:`shared vocabulary <anneal_vocabulary>` used by those pages
and by ``hpo_run.log`` is defined once on the optimizer page.

.. _tadah_anneal_mpi:

Multi-node runs (TADAH_HPO_MPI) and ARCHER2
...........................................

Both parallel drivers scale past one node through the opt-in
``-DTADAH_HPO_MPI=ON`` CMake option: replicas/walkers distribute over
MPI ranks in contiguous slot blocks, every rank trains locally and runs its
LAMMPS instances on ``MPI_COMM_SELF``.  PT boundary exchanges communicate
``f`` and the full parameter vector ``x``; PA gathers losses and, when
resampling, gathers/scatters full ``(f, x, V)`` walker states.  Control and
best-broadcast messages also travel; training databases and model fitting
remain rank-local.  The same config file can run serially, threaded, or
under ``srun``/``mpirun`` provided the selected driver's global ``REPLICAS`` or ``POPULATION``
count is at least the chosen rank count.
Requirements and behaviour:

* a multi-rank launch accepts only a plain ``TADAH:ANNEAL_PT`` or
  ``TADAH:ANNEAL_PA`` strategy.  Other drivers and ``STRATEGY TYPE
  MULTISTART`` / ``PRESTAGE`` fail before policy setup; an MPI launch of
  size one keeps the normal serial behaviour;
* a multi-rank launch rejects ``RESTART`` (:ref:`restart_decorator`):
  each rank tracks stagnation over its own evaluations, so the ranks
  would restart at different times and the run would hang in the
  driver's next collective.  With ``AFTER_STAGNATION`` and ``MAX`` both
  positive it fails before any evaluation, with ``multi-rank HPO does not
  support RESTART (…): stagnation is tracked per rank; run on one rank or
  remove RESTART``;
* an MPI-flavoured LAMMPS library (the serial archive carries MPI stub
  symbols that collide with a real MPI at link time — configure-time
  check);
* ``MPI_THREAD_MULTIPLE`` (evaluation threads run LAMMPS concurrently);
  on Cray MPICH: ``export MPICH_MAX_THREAD_SAFETY=multiple``;
* the global ``REPLICAS`` (PT) or ``POPULATION`` (PA) count must be at
  least the MPI rank count;
* the MPI C interfaces use ``int`` element counts.  PT and PA therefore
  require ``D + 2 <= INT_MAX`` for their calibration packets; PT additionally
  requires ``D <= INT_MAX`` for boundary exchanges.  PA requires
  ``POPULATION <= INT_MAX`` and
  ``POPULATION * (2*D + 1) <= INT_MAX`` for gathered/scattered states.
  In a multi-rank run, PA rejects these sizes before dimension-sized driver
  allocation, host evaluation, or communication;
* rank-local fixed-name outputs carry an explicit ``.r<rank>`` suffix
  on **every** rank, including rank 0;
* the unsuffixed ``best_pot.tadah`` is a separate canonical global-best
  file, updated live by every rank under a cross-process lock.  A
  complete potential replaces it for a lower loss, or for an exactly equal
  loss and a lexicographically smaller ``(rank, slot, evaluation)`` key;
* ``MAXEVAL`` is a hard global cap across seed/probe evaluation,
  population initialisation, annealing, and finalisation.  No rank-0
  re-evaluation is performed after the budget is exhausted;
* at exit the ranks meet in a barrier of their own.  Ranks that all
  finish — normally or failing together — exit normally, each with its
  journal complete.  A rank that ends with a non-zero status and is not
  joined by every other rank within 30 s aborts the whole job
  (``MPI_Abort`` with its exit status; its own ``hpo_run.log.r<rank>`` is
  already closed): the missing ranks are blocked in a collective the
  failed rank left — after a failure confined to one rank, such as a best
  potential it could not save during an ``INIT_MODE JITTER AUTO`` probe or
  ``NOISE_CALIBRATION`` — and would otherwise hold the allocation until it
  ends.  Their journals then end without a RESULT section.

``TADAH_HPO_MPI`` is the only MPI build of Tadah!MLIP: the legacy MPI
training build (``TADAH_BUILD_MPI``) was removed in 1.6.0, and
configuring with ``-DTADAH_BUILD_MPI=ON`` is an error
(:ref:`installation_hpo_mpi`).

ARCHER2 sketch (128 cores/node, 2x AMD EPYC 7742; inter-node
communication requires MPI; OpenMP cannot span nodes):

.. code-block:: bash

   #!/bin/bash
   #SBATCH --nodes=2
   #SBATCH --ntasks-per-node=8        # MPI ranks (replica blocks)
   #SBATCH --cpus-per-task=16         # shared-pool workers per rank
   #SBATCH --time=01:00:00
   export OMP_NUM_THREADS=1           # residual OpenMP kernels only
   export MPICH_MAX_THREAD_SAFETY=multiple
   # THREADS 16 in STRATEGY: one 16-worker shared pool per rank
   srun --distribution=block:block --hint=nomultithread \
        tadah hpo --hpotarget config.hpo -c config.train

Size the main worker pool as ``ranks * THREADS <= total cores``.
``OMP_NUM_THREADS`` is an independent width for short residual kernels,
not a factor in that equation; start at 1 for PT/PA and benchmark it
separately.  ``REPLICAS`` (PT) and ``POPULATION`` (PA) must each be at least the
rank count when their respective driver is selected; multiples distribute
slots evenly but are not required.

.. _restart_decorator:

RESTART decorator
-----------------

``RESTART { ... } ENDRESTART`` decorates a ``TYPE PLAIN`` /
``MULTISTART`` / ``PRESTAGE`` strategy with a stagnation watchdog.

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB DLIB
     ALGO BFGS
     MAXEVAL 1000
     RESTART
       AFTER_STAGNATION 80
       MAX 5
       METHOD SOBOL 1
     ENDRESTART
   ENDSTRATEGY

.. list-table::
   :header-rows: 1
   :widths: 22 16 62

   * - Key
     - Args / range
     - Effect
   * - ``AFTER_STAGNATION``
     - int ≥ 0 (default ``0``)
     - After this many consecutive evals without a new global best,
       abort the inner run and restart from a fresh point.
   * - ``MAX``
     - int ≥ 0 (default ``0``)
     - Cap on the number of restarts.
   * - ``METHOD`` (alias ``STRATEGY``)
     - repeatable
     - Init chain used to draw the fresh point(s) on each restart
       (the ``INIT``-block grammar, including ``K_TOTAL``).  Implicit
       ``CONFIG`` is *not* auto-prepended here: a stagnated run already
       has CONFIG in its history.  Without a ``METHOD`` line a restart
       uses the single ``CONFIG`` point.  Its seeds (``CONFIG`` and
       ``WARM``) are resolved at startup when the block is active: a
       ``WARM`` file that is missing or unreadable, or whose ``RHOFORM``
       the run refuses, is a startup error (it was one only when the
       first stagnation drew it), the ``EMBF0`` note comes at startup, and
       the density gates check these points as seeds
       (:ref:`hpo_feasibility_gates`).
   * - ``SEED``
     - int ≥ 0 (default: none)
     - Seed of the ``METHOD`` chain: restart *r* (``1`` … ``MAX``) draws
       its fresh points with ``SEED + r``, so the restart points of a run
       are reproducible (with ``SOBOL``, block ``SEED + r`` of the
       sequence).  Without ``SEED`` each restart draws from
       ``std::random_device``.

The decorator is active only when **both** ``AFTER_STAGNATION`` and
``MAX`` are positive; with either at its default ``0`` the ``RESTART``
block has no effect.  An active ``RESTART`` ends the run: once its
``MAX`` restarts are used, the next ``AFTER_STAGNATION`` evaluations
without a new best stop it (``STOPPED -- stagnation: no new best in W
evals after R restart(s)``), so it is the stop of a backend that needs one
(:ref:`hpo_stop_criteria`; not of ``GD_STOGO`` and ``GD_STOGO_RAND``,
which need ``MAXEVAL``): ``NLOPT GN_DIRECT_L`` with no stop of its
own under ``AFTER_STAGNATION 60`` and ``MAX 1`` ran 699 evaluations on
the examples' data.  A ``SEED`` inside ``RESTART`` seeds its ``METHOD``
chain: restart *r* draws its fresh points with ``SEED + r`` (with
``SEED 42``, restart 1 draws with 43 and restart 2 with 44).  With
``METHOD SOBOL 3`` these are blocks 43 and 44 of the Sobol sequence,
points 129–131 and 132–134, so no two rounds draw the same point; an
``INIT`` block with ``METHOD SOBOL 3`` and ``SEED 42`` draws block 42,
points 126–128 (:ref:`INIT methods <hpo_init_methods>`).  The first
round does not use the ``RESTART`` chain, so ``SEED`` leaves it
unchanged.  Without a ``SEED`` each restart draws from
``std::random_device``, so its fresh points are not reproducible.  Like
an ``INIT`` block's ``SEED``, it needs a ``RANDOM``, ``LHS`` or
``SOBOL`` method to draw from: with only ``CONFIG`` or ``WARM`` (or no
``METHOD`` line) a startup warning says the seed does not affect the
restart points.  (Before 1.6.0 the ``SEED`` was accepted and ignored.)
Each restart gets its own stagnation window — the windows do not bleed
into each other.  The window counts evaluations since the last new
*global* best, so a restart must beat the incumbent within
``AFTER_STAGNATION`` evaluations to keep going.  ``MAXEVAL`` applies to
each launch, so a run can use up to ``(MAX + 1) x MAXEVAL`` evaluations.  ``RESTART`` runs on one MPI rank only: a launch on more
than one rank rejects it at startup (:ref:`tadah_anneal_mpi`).

A restart re-runs the whole strategy from the *K* fresh points the
``METHOD`` chain draws (``K_TOTAL``, or the chain's default):

* ``PLAIN``: the optimiser starts again from the first of them (a
  multi-seed optimiser such as ``DLIB:GFS`` takes all *K*).
* ``MULTISTART``: every start runs in a stagnation window of its own; a
  start that runs out of it ends and the next start runs.  After the
  last start, if any start stagnated, the batch reports stagnation and
  the restart runs the ``MULTISTART`` again from its first start, with
  ``min(N_STARTS, K)`` starts, one per fresh point — use a ``METHOD`` with
  *K* ≥ ``N_STARTS`` (e.g. ``METHOD LHS 8``) to re-run every start.
  Without a ``METHOD`` line, a restart runs a single start, from
  ``CONFIG``.
* ``PRESTAGE``: a stagnating exploration hands over to the refinement
  early, and every refinement seed runs in a window of its own.  After
  the last seed, if any seed stagnated, the run restarts both phases —
  also when the refinement improved the global best before it stagnated,
  as a ``PLAIN`` run that improved and then stagnated restarts
  (``REFINEMENT: 1 of 2 seed(s) stopped on stagnation (the refinement
  improved the global best); reporting stagnation to RESTART``).  The
  exploration then starts again from the fresh points, or from its own
  points again when the ``EXPLORE`` block has an ``INIT`` block of its
  own; the refinement then refines the new top-K.

Under an active ``RESTART`` a stagnation therefore always restarts the
strategy until the ``MAX`` restarts are used; no strategy ends a run on a
stagnation it absorbed, except the ``PRESTAGE`` exploration, which hands
over to the refinement.

Restart *r* (``1`` … ``MAX``) also gives every optimiser that has a
``SEED`` a seed of its own, so a seeded stochastic optimiser or
exploration draws new random numbers instead of repeating round 0's from
the new point:

* the ``STRATEGY``-level optimiser runs with ``SEED + r·S``, where *S*
  is the number of starts the strategy runs from its ``SEED``:
  ``N_STARTS`` under ``MULTISTART``, ``TOPK`` under ``PRESTAGE`` (the
  refinement's candidates) and ``1`` under ``PLAIN``.  Start *i* of the
  round, which uses the round's seed ``+ i``, gets ``SEED + r·S + i`` — a
  seed no other start of the run uses.  With ``SEED 7``, ``TOPK 3`` and
  ``MAX 2``, the refinement's candidates run with seeds 7, 8, 9, then 10,
  11, 12, then 13, 14, 15;
* an ``EXPLORE`` optimiser runs with its ``SEED + r``.

An ``INNER`` block has no ``SEED`` of its own: its search draws from the
random stream of the enclosing optimiser's run, seeded with that
optimiser's derived seed (:ref:`hpo_inner_block`).

The first round keeps every ``SEED``, so up to its first restart a run
draws the same random numbers as without ``RESTART``.  Under
``PRESTAGE`` the first round can still take another course: a stagnating
exploration hands over to the refinement early (above), where without
``RESTART`` it runs to its own stop criteria.  The journal's
``RESTART r/MAX`` line lists the round's seeds: the seed its fresh
points were drawn with, when the ``RESTART`` block has a ``SEED``, then
the optimisers' derived seeds.  A ``PRESTAGE`` with ``TOPK 1``,
``SEED 42`` on its refinement, ``SEED 1717`` on its ``EXPLORE`` block
and ``SEED 11`` in its ``RESTART`` block logs, at the first restart,
``RESTART 1/3 -- fresh init from SOBOL; RESTART SEED 12, SEED 43,
EXPLORE SEED 1718``.  The seeds are separate settings: the ``RESTART``
block's ``SEED`` draws the points, and each optimiser's own ``SEED`` its
random numbers.  Give them different values.  The ``RANDOM`` method and
``TADAH:RANDOM`` draw from the same generator, so with equal seeds a
restart's fresh ``RANDOM`` point is the optimiser's first draw of the
round, evaluated twice (with ``SEED 5`` in both blocks under ``PLAIN``,
round *r* seeds both with ``5 + r``).  The same can happen in the first
round when an ``INIT`` block's ``SEED`` equals the optimiser's.  An
optimiser without ``SEED`` is unaffected.  NLopt draws from the derived seeds too, even when an
``INIT`` block also has a ``SEED``.  Only an NLopt optimiser without a
``SEED`` of its own takes its random seed from its ``INIT`` block's
``SEED``, which is not derived, so it repeats its random sequence in every
round.

``RESTART`` fires with every backend, including NLopt (also
inside the ``INNER`` local search of MLSL/AUGLAG), ``DLIB:GFS`` with any
``THREADS``, Ceres, ASA and the ``TADAH`` annealers.  Once the ``MAX``
restarts are used up, the next stagnation ends the run normally (exit
status ``0``): the journal records ``RESTART: stagnation reached, max
restarts (MAX) used; stopping``, and its trailer reads ``STOPPED --
stagnation: no new best in W evals after MAX restart(s)`` (*W* is
``AFTER_STAGNATION``; under ``MULTISTART`` followed by the failed-starts
note when a start failed).

.. _hpo_noise_block:

NOISE block
-----------

``NOISE { ... } ENDNOISE`` controls how HPO handles objective noise.

.. code-block:: none

   NOISE
     AUTO
     CALIBRATION 0
   ENDNOISE

================== ============= =================================================================
Key                Args          Effect
================== ============= =================================================================
``AUTO``           bool          Auto-derive the finite-difference step ``PARAM fd_step``
                                 from the search-space geometry:
                                 ``fd_step = max(1e-3 × diag / sqrt(dim), 1e-6)``,
                                 where ``diag`` is the L2 norm of ``high − low`` in
                                 internal coordinates, for the ``STRATEGY``-level
                                 optimiser (each ``MULTISTART`` start, the
                                 ``PRESTAGE`` refinement) when it takes
                                 finite-difference gradients — a gradient-based
                                 backend, or an ``MLSL`` / ``AUGLAG`` whose local
                                 search is one, under ``GRAD_STRATEGY NUMERICAL`` or
                                 ``HYBRID``; the journal has an ``INFO`` line
                                 ``NOISE_AUTO: PARAM fd_step set to …``.  Beside a
                                 ``PARAM fd_step`` written in the block it has no
                                 effect and is refused on the phase concerned
                                 (``… NOISE AUTO has no effect: PARAM fd_step 0.0001
                                 is set; remove one of them.``); before 1.6.0 the
                                 written step was taken, with a note on stderr.  Where that
                                 optimiser measures no slopes — a derivative-free one,
                                 ``ASA`` (its own probes use ``Delta_X``), or
                                 ``GRAD_STRATEGY ANALYTICAL``, written or resolved —
                                 ``AUTO`` would have no effect and is refused at
                                 startup (``the STRATEGY-level optimiser
                                 NLOPT:LN_SBPLX: NOISE AUTO has no effect:
                                 NLOPT:LN_SBPLX measures no slopes; remove AUTO from
                                 the NOISE block.``); before 1.6.0 it was ignored with
                                 a note on stderr, and an ``MLSL`` / ``AUGLAG`` block
                                 got no step from it.  Bareword ``AUTO`` parses as
                                 ``true``; default off.  The derived step must fit
                                 every ``OPTIM`` box like any other: under
                                 ``OPTIM_SCALE NONE`` (Ceres' rule, relative to
                                 ``|u|``) a box much narrower than the others can be
                                 refused at startup, naming ``NOISE AUTO`` as the
                                 step's source and the largest ``fd_step`` that fits,
                                 with the advice to remove ``AUTO`` and set ``PARAM
                                 fd_step`` to at most that (:ref:`hpo_fd_stencil`).
``CALIBRATION``    int (0 or ≥2) Repeat the seed evaluation ``N`` times to surface
                                 the objective's noise floor and emit a suggested
                                 ``FTOL_REL``.  ``0`` disables; ``1`` is rejected as
                                 statistically meaningless.
================== ============= =================================================================

.. _hpo_inner_block:

INNER block — NLopt MLSL / AUGLAG sub-optimiser
-----------------------------------------------

NLopt's MLSL / G_MLSL_LDS / AUGLAG drivers delegate inner work to a
local optimiser.  Configure the inner via:

.. code-block:: none

   STRATEGY
     TYPE PLAIN
     LIB NLOPT
     ALGO G_MLSL_LDS
     MAXEVAL 400
     INNER
       LIB NLOPT
       ALGO LN_BOBYQA
       MAXEVAL 50
     ENDINNER
   ENDSTRATEGY

Only the driver-attribute keys (``LIB`` / ``ALGO`` / ``MAXEVAL`` /
``MAXTIME`` / ``FTOL_*`` / ``XTOL_*`` / ``POPULATION`` / ``STEP`` /
``VECTOR_ARR`` / ``THREADS`` / ``PARAM``) are valid inside ``INNER``;
``SEED``, ``GRAD_STRATEGY`` and ``STOPVAL`` belong to the enclosing block
(see below), and the ``INNER`` search refuses, like any phase, an
attribute its algorithm never reads (:ref:`hpo_stop_criteria`).  ``LIB``
and ``ALGO`` are required, as in every block that names an optimiser: a
missing one is an error at the ``ENDINNER`` line that names the
enclosing block (``STRATEGY``, ``EXPLORE`` or ``REFINE``):

.. code-block:: none

   Error on line 10: the INNER block inside STRATEGY names no optimiser: ALGO is not set; set LIB and ALGO inside the INNER block (the local optimiser that NLopt's MLSL and AUGLAG run)
   Line:     ENDINNER

Before 1.6.0 an ``INNER`` block without ``ALGO`` was accepted and failed
inside NLopt once the run had started.

Only NLopt's MLSL and AUGLAG algorithms run an ``INNER`` search, and they
run it with NLopt, so the ``INNER`` block's ``LIB`` is ``NLOPT``.  Both
rules are startup errors, raised before any data is loaded and naming the
optimiser the ``INNER`` block belongs to (the ``STRATEGY``-level
optimiser, the ``PRESTAGE`` refinement or the ``EXPLORE`` block):

.. code-block:: none

   HPOEngine error: DLIB:BFGS does not accept an INNER sub-optimiser (the INNER block of the STRATEGY-level optimiser): only NLopt's MLSL and AUGLAG algorithms run a local search inside their run (NLOPT:AUGLAG, NLOPT:AUGLAG_EQ, NLOPT:GD_MLSL, NLOPT:GD_MLSL_LDS, NLOPT:GN_MLSL, NLOPT:GN_MLSL_LDS, ... (6 more)); remove the INNER block or pick one of them
   HPOEngine error: the INNER block of the STRATEGY-level optimiser names DLIB:BFGS, but NLOPT:G_MLSL_LDS runs its INNER search with NLopt: set LIB NLOPT and an NLopt ALGO (such as LN_BOBYQA) inside the INNER block

Before 1.6.0 the first was reported only once the host had loaded the
data, and an ``INNER`` block with another ``LIB`` failed inside NLopt once
the run had started (``wrong algorithm string``).  With MLSL/AUGLAG an
outer ``STEP`` overrides an ``INNER`` ``STEP`` (NLopt forwards the outer
initial step to the local optimiser); a startup warning says so.

``AUGLAG``, ``AUGLAG_EQ``, ``G_MLSL`` and ``G_MLSL_LDS`` have no default
local optimiser, so they need an ``INNER`` block, and for the same reason
an ``INNER`` block cannot name one of them (``INNER`` blocks do not
nest); ``LN_AUGLAG``, ``LD_AUGLAG``, their ``_EQ`` forms and the
``GN_``/``GD_`` MLSL forms run NLopt's ``LN_COBYLA`` or ``LD_MMA``
without one.  Both are startup errors that name the phase:

.. code-block:: none

   HPOEngine error: NLOPT:AUGLAG needs an INNER block (the PRESTAGE refinement): NLopt has no default local optimiser for it; add INNER ... ENDINNER with LIB NLOPT and a local ALGO such as LN_BOBYQA, or use one of NLOPT:GD_MLSL, NLOPT:GD_MLSL_LDS, NLOPT:GN_MLSL, NLOPT:GN_MLSL_LDS, NLOPT:LD_AUGLAG, NLOPT:LD_AUGLAG_EQ, NLOPT:LN_AUGLAG, NLOPT:LN_AUGLAG_EQ, which run LD_MMA or LN_COBYLA without one
   HPOEngine error: the INNER block of the STRATEGY-level optimiser names NLOPT:G_MLSL_LDS, which needs an INNER block of its own; INNER blocks do not nest: pick a local algorithm such as LN_BOBYQA

Before 1.6.0 NLopt refused the first only once the run had started
(``local optimizer must be specified for AUGLAG``), after the data were
loaded and, under ``PRESTAGE``, after the whole exploration; the second
failed after some evaluations with only ``nlopt invalid argument``.

NLopt hands each ``INNER`` search what is left of the enclosing block's
``MAXEVAL`` and ``MAXTIME`` when they are tighter than the ``INNER``
block's own, and the enclosing block's ``STOPVAL`` in place of the
``INNER`` block's, so ``STOPVAL`` inside ``INNER`` is an error at its
line, like ``SEED``:

.. code-block:: none

   HPOEngine error: Error on line 11: STOPVAL is not accepted inside an INNER block: NLopt's MLSL and AUGLAG replace the INNER search's STOPVAL with the enclosing block's; set STOPVAL on the enclosing STRATEGY block
   Line:       STOPVAL 0.1

Before 1.6.0 it was accepted and had no effect: a ``STOPVAL`` above
every loss did not stop the search, and the journal still listed it.
Tolerances are the other way round: the ``INNER`` block's own are its
search's, and those of the enclosing MLSL or AUGLAG block reach no
``INNER`` search, so they are refused there (``NLOPT:G_MLSL_LDS does not support
FTOL_REL with an INNER block: …; set it in the INNER block, whose local
search tests it``, or, when the ``INNER`` algorithm does not test it
either, the stops that search honours).  NLopt gives an ``INNER`` search under MLSL that sets
none of ``FTOL_REL``, ``FTOL_ABS`` and ``XTOL_REL`` the tolerances
``FTOL_REL 1e-15`` and ``XTOL_REL 1e-7`` (``optimize.c``); before 1.6.0
an ``XTOL_ABS`` on the MLSL block switched these off.  MLSL's own loop
never ends without a stop, and AUGLAG ends when
its ``INNER`` search ends, so an ``INNER`` search that needs a stop
(:ref:`hpo_stop_criteria`) takes it from either block.

The two ``MAXEVAL`` count different things.  The enclosing block's counts
every evaluation of the run, the losses and each gradient's evaluations,
and ends the run when it is spent.  The ``INNER`` block's counts the
NLopt calls of each local search, as NLopt does: one call is a loss and,
for a gradient-based local search, its gradient (2 *n* more evaluations
with ``CENTRAL`` finite differences in *n* dimensions), and HPO cannot
tell where one local search of MLSL ends and the next begins.  The
journal's ``Inner stop criteria`` say so for a gradient-based ``INNER``
search:

.. code-block:: none

   Inner stop criteria:
     Max evaluations     : 10
     (MAXEVAL here counts the NLopt calls of each local search, not its evaluations: a call evaluates the loss and its gradient; the MAXEVAL of the enclosing block counts every evaluation of the run, each loss, each finite-difference point and each analytical gradient)

AUGLAG evaluates the result of its ``INNER`` search once more when that
search ends (NLopt's ``auglag.c``); before 1.6.0 that evaluation could
come after the enclosing block's ``MAXEVAL`` was spent, one more than
it, and it now counts against it like any other.  An ``INNER`` search
that needs a stop and has none in either block is a startup error:

.. code-block:: none

   HPOEngine error: the STRATEGY-level optimiser: the INNER search of NLOPT:AUGLAG (NLOPT:GN_DIRECT_L) needs a stop: NLopt's global search has no convergence test of its own and runs until it is interrupted; set MAXEVAL, MAXTIME or a positive tolerance it honours (FTOL_REL, FTOL_ABS, XTOL_REL, XTOL_ABS) in the INNER block, or MAXEVAL, MAXTIME or STOPVAL in the NLOPT:AUGLAG block, which NLopt hands on to it; DIRECT's XTOL_REL and XTOL_ABS end it only once every rectangle is that small, after roughly (1/XTOL_REL)^n evaluations in n dimensions

``SEED`` is not valid inside ``INNER``.  MLSL and AUGLAG run the
``INNER`` search inside the enclosing block's NLopt run, and NLopt has
one random stream per run.  The enclosing block's ``SEED`` seeds it (or,
without one, the enclosing block's ``INIT``-block ``SEED``), and every
random number of the run comes from it: G_MLSL's sample points and the
draws of the ``INNER`` optimiser, such as a ``GN_CRS2_LM`` population
under AUGLAG.  An ``INNER`` ``SEED`` could only reseed that stream in
place of the enclosing block's seed, so the startup check rejects it and
names the line.  This configuration:

.. code-block:: none

   OPTIMIZER
     INIT
       METHOD LHS 3
     ENDINIT
     STRATEGY
       TYPE MULTISTART
       LIB NLOPT
       ALGO AUGLAG
       N_STARTS 3
       MAXEVAL 300
       SEED 10
       INNER
         LIB NLOPT
         ALGO GN_CRS2_LM
         SEED 20
       ENDINNER
     ENDSTRATEGY
   ENDOPTIMIZER

stops at startup: ``tadah hpo`` prints this error on stderr and exits
with status ``1``:

.. code-block:: none

   HPOEngine error: Error on line 15: SEED is not accepted inside an INNER block: NLopt draws the INNER search from the enclosing run's random stream; set SEED on the enclosing STRATEGY block
   Line:       SEED 20

The message names ``EXPLORE`` or ``REFINE`` when the ``INNER`` block is
there.  Set ``SEED`` on that block instead.  Here, deleting ``SEED 20``
leaves ``SEED 10``: the three starts run with seeds 10, 11 and 12, each
with a ``MAXEVAL`` of 100 (``MAXEVAL 300`` split over ``N_STARTS``), and
each start's ``GN_CRS2_LM`` search draws a population of its own from
its seed.  In the same way the ``PRESTAGE`` refinement runs candidate
*i* with ``SEED + i``, and each ``RESTART`` round runs with a derived seed
(:ref:`restart_decorator`) that its ``INNER`` search draws from too.  An
``INIT`` block is not valid inside ``INNER`` either; MLSL picks its local
starting points itself.

``GRAD_STRATEGY`` is not valid inside ``INNER`` either.  MLSL and AUGLAG
hand their ``INNER`` search the gradient of their own objective, which
the enclosing block obtains with its ``GRAD_STRATEGY``
(:ref:`hpo_optimizer_common`), so a value written in the ``INNER`` block
would never be used; the startup check rejects it and names the line.
This block:

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB NLOPT
       ALGO G_MLSL_LDS
       MAXEVAL 400
       INNER
         LIB NLOPT
         ALGO LD_LBFGS
         MAXEVAL 50
         GRAD_STRATEGY HYBRID
       ENDINNER
     ENDSTRATEGY
   ENDOPTIMIZER

stops at startup with:

.. code-block:: none

   HPOEngine error: Error on line 11: GRAD_STRATEGY is not accepted inside an INNER block: the enclosing block's GRAD_STRATEGY applies to its local search; set GRAD_STRATEGY on the enclosing STRATEGY block
   Line:       GRAD_STRATEGY HYBRID

Move the line to the ``STRATEGY``, ``EXPLORE`` or ``REFINE`` block the
``INNER`` block sits in, which the message names.  Before 1.6.0 it was
accepted and had no effect.

.. _hpo_load_transforms:

Load-time data transforms
-------------------------

The training-side load transforms (``LSCALE``, ``ESHIFT*``,
``EFILTER`` / ``FFILTER``, ``EWEIGHT_TEMP``, ``WDBFILE`` /
``WDBFILE_AUTO``, ``ZERO_COM_FORCE``) are documented under
:ref:`load_transforms`; place them in ``--config`` (the training
configuration). HPO applies them on every inner training run.

Validation-set sanitisation
...........................

The validation set is transformed with a *stripped* context that
contains:

* ``LSCALE`` from the training (master) configuration,
* the **resolved** ``ESHIFT`` from the training run (so
  ``ESHIFT_ATOM`` / ``ESHIFT_DBATOM`` derivations propagate as
  per-Z values; the raw ``*_ATOM`` / ``*_DBATOM`` keys are not
  re-derived on validation),
* ``EFILTER`` and ``FFILTER`` **only when they appear in the
  --validation file itself**.

Training-only knobs — ``EFILTER`` / ``FFILTER`` set in the master
config, ``WDBFILE`` / ``WDBFILE_AUTO``, ``EWEIGHT_TEMP``,
``ZERO_COM_FORCE`` — never reach the validation set: they would
silently change what the validation metric is computed on without
the user opting in, breaking apples-to-apples comparison across HPO
trials.

Per-validation outlier removal is therefore declared in the file you
pass to ``--validation`` directly:

.. code-block:: none

   # validation.in
   DBFILE   val_eos.tadahdb
   EFILTER  -12.0  -2.0       # honoured for validation only
   FFILTER   20.0

``FORCE`` and ``STRESS`` are refused in this file, at their line (also in a
file it includes with ``CONFIG``): validation predicts forces for
``PC_FRMSE`` and stresses for ``PC_SRMSE`` whatever they say
(:ref:`What validation predicts <hpo_validation_follows_metrics>`).  Before 1.6.0 they were accepted
and ignored.

The host emits a single ``INFO`` log line at start-up summarising
the resulting validation set::

   Validation set: 532 configs loaded; dropped 4 by EFILTER, 1 by FFILTER; 527 remain.

The line is unconditional — when no filter is active the dropped
counts are zero and the line still confirms the loaded size, so it
is always clear which configs the validation metric is being
computed on.

.. _output_files:

Output files
------------

Unless you change them, every artefact produced by the outer optimiser
is written to **the directory in which you started** ``tadah hpo``.

Main log files
..............

Each file begins with a ``#`` header describing its columns.  Eligible rows
follow the ``OUTPUT`` cadence.  The first two columns are always the
evaluation (iteration) number and the slot (``1`` for serial drivers; the
replica or walker for ``ANNEAL_PT``/``ANNEAL_PA``).

When embedding successive HPO runs in one process, flush each host before
starting the next. Result batches use the current run's output files,
including when the working directory changes between runs.

=====================  =====================================================================
File                   Columns
=====================  =====================================================================
``loss.tadah``         All enabled individual loss terms, then the total loss (their mean).
``params.tadah``       One column per optimised dimension, labelled ``KEY[n]``, in the
                       optimiser order: by key, then by index (``FIXWEIGHT``: by
                       ``FIXINDEX`` slot; see ``OPTIM``) — not the order of the
                       ``OPTIM`` lines.  Values are in real units.
``outvar.tadah``       Per-evaluation wall time in milliseconds, enabled validation metrics,
                       LAMMPS ``--outvar``/``--varloss`` values, and typed-observable values
                       (``<tag>[k]`` for the *k*-th ``PC_OBSERVABLE`` line, e.g.
                       ``Ecoh[1]``; its loss column is ``L(Ecoh)[1]``).
``stats.tadah``        Per-validation-dataset values and their average for each enabled metric.
=====================  =====================================================================

Every row has the columns of its file's header.  A cell an evaluation did
not compute is NaN: ``nan`` in ``loss.tadah`` and ``stats.tadah`` (as for a
validation dataset that is not measured), ``NaN`` in ``outvar.tadah`` (as
for a variable that cannot be read back, :ref:`pc_lammps_readback`).  That
is so for the ``PC_LAMMPS`` and ``PC_OBSERVABLE`` columns of an evaluation
stopped before them (``--skip-above``/``--skip-rel``, or a loss already at
``FAILSCORE``), for every metric, ``PC_LAMMPS`` and ``PC_OBSERVABLE`` column
of a candidate a feasibility gate rejects before training
(:ref:`hpo_feasibility_gates`), and for the columns an evaluation ended by
an error did not reach.  Before 1.6.0 such a row held only the cells the
evaluation had, so its values did not line up with the header, and a gate
reject showed its metric averages as ``0``.

Cadence and formatting:

============ ========= =================================================================
Key          Default   Effect
============ ========= =================================================================
``OUTPUT N`` ``1``     Write a main-file row when the iteration is divisible by ``N``;
                       integer ``N ≥ 1`` (``0`` is rejected — use a large value to write
                       little).
``DIGITS d`` ``6``     Decimal digits in scientific-notation columns (also the RESULT
                       table in the ``#`` header of potential archives); integer
                       ``1 ≤ d ≤ 15``.
``QFLUSH N`` ``10``    Queued completed results that trigger a writer-thread drain;
                       integer ``N ≥ 0`` (``0`` and ``1`` drain after every result).
                       The end of the run drains the remainder
                       (:ref:`hpo_stopping`).
============ ========= =================================================================

Best-improvement histories
..........................

Every new minimum is numbered by an improvement ordinal ``k = 1, 2, …``
and, following the ``BOUTPUT`` cadence, contributes a row to the
companion history files.  Under multi-rank PT/PA, the suffixed files
record that rank's improvements; the unsuffixed ``best_pot.tadah`` is
the canonical cross-rank winner:

* ``best_loss.tadah``
* ``best_params.tadah``
* ``best_outvar.tadah``
* ``best_stats.tadah``

These files append one row per qualifying improvement; they are histories,
not single-row snapshots.  They have the same columns as their main
counterparts except that the **first column is the improvement ordinal**
``k``, not the evaluation number (the evaluation that produced a best is
recorded in ``hpo_run.log`` and in the header of ``best_pot.tadah``).

.. list-table::
   :header-rows: 1
   :widths: 22 14 64

   * - Key
     - Default
     - Effect
   * - ``BOUTPUT N``
     - ``1``
     - A cadence over new bests: write the ``best_*`` history row of
       every ``N``-th improvement (``k`` divisible by ``N``).  Integer
       ``N ≥ 1``; ``0`` is rejected.  ``best_pot.tadah`` is updated on every
       improvement regardless.

.. _hpo_potential_archives:

Potential archives
..................

The current global best potential is saved live to the canonical
``best_pot.tadah``.  Every process-local improvement synchronously
competes for this file under a cross-process lock, before the evaluation
that produced it returns to the optimiser.  Candidates are
ordered by ``(loss, source rank, source slot, source evaluation)``: lower
loss wins, with the remaining fields providing an exact-loss tie-break.
The complete winning potential is
written to a unique temporary file, flushed, and atomically renamed, so
an abrupt ``SIGKILL`` leaves either the preceding complete winner or the
new complete winner, never a torn file.  Its header records the loss,
source rank/slot/evaluation, and candidate-local hot-variable values.
A write or durability failure of the canonical file is fatal rather than
silently allowing the optimisation to continue without a durable
incumbent.  A rank-local diagnostic-mirror failure is reported but cannot
invalidate an already-durable canonical winner; the mirror retries for at
most 1 s, and after a failure makes a single attempt per new best
(unlogged) until one succeeds again.

Transient filesystem errors are retried first.  On network filesystems
(SMB/CIFS, NFS) a lock, write, ``fsync`` or rename can fail for a moment
(``EBUSY``, ``EAGAIN``, ``ESTALE``, ``ETXTBSY``, ``EIO``, ``ENOLCK``,
``ETIMEDOUT``, ``EDEADLK``) and succeed when repeated.  ``EACCES`` (an SMB
sharing violation or delete-pending file) is retried too, but only when the
directory is on a network filesystem (SMB/CIFS, NFS, AFS, Ceph, Lustre,
GPFS, 9P or FUSE); on a local disk it is a real permission problem and
fails at once.  A rename that reports ``ENOENT`` is also retried: on NFS a
retransmitted rename that had already landed fails that way, and the retry
recognises its own potential in place.
Publication then retries its whole lock → read → compare → replace
transaction with exponential backoff (10 ms doubling to 2 s, ±20 %
jitter), releasing its locks between attempts so that a better candidate
published meanwhile still wins.  Retrying stops once 30 s have passed since
the first failure; a failing call that blocks by itself (a soft-mount
timeout) can add its own duration at either end, and no budget bounds a
call that hangs on a hard mount.  Meanwhile the evaluations whose loss may
still become the new best wait at their finalize step, and so does an
evaluation whose :ref:`RESTART <restart_decorator>` stagnation check would
stop the optimiser (it first waits until as many new-best decisions have
finished as were in progress when it checked); the others carry on.
Permanent errors (``ENOSPC``, ``EROFS``, ``ENOENT``, …) are not retried.
When the error is permanent or the retries are exhausted the run fails,
whatever the optimiser (NLopt, MULTISTART and the
``ANNEAL_PT``/``ANNEAL_PA`` calibration included): the RESULT section
reads ::

   Optimiser status        : FAILED -- best-potential persistence: <error> (gave up after N attempts over T s)

and ``tadah hpo`` exits with status ``1`` (:ref:`hpo_stopping`).  New bests
of other chains that are still waiting to be saved then get a single
attempt each, without backoff: a permanent error fails at once, as before
the failure, and only a rename that reports ``ENOENT`` (which may have
landed) gets one immediate re-attempt.  A failed one is logged as
``best-potential persistence failure at eval M (no backoff after the
failure at eval N): <error>``, which after a transient error ends in
``(gave up after 1 attempt over 0 s)`` (``2 attempts`` when it follows
such a rename); the ``over 0 s`` counts from that candidate's first
failure, so it does not show the call's own duration.  So the run ends
after about one retry budget, not one per waiting candidate, and the
first failure stays the status -- but storage that has recovered by then
still saves them (``best potential of eval M saved after the persistence
failure at eval N (no backoff); the run still ends FAILED``; see below).
This holds when failing calls fail fast: with a soft NFS/SMB mount that
times out after T seconds each waiting candidate adds about T, and at
most one candidate waits per other thread or chain (``THREADS - 1`` in a
``DLIB:GFS`` run); a hard mount that hangs is not bounded at all.
After a shutdown signal (``SIGINT``, ``SIGTERM``, ``SIGUSR2``) retrying
goes on for at most 2 s, counted from when the retry first sees the
signal, with the delays shortened to fit: the 30 s budget does not hold
up the exit, and one transient blip right after the signal (a
scheduler's time-limit warning, say) still saves the candidate.  When
those 2 s are spent the candidate was not saved, so the run still ends
``FAILED`` (status ``1``, not ``128`` + the signal number), with
``(retry abandoned: shutdown requested) after N attempts over T s`` in
place of the ``gave up`` note.  The run also fails when the error meets
a concurrent stop on its way up (another chain's stagnation stop or a
shutdown request in the same epoch of a parallel annealer, ASA block or
``DLIB:GFS`` batch, or a PRESTAGE or MULTISTART stage that ends on
stagnation): a new best that could not be saved is never overruled, and
the run stops at once.  RESULT names the candidate that was lost and
what the file holds::

   Best total loss         : 9.0390e-02  (eval 3) -- NOT saved (best-potential persistence failed)
   Last saved best         : 1.0133e-01  (eval 2)
   Best potential saved    : not confirmed -- best_pot.tadah may hold eval 3 (write not synced) or eval 2; check its HPO_SOURCE_EVAL header

``Last saved best`` appears only when an earlier best was saved; the best
metrics below it (``Best EMAE``, ...) are that potential's.  ``not
confirmed`` means the candidate's rename may have happened (a directory
``fsync`` failed afterwards, or the rename itself reported ``ENOENT`` or a
transient error, which on a network filesystem it can do after landing);
otherwise — also when the rename failed with ``EROFS``, ``EXDEV``,
``ENOSPC``, ``EPERM`` or the like, which leave the file untouched — the
line reads ``best_pot.tadah (eval M)``, or ``(none written)`` for a first
best.  When several chains fail to save, the lowest loss is reported,
and ``may hold`` names each candidate whose rename may have happened
(not necessarily the lowest).  If another chain saved a *lower* best after
the failure (the storage recovered before the run stopped, and its single
attempt succeeded), that saved best is the result and the lost candidate
is listed after it; the run still ends ``FAILED`` with the first failure
as its status and exit status ``1``, because a best was lost on the way::

   Best total loss         : 8.0185e-02  (eval 23)
   Unsaved candidate       : 9.1781e-02  (eval 9) -- could not be persisted; superseded by eval 23
   Best potential saved    : best_pot.tadah (eval 23)

The per-evaluation files (``loss.tadah``, ``params.tadah``,
``outvar.tadah``, ``stats.tadah`` and their ``best_*`` twins) and the
``DUMP``/``BDUMP`` archives are not fatal.  Each writer drain retries
transient errors for up to 10 s *in total*, shared by all its files and
archives, so the first drain after a filesystem goes bad stalls for about
10 s, not 10 s per file.  Every file and archive still makes its own
attempt in every drain (and, after a permanent error such as ``ENOSPC``
or ``EROFS``, one immediate re-attempt); those calls are not bounded by
the budget.  This matters only when failing calls are slow themselves:
with a soft NFS/SMB mount that times out after T seconds the first drain
costs about 10 s plus (files + archives) × T, and every later drain of
the outage about (files + archives) × T; a hard mount that hangs is not
bounded at all.  A batch that still fails is dropped whole — never half
a row, also when the file was truncated underneath the run — and
reported, and the run goes on; that file (or the archives) then gets a
single attempt per drain, without backoff, until a write succeeds again.
After the first ``WARNING`` of such an outage, repeats are summarised (on
the 2nd, 4th, 8th, … failure, or every 10 min, with the rows lost so
far), and an ``INFO`` line reports when writes resume.  After a shutdown
signal a drain's whole retry budget is 2 s instead of 10 s (the final
drain keeps its rows through a blip and still ends well inside a
scheduler's kill timeout; rows it cannot write count as lost), and the
final drain after a best-potential persistence failure does not back off
(one immediate re-attempt after a permanent error).  An archive that
cannot be written is removed only when this run created or truncated it:
a read-only file already at that path is left alone.  The per-evaluation
files are not ``fsync``'d (that would cost every drain a round trip to
the server), so a write-back error that NFS or SMB reports only later is
not seen; ``best_pot.tadah`` is ``fsync``'d.  Every retry is a ``WARNING``
in ``hpo_run.log``, and DIAGNOSTICS gains ::

   Transient I/O retries   : N (best-potential a, per-eval logs/archives b); rows lost c; archives lost d

whenever one of these counts is non-zero (a single immediate re-attempt
without backoff -- a log writer's after a permanent error, or a waiting
candidate's after a rename's ``ENOENT`` -- is logged but not counted as a
transient retry).

``best_pot.tadah`` therefore holds the incumbent — the lowest-loss
evaluation that completed — at all times, however the run ends: stop
criteria, a signal, an error or ``SIGKILL`` (:ref:`hpo_stopping`).  The
one exception is the error that ends a run when ``best_pot.tadah``
cannot be saved: the evaluation whose potential could not be saved has
its ``loss.tadah`` row, but ``best_pot.tadah`` and ``best_params.tadah``
keep the previous incumbent.  ``best_pot.tadah`` carries the complete
configuration, its header names the evaluation that produced it, and it
is the authoritative record of the result.
``best_params.tadah`` and the other history files are appended by the
result writer in ``QFLUSH`` batches, so while the run is going they
trail ``best_pot.tadah`` by the rows still queued.  Every ending except
``SIGKILL`` (or a crash) writes the queued rows before the process exits,
so the history files then have one row per completed evaluation, and the
``best_params.tadah`` row with the highest improvement ordinal is the
incumbent's.  That is the last row unless evaluations ran concurrently:
the rows are written in evaluation order, so a later improvement can
come before an earlier one.  After ``SIGKILL`` the files can lack the
rows still queued, including the incumbent's.

HPO runs are **not resumable**: there is no checkpoint to continue from,
and every run starts fresh.  Before evaluating its first candidate, a run
removes the canonical and rank-local incumbents a previous run left in the
working directory, so an earlier objective's loss cannot suppress the new
run's output (copy ``best_pot.tadah`` elsewhere first to keep it).  A
``WARM`` seed is parsed before this step, so starting a new run from an old
potential remains supported.

If ``SIGKILL`` lands after the temporary file is flushed but before its
rename, a hidden ``.best_pot.tadah.tmp.<pid>.<counter>`` file can remain.
It is harmless and ignored: readers and later publishers compare only
the canonical path and each later write chooses a fresh temporary name.

In an MPI HPO run, rank-local potential outputs are additionally named
``best_pot.tadah.r<rank>`` on every rank (including ``.r0``).  They are
diagnostic local views; the unsuffixed canonical file is the
lowest-loss potential observed across all ranks and is suitable for a
live warm start without waiting for normal program termination.

============================ ============== =================================================================
Key                          Default        Effect
============================ ============== =================================================================
``DUMP <N> <DIR>``           off            Save the trial potential of every ``N``-th evaluation as
                                            ``<DIR>/pot_<iter>.tadah``.  Integer ``N ≥ 1`` and ``<DIR>``
                                            are both required (``0`` is rejected), and nothing may
                                            follow them; the directory is created once the line is
                                            valid.
``BDUMP <N> <DIR>``          off            Save every ``N``-th new best as
                                            ``<DIR>/best_pot_<k>.tadah``, ``k`` being the improvement
                                            ordinal.  Same rules as ``DUMP``.
============================ ============== =================================================================

Number format.  ``best_pot.tadah``, its rank-local mirrors and the
``DUMP``/``BDUMP`` archives follow the potential-file rules of
:ref:`pot_number_format`: every non-integer number of the key/value
lines is written with ``NUMERIC`` significant digits, default ``18`` —
exact, so the file re-parses to precisely the doubles the evaluation used
— and ``NUMERIC`` itself is never written.  A ``WARM`` restart from it
starts from the file's ``OPTIM`` values bit for bit when they lie inside
the boxes (starting points are evaluated exactly, see
:ref:`search_space_scaling`); every other key comes from the ``-c``
configuration, so with the same configuration the restart's starting
point is the archived potential.  The precision is read once, from the
configuration given with ``-c``; it only shapes the files, never the
evaluation.  The ``#`` header is an annotation that ``NUMERIC`` does not
govern.  Its machine-read lines, ``# HPO_BEST_LOSS`` and ``# HOT``, are
always written with 18 significant digits (lossless), whatever
``NUMERIC`` is: the durable-best check compares the loss exactly, and
``WARM`` primes the hot-value cache from ``# HOT``.  The rest of the
header (banner, LOSS, RESULT, DATA and OPTIMISER sections) is for
reading: the RESULT table and the archive banners' ``loss=`` use
``DIGITS``, the other annotations fixed short formats.

.. warning::

   ``best_pot.tadah`` files written by ``tadah hpo`` 1.5.0 contain an
   automatically inserted ``NUMERIC 12`` line.  Used as the ``-c``
   configuration of a new run, that line is an explicit setting: the new
   run writes 12-digit potentials and warns about it (``hpo_run.log`` and
   stderr).  Delete the ``NUMERIC`` line to get exact potentials again.

Run log
.......

``hpo_run.log`` (rank-suffixed under MPI, like the other per-rank
outputs) is the run's structured journal. It opens with a banner
(version, working directory, PID, timestamp) followed by nine sections::

   -- FILES         --   config / output files
   -- OPTIMISER     --   algorithm, stop criteria, parameters, the config-level
                         ``Initial step`` and ``Step consumption`` lines
   -- SEARCH SPACE  --   scaling policy (``OPTIM_SCALE``, ``LOG_HP``), real width
                         ratio, one row per dimension: scale, box origin, real
                         and internal bounds, start point in both spaces
   (PROGRESS)        --   NLopt runs add an ``Initial step`` INFO block: provenance,
                         consumption mode, and per dimension the step in internal
                         units, as a fraction of the width, in real units at the
                         start, with near-bound / inward / capped notes (WARNINGs)
   (SUMMARY)         --   ``best point (eval N) sits within 2% of an OPTIM limit on
                         K dim(s)`` table + one HINT (bound-occupancy audit)
   -- DIAGNOSTICS   --   also ``Bound occupancy of the final best`` (or ``[ok]``)
   -- POTENTIAL     --   2B/MB terms, cutoffs, element list; DM_REAM:
                         ``Embedding F(0)`` (EMBF0) and the raw F(0) of -c,
                         the density report (``Density form``, ``Density
                         rho(r)``, ``Density rhobar``) of -c and each WARM seed
   -- LOSS FUNCTION --   targets, weights, FAILSCORE
   -- PROGRESS      --   timestamped events: bests, warnings, milestones,
                         feasibility-gate rejects
   -- DIAGNOSTICS   --   NaN/Inf clampings, graded failures with the gate
                         rejects, unusable losses (every evaluation without a
                         usable loss) with a verdict, positive-form
                         projections, bound-proximity analysis
   -- PERFORMANCE   --   timing statistics
   -- RESULT        --   final status, best loss, best metrics; DM_REAM:
                         raw F(0) of the first and the best evaluation, the
                         best's density rows

The optimizer journals documented on the algorithm pages (banners,
per-rung and per-window reports, audits, the final ``stopped:`` line)
land in PROGRESS, tagged ``INFO``/``WARNING``/``HINT``/``SUMMARY``:
``WARNING`` states something provable about this run, ``HINT`` an
empirical caution, and multi-line messages carry a ``*`` on the head
line. ``hpo_run.log`` is the canonical record of which features fired
during a run; the stdout markers in :ref:`diagnostic_markers` are the
quick-look subset.

Practical tips
..............

* Large optimisations can produce thousands of potentials; watch disk
  usage when ``DUMP`` is enabled.
* The log files are plain text and append-only.  They are tail-friendly:

  .. code-block:: bash

     tail -f loss.tadah

* ``params.tadah`` columns follow the optimiser order (by key, then
  index; :ref:`dimension order <hpo_dimension_order>`), which need not
  match the order of your ``OPTIM`` lines; read
  the columns by their ``KEY[n]`` labels.  To reproduce a candidate,
  use the potential itself (``best_pot.tadah`` or a ``DUMP``/``BDUMP``
  archive), which carries the full configuration.

.. _hpo_stopping:

Stopping a run
--------------

A run ends when the optimiser meets its stop criteria (``MAXEVAL``,
``STOPVAL``, a tolerance, ``MAXTIME``, the ``RESTART`` budget, …), when
a point without a usable gradient ends a gradient-based search
(:ref:`hpo_gradient_not_finite`), when a signal stops it, or when an
error stops it.  ``best_pot.tadah`` holds
the incumbent in every case (:ref:`hpo_potential_archives`) — unless no
evaluation produced a usable loss (a feasible one: finite and below the
graded-failure band ``[0.999 × FAILSCORE, FAILSCORE)``).  Then there is no
incumbent and no ``best_pot.tadah`` is written, and a run that ends on its
own ends as a failed one (status ``1``); a signal or an error decides the
status first (below).  ``FAILSCORE`` is a positive number, so every failed
evaluation lies in the failure band; before 1.6.0 a ``FAILSCORE`` of
``0`` or below was accepted and left no band, and a run all of whose
evaluations failed ended as a completed one, with a ``best_pot.tadah``
that held a failed evaluation.

**Signals.** ``tadah hpo`` handles ``SIGINT`` (Ctrl-C), ``SIGTERM``
(``kill``, ``scancel``) and ``SIGUSR2`` (an advance warning before a
batch job's time limit, below).  The first one stops the run: no new
evaluation starts, the evaluations already running finish and are
recorded, and the optimiser and strategy stop with them — later
``MULTISTART`` starts, ``RESTART`` rounds and the ``PRESTAGE``
refinement do not run, and a signal during ``NOISE_CALIBRATION`` ends the
calibration.  The queued rows are then written to the history files and
the journal is closed.  A second signal does not interrupt that
shutdown; ``kill -9`` (``SIGKILL``) ends the process at once.  The
``PC_ISOLATION PROCESS`` worker processes ignore all three signals, so a
signal sent to the whole process group or job step (Ctrl-C in a
terminal, ``kill -<signal> -<pgid>``, a signal SLURM sends to a job step)
does not kill the evaluations they are running.

**Batch jobs.** SLURM's ``--signal=[B:]USR2@<seconds>`` sends ``SIGUSR2``
that many seconds before the time limit.  With ``B:`` it signals only
the batch shell, which ``SIGUSR2`` kills without passing it on: make
``tadah`` the batch shell by starting it with ``exec`` as the script's
last command.  Without ``B:``, SLURM signals the job steps: start
``tadah`` with ``srun``.

.. code-block:: bash

   #!/bin/bash
   #SBATCH --time=24:00:00
   #SBATCH --signal=B:USR2@600     # SIGUSR2 to the batch shell 10 min early
   exec tadah hpo -c config.tadah --hpotarget config.hpo --validation config.val

   # or, with  #SBATCH --signal=USR2@600  (every job step is signalled):
   # srun tadah hpo -c config.tadah --hpotarget config.hpo --validation config.val

A plain ``tadah hpo …`` line under ``--signal=B:USR2@…`` never receives
``SIGUSR2``: the signal kills the batch shell instead, and ``tadah`` gets
no advance warning.

.. list-table:: How a run ends
   :header-rows: 1
   :widths: 22 20 32 26

   * - Ending
     - Exit status of ``tadah``
     - ``Optimiser status`` in the journal's ``RESULT`` section
     - History files
   * - Stop criteria met
     - ``0``
     - Why the optimiser run that ended the run ended (under
       ``MULTISTART`` the last successful start, under ``PRESTAGE`` the
       refinement; :ref:`hpo_end_status`): ``<LIB>:<ALGO>: <reason>``
       for TADAH, DLIB and CERES (``TADAH:RANDOM: STOPVAL reached``,
       ``DLIB:BFGS: FTOL_ABS reached``), NLopt's result (``MAXEVAL_REACHED
       -- …``), ``ASA: …``; ``COMPLETED`` before 1.6.0 where a backend
       gave none.  An NLopt algorithm that terminated itself
       (``NLOPT_FAILURE`` or ``NLOPT_ROUNDOFF_LIMITED``, a masked
       diagnostic) reads ``STOPPED -- <reason> (…)``, and a Ceres solve
       that failed on its own ``STOPPED -- CERES:<ALGO>: FAILURE (…)``;
       the best result stands.  When the ``RESTART`` budget ends the run:
       ``STOPPED -- stagnation: no new best in W evals after R
       restart(s)``.  Under ``MULTISTART``, when a start failed, followed
       by ``(N of M MULTISTART starts failed; see the WARNINGs)``, counted
       over the run.
     - Complete.
   * - A point without a usable gradient ended the gradient-based search
       that ended the run (:ref:`hpo_gradient_not_finite`)
     - ``0`` (``1`` without a usable loss: next row)
     - ``STOPPED -- no usable gradient: a failed evaluation in the
       finite-difference stencil`` (or another cause, e.g. ``… a failed
       evaluation at the point``, ``… the finite-difference quotient
       overflowed``)
     - Complete.
   * - Stop criteria met (the ``RESTART`` budget included), or a
       point without a usable gradient ended the search, but no
       evaluation produced a usable loss
     - ``1``, with ``[HPO] ERROR: no evaluation produced a usable loss;
       no best_pot.tadah was written`` on stderr (and on an ``ERROR``
       line of the journal)
     - ``STOPPED -- no evaluation produced a usable loss``
     - Complete; every row holds ``FAILSCORE`` (or a graded failure),
       and there is no ``best_pot.tadah``.
   * - ``SIGINT``, ``SIGTERM``, ``SIGUSR2``
     - ``128`` + the signal number: ``130``, ``143``, ``140`` (Linux)
     - ``SHUTDOWN (SIGINT)``, ``SHUTDOWN (SIGTERM)``,
       ``SHUTDOWN (SIGUSR2)``
     - Complete.
   * - A new best could not be saved (:ref:`hpo_potential_archives`),
       also when the run then ended on a stop criterion, the ``RESTART``
       budget, a signal (its retries after the signal used up their
       2 s) or a concurrent stagnation stop; or the reset of
       ``best_pot.tadah`` before the first evaluation failed
     - ``1``
     - ``FAILED -- best-potential persistence: <error>`` (the first
       failure of the run)
     - Complete, except that the lost candidate's evaluation is not the
       incumbent (below).
   * - An optimiser that ended normally without recording why (an
       internal error; it outranks a run without a usable loss)
     - ``1``
     - ``FAILED -- internal error: the optimiser ended without recording
       why it ended``
     - Complete.
   * - Any other error during the run: an optimiser failure, every start
       of a ``MULTISTART`` launch failing, an exception outside the
       per-evaluation ``FAILSCORE`` recovery
     - ``1``
     - ``FAILED -- <message>`` (``FAILED -- non-standard exception`` for
       an error that is not a C++ standard exception).  Under
       ``MULTISTART`` a failure confined to one start is a ``WARNING``
       (journal and stderr) and the next start runs.
     - Complete.
   * - ``SIGKILL``, a crash, a node failure
     - Killed (a shell reports ``137`` for ``SIGKILL``)
     - None: the journal has no ``RESULT`` section.
     - Can lack the rows still queued: up to ``QFLUSH − 1``, more when
       evaluations run concurrently and a slow one holds later rows back.

"Complete" means one row per completed evaluation (subject to the
``OUTPUT`` / ``BOUTPUT`` cadences), with the ``best_params.tadah`` row of
the highest improvement ordinal belonging to the incumbent — except
after a failure to save ``best_pot.tadah``, whose evaluation has its
``loss.tadah`` row but is not the incumbent
(:ref:`hpo_potential_archives`).  If an evaluation
never delivered its result, the rows after it are written anyway at the
end of the run and the journal warns ``… stranded behind missing
iteration N``.

An error found while the run is set up, before the optimiser starts (a
configuration or data error), ends ``tadah`` with status ``1`` and a
message on stderr; there are no history rows, and the journal, if it was
opened, has no ``RESULT`` section but records the error on an ``ERROR``
line (``ERROR     startup: <message>``).  The configuration rules are
checked before the worker pool is sized, before any ``PC_ISOLATION
PROCESS`` worker is started and before any data is loaded.  An error the optimiser raises before
its first evaluation (for example an unknown ``PARAM`` of ``TADAH:GRID``)
also leaves no history rows, but the journal has a ``RESULT`` section
with ``FAILED -- <message>`` and zero evaluations.  So does a run whose
reset of ``best_pot.tadah`` before the first evaluation fails (its lock
cannot be taken, say): ``FAILED -- best-potential persistence: <error>``,
status ``1``, its retries counted in ``DIAGNOSTICS``.

In a multi-rank ``ANNEAL_PT`` / ``ANNEAL_PA`` run
(:ref:`tadah_anneal_mpi`), each rank stops on the signal it receives
itself.  A rank that has not seen it when another rank stops ends with an
error instead, status ``1`` and ``FAILED -- … worker failed on
another MPI rank …`` (or ``… rank 0 aborted during … calibration`` when
the signal arrives during the calibration); only the ranks that saw the
signal report ``SHUTDOWN`` and ``128`` + the signal number.  Every rank
still writes its queued rows, and ``best_pot.tadah`` holds the global
incumbent.

A signal or an error decides how a run ends whether or not an
evaluation produced a usable loss: a run without one that a signal stops
still exits with ``128`` + the signal number and ``SHUTDOWN
(<signal>)``, and one an error stops with ``1`` and ``FAILED --
…``.  Before 1.6.0 a run without a usable loss exited with ``0``
and ``COMPLETED``.  In a multi-rank run, a rank ends as a successful one
when any rank produced a usable loss (``best_pot.tadah`` is global).
Each rank's journal (``hpo_run.log.r<rank>``) counts that rank's
evaluations only, so its DIAGNOSTICS verdict names the rank: a rank whose
own evaluations all failed says ``[!] No evaluation on MPI rank 1 (of 2)
produced a usable loss.`` and still ends as a completed run when another
rank produced one.

The run's exit status is also ``tadah``'s: a job script can tell a
completed run (``0``) from a stopped one (``130``, ``143``, ``140``) and
a failed one (``1``: an error, or no usable loss).  A script should test
the exit status, not the status text, which names why the search ended
and changes with the backend (a script that looked for ``Optimiser
status : COMPLETED`` no longer matches: 1.6.0 writes the reason).

.. _warmstart_protocol:

Warm-start protocol and bad-potential early-exit
------------------------------------------------

Warm-start: ``${hot_a0}`` (or any ``--hotvar``)
...............................................

For LAMMPS scripts that start with a ``box/relax`` from a hard-coded
lattice guess (typical for elastic constants, surface energies, …), HPO
can short-circuit the expensive minimisation:

#. Declare the producing ``PC_LAMMPS`` line as hot:

   .. code-block:: none

      PC_LAMMPS --script in.a0_relax \
                --hotvar hot_a0 2.5 4.5 3.302 \
                --varloss ...

   ``<min> <max>`` bound the physical range; ``<initial>`` is optional
   (midpoint of ``[min, max]`` when omitted).

#. Replace the hard-coded ``lattice bcc 3.2`` in that script with the
   injected value. ``lattice bcc ${hot_a0}`` works, but LAMMPS expands
   ``${hot_a0}`` with 15 significant digits; a ``format``-style copy
   passes the exact value (see :ref:`hot_value_precision`):

   .. code-block:: none

      variable hot_a0_s format hot_a0 %.17g
      lattice bcc ${hot_a0_s}

#. End the script by redefining ``hot_a0`` as an ``equal``-style
   variable (**not** ``string``):

   .. code-block:: none

      variable hot_a0 equal lx/<divisor>

   where ``<divisor>`` is the number of conventional cells along x
   (e.g. ``4.0`` for the ``region box block 0 4 0 4 0 4 units lattice``
   of ``examples/common/lammps/Ecoh.in``). HPO injects
   ``hot_a0`` as an ``equal``-style variable, so this is a same-style
   overwrite that LAMMPS accepts without a prior ``variable delete``.
   A ``string``-style readback makes the next evaluation's injection
   fail: LAMMPS refuses to redefine the string as an ``equal``-style
   variable (``Cannot redefine variable hot_a0 with a different
   style``), so the hot script does not run, its evaluation fails and the
   journal says so (``--hotvar``, :ref:`pc_lammps_options`); the instance
   is then discarded, and the evaluation after it starts on a fresh one.
   Before 1.6.0 the refusal was ignored and the script ran on the value
   its instance had kept.

HPO injects ``${hot_a0}`` as an ``equal``-style variable on every call
(``<initial>`` from ``--hotvar`` on the first call, the cached value
thereafter), captures the script's ``${hot_a0}`` after each successful
run, and reuses it on the next evaluation. The variable is then
propagated into the cold (no-``--hotvar``) ``PC_LAMMPS`` scripts that
read it (``${hot_a0}`` or ``v_hot_a0``), which run in phase B. When ``WARM`` reads a ``pot.tadah``
whose writer dumped ``# HOT_A0 <script> <a0>`` lines, the cache is
primed automatically; ``[HPO] WARM: primed warmstart cache with N
HOT_A0 entry(ies)`` confirms how many lines were absorbed. The cached
values belong to the run: a second run in the same process (a task file
with two ``hpo`` tasks) starts from ``<initial>``, or from its ``WARM``
seed, again.

Multiple ``--hotvar`` flags may be repeated on the same ``PC_LAMMPS``
line; ``hot_a0`` is just a conventional name (any identifier works).
Per-line environment variables (``TADAH_HPO_HOT_A0_DEFAULT``,
``TADAH_HPO_A0_MIN``, ``TADAH_HPO_A0_MAX``) were removed in this
release; the per-line ``--hotvar`` flag replaces all three.

.. _hot_value_precision:

Precision of values passed to and read from LAMMPS
..................................................

HPO's side of the hand-over is lossless: an injected value (a ``--hotvar``
value, or a hot value propagated to a cold script) is defined as
``variable <name> equal <value>`` with 17 significant digits, and every
``--varloss``, ``--outvar``, ``--failvar`` and ``--hotvar`` readback
evaluates the variable's formula directly (LAMMPS ``compute_equal``),
keeping the full double.  Inside the script, however, LAMMPS turns
variables into text with 15 significant digits in two places:

* ``${name}`` of an ``equal``- or ``internal``-style variable, and
* ``v_name`` of an ``equal``-style variable inside a formula, including
  inside ``$(...)`` (``v_name`` of an ``internal``-style variable reads
  the double directly).

A value assembled through these references therefore carries 15 digits
(a relative change of up to ~5e-15), whatever HPO does at either end.
Lossless idioms:

.. list-table::
   :header-rows: 1
   :widths: 26 34 40

   * - Need
     - 15 digits
     - Exact
   * - an injected value as a command argument
     - ``lattice bcc ${hot_a0}``
     - ``variable hot_a0_s format hot_a0 %.17g`` then
       ``lattice bcc ${hot_a0_s}``
   * - capture a scalar for later formulas
     - ``variable Ebulk equal $(c_eatoms)``, used as ``v_Ebulk``
     - ``variable Ebulk internal $(c_eatoms)``, used as ``v_Ebulk``
   * - an outvar built from other variables
     - ``variable Ecoh equal v_Ecoh_raw`` with ``Ecoh_raw``
       ``equal``-style
     - one formula over computes, thermo keywords and ``internal``
       captures, e.g. ``variable Ecoh equal ternary(v_eq_ok,
       -(v_Ebulk-v_N*v_Eiso)/v_N, 100.0)``

``$(...)`` itself renders with 20 significant digits, and ``format``- and
``internal``-style variables may be redefined with the same style on
every evaluation of the cached LAMMPS instance.  Keep the readback of a
hot script as ``variable hot_a0 equal lx/<divisor>``: HPO reads it with
``compute_equal``, so it is exact.  Every variable HPO reads
(``--varloss``, ``--outvar``, ``--failvar``, ``--hotvar``) must stay
``equal``-style; define it over ``internal`` captures (``variable Eiso
equal v_Eiso_i``).  The example scripts ``HPO/examples/common/lammps/Ecoh.in``
and ``Ecoh_param.in`` follow these idioms.

Bad-potential early-exit
........................

Two cheap checks let HPO skip the expensive LAMMPS stage when training
has produced an unphysical potential:

* the per-metric ``PC_<METRIC> --skip-above`` / ``--skip-rel`` filters
  documented at :ref:`performance_constraints` (Per-metric bad-pot
  pre-filter), and
* the post-LAMMPS ``--hotvar`` range filter described above (a relaxed
  value outside ``[min, max]`` fail-scores the iteration and does not
  update the cache).

The ``TADAH_HPO_BAD_POT_ERMSE_MEV`` / ``_REL`` environment
variables were removed in this release; use
``PC_ERMSE … --skip-above <cap>`` / ``--skip-rel <ratio>`` instead.

.. _performance_parallelism:

Performance & parallelism
-------------------------

Every ``tadah hpo`` run parallelises within one process through the
shared worker pool below.  The MPI-parallel build (``TADAH_HPO_MPI``)
additionally spreads the ``TADAH:ANNEAL_PT`` / ``TADAH:ANNEAL_PA``
optimisers over MPI ranks.

Within one process (OpenMP and the worker pool)
...............................................

* **Shared worker pool** — ``THREADS`` in the ``STRATEGY`` block is the
  total pool width for ``PC_LAMMPS`` fan-out and MLIP train/predict work.
  Serial drivers use the pool inside each objective evaluation;
  ``DLIB:GFS`` and ``ANNEAL_PT``/``ANNEAL_PA`` can also feed work from
  several candidates, replicas, or walkers into that same pool.

* **Residual OpenMP kernels** — ``OMP_NUM_THREADS`` controls the
  remaining OpenMP work, such as neighbour-list construction and data
  statistics:

  .. code-block:: bash

     export OMP_NUM_THREADS=<n>

  It does not size the shared pool and does not multiply ``THREADS``.
  Start with ``THREADS`` equal to the per-rank physical-core budget and
  ``OMP_NUM_THREADS=1``; tune the residual width separately if those
  kernels are material.  The primary sizing rule is::

     THREADS  ≤  physical cores allocated to this rank

* **LAMMPS runs** — every LAMMPS instance runs on a single rank: the
  default build links a *serial* LAMMPS library, and the MPI-parallel
  build runs each instance on ``MPI_COMM_SELF``.  Parallelism is driven
  *around* LAMMPS, not inside it.

Across MPI ranks (TADAH_HPO_MPI)
................................

The MPI-parallel build distributes the replicas of ``TADAH:ANNEAL_PT``
and the walkers of ``TADAH:ANNEAL_PA`` over MPI ranks; every rank trains
its own models and runs its own ``THREADS`` pool.  Build it with
``-DTADAH_HPO_MPI=ON`` (:ref:`installation_hpo_mpi`); the requirements
and an ARCHER2 job script are in :ref:`tadah_anneal_mpi`.  Model training
itself is never distributed: the legacy MPI training build and its
``PC_LAMMPS --ncpu`` option were removed in 1.6.0.

Practical advice
................

* Size ``THREADS`` first: it is the main fan-out and train/predict CPU
  budget, including for a serial annealing chain.
* Increase ``OMP_NUM_THREADS`` only when profiling shows the residual
  OpenMP kernels matter; it does not widen ``PC_LAMMPS`` fan-out.
* Measure, do not guess: a few short test runs sweeping ``THREADS`` and
  then the residual OpenMP width will reveal the sweet spot.

.. _hpo_runtime_checks:

Audits and health monitoring
----------------------------

Three keys of the *training configuration* (``--config``) control
diagnostics that ``tadah hpo`` runs around the optimisation.
``AUDIT`` and ``HEALTH_LOG`` are stripped from every potential file HPO
writes.

``AUDIT off|warn|error`` (default ``off``)
   Before the optimiser starts, run the pre-flight audit on the training
   data: per-descriptor health checks (e.g. a fixed, off-shell ``CGRIDMB``
   that collapses the density) plus a check for *critical* descriptor
   parameters that have no ``OPTIM`` line.  ``warn`` logs the findings
   and continues; ``error`` aborts on any FAIL-level finding.  Skipped
   when there is no training ``DBFILE``.
``AUDIT_SAMPLE <n>`` (default ``256``)
   Number of training structures (deterministic sample) the audit's
   dataset scan uses; ``0`` scans all of them.
``HEALTH_LOG off|summary|full`` (default ``summary``)
   Per-evaluation health monitor of the weights each evaluation's model
   holds (fitted, or set from ``FIXWEIGHT``), by the blocks of its design
   matrix: the bias column, the two-body and the many-body descriptor's
   columns, or one block for a non-linear basis function or kernel.  It
   warns when a weight is NaN or infinite, when every weight of a block
   is exactly zero, when fewer than 5 % of a block's weights exceed
   1e-15 in magnitude (a block that holds a NaN or infinite weight is
   checked for neither: the NaN/Inf warning describes it), and when the
   model has not as many weights as its design matrix has columns (the
   block checks are then skipped).
   ``summary`` gives at most one warning of each kind (per block) in 100
   evaluations, ``full`` reports every offending evaluation, ``off``
   disables it.  The warnings are printed on standard output
   (``WARNING: iter_health: eval <n> ...``), not written to the
   journal.  Before, the monitor read a design matrix that ``tadah hpo``
   never builds, and it reported nothing.

.. _hpo_env_overrides:

Environment overrides
---------------------

These environment variables override the corresponding configuration
keys without editing the files (intended for A/B comparisons and test
harnesses).  An invalid value is an error, not a silent fallback.

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Variable
     - Effect
   * - ``TADAH_OPTIM_SCALE=AUTO|NONE``
     - Replaces ``OPTIM_SCALE`` (:ref:`search_space_scaling`).
   * - ``TADAH_PC_ISOLATION=THREAD|PROCESS``
     - Replaces ``PC_ISOLATION`` (:ref:`pc_lammps_execution`); counts as
       an explicit request, so ``PROCESS`` is an error in the MPI-parallel
       build.
   * - ``TADAH_PC_WORKER_TIMEOUT=<seconds>``
     - Replaces ``PC_WORKER_TIMEOUT``; same validation.
   * - ``TADAH_FAILSCORE_GRADING=ON|OFF``
     - Replaces ``FAILSCORE_GRADING`` (:ref:`failscore_grading`).
   * - ``TADAH_BUMPGATE=CANDIDATE|STRICT``
     - Replaces ``BUMPGATE`` (:ref:`hpo_feasibility_gates`).

.. _diagnostic_markers:

Diagnostic stdout markers
-------------------------

HPO emits a one-line stdout (or stderr) marker each time a notable
event fires.  They are stable and intended to be grep-friendly.

============================================================================ ============================================================
Marker                                                                       Meaning
============================================================================ ============================================================
``[HPO] LIB:ALGO   dim=N``                                                    Active driver and search-space dimension.
``[HPO] InitStrategy NAME produced N starting points``                        After init resolution.
``[HPO] WARM: loaded N parameter(s) from <file>``                             ``WARM`` read all keys.
``[HPO] WARM: primed warmstart cache with N HOT_A0 entry(ies)``               ``HOT_A0`` lines parsed from a ``WARM`` file.
``[HPO] WARM: clamped dim[d] from X to Y (bounds […])``                       Warm point fell outside narrowed bounds.
``[HPO] DLIB:GFS seeding with N starting points``                             Multi-seed forwarding.
``[HPO] GRID: MAXEVAL N < M grid points (…); evaluating the first N …``       ``TADAH:GRID`` stops its sweep after ``MAXEVAL`` points.
``[HPO] TADAH:RANDOM stopped: <stop> after N evaluation(s)``                 The stop that ended ``RANDOM``: ``MAXEVAL reached``, ``MAXTIME exceeded``,
                                                                             ``STOPVAL reached``.
``[HPO] TADAH:GRID stopped: <stop> after N of M grid points``                The stop that ended the sweep early; ``TADAH:GRID finished: all M
                                                                             grid points evaluated`` otherwise.
``[HPO] DLIB:GFS stopped: <stop> after N evaluation(s)``                     The stop that ended ``DLIB:GFS`` (``MAXEVAL``, ``MAXTIME``, ``STOPVAL``).
``[HPO] DLIB:BFGS stopped: <reason> after N evaluation(s): <test>``          The convergence test that ended ``DLIB:BFGS`` (``FTOL_ABS reached``,
                                                                             ``tol_gradinf reached``) and its value; a spent ``MAXEVAL`` prints
                                                                             ``[HPO] DLIB:BFGS -- MAXEVAL=N reached; preserving best-so-far``.
``[HPO] CERES:ALGO stopped: <reason> after N evaluation(s): <setting>; …``   Why a CERES solve ended (:ref:`hpo_end_status`), with the setting
                                                                             and Ceres' message.
``[HPO] MULTISTART: running K starts (parallel=N)``                           MULTISTART fires.
``[HPO] MULTISTART per-start MAXEVAL = N``                                    Either from ``BUDGET_PER_START`` or split.
``[HPO] MULTISTART start i/K -- running inner optimiser (seed=…)``            Per-start; ``(seed=…)`` only when ``SEED`` is set.
``[HPO] PRESTAGE: exploration phase LIB:ALGO (MAXEVAL=…)``                    PRESTAGE phase starts (``MAXEVAL=none: no evaluation limit``
                                                                              when the ``EXPLORE`` block has none).
``[HPO] PRESTAGE: top-K losses: …``                                           Snapshot quality after PRESTAGE finishes.
``[HPO] REFINEMENT: refining top-K seed(s) via LIB:ALGO``                     Outer (refinement) phase starts.
``[HPO] RESTART i/M -- fresh init from METHOD; SEED …``                       Stagnation watchdog fired.  ``METHOD`` is the ``RESTART`` block's
                                                                              init method (``CHAIN`` for several ``METHOD`` lines, ``CONFIG``
                                                                              without one); ``; …`` lists the round's seeds: ``RESTART SEED``
                                                                              (the fresh points', ``SEED + i``) when the ``RESTART`` block has a
                                                                              ``SEED``, then the derived seeds of the phases that have one.
``[HPO] RESTART: stagnation reached, max restarts (M) used; stopping``         Restarts exhausted; the run stops.
``[HPO] MULTISTART: N of K start(s) stopped on stagnation; …``                Batch outcome reported to RESTART.
``[HPO] REFINEMENT: N of K seed(s) stopped on stagnation …``                  PRESTAGE refinement outcome, reported to RESTART.
``[HPO] WARNING: MULTISTART start i/K failed: …``                             One start failed; the next one runs.
``[HPO] NOISE_AUTO: PARAM fd_step set to V (diag=…, dim=…)``                  ``NOISE.AUTO`` fired and shows the value used.
``[HPO] WARNING: eval N: KEY = v lies outside its OPTIM box [lo, hi] …``      stderr: the first evaluation outside the box (an optimiser that ignores
                                                                              the bounds); ``DIAGNOSTICS`` counts them all (:ref:`hpo_fd_stencil`).
``[HPO] Initial step (LIB:ALGO): …``                                         NLopt run started; the effective per-dimension step is in the journal.
``[HPO] OPTIM_SCALE X taken from the TADAH_OPTIM_SCALE …``                    Env override of the scaling policy is active.
``[HPO] search space: N dim(s); OPTIM width min … max …``                     Startup width audit; a WARNING names near-pinned dims.
``[HPO] LIB:ALGO (…) does not use STEP; …``                                    Startup rejection of a ``STEP`` the backend would ignore.
``[HPO] DLIB:GFS: dlib applies its own ln transform to …``                     dlib's hidden log transform fires (``OPTIM_SCALE NONE`` only).
``[HPO] NOISE_CALIBRATION: running N evals at the seed point``                ``NOISE.CALIBRATION`` started.
``[HPO] ASA[Ingber ASA-30.53]: seed=… dim=…``                                 ``ASA:ASA`` entry banner with resolved options.
``[HPO] ASA stopped: <reason> after N host eval(s) (asa exit: …)``            ``ASA:ASA`` stop reason + upstream exit code.
``[HPO] ERROR: no evaluation produced a usable loss; …``                     stderr: the run ends with status ``1`` and no ``best_pot.tadah``
                                                                             (:ref:`hpo_stopping`).
============================================================================ ============================================================
