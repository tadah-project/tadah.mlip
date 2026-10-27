.. _loss_functions:

.. _loss-global-loss-function:

.. _loss-options:

.. _failscore_grading:

.. _failure-handling-graded-failscore:

Loss functions and failure scores
=================================

Use ``LOSS`` at the root of the :doc:`target file <nfp_configuration>`
to choose how measured values are compared with their targets.

.. code-block:: none

   LOSS HUBER 10
   FAILSCORE 1e9
   FAILSCORE_GRADING OFF

LOSS
----

Syntax: ``LOSS <name> [<parameter>]``. The default is ``L2``. Loss names
are case-insensitive. Let :math:`y` be the measured value, :math:`t` the
target and :math:`r=y-t`. The definitions give each loss before multiplication
by its weight; :math:`\epsilon=10^{-12}`.

``L1``
   :math:`|r|`.

``L2``
   :math:`r^2`.

``HUBER [delta]``
   :math:`r^2/2` when :math:`|r|\leq\delta`, otherwise
   :math:`\delta(|r|-\delta/2)`. The default :math:`\delta` is ``50``.

``TUKEY [c]``
   :math:`1-(1-(r/c)^2)^3` when :math:`|r|\leq c`, otherwise ``1``.
   The default :math:`c` is ``4.685``.

``LOG_COSH``
   :math:`\log\cosh r`, evaluated in a numerically stable form.

``MAPE``
   :math:`|r|/|t|`. A fractional loss relative to the target.

``RMSLE``
   :math:`[\log(y+\epsilon)-\log(t+\epsilon)]^2`.
   A measured value at or below :math:`-\epsilon` fails the evaluation.

An explicit ``HUBER`` or ``TUKEY`` parameter must be finite and positive.
Other losses accept the name alone. ``RMSLE`` is a squared logarithmic
loss. A target that its loss cannot measure is refused at its line: a
target of 0 under ``MAPE``, and one at or below :math:`-\epsilon` under
``RMSLE``.

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

For a successful evaluation, HPO divides the sum of enabled weighted loss
terms by the number of terms. Each enabled validation metric
contributes one term, each LAMMPS ``--varloss`` one term, and each typed
observable one term. Indexed validation targets first form their own
dataset average.

FAILSCORE
---------

Syntax: ``FAILSCORE <value>``. The value must be finite and positive;
the default is the largest representable double.

An evaluation that fails, or whose total loss is non-finite or exceeds
this ceiling, receives ``FAILSCORE``. With grading enabled, classified
failures instead receive a score in the failure band below it. Choose
a ceiling above the losses of useful candidates: HPO treats scores at
or above ``0.999 * FAILSCORE`` as failures when selecting usable results.

:hpo:opt:`DLIB:GFS`, :hpo:opt:`DLIB:BFGS` and the NLopt optimisers are
given a finite stand-in for a failed evaluation, so a ceiling as large as
the default does not break their models of the loss. The entry of
:hpo:key:`FAILSCORE <ROOT.FAILSCORE>` gives the stand-in. A failed point
whose gradient fails is given none: :doc:`nfp_gradients` describes such
a point. ``loss.tadah`` and the journal record the loss the evaluation
reported, not the stand-in.

.. versionchanged:: 1.6.0
   1.5.0 gave NLopt, :hpo:opt:`!DLIB:GFS` and :hpo:opt:`!DLIB:BFGS`
   :hpo:key:`!FAILSCORE <ROOT.FAILSCORE>` itself for a failed evaluation.

The top-level :hpo:key:`!FAILSCORE` differs from
:hpo:key:`PC_LAMMPS --failscore <PC_LAMMPS.--failscore>`, which supplies
replacement *variable values* for a failed script. The evaluation retains
its failed status. A :hpo:key:`PC_LAMMPS` or :hpo:key:`PC_FAILPROBE` line
without ``--failscore`` reads the :hpo:key:`!FAILSCORE` above it as its
replacement value; ``--failscore`` gives it its own. A second
:hpo:key:`!FAILSCORE` line is refused unless such a line between reads
the first, and the ceiling of every loss is the last.

.. _nfp_failscore_grading:

FAILSCORE_GRADING
-----------------

Syntax: ``FAILSCORE_GRADING ON|OFF``; default ``OFF``. ``ON`` assigns
classified failures a score in ``[0.999 * FAILSCORE, FAILSCORE)`` using
their failure category and violation measure. These remain failed
evaluations, even when the score is finite and below the ceiling.

Grading requires ``FAILSCORE < 1e300`` and a value large enough that all
four grading bands are representable. The default largest-double
ceiling is therefore unsuitable; set an explicit value such as ``1e9``.
The environment variable ``TADAH_FAILSCORE_GRADING`` accepts ``ON`` or
``OFF`` and overrides the file.

LAMMPS :hpo:key:`--failvar` and :hpo:key:`PC_FAILPROBE` supply
violation measures. In both modes a failure signal fails the evaluation:
a ``--failvar`` value above 0, a probe value above :hpo:key:`--gate`,
and a probe failure (its script fails, or its probe value cannot be read,
or is negative, not finite or at or above its ``--failscore``). The
evaluation scores :hpo:key:`!FAILSCORE` under ``OFF`` and a graded score
under ``ON``, and a warning in ``hpo_run.log`` names the signal. The
settings that only grade a failure, :hpo:key:`--s0` and an explicit
``--failvar`` scale, are refused under ``OFF``.
:doc:`Feasibility gates <nfp_feasibility>` can reject candidates before
training.

.. versionchanged:: 1.6.0
   Under ``FAILSCORE_GRADING OFF``, 1.5.0 ignored a ``--failvar`` value
   above 0, a probe value above ``--gate`` and a probe value that is
   negative, not finite or at or above ``--failscore``.
