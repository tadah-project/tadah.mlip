.. _pc_lammps_options:

.. _additional-options:

LAMMPS Line Options
===================

This page lists the options of a :hpo:key:`PC_LAMMPS` line, which runs a
LAMMPS script in each evaluation and turns variables of the script into
loss terms. It assumes a target file (see :doc:`nfp_configuration`) and a
LAMMPS script that defines the variables to read.

The options follow :hpo:key:`!PC_LAMMPS` on one logical line. They are separate
from the top-level :doc:`execution keys <nfp_lammps_targets>`.

.. _required-options:

.. _result-contract-for-user-scripts:

.. _pc_lammps_result_contract:

Required options
----------------

``--script <file>``
   LAMMPS input script. Requested outputs must be defined as equal-style
   variables; ``tadah hpo`` checks the definitions when it reads the
   target file. A script with a ``quit`` command, or an ``if`` command
   that runs one, is refused: ``quit`` ends the process that runs the
   script.

``--varloss <name> <target> <weight> [<loss> [<parameter>]]``
   The variable is read after the script and gives one loss term with the
   loss-term weight ``<weight>``. A line may repeat the option for more
   terms. Target and weight must be finite; unlike the loss-term weights
   of the validation metrics and of :hpo:key:`PC_OBSERVABLE` lines, a
   negative loss-term weight is accepted here. The default loss is the
   top-level :hpo:key:`LOSS` as
   it stood when this line was read. An optional
   :doc:`loss name <nfp_losses>` and parameter override it for this
   variable only, which then does not read :hpo:key:`!LOSS`. A target
   that the loss cannot measure is refused.

.. _pc_lammps_readback:

Inputs and diagnostics
----------------------

``--invar <name> <value>``
   A LAMMPS command-line variable, equivalent to ``-var``. The value is
   one token.

``--outvar <name>``
   An equal-style variable that is recorded without adding a loss term.
   ``--varloss`` variables are recorded automatically. An unreadable
   diagnostic becomes ``NaN``; an unreadable loss variable fails the
   script. The first output of a :hpo:key:`PC_FAILPROBE` is its required
   scalar, so unreadable probe output fails the probe.

Timeout and failed-script values
--------------------------------

``--maxtime <time>``
   The script's own limit in place of :hpo:key:`PC_SCRIPT_TIMEOUT`:
   ``off``, ``unlimited`` or a positive ``[[HH:]MM:]SS`` time; the line
   then does not read :hpo:key:`!PC_SCRIPT_TIMEOUT`.

``--failscore <value>``
   Value substituted for this script's output and loss variables if it
   fails. Any finite number is accepted for :hpo:key:`PC_LAMMPS`;
   default: the top-level :hpo:key:`FAILSCORE` when the line is read. The
   substituted variable passes through its configured loss for
   reporting, but the whole evaluation still receives a failure score.
   The line then does not read the top-level
   :hpo:key:`!FAILSCORE`. On :hpo:key:`!PC_FAILPROBE` this option instead
   requires a positive number.

Hot variables
-------------

``--hotvar <name> <min> <max> [<initial>]``
   A variable that the script returns for reuse by subsequent
   evaluations and by other scripts that reference it. Bounds and
   initial value must be finite, with ``max > min``. The omitted initial
   value is the midpoint. The returned value must lie in the closed
   interval; an out-of-range value fails the evaluation.

The script must define the returned value as an equal-style variable.
Before the script runs, ``tadah hpo`` sets the variable to its cached
value: the last returned value inside the interval, else the initial
value. Scripts without ``--hotvar`` that reference a returned hot value
(``${name}``, ``v_name``, or ``$name`` for a one-character name) run
after its producer and receive its value. A script with ``--hotvar``
receives only its own cached values. Each hot-variable name must have
only one producing script.

A line is refused when it defines one of its own ``--hotvar`` names with
``--invar``, ``-var`` or ``-v``. A line without ``--hotvar`` is refused
when it defines in that way a ``--hotvar`` name that its script
references. Those options create index-style variables, which conflict
with equal-style injection. A line with ``--hotvar`` may define another
line's hot name, and its script then reads that definition.

Violation variables
-------------------

``--failvar <name> [<scale>]``
   An equal-style, nonnegative violation measure: zero means
   no detected failure; a positive value indicates a failed simulation.
   The optional scale must be finite and positive; default ``1``.

A positive violation marks the simulation as failed, and its evaluation
fails in both grading modes (see :ref:`nfp_failscore_grading`). With
:hpo:key:`FAILSCORE_GRADING` ``ON``, ``scale * value`` grades its
severity; with ``OFF``, an explicit scale is refused. In either mode an
unreadable or non-finite fail variable fails the simulation. The script
defines it before any command that might stop the script early.

LAMMPS command-line options
---------------------------

Single-dash options and their values are forwarded to LAMMPS, for
example ``-screen none``, ``-log run.log`` or ``-var pressure 0``; the
short forms ``-sc`` and ``-l`` count as ``-screen`` and ``-log``.
LAMMPS checks whether its build supports them. ``tadah hpo`` reserves
``maxtime`` for its timer: ``--invar maxtime``, ``-var maxtime`` and
``-v maxtime`` are refused.

Double-dash options are read by ``tadah hpo``. Unknown options and extra
values after a double-dash option are errors. ``--script``,
``--failscore`` and ``--maxtime`` are given once per line; a variable is
declared once by ``--hotvar`` or ``--failvar``, and defined once by
``--invar``, ``-var`` or ``-v``. Parallel script execution uses
:hpo:key:`THREADS <OPTIMIZER.THREADS>` and :hpo:key:`PC_ISOLATION`; the
option ``--ncpu`` is refused. ``--gate`` and ``--s0`` belong only to
:hpo:key:`PC_FAILPROBE`.
