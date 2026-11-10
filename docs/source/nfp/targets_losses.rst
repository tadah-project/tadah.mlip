.. _nfp_targets:

Targets and Losses
==================

This guide shows how to choose what ``tadah hpo`` scores. It covers the
validation data, the targets and weights of the loss terms, the loss
function, properties from LAMMPS or ``tadah`` itself, and failed
evaluations. It builds on the :ref:`nfp_quickstart`;
:ref:`fit_objective` describes what each fit inside the search minimises.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

.. _nfp_targets_mean:

How the loss is computed
------------------------

Each :term:`evaluation` trains one potential and scores it with the
:term:`loss terms <loss term>` of the :term:`target file`. A term is its
:term:`weight <loss-term weight>` times the loss function of a measured value
and its target. Each validation metric gives one term, whatever the number of
its lines; each :hpo:key:`--varloss` one, and each :hpo:key:`PC_OBSERVABLE`
line one.

The loss of the evaluation is the mean of its terms; a term of weight 0 adds
nothing but still counts in the mean. Scaling every weight by one positive
factor scales every usable loss and keeps their order; a setting in loss units,
such as :hpo:key:`FAILSCORE`, needs the same factor. Losses of target files with
different terms cannot be compared. ``loss.tadah`` holds every term of every
evaluation and the loss of the evaluation (:ref:`nfp_output_histories`).

.. _nfp_targets_validation:

Choosing validation data
------------------------

The :term:`validation configuration` lists the validation datasets with
:ref:`DBFILE <dbfile>` lines (:ref:`validation`; data format in
:ref:`dataset`). Its data are never fitted
(:ref:`nfp_training_hpo_validation`), but they decide which potential is
best and so take part in the fitting; a test set that no loss term reads
measures the best potential (:ref:`nfp_overview_loss_data`).

The recommended validation data are structures that the training data do
not contain, from the conditions the potential is meant for. One dataset
per kind of structure is the recommended layout, as in Unit 3's
:download:`config.val <files/tutorial/unit3/config.val>`:

.. literalinclude:: files/tutorial/unit3/config.val
   :language: ini

``stats.tadah`` reports each metric for every dataset
(:ref:`nfp_output_histories`), and per-dataset targets can weigh them
(:ref:`nfp_targets_datasets`). A metric line without an index
list scores the mean of the per-dataset values, so a small dataset counts as
much as a large one: structures that belong together go into one file.
Validation predicts the forces and stresses its metrics need
(:ref:`nfp_training_hpo_fit`).

.. _nfp_targets_losses:

Choosing loss terms
-------------------

The recommended metrics are :hpo:key:`PC_ERMSE`, :hpo:key:`PC_FRMSE` and
:hpo:key:`PC_SRMSE_GPA`, one for each quantity of the fit. They measure
the energy per atom in meV/atom, the force components in eV/Å and the
stress tensors in GPa. These are the units of the
:term:`error scales <error scale>` of the training configuration (under
the default :ref:`ENORM`). Only these three take per-dataset targets.
:hpo:key:`PC_EMAE` is less sensitive to a few large energy errors.
:hpo:key:`PC_ErRMSE` measures energy errors relative to the energies.
:hpo:key:`PC_ERSQ` scores how much of the spread of the energies the
potential reproduces (:ref:`nfp_ref_pc_options`).

The recommended target of an error metric is 0. Under the default loss
function :hpo:key:`L2`, its weight is :math:`1/a^2`, where :math:`a` is
the accuracy chosen for that quantity. The term is then the squared ratio
of the error to the accuracy: a term of 1 is an error equal to it, and
terms of different units can be compared. Unit 2 takes the error scales of its
training configuration as its accuracies:

.. literalinclude:: files/tutorial/unit2/config.hpo
   :language: none
   :start-at: PC_ERMSE
   :end-at: PC_SRMSE_GPA

The accuracy is a fixed number of the target file; the error scale of
the quantity in the training configuration, or its default, is the usual
choice. A search of an error scale leaves the loss-term weights unchanged
(:ref:`nfp_unit_4` searches :ref:`ESIGMA`). Loss-term weights that followed
the searched value would let a candidate lower its loss with a larger error
scale instead of smaller errors. The same rule sets the weight of a
property: the target is the reference value, such as a DFT value
(:ref:`nfp_ta_reference`), and :math:`a` is in the unit of the property. The
terms compete: a larger weight for one lowers its error at the cost of
the others.

.. _nfp_targets_datasets:

Per-dataset targets
~~~~~~~~~~~~~~~~~~~

When the validation datasets deserve different accuracies, an index list
gives each one its own target and weight; the indices count the dataset
files of the :ref:`DBFILE <dbfile>` lines in order, from 1. Unit 3 sets a
coarser energy accuracy for the compressed crystals than for the others:

.. literalinclude:: files/tutorial/unit3/config.hpo
   :language: none
   :start-at: PC_ERMSE
   :end-at: PC_ERMSE   (3)

Every dataset then needs a target and a weight, 0 for one without the
quantity, and the term is the mean of the losses of the datasets. Under
:hpo:key:`!L2` this mean penalises one poor dataset more than the loss of
the mean error does, so a good average cannot hide it.

.. _nfp_targets_function:

Choosing the loss function
--------------------------

:hpo:key:`LOSS` sets the loss function. The default, :hpo:key:`L2`, is the
recommended choice: it is smooth, which suits the local optimisers, and it
gives the accuracy rule above. One :hpo:key:`!LOSS` line at the top of the
target file sets the function of every term; :ref:`nfp_target_file_order` gives
the rule for several.

Other functions suit terms far from their targets for most candidates: under
:hpo:key:`!L2` the squared differences would decide the mean.
:hpo:key:`HUBER`, :hpo:key:`L1` and :hpo:key:`LOG_COSH` grow linearly for
large differences. :hpo:key:`TUKEY` is bounded by the weight of its term, so a
term far off stops steering the search. :hpo:key:`MAPE` divides by the target,
for properties of different units; it refuses a target of 0. :hpo:key:`RMSLE`
compares logarithms, for positive values that span orders of magnitude.

The accuracy rule holds for :hpo:key:`!L2` only; the parameters of
:hpo:key:`!HUBER` and :hpo:key:`!TUKEY` are in the unit of the difference. A
:hpo:key:`--varloss` term and a :hpo:key:`!PC_OBSERVABLE` line can name their
own function, for example one that limits large differences of a single
property while the metrics stay under :hpo:key:`!L2`.

.. _nfp_targets_scripts:

Properties from LAMMPS
----------------------

A :hpo:key:`PC_LAMMPS` line adds properties that the validation data do
not hold, such as the cohesive energy, lattice constant, vacancy formation
energy and elastic constants of :ref:`nfp_unit_4`. Each evaluation runs
its LAMMPS script with the candidate potential and reads equal-style
variables of the script: a :hpo:key:`--varloss` adds a term, a
:hpo:key:`--outvar` only records a value. :hpo:topic:`LAMMPS_SCRIPT`
describes what a script provides, and :ref:`nfp_ref_pc_options` lists the
options of the line.

.. literalinclude:: files/tutorial/unit4/config.hpo
   :language: none
   :start-at: PC_SCRIPT_TIMEOUT
   :end-at: --varloss Ecoh

A :hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` value, here the relaxed lattice
constant, passes to the next evaluation as its start and to the other scripts
that read it. A value outside its range fails the evaluation, so the range also
checks the candidate. :hpo:key:`PC_SCRIPT_TIMEOUT` limits the wall-clock time
of each section, up to and after each ``clear``, of the scripts of the lines
after it (:ref:`nfp_target_file_order`); a section that reaches it fails the
evaluation.

LAMMPS results have no gradient: with a :hpo:key:`!PC_LAMMPS` or
:hpo:key:`PC_FAILPROBE` line, ``tadah hpo`` refuses every optimiser that takes
one (:hpo:topic:`RULE.PC_LAMMPS_gradient_free`). Unit 4 uses
:hpo:opt:`NLOPT:LN_SBPLX`, and :ref:`nfp_choosing` gives the alternatives.

.. _nfp_targets_observables:

Built-in observables
--------------------

A :hpo:key:`PC_OBSERVABLE` line adds a property that ``tadah`` computes
itself, without LAMMPS. The one kind, :hpo:key:`cohesive_energy`, is the
cohesive energy of a body-centred cubic (bcc) crystal of one element,
relaxed at zero pressure. The term has an analytical gradient, so a
gradient-based optimiser such as :hpo:opt:`DLIB:BFGS` can refine a search
with it (:ref:`nfp_example_gradient_observable`). The default analytical
gradient needs the settings that :hpo:key:`GRAD_STRATEGY` lists, such as
``STRESS false``, a fixed :ref:`LAMBDA` and no :hpo:key:`PC_SRMSE_GPA` line
(:ref:`nfp_choosing_gradients`).

:hpo:key:`!PC_OBSERVABLE` is the choice for the cohesive energy of a
one-element bcc crystal in a gradient-based search; :hpo:key:`PC_LAMMPS` covers
every other property. A target file holds :hpo:key:`!PC_OBSERVABLE` lines or
:hpo:key:`!PC_LAMMPS` and :hpo:key:`PC_FAILPROBE` lines, not both
(:hpo:topic:`RULE.PC_OBSERVABLE_excludes_PC_LAMMPS`).

.. _nfp_targets_failures:

Failed evaluations
------------------

An evaluation fails when it gives no usable loss. For example, a script
fails or reaches its time limit, a :hpo:key:`--hotvar <PC_LAMMPS.--hotvar>`
value leaves its range, a :hpo:key:`--failvar` value is above 0, or the loss is
not finite. Its loss is then :hpo:key:`FAILSCORE`, and it never becomes the
best potential; a loss at or above 0.999 times :hpo:key:`!FAILSCORE` counts
as a failure too. A value well above the losses of useful candidates is
recommended, as in :ref:`nfp_unit_3`: one that is too low turns poor but
usable candidates into failures. Unset, it is the largest floating-point
number, and the records show losses of that size.

:hpo:key:`FAILSCORE_GRADING` ``ON`` gives a failed evaluation a loss
slightly below :hpo:key:`!FAILSCORE` that grows with the size of the
failure, such as the distance of a :hpo:key:`!--hotvar <PC_LAMMPS.--hotvar>`
value from its range. An optimiser then finds a slope towards candidates
that do not fail, instead of a flat plateau; it suits a search in which
failures are frequent. A :hpo:key:`PC_FAILPROBE` script starts before the
other scripts and checks the candidate: a probe value above
:hpo:key:`--gate` fails the evaluation, and its scripts that have not
started do not run. :ref:`nfp_example_failure_grading` combines both:

.. literalinclude:: files/examples/failure_grading/config.hpo
   :language: none
   :start-at: FAILSCORE_GRADING ON
   :end-at: --s0

.. _nfp_targets_rejection:

Rejecting candidates early
~~~~~~~~~~~~~~~~~~~~~~~~~~

A :hpo:key:`--skip-above` cap on a metric line, in the unit of the metric,
rejects a candidate whose metric exceeds it: the candidate skips its
:hpo:key:`PC_LAMMPS` and :hpo:key:`PC_OBSERVABLE` terms and fails. A cap
saves the property scripts of candidates that are poor already on the
validation data. An absolute cap is the recommended form, because
:hpo:key:`--skip-rel` compares with the smallest value of the metric so
far: two evaluations of one point can then score differently.
:hpo:key:`PC_ERSQ` takes neither option.

The journal warns of a rejection with the metric, its value and its cap,
and with the property terms it skips, if any; :ref:`nfp_unit_3` shows
one. Only the first warning of each Message Passing Interface (MPI) rank
reaches the terminal, and after the fifth the journal warns at growing
intervals. ``DIAGNOSTICS`` counts every rejection
(:ref:`nfp_output_histories`).
