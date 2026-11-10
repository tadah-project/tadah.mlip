.. _nfp_example_pinned_coefficients:

Pinned Coefficients
===================

This example pins the :term:`coefficients <coefficient>` that no training
data reach to chosen values, and searches one of them against hot and
high-pressure validation snapshots while the fit sets all the others. It
builds on the :term:`training configuration` of :ref:`nfp_unit_2`;
:ref:`nfp_search_space_coefficients` introduces the keys it uses.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When pinning applies
--------------------

The many-body term of Unit 2's training configuration has an embedding
function F of the density of an atom: a sum of blips along the density
(:ref:`CEMBFUNC`). A blip that no training density reaches has a coefficient
that the training data do not determine. The fit gives that coefficient
zero, so above the densities of the data F follows from that zero, not
from the physics. In ``examples/pinned_coefficients`` (:ref:`layout of the
files <nfp_tutorial_files>`), the command below fits Unit 2's training
configuration without the pins:

.. code-block:: bash

   tadah train -c ../../tutorial/unit2/config.train -o unpinned.tadah

Its fit warns that the evidence approximation stopped at its iteration limit
(its last penalty holds), and that singular directions of the design matrix lie
at or below the rank threshold of :ref:`OALGO` ``3``. It leaves those directions
out of the coefficients, and its :term:`balance table` counts them out of the
rank:

.. output: pinned_coefficients stdout command=1
.. code-block:: text

   28 coefficients, rank 25

Pinning is the recommended choice for these coefficients when the potential may
meet denser environments than its training data hold but need not describe them
accurately. :ref:`FIXINDEX` and :ref:`FIXWEIGHT` set them to values from a
physical argument, here a straight continuation of the fitted coefficients. No
training row reaches them, so the fit of the other coefficients stays as it
was. A search then sets the pinned coefficients that validation data reach.

Training on snapshots of the denser environments is the alternative when
the potential is meant to describe them (see the variations below).

Which blips the data reach depends on the densities, and so on the
many-body descriptor. The example keeps the descriptor of Unit 2's training
configuration as written, not the best values of the search of Unit 2.

Files
-----

The training configuration includes :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` with :ref:`CONFIG` and adds the two
lines that pin coefficients 26 to 28.
The datasets are :download:`ta_md_train.tadahdb
<../files/data/ta_md_train.tadahdb>`, :download:`ta_md_val.tadahdb
<../files/data/ta_md_val.tadahdb>`, :download:`ta_md_ext_train.tadahdb
<../files/data/ta_md_ext_train.tadahdb>` and :download:`ta_md_ext_val.tadahdb
<../files/data/ta_md_ext_val.tadahdb>` from :ref:`the Ta data <nfp_ta_data>`.

:download:`config.train <../files/examples/pinned_coefficients/config.train>`:

.. literalinclude:: ../files/examples/pinned_coefficients/config.train
   :language: ini

:download:`config.val <../files/examples/pinned_coefficients/config.val>`
is the :term:`validation configuration`:

.. literalinclude:: ../files/examples/pinned_coefficients/config.val
   :language: ini

:download:`config.hpo <../files/examples/pinned_coefficients/config.hpo>`
is the :term:`target file`:

.. literalinclude:: ../files/examples/pinned_coefficients/config.hpo
   :language: none

Running the search and prediction
---------------------------------

The commands run in ``examples/pinned_coefficients``. The search runs first.
``tadah predict`` then measures its :term:`best potential` on the solid
validation snapshots and on ``ta_md_ext_train``: other steps of the MD runs of
``config.val``, which the search never sees. The prediction reports energy,
force and stress errors on both datasets (:ref:`prediction`; options in
:ref:`cli_predict`):

.. code-block:: bash

   tadah hpo --config config.train --validation config.val \
       --hpotarget config.hpo
   tadah predict -p best_pot.tadah -F -S -A -d ../../data/ta_md_val.tadahdb \
       ../../data/ta_md_ext_train.tadahdb

The three commands take a few seconds on one processor core.

The search runs to the budget of its :hpo:key:`MAXEVAL`. The ``RESULT``
section of the journal, ``hpo_run.log`` (:ref:`nfp_output`), gives the
status, the best loss with its evaluation, and the errors of the best
potential on the snapshots of ``config.val``:

.. output: pinned_coefficients hpo_run.log tol=stress_rmse
.. code-block:: text

   -- RESULT --------------------------------------------------------------
     Optimiser status        : MAXEVAL_REACHED -- maximum evaluations exhausted
     Best total loss         : 3.9271e+03  (eval 4)
     Best ERMSE              : 196.42 meV/atom
     Best FRMSE              : 2.1221 eV/Ang
     Best SRMSE              : 20.5301 GPa

Expected result
---------------

:download:`expected.csv <../files/examples/pinned_coefficients/expected.csv>`:

.. csv-table::
   :file: ../files/examples/pinned_coefficients/expected.csv
   :header-rows: 1
   :widths: 60 40

The first five rows are coefficients of ``unpinned.tadah``, the fit without
the pins (`When pinning applies`_): 20 and 25, the fitted values at rho 2 and 7
that the pinned values continue, and 26 to 28, which that fit sets to zero.
The best loss is 3927, at a coefficient 26 of 5.15 eV, inside its box.
The ``FIXWEIGHT`` line of ``best_pot.tadah`` holds this value with the two
other pinned values, and its :ref:`WEIGHTS` line holds it as coefficient 26
(:ref:`nfp_output_best`). The ``DIAGNOSTICS`` section of the journal reports
the best value away from the bounds (:ref:`nfp_search_space_bounds`).

With coefficients 26 to 28 pinned, the fit of an :term:`evaluation` of this
example has no undetermined direction left, and the warning on singular
directions does not appear. The balance table of the best potential, which ends
the run of ``tadah hpo``, counts the pinned coefficients apart, and its rank
equals the number of the others (:ref:`fit_balance`):

.. output: pinned_coefficients stdout tol=1e-2
.. code-block:: text

   [tadah hpo] best_pot.tadah:
   Fit balance: LAMBDA -1 (evidence approximation: 0.629867);
       25 coefficients (FIXINDEX pins 3 more), rank 25

In the table of ``tadah predict -A``, ``Val1`` is ``ta_md_val`` and ``Val2`` is
``ta_md_ext_train``, the order of ``-d`` (:ref:`prediction`). On ``ta_md_val``,
inside the training range, the errors are 13.0 meV/atom, 0.334 eV/Å and
1.12 GPa. No training density reaches the pinned blips, so they are those of
the fit of Unit 2's training configuration without the pins, as the comment of
``config.train`` states.

On ``ta_md_ext_train`` the errors are 200 meV/atom, 2.30 eV/Å and
24.7 GPa. They are of the size of the errors that the search measured on
its own snapshots (the ``RESULT`` section above): the value of coefficient 26
is not tuned to the steps of ``config.val``. They remain far above the errors
on the solid. Only one coefficient of F above the training densities is
searched, and the training data hold none of the extended MD snapshots
(:ref:`nfp_ta_data`). The pins give F a defined course where no data reach;
they do not make the potential accurate there.

How the values were chosen
--------------------------

**Which coefficients.** The comment of ``config.train`` numbers the
coefficients: the pair term first, then the blips of F. Blips 8 to 10 lie
above the training densities, so :ref:`FIXINDEX` pins their coefficients:

.. literalinclude:: ../files/examples/pinned_coefficients/config.train
   :language: ini
   :lines: 4-6,20

Of blips 8 to 10, the densities of the 400 GPa snapshots reach the central
part of blip 8 only, so the target file searches coefficient 26.
Coefficients 27 and 28 keep their pinned values.

**Pinned values.** :ref:`FIXWEIGHT` continues the fitted coefficients in a
straight line:

.. literalinclude:: ../files/examples/pinned_coefficients/config.train
   :language: ini
   :lines: 10-14,21

The pinned coefficients continue the mean increase of the fitted ones.
F of the start therefore keeps rising at about its slope over the training
densities, until the end of the grid bends it over. A straight continuation
is the choice that assumes least.

The best value of coefficient 26 lies below its start and below the fitted
coefficient of the blip at rho 7. The best F therefore dips slightly above
the training densities before it rises again.

**The search parameter.** The start is the pinned value of the training
configuration (:ref:`nfp_training_hpo_start`):

.. literalinclude:: ../files/examples/pinned_coefficients/config.hpo
   :language: none
   :lines: 3-7

The index names the coefficient as :ref:`FIXINDEX` numbers it
(:ref:`nfp_search_space_coefficients`). At the lower bound blip 8 adds nothing
to F, and the upper bound leaves the start near the middle of the box.

**Loss terms.** The :term:`loss-term weights <loss-term weight>` of
:hpo:key:`PC_ERMSE`, :hpo:key:`PC_FRMSE` and :hpo:key:`PC_SRMSE_GPA` come
from the accuracies of Unit 2 (:ref:`nfp_targets_losses`). The loss then
measures the errors on the snapshots against the accuracies that the error
scales of the fit express (:ref:`fit_objective`).

**Optimiser and budget.** :hpo:opt:`NLOPT:LN_BOBYQA` builds a quadratic
model of the loss from its first evaluations (:ref:`nfp_choosing_local`).
The loss is a parabola in coefficient 26, so the model is exact:

.. literalinclude:: ../files/examples/pinned_coefficients/config.hpo
   :language: none
   :lines: 18-24
   :dedent: 4

:hpo:key:`STEP <OPTIMIZER.STEP>` 0.25 suits a start that is a guess
(:ref:`nfp_choosing_step`). :hpo:opt:`!NLOPT:LN_BOBYQA` draws no random
numbers, so the target file sets no seed (:ref:`nfp_optimizer_block_threads`).

Variations
----------

``OPTIM FIXWEIGHT (26,27) 0 20``, with a larger :hpo:key:`MAXEVAL`, searches
coefficient 27 as well. The snapshots reach only the flank of blip 9, so they
constrain that coefficient weakly, and its pinned value is the safer choice.

Pinning a coefficient whose blip the training densities reach, such as that
of the blip at rho 4, changes the whole potential. The free coefficients
compensate for the pinned value inside the training range, and a search
over it moves the fit of every other coefficient.

Training on ``ta_md_ext_train`` as well determines from data the
coefficients of the blips that its densities reach. With the descriptor as
written, these are all blips except blip 10. The fit to the solid then
changes; :ref:`nfp_unit_7` shows that trade-off with the same datasets.

With other descriptor values, such as the best values of the search of
Unit 2, the densities of the data change, and with them the blips that no
training density reaches. A ``tadah train`` of that configuration counts
them in the same way, without naming them.
