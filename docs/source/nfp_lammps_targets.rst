.. _pc_lammps_execution:

.. _physics-informed-constraints-pc-lammps:

.. _physical-constraints:

.. _example:

LAMMPS targets and probes
=========================

Use ``PC_LAMMPS`` for physical properties computed by a LAMMPS input
script. Each line belongs at the root of the
:doc:`target file <nfp_configuration>` and needs ``--script`` plus at
least one ``--varloss``.

.. code-block:: none

   LOSS L2
   FAILSCORE 1e9
   PC_SCRIPT_TIMEOUT 1:30
   PC_LAMMPS --script Ecoh.in --varloss Ecoh 8.10 100 --outvar a0

Here ``Ecoh`` and ``a0`` must be defined as equal-style variables in
``Ecoh.in``. Each ``--varloss`` creates one loss term; ``--outvar`` alone
records a diagnostic. Script option details are on
:doc:`nfp_lammps_options`.

The parser rejects a target file that mixes ``PC_LAMMPS`` with
``PC_OBSERVABLE``.
Several ``PC_LAMMPS`` lines are allowed. Their declared order determines
the output-column order, even when scripts execute in parallel.

Execution keys
--------------

These are root keys, independent of the ``OPTIMIZER`` block.

.. list-table:: Execution settings
   :header-rows: 1
   :widths: 31 18 51

   * - Key
     - Default
     - Values and meaning
   * - ``PC_ISOLATION``
     - ``PROCESS``
     - ``PROCESS`` uses persistent worker processes; ``THREAD`` runs
       LAMMPS in the main process's worker threads. MPI-parallel HPO
       builds default to ``THREAD`` and reject explicit ``PROCESS``.
   * - ``PC_SCRIPT_TIMEOUT``
     - ``off``
     - ``off``, ``unlimited`` or a positive ``[[HH:]MM:]SS`` time.
       This is the script's soft LAMMPS timer limit.
   * - ``PC_WORKER_TIMEOUT``
     - ``0``
     - Finite, nonnegative seconds for a hard worker-process deadline.
       ``0`` derives the deadline from each script's soft timeout.

For a finite soft limit :math:`t`, the derived hard deadline is
:math:`\max(3t,t+120\,\mathrm{s})`. With no soft timeout, ``0`` means no
hard deadline. ``PC_WORKER_TIMEOUT`` affects ``PROCESS`` mode only.
Deadlines exceeding the worker's millisecond-counter range are refused.

``PC_SCRIPT_TIMEOUT`` accepts ``90``, ``1:30`` and ``00:01:30`` for
90 seconds; ``off`` disables the limit, while ``0`` is invalid. Each script line
captures the current value when parsed; ``--maxtime`` overrides it for
that line. Root ``MAXTIME`` is a legacy alias for this key. ``MAXTIME``
inside an optimiser block instead limits an optimiser search.

The environment variables ``TADAH_PC_ISOLATION`` and
``TADAH_PC_WORKER_TIMEOUT`` override the corresponding keys.
The optimiser's ``THREADS`` setting controls worker slots.

PC_FAILPROBE
------------

A probe supplies a scalar violation measure used by
:doc:`failure grading <nfp_losses>` and can reject a candidate before
all its property calculations finish. At most one probe is allowed.
It runs at the front of the first batch of scripts; other scripts may
already be running when its result becomes available.

.. code-block:: none

   FAILSCORE 1e9
   FAILSCORE_GRADING ON
   PC_FAILPROBE --script failprobe.in --outvar S --gate 100 --s0 1

``--script`` and at least one ``--outvar`` are required. The first
``--outvar`` is the probe scalar: it should be nonnegative, with zero
indicating no detected violation. Additional outputs are diagnostics.

.. list-table:: Probe options
   :header-rows: 1
   :widths: 32 68

   * - Option
     - Meaning
   * - ``--gate <threshold>``
     - With grading ``ON``, reject if the probe scalar exceeds this
       finite threshold. Omit it to disable probe-based rejection.
   * - ``--s0 <scale>``
     - Finite positive saturation scale for the grading curve;
       default ``1``.
   * - ``--failscore <value>``
     - Finite positive replacement value when the probe fails;
       defaults to root ``FAILSCORE`` at the probe line. A scalar at
       or above this value also counts as probe failure.
   * - ``--invar``, ``--outvar``, ``--maxtime``
     - Same syntax as :doc:`LAMMPS line options <nfp_lammps_options>`.

A probe takes no ``--varloss``, ``--hotvar`` or ``--failvar`` and adds no
ordinary loss term. With grading ``OFF``, its threshold and severity
measure are inactive; a script or scalar-readback failure still fails
the evaluation. The probe's requested output variables must follow the
same equal-style contract as other script outputs.

.. toctree::
   :maxdepth: 1

   nfp_lammps_options
