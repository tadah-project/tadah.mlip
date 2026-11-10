.. _nfp_target_file:

The Target File
===============

This guide describes how ``tadah hpo`` reads a target file: its parts, its
lines and values, its blocks and the order of its lines. It builds on the
:ref:`nfp_quickstart`, whose ``config.hpo`` is a complete target file. The
:ref:`nfp_reference` gives every key, its values and its default.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_target_file_parts:

The parts of a target file
--------------------------

The :term:`target file` (``--hpotarget``) states what ``tadah hpo`` searches,
how it scores each :term:`evaluation` and which optimiser runs the search. The
model and the training data belong to the :term:`training configuration`
(:ref:`config_file`), and the validation data to the
:term:`validation configuration` (:ref:`validation`).

A target file has :term:`top-level keys <top-level key>` and one
:hpo:block:`OPTIMIZER` block. The top-level keys are of three kinds:

- :hpo:key:`OPTIM <ROOT.OPTIM>` lines make up the search space
  (:ref:`nfp_search_space`);
- the loss terms, such as :hpo:key:`PC_ERMSE <ROOT.PC_ERMSE>` and
  :hpo:key:`PC_LAMMPS <ROOT.PC_LAMMPS>` lines, score each evaluation
  (:ref:`nfp_targets`);
- other top-level keys set up the run, such as the files it writes
  (:ref:`nfp_output`).

The :hpo:block:`!OPTIMIZER` block holds the search: the optimiser, its budget
and its strategy (:ref:`nfp_optimizer_block`). The comments of the short target
file below name its parts:

.. code-block:: none

   # Top-level keys: the search space and the loss.
   OPTIM      RCUT2B (1) 4.0 6.5   # a search parameter
   PC_ERMSE   0 1                  # a loss term

   OPTIMIZER                       # the search
       LIB      NLOPT
       ALGO     LN_SBPLX
       MAXEVAL  20
   ENDOPTIMIZER

Top-level keys stand before or after the :hpo:block:`!OPTIMIZER` block, never
inside it. ``tadah hpo`` refuses a file without the block, a block without the
:hpo:key:`LIB <OPTIMIZER.LIB>` and :hpo:key:`ALGO <OPTIMIZER.ALGO>` lines that
name its optimiser, and a file without a loss term. The last refusal lists the
lines that give a loss term.

``tadah refit`` reads an ``OPTIMIZER`` block of its own, inside a stage block
of the refit configuration and not in a target file
(:ref:`refit_stage_blocks`). :ref:`refit` describes the target file that
``tadah refit`` writes for ``tadah hpo``.

.. _nfp_target_file_lines:

Lines and values
----------------

``tadah hpo`` reads the file line by line. The first word of a line is its key,
and the words after it are its values, separated by blanks or tabs. Indentation
has no meaning, except at the start of a continued line; the files of this
chapter indent the lines of a block for readability.

``#`` starts a comment, which runs to the end of the line, also after a value,
so a value cannot contain ``#``. Blank lines and comment lines are skipped.

A line that ends with ``\`` continues on the next line. The two lines are
joined as written, indentation included, with no blank added. Two words meet
without a separator unless a blank stands before the ``\`` or at the start of
the next line. A comment may follow the ``\``.

The next line is joined even when it is blank or a comment line, which then
ends the continued line. A last line that ends with ``\`` is refused. Long
:hpo:key:`PC_LAMMPS <ROOT.PC_LAMMPS>` lines are continued this way:

.. literalinclude:: files/tutorial/unit4/config.hpo
   :language: none
   :start-at: PC_LAMMPS  --script ../../lammps/vacancy.in
   :end-at: --varloss Evf

A relative path in a target file, such as this ``--script``, is read from the
working directory of the run, not from the directory of the target file.

Keys are matched as written: in upper case, as the reference writes them, and
:hpo:key:`PC_ErRMSE <ROOT.PC_ErRMSE>` with its lower-case r. The case of a
value is a rule of its key, which its entry in :ref:`nfp_reference` gives.

A number is one token in decimal notation, with an optional sign, decimal point
and exponent, such as ``1e-6``. Hexadecimal numbers, ``inf`` and ``nan`` are
refused, and so is a number other than 0 beyond the range of a double or below
its smallest normal value. A key that takes a whole number, such as
:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` or
:hpo:param:`GRID_POINTS <PARAM.TADAH:GRID.GRID_POINTS>`, takes decimal digits
only: ``1e1``, ``+10`` and ``10.0`` are refused.

A key that sets one value is given once for what it configures: a second line
is refused, not read as a replacement. :hpo:key:`LOSS <ROOT.LOSS>`,
:hpo:key:`FAILSCORE <ROOT.FAILSCORE>` and
:hpo:key:`PC_SCRIPT_TIMEOUT <ROOT.PC_SCRIPT_TIMEOUT>` follow the rule of
:ref:`nfp_target_file_order` instead. Lines that add something, such as
:hpo:key:`OPTIM <ROOT.OPTIM>` and :hpo:key:`!PC_LAMMPS <ROOT.PC_LAMMPS>`
lines, may repeat. An optimiser key is given in only one of the blocks that
configure the optimiser (:hpo:topic:`RULE.set_once_for_optimiser`).

.. _nfp_target_file_blocks:

Blocks and nesting
------------------

A :term:`block` of a target file opens with a line that holds its name alone,
such as :hpo:block:`OPTIMIZER`, and closes with ``END`` and its name,
``ENDOPTIMIZER``. Each key inside the block stands on a line of its own. A
target file has eight types of block, each opening directly inside the blocks
listed here:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Block
     - Opens directly inside
   * - :hpo:block:`!OPTIMIZER`
     - The top level, once
   * - :hpo:block:`INIT`
     - :hpo:block:`!OPTIMIZER`, :hpo:block:`STRATEGY`,
       :hpo:block:`EXPLORE` or :hpo:block:`REFINE`
   * - :hpo:block:`NOISE`
     - :hpo:block:`!OPTIMIZER`
   * - :hpo:block:`!STRATEGY`
     - :hpo:block:`!OPTIMIZER`, once
   * - :hpo:block:`!EXPLORE`
     - :hpo:block:`!STRATEGY`, after :hpo:key:`TYPE <STRATEGY.TYPE>`
       :hpo:key:`PRESTAGE <STRATEGY.TYPE.PRESTAGE>`
   * - :hpo:block:`!REFINE`
     - :hpo:block:`!STRATEGY`, after :hpo:key:`!TYPE <STRATEGY.TYPE>`
       :hpo:key:`!PRESTAGE <STRATEGY.TYPE.PRESTAGE>`
   * - :hpo:block:`INNER`
     - :hpo:block:`!STRATEGY`, :hpo:block:`!EXPLORE` or
       :hpo:block:`!REFINE`
   * - :hpo:block:`RESTART`
     - :hpo:block:`!STRATEGY`

A second :hpo:block:`!INIT`, :hpo:block:`!NOISE`, :hpo:block:`!EXPLORE`,
:hpo:block:`!REFINE` or :hpo:block:`!RESTART` block inside the same block
continues the first. The :hpo:block:`!INIT` blocks inside
:hpo:block:`!OPTIMIZER`, :hpo:block:`!STRATEGY` and :hpo:block:`!REFINE` make
one chain, as these blocks configure one optimiser; an :hpo:block:`!EXPLORE`
block takes it unless its :hpo:block:`!INIT` block has a
:hpo:key:`METHOD <INIT.METHOD>` line
(:ref:`nfp_optimizer_block_initialisation`). An :hpo:block:`!INNER` block
opens once for each optimiser.

An ``END`` line closes the innermost open block, and every block closes before
the file ends. ``tadah hpo`` refuses an ``END`` line that names another block,
a block left open and a key that the open block does not take. A top-level key
inside a block is refused with the line that opened the block, as is the
:hpo:key:`FAILSCORE <ROOT.FAILSCORE>` line of this file:

.. hpo:refusal::
   :wrap: optimizer
   :file:

   FAILSCORE 1e9

:ref:`nfp_optimizer_block` describes what each block holds and when a search
needs it.

.. _nfp_target_file_order:

Order of lines
--------------

:hpo:key:`LOSS <ROOT.LOSS>`, :hpo:key:`FAILSCORE <ROOT.FAILSCORE>` and
:hpo:key:`PC_SCRIPT_TIMEOUT <ROOT.PC_SCRIPT_TIMEOUT>` apply to the lines after
them. A :hpo:key:`PC_LAMMPS <ROOT.PC_LAMMPS>` or
:hpo:key:`PC_FAILPROBE <ROOT.PC_FAILPROBE>` line takes the
:hpo:key:`!FAILSCORE <ROOT.FAILSCORE>` and the
:hpo:key:`!PC_SCRIPT_TIMEOUT <ROOT.PC_SCRIPT_TIMEOUT>` above it, unless it
gives its own with :hpo:key:`--failscore <PC_LAMMPS.--failscore>` or
:hpo:key:`--maxtime <PC_LAMMPS.--maxtime>`. A
:hpo:key:`--varloss <PC_LAMMPS.--varloss>` term or a
:hpo:key:`PC_OBSERVABLE <ROOT.PC_OBSERVABLE>` line takes the
:hpo:key:`!LOSS <ROOT.LOSS>` above it, unless it names a loss function. In the
fragment below, the :hpo:key:`!PC_LAMMPS <ROOT.PC_LAMMPS>` line takes the
:hpo:key:`!PC_SCRIPT_TIMEOUT <ROOT.PC_SCRIPT_TIMEOUT>` line above it:

.. literalinclude:: files/tutorial/unit4/config.hpo
   :language: none
   :lines: 21-23,30-31

Every other use of these keys takes their last line: the metric lines, such
as :hpo:key:`PC_ERMSE <ROOT.PC_ERMSE>`, take the last
:hpo:key:`!LOSS <ROOT.LOSS>` line of the file, wherever they stand. A second
line of one of the three keys is accepted only after a line that takes the
first. A line that no later line takes is refused when it would have no effect
(:hpo:topic:`RULE.settings_taken_by_lines`). :ref:`nfp_targets` gives the
choice of these values.

Inside a :hpo:block:`STRATEGY` block, :hpo:key:`TYPE <STRATEGY.TYPE>`
:hpo:key:`PRESTAGE <STRATEGY.TYPE.PRESTAGE>` comes before an
:hpo:block:`EXPLORE` or :hpo:block:`REFINE` block. The
:hpo:key:`METHOD <INIT.METHOD>` lines of a block add starting points in the
order of the file (:ref:`nfp_optimizer_block_initialisation`).

.. _nfp_target_file_check:

Checking a target file
----------------------

``tadah hpo --check``, with the other arguments of the run, makes the startup
checks of a run without evaluating a candidate (:ref:`cli_hpo`). A changed
target file is best checked this way before a long run.
For the files of :ref:`nfp_unit_1`, the check prints, shortened:

.. output: unit1 stdout command=1

.. code-block:: text

   [tadah hpo] the INIT chain of the STRATEGY-level optimiser (METHOD CONFIG, by
       default) gives 1 starting point
   tadah hpo --check: config.hpo passed the startup checks.
   ...
   No candidate was evaluated and no file was written.

The first line, which a run prints too, counts the starting points of the
:hpo:block:`INIT` block, or of the training configuration (:hpo:key:`CONFIG`)
without one (:ref:`nfp_optimizer_block_initialisation`). The lines left out,
and the warnings and hints a check lists, are described with ``--check`` in
:ref:`tadah hpo <cli_hpo>`.

A refusal about a line names the line and quotes it. For a continued line, it
names the last line and quotes the joined text, so two words that the ``\``
joined show as one. A :hpo:key:`PARAM <OPTIMIZER.PARAM>` value written as a
number that is not finite, such as ``inf``, is refused at its own line. The
optimiser that :hpo:key:`LIB <OPTIMIZER.LIB>` and
:hpo:key:`ALGO <OPTIMIZER.ALGO>` name is checked when the block of the optimiser
closes, and any other :hpo:key:`!PARAM` key or value at startup, after the whole
file. Both refusals name the ``ENDOPTIMIZER`` line, or the ``ENDEXPLORE`` or
``ENDINNER`` line for an :hpo:block:`EXPLORE` or :hpo:block:`INNER` block.
