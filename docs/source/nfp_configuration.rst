.. _block-hierarchy:

.. _configuration-file:

.. _see-also:

.. _hpo-manual:

.. _manual:

.. _nfp_manual:

Configuration syntax and block layout
=====================================

The file passed to ``tadah hpo --hpotarget`` describes the outer search.
It is usually called ``config.hpo``. The model and training data belong in
the separate ``--config`` file; validation datasets belong in
``--validation``. See :doc:`nfp_quickstart` for the three inputs together.

This page explains the file format. :doc:`nfp_block_keys` lists the root
keys; :doc:`nfp_optimizer` covers the search block and its subblocks.
Their values, defaults and restrictions are in :doc:`nfp_reference`.

.. _writing-the-file:

File format
-----------

* Each directive occupies one line: a keyword, then its space-separated values.
  Indentation is for readability; block delimiters determine the scope.
* A block opens with a line that holds only its name, and closes with a
  line that holds only ``END`` plus that name. ``tadah hpo`` refuses any
  other word on these lines.
* ``#`` starts a comment, including after a value. Blank lines are ignored.
* A trailing ``\`` continues a directive on the next line. Whitespace
  separates the tokens on the joined lines.
* Directive names are case-sensitive; in
  particular, the relative-energy metric is ``PC_ErRMSE``. The parameter
  name following ``OPTIM`` is case-insensitive.
* Dedicated integer keys use decimal digits, for example ``MAXEVAL 1000``.
  Algorithm-specific ``PARAM`` values follow their own reference rules.
  Real settings accept finite decimal numbers and exponents, for example
  ``1e-6``. ``nan``, ``inf``, hexadecimal floats, trailing text and
  underflowing nonzero numbers are rejected.
* Training-file ``CONFIG`` includes and ``DESCRIPTOR`` blocks belong to the
  separate model configuration.

Schematic template
------------------

The **layout template** shows every block type and its purpose. Angle
brackets mark values to supply; optional subblocks apply to the selected
search policy and algorithm. A complete runnable example appears in
:doc:`nfp_quickstart`.

.. code-block:: none

   # Root directives: loss terms, variable parameters and output controls.
   LOSS <loss function>
   FAILSCORE <failure ceiling>
   PC_SCRIPT_TIMEOUT <script time limit>
   OPTIM <model parameter> (<indices>) <bounds>
   PC_ERMSE <target> <weight>              # optional validation metric
   PC_LAMMPS <script and target options>  # script-derived property target
   OUTPUT <history interval>

   OPTIMIZER                             # root block: outer search
     INIT                                # optional initial point(s)
       METHOD <method and arguments>
     ENDINIT

     NOISE                               # optional noise calibration
       AUTO <boolean>
       CALIBRATION <repeat count>
     ENDNOISE

     STRATEGY                            # optional run-policy subblock
       TYPE PRESTAGE                     # required for EXPLORE and REFINE
       EXPLORE                           # only for a staged search
         LIB <exploration library>
         ALGO <exploration algorithm>
         MAXEVAL <exploration budget>
       ENDEXPLORE

       REFINE                            # optional staged refinement settings
         LIB <refinement library>
         ALGO <refinement algorithm>
         MAXEVAL <refinement budget>

         INNER                           # only for an NLopt composite driver
           LIB <local-search library>
           ALGO <local-search algorithm>
           MAXEVAL <local-search budget>
         ENDINNER
       ENDREFINE

       RESTART                           # optional restart policy
         AFTER_STAGNATION <window>
         MAX <restart count>
         METHOD <new-point method and arguments>
       ENDRESTART
     ENDSTRATEGY
   ENDOPTIMIZER

The root metric and script lines illustrate different loss sources.
``PC_OBSERVABLE`` provides an alternative to ``PC_LAMMPS`` for property
targets. Root settings and output controls appear outside ``OPTIMIZER``.

Block placement and order
-------------------------

The file has top-level keys and one ``OPTIMIZER`` block, which describes
the search. ``OPTIMIZER`` never stands inside another block. Inside it you
can open ``INIT``, ``NOISE`` and ``STRATEGY`` blocks. ``EXPLORE`` and
``REFINE`` blocks go inside ``STRATEGY``. :doc:`nfp_optimizer` lists
every block and the keys it accepts.

For a plain search, ``LIB``, ``ALGO`` and the other optimiser settings
can appear directly inside ``OPTIMIZER``. A ``STRATEGY`` block inside it
groups the search strategy and its settings.

Close each block with its ``END`` line before the block around it closes,
and before the end of the file. ``tadah hpo`` refuses a file that breaks
these rules and names the line, for example an ``ENDOPTIMIZER`` line
inside an open ``STRATEGY`` block.

A repeated ``INIT``, ``NOISE``, ``EXPLORE``, ``REFINE`` or ``RESTART``
block continues the first one. The ``METHOD`` lines of a repeated
``INIT`` or ``RESTART`` block add to the same list of starting-point
methods (see :doc:`nfp_initialisation`).

Each key that takes one value is set once for what it configures.
``OPTIMIZER``, ``STRATEGY`` and ``REFINE`` configure the same optimiser,
and the ``INIT`` blocks inside ``OPTIMIZER``, ``STRATEGY`` and ``REFINE``
the same starting points. A top-level key such as ``QFLUSH`` or ``DUMP``
configures the run. A key set twice, in one block, in a repeated block or
in two such blocks, is refused, and the message names the line of the
first.

Lines that add are no repeats: ``METHOD`` lines, ``OPTIM``,
``PC_LAMMPS`` and ``PC_OBSERVABLE`` lines, and ``PC_ERMSE`` and
``PC_FRMSE`` lines of other datasets. A ``STEP <KEY>`` override of other
indices refines the block-wide ``STEP``.

.. versionchanged:: 1.6.0
   A key set twice for one optimiser, its starting points, the ``NOISE``,
   ``RESTART`` or ``STRATEGY`` settings or the run is refused. 1.5.0 used
   the later value.

.. versionchanged:: 1.6.0
   ``tadah hpo`` refuses a second ``OPTIMIZER`` block, an ``OPTIMIZER``
   line inside a block and a block left open at the end of the file. It
   also refuses an ``ENDOPTIMIZER`` line inside another block or with no
   block open, and a word after the name on a line that opens or closes a
   block.

``TYPE PRESTAGE`` precedes the opening of ``EXPLORE`` or ``REFINE``.
``REFINE`` groups the refinement settings of the surrounding ``STRATEGY``.

``LOSS``, ``FAILSCORE`` and ``PC_SCRIPT_TIMEOUT`` precede the
``PC_LAMMPS`` and ``PC_FAILPROBE`` lines they affect. Each target line
captures those settings when it is read, so the three may be given again
for the lines after them. Repeated ``OPTIM`` lines select distinct
dimensions.

.. raw:: html

   <script src="_static/nfp_redirect.js"></script>
