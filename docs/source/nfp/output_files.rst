.. _nfp_output:

Output Files
============

This guide describes the files of ``tadah hpo``, how to read them and how a
run ends. It builds on a first search, such as that of the :ref:`nfp_quickstart`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_output_files:

The files of a run
------------------

A run writes its files into the working directory; ``--check`` (:ref:`cli_hpo`)
writes none. The run rewrites the journal as it starts, the history files once
its input is read, and removes ``best_pot.tadah`` before its first evaluation.
A file of an earlier run stays until this run rewrites or removes it.

``hpo_run.log``
   The journal: the settings, the progress and the result of the run.
``loss.tadah``, ``params.tadah``, ``outvar.tadah``, ``stats.tadah``
   The history files: one row per evaluation.
``best_loss.tadah``, ``best_params.tadah``, ``best_outvar.tadah``, ``best_stats.tadah``
   The best history files: one row per improvement of the best loss.
``best_pot.tadah``
   The :term:`best potential` (:ref:`nfp_output_best`).
``hpo_search_space.tsv``
   One row per search parameter: its label, its bounds and their origin
   (an :hpo:key:`OPTIM` line or an :hpo:key:`AUTO <OPTIM.AUTO>` rule), its
   starting value, its shape and its coordinates in the optimiser. Comment
   lines at the top define the columns, which tabs separate.
``hpo_optim_resolved.hpo``
   Written when an :hpo:key:`!AUTO <OPTIM.AUTO>` rule sets a box: one
   :hpo:key:`!OPTIM` line per search parameter, with the exact bounds of the
   run. In place of the :hpo:key:`!OPTIM` lines of the target file, they
   search the same space again (:ref:`nfp_search_space_bounds`).
The :hpo:key:`DUMP` and :hpo:key:`BDUMP` directories
   Archived potentials (:ref:`nfp_output_archives`).
``hpo_step_scales.tsv``, ``hpo_temperature_ladder.tsv``, ``hpo_init_jitter.tsv``, ``hpo_corana_acceptance.tsv``
   Tables of :hpo:opt:`TADAH:ANNEAL_PT` (:ref:`nfp_annealing`): the steps of
   its warm-up, unless :hpo:param:`STEP_MODE` is ``FIXED`` or
   :hpo:param:`CORANA` is ``ON``; the measured displacements under
   :hpo:param:`INIT_MODE` ``JITTER AUTO``; the ladder under
   :hpo:param:`LADDER_ADAPT`; the acceptance under :hpo:param:`!CORANA`.
``asa_out``
   The output file of the ASA library (:hpo:opt:`ASA:ASA`, :ref:`nfp_asa`); a
   later ASA search of the run writes ``asa_out.1``, and so on.
``hpo_phase_times.csv``, ``hpo_script_times.csv``
   Written when the environment variable ``TADAH_HPO_BENCH`` is set and not
   ``0``: the times of the stages of every evaluation that runs to its end
   (training, prediction, LAMMPS) and of every :hpo:key:`PC_LAMMPS` and
   :hpo:key:`PC_FAILPROBE` script, in milliseconds.

On several MPI ranks, each rank writes its own journal, history files, archives
and timing tables, named with ``.r<rank>`` at the end. ``best_pot.tadah`` is
the best of all ranks, and ``best_pot.tadah.r<rank>`` that of one rank.

.. _nfp_output_histories:

Reading the journal and history files
-------------------------------------

``hpo_run.log`` is written as the run goes: a header (version, start time,
working directory, journal name, process ID), then ``SETUP DIAGNOSTICS``
(notes of the startup, if any), ``FILES``, ``OPTIMISER``, ``SEARCH SPACE``,
``POTENTIAL SETUP``, ``LOAD TRANSFORMS`` (if any, :ref:`load_transforms`),
``LOSS FUNCTION`` (the loss terms and :hpo:key:`FAILSCORE`),
``NOISE CALIBRATION`` (if :hpo:key:`CALIBRATION <NOISE.CALIBRATION>` asks for
it), ``PROGRESS``, ``DIAGNOSTICS``, ``PERFORMANCE`` and ``RESULT``.
``PROGRESS`` has one line per event, with its time and a tag: ``INFO``,
``WARNING``, ``HINT`` or ``ERROR`` for a note; ``BEST`` for an improvement of
the best loss, with its evaluation, its loss and its validation errors, and a
``top:`` line of its largest :term:`loss terms <Loss term>` (at most five);
``SUMMARY`` at widening intervals. ``DIAGNOSTICS`` counts the evaluations, the
failed ones and the unusable losses, and says whether the best point lies at a
bound. ``RESULT`` gives why the run ended (:ref:`nfp_output_end`), the best
loss, its evaluation and the validation errors of the best potential.

Each history file starts with a line that names its columns. The first two are
``#ITERATION``, the number of the evaluation, and ``SLOT``; ``outvar.tadah``
has ``IT-TIME/ms``, the wall time of the evaluation in milliseconds, between
them. The other columns depend on the target file:

``loss.tadah``
   The loss of each term. ``L(<metric>)[i]`` is the loss of the metric on
   validation dataset ``i``, and ``L(<metric>)[avg]`` the term the metric adds
   to the loss. ``L(<variable>)`` belongs to a
   :hpo:key:`--varloss <PC_LAMMPS.--varloss>` variable, and ``L(<tag>)[k]`` to
   the ``k``-th :hpo:key:`PC_OBSERVABLE` line (``L(Ecoh)[1]``). The last
   column, ``TOTAL_LOSS/N``, is the loss of the evaluation, with ``N`` the
   number of loss terms (:ref:`nfp_targets_mean`). An entry of the journal's
   ``top:`` line is a term over ``N`` with its share: ``<metric>_global`` for
   ``L(<metric>)[avg]``; ``<metric>[i]`` for dataset ``i`` of an indexed line,
   ``L(<metric>)[i]`` over the number of validation datasets.
``params.tadah``
   The value of each search parameter in the units of its key, not in the
   coordinates of the optimiser. The columns are the rows of
   ``hpo_search_space.tsv``, with their labels (``RCUT2B``, ``CGRIDMB[2]``).
``outvar.tadah``
   Each metric, averaged over the validation datasets
   (:ref:`nfp_targets_validation`), in its unit (:ref:`nfp_targets_losses`);
   each :hpo:key:`!--varloss <PC_LAMMPS.--varloss>` variable and each
   :hpo:key:`--outvar <PC_LAMMPS.--outvar>` variable (of :hpo:key:`PC_FAILPROBE`
   too); the value ``<tag>[k]`` of each :hpo:key:`!PC_OBSERVABLE` line.
``stats.tadah``
   Each metric on each validation dataset, ``<metric>[i]``, and their average,
   ``<metric>[avg]``. Without a metric the file holds its header and a comment
   line only.

A cell that the evaluation did not compute, such as a metric of a candidate
refused before training, holds ``nan`` (``NaN`` in ``outvar.tadah``). Each
``best_`` file has the columns of its history file, with ``#IMPROVEMENT``, the
improvement number, and ``EVAL``, its evaluation, in place of ``#ITERATION``.

.. versionchanged:: 1.6.0
   1.5.0 wrote the improvement number in a column ``#ITERATION`` of the
   ``best_`` files and gave no evaluation.

Rows reach the files in groups of :hpo:key:`QFLUSH` results, a few
evaluations behind the journal. :hpo:key:`OUTPUT` and :hpo:key:`BOUTPUT`
thin the rows of a long run; :hpo:key:`DIGITS` sets the digits:

.. code-block:: none

   OUTPUT   10          # a history row for every tenth evaluation
   QFLUSH   1           # rows written as each evaluation ends
   DIGITS   10          # more digits in history files and potential headers

.. _nfp_output_numbers:

Evaluation numbers
~~~~~~~~~~~~~~~~~~

Evaluations are numbered in the order in which the optimiser proposes them;
those of a :term:`batch <Batch>` slot by slot (:ref:`nfp_annealing_terms`;
``SLOT``). Each is recorded once the lower-numbered ones have ended. A stop can
leave a number without a row, which the journal names.

.. _nfp_output_best:

The best potential
------------------

``best_pot.tadah`` is the potential of the evaluation with the lowest loss so
far. The run replaces it in a single step at each improvement of the best loss,
whatever :hpo:key:`OUTPUT`, :hpo:key:`QFLUSH`, :hpo:key:`DUMP` and
:hpo:key:`BDUMP` say. It is thus a whole potential at any time, also after the
process is killed. It serves as any other potential, in ``tadah predict``
(:ref:`prediction`) and in LAMMPS (:ref:`lammps_usage`), and as the seed of a
later search (:hpo:key:`WARM <METHOD.WARM>`). Its header, comment lines that
are not read as keys, has these parts:

``# HPO_BEST_*`` and ``# HPO_SOURCE_*`` lines, and a banner
   For programs: the format version, the loss at full precision, the MPI
   rank, the slot and the evaluation. For readers: the evaluation, the
   improvement, the loss and the time.
``LOSS FUNCTION``
   The loss function (:hpo:key:`LOSS`), :hpo:key:`FAILSCORE` and every loss term
   with its target and weight; the term of a metric line, such as
   :hpo:key:`PC_ERMSE`, is ``global (loss of average metric)``, or
   ``indexed (average of dataset losses)`` with a target and weight per dataset
   (:ref:`nfp_targets_datasets`). A :hpo:key:`PC_LAMMPS` line lists its script
   and its variables; the ``k``-th :hpo:key:`!PC_OBSERVABLE` line is named after
   its observable (``cohesive_energy[1]``).
``RESULT``
   The value and the loss of each metric on each validation dataset (``[i]``)
   and on average (``[avg]``, the term the metric adds to the loss), of each
   variable of the :hpo:key:`!PC_LAMMPS` lines (an
   :hpo:key:`!--outvar <PC_LAMMPS.--outvar>` variable has no loss) and of each
   :hpo:key:`!PC_OBSERVABLE` line with its unit (``Ecoh (eV/atom)[1]``).
   ``Total loss / N``, the mean of the loss terms, is the loss of the potential.
``DATA``, ``LOAD TRANSFORMS``, ``OPTIMISER``
   The data; their transforms, if any; the optimiser with its stops and
   settings, and the bounds of every search parameter.
``# HOT`` lines
   The value each :hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` variable reached,
   from which a :hpo:key:`!WARM <METHOD.WARM>` seed starts its scripts.
The training record
   The settings of the run that are not keys of the potential
   (:ref:`training record <training_record>`), ending with the
   :term:`balance table <Balance table>` of the fit (:ref:`fit_balance`). The
   key lines of the potential follow it.

.. _nfp_output_archives:

Saving potentials
-----------------

``best_pot.tadah`` keeps the last best only. :hpo:key:`BDUMP` keeps the
potential of every ``n``-th improvement, :hpo:key:`DUMP` that of every ``n``-th
evaluation. ``BDUMP 1`` is the recommended way to follow a search, as
improvements are far fewer than evaluations. A small ``DUMP`` cadence suits
short runs only, as each archive is a whole potential:

.. code-block:: none

   DUMP     25 pots      # every 25th candidate, to compare them later
   BDUMP    1 best_pots  # the potential of every improvement

An archive has the ``# HOT`` lines, key lines and training record of its
potential, without the balance table. Above them, ``best_pot_<i>.tadah``
names its improvement, evaluation, loss and time and has the ``RESULT``
table; ``pot_<i>.tadah`` names its evaluation, loss and time. The
:hpo:key:`!DUMP` entry names the candidates whose archive has no potential.

.. _nfp_output_end:

How a run ends
--------------

A search ends at one of its stops (:ref:`nfp_optimizer_block_budgets`) or at
the stagnation window of a :hpo:block:`RESTART` block
(:ref:`nfp_optimizer_block_restart`). The run ends with its last search: the
last start of :hpo:key:`MULTISTART <STRATEGY.TYPE.MULTISTART>`, the refinement of
:hpo:key:`PRESTAGE <STRATEGY.TYPE.PRESTAGE>`. It then writes the queued rows
and the last sections of the journal, and prints the balance table of
``best_pot.tadah`` (not under ``-v 0``, :ref:`cli_hpo`). The status at the
top of ``RESULT`` says why the last search ended, here that of ``seed2.hpo``
(:ref:`nfp_example_gfs_descriptor`):

.. output: gfs_descriptor,gfs_descriptor_short hpo_run.log search=seed2.hpo
.. code-block:: text

   -- RESULT --------------------------------------------------------------
     Optimiser status        : DLIB:GFS: MAXEVAL reached

The exit status of ``tadah hpo`` tells how the run ended:

``0``
   At a stop, with ``best_pot.tadah`` written.
``1``
   On an error, such as one in the input or an unsaved best potential; the last
   best saved, if any, stays in ``best_pot.tadah``. Also without a usable loss:
   an ``ERROR`` line then says that no ``best_pot.tadah`` was written.
128 plus the signal number
   On ``SIGINT`` (Ctrl-C), ``SIGTERM`` or ``SIGUSR2``. The first such signal
   ends the run as a stop does, once the evaluations in progress end, and the
   status names the signal. ``tadah`` reports it at once on the standard error.
   A job scheduler such as SLURM can send ``SIGUSR2`` before a time limit.

A second ``SIGINT`` or ``SIGTERM`` ends the process at once, as ``SIGKILL``
does. The history files can then lack the queued rows and the results a batch
held, and the journal its ``RESULT`` section. ``best_pot.tadah.lock`` can
remain, and the next run reuses it. Signals of different kinds do not add up,
nor do two ``SIGUSR2``: the stop of the first goes on.
