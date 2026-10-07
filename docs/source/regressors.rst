.. _regressors:

Regressors
==========

Overview
--------

The models listed in this section map to C++ classes **and** the exact
keywords you use in a configuration or task file.  The match is
literal but case-insensitive:

.. code-block:: ini

   MODEL  M_KRR  Kern_RBF     # pick Kernel Ridge Regression
   MODEL  M_BLR  BF_Polynomial2   # pick Bayesian Linear Regression

Quick syntax guide
------------------

.. code-block:: ini

   MODEL  <Regressor>  [<Kernel> | <BasisF>]

* ``<Regressor>`` – one of the names under **Regressors** below  
  (``M_KRR``, ``M_BLR`` …).
* ``<Kernel>`` – required for kernel methods (choose from the *Kernels*
  subsection).  
* ``<BasisF>`` – required for linear / basis-function methods (choose from
  the *Basis Functions* subsection).  
* Follow-up keys such as ``MPARAMS``, ``SBASIS`` configure the
  model and are listed in each class’s table.

For ``M_KRR`` a third token selects the implementation: ``1`` (the
default) fits energies, forces and stresses by least squares over the
design matrix; ``2`` fits energies only, solving
:math:`(K + \lambda I) w = t` on the basis vectors.

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
``tadah hpo`` and run in LAMMPS like the linear ones (see **LAMMPS**
below).

**Kernel parameters** (``MPARAMS``): ``Kern_RBF`` takes
:math:`\sigma`, :math:`K(\mathbf{b},\mathbf{x}) = \exp(-|\mathbf{x}-\mathbf{b}|^2/2\sigma^2)`
(:math:`\gamma = 1/\sigma^2`); ``Kern_Polynomial`` takes ``d gamma c``,
:math:`(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)^d`; ``Kern_Sigmoid`` takes
``gamma c``, :math:`\tanh(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)`.
``Kern_Quadratic`` (:math:`(\mathbf{b}\cdot\mathbf{x})^2`) and ``Kern_LQ``
(:math:`(\mathbf{b}\cdot\mathbf{x})^2 + \mathbf{b}\cdot\mathbf{x}`) take
none, nor do ``Kern_Linear``, ``BF_Linear`` and ``BF_Polynomial2``. The
degree ``d`` is an integer: ``2``, ``2.0`` or ``2e0`` is read as 2, and a
value that is not an integer (``2.7``) is refused naming ``MPARAMS (1)``
(it was read as 2). ``Kern_RBF`` refuses :math:`\sigma = 0`; a negative
:math:`\sigma` is the kernel of :math:`|\sigma|`.

**The basis of a kernel** (``SBASIS``, required). The atomic energy is
:math:`E_i = \sum_j w_j K(\mathbf{b}_j, \mathbf{x}_i)` over ``SBASIS``
basis vectors, one weight each. The first :math:`n` are the intercept
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
:math:`n`.) The potential records
``SBASIS`` and the basis vectors (``BASIS``) with the weights; ``tadah
predict``, ``tadah hpo`` and ``pair_style tadah`` refuse, naming the keys,
a potential without them, with a number of weights other than ``SBASIS``,
or with basis vectors of another length than the descriptor's. Training
draws the basis anew and reads neither ``WEIGHTS`` nor ``BASIS``, so a
potential retrains from its own file with any ``SBASIS`` (``tadah train -c
pot.tadah`` with a ``DBFILE``).

**Intercepts and the ridge.** ``LAMBDA`` penalises every weight but those
of the model's intercept columns, the columns that depend on an atom only
through its species (also when ``FIXINDEX`` pins weights). With ``BIAS
true`` (and ``ATOM``, which ``BIAS`` requires; :math:`N` species, the
descriptor's intercept components :math:`0 \dots N-1`):

.. list-table::
   :header-rows: 1

   * - Function
     - Intercept columns
   * - ``BF_Linear``, ``Kern_Linear``
     - the bias columns of the descriptor
   * - ``BF_Polynomial2``
     - the product :math:`(k,k)` of each intercept component with itself,
       one per species (column 0 with one species; columns 0 and
       :math:`D` with two)
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
``rcond``). Choose ``MPARAMS`` for the scale of the descriptors; ``NORM
true`` standardises the descriptor components first.

**LAMMPS.** ``pair_style tadah`` evaluates a non-linear potential as
``tadah predict`` does, also with ``BIAS true`` on several species: the
descriptor has one intercept component per species in training and in
``pair_style tadah`` alike (see :ref:`bias_term`). A potential trained
before 1.6.0 with ``BIAS true`` on several species, with one bias component
shared by the species, is refused with the instruction to retrain it.
``pair_style tadah`` also refuses a ``BF_Polynomial2`` potential without
:math:`D(D+1)/2` weights for its :math:`D` descriptor components, and a
potential of any non-linear model whose ``NMEAN``/``NSTDEV`` (``NORM
true``) or basis vectors do not fit the descriptor.

**FIXINDEX/FIXWEIGHT** with a kernel pin weights of the basis vectors
(weights 1 to :math:`n` belong to the intercept vectors, the others to
vectors drawn from the training data): a training ``DBFILE`` is required,
also when every weight is pinned.

**Searching MPARAMS.** A kernel reads ``MPARAMS`` when it is built, and
``tadah hpo`` builds the model of every evaluation from that evaluation's
candidate, so ``OPTIM MPARAMS`` searches the kernel's parameters. At
startup ``tadah hpo`` refuses ``OPTIM MPARAMS (i)`` on a function that
reads fewer than *i* values (``Kern_Linear``, ``Kern_Quadratic``,
``Kern_LQ``, ``BF_Linear`` and ``BF_Polynomial2`` read none), on the
degree of ``Kern_Polynomial`` (an integer) and on the :math:`\sigma` of
``Kern_RBF`` with a box that is not positive. (Before this was fixed,
every evaluation after a worker's first trained with that first
``MPARAMS``, while ``best_pot.tadah`` recorded the candidate's value: a
potential from such a search predicts otherwise than the loss it was
reported with, so re-run the search.)

Reading the autogenerated tables
--------------------------------

Each class entry provides:

* A concise description & mathematical form.
* The expected template parameter (kernel or basis).
* Required context keys (*bold* in the “Required key” lines).

Select the class name you need, supply exactly the keys it expects (see
*Value Limits*), and Tadah!MLIP will construct the corresponding regressor at
run-time.

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
