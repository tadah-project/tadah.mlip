.. _nfp_example_warm_start:

Warm Start from an Earlier Search
=================================

This example starts a :term:`search` of a finer model at the
:term:`best potential` of the search of :ref:`nfp_unit_2`. It compares that
search with a search of the same model from its
:term:`training configuration`. It builds on :ref:`nfp_unit_2` and uses its
files; :ref:`nfp_optimizer_block_initialisation` covers the choice of
starting points.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When a warm start applies
-------------------------

A warm start suits a search that changes a solved problem a little: a finer
or larger model, more data of the same kind, or another accuracy, with
:term:`search parameters <search parameter>` that keep their meaning. Here
the pair term of :ref:`nfp_unit_2` gets twice the resolution over the same
range of distances, and the three search parameters stay those of Unit 2.

The recommended start is then the best potential of the earlier search,
read by :hpo:key:`WARM <METHOD.WARM>` in an :hpo:block:`INIT` block, with a
small first step and a small budget. The reason: the earlier best values
lie near those of the changed problem. A local search from the values of
the training configuration has to find that region again, and can end
elsewhere at a higher loss.

A search from the training configuration, or a global search
(:ref:`nfp_choosing_global`), suits a change that can move the best values
far from the earlier ones, such as other kinds of
:term:`loss terms <loss term>`.

Files
-----

The example reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>`,
:download:`validation configuration <../files/tutorial/unit2/config.val>`
and :download:`target file <../files/tutorial/unit2/config.hpo>`,
with their :ref:`datasets <nfp_unit_2>`. Every search of the example scores
its candidates on the validation data of Unit 2.

The training configuration of the finer model changes :ref:`TYPE2B`,
:ref:`CGRID2B` and :ref:`SGRID2B` of Unit 2 and keeps every other key.

:download:`config.train <../files/examples/warm_start/config.train>`:

.. literalinclude:: ../files/examples/warm_start/config.train
   :language: ini

The :term:`target file` keeps the boxes and the loss terms of Unit 2 and
starts the search at the best potential of the run before it.

:download:`config.hpo <../files/examples/warm_start/config.hpo>`:

.. literalinclude:: ../files/examples/warm_start/config.hpo
   :language: none

Running the searches
--------------------

The commands run in ``examples/warm_start`` (:ref:`layout of the files
<nfp_tutorial_files>`). The first command searches the finer model as Unit 2
searches its own model: with the target file of Unit 2, from the values of the
training configuration.

.. code-block:: bash

   tadah hpo --config config.train \
       --validation ../../tutorial/unit2/config.val \
       --hpotarget ../../tutorial/unit2/config.hpo

The second command repeats the search of Unit 2 in this directory. Its best
potential, ``best_pot.tadah``, is the seed of the third command;
:ref:`nfp_unit_2` gives its result.

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val \
       --hpotarget ../../tutorial/unit2/config.hpo

The third command searches the finer model from the seed:

.. code-block:: bash

   tadah hpo --config config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The three searches take about three minutes in all on one processor core.

Each run rewrites the journal and history files in its directory and
removes ``best_pot.tadah`` (:ref:`nfp_output_files`). The first command's
results are thus read before the second runs, and the second runs
immediately before the third. The third reads the seed at startup, warns about the
file, then removes it and writes its own best potential there:

.. output: warm_start stderr search=config.hpo

.. code-block:: text

   [tadah hpo] WARNING: the INIT block on line 17: METHOD WARM best_pot.tadah (line
       25) reads best_pot.tadah, the file this run writes its best potential to:
       the run removes it before its first evaluation and writes its best potential
       there, so only a copy under another name keeps this starting potential.

.. warning::

   A repeat of the third command from the seed needs the second command
   first. Without it, the repeat starts at the best potential of the third
   command, or stops at startup when that command wrote none.

A seed file that cannot be opened stops the run at startup, whatever
:hpo:key:`WARM_ON_MISMATCH` says. The refusal names the file:

.. hpo:refusal::

   METHOD WARM opt_missing.tadah

Expected result
---------------

:download:`expected.csv <../files/examples/warm_start/expected.csv>`:

.. csv-table::
   :file: ../files/examples/warm_start/expected.csv
   :header-rows: 1
   :widths: 60 40

The rows marked ``run 1`` belong to the first command, ``run 2`` to the
second, and ``config.hpo`` to the third.

With a budget of 75 evaluations, the search from the training configuration
ends at a best loss of 13.06, at an :ref:`RCUT2B` of 6.9 Å, an
:ref:`RCUTMB` of 5.6 Å and an :ref:`SGRIDMB` of 0.18 Å⁻².

The warm search starts at the best values of Unit 2, the ``Start`` rows. Its
first evaluation has a loss of 13.03, below the 14.33 of the second
command at those values: the finer pair term predicts the validation data
better. With 20 evaluations it reaches 12.96, at an :ref:`RCUT2B` of
6.7 Å, an :ref:`RCUTMB` of 5.3 Å and an :ref:`SGRIDMB` of 0.15 Å⁻², close
to the seed.

The journal of the third command, ``hpo_run.log``, shows that the seed
holds every search parameter and that the last evaluation is the best:

.. output: warm_start hpo_run.log search=config.hpo tol=best_loss

.. code-block:: text

   INFO      WARM: the starting point is read from best_pot.tadah, which gives a
       value for each of the 3 dimensions of the search space
   ...
     Starting point: 1 point from the INIT chain (1 WARM best_pot.tadah)
     Init strategy:
       [1] WARM best_pot.tadah
       WARM_ON_MISMATCH  : error
   ...
     Best total loss         : 1.2963e+01  (eval 20)

``hpo_search_space.tsv`` of the third command gives the seed as the
starting value of each search parameter (:ref:`nfp_output_files`), and
``params.tadah`` holds it in its first row.

How the values were chosen
--------------------------

The pair grid of the training configuration:

.. literalinclude:: ../files/examples/warm_start/config.train
   :language: ini
   :lines: 12-19

- 33 blips over the range of Unit 2, 2.0 to 6.0 Å, put one every
  0.125 Å, half the spacing of Unit 2. Their width keeps eight grid
  spacings within the support of each blip, as in Unit 2. Each evaluation
  costs more than in Unit 2.
- The centres keep the range of Unit 2, so the last centre stays at the
  lower bound of the :ref:`RCUT2B` box, and every box of Unit 2 keeps its
  meaning.
- Every other key is that of Unit 2: the example changes one thing, so
  the seed stays a good start. In this file the values of
  :ref:`RCUT2B`, :ref:`RCUTMB` and :ref:`SGRIDMB`, the start of Unit 2,
  serve the first command only; the third takes its start from the seed.

The :hpo:block:`INIT` block of the target file:

.. literalinclude:: ../files/examples/warm_start/config.hpo
   :language: none
   :lines: 17-29
   :dedent: 4

- :hpo:key:`WARM <METHOD.WARM>` takes the values of the search parameters
  from the seed and every other setting from the training configuration of
  the run (:ref:`nfp_optimizer_block_initialisation`). A seed with 17 pair
  blips thus starts a search with 33.
- :hpo:key:`WARM_ON_MISMATCH` ``error`` stops the run at startup when the
  seed lacks a search parameter or holds a value outside its box. The seed
  lies inside the boxes of Unit 2, more than one step from each bound
  (:ref:`nfp_choosing_step`), so the search starts exactly at the seed.

The optimiser keys:

.. literalinclude:: ../files/examples/warm_start/config.hpo
   :language: none
   :lines: 30-35
   :dedent: 4

- :hpo:opt:`NLOPT:LN_BOBYQA` is the optimiser of Unit 2. It starts from one
  point, the seed (:ref:`nfp_choosing_local`).
- :hpo:key:`STEP <OPTIMIZER.STEP>` ``0.05`` keeps the first evaluations
  near a trusted start: 5 % of the width of each box in the coordinates of
  the search, additive for the cutoffs and multiplicative for
  :ref:`SGRIDMB` (:ref:`nfp_choosing_step`). Unit 2 takes a quarter from a
  guessed start.
- :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` ``20`` gives
  :hpo:opt:`!NLOPT:LN_BOBYQA` 2\ *n* + 1 = 7 evaluations for its first
  model of the loss (*n* = 3 search parameters) and 13 to refine it
  (:ref:`nfp_choosing_budget`). Unit 2 spends 25 per search parameter from
  a guessed start. The warm search still improves at its last evaluation;
  a larger budget continues it (:ref:`nfp_choosing_budget`).
- Neither :hpo:opt:`!NLOPT:LN_BOBYQA` nor :hpo:key:`!WARM <METHOD.WARM>`
  draws random numbers, and :hpo:opt:`!NLOPT:LN_BOBYQA` refuses
  :hpo:key:`OPTIMIZER.SEED`.

Variations
----------

:hpo:key:`STEP <OPTIMIZER.STEP>` ``0.25``, the step of Unit 2, with the
same budget places the first evaluations a quarter of each box away from
the seed. The search then spends its first model on a region that a
trusted start does not need.

A run of :ref:`nfp_unit_2` in ``tutorial/unit2`` leaves its best potential
there. The line ``METHOD WARM ../../tutorial/unit2/best_pot.tadah`` reads
that seed without the second command. A run removes only the
``best_pot.tadah`` of its working directory (:ref:`nfp_output_files`), so
this seed survives the third command, which can then run again from it.

Narrower boxes, such as an :ref:`RCUTMB` box that ends below the seed,
show what :hpo:key:`WARM_ON_MISMATCH` does. With ``error`` the run stops
at startup; with ``warn`` the search starts from the value moved to the
bound, and a warning on the terminal and in the journal names the value
and its box.
