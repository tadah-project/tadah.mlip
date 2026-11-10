.. _nfp_example_failure_grading:

Failed Evaluations and Their Grading
====================================

This example adds a physical check to the local search of :ref:`nfp_unit_2`:
a candidate potential whose compressed crystal shows a negative pressure
fails, and the search goes on among the others. It uses the configurations
and datasets of :ref:`nfp_unit_2` and the cohesive-energy script of
:ref:`nfp_unit_4`; :ref:`nfp_targets_failures` describes failed
:term:`evaluations <evaluation>`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When it applies
---------------

The :term:`search space` of :ref:`nfp_unit_2` can hold candidates that fit
the validation data well and still fail for a perfect crystal under
compression. The data of :ref:`nfp_unit_2` are molecular dynamics snapshots
of the solid at 0 to 250 GPa. A perfect crystal compressed into that range,
or beyond it up to the 395 GPa of the smallest cell of the check
(:ref:`nfp_ta_data`), can contract further on its own: its pressure is
negative. The validation errors do not show it, and such a potential is
unusable for compression or shock simulations.

A check of this kind suits a property that decides whether a potential is
usable at all, not how good it is. A property that only makes a potential
worse belongs in the loss as a term (:ref:`nfp_targets_scripts`).

Files
-----

The example reuses :download:`Unit 2's training configuration
<../files/tutorial/unit2/config.train>` and
:download:`validation configuration <../files/tutorial/unit2/config.val>`,
with their :ref:`datasets <nfp_unit_2>`.
The property script is :download:`ecoh_a0.in <../files/lammps/ecoh_a0.in>`
from :ref:`nfp_unit_4`.

:download:`config.hpo <../files/examples/failure_grading/config.hpo>`:

.. literalinclude:: ../files/examples/failure_grading/config.hpo
   :language: none

:download:`failprobe.in <../files/lammps/failprobe.in>`:

.. literalinclude:: ../files/lammps/failprobe.in
   :language: none

Running the search
------------------

The command runs in ``examples/failure_grading`` (:ref:`layout of the files
<nfp_tutorial_files>`):

.. code-block:: bash

   tadah hpo --config ../../tutorial/unit2/config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The search takes under a minute on one processor core.

The check
---------

``failprobe.in`` builds nine body-centred cubic (bcc) cells, with lattice
constants from 2.70 to 3.10 Å, and computes the pressure of each without
relaxing it. ``S`` adds up the magnitudes of the negative pressures, in GPa:

.. literalinclude:: ../files/lammps/failprobe.in
   :language: none
   :start-at: thermo_style
   :end-at: variable        S equal

``S`` is 0 when every cell pushes outwards, as the DFT cells of that range
do (:ref:`nfp_ta_data`). The upper end of the range is the lower bound of the
:hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` range of the ``ecoh_a0.in`` line,
so no cell is larger than the lattice constant of a potential that passes
that range.

The :hpo:key:`PC_FAILPROBE` line starts its script first in each
evaluation, after the validation, and reads its first
:hpo:key:`--outvar <PC_LAMMPS.--outvar>` as the probe value:

.. literalinclude:: ../files/examples/failure_grading/config.hpo
   :language: none
   :start-at: PC_FAILPROBE
   :end-at: --s0

:hpo:key:`--gate` ``0`` fails every evaluation whose ``S`` is above 0, that
is, with a negative pressure in any cell. ``ecoh_a0.in`` then does not run,
and the evaluation scores a failure loss. The line adds no loss term:
``S`` changes no loss of an evaluation that passes the check.

Expected result
---------------

:download:`expected.csv <../files/examples/failure_grading/expected.csv>`:

.. csv-table::
   :file: ../files/examples/failure_grading/expected.csv
   :header-rows: 1
   :widths: 60 40

The best potential, with a loss of 8.731, passes the check: none of
the nine cells has a negative pressure.

In this search the check rejects 7 candidates, the row ``Graded failures``.
The journal ``hpo_run.log`` warns of the first five rejections, each with
``S`` and the count so far:

.. output: failure_grading hpo_run.log tol=1e-2

.. code-block:: text

   WARNING   eval 4: the PC_FAILPROBE value 17.519476792114382 is above its --gate
       0: the evaluation fails, and its other scripts stop (1 evaluation stopped by
       --gate so far)

The ``DIAGNOSTICS`` section of ``hpo_run.log`` counts every failed
evaluation, by kind. All 7 are of kind D, a potential rejected as unusable.
Kind A counts the rejections of the
:hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` range of the ``ecoh_a0.in`` line:
there are none.

.. output: failure_grading hpo_run.log

.. code-block:: text

     Graded failures         : 7  (9.3%)
       A=0 (a --hotvar value outside its range)
           B=0 (a script or PC_OBSERVABLE term failed)
           C=0 (a simulation did not finish)
           D=7 (a potential rejected as unusable)
     Unusable losses         : 7  (9.3%)

``outvar.tadah`` records ``S`` for every evaluation. A rejected evaluation
has :hpo:key:`FAILSCORE` in place of ``Ecoh`` and ``a0``, which it did not
compute. Its loss, the last column of ``loss.tadah``, lies a little below
:hpo:key:`!FAILSCORE` and grows with ``S``. :ref:`nfp_output` describes the
output files.

The loss is the mean of five terms here and of three in :ref:`nfp_unit_2`,
so the two best losses are not comparable (:ref:`nfp_targets_mean`). The
``RESULT`` section of each journal gives the validation errors of its best
potential, and those are comparable.

How the values were chosen
--------------------------

The search space, the accuracies of the metrics, the optimiser,
:hpo:key:`MAXEVAL` and :hpo:key:`STEP` are those of :ref:`nfp_unit_2`, so
the check, the failure handling and the two property terms are the only
changes.

:hpo:key:`FAILSCORE` ``1e4`` lies far above the loss of any usable
candidate. Each term is the squared ratio of an error to its accuracy
(:ref:`nfp_targets_losses`), so a loss of ``1e4`` means errors about 100
times the chosen accuracies. The line stands above the two script lines, so
both take its value (:ref:`nfp_target_file_order`). On the
:hpo:key:`PC_FAILPROBE` line it is the
:hpo:key:`--failscore <PC_LAMMPS.--failscore>`: an ``S`` of 1e4 GPa or more
counts as a failed probe and gives the highest loss of a rejection.

:hpo:opt:`NLOPT:LN_BOBYQA` fits a model of the loss through the values it
is given. A failed evaluation gives it a finite stand-in, as the
:hpo:key:`!FAILSCORE` entry describes. Once a usable loss exists, the
stand-in differs from the loss in ``loss.tadah`` and does not depend on the
size of :hpo:key:`!FAILSCORE`. :hpo:key:`!FAILSCORE` sets which losses
count as failures and the losses the records show.

:hpo:key:`FAILSCORE_GRADING` ``ON`` orders the evaluations that the check
rejects by ``S``: a candidate that misses by little scores lower than one
that misses by much. An ``a0`` outside the
:hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` range is graded by its distance to
the range instead. :hpo:key:`--gate` rejects under ``ON`` and ``OFF`` alike;
:hpo:key:`--s0` needs ``ON``.

:hpo:key:`!--s0` ``400`` sets the scale of ``S`` in the graded loss: an
``S`` of 400 GPa gives a loss half-way between the lowest and the highest
loss the check gives. That is about the largest DFT pressure in the range of
the check. It spreads the losses of the rejected evaluations, and with them
the stand-ins the optimiser is given, so another :hpo:key:`!--s0` can change
the path of the search. The check itself does not depend on it.

:hpo:key:`THREADS` ``1`` runs the scripts one after the other, so the check
finishes before ``ecoh_a0.in`` starts and a rejection saves that script.

Variations
----------

Under :hpo:key:`FAILSCORE_GRADING` ``OFF`` a candidate that the check
rejects still fails, with the loss :hpo:key:`FAILSCORE`, and the optimiser
is given stand-ins that do not tell such failures apart. :hpo:key:`--s0` is
refused under ``OFF``, so the line of the check drops that option.

Without :hpo:key:`--gate` the line records ``S`` in ``outvar.tadah`` and
fails an evaluation only when the probe itself fails (the
:hpo:key:`PC_FAILPROBE` entry). Nothing then keeps a potential whose
compressed cells show a negative pressure from becoming the best potential.

A property script can report its own violation, such as an unstable
crystal after a relaxation, through :hpo:key:`--failvar` on its
:hpo:key:`PC_LAMMPS` line. A value above 0 fails the evaluation, and the
graded loss grows with it.

With :hpo:key:`THREADS` ``2`` the check and ``ecoh_a0.in`` start together.
Under :hpo:key:`PC_ISOLATION` ``PROCESS`` a rejection stops ``ecoh_a0.in``
if it is still running; under ``THREAD``, the only mode of the MPI build, it
runs to its end. A rejected evaluation can then record ``Ecoh`` and ``a0``,
and the search can take another path; the evaluation fails all the same.
