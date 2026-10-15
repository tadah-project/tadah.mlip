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

This page explains the file format. :doc:`nfp_block_keys` lists the keys
accepted in each scope. Their values, defaults and restrictions are in
:doc:`nfp_reference`.

Writing the file
----------------

* Write one directive per line: a keyword, then its space-separated values.
  Indentation is for readability; block delimiters determine the scope.
* Open a block with its name and close it with ``END`` plus that name,
  on a separate line. Do not write braces around blocks.
* ``#`` starts a comment, including after a value. Blank lines are ignored.
* Continue a directive with a trailing ``\``. Keep whitespace between the
  tokens on the joined lines.
* Spell directive names exactly as shown. They are case-sensitive; in
  particular, the relative-energy metric is ``PC_ErRMSE``. The parameter
  name following ``OPTIM`` is case-insensitive.
* Dedicated integer keys use decimal digits, for example ``MAXEVAL 1000``.
  Algorithm-specific ``PARAM`` values follow their own reference rules.
  Real settings accept finite decimal numbers and exponents, for example
  ``1e-6``. ``nan``, ``inf``, hexadecimal floats, trailing text and
  underflowing nonzero numbers are rejected.
* This file has no ``CONFIG`` include directive. Training-file includes and
  ``DESCRIPTOR`` blocks belong to the separate model configuration.

Schematic template
------------------

This is a **layout template**, not a runnable configuration. Replace angle
brackets with settings and remove unused blocks. It shows every block type;
several are conditional. A small runnable configuration follows in
:doc:`nfp_quickstart`.

.. code-block:: none

   # Root directives: define what to minimise and which parameters may vary.
   LOSS <loss function>
   FAILSCORE <failure ceiling>
   PC_SCRIPT_TIMEOUT <script time limit>
   OPTIM <model parameter> (<indices>) <bounds>
   PC_ERMSE <target> <weight>              # validation metric, if wanted
   PC_LAMMPS <script and target options>  # alternative: PC_OBSERVABLE
   OUTPUT <history interval>

   OPTIMIZER
     INIT                                # optional initial point(s)
       METHOD <method and arguments>
     ENDINIT

     NOISE                               # optional noise calibration
       AUTO <boolean>
       CALIBRATION <repeat count>
     ENDNOISE

     STRATEGY                            # optional explicit run policy
       TYPE <strategy type>
       LIB <library>
       ALGO <algorithm>
       MAXEVAL <evaluation budget>

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

The root metric and script lines above illustrate different loss sources.
``PC_OBSERVABLE`` and ``PC_LAMMPS`` cannot be combined in one run. Root
settings and output controls have no enclosing block.

Block placement and order
-------------------------

Use one ``OPTIMIZER`` block. An explicit ``STRATEGY`` block makes the run
policy visible. For a plain search it can be omitted: put ``LIB``, ``ALGO``
and the other optimiser settings directly inside ``OPTIMIZER``.

``INIT`` can also appear inside ``STRATEGY``, ``EXPLORE`` or ``REFINE``.
``INNER`` can appear inside ``STRATEGY`` or ``EXPLORE`` as well as the
``REFINE`` location shown above. ``RESTART`` is a child of ``STRATEGY``;
``NOISE`` is a child of ``OPTIMIZER``. See :doc:`nfp_block_keys` for the
complete scope table.

Declare ``TYPE PRESTAGE`` before opening ``EXPLORE`` or ``REFINE``.
``REFINE`` writes the same refinement settings as the surrounding
``STRATEGY``; it is a grouping device, not a second refinement phase.

Put ``LOSS``, ``FAILSCORE`` and ``PC_SCRIPT_TIMEOUT`` before the
``PC_LAMMPS`` and ``PC_FAILPROBE`` lines they should affect. Each such
line captures those settings when it is read. Avoid repeating a setting
as a way to override it: repetition rules differ by key. Repeated
``OPTIM`` lines must select distinct dimensions.

.. toctree::
   :maxdepth: 1

   nfp_block_keys

.. raw:: html

   <script src="_static/nfp_redirect.js"></script>
