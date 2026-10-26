.. _pc_lammps_options:

.. _pc_lammps_result_contract:

.. _pc_lammps_readback:

.. _additional-options:

.. _required-options:

.. _result-contract-for-user-scripts:

LAMMPS line options
===================

These options follow ``PC_LAMMPS`` on one logical line. They are separate
from the root :doc:`execution keys <nfp_lammps_targets>`.

Required options
----------------

``--script <file>``
   LAMMPS input script. Requested outputs must be defined as equal-style
   variables; HPO checks the definitions when it reads the target file.
   A script with a ``quit`` command, or an ``if`` command that runs one,
   is refused: ``quit`` ends the process that runs the script.

``--varloss <name> <target> <weight> [<loss> [<parameter>]]``
   Read this variable after the script and create a weighted loss term.
   Repeat for additional terms. Target and weight must be finite;
   unlike validation and typed-observable weights, a negative weight is
   accepted here. The default loss is root ``LOSS`` as it stood when
   this line was read. An optional :doc:`loss name <nfp_losses>` and
   parameter override it for this variable only. A target that the loss
   cannot measure is refused.

Inputs and diagnostics
----------------------

``--invar <name> <value>``
   Supply a LAMMPS command-line variable, equivalent to ``-var``.
   The value is one token.

``--outvar <name>``
   Record an equal-style variable without adding a loss term.
   ``--varloss`` variables are recorded automatically. An unreadable
   diagnostic becomes ``NaN``; an unreadable loss variable fails the
   script. The first output of a ``PC_FAILPROBE`` is its required
   scalar, so unreadable probe output fails the probe.

Timeout and failed-script values
--------------------------------

``--maxtime <time>``
   Override ``PC_SCRIPT_TIMEOUT`` for this script: ``off``, ``unlimited``
   or a positive ``[[HH:]MM:]SS`` time.

``--failscore <value>``
   Value substituted for this script's output and loss variables if it
   fails. Any finite number is accepted for ``PC_LAMMPS``; default:
   root ``FAILSCORE`` when the line is read. The substituted variable
   passes through its configured loss for reporting, but the whole
   evaluation still receives a failure score. On ``PC_FAILPROBE`` this
   option instead requires a positive number.

Warm variables
--------------

``--hotvar <name> <min> <max> [<initial>]``
   Declare a variable that the script returns for reuse by subsequent
   evaluations and by other scripts that reference it. Bounds and
   initial value must be finite, with ``max > min``. The omitted initial
   value is the midpoint. The returned value must lie in the closed
   interval; an out-of-range value fails the evaluation.

The script must define the returned value as an equal-style variable.
HPO injects the cached seed before running it. Scripts without
``--hotvar`` that reference a returned hot value run after its producer.
Each hot-variable name must have only one producing script.
A line that defines the same name with ``--invar``, ``-var`` or ``-v``
is refused, when the name is a ``--hotvar`` of the line or one its
script reads: those options create index-style variables, which
conflict with equal-style injection.

Violation variables
-------------------

``--failvar <name> [<scale>]``
   Declare an equal-style, nonnegative violation measure: zero means
   no detected failure; a positive value indicates a failed simulation.
   The optional scale must be finite and positive; default ``1``.

A positive violation marks the simulation as failed, and its evaluation
fails. With ``FAILSCORE_GRADING ON``, ``scale * value`` grades its
severity. With grading ``OFF``, the evaluation scores ``FAILSCORE``,
``hpo_run.log`` has a warning naming the variable, and an explicit
scale is refused. In either mode an unreadable or non-finite fail
variable fails the simulation. Define it before commands that might
prevent the script from reaching its end.

LAMMPS command-line options
---------------------------

Single-dash options and their values are forwarded to LAMMPS, for
example ``-screen none``, ``-log run.log`` or ``-var pressure 0``; the
short forms ``-sc`` and ``-l`` count as ``-screen`` and ``-log``.
LAMMPS checks whether its build supports them. HPO reserves ``maxtime``
for its timer: ``--invar maxtime``, ``-var maxtime`` and ``-v maxtime``
are refused.

Double-dash options belong to HPO. Unknown options and extra values
after an HPO option are errors. ``--script``, ``--failscore`` and
``--maxtime`` are given once per line; a variable is declared once by
``--hotvar`` or ``--failvar``, and defined once by ``--invar``, ``-var``
or ``-v``. Parallel script execution uses
``THREADS`` and ``PC_ISOLATION``; the old ``--ncpu`` option is rejected.
``--gate`` and ``--s0`` belong only to ``PC_FAILPROBE``.
