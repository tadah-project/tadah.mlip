.. _output_files:

.. _hpo_potential_archives:

.. _best-improvement-histories:

.. _best-only-snapshots:

.. _main-log-files:

.. _potential-archives:

.. _run-log:

Output settings
===============

These root-level keys control history files and optional potential
archives.

History cadence and precision
-----------------------------

.. list-table:: History options
   :header-rows: 1
   :widths: 25 15 60

   * - Syntax
     - Default
     - Effect
   * - ``OUTPUT N``
     - ``1``
     - Write a row to the main history files when the evaluation
       number is divisible by ``N``. Requires integer ``N >= 1``.
   * - ``BOUTPUT N``
     - ``1``
     - Write a row to the best histories for every ``N``-th improvement.
       Requires integer ``N >= 1``.
   * - ``DIGITS d``
     - ``6``
     - Decimal digits in scientific-notation history columns and
       potential-header result tables. Requires integer ``1 <= d <= 15``.
   * - ``QFLUSH N``
     - ``10``
     - Queue threshold for draining completed results to the history
       writer. Requires integer ``N >= 0``; ``0`` and ``1`` attempt a
       drain after every result. The run drains remaining results at exit.

``OUTPUT`` and ``BOUTPUT`` are cadences, so zero is invalid. Use a larger
positive value to reduce history volume. A queue drain can still wait
for an earlier evaluation to finish or for an active writer.

.. list-table:: History files
   :header-rows: 1
   :widths: 30 70

   * - File
     - Contents
   * - ``loss.tadah``
     - Individual enabled loss terms and total loss.
   * - ``params.tadah``
     - Searched parameter values in their selected physical coordinates.
   * - ``outvar.tadah``
     - Evaluation time, validation metrics and property outputs.
   * - ``stats.tadah``
     - Enabled validation metrics by dataset and their averages.

The first column of these histories, ``#ITERATION``, is the evaluation
number. These files accumulate history rows; each file's header lists the
enabled columns.

Each has a corresponding ``best_`` history, such as
``best_params.tadah``, with a row for each improvement of the best loss
that ``BOUTPUT`` writes. Its first column, ``#IMPROVEMENT``, numbers the
improvements from one. Its second, ``EVAL``, gives the ``#ITERATION`` of
the evaluation that made the improvement. The matching main history, such
as ``params.tadah``, holds that row when ``OUTPUT`` writes the evaluation,
which ``OUTPUT 1`` does for every evaluation.

The run writes these files, ``hpo_run.log`` and ``best_pot.tadah`` into
the working directory. A working directory without write permission, or
an existing history file, journal or lock of ``best_pot.tadah`` that the
run cannot open (to write; the lock to read and write), stops the run,
and ``tadah hpo --check``, at startup, before any data is loaded. A
history file, the journal or the lock that is a symbolic link is written
through the link, and is checked where the link leads;
``best_pot.tadah`` is replaced, link and all. ``hpo_run.log`` may be a
named pipe, which the run opens once and whose reader it waits for; a
history file may not, as the run reopens it for each row. A device such
as ``/dev/null`` may take any of these files, a socket none.

Potential archives
------------------

.. code-block:: none

   DUMP 100 pots
   BDUMP 5 best_pots

``DUMP N directory`` archives the potential of every ``N``-th
evaluation as ``directory/pot_<evaluation>.tadah``.
``BDUMP N directory`` archives every ``N``-th improvement as
``directory/best_pot_<improvement>.tadah``. Both require integer
``N >= 1``. Without the line, that archive is disabled.

The directories are checked when the target file is read, where the
operating system puts them: symbolic links are followed and ``..`` is
taken from the directory reached. A path that is itself a symbolic link
must lead to an existing directory, because the run creates no directory
through a link: ``DUMP 100 pots``, with ``pots`` a link to a directory
that does not exist, is refused at its line. An existing directory must
have write permission. A directory that does not exist must be in an existing
directory with write permission, since only the last directory of the
path is created, or in the directory of the other line, which the run
creates first (``DUMP 100 archive`` with ``BDUMP 5 archive/best``, in
either order). Otherwise the run, and ``tadah hpo --check``, stop at the
line. The run creates a missing directory right before its first
evaluation, so a run that stops before then creates none.

``DUMP`` archives evaluated candidates, including failed fits and
candidates whose losses exceed the current best. Archiving is limited
to points inside the search box.

The current best is saved to ``best_pot.tadah`` independently of
``OUTPUT``, ``BOUTPUT``, ``DUMP``, ``BDUMP`` and ``QFLUSH``. It is updated
at every improvement. Its header names the evaluation and the
improvement (``Evaluation 7, improvement 4``), as does the header of a
``BDUMP`` archive (``improvement 4 (eval 7)``). ``DIGITS`` controls report formatting; potential
parameters retain their full saved precision.

For an MPI PT/PA run, rank-local histories and archives receive rank
suffixes. The unsuffixed ``best_pot.tadah`` is the shared winning
potential.

Example
-------

.. code-block:: none

   OUTPUT 10          # one main history row per ten evaluations
   BOUTPUT 1          # retain every improvement in best histories
   DIGITS 10          # more precision in report columns
   QFLUSH 10          # batch history writing
   DUMP 100 pots      # occasional candidate snapshots
   BDUMP 1 best_pots  # archive each improvement

The search-space records ``hpo_search_space.tsv`` and, when ``AUTO``
bounds are present, ``hpo_optim_resolved.hpo`` are described in
:doc:`nfp_search_space`.
