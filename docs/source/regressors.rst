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
``tadah hpo`` and run in LAMMPS like the linear ones.

**Kernel parameters** (``MPARAMS``): ``Kern_RBF`` takes
:math:`\sigma`, :math:`K(\mathbf{b},\mathbf{x}) = \exp(-|\mathbf{x}-\mathbf{b}|^2/2\sigma^2)`
(:math:`\gamma = 1/\sigma^2`); ``Kern_Polynomial`` takes ``d gamma c``,
:math:`(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)^d`; ``Kern_Sigmoid`` takes
``gamma c``, :math:`\tanh(\gamma\,\mathbf{b}\cdot\mathbf{x} + c)`.
``Kern_Quadratic`` (:math:`(\mathbf{b}\cdot\mathbf{x})^2`) and ``Kern_LQ``
(:math:`(\mathbf{b}\cdot\mathbf{x})^2 + \mathbf{b}\cdot\mathbf{x}`) take
none.

**The basis of a kernel** (``SBASIS``, required). The atomic energy is
:math:`E_i = \sum_j w_j K(\mathbf{b}_j, \mathbf{x}_i)` over ``SBASIS``
basis vectors, one weight each. :math:`\mathbf{b}_0` is the bias vector
:math:`e_0 = (1, 0, \dots, 0)`; the other ``SBASIS-1`` are atomic energy
descriptors of the training set (with ``NORM true``, standardised), drawn
by a fixed draw: the first ``SBASIS-1`` steps of a Fisher-Yates shuffle of
the descriptors, in the order of the training set, driven by
``std::mt19937_64`` with its default seed 5489. The same training set and
``SBASIS`` give the same basis on every run and platform, so ``tadah
train`` writes the same potential every time and the loss of ``tadah hpo``
at a given point does not depend on the draw. ``SBASIS`` must be at least
2 and at most the number of training atoms plus 1. (Implementation ``2``
draws structures instead, and takes the mean descriptor of each.) The potential records
``SBASIS`` and the basis vectors (``BASIS``) with the weights; ``tadah
predict``, ``tadah hpo`` and ``pair_style tadah`` refuse, naming the keys,
a potential without them, with a number of weights other than ``SBASIS``,
or with basis vectors of another length than the descriptor's.

**Intercepts and the ridge.** ``LAMBDA`` penalises every weight but those
of the model's intercept columns, the columns that are the same for every
atom. With ``BIAS true`` (and ``ATOM``, which ``BIAS`` requires):

.. list-table::
   :header-rows: 1

   * - Function
     - Intercept columns
   * - ``BF_Linear``, ``Kern_Linear``
     - the bias columns of the descriptor
   * - ``BF_Polynomial2``
     - column 0, bias :math:`\times` bias
   * - ``Kern_Quadratic``, ``Kern_LQ``, ``Kern_Polynomial``,
       ``Kern_Sigmoid``
     - column 0, :math:`K(e_0, \mathbf{x})`, which depends on
       :math:`\mathbf{x}` only through the bias
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

**FIXINDEX/FIXWEIGHT** with a kernel pin weights of the basis vectors
(weight 1 belongs to :math:`e_0`), which are drawn from the training
data: a training ``DBFILE`` is required, also when every weight is pinned.

**Not yet:** a kernel reads ``MPARAMS`` when it is built, and ``tadah
hpo`` reuses a worker's model between evaluations, so ``OPTIM MPARAMS``
fits every evaluation after a worker's first with that first value (the
losses do not change) while ``best_pot.tadah`` records the candidate's
value. Until that is fixed, search ``MPARAMS`` only with the environment
variable ``TADAH_HPO_FORCE_REBUILD`` set, which builds every evaluation's
model from scratch (slower).

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
