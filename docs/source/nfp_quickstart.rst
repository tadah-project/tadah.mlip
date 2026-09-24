.. _nfp_quick_start:

Quick Start
===========


Nested fitting needs up to three input files:

#. A :term:`training configuration` — the same configuration file you
   would use for ``tadah train``.  See :ref:`ConfigSection` for the
   full key list.
#. A *validation* file — a list of validation datasets passed via the
   ``DBFILE`` keyword.  *Optional*: only required when the loss uses
   validation-set metrics (``PC_ERMSE``, ``PC_FRMSE``, …); a run whose
   loss comes entirely from ``PC_OBSERVABLE`` or ``PC_LAMMPS`` lines can
   omit ``--validation`` (the two kinds are mutually exclusive in one
   HPO configuration).
#. An :term:`HPO configuration` — the file documented in
   :ref:`nfp_manual`.  Conventionally named ``config.hpo``.

The training configuration's ``DBFILE`` is itself *optional* when every
model weight is pinned via ``FIXINDEX``/``FIXWEIGHT`` (see
:ref:`hpo_fixweight`) — with no free weights there is nothing for the
least-squares stage to fit.  In that mode the element set must be
declared explicitly with the ``ATOM`` key, and at least one loss source
must remain.

The driving command line is

::

   tadah hpo --config <config.train> --hpotarget <config.hpo> --validation <config.val>

A minimal ``config.hpo``:

.. code-block:: none

   OPTIMIZER
     STRATEGY
       TYPE PLAIN
       LIB DLIB
       ALGO BFGS
       MAXEVAL 50
     ENDSTRATEGY
   ENDOPTIMIZER

   LOSS L2
   PC_ERMSE 0 1.0
   OPTIM RCUT2B (1) 4.0 6.0

Each ``PC_*`` line adds one term to the loss, and the loss of an
evaluation is the mean of its terms (:ref:`nested_fitting`).  Keep
``LOSS`` (and ``FAILSCORE``) above any ``PC_LAMMPS`` line: each
``PC_LAMMPS`` line takes the values in force when it is read.

See :ref:`cli_example_2` for a slightly bigger walkthrough, and
:ref:`hpo_examples` for a graded series of worked examples.
