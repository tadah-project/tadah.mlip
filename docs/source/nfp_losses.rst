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
target and :math:`r=y-t`. The table gives each loss before multiplication
by its weight; :math:`\epsilon=10^{-12}`.

.. list-table:: Available losses
   :header-rows: 1
   :widths: 17 20 63

   * - Name
     - Parameter
     - Loss
   * - ``L1``
     - None
     - :math:`|r|`
   * - ``L2``
     - None
     - :math:`r^2`
   * - ``HUBER``
     - :math:`\delta`; default ``50``
     - :math:`r^2/2` when :math:`|r|\leq\delta`, otherwise
       :math:`\delta(|r|-\delta/2)`.
   * - ``TUKEY``
     - :math:`c`; default ``4.685``
     - :math:`1-(1-(r/c)^2)^3` when :math:`|r|\leq c`, otherwise ``1``.
   * - ``LOG_COSH``
     - None
     - :math:`\log\cosh r`, evaluated in a numerically stable form.
   * - ``MAPE``
     - None
     - :math:`|r|/(|y|+\epsilon)`. The denominator uses the measured
       value; there is no multiplication by 100.
   * - ``RMSLE``
     - None
     - :math:`[\log(y+\epsilon)-\log(t+\epsilon)]^2`.
       Both logarithm arguments must be positive.

An explicit ``HUBER`` or ``TUKEY`` parameter must be finite and positive.
Other losses accept no parameter. ``RMSLE`` is a squared logarithmic
loss here: its name does not imply an additional square root.

Where the setting applies
-------------------------

* :doc:`Validation metrics <nfp_validation_targets>` use the root loss.
* Each :doc:`PC_LAMMPS <nfp_lammps_targets>` line captures the root loss
  in force when that line is read. Its ``--varloss`` options can override
  the loss separately for each variable.
* Each :doc:`PC_OBSERVABLE <nfp_observable_targets>` line has its own
  ``--loss`` option, defaulting to ``L2`` independently of root ``LOSS``.

Put ``LOSS`` before the ``PC_LAMMPS`` lines it should affect. For a
successful evaluation, HPO averages the enabled weighted loss terms; it
does not divide by the sum of weights. Each enabled validation metric
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

This root setting differs from ``PC_LAMMPS --failscore``, which supplies
replacement *variable values* for a failed script. The latter does not
make a failed evaluation usable. Put root ``FAILSCORE`` before script
lines so their default replacement values use it.

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

LAMMPS ``--failvar`` and :doc:`PC_FAILPROBE <nfp_lammps_targets>` supply
violation measures. :doc:`Feasibility gates <nfp_feasibility>` can
reject candidates before training.
