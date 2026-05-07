.. _cutoffs:

Cutoff Functions
================

Cutoff functions multiply every radial term to enforce a smooth boundary at
the cutoff radius.  The config label is the exact class name in the section
heading; pass it as the value of ``RCTYPE2B`` or ``RCTYPEMB``.

.. list-table::
   :header-rows: 1
   :widths: 14 7 8 9 62

   * - Label
     - f(0)
     - f(r\ :sub:`cut`)
     - Smooth
     - Notes
   * - ``Cut_Cos``
     - 1
     - 0
     - yes
     - Cosine taper; Behler–Parrinello (2007) standard
   * - ``Cut_CosS``
     - 1
     - 0
     - yes
     - Cosine taper with a flat inner plateau
   * - ``Cut_Poly``
     - 1
     - 0
     - yes
     - 5th-order polynomial; first and second derivatives zero at r\ :sub:`cut`
   * - ``Cut_PolyS``
     - 1
     - 0
     - yes
     - Polynomial with a flat inner plateau
   * - ``Cut_PT``
     - 1
     - 0
     - yes
     - Pseudo-Thomas–Fermi taper; very smooth tail
   * - ``Cut_Tanh``
     - 1
     - 0
     - yes
     - tanh\ :sup:`3` taper; Behler (2011)
   * - ``Cut_Dummy``
     - 1
     - 0
     - **no**
     - Sharp step; for testing only, not for production runs
   * - ``Cut_SinInv``
     - 0
     - 1
     - yes
     - Inverse envelope; used internally by ``DM_mBlipEnv``


Standard cutoffs — f(0) = 1, f(r\ :sub:`cut`) = 0
---------------------------------------------------

Cut_Cos
.......

.. doxygenclass:: tadah::models::Cut_Cos

Cut_CosS
........

.. doxygenclass:: tadah::models::Cut_CosS

Cut_Poly
........

.. doxygenclass:: tadah::models::Cut_Poly

Cut_PolyS
.........

.. doxygenclass:: tadah::models::Cut_PolyS

Cut_PT
......

.. doxygenclass:: tadah::models::Cut_PT

Cut_Tanh
........

.. doxygenclass:: tadah::models::Cut_Tanh

Cut_Dummy (discontinuous)
.........................

.. doxygenclass:: tadah::models::Cut_Dummy

Inverse envelope — f(0) = 0, f(r\ :sub:`cut`) = 1
---------------------------------------------------

Cut_SinInv
..........

.. doxygenclass:: tadah::models::Cut_SinInv
