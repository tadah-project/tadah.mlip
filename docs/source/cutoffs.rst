.. _cutoffs:

Cutoff Functions
================

Cutoff functions multiply every radial term to enforce a smooth boundary at
the cutoff radius.  The config label is the exact class name in the section
heading; it is given as the value of ``RCTYPE2B`` or ``RCTYPEMB``.

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
     - Normalised tanh switch with a flat tail
   * - ``Cut_Tanh``
     - tanh(1)^3
     - 0
     - yes
     - tanh\ :sup:`3` taper; Behler (2011)
   * - ``Cut_Dummy``
     - 1
     - 0
     - **no**
     - Sharp step; suitable when each compact radial basis terminates smoothly within the radius
   * - ``Cut_SinInv``
     - 0
     - 1
     - yes
     - Inverse envelope; used internally by ``DM_mBlipEnv``


Spatial cutoffs — f(r\ :sub:`cut`) = 0
---------------------------------------------------

``Cut_Tanh`` has f(0) = tanh(1)^3 (approximately 0.442), rather than 1.
Cosine cutoffs match with C1 smoothness; the quintic polynomial and tanh-cubed
cutoffs match with C2; Cut_PT has a flat limiting tail. ``Cut_SinInv`` is an
increasing density envelope, with C1 matching, rather than a spatial taper.
``DM_Embed`` calls a verified self-terminating density's spatial option ``NONE``.

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
