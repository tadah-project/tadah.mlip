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
Both EAM functions are basis expansions: the density
:math:`\rho(r)=\sum_n a_n\,\psi_n(r)` (centres ``CGRIDMB``, widths
``SGRIDMB``, fixed amplitudes ``AMPGRIDMB``) and the embedding
:math:`F(\rho)=\sum_m w_m\,\tilde g_m(\rho)` (centres ``CEMBFUNC``, widths
``SEMBFUNC``), whose coefficients :math:`w_m` are the model's linear weights.

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
