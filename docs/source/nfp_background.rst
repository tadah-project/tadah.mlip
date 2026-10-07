.. _nested_fitting:

Background
==========


.. note::

   We use :term:`NFP` (Nested Fitting Procedure) and :term:`HPO`
   (Hyperparameter Optimization) interchangeably to refer to this feature.
   The main :term:`CLI` entry point is ``tadah hpo``.  *NFP* is preferred in
   prose to emphasise the two-level fitting structure that is specific to
   Tadah!MLIP; *HPO* is the spelling used by every keyword and source-file
   identifier.

.. figure:: images/nested_fitting.jpg
   :alt: Nested Fitting Procedure
   :width: 95%
   :align: center

   Workflow of the :term:`nested fitting procedure`: the outer optimiser
   proposes new :term:`hyperparameters<Hyperparameter>`, the model is
   retrained, and each trial potential is validated against
   :term:`performance constraints<Performance constraint>` that feed back
   into the global loss.

Nested fitting is Tadah!MLIP’s automated, two-level fitting workflow:

* **inner loop** — ordinary regression that determines the
  :term:`learned parameters<Learned Parameter>` :math:`\mathbf w` for a
  fixed model definition;
* **outer loop** — a global optimiser that samples from the
  :term:`search space constraints<Search Space Constraint>` (SSC),
  retrains the model, and judges the result against user-defined
  :term:`performance constraints<Performance constraint>` (PC).

Letting a computer search the :term:`hyperparameter` space helps to

* escape the “good on the validation set, unstable in MD” trap;
* trade accuracy for speed (or vice versa) in a reproducible way;
* fold real-world performance constraints — elastic constants, phase
  stability, surface energies, … — directly into the loss function.

A traditional MLIP workflow stops once regression converges on a
training/validation split.  Unfortunately, a potential that interpolates
perfectly may still

* disintegrate during an MD run,
* produce an incorrect equation of state,
* predict the wrong ground-state crystal,
* extrapolate poorly beyond the training set.

Manual hyperparameter tuning or simply enlarging the training set are
both tedious and not guaranteed to succeed.

Nested fitting tackles the problem by *measuring* emergent properties
during the fit.  Each trial potential is validated on held-out data and,
optionally, dropped into short LAMMPS runs; the resulting quantities are
compared to their targets and every discrepancy becomes one *term* of a
**global loss**.

The global loss
---------------

A **loss term** is one of

* each enabled validation metric (``PC_EMAE``, ``PC_ERMSE``, ``PC_ErRMSE``,
  ``PC_ERSQ``, ``PC_FRMSE``, ``PC_SRMSE``) — one term per metric, however
  many validation datasets it covers;
* each ``--varloss`` variable of each ``PC_LAMMPS`` line;
* each ``PC_OBSERVABLE`` line.

The loss of one evaluation is the **mean** of its terms,

.. math::

  L(\theta) = \min\Bigl(\mathrm{FAILSCORE},\;
               \frac{1}{N_\text{terms}}\sum_{i=1}^{N_\text{terms}}
               w_i\,\mathcal L_i\bigl(\hat y_i(\theta),\,y_i\bigr)\Bigr),

where :math:`w_i` is the user-supplied weight, :math:`y_i` the target,
:math:`\hat y_i` the measured value and :math:`\mathcal L_i` one of the
built-in loss functions (``L1``, ``L2``, ``HUBER``, ``TUKEY``,
``LOG_COSH``, ``MAPE``, ``RMSLE``; see :ref:`loss_functions`).  A term
with weight ``0`` contributes nothing to the sum but still counts in
:math:`N_\text{terms}`, and weights are not normalised by their sum.

A term that cannot be measured — a LAMMPS run that crashed, a variable
that could not be read, an observable whose relaxation diverged — does
not silently disappear: it fails the whole evaluation, whose loss is
``FAILSCORE`` whatever its other terms (the term keeps a failure value in
``loss.tadah``), or, with ``FAILSCORE_GRADING ON``, a *graded* failure
loss just below ``FAILSCORE`` that tells the optimiser which way is out
(:ref:`failscore_grading`).  Before 1.6.0 the failure value of an
observable, and under ``FAILSCORE_GRADING OFF`` that of a LAMMPS run or
variable, was averaged with the other terms (under ``ON`` a LAMMPS
failure was graded), and the mean could fall below ``0.999 ×
FAILSCORE``: a usable-looking loss.  A non-finite total is also clipped to
``FAILSCORE``.

The two loops
-------------

**Inner fit.**  For a given hyperparameter vector :math:`\theta` the
model is retrained with the configured regression, a least-squares
solve over its *free* linear weights.  Weights pinned with ``FIXINDEX``/``FIXWEIGHT``
are excluded from that solve.  When every weight is pinned — or there is
no training ``DBFILE`` — there is no least-squares solve at all, and the
outer optimiser searches the pinned coefficients directly through
``OPTIM FIXWEIGHT`` (:ref:`hpo_fixweight`).

**Outer optimiser.**  The outer loop minimises :math:`L(\theta)` over
the box declared with the ``OPTIM`` directive:

.. math::

  \theta^\star
  = \operatorname*{arg\,min}_{\theta\in\Theta} L(\theta).

Any backend can drive it — local or global, gradient-free or
gradient-based (:doc:`optimizer`).  A gradient-based backend takes
finite differences of :math:`L(\theta)` by default; with
``PC_OBSERVABLE`` it uses the analytical gradient
(``GRAD_STRATEGY``, :ref:`pc_observable`).  The backend is
wrapped by a ``STRATEGY``: ``PLAIN`` (one run), ``MULTISTART`` (one run
per starting point) or ``PRESTAGE`` (an exploration phase followed by a
refinement of its best candidates), optionally decorated by ``RESTART``
(fresh starts after stagnation).  See :ref:`strategy_block`.

One evaluation, step by step
----------------------------

For every candidate :math:`\theta` the host

#. under ``RHOFORM NONNEG``/``MONOTONE``, **projects** every negative
   ``AMPGRIDMB`` coordinate onto 0, and evaluates and records that point
   (:ref:`hpo_rhoform_positive`);
#. **checks the bump-support gate** (``BUMPGATE CANDIDATE``, the
   default): a candidate whose hard-cutoff bumps would reach past their
   cutoff is rejected with a failure loss before any training work;
#. **injects the values**: a copy of the training configuration is made
   with every ``OPTIM``'d key rewritten to the candidate values (the
   other indices of a key keep their configured values); for knot-family
   refit potentials the ``RCUT`` keys are re-derived from the current knot
   positions (:ref:`hpo_knot_rcut`).  The model of the evaluation (its
   descriptors, basis function or kernel, design matrix and regressor) is
   built from that copy, so every key it reads, searched or not, is the
   candidate's, and nothing an earlier evaluation or an earlier run
   computed is carried over (building it costs far less than the fit; a
   ``SETFL`` file is parsed once while its contents are unchanged);
#. **checks the density gates** (``RHOGATE``, ``RHOBARGATE``, when
   configured) on that configuration: a DM_REAM density that fails them is
   rejected with a failure loss before any training work
   (:ref:`hpo_feasibility_gates`);
#. **trains** the model (the inner fit above);
#. **computes the validation metrics** and their loss terms;
#. **applies the skip caps** (``--skip-above`` / ``--skip-rel``): an
   obviously bad potential is fail-scored and nothing downstream runs;
#. **evaluates the typed observables** (``PC_OBSERVABLE``);
#. **runs the LAMMPS constraints** (``PC_LAMMPS``) — only if the loss so
   far is still below ``FAILSCORE``.  Scripts declared *hot*
   (``--hotvar``) run in a first phase together with the independent
   scripts; their results (e.g. a relaxed lattice constant) are then
   injected into the *cold* scripts of the second phase;
#. **records the result**: the loss is the mean above; a new best is
   written to ``best_pot.tadah`` and the history files.

The optimiser vector is ordered by ``OPTIM`` key (alphabetically) and
then by index within the key (``FIXWEIGHT``: by ``FIXINDEX`` slot),
independent of the order of the ``OPTIM`` lines; ``params.tadah``,
``best_params.tadah`` and ``hpo_search_space.tsv`` use the same order
(:ref:`output_files`, :ref:`dimension order <hpo_dimension_order>`).

Because the model is retrained for every candidate :math:`\theta`, the
procedure is computationally expensive but also powerful: it can expose
regions of hyperparameter space that yield stable, accurate, and fast
potentials.  It is not a silver bullet — **success still depends on
sensible choices of performance constraints and search space limits.**
