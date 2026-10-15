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
archives. They do not change the objective or the search bounds.

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

Each has a corresponding ``best_`` history, such as
``best_params.tadah``. Its first column is the improvement ordinal,
starting at one, rather than the evaluation number. These files append
history rows; they are not single-row snapshots. Read each file's header
for the enabled columns.

Potential archives
------------------

.. code-block:: none

   DUMP 100 pots
   BDUMP 5 best_pots

``DUMP N directory`` archives the potential of every ``N``-th
evaluation as ``directory/pot_<evaluation>.tadah``.
``BDUMP N directory`` archives every ``N``-th improvement as
``directory/best_pot_<improvement>.tadah``. Both require integer
``N >= 1`` and create the directory if necessary. Omit the directive to
leave that archive disabled.

``DUMP`` is an evaluation archive: a saved candidate need not be a
successful fit or a new best. Points rejected for being outside the
search box are not archived.

The current best is saved to ``best_pot.tadah`` independently of
``OUTPUT``, ``BOUTPUT``, ``DUMP``, ``BDUMP`` and ``QFLUSH``. Reducing the
history or archive cadence does not reduce how often this potential is
updated. ``DIGITS`` controls report formatting; it does not set the
precision of the potential's parameter values.

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
