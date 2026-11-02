.. _loss_functions:

Loss Functions and Failure Scores
=================================

This page describes how the loss compares measured values with their
targets and how a failed evaluation is scored. It assumes a
:doc:`target file <nfp_configuration>`.

The top-level key :hpo:key:`LOSS` sets how measured values are compared
with their targets. The top-level keys :hpo:key:`FAILSCORE` and
:hpo:key:`FAILSCORE_GRADING` set the score of a failed evaluation.

.. code-block:: none

   LOSS HUBER 10
   FAILSCORE 1e9
   FAILSCORE_GRADING OFF

.. _loss-global-loss-function:

LOSS
----

The loss functions are :hpo:key:`L1 <LOSS.L1>`,
:hpo:key:`L2 <LOSS.L2>`, :hpo:key:`HUBER <LOSS.HUBER>`,
:hpo:key:`TUKEY <LOSS.TUKEY>`, :hpo:key:`LOG_COSH <LOSS.LOG_COSH>`,
:hpo:key:`MAPE <LOSS.MAPE>` and :hpo:key:`RMSLE <LOSS.RMSLE>`. Each
entry defines its function, its parameter and its default, and the entry
of :hpo:key:`!LOSS` gives the syntax and the default function. Loss names
are case-insensitive. A term multiplies its loss by its loss-term weight.

.. _loss-options:

An explicit ``HUBER`` or ``TUKEY`` parameter must be finite and positive.
Other losses accept the name alone. ``LOG_COSH`` is evaluated in a
numerically stable form. A target that its loss cannot measure is refused
at its line: a target of 0 under ``MAPE``, and one at or below -1e-12
under ``RMSLE``. A measured value at or below -1e-12 under ``RMSLE``
fails the evaluation.

.. versionchanged:: 1.6.0
   ``MAPE`` divides by the target; 1.5.0 divided by the measured value.

.. _where-the-setting-applies:

Loss scope
----------

* :doc:`Validation metrics <nfp_validation_targets>` use the loss
  function of the last :hpo:key:`LOSS` line.
* Each :doc:`PC_LAMMPS <nfp_lammps_targets>` line captures the top-level
  :hpo:key:`!LOSS` in force when that line is read. Its ``--varloss``
  options can override the loss separately for each variable.
* Each :doc:`PC_OBSERVABLE <nfp_observable_targets>` line captures the
  top-level :hpo:key:`!LOSS` in force when that line is read. Its
  ``--loss`` option overrides it for that term.

:hpo:key:`!LOSS` precedes the terms it affects: a :hpo:key:`--varloss`
term or a :hpo:key:`PC_OBSERVABLE` line that names no loss function
reads it. A second :hpo:key:`!LOSS` line is refused unless such a term
between reads the first. In a target file without validation metrics, a
:hpo:key:`!LOSS` line that no such term follows is refused too.

For a successful evaluation, ``tadah hpo`` divides the sum of the enabled
weighted loss terms by the number of terms. Each enabled validation
metric contributes one term, each LAMMPS ``--varloss`` one term, and each
:hpo:key:`!PC_OBSERVABLE` line one term. The datasets of an indexed
validation metric first form their mean loss, which is one term (see
"Dataset-specific targets" in :doc:`nfp_validation_targets`).

.. _failure-handling-graded-failscore:

FAILSCORE
---------

The syntax, range and default of :hpo:key:`FAILSCORE <ROOT.FAILSCORE>`
are in its reference entry.

An evaluation that fails, or whose total loss is non-finite or exceeds
this ceiling, receives :hpo:key:`!FAILSCORE`. With grading enabled,
classified failures instead receive a score in the failure band below it.
The recommended ceiling lies above the losses of useful candidates:
``tadah hpo`` treats scores at or above ``0.999 * FAILSCORE`` as failures
when it selects usable results.

:hpo:opt:`DLIB:GFS`, :hpo:opt:`DLIB:BFGS` and the NLopt optimisers are
given a finite stand-in for a failed evaluation, so a ceiling as large as
the default does not break their models of the loss.
:hpo:opt:`CERES:STEEPEST_DESCENT` is given a failed evaluation's loss
scaled to at most ``1e100``, so such a ceiling does not overflow its line
search. The entry of :hpo:key:`!FAILSCORE <ROOT.FAILSCORE>` gives the
stand-in and the scale. A failed point whose gradient fails is given
none: :doc:`nfp_gradients` describes such a point. ``loss.tadah`` and the
journal record the loss the evaluation reported, not the value the
optimiser was given.

.. versionchanged:: 1.6.0
   1.5.0 gave NLopt, :hpo:opt:`!DLIB:GFS` and :hpo:opt:`!DLIB:BFGS`, and
   under a :hpo:key:`!FAILSCORE <ROOT.FAILSCORE>` above ``1e100``
   :hpo:opt:`!CERES:STEEPEST_DESCENT`, the loss of a failed evaluation as
   the evaluation reported it.

The top-level :hpo:key:`!FAILSCORE` differs from
:hpo:key:`PC_LAMMPS --failscore <PC_LAMMPS.--failscore>`, which supplies
replacement *variable values* for a failed script. The evaluation retains
its failed status. A :hpo:key:`PC_LAMMPS` or :hpo:key:`PC_FAILPROBE` line
without ``--failscore`` reads the :hpo:key:`!FAILSCORE` above it as its
replacement value; :hpo:key:`!--failscore <PC_LAMMPS.--failscore>` gives
it its own. A second
:hpo:key:`!FAILSCORE` line is refused unless such a line between reads
the first, and the ceiling of every loss is the last.

.. _nfp_failscore_grading:

FAILSCORE_GRADING
-----------------

.. _failscore_grading:

With :hpo:key:`FAILSCORE_GRADING <ROOT.FAILSCORE_GRADING>` ``ON``,
classified failures receive a score in ``[0.999 * FAILSCORE, FAILSCORE)``
using their failure category and violation measure. These remain failed
evaluations, even when the score is finite and below the ceiling.

Grading takes any :hpo:key:`FAILSCORE <ROOT.FAILSCORE>`, the default
largest double too, except with :hpo:opt:`ASA:ASA`: the entry of
:hpo:key:`!FAILSCORE_GRADING <ROOT.FAILSCORE_GRADING>` gives its limit,
about ``1e18``. The environment variable
:hpo:key:`TADAH_FAILSCORE_GRADING <ENV.TADAH_FAILSCORE_GRADING>` accepts
``ON`` or ``OFF`` and overrides the file.

LAMMPS :hpo:key:`--failvar` and :hpo:key:`PC_FAILPROBE` supply
violation measures. In both modes a failure signal fails the evaluation.
The signals are a ``--failvar`` value above 0, a probe value above
:hpo:key:`--gate`, and a failed probe. A probe fails when its script
fails, or when its probe value cannot be read, is negative, is not
finite, or is at or above its ``--failscore``.

The evaluation scores :hpo:key:`!FAILSCORE` under ``OFF`` and a graded
score under ``ON``, and a warning in ``hpo_run.log`` names the signal.
The settings that only grade a failure, :hpo:key:`--s0` and an explicit
``--failvar`` scale, are refused under ``OFF``.
:doc:`Feasibility checks <nfp_feasibility>` can reject a candidate before
training.

.. versionchanged:: 1.6.0
   1.5.0 refused a :hpo:key:`!FAILSCORE <ROOT.FAILSCORE>` of ``1e300`` or
   more under ``ON``, and its :hpo:opt:`!ASA:ASA` search took graded
   failures from about ``1e18`` for invalid points. Under ``OFF`` it
   ignored a ``--failvar`` value above 0, a probe value above ``--gate``
   and a probe value that is negative, not finite or at or above
   ``--failscore``.
