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
* A block opens with its name and closes with ``END`` plus that name,
  on a separate line. These delimiter lines define the block boundaries.
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

``OPTIMIZER`` is the only root block in ``config.hpo``. Its immediate
subblocks are ``INIT``, ``NOISE`` and ``STRATEGY``. ``EXPLORE`` and
``REFINE`` are nested within ``STRATEGY``. The :doc:`nfp_optimizer`
section gives the full subblock hierarchy and accepted keys.

One ``OPTIMIZER`` block describes the search. For a plain search,
``LIB``, ``ALGO`` and the other optimiser settings can appear directly
inside it. An explicit ``STRATEGY`` subblock groups the run policy and
its settings.

``TYPE PRESTAGE`` precedes the opening of ``EXPLORE`` or ``REFINE``.
``REFINE`` groups the refinement settings of the surrounding ``STRATEGY``.

``LOSS``, ``FAILSCORE`` and ``PC_SCRIPT_TIMEOUT`` precede the
``PC_LAMMPS`` and ``PC_FAILPROBE`` lines they affect. Each target line
captures those settings when it is read. Repetition rules depend on the
key. Repeated ``OPTIM`` lines must select distinct dimensions.

.. raw:: html

   <script src="_static/nfp_redirect.js"></script>
