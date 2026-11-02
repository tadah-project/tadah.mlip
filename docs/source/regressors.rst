.. _regressors:

Regressors
==========

This page shows how the :ref:`MODEL` key of a training configuration (see
:ref:`ConfigSection`) selects a regressor and its kernel or basis function,
and which further keys each one reads.

Overview
--------

The regressors, kernels and basis functions listed on this page are selected
by their exact names in the ``MODEL`` key. The names are case-sensitive
(``MODEL m_krr Kern_RBF`` is refused):

.. code-block:: ini

   MODEL  M_KRR  Kern_RBF     # pick Kernel Ridge Regression
   MODEL  M_BLR  BF_Polynomial2   # pick Bayesian Linear Regression

Quick syntax guide
------------------

.. code-block:: ini

   MODEL  <Regressor>  <Kernel | BasisF>  [<implementation>]

* ``<Regressor>`` – one of the names under **Regressors** below
  (``M_KRR``, ``M_BLR`` …).
* ``<Kernel>`` – required for kernel methods (one of the *Kernels*
  subsection).
* ``<BasisF>`` – required for linear / basis-function methods (one of
  the *Basis Functions* subsection).
* ``<implementation>`` – optional third token of ``M_KRR`` (described
  below).
* Further keys such as :ref:`MPARAMS` and :ref:`SBASIS` configure the
  model and are listed in each regressor’s table.

For ``M_KRR`` a third token selects the implementation: ``1`` (the
default) fits energies, forces and stresses by least squares over the
design matrix; ``2`` fits energies only, solving
:math:`(K + \lambda s^2 I) w = t` on the basis vectors (``SBASIS`` of them,
drawn from the training structures, for every kernel), :math:`s` the energy
:term:`error scale` :ref:`ESIGMA` in eV: :ref:`LAMBDA` weighs against the energies
per atom over ``ESIGMA``, as in implementation ``1``
(:ref:`fit_objective`). With ``Kern_Linear``
the model's coefficients on the descriptor components are
:math:`\sum_j w_j \mathbf{b}_j`, the same model, since the kernel is linear:
its potential has ``DSIZE`` ``WEIGHTS`` and no ``BASIS``, as with
implementation ``1`` (with ``NORM true`` they are converted to the raw
descriptors, as implementation ``1``'s are). So implementation ``2`` takes
``SBASIS`` with every kernel, ``Kern_Linear`` too, with the bound of the
basis below.

.. versionchanged:: 1.6.0
   ``MODEL M_KRR Kern_Linear 2`` wrote the coefficients :math:`w` of its
   basis vectors as its ``WEIGHTS``, which ``tadah predict`` and ``pair_style
   tadah`` evaluated as descriptor coefficients, so its energies were wrong (the
   check of a potential's fit refuses such a potential unless ``SBASIS``
   equals ``DSIZE``). Such potentials need retraining.

Examples
........

Kernel ridge regression with an RBF kernel:

.. code-block:: ini

   MODEL     M_KRR  Kern_RBF  1
   MPARAMS   1.0        # sigma: K(b, x) = exp(-|x - b|^2 / (2 sigma^2))
   SBASIS    100        # basis vectors: e0 and 99 training descriptors

Bayesian linear regression with the second order polynomial basis function:

.. code-block:: ini

   MODEL     M_BLR  BF_Polynomial2

.. _nonlinear_models:

Non-linear models
-----------------

``M_KRR`` with ``Kern_RBF``, ``Kern_Polynomial``, ``Kern_Quadratic``,
``Kern_LQ`` or ``Kern_Sigmoid``, and ``M_BLR`` with ``BF_Polynomial2``, are
non-linear in the descriptor. They are trained, predicted, optimised by
``tadah hpo`` and run in LAMMPS like the linear ones (as **LAMMPS**
below describes).

**Kernel parameters** (``MPARAMS``): ``Kern_RBF`` takes
:math:`\sigma`, :math:`K(\mathbf{b},\mathbf{x}) = \exp(-|\mathbf{x}-\mathbf{b}|^2/(2\sigma^2))`
(:math:`\gamma = 1/\sigma^2`); ``Kern_Polynomial`` takes ``d gamma c``,
:math:`(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)^d`; ``Kern_Sigmoid`` takes
``gamma c``, :math:`\tanh(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)`.
``Kern_Quadratic`` (:math:`(\mathbf{b}\cdot\mathbf{x})^2`) and ``Kern_LQ``
(:math:`(\mathbf{b}\cdot\mathbf{x})^2 + \mathbf{b}\cdot\mathbf{x}`) take
none, nor do ``Kern_Linear``, ``BF_Linear`` and ``BF_Polynomial2``. The
degree ``d`` is an integer: ``2``, ``2.0`` or ``2e0`` is read as 2, and a
value that is not an integer (``2.7``) is refused naming ``MPARAMS (1)``.
``Kern_RBF`` refuses :math:`\sigma = 0`; a negative
:math:`\sigma` is the kernel of :math:`|\sigma|`.

**The basis of a kernel** (``SBASIS``, required). The atomic energy is
:math:`E_i = \sum_j w_j K(\mathbf{b}_j, \mathbf{x}_i)` over ``SBASIS``
basis vectors, one coefficient each. The first :math:`n` are the intercept
("bias") vectors :math:`e_0, \dots, e_{n-1}` (:math:`e_k` is 1 in component
:math:`k`, 0 elsewhere): with ``BIAS true`` one per ``ATOM`` species, the
species' intercept columns of the descriptor; with ``BIAS false``
:math:`n = 1`, the bias vector :math:`e_0 = (1, 0, \dots, 0)`. The other
``SBASIS-n`` are atomic energy descriptors of the training set (with
``NORM true``, standardised), drawn by a fixed draw: the first
``SBASIS-n`` steps of a Fisher-Yates shuffle of the descriptors, in the
order of the training set, driven by ``std::mt19937_64`` with its default
seed 5489. The same training set and ``SBASIS`` give the same basis on
every run and platform, so ``tadah train`` writes the same potential every
time and the loss of ``tadah hpo`` at a given point does not depend on the
draw. ``SBASIS`` must be at least :math:`n + 1` (2 with one species or
``BIAS false``) and at most the number of training atoms plus :math:`n`.
(Implementation ``2`` draws structures instead, takes the mean descriptor
of each, and allows at most the number of training structures plus
:math:`n`.) A missing ``SBASIS``, or one below :math:`n + 1`, is refused
naming the model (``M_KRR Kern_RBF 1: SBASIS 1: the fit needs at least 2
basis vectors ...``): by ``tadah train`` before it reads the training data
(with ``BIAS true`` and no ``ATOM``, once ``ATOM`` is set from the
datasets) and by ``tadah hpo`` at startup. The potential records
``SBASIS`` and the basis vectors (``BASIS``) with the coefficients; ``tadah
predict``, ``tadah hpo`` and ``pair_style tadah`` refuse, naming the keys,
a potential without them, with a number of coefficients other than ``SBASIS``,
or with basis vectors of another length than the descriptor's. Training
draws the basis anew and reads neither ``WEIGHTS`` nor ``BASIS``, so a
potential retrains from its own file with any ``SBASIS`` (``tadah train -c
pot.tadah`` with a ``DBFILE``).

**Intercepts and the ridge.** ``LAMBDA`` penalises every coefficient but those
of the model's intercept columns, the columns that depend on an atom only
through its species (also when ``FIXINDEX`` pins coefficients). With ``BIAS
true`` (and ``ATOM``, which ``BIAS`` requires; :math:`N` species, the
descriptor's intercept components :math:`0 \dots N-1`):

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Intercept columns
   * - ``BF_Linear``, ``Kern_Linear``
     - the bias columns of the descriptor
   * - ``BF_Polynomial2``
     - the product :math:`(k,k)` of each intercept component with itself,
       one per species (column 0 with one species; columns 0 and
       :math:`D`, :math:`D` the number of descriptor components, with two)
   * - ``Kern_Quadratic``, ``Kern_LQ``, ``Kern_Polynomial``,
       ``Kern_Sigmoid``
     - columns 0 to :math:`N-1`, :math:`K(e_k, \mathbf{x})`, which depends
       on :math:`\mathbf{x}` only through intercept component :math:`k`
   * - ``Kern_RBF``
     - none

With ``BIAS false`` no function has one. ``OALGO 3`` penalises every
column alike, so with ``LAMBDA != 0`` it refuses a model with an intercept
column (``Kern_RBF`` with ``BIAS true`` trains).

**Conditioning.** The fit is a least-squares problem in ``SBASIS`` (or
:math:`D(D+1)/2`) columns. Basis vectors close to each other, or a kernel
width far from the spread of the descriptors, make the columns nearly
collinear, and ``OALGO 1`` then drops the small singular values (its
rank threshold, the second value of :ref:`OALGO`). The recommended ``MPARAMS`` match the scale of the descriptors;
``NORM true`` standardises the descriptor components first.

**LAMMPS.** ``pair_style tadah`` evaluates a non-linear potential as
``tadah predict`` does, also with ``BIAS true`` on several species: the
descriptor has one intercept component per species in training and in
``pair_style tadah`` alike (:ref:`bias_term`).
``pair_style tadah`` also refuses a ``BF_Polynomial2`` potential without
:math:`D(D+1)/2` coefficients for its :math:`D` descriptor components, and a
potential of any non-linear model whose ``NMEAN``/``NSTDEV`` (``NORM
true``) or basis vectors do not fit the descriptor.

.. versionchanged:: 1.6.0
   A potential trained by 1.5.x or earlier with ``BIAS true`` on several
   species, with one intercept component shared by the species, is refused
   with the instruction to retrain it.

**FIXINDEX/FIXWEIGHT** with a kernel pin coefficients of the basis vectors
(coefficients 1 to :math:`n` belong to the intercept vectors, the others to
vectors drawn from the training data): a training ``DBFILE`` is required,
also when every coefficient is pinned. ``M_KRR`` implementation ``2``
(``MODEL M_KRR <kernel> 2``) pins no coefficient and refuses both keys.

**Searching MPARAMS.** A kernel reads ``MPARAMS`` when it is built, and
``tadah hpo`` builds the model of every evaluation from that evaluation's
candidate, so ``OPTIM MPARAMS`` searches the kernel's parameters. At
startup ``tadah hpo`` refuses ``OPTIM MPARAMS (i)`` on a function that
reads fewer than *i* values (``Kern_Linear``, ``Kern_Quadratic``,
``Kern_LQ``, ``BF_Linear`` and ``BF_Polynomial2`` read none), on the
degree of ``Kern_Polynomial`` (an integer) and on the :math:`\sigma` of
``Kern_RBF`` with a box that is not positive.

.. versionchanged:: 1.6.0
   A search over ``OPTIM MPARAMS`` trains every evaluation with its own
   candidate. In 1.5.0 every evaluation after a worker's first trained with
   that first ``MPARAMS`` while ``best_pot.tadah`` recorded the candidate's
   value, so such a search needs re-running.

Reading the autogenerated tables
--------------------------------

Each entry below gives:

* a short description and the mathematical form;
* the expected kernel or basis function;
* the keys it reads (a kernel's ``MPARAMS`` line, the bold key names of a
  regressor).

The ``MODEL`` value names the regressor and its kernel or basis function.
Each key an entry names takes exactly the number of values that its entry
in :ref:`ConfigSection` gives.

The remainder of this page lists every available regressor, followed by
supported kernels and basis functions.

.. rubric:: Regressors

M_KRR
-----

.. doxygenclass:: tadah::mlip::M_KRR

Kernels
.......

Kern_Linear
~~~~~~~~~~~

.. doxygenclass:: tadah::models::Kern_Linear

Kern_LQ
~~~~~~~

.. doxygenclass:: tadah::models::Kern_LQ

Kern_Polynomial
~~~~~~~~~~~~~~~

.. doxygenclass:: tadah::models::Kern_Polynomial

Kern_Quadratic
~~~~~~~~~~~~~~

.. doxygenclass:: tadah::models::Kern_Quadratic

Kern_RBF
~~~~~~~~

.. doxygenclass:: tadah::models::Kern_RBF

Kern_Sigmoid
~~~~~~~~~~~~

.. doxygenclass:: tadah::models::Kern_Sigmoid

M_BLR
-----

.. doxygenclass:: tadah::mlip::M_BLR

Basis Functions
...............

BF_Linear
~~~~~~~~~

.. doxygenstruct:: tadah::models::BF_Linear

BF_Polynomial2
~~~~~~~~~~~~~~

.. doxygenstruct:: tadah::models::BF_Polynomial2
