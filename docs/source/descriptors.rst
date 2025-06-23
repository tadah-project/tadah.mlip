.. _descriptors:

Descriptors
===========

This page lists every two-body (``D2_*``) and many-body (``DM_*``) descriptor
compiled into Tadah!, together with their constructor arguments.  
Because the tables are generated directly from the header files, *the class
name you see here is exactly the keyword you must write in your configuration
file.*

General workflow
----------------

1.  Decide which descriptor families you need (two-body, many-body, or both).  
2.  Switch the family **on** with the corresponding *INIT* flag::

        INIT2B  true      # activate two-body block
        INITMB  false     # skip many-body block

    At least one of ``INIT2B`` / ``INITMB`` must be ``true``.  If both are
    ``true`` Tadah! will build one descriptor of each type **unless** you say
    otherwise with ``TYPE2B`` / ``TYPEMB``.  
3.  Provide **exactly one** line per descriptor you want to build, using the
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
* ``<EL1> <EL2>`` are element symbols (use ``*`` for “any”). Multiple pairs 
  are allowed, e.g. ``DM_EAD 1 4 4 Ti Ti Ti Nb``.
* ``L`` is the maximum angular momentum number needed by DM descriptors.  
* ``N_C``, ``N_S`` are the sizes of the *centre* and *width* grids
  (``CGRID*`` / ``SGRID*``).  
* ``N_CE``, ``N_SE`` appear only when non-linear function specification is required
  (``CEMBFUNC`` / ``SEMBFUNC``).

Remember to supply matching auxiliary keys:

=============  =========================================================
``RCTYPE2B``   cutoff function(s) – one per descriptor in ``TYPE2B``
``RCUT2B``     cutoff distance(s)
``CGRID2B``    centre grid(s) ‑ if the descriptor uses them
``SGRID2B``    width  grid(s) ‑ same length as the corresponding centres
(and the analogous ``*MB`` variants for many-body descriptors)
=============  =========================================================

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
insight (e.g. “add a short–range ZBL shield to a Blip term”) without
editing source code.  The idea is implemented via the meta-classes
``D2_mJoin`` (two-body) and ``DM_mJoin`` (many-body).

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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   # -- simple monatomic model -----------------------------------------
   INIT2B     true
   TYPE2B     D2_LJ  Kr Kr    # no extra parameters
   RCTYPE2B   Cut_Cos
   RCUT2B     6.0

2-body BP + many-body EAD
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   # ----- two-body Behler–Parrinello ----------------------------------
   INIT2B  true
   TYPE2B     D2_BP 10 10 Kr Kr    # 10 radial functions
   RCTYPE2B   Cut_Cos
   RCUT2B     6.5
   CGRID2B    LIN 10 0.0 6.5       # matching grid of centres
   SGRID2B    GEOM 10 0.05 0.70    # matching grid of widths

   # ----- many-body embedded density ----------------------------------
   INITMB  true
   TYPEMB     DM_EAD 1 7 7 Kr Kr  # L=1, 7 centres, 7 widths
   RCTYPEMB   Cut_Poly2
   RCUTMB     6.5
   CGRIDMB    LIN 7 0.0 6.5
   SGRIDMB    GEOM 7 0.05 0.70

Composite 2-body terms with ``D2_mJoin``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

   INIT2B  true

   TYPE2B     D2_mJoin                # meta descriptor

   TYPE2B     D2_MIE 12 6  Ti Ti      # --- component 1
   RCUT2B     5.0
   RCTYPE2B   Cut_Cos

   TYPE2B     D2_Blip 4 4  Ti Nb      # --- component 2
   RCUT2B     7.5
   RCTYPE2B   Cut_Poly2
   CGRID2B    LIN 4 0.0 7.5
   SGRID2B    GEOM 4 0.05 0.70

   TYPE2B     D2_Blip 3 3  * *      # --- component 3
   RCUT2B     4.0
   RCTYPE2B   Cut_Cos
   CGRID2B    LIN 3 0.0 7.5
   SGRID2B    GEOM 3 0.05 0.70

Reading the tables below
------------------------

Following this overview you will find the *Two-Body* and *Many-Body* sections.
Each entry shows

* the C++ signature,
* a short description & equation,
* **Required context keys**, i.e. the options you must specify in the
  configuration file.

Copy the class name into the ``TYPE2B`` / ``TYPEMB`` line, supply the required
keys, and you are ready to train.

--------------

Two-Body
--------

Below is a list of all two-body descriptors supported by Tadah:

D2_LJ
.....

.. doxygenclass:: tadah::models::D2_LJ

D2_BP
.....

.. doxygenclass:: tadah::models::D2_BP

D2_Blip
.......

.. doxygenclass:: tadah::models::D2_Blip

D2_EAM
......

.. doxygenclass:: tadah::models::D2_EAM

D2_MIE
......

.. doxygenclass:: tadah::models::D2_MIE

D2_ZBL
........

.. doxygenclass:: tadah::models::D2_ZBL

D2_Dummy
........

.. doxygenclass:: tadah::models::D2_Dummy

D2_mJoin
........

.. doxygenclass:: tadah::models::D2_mJoin


Many-Body
---------

DM_Blip
.......

.. doxygenclass:: tadah::models::DM_Blip

DM_EAD
......

.. doxygenclass:: tadah::models::DM_EAD

DM_EAM
......

.. doxygenclass:: tadah::models::DM_EAM

DM_mEAD
.......

.. doxygenclass:: tadah::models::DM_mEAD

DM_mEAD functions
~~~~~~~~~~~~~~~~~

.. doxygenclass:: tadah::models::F_RLR

.. doxygenclass:: tadah::models::F_SQ

.. doxygenclass:: tadah::models::F_SQRT

DM_Dummy
........

.. doxygenclass:: tadah::models::DM_Dummy

DM_mJoin
........

.. doxygenclass:: tadah::models::DM_mJoin
