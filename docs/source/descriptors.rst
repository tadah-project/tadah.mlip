.. _descriptors:

Descriptors
===========

This page describes the two-body (``D2_*``) and many-body (``DM_*``)
descriptors of Tadah! and the keys that select and configure them in the
training configuration (format: :ref:`config_file`; keys:
:ref:`ConfigSection`). The heading of each descriptor below is exactly the
value written after ``TYPE2B`` or ``TYPEMB`` in the training configuration.
The headings ``DM_mEAD variants`` and ``DM_Poly variants`` name families of
descriptors instead; their tables list the labels.

For new scalar-density work, :ref:`DM_Embed <dm_embed>` selects density basis,
embedding basis, spatial cutoff and optional envelope in independent component
blocks. Existing flat descriptors remain supported.

General workflow
----------------

#.  A configuration uses two-body descriptors, many-body descriptors, or both.
#.  Each family is switched **on** with the corresponding *INIT* flag::

        INIT2B  true      # activate two-body block
        INITMB  false     # skip many-body block

    At least one of ``INIT2B`` / ``INITMB`` must be ``true``.  If both are
    ``true`` Tadah! builds one descriptor of each type **unless** ``TYPE2B`` /
    ``TYPEMB`` say otherwise.
#.  Each descriptor to build has **exactly one** line, in the formats below.
    Several descriptors of the same family are concatenated with the meta
    classes ``D2_mJoin`` or ``DM_mJoin``, which list the components *in a
    block* right after the meta keyword.

.. _bias_term:

The bias term
-------------

``BIAS true`` gives the model one constant energy per species: every atom's
descriptor starts with :math:`N` one-hot intercept columns, one per ``ATOM``
species in ``ATOM`` order (1 in the column of the atom's species, 0 in the
others), followed by the two-body and the many-body descriptors.

.. code-block:: ini

   ATOM  Nb Ta
   BIAS  true      # descriptor: [Nb, Ta, two-body ..., many-body ...]

The layout is the same on every path (``tadah train``, ``predict``,
``hpo``, ``properties``, ``analysis`` and ``pair_style tadah``):

* With a linear model (``BF_Linear``, ``Kern_Linear``) ``WEIGHTS`` 1 to
  :math:`N` are the per-species constants, in ``ATOM`` order, and the
  descriptor coefficients follow, so ``FIXINDEX`` indices of descriptor
  coefficients start at :math:`N + 1`. With ``BF_Polynomial2`` the constant
  of species :math:`k` is the product of its intercept component with itself,
  :math:`(k,k)`: coefficients 1 and :math:`D + 1` for two species
  (:math:`D` = ``DSIZE``). With a dot-product kernel (``Kern_Quadratic``,
  ``Kern_LQ``, ``Kern_Polynomial``, ``Kern_Sigmoid``) coefficients 1 to
  :math:`N` belong to the intercept basis vectors. ``tadah train`` and
  ``tadah hpo`` print a WARNING when ``FIXINDEX`` is used with ``BIAS true``
  and two or more species, naming the model's intercept coefficients.
* ``BIAS true`` requires ``ATOM``; an atom of an element ``ATOM`` does not
  list has no intercept column and is refused.
* The ridge (``LAMBDA``) does not penalise the intercepts, also when
  ``FIXINDEX`` pins some of them, and penalises every other coefficient;
  ``NORM`` does not normalise them.
* ``FIXINDEX``/``FIXWEIGHT`` is refused with ``NORM true`` (``tadah train``
  and ``tadah hpo``). The fit would pin coefficients of the normalised
  descriptors, and their conversion to coefficients of the raw descriptors
  moves them, so the potential would not hold the ``FIXWEIGHT`` values it
  records.
* ``NORM true`` with ``BF_Linear`` or ``Kern_Linear`` requires
  ``BIAS true``. The coefficients fitted to the normalised descriptors are
  converted to coefficients of the raw descriptors, and the constant this
  moves goes into the intercepts. A linear potential with ``NORM true`` and
  ``BIAS false`` is refused when it predicts; it needs retraining with
  ``NORM false`` or ``BIAS true``.
* Only energies determine the constants (their force and stress rows are
  zero). The training structures whose energies the fit observes must
  therefore have compositions that determine the model's intercepts. An
  energy is observed when its priority (``eweight`` x ``WDBFILE``) is above 0
  and ``ENERGY`` is true (:ref:`fit_objective`).
* Data whose structures all have one composition are refused before the fit,
  and so are data with no observed energy. One composition means, for
  example, a fixed stoichiometry, or ``Nb27 Ta27`` and ``Nb54 Ta54``. The
  message names the species, the rank found, the model's intercept
  coefficients and the remedies.
* Under ``NORM false`` there are three remedies. The first is ``BIAS false``
  with explicit ``ESHIFT`` values (a per-species reference energy, such as
  the isolated atom's) or ``ESHIFT_ATOM`` (isolated-atom structures in the
  data). The second is structures of other compositions (for example of the
  pure elements, or an isolated atom of each species). The third is
  intercepts pinned with ``FIXINDEX``/``FIXWEIGHT`` (the coefficients the
  message names). Under ``NORM true`` a linear model needs ``BIAS true`` and
  ``FIXINDEX`` is refused, so the remedies are structures of other
  compositions or ``NORM false``.
* ``ESHIFT_DBATOM`` is no remedy: its least-squares fit of the species
  energies is singular on such data and sets every ``ESHIFT`` to 0 (with a
  WARNING), so the constant goes into the descriptor coefficients.
  ``tadah train`` checks before it computes any descriptor, ``tadah hpo`` at
  startup.
* A non-linear kernel's intercepts are those of the basis its fit draws,
  which starts with the intercept vectors
  (:ref:`Non-linear models <nonlinear_models>`), whatever basis a potential
  used as the training configuration holds: a dot-product kernel's
  coefficients 1 to :math:`N`. A model without intercept columns
  (``Kern_RBF``) is not refused, and neither is ``M_KRR`` implementation
  ``2``, whose energies-only solve penalises every coefficient.
* With ``BF_Polynomial2`` the products of two different intercept columns
  are identically zero (an atom has one species): :math:`N (N - 1) / 2` of
  its :math:`D (D + 1) / 2` coefficients have no data (the ridge penalises
  them, so with ``LAMBDA > 0`` they are 0). With a non-linear KRR kernel the
  basis starts with one intercept vector per species.

``tadah predict`` and ``pair_style tadah`` refuse a potential with ``BIAS
true`` and two or more species whose ``WEIGHTS`` are those of one intercept
column shared by the species. The message names ``WEIGHTS``, ``DSIZE``,
``BIAS`` and ``ATOM``.

A potential refused for its intercept columns is retrained with its
training configuration or with the potential itself
(:ref:`retrain_from_potential`; an ``M_KRR`` potential without a
``DBFILE`` line needs one added): ``tadah train -c pot.tadah`` replaces its
``WEIGHTS``, ``NMEAN`` and ``NSTDEV``.

Descriptor key syntax
---------------------

Two-body::

   TYPE2B  D2_<Name>   [param ...]   <EL1> <EL2> ...

Many-body::

   TYPEMB  DM_<Name>  L  N_C  N_S  [N_CE  N_SE]  <EL1> <EL2> ...

where

* ``<Name>`` is copied from the headings below (case-sensitive).
* ``[param …]`` are the extra integers/doubles required by a given class
  (its table gives the details).
* ``<EL1> <EL2>`` are element symbols (``*`` stands for "any"). Multiple pairs
  are allowed, e.g. ``DM_EAD 1 4 4 Ti Ti Ti Nb``.
* ``L`` is the maximum angular momentum number needed by DM descriptors.
* ``N_C``, ``N_S`` are the sizes of the *centre* and *width* grids
  (``CGRID*`` / ``SGRID*``).
* ``N_CE``, ``N_SE`` appear only when a non-linear embedding function is used
  (``CEMBFUNC`` / ``SEMBFUNC``).

Each descriptor needs matching auxiliary keys:

=============  =========================================================
``RCTYPE2B``   cutoff function(s) – one per descriptor in ``TYPE2B``
``RCUT2B``     cutoff distance(s)
``CGRID2B``    centre grid(s) – if the descriptor uses them
``SGRID2B``    width  grid(s) – same length as the corresponding centres
=============  =========================================================

(and the analogous ``*MB`` variants for many-body descriptors).

Ordering matters: the recommended layout keeps the block

.. code-block:: ini

   TYPE2B   …
   RCTYPE2B …
   RCUT2B   …
   CGRID2B  …
   SGRID2B  …

together, before the next descriptor family starts, so the eye can verify
that list lengths match.

.. _composite_descriptors:

Composite Descriptors
---------------------

Tadah!MLIP **stitches several primitive descriptors together into one
feature vector** – a *composite descriptor* – which embeds physical insight
(e.g. "a short-range ZBL shield added to a Blip term") without editing
source code.  The idea is implemented via the meta-classes ``D2_mJoin``
(two-body) and ``DM_mJoin`` (many-body).

Key points
~~~~~~~~~~

* The relevant family is activated first: ``INIT2B`` / ``INITMB``.
* The meta descriptor is declared:

  ``TYPE2B  D2_mJoin``  *or* ``TYPEMB  DM_mJoin``

* One **TYPE** line per constituent descriptor follows immediately, in
  the order of concatenation.
* Each constituent takes one ``RCTYPE*`` and one ``RCUT*`` value, in the
  order of the **TYPE** lines.
* The values of a list key (``CGRID*``, ``SGRID*``, ``C/SEMBFUNC`` and
  the other lists a descriptor takes) form one list, whatever lines they
  are written on. In a potential, each such key has its one line
  (:ref:`retrain_from_potential`). Each constituent takes as many values
  as its **TYPE** line asks for, in the order of the **TYPE** lines, as a
  single descriptor does.
* More or fewer values are refused, and so is a list key that no
  constituent reads.
* A ``DM_Poly`` descriptor with the RLR or Blip embedding reads
  ``C/SEMBFUNC`` whole and cannot be a constituent; one with the SQRT
  embedding is joined with every ``SEMBFUNC`` value 1.
* Each constituent can target its own element pair(s), cutoff type and
  distance.

In a potential, some of these values give a warning that names them
instead of a refusal, and the potential loads. They are ``RCTYPE*`` and
``RCUT*`` values beyond one per descriptor, the list values that the
descriptors of a joined line leave over, and a list key that a single
descriptor does not read. A list key of a single descriptor with more
values than its **TYPE** line asks for is refused in a potential too.

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
   CGRID2B    LIN 3 0.0 3.5
   SGRID2B    GEOM 3 0.05 0.70

Reading the tables below
------------------------

The *Two-Body* and *Many-Body* sections follow this overview.
Each entry shows

* a short description and equation,
* **Required config keys**, i.e. the options the configuration file must
  specify.

With the class name in the ``TYPE2B`` / ``TYPEMB`` line and the required keys
supplied, the configuration is ready to train.

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
     - Pair function of the Embedded Atom Method, from the setfl file ``SETFL``
   * - ``D2_MIE``
     - Mie potential with user-chosen exponents n, m
   * - ``D2_ZBL``
     - Ziegler–Biersack–Littmark screened nuclear repulsion
   * - ``D2_Dummy``
     - Identity descriptor (all ones); for testing
   * - ``D2_mJoin``
     - Concatenates multiple D2 descriptors into one vector

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
     - Many-body part of the Embedded Atom Method (density and embedding function), from the same ``SETFL`` file
   * - ``DM_Embed``
     - Scalar EAM-form density and embedding composed in a ``DESCRIPTOR`` block (``tadah refit`` writes it); :ref:`dm_embed`
   * - ``DM_REAM``
     - Refittable EAM: flexible density and embedding expansions
   * - ``DM_mRLR``
     - mEAD with s·ρ·log(c·ρ) embedding
   * - ``DM_mSQRT``
     - mEAD with s·√ρ embedding
   * - ``DM_mBlip``
     - mEAD with blip-function embedding
   * - ``DM_mBlipEnv``
     - mEAD with blip-function embedding and a Sin-inverse envelope
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
     - Concatenates multiple DM descriptors into one vector

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

The native EAM many-body descriptor. :ref:`tadah refit <refit>` writes the
many-body term as a :ref:`DM_Embed <dm_embed>` component; DM_REAM potentials
load, train, predict and edit. Both EAM functions are basis expansions: the pair density
:math:`\rho(r)=W f_c(r)\sum_n a_n\,T_n(r)` (centres ``CGRIDMB``, widths
``SGRIDMB``, fixed amplitudes ``AMPGRIDMB``; :math:`T_n` depends on the
density form ``RHOFORM``, below) and the embedding
:math:`F(\rho)=\sum_m w_m\,\tilde g_m(\rho)` (centres ``CEMBFUNC``, widths
``SEMBFUNC``), whose coefficients :math:`w_m` enter the model linearly.

.. _dm_ream_rhoform:

**Density form, RHOFORM.** The key ``RHOFORM SIGNED | NONNEG | MONOTONE`` is
**mandatory** for every DM_REAM potential and configuration: it has no
default, and nothing supplies it implicitly. With :math:`W` the ``WATOM``
pair weight (per species :math:`W_{ii}=w_i`,
:math:`W_{ij}=\sqrt{w_i w_j}`; or the listed pair value), :math:`f_c` the
many-body cutoff ``RCTYPEMB``, :math:`\psi_n` the basis function of centre
:math:`n` (the ``TYPEMB`` basis code) and :math:`S_n` its integrated bump
(below):

``SIGNED``
   Pair density :math:`W f_c(r) \sum_n a_n\,\psi_n(r)`, amplitudes of any
   sign. Guarantee: none, :math:`\rho` can go negative (in the tail, say).

``NONNEG``
   Pair density :math:`W \max(f_c(r),0) \sum_n a_n\,\psi_n(r)`, every
   :math:`a_n \ge 0`. Guarantee: :math:`\rho \ge 0` exactly. With the knot
   family, knots :math:`\le` ``RCUTMB`` and a non-increasing cutoff (every
   cutoff except ``Cut_SinInv``), :math:`\rho` is also non-increasing and
   convex.

``MONOTONE``
   Pair density :math:`W \max(f_c(r),0) \sum_n a_n\,S_n(r)`, every
   :math:`a_n \ge 0`. Guarantee: :math:`\rho \ge 0` exactly and
   :math:`\rho' \le 0` (to rounding), so the lattice density
   :math:`\bar\rho(a)` is non-increasing in the lattice constant on every
   lattice.

On a density that is valid under both (every amplitude :math:`\ge 0`),
``NONNEG`` is bit-identical to ``SIGNED`` wherever :math:`f_c \ge 0`; the two
differ only where the cutoff dips below 0 (``Cut_Poly`` and ``Cut_PolyS``, by
about ``-2.7e-15`` near ``RCUTMB``). ``SIGNED`` evaluates the sums of the
density with Tadah.MODELS' own flags.

In a Tadah.MODELS compiled without fused multiply-adds (FMA), the other
evaluators of the density agree with the descriptor: bit for bit the density
checks and reports of ``tadah hpo`` and ``tadah refit --edit``, within one
unit in the last place the density ``tadah refit`` fits. The flags of the
presets on x86-64 (no ``-march=native``, no ``-ffast-math``) compile it
without FMA, with or without LTO, and also in a ``tadah`` built with
``TADAH_MARCH_NATIVE``. In a Tadah.MODELS compiled with FMA
(``-march=native`` in ``CMAKE_CXX_FLAGS`` on a recent x86-64, or on an
AArch64 machine, where the compilers fuse multiply-adds at their default
flags), these relations hold to rounding errors only.

The Gaussian basis, and every cutoff except ``Cut_Dummy``, ``Cut_Poly`` and
``Cut_PolyS``, call mathematical functions of the C library (``cos``,
``exp``, ``tanh`` and others). The last bits of a density that uses one of
them depend on the C library (for example glibc or musl) and on the code
that the C library selects for the processor.

The key is global: every DM_REAM block, including the
children of a ``DM_mJoin``, uses the same form. Values are
case-insensitive and written in upper case; any other value is an error at
its line of the configuration or potential file (``Invalid value for key
RHOFORM on line 24 of pot.tadah: 'SQUARE' (allowed: SIGNED, NONNEG,
MONOTONE)``), as for ``EMBF0``. It is not an ``OPTIM`` target.

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
  ``MONOTONE``; ``NONNEG`` serves instead: a non-negative knot density is
  already non-increasing with a non-increasing cutoff.

Under ``MONOTONE`` an amplitude is the height of a step, not of a bump:
:math:`\partial\rho/\partial a_n = W S_n f_c` equals :math:`W f_c`
everywhere inside the bump's inner edge, so :math:`a_n` is how much
:math:`\rho` drops across bump :math:`n`, and changing it moves the whole
density inside that bump's outer edge, the core included. Below the
innermost inner edge every :math:`S_n = 1`, so :math:`\rho = W f_c(r)
\sum_n a_n` there: constant under ``Cut_Dummy``, following the cutoff
otherwise. This non-local meaning matters when sizing
``OPTIM AMPGRIDMB`` boxes: a change to an outer amplitude shifts the density
by the same amount (times :math:`W f_c`) all the way in to the core.
Monotonicity holds to rounding only: the Blip5 step has 1-ulp rises for
:math:`|x| \lesssim 1.2`, and ``Cut_Poly``/``Cut_PolyS`` rise at the
``1e-15`` level near the cutoff.
``tadah analysis bfunc`` plots the basis functions :math:`\psi_n`, never the
steps :math:`S_n`.

*Validation* (when the descriptor is built and in every evaluation of
``tadah hpo``; an invalid
density is an error naming the 1-based entry and its value). Every form:
the ``TYPEMB`` basis code (the token after ``DM_REAM``) is an integer naming
a known basis, 0 to 3 (a code that is not an integer, or no code, is an
error, ``DM_REAM: TYPEMB basis code 'x' is not an integer (known: 0=Blip,
1=Gaussian, 2=Knot5, 3=Blip5).``, not the Blip basis); the existing size
checks (``CGRIDMB``/``SGRIDMB``/``AMPGRIDMB``/``KDEGMB``), no zero
``SGRIDMB`` entry for the bump families and ``KDEGMB`` entries in
``{3, 5}``. ``WATOM`` values are read with the decimal number grammar of
potential files (a sign, digits with at most one ``.``, an optional
exponent; a finite value) by the model, the density checks and the density
checks of ``tadah hpo`` alike: anything else, such as ``0x1p-1``, ``inf`` or ``nan``,
is an error (``WATOM entry 1 ('0x1p-1') is not a decimal number: ...``).
``NONNEG`` and ``MONOTONE`` add:

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
   a_n: the density before tadah 1.6.0, which a RHOFORM SIGNED line declares
   for an existing potential, with an EMBF0 FREE line for its embedding
   reference), NONNEG (the same with every a_n >= 0: rho >= 0 by
   construction) or MONOTONE (rho = W f_c sum a_n S_n(r), S_n the integrated
   bump, a_n >= 0: rho >= 0 and non-increasing by construction).

``tadah predict``, ``tadah analysis
descriptor``, ``tadah analysis bfunc`` and ``tadah properties pairwise`` refuse a key-less DM_REAM
potential (the form belongs to the potential, whose amplitudes were fitted
in it), and ``tadah refit --edit`` refuses a ``RHOFORM`` of its
configuration file that differs from the potential's, or that a key-less
potential would take. The LAMMPS pair
style refuses a key-less potential with ``pair_coeff: failed to load Tadah
potential from '<file>': DM_REAM: RHOFORM is required: ...``, and on a
successful load its banner states the form (``Tadah! DM_REAM density form
RHOFORM <form>``). Every DM_REAM ``pot.tadah`` that tadah writes carries the
``RHOFORM`` line (``tadah train``, ``refit``, ``refit --edit`` and ``hpo``).
Changing the key of an existing potential is safe only from
``SIGNED`` to ``NONNEG`` when every amplitude is already :math:`\ge 0` and
every pair weight :math:`> 0` (the density is then unchanged);
``MONOTONE`` reads the same amplitudes as step heights, which is a different
density. A density in a positive form is fitted in that form:
:ref:`tadah refit <refit_rhoform>` with ``RHOFORM`` in the refit
configuration.

.. _dm_ream_only_keys:

*RHOFORM and EMBF0 without a DM_REAM block.* Both keys apply only to a
DM_REAM many-body block (bare or inside a ``DM_mJoin``). A value that is not
allowed (``RHOFORM SQUARE``) is an error at its line whatever the
potential, but a valid ``RHOFORM`` or ``EMBF0`` in a potential or
configuration without a DM_REAM block has no effect. A fit (``tadah
train``, ``tadah hpo``) refuses a configuration that has one, naming the
key, its line and its file, as it refuses a :ref:`deprecated key <MBLOCK>`
(``tadah refit`` of a configuration without ``DESCRIPTOR`` blocks, which
reads both keys itself, takes them). A potential
that has one still loads; the key is never written into a potential, and
it is reported on standard error once per key and process, like a
deprecated key::

   WARNING: Key RHOFORM is ignored: it applies only to a DM_REAM many-body
   block, and the potential has none (the key has no effect and is not
   written to a potential).

The notice comes from wherever such a potential is used (``tadah predict``,
``analysis``, a program that builds a model from it), also when only a
configuration file given with ``-c`` sets the key. Neither the log level nor ``VERBOSE 0``
hides it. ``pair_style tadah`` writes it as a LAMMPS warning on rank 0
instead, in ``log.lammps`` and on the screen, each time ``pair_coeff``
loads such a potential (every LAMMPS instance of a process, such as the
evaluations of ``tadah hpo``'s ``PC_LAMMPS``, has it in its own log)::

   WARNING: pair tadah: potential 'pot.tadah': Key RHOFORM is ignored: ...

**Embedding reference, EMBF0.** An isolated atom has :math:`\rho = 0`, so its
embedding energy is :math:`F(0)`. The key ``EMBF0`` selects how :math:`F(0)`
is treated:

* ``EMBF0 ZERO`` (default): every embedding basis function is
  zero-referenced, :math:`\tilde g_m(\rho) = g_m(\rho) - g_m(0)`, with
  :math:`g_m(0)` computed from the ``CEMBFUNC``/``SEMBFUNC``/``KDEGEMB``
  values actually evaluated (in every evaluation of ``tadah hpo`` as well). Hence
  :math:`F(0) = 0` exactly for any coefficients: with a linear model and
  ``BIAS false`` an isolated atom has energy 0.0, on every path (``tadah
  predict``/``train``/``hpo`` and the LAMMPS pair style, all kernels).
* ``EMBF0 FREE``: the raw basis :math:`\tilde g_m = g_m`, where
  :math:`F(0) = \sum_m w_m g_m(0)` is whatever the coefficients make it.

``EMBF0`` is a property of the potential: every DM_REAM ``pot.tadah`` that
tadah writes carries an explicit ``EMBF0`` line, and a potential without the
line is read as ``ZERO``. It takes ``ZERO`` or ``FREE`` only, in any case;
any other value is an error at its line when the configuration file or
potential is read (``Invalid value for key EMBF0 on line N of
pot.tadah: 'ZEROO' (allowed: ZERO, FREE)``). It is not an ``OPTIM`` target (:ref:`nfp_search_space_components`). ``EMBF0`` has no
effect on a potential without a DM_REAM block (:ref:`RHOFORM and EMBF0 without a DM_REAM block <dm_ream_only_keys>`).

.. list-table:: What changes between ZERO and FREE for the same coefficients
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

A potential that has no ``EMBF0`` line and whose coefficients were fitted
in the raw basis reproduces its energies with ``EMBF0 FREE`` added. A
trained DM_REAM potential loaded without the key
prints a warning: ``tadah`` logs it once per process, and the LAMMPS pair
style prints a LAMMPS ``WARNING`` (screen and ``log.lammps``) on every
``pair_coeff`` that loads one, with a banner that reads ``EMBF0 ZERO (key
absent: default ZERO, ...)``. Training on absolute reference energies, such as DFT totals, under
``ZERO`` needs a per-atom reference energy from ``ESHIFT``, ``ESHIFT_ATOM`` or
``ESHIFT_DBATOM`` (compatible with the LAMMPS ``EAMFAST`` kernel) or
``BIAS true`` (disables ``EAMFAST``; its intercept columns, one per ``ATOM``
species, precede the descriptor coefficients, so ``FIXINDEX``/``FIXWEIGHT``
indices shift). External tools that tabulate the embedding from ``WEIGHTS``
(setfl converters) must tabulate :math:`F(\rho) - F(0)` unless the potential
says ``EMBF0 FREE``.

.. doxygenclass:: tadah::models::DM_REAM

.. _dm_mead_variants:

DM_mEAD variants
~~~~~~~~~~~~~~~~

These descriptors combine the mEAD density accumulation with a non-linear
embedding function **F** applied before the quadratic sum.  The label
selects the embedding function:

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

.. _descriptor_analytical_derivatives:

Analytical derivatives in tadah hpo
-----------------------------------

The analytical gradient of ``tadah hpo`` (``GRAD_STRATEGY ANALYTICAL`` or
``HYBRID``, :ref:`nfp_choosing_gradients`) differentiates the parameters of
the descriptors below, in the energy and the force rows unless the table
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
(:ref:`dm_embed`).

Most two-body and many-body descriptors read :ref:`WATOM` (the key names
those that do not), so both descriptors of a configuration usually depend
on it. ``DM_REAM``, the ``DM_Poly`` descriptors, ``DESCRIPTOR`` blocks,
``DM_mJoin`` and ``D2_mJoin`` depend on ``WATOM`` and give no derivative
with respect to it. A ``WATOM`` value that weighs their pairs has no
analytical derivative, whatever the other descriptor gives. ``HYBRID``
takes it by finite differences, and ``ANALYTICAL`` refuses it at
startup. Beside ``DM_Blip``, ``DM_EAD``, ``DM_mRLR`` or ``DM_mSQRT`` it
has one in the energy rows only.
