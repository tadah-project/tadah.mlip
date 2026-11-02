.. _cutoffs:

Cutoff Functions
================

This page lists the cutoff functions that the training keys :ref:`RCTYPE2B`
and :ref:`RCTYPEMB` accept for the descriptors (see :doc:`descriptors`).
Cutoff functions multiply every radial term to enforce a smooth boundary at
the cutoff radius. In the training configuration, the value of ``RCTYPE2B``
or ``RCTYPEMB`` is the name in the section heading below (for example
``Cut_Cos``). Each entry gives f(0), f(r\ :sub:`cut`) and whether the
function is smooth.

``Cut_Cos``
   f(0) = 1, f(r\ :sub:`cut`) = 0, smooth. Cosine taper;
   Behler–Parrinello (2007) standard.

``Cut_CosS``
   f(0) = 1, f(r\ :sub:`cut`) = 0, smooth. Cosine taper with a flat inner
   plateau.

``Cut_Poly``
   f(0) = 1, f(r\ :sub:`cut`) = 0, smooth. 5th-order polynomial; first and
   second derivatives zero at r\ :sub:`cut`.

``Cut_PolyS``
   f(0) = 1, f(r\ :sub:`cut`) = 0, smooth. Polynomial with a flat inner
   plateau.

``Cut_PT``
   f(0) = 1, f(r\ :sub:`cut`) = 0, smooth. Normalised tanh switch with a
   flat tail.

``Cut_Tanh``
   f(0) = tanh(1)^3, f(r\ :sub:`cut`) = 0, smooth. tanh\ :sup:`3` taper;
   Behler (2011).

``Cut_Dummy``
   f(0) = 1, f(r\ :sub:`cut`) = 0, **not smooth**. Sharp step; suitable when
   each compact radial basis terminates smoothly within the radius.

``Cut_SinInv``
   f(0) = 0, f(r\ :sub:`cut`) = 1, smooth. Inverse envelope; used
   internally by ``DM_mBlipEnv``.


Spatial cutoffs — f(r\ :sub:`cut`) = 0
---------------------------------------------------

``Cut_Tanh`` has f(0) = tanh(1)^3 (approximately 0.442), rather than 1.
At r\ :sub:`cut`, the cosine cutoffs reach zero with a continuous first
derivative (C1). ``Cut_Poly``, ``Cut_PolyS`` and ``Cut_Tanh`` also have a
continuous second derivative there (C2). ``Cut_PT`` has a flat limiting
tail. ``Cut_SinInv`` is an increasing density envelope with a continuous
first derivative at r\ :sub:`cut` (C1), rather than a spatial taper.
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
