.. _nfp_example_gradient_observable:

Gradient Refinement with a Built-in Observable
==============================================

This example refines the best point of an earlier search with the
gradient-based optimiser :hpo:opt:`DLIB:BFGS`, with the cohesive energy of
the relaxed crystal in the loss. It builds on :ref:`nfp_unit_2`;
:ref:`nfp_choosing_gradients` covers the optimiser choice and what a
gradient costs.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

When it applies
---------------

A gradient-based refinement suits a start close to a minimum, such as the
best values of an earlier search, and a smooth loss. The analytical
gradient then costs one :term:`evaluation`, whatever the number of
:term:`search parameters <search parameter>`
(:ref:`nfp_choosing_gradients`). Every :term:`loss term` needs a
derivative: the energy and force metrics of this example have one, and so
does the cohesive energy of :hpo:key:`PC_OBSERVABLE`
(:ref:`nfp_targets_observables`). The fit also has to do without the
stresses.

With a guessed start, a global search comes first
(:ref:`nfp_choosing_global`); with a property computed by LAMMPS, a
gradient-free local search (:ref:`nfp_choosing_local`).
With stresses in the fit, only :hpo:key:`GRAD_STRATEGY` ``HYBRID`` runs,
and every gradient is a finite difference (`Variations`_).

Files
-----

The example reuses :download:`Unit 2's validation configuration
<../files/tutorial/unit2/config.val>`.
The datasets are :download:`ta_md_train.tadahdb
<../files/data/ta_md_train.tadahdb>` and :download:`ta_md_val.tadahdb
<../files/data/ta_md_val.tadahdb>` from :ref:`the Ta data <nfp_ta_data>`.

The :term:`training configuration` is that of Unit 2, at the best values
of its search, with the settings of the analytical gradient:

:download:`config.train <../files/examples/gradient_observable/config.train>`:

.. literalinclude:: ../files/examples/gradient_observable/config.train
   :language: ini

The :term:`target file` keeps the search space of Unit 2 and its energy
and force terms, adds the cohesive energy and replaces the optimiser:

:download:`config.hpo <../files/examples/gradient_observable/config.hpo>`:

.. literalinclude:: ../files/examples/gradient_observable/config.hpo
   :language: none

Running the search
------------------

The command runs in ``examples/gradient_observable`` (:ref:`layout of the files
<nfp_tutorial_files>`):

.. code-block:: bash

   tadah hpo --config config.train \
       --validation ../../tutorial/unit2/config.val --hpotarget config.hpo

The search takes under a minute on one processor core.

The run writes the usual files into this directory (:ref:`nfp_output`).

Important lines
---------------

The fit leaves the stresses out:

.. literalinclude:: ../files/examples/gradient_observable/config.train
   :language: ini
   :start-at: # The analytical gradient does not
   :end-at: STRESS

Unit 2 fits with the evidence approximation, which chooses the
penalty :ref:`LAMBDA` itself. The analytical gradient differentiates the normal
equations at a fixed penalty, so the training configuration sets
:ref:`OALGO` and a fixed ``LAMBDA``:

.. literalinclude:: ../files/examples/gradient_observable/config.train
   :language: ini
   :start-at: # The analytical gradient differentiates
   :end-at: LAMBDA

The :hpo:key:`GRAD_STRATEGY` entry lists every setting the analytical
gradient needs, in both files; startup refuses a setup that lacks one.
With :ref:`STRESS <stress>` true, the error at the ``ENDOPTIMIZER`` line
names the stress rows, which the analytical gradient does not
differentiate, and proposes ``GRAD_STRATEGY HYBRID`` (`Variations`_).

The cohesive energy enters the loss through a built-in observable:

.. literalinclude:: ../files/examples/gradient_observable/config.hpo
   :language: none
   :start-at: PC_OBSERVABLE cohesive_energy
   :end-at: --hotvar a0

``tadah`` builds the body-centred cubic (bcc) crystal itself and relaxes
the cell to zero pressure (:hpo:key:`cohesive_energy`). The pressure
tolerance, :hpo:key:`--ptol <PC_OBSERVABLE.cohesive_energy.--ptol>`, keeps
its default. At the default the residual pressure is small, so the
cohesive energy depends very little on the lattice constant at which a
relaxation starts. Each relaxation starts where the
previous one ended (:hpo:key:`!cohesive_energy`). A relaxed lattice
constant outside the
:hpo:key:`--hotvar <PC_OBSERVABLE.cohesive_energy.--hotvar>` range fails
the evaluation.

The :hpo:block:`OPTIMIZER` block sets no :hpo:key:`!GRAD_STRATEGY`:

.. literalinclude:: ../files/examples/gradient_observable/config.hpo
   :language: none
   :start-at: LIB      DLIB
   :end-at: MAXEVAL
   :dedent: 4

Its default is ``NUMERICAL``, but beside a :hpo:key:`PC_OBSERVABLE` line
a gradient-based optimiser takes the analytical gradient
(:hpo:topic:`RULE.PC_OBSERVABLE_GRAD_STRATEGY`). :hpo:opt:`DLIB:BFGS`
draws no random numbers and refuses :hpo:key:`OPTIMIZER.SEED`; the search
repeats without one.

The journal, ``hpo_run.log``, records the choice. At the first
evaluation, its gradient diagnostic counts the search parameters with an
analytical derivative: all three.

.. output: gradient_observable hpo_run.log

.. code-block:: text

   Library / algo        : DLIB / BFGS
   Dimensions            : 3 parameters being optimised
   GRAD_STRATEGY         : ANALYTICAL (resolved for PC_OBSERVABLE)
   ...
   INFO      Gradient diagnostic (first evaluation): GRAD_STRATEGY ANALYTICAL |
       search parameters 3 | analytical 3 | finite differences 0

Expected result
---------------

:download:`expected.csv <../files/examples/gradient_observable/expected.csv>`:

.. csv-table::
   :file: ../files/examples/gradient_observable/expected.csv
   :header-rows: 1
   :widths: 60 40

The best loss is 16.51. Every searched value ends inside its box, away
from the bounds, so the box does not hold the refinement back. Stopping
tolerances can end the search before :hpo:key:`OPTIMIZER.MAXEVAL`. Here
the change of the loss from one iteration to the next falls below
:hpo:key:`FTOL_ABS`, at its default, and the journal records that stop:

.. output: gradient_observable hpo_run.log

.. code-block:: text

   Optimiser status        : DLIB:BFGS: FTOL_ABS reached

The iteration at which the change falls below :hpo:key:`!FTOL_ABS`, and
with it the count of evaluations, depends on the rounding of the build: the
status is the same. The journal's stop line counts each analytical gradient
as an evaluation, as :hpo:key:`!MAXEVAL` does; ``Total evaluations`` counts
the losses alone.

The header of ``best_pot.tadah`` lists every loss term with its value and
loss, the cohesive energy included (:ref:`nfp_output_best`).
``outvar.tadah`` holds the cohesive energy of every evaluation.

How the values were chosen
--------------------------

The start is the best point of Unit 2: a refinement starts where an earlier
search ended. The bounds are those of Unit 2, set by the physics of the
descriptors (:ref:`nfp_search_space_bounds`). The box is not narrowed
around the start: the best :ref:`RCUT2B` and :ref:`SGRIDMB` lie well away
from the starting point.

The error scales and the loss-term weights of the energy and force metrics
are those of Unit 2. The cohesive energy is a target here, not only a guard
against a broken crystal: its loss-term weight comes from the accuracy of
the energies, in eV/atom, by the rule of :ref:`nfp_targets_losses`. The DFT value
(:ref:`nfp_ta_reference`) is the target, and its lattice constant the
start of the first relaxation.

The penalty is small and positive. With a penalty of zero, the fit of the
start fails. Some embedding functions see no density in the training data,
so their columns are zero. The normal equations then have no unique solution.

The budget leaves room for the gradients and for the loss evaluations of
each line search.

Variations
----------

:hpo:key:`GRAD_STRATEGY` ``HYBRID`` gives the same search here, because
the descriptors differentiate every search parameter. It matters when a
search parameter has no analytical derivative, as :ref:`LAMBDA` has none
when it is searched. The analytical gradient is then refused at startup.
``HYBRID`` takes finite differences for that parameter alone
(:ref:`nfp_choosing_gradients`).
``NUMERICAL`` is refused beside :hpo:key:`PC_OBSERVABLE`.

Keeping the stresses, with :ref:`STRESS <stress>` true and a
:hpo:key:`PC_SRMSE_GPA` term, needs ``HYBRID`` here: every gradient comes
from finite differences, two loss evaluations per search parameter. The
fit then uses the stresses of the data, at the cost of a dearer gradient.

:hpo:opt:`NLOPT:LN_BOBYQA` refines the same start without gradients, with
or without the stresses (:ref:`nfp_choosing_local`).

The cohesive energy can also come from a LAMMPS script, as in
:ref:`nfp_unit_4`. A :hpo:key:`PC_LAMMPS` line gives no gradient, so
:hpo:opt:`DLIB:BFGS` is refused beside it
(:hpo:topic:`RULE.PC_LAMMPS_gradient_free`), and a target file holds either
kind of line, not both (:hpo:topic:`RULE.PC_OBSERVABLE_excludes_PC_LAMMPS`).
