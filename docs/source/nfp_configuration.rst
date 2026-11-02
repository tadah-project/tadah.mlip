.. _block-hierarchy:

.. _configuration-file:

.. _see-also:

.. _hpo-manual:

.. _manual:

.. _nfp_manual:

The Target File
===============

This page shows how to write the target file, the file given to
``tadah hpo`` with ``--hpotarget`` (``config.hpo`` by convention). The target
file describes the search. The model and the training data belong in the
training configuration (see :ref:`ConfigSection`); the validation datasets
belong in the validation configuration (``--validation``).
:doc:`nfp_quickstart` shows the three inputs together.

This page explains the file format. :doc:`nfp_block_keys` lists the top-level
keys; :doc:`nfp_optimizer` covers the ``OPTIMIZER`` block and the blocks
inside it. Their values, defaults and restrictions are in
:doc:`nfp_reference`.

.. _writing-the-file:

File format
-----------

* Each line holds one key, then its space-separated values.
  Indentation is for readability; the opening and closing lines of a block
  determine which lines it holds.
* A block opens with a line that holds only its name, and closes with a
  line that holds only ``END`` plus that name. ``tadah hpo`` refuses any
  other word on these lines.
* ``#`` starts a comment, including after a value. Blank lines are ignored.
* A trailing ``\`` joins the next line to the current one. Whitespace
  separates the tokens on the joined lines.
* Key names are case-sensitive; in
  particular, the relative-energy metric is ``PC_ErRMSE``. The parameter
  name following ``OPTIM`` is case-insensitive.
* Dedicated integer keys use decimal digits, for example ``MAXEVAL 1000``.
  Algorithm-specific ``PARAM`` values follow their own reference rules.
  Real settings accept finite decimal numbers and exponents, for example
  ``1e-6``. ``nan``, ``inf``, hexadecimal floats, trailing text and
  underflowing nonzero numbers are rejected.
* ``CONFIG`` includes and ``DESCRIPTOR`` blocks belong to the training
  configuration (see :ref:`ConfigSection`).

Schematic template
------------------

The outline below shows every block type and its purpose. Angle brackets
mark values to supply; the optional blocks apply to the chosen strategy and
algorithm. A complete runnable example appears in :doc:`nfp_quickstart`.

.. code-block:: none

   # Top-level keys: loss terms, search parameters and output settings.
   LOSS <loss function>
   FAILSCORE <failure ceiling>
   PC_SCRIPT_TIMEOUT <script time limit>
   OPTIM <KEY> (<indices>) <low> <high> [LIN | LOG | ASINH <c>]
   PC_ERMSE <target> <weight>              # optional validation metric
   PC_LAMMPS <script and target options>  # script-derived property target
   OUTPUT <history interval>

   OPTIMIZER                             # the search
     INIT                                # optional initial point(s)
       METHOD <method and arguments>
     ENDINIT

     NOISE                               # optional noise calibration
       AUTO <boolean>
       CALIBRATION <repeat count>
     ENDNOISE

     STRATEGY                            # optional strategy
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

         INNER                           # only for NLopt MLSL or AUGLAG
           LIB <local-search library>
           ALGO <local-search algorithm>
           MAXEVAL <local-search budget>
         ENDINNER
       ENDREFINE

       RESTART                           # optional restart on stagnation
         AFTER_STAGNATION <window>
         MAX <restart count>
         METHOD <new-point method and arguments>
       ENDRESTART
     ENDSTRATEGY
   ENDOPTIMIZER

The top-level metric and script lines of the outline show two kinds of loss
term. :hpo:key:`PC_OBSERVABLE` provides an alternative to
:hpo:key:`PC_LAMMPS` for property targets. Top-level keys, including the
output settings, appear outside :hpo:block:`OPTIMIZER`.

Block placement and order
-------------------------

The file has top-level keys and one :hpo:block:`OPTIMIZER` block, which
describes the search. :hpo:block:`!OPTIMIZER` never stands inside another
block. :hpo:block:`INIT`, :hpo:block:`NOISE` and :hpo:block:`STRATEGY`
blocks may appear inside it. :hpo:block:`EXPLORE` and
:hpo:block:`REFINE` blocks go inside :hpo:block:`!STRATEGY`.
:doc:`nfp_optimizer` lists every block and the keys it accepts.

For a plain search, :hpo:key:`LIB`, :hpo:key:`ALGO` and the other optimiser
settings can appear directly inside :hpo:block:`!OPTIMIZER`. A
:hpo:block:`!STRATEGY` block inside it groups the search strategy and its
settings.

Each block closes with its ``END`` line before the block around it closes,
and before the end of the file. ``tadah hpo`` refuses a file that breaks
these rules and names the line, for example an ``ENDOPTIMIZER`` line
inside an open :hpo:block:`!STRATEGY` block.

A repeated :hpo:block:`!INIT`, :hpo:block:`!NOISE`, :hpo:block:`!EXPLORE`,
:hpo:block:`!REFINE` or :hpo:block:`RESTART` block continues the first one.
The :hpo:key:`METHOD` lines of a repeated :hpo:block:`!INIT` or
:hpo:block:`!RESTART` block add to the same list of starting-point methods
(see :doc:`nfp_initialisation`).

Each key that takes one value is set once for what it configures.
:hpo:block:`!OPTIMIZER`, :hpo:block:`!STRATEGY` and :hpo:block:`!REFINE`
configure the same optimiser, and the :hpo:block:`!INIT` blocks inside
:hpo:block:`!OPTIMIZER`, :hpo:block:`!STRATEGY` and :hpo:block:`!REFINE`
the same starting points. A top-level key such as :hpo:key:`QFLUSH` or
:hpo:key:`DUMP` configures the run. A key set twice, in one block, in a
repeated block or in two such blocks, is refused, and the message names the
line of the first.

Lines that add are no repeats: :hpo:key:`!METHOD` lines, :hpo:key:`OPTIM`,
:hpo:key:`!PC_LAMMPS` and :hpo:key:`!PC_OBSERVABLE` lines, and
:hpo:key:`PC_ERMSE` and :hpo:key:`PC_FRMSE` lines of other datasets. A
:hpo:key:`STEP` line for other indices refines the block-wide
:hpo:key:`!STEP`.

.. versionchanged:: 1.6.0
   A key set twice for one optimiser, its starting points, the ``NOISE``,
   ``RESTART`` or ``STRATEGY`` settings or the run is refused; 1.5.0 used
   the later value. A second ``OPTIMIZER`` block, an ``OPTIMIZER`` line
   inside a block, a misplaced ``ENDOPTIMIZER`` line, a block left open at
   the end of the file and a word after the name on a line that opens or
   closes a block are also refused.

:hpo:key:`TYPE` :hpo:key:`PRESTAGE` precedes the opening of
:hpo:block:`!EXPLORE` or :hpo:block:`!REFINE`. :hpo:block:`!REFINE` groups
the refinement settings of the surrounding :hpo:block:`!STRATEGY`.

:hpo:key:`LOSS`, :hpo:key:`FAILSCORE` and :hpo:key:`PC_SCRIPT_TIMEOUT`
precede the lines that read them (see :ref:`where-the-setting-applies`
and :doc:`nfp_lammps_targets`).

Repeated ``OPTIM`` lines select distinct dimensions.

.. raw:: html

   <script src="_static/nfp_redirect.js"></script>
