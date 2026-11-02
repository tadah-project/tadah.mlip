.. _pc_lammps_execution:

.. _physics-informed-constraints-pc-lammps:

.. _physical-constraints:

.. _example:

LAMMPS Targets and Probes
=========================

A :hpo:key:`PC_LAMMPS` line adds loss terms on physical properties
computed by a LAMMPS input script. The key is a top-level key of the
:doc:`target file <nfp_configuration>`. Each line needs ``--script`` plus
at least one ``--varloss``.

.. code-block:: none

   LOSS L2
   FAILSCORE 1e9
   PC_SCRIPT_TIMEOUT 1:30
   PC_LAMMPS --script Ecoh.in --varloss Ecoh 8.10 100 --outvar a0

Here ``Ecoh`` and ``a0`` must be defined as equal-style variables in
``Ecoh.in``. Each ``--varloss`` creates one loss term; ``--outvar`` alone
records a diagnostic. Script option details are on
:doc:`nfp_lammps_options`.

The parser rejects a target file that mixes :hpo:key:`!PC_LAMMPS` with
:hpo:key:`PC_OBSERVABLE`. Several :hpo:key:`!PC_LAMMPS` lines are
allowed. Their declared order determines the output-column order, even
when scripts execute in parallel.

Execution keys
--------------

These are top-level keys, outside the :hpo:block:`OPTIMIZER` block.
:hpo:key:`PC_ISOLATION` chooses whether the scripts run in worker
processes or in threads of the ``tadah`` process.
:hpo:key:`PC_SCRIPT_TIMEOUT` is the wall-clock limit of each section of a
script: the lines up to the first ``clear`` and those after each
``clear``. :hpo:key:`PC_WORKER_TIMEOUT` is the deadline of a script task
in a worker process. The values and defaults are in the reference entries
of the three keys.

For a finite soft limit :math:`t`, the derived hard deadline is
:math:`\max(3t,t+120\,\mathrm{s})`. With no soft timeout, ``0`` means no
hard deadline. Deadlines exceeding the worker's millisecond-counter
range are refused. Under ``THREAD`` the scripts have no worker deadline,
so :hpo:key:`!PC_WORKER_TIMEOUT` and
:hpo:key:`TADAH_PC_WORKER_TIMEOUT <ENV.TADAH_PC_WORKER_TIMEOUT>` are
refused there.

.. versionchanged:: 1.6.0
   1.5.0 accepted :hpo:key:`!PC_WORKER_TIMEOUT` under ``THREAD``, where it
   had no effect.

:hpo:key:`!PC_SCRIPT_TIMEOUT` accepts ``90``, ``1:30`` and ``00:01:30``
for 90 seconds; ``off`` disables the limit, while ``0`` is invalid. A
:hpo:key:`!PC_LAMMPS` or :hpo:key:`PC_FAILPROBE` line without
:hpo:key:`--maxtime` reads the :hpo:key:`!PC_SCRIPT_TIMEOUT` above it
when the line is read; ``--maxtime`` gives the line its own limit. A
second :hpo:key:`!PC_SCRIPT_TIMEOUT` line is refused unless a
:hpo:key:`!PC_LAMMPS` or :hpo:key:`!PC_FAILPROBE` line without
:hpo:key:`!--maxtime` between the two reads the first. A
:hpo:key:`!PC_SCRIPT_TIMEOUT` line that no later line reads is also
refused. The top-level :hpo:key:`MAXTIME <ROOT.MAXTIME>` is another name
for this key. :hpo:key:`MAXTIME <OPTIMIZER.MAXTIME>` inside the
:hpo:block:`!OPTIMIZER` block instead limits a search.

The environment variables :hpo:key:`TADAH_PC_ISOLATION <ENV.TADAH_PC_ISOLATION>`
and :hpo:key:`!TADAH_PC_WORKER_TIMEOUT <ENV.TADAH_PC_WORKER_TIMEOUT>`
override the corresponding keys. The :hpo:key:`THREADS <OPTIMIZER.THREADS>`
key of the :hpo:block:`!OPTIMIZER` block sets the number of workers that run
the LAMMPS scripts and the training and prediction of the potential at the
same time.

PC_FAILPROBE
------------

A probe supplies a scalar violation measure used by
:doc:`failure grading <nfp_losses>` and can reject a candidate before
all its property calculations finish. At most one probe is allowed.
It runs at the front of the first group of scripts; other scripts may
already be running when its result becomes available.

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   PC_FAILPROBE --script failprobe.in --outvar S --gate 100 --s0 1

``--script`` and at least one ``--outvar`` are required. The first
``--outvar`` is the probe value: it should be nonnegative, with zero
indicating no detected violation. Additional outputs are diagnostics.

A probe line takes the options of a :hpo:key:`PC_LAMMPS` line described in
:doc:`nfp_lammps_options`, except ``--varloss``, ``--hotvar`` and
``--failvar``, and two options of its own. An evaluation whose probe value
exceeds the threshold of :hpo:key:`--gate <PC_FAILPROBE.--gate>` fails,
with a graded score under :hpo:key:`FAILSCORE_GRADING` ``ON`` and with
:hpo:key:`FAILSCORE` under ``OFF``. Without ``--gate``, only a probe
failure fails the evaluation. The scale of
:hpo:key:`--s0 <PC_FAILPROBE.--s0>` shapes the grading curve and is
refused under ``OFF``. The syntax, defaults and ranges of all options are
in the reference entry of :hpo:key:`PC_FAILPROBE`.

A probe adds no loss term: a target file with a probe and no validation
metric, :hpo:key:`!PC_LAMMPS` or :hpo:key:`!PC_OBSERVABLE` line has no loss
term and is refused. A probe has failed when its script fails, or when the
probe value cannot be read, is negative, is not finite, or is at or above
the ``--failscore`` of the probe line (see the reference entry of
:hpo:key:`!PC_FAILPROBE`). A probe failure, and a probe value above
``--gate``, fail the evaluation in both grading modes (see
:ref:`nfp_failscore_grading`).
The output variables that the probe requests must be equal-style
variables, as for other script outputs.
