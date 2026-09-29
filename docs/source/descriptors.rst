.. _descriptors:

Descriptors
===========

This page lists every two-body (``D2_*``) and many-body (``DM_*``) descriptor
compiled into Tadah!MLIP.  The class name shown in each heading is exactly the
keyword you write in your configuration file.

General workflow
----------------

#.  Decide which descriptor families you need (two-body, many-body, or both).
#.  Switch the family **on** with the corresponding *INIT* flag::

        INIT2B  true      # activate two-body block
        INITMB  false     # skip many-body block

    At least one of ``INIT2B`` / ``INITMB`` must be ``true``.  If both are
    ``true`` Tadah! will build one descriptor of each type **unless** you say
    otherwise with ``TYPE2B`` / ``TYPEMB``.
#.  Provide **exactly one** line per descriptor you want to build, using the
    formats below.  If you need to concatenate several descriptors of the same
    family, use the meta classes ``D2_mJoin`` or ``DM_mJoin`` and list the
    components *in a block* right after the meta keyword.

The bias term
-------------

The first component of the overall descriptor vector can be a constant 1
("bias").  Add it with

.. code-block:: ini

   BIAS  true

Descriptor key syntax
---------------------

Two-body::

   TYPE2B  D2_<Name>   [param ...]   <EL1> <EL2> ...

Many-body::

   TYPEMB  DM_<Name>  L  N_C  N_S  [N_CE  N_SE]  <EL1> <EL2> ...

where

* ``<Name>`` is copied from the headings below (case-sensitive).
* ``[param …]`` are the extra integers/doubles required by a given class
  (see its table for details).
* ``<EL1> <EL2>`` are element symbols (use ``*`` for "any"). Multiple pairs
  are allowed, e.g. ``DM_EAD 1 4 4 Ti Ti Ti Nb``.
* ``L`` is the maximum angular momentum number needed by DM descriptors.
* ``N_C``, ``N_S`` are the sizes of the *centre* and *width* grids
  (``CGRID*`` / ``SGRID*``).
* ``N_CE``, ``N_SE`` appear only when a non-linear embedding function is used
  (``CEMBFUNC`` / ``SEMBFUNC``).

Remember to supply matching auxiliary keys:

=============  =========================================================
``RCTYPE2B``   cutoff function(s) – one per descriptor in ``TYPE2B``
``RCUT2B``     cutoff distance(s)
``CGRID2B``    centre grid(s) – if the descriptor uses them
``SGRID2B``    width  grid(s) – same length as the corresponding centres
=============  =========================================================

(and the analogous ``*MB`` variants for many-body descriptors).

Ordering matters: we recommend writing the block

.. code-block:: ini

   TYPE2B   …
   RCTYPE2B …
   RCUT2B   …
   CGRID2B  …
   SGRID2B  …

together before starting the next descriptor family, so the eye can verify
that list lengths match.

.. _composite_descriptors:

Composite Descriptors
---------------------

Tadah!MLIP lets you **stitch several primitive descriptors together into one
feature vector** – a *composite descriptor* – so you can embed physical
insight (e.g. "add a short-range ZBL shield to a Blip term") without editing
source code.  The idea is implemented via the meta-classes ``D2_mJoin``
(two-body) and ``DM_mJoin`` (many-body).

Key points
~~~~~~~~~~

* Activate the relevant family first: ``INIT2B`` / ``INITMB``.
* Declare the meta descriptor:

  ``TYPE2B  D2_mJoin``  *or* ``TYPEMB  DM_mJoin``

* Immediately follow with one **TYPE** line per constituent descriptor,
  in the order you want them concatenated.
* Provide matching lists for every auxiliary key
  (``RCTYPE*``, ``RCUT*``, ``CGRID*``, ``SGRID*``,
  and, if required, ``C/SEMBFUNC``).
  List length **must** equal the number of constituents.
* Each constituent can target its own element pair(s), cutoff type and
  distance.

Quick examples
--------------

Single Lennard-Jones descriptor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   # -- simple monatomic model -----------------------------------------
   INIT2B     true
   TYPE2B     D2_LJ  Kr Kr    # no extra parameters
   RCTYPE2B   Cut_Cos
   RCUT2B     6.0

2-body BP + many-body EAD
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   # ----- two-body Behler-Parrinello ----------------------------------
   INIT2B  true
   TYPE2B     D2_BP 10 10 Kr Kr    # 10 radial functions
   RCTYPE2B   Cut_Cos
   RCUT2B     6.5
   CGRID2B    LIN 10 0.0 6.5       # matching grid of centres
   SGRID2B    GEOM 10 0.05 0.70    # matching grid of widths

   # ----- many-body embedded density ----------------------------------
   INITMB  true
   TYPEMB     DM_EAD 1 7 7 Kr Kr  # L 1, 7 centres, 7 widths
   RCTYPEMB   Cut_Poly
   RCUTMB     6.5
   CGRIDMB    LIN 7 0.0 6.5
   SGRIDMB    GEOM 7 0.05 0.70

Composite 2-body terms with ``D2_mJoin``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   INIT2B  true

   TYPE2B     D2_mJoin                # meta descriptor

   TYPE2B     D2_MIE 12 6  Ti Ti      # --- component 1
   RCUT2B     5.0
   RCTYPE2B   Cut_Cos

   TYPE2B     D2_Blip 4 4  Ti Nb      # --- component 2
   RCUT2B     7.5
   RCTYPE2B   Cut_Poly
   CGRID2B    LIN 4 0.0 7.5
   SGRID2B    GEOM 4 0.05 0.70

   TYPE2B     D2_Blip 3 3  * *        # --- component 3
   RCUT2B     4.0
   RCTYPE2B   Cut_Cos
   CGRID2B    LIN 3 0.0 7.5
   SGRID2B    GEOM 3 0.05 0.70

Reading the tables below
------------------------

Following this overview you will find the *Two-Body* and *Many-Body* sections.
Each entry shows

* a short description and equation,
* **Required config keys**, i.e. the options you must specify in the
  configuration file.

Copy the class name into the ``TYPE2B`` / ``TYPEMB`` line, supply the required
keys, and you are ready to train.

--------------

Two-Body
--------

.. list-table::
   :header-rows: 1
   :widths: 14 86

   * - Label
     - Description
   * - ``D2_LJ``
     - Lennard-Jones 12-6 pair potential descriptor
   * - ``D2_BP``
     - Behler–Parrinello Gaussian radial symmetry functions
   * - ``D2_Blip``
     - Cubic B-spline (blip) radial basis
   * - ``D2_EAM``
     - Pair-density part of the Embedded Atom Method
   * - ``D2_MIE``
     - Mie potential with user-chosen exponents n, m
   * - ``D2_ZBL``
     - Ziegler–Biersack–Littmark screened nuclear repulsion
   * - ``D2_Dummy``
     - Identity descriptor (all ones); for testing
   * - ``D2_mJoin``
     - Concatenate multiple D2 descriptors into one vector

D2_LJ
~~~~~

.. doxygenclass:: tadah::models::D2_LJ

D2_BP
~~~~~

.. doxygenclass:: tadah::models::D2_BP

D2_Blip
~~~~~~~

.. doxygenclass:: tadah::models::D2_Blip

D2_EAM
~~~~~~

.. doxygenclass:: tadah::models::D2_EAM

D2_MIE
~~~~~~

.. doxygenclass:: tadah::models::D2_MIE

D2_ZBL
~~~~~~

.. doxygenclass:: tadah::models::D2_ZBL

D2_Dummy
~~~~~~~~

.. doxygenclass:: tadah::models::D2_Dummy

D2_mJoin
~~~~~~~~

.. doxygenclass:: tadah::models::D2_mJoin


Many-Body
---------

.. list-table::
   :header-rows: 1
   :widths: 16 84

   * - Label
     - Description
   * - ``DM_Blip``
     - Angular-momentum-projected blip basis; EAM-style density accumulation
   * - ``DM_EAD``
     - Embedded Atom Descriptor with Gaussian-type orbital basis
   * - ``DM_EAM``
     - Many-body part of the Embedded Atom Method
   * - ``DM_REAM``
     - Refittable EAM: flexible density and embedding expansions (``tadah refit``)
   * - ``DM_mRLR``
     - mEAD with s·ρ·log(c·ρ) embedding
   * - ``DM_mSQRT``
     - mEAD with s·√ρ embedding
   * - ``DM_mBlip``
     - mEAD with blip-function embedding
   * - ``DM_mBlipEnv``
     - mEAD with blip-function embedding gated by a Sin-inverse envelope
   * - ``DM_PS_SQ``
     - DM_Poly: blip radial, spherical, ρ² embedding
   * - ``DM_PS_SQRT``
     - DM_Poly: blip radial, spherical, √ρ embedding
   * - ``DM_PS_RLR``
     - DM_Poly: blip radial, spherical, ρ·log(ρ) embedding
   * - ``DM_PS_Blip``
     - DM_Poly: blip radial, spherical, blip embedding
   * - ``DM_GPS_SQ``
     - DM_Poly: Gaussian radial, spherical, ρ² embedding
   * - ``DM_GPS_SQRT``
     - DM_Poly: Gaussian radial, spherical, √ρ embedding
   * - ``DM_GPS_RLR``
     - DM_Poly: Gaussian radial, spherical, ρ·log(ρ) embedding
   * - ``DM_GPS_Blip``
     - DM_Poly: Gaussian radial, spherical, blip embedding
   * - ``DM_OW_SQ``
     - DM_Poly: blip radial, orbital-wise, ρ² embedding
   * - ``DM_OW_SQRT``
     - DM_Poly: blip radial, orbital-wise, √ρ embedding
   * - ``DM_OW_RLR``
     - DM_Poly: blip radial, orbital-wise, ρ·log(ρ) embedding
   * - ``DM_OW_Blip``
     - DM_Poly: blip radial, orbital-wise, blip embedding
   * - ``DM_GOW_SQ``
     - DM_Poly: Gaussian radial, orbital-wise, ρ² embedding
   * - ``DM_GOW_SQRT``
     - DM_Poly: Gaussian radial, orbital-wise, √ρ embedding
   * - ``DM_GOW_RLR``
     - DM_Poly: Gaussian radial, orbital-wise, ρ·log(ρ) embedding
   * - ``DM_GOW_Blip``
     - DM_Poly: Gaussian radial, orbital-wise, blip embedding
   * - ``DM_Dummy``
     - Identity descriptor (all ones); for testing
   * - ``DM_mJoin``
     - Concatenate multiple DM descriptors into one vector

DM_Blip
~~~~~~~

.. doxygenclass:: tadah::models::DM_Blip

DM_EAD
~~~~~~

.. doxygenclass:: tadah::models::DM_EAD

DM_EAM
~~~~~~

.. doxygenclass:: tadah::models::DM_EAM

.. _dm_ream:

DM_REAM
~~~~~~~

The native EAM many-body descriptor written by :ref:`tadah refit <refit>`.
Both EAM functions are basis expansions: the pair density
:math:`\rho(r)=W f_c(r)\sum_n a_n\,T_n(r)` (centres ``CGRIDMB``, widths
``SGRIDMB``, fixed amplitudes ``AMPGRIDMB``; :math:`T_n` depends on the
density form ``RHOFORM``, below) and the embedding
:math:`F(\rho)=\sum_m w_m\,\tilde g_m(\rho)` (centres ``CEMBFUNC``, widths
``SEMBFUNC``), whose coefficients :math:`w_m` are the model's linear weights.

.. _dm_ream_rhoform:

**Density form, RHOFORM.** The key ``RHOFORM SIGNED | NONNEG | MONOTONE`` is
**mandatory** for every DM_REAM potential and configuration: it has no
default, and nothing supplies it implicitly. With :math:`W` the ``WATOM``
pair weight (per species :math:`W_{ii}=w_i`,
:math:`W_{ij}=\sqrt{w_i w_j}`; or the listed pair value), :math:`f_c` the
many-body cutoff ``RCTYPEMB``, :math:`\psi_n` the basis function of centre
:math:`n` (the ``TYPEMB`` basis code) and :math:`S_n` its integrated bump
(below):

.. list-table::
   :header-rows: 1
   :widths: 13 37 50

   * - Form
     - Pair density
     - Guarantee
   * - ``SIGNED``
     - :math:`W f_c(r) \sum_n a_n\,\psi_n(r)`, amplitudes of any sign
     - None: :math:`\rho` can go negative (in the tail, say). This is the
       density of tadah 1.5.1 and earlier, bit for bit.
   * - ``NONNEG``
     - :math:`W \max(f_c(r),0) \sum_n a_n\,\psi_n(r)`, every
       :math:`a_n \ge 0`
     - :math:`\rho \ge 0` exactly. With the knot family, knots
       :math:`\le` ``RCUTMB`` and a non-increasing cutoff (every cutoff
       except ``Cut_SinInv``), :math:`\rho` is also non-increasing and
       convex.
   * - ``MONOTONE``
     - :math:`W \max(f_c(r),0) \sum_n a_n\,S_n(r)`, every
       :math:`a_n \ge 0`
     - :math:`\rho \ge 0` exactly and :math:`\rho' \le 0` (to rounding), so
       the lattice density :math:`\bar\rho(a)` is non-increasing in the
       lattice constant on every lattice.

On a density that is valid under both (every amplitude :math:`\ge 0`),
``NONNEG`` is bit-identical to ``SIGNED`` wherever :math:`f_c \ge 0`; the two
differ only where the cutoff dips below 0 (``Cut_Poly`` and ``Cut_PolyS``, by
about ``-2.7e-15`` near ``RCUTMB``). The key is global: every DM_REAM block,
including the children of a ``DM_mJoin``, uses the same form. Values are
case-insensitive and written in upper case. It is not an ``OPTIM`` target.

*The integrated bump.* :math:`S_n(r)` is the integral of :math:`\psi_n` from
:math:`r` outwards divided by its integral over the whole support, so
:math:`S_n = 1` inside the bump's inner support edge,
:math:`S_n(c_n) = 1/2` exactly and :math:`S_n = 0` beyond the outer edge
(the same outer edge as the bump's). With :math:`x = s_n (r - c_n)`
(:math:`s_n` the ``SGRIDMB`` width) and :math:`a = |x|`:

* ``Blip`` (basis code 0): :math:`Q(a) = (2-a)^4 - 4(1-a)_+^4`,
  :math:`S = Q/24` for :math:`x \ge 0` and :math:`1 - Q/24` for
  :math:`x < 0` (:math:`S = 1` for :math:`x \le -2`, 0 for
  :math:`x \ge 2`); :math:`dS/dr = -s_n B(x)/1.5`.
* ``Blip5`` (basis code 3): :math:`P(a) = (3-a)^6 - 6(2-a)_+^6 + 15(1-a)_+^6`,
  :math:`S = P/720` for :math:`x \ge 0` and :math:`1 - P/720` for
  :math:`x < 0` (support :math:`|x| < 3`);
  :math:`dS/dr = -s_n B_5(x)\cdot 11/20`.
* ``Gaussian`` (basis code 1):
  :math:`S = \tfrac12\,\mathrm{erfc}\big(\sqrt{s_n}\,(r - c_n)\big)`,
  :math:`dS/dr = -\sqrt{s_n/\pi}\,e^{-s_n (r-c_n)^2}`. Like the Gaussian
  itself, the step has no compact support.
* the knot family (basis code 2, any ``KDEGMB``): **refused** under
  ``MONOTONE``. Use ``NONNEG``: a non-negative knot density is already
  non-increasing with a non-increasing cutoff.

Under ``MONOTONE`` an amplitude is the height of a step, not of a bump:
:math:`\partial\rho/\partial a_n = W S_n f_c` equals :math:`W f_c`
everywhere inside the bump's inner edge, so :math:`a_n` is how much
:math:`\rho` drops across bump :math:`n`, and changing it moves the whole
density inside that bump's outer edge, the core included. Below the
innermost inner edge every :math:`S_n = 1`, so :math:`\rho = W f_c(r)
\sum_n a_n` there: constant under ``Cut_Dummy``, following the cutoff
otherwise. Keep this non-local meaning in mind when sizing
``OPTIM AMPGRIDMB`` boxes: a change to an outer amplitude shifts the density
by the same amount (times :math:`W f_c`) all the way in to the core.
Monotonicity holds to rounding only: the Blip5 step has 1-ulp rises for
:math:`|x| \lesssim 1.2`, and ``Cut_Poly``/``Cut_PolyS`` rise at the
``1e-15`` level near the cutoff.
``tadah analysis bfunc`` plots the basis functions :math:`\psi_n`, never the
steps :math:`S_n`.

*Validation* (in the constructor and on every HPO candidate; an invalid
density is an error naming the 1-based entry and its value). Every form:
the existing size checks (``CGRIDMB``/``SGRIDMB``/``AMPGRIDMB``/``KDEGMB``),
no zero ``SGRIDMB`` entry for the bump families and ``KDEGMB`` entries in
``{3, 5}``. ``NONNEG`` and ``MONOTONE`` add:

* every ``AMPGRIDMB`` entry finite and :math:`\ge 0`, and every ``CGRIDMB``
  entry finite;
* every density pair weight :math:`W > 0`, i.e. every ``WATOM`` value finite
  and :math:`> 0`. Without a ``WATOM`` line :math:`W = 1` where DM_REAM
  reads the potential directly (``tadah predict``, LAMMPS); ``tadah train``
  and ``tadah hpo`` first insert ``WATOM`` = Z (the atomic number), so there
  :math:`W = Z`;
* ``NONNEG``: every ``SGRIDMB`` entry finite, and :math:`> 0` for the
  Gaussian basis;
* ``MONOTONE``: every ``SGRIDMB`` entry finite and :math:`> 0` (a step with
  a negative width rises), a bump basis (not the knot family), and a
  many-body cutoff on the list of measured non-negative, non-increasing
  cutoffs: ``Cut_Dummy``, ``Cut_Cos``, ``Cut_CosS``, ``Cut_Tanh``,
  ``Cut_PT``, ``Cut_Poly`` and ``Cut_PolyS``. ``Cut_SinInv`` (increasing)
  and any other cutoff are refused.

*Migration.* A DM_REAM potential or configuration without ``RHOFORM`` is
refused with::

   DM_REAM: RHOFORM is required: SIGNED (rho = W f_c sum a_n psi_n with signed
   a_n, the tadah <= 1.5.1 density; add 'RHOFORM SIGNED' to use an existing
   potential unchanged), NONNEG (the same with every a_n >= 0: rho >= 0 by
   construction) or MONOTONE (rho = W f_c sum a_n S_n(r), S_n the integrated
   bump, a_n >= 0: rho >= 0 and non-increasing by construction).

Add ``RHOFORM SIGNED`` to a potential written by tadah 1.5.1 or earlier to
use it unchanged (bit for bit). ``tadah predict``, ``tadah analysis
descriptor`` and ``tadah properties pairwise`` refuse a key-less DM_REAM
potential even when the task configuration sets ``RHOFORM`` (the form
belongs to the potential, whose amplitudes were fitted in it), and a task
value that differs from the potential's is refused as well. The LAMMPS pair
style refuses a key-less potential with ``pair_coeff: failed to load Tadah
potential from '<file>': DM_REAM: RHOFORM is required: ...``, and on a
successful load its banner states the form (``Tadah! DM_REAM density form
RHOFORM <form>``). Every DM_REAM ``pot.tadah`` that tadah writes carries the
``RHOFORM`` line (``tadah train``, ``refit``, ``refit --edit`` and ``hpo``),
and binaries older than the key refuse such files with ``Invalid key:
RHOFORM``. Changing the key of an existing potential is safe only from
``SIGNED`` to ``NONNEG`` when every amplitude is already :math:`\ge 0` and
every pair weight :math:`> 0` (the density is then unchanged);
``MONOTONE`` reads the same amplitudes as step heights, which is a different
density. To get a density in a positive form, fit it in that form:
:ref:`tadah refit <refit_rhoform>` with ``RHOFORM`` in the refit
configuration.

**Embedding reference, EMBF0.** An isolated atom has :math:`\rho = 0`, so its
embedding energy is :math:`F(0)`. The key ``EMBF0`` selects how :math:`F(0)`
is treated:

* ``EMBF0 ZERO`` (default): every embedding basis function is
  zero-referenced, :math:`\tilde g_m(\rho) = g_m(\rho) - g_m(0)`, with
  :math:`g_m(0)` computed from the ``CEMBFUNC``/``SEMBFUNC``/``KDEGEMB``
  values actually evaluated (for every HPO candidate as well). Hence
  :math:`F(0) = 0` exactly for any weights: with a linear model and
  ``BIAS false`` an isolated atom has energy 0.0, on every path (``tadah
  predict``/``train``/``hpo`` and the LAMMPS pair style, all kernels).
* ``EMBF0 FREE``: the raw basis :math:`\tilde g_m = g_m` of tadah 1.5.0 and
  earlier, where :math:`F(0) = \sum_m w_m g_m(0)` is whatever the weights
  make it.

``EMBF0`` is a property of the potential: every DM_REAM ``pot.tadah`` that
tadah writes carries an explicit ``EMBF0`` line (binaries older than the key
refuse such files with ``Invalid key: EMBF0``), and a potential without the
line is read as ``ZERO``. It takes ``ZERO`` or ``FREE`` only, in any case;
any other value is an error at its line when the configuration, task file
or potential is read (``Invalid value for key EMBF0 on line N of
pot.tadah: 'ZEROO' (allowed: ZERO, FREE)``). It is not an ``OPTIM`` target (:ref:`hpo_embf0`). ``EMBF0`` has no
effect on a potential without a DM_REAM block: ``tadah train`` and ``tadah
refit`` do not write it into such a potential (a warning says so, once per
process), and ``tadah predict`` ignores a task value for it with a warning;
it is kept, rather than refused, so that one configuration can serve
several descriptor choices.

.. list-table:: What changes between ZERO and FREE for the same weights
   :header-rows: 1
   :widths: 30 70

   * - Model
     - Effect
   * - Linear (``BF_Linear``, ``Kern_Linear``)
     - The derivatives :math:`g'_m` are identical, so forces and stresses are
       identical; every atomic energy moves by the same constant
       (:math:`-F(0)`, from the stored ``WEIGHTS``, with or without
       ``NORM``). Energy differences at fixed atom count, and cohesive or
       formation energies, are unchanged.
   * - Non-linear (``BF_Polynomial2``, non-linear KRR kernels)
     - The descriptor values change, so energies **and** forces change.
   * - ``DM_mJoin`` containing DM_REAM
     - One global setting for every DM_REAM block; only the DM_REAM block
       vanishes at :math:`\rho = 0`, other many-body blocks keep their value.

To reproduce the energies of a potential written by tadah 1.5.0 or earlier,
add ``EMBF0 FREE`` to it. A trained DM_REAM potential loaded without the key
prints a warning: ``tadah`` logs it once per process, and the LAMMPS pair
style prints a LAMMPS ``WARNING`` (screen and ``log.lammps``) on every
``pair_coeff`` that loads one, with a banner that reads ``EMBF0 ZERO (key
absent: default ZERO, ...)``. Training on absolute reference energies (e.g. DFT totals) under
``ZERO`` needs a per-atom reference energy from ``ESHIFT``, ``ESHIFT_ATOM`` or
``ESHIFT_DBATOM`` (compatible with the LAMMPS ``EAMFAST`` kernel) or
``BIAS true`` (disables ``EAMFAST``; its intercept column precedes the
descriptor weights, so ``FIXINDEX``/``FIXWEIGHT`` indices shift). External tools that tabulate the embedding from ``WEIGHTS``
(setfl converters) must tabulate :math:`F(\rho) - F(0)` unless the potential
says ``EMBF0 FREE``.

.. doxygenclass:: tadah::models::DM_REAM

DM_mEAD variants
~~~~~~~~~~~~~~~~

These descriptors combine the mEAD density accumulation with a non-linear
embedding function **F** applied before the quadratic sum.  Choose the label
that matches the embedding function you want:

.. list-table::
   :header-rows: 1
   :widths: 18 12 70

   * - Label
     - Embedding F
     - Form
   * - ``DM_mRLR``
     - F_RLR
     - s · ρ · log(c · ρ)
   * - ``DM_mSQRT``
     - F_SQRT
     - s · √ρ
   * - ``DM_mBlip``
     - F_Blip
     - blip-function B(ρ)
   * - ``DM_mBlipEnv``
     - F_mEnv
     - B(ρ) · Sin-inverse envelope (suppresses contribution near ρ = 0)

.. doxygenclass:: tadah::models::DM_mEAD

Embedding functions
~~~~~~~~~~~~~~~~~~~

.. doxygenclass:: tadah::models::F_RLR

.. doxygenclass:: tadah::models::F_SQ

.. doxygenclass:: tadah::models::F_SQRT

.. doxygenclass:: tadah::models::F_Blip

.. doxygenclass:: tadah::models::F_mEnv

DM_Poly variants
~~~~~~~~~~~~~~~~

``DM_Poly`` is a generalised many-body descriptor with three independent
choices:

* **Radial basis** – how the atomic density is expanded radially:
  blip (cubic B-spline) or Gaussian.
* **Aggregation** – spherical (rotationally invariant sum) or
  orbital-wise (one channel per angular-momentum orbital).
* **Embedding function F** – applied per-channel before the invariant sum:
  ρ² (``SQ``), √ρ (``SQRT``), ρ·log(ρ) (``RLR``), or blip (``Blip``).

The 16 registered labels follow the pattern
``DM_{radial}{aggregation}_{embedding}``:

.. list-table::
   :header-rows: 1
   :widths: 20 18 16 46

   * - Label
     - Radial basis
     - Aggregation
     - Embedding F
   * - ``DM_PS_SQ``
     - Blip
     - Spherical
     - ρ²
   * - ``DM_PS_SQRT``
     - Blip
     - Spherical
     - √ρ
   * - ``DM_PS_RLR``
     - Blip
     - Spherical
     - ρ·log(ρ)
   * - ``DM_PS_Blip``
     - Blip
     - Spherical
     - blip B(ρ)
   * - ``DM_GPS_SQ``
     - Gaussian
     - Spherical
     - ρ²
   * - ``DM_GPS_SQRT``
     - Gaussian
     - Spherical
     - √ρ
   * - ``DM_GPS_RLR``
     - Gaussian
     - Spherical
     - ρ·log(ρ)
   * - ``DM_GPS_Blip``
     - Gaussian
     - Spherical
     - blip B(ρ)
   * - ``DM_OW_SQ``
     - Blip
     - Orbital-wise
     - ρ²
   * - ``DM_OW_SQRT``
     - Blip
     - Orbital-wise
     - √ρ
   * - ``DM_OW_RLR``
     - Blip
     - Orbital-wise
     - ρ·log(ρ)
   * - ``DM_OW_Blip``
     - Blip
     - Orbital-wise
     - blip B(ρ)
   * - ``DM_GOW_SQ``
     - Gaussian
     - Orbital-wise
     - ρ²
   * - ``DM_GOW_SQRT``
     - Gaussian
     - Orbital-wise
     - √ρ
   * - ``DM_GOW_RLR``
     - Gaussian
     - Orbital-wise
     - ρ·log(ρ)
   * - ``DM_GOW_Blip``
     - Gaussian
     - Orbital-wise
     - blip B(ρ)

.. doxygenclass:: tadah::models::DM_Poly

DM_Dummy
~~~~~~~~

.. doxygenclass:: tadah::models::DM_Dummy

DM_mJoin
~~~~~~~~

.. doxygenclass:: tadah::models::DM_mJoin
