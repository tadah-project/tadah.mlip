.. _fit_objective:

What a Fit Minimises
====================

This page describes the sum that every fit of Tadah! minimises: which
observations it counts, the error scales and priorities that weight them,
and how a configuration of Tadah! 1.5.0 is converted. It applies to
``tadah train`` (:ref:`training_page`), to every fit of ``tadah hpo`` and to
the C++ API. The keys are listed in :ref:`ConfigSection`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

The sum a fit minimises
-----------------------

A fit finds the coefficients :math:`w` (the ``WEIGHTS`` of the potential)
that minimise

.. math::

   \sum_i p_i\, m_i \left(\frac{r_i}{\sigma_i}\right)^2
   + \lambda \sum_j w_j^2

over the observations :math:`i` of the training data. Each observation has
its residual :math:`r_i` (reference value minus prediction), its
:term:`error scale` :math:`\sigma_i` and its :term:`priority` :math:`p_i`.
:math:`\lambda` is :ref:`LAMBDA`, and the penalty runs over every
coefficient but the intercepts of :ref:`BIAS <BIAS>`. The observations are:

- the energy of each structure, with :ref:`ENERGY` ``true`` (the default):
  the residual of the total energy divided by :math:`N`, the number of
  atoms of the structure (:ref:`ENORM` ``ATOM``, the default), or by
  :math:`\sqrt N` (``ENORM SQRT``);
- each Cartesian force component, with :ref:`FORCE <FORCE>` ``true``;
- the stress tensor of each structure, with :ref:`STRESS <STRESS>`
  ``true``: its six independent entries, the off-diagonal ones (xy, xz, yz)
  with :math:`m_i = 2`. The sum over a tensor is then that over its nine
  entries, the squared Frobenius norm of the residual tensor, the same in
  every orientation of the cell. :math:`m_i = 1` for every other
  observation.

The costs are summed; nothing is averaged per structure, per dataset or
per quantity. A 54-atom cell gives one energy and 162 force components, a
2-atom cell one energy and 6 force components. A row of priority 0 is not
an observation, and neither is a row the model cannot change: a force
component that is zero by symmetry, such as the y and z forces of a dimer
along x or the forces of an atom at a centre of symmetry. Such a row is
zero to rounding: no value above 1e-10 of the largest of its column over
the rows of priority above 0. A rigid translation of the atoms changes
neither the count nor the fit, and neither does a dataset of priority 0.

Error scales
------------

Each error scale is given in the unit of the metric that measures its
quantity: meV/atom for energies (:ref:`ESIGMA`, default 1), eV/Å for forces
(:ref:`FSIGMA`, default 0.1) and GPa for stresses (:ref:`SSIGMA`, default
0.5; 1 GPa = 6.241509074e-3 eV/Å³). The data files keep eV, Å and eV/Å³.

``FSIGMA a b`` and ``SSIGMA a b`` take a second value, the relative part
:math:`b` (default 0). The error scale of the forces of a structure is then

.. math::

   \sigma_s = \sqrt{a^2 + (b\, M_s)^2},

with :math:`M_s` the root mean square of the structure's own reference force
components (for stresses, of the nine entries of its reference stress).
:math:`a` must be above 0, so a structure with small forces keeps a finite
error scale. M_KRR implementation 2 fits energies only, their residuals per
atom over ``ESIGMA``, and refuses an ``FSIGMA`` or ``SSIGMA`` other than the
default and ``ENORM SQRT`` (:ref:`MODEL`).

Only the ratios of the error scales and :ref:`LAMBDA` shape the fit: every
error scale multiplied by :math:`c` fits as ``LAMBDA`` multiplied by
:math:`c^2`. A smaller ``ESIGMA`` pulls the fit towards the energies. At
``-v 2`` every fit names its error scales and which are defaults, for
example::

   INFO: Fit error scales: ESIGMA 1 meV/atom (default), ENORM ATOM
   (default), ENERGY true (default), FSIGMA 0.1 0 eV/Å (default; FORCE
   false), SSIGMA 0.5 0 GPa (default).

Every potential a fit writes has an ``ESIGMA``, ``FSIGMA``, ``SSIGMA``,
``ENORM`` and ``ENERGY`` line, the defaults included, so a retrain from it
(:ref:`retrain_from_potential`) fits with the same error scales.

Energy per atom or per square root of the atoms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``ENORM ATOM``, the default, is the recommended choice: ``ESIGMA`` then
means the same accuracy for a 2-atom and for a 250-atom cell, as the energy
RMSE of ``tadah predict -A`` does. A cell repeated :math:`k` times in a
supercell fits as the cell with each force priority multiplied by
:math:`k`: its energy per atom and its stress are those of the cell, its
force components :math:`k` times as many.

``ENORM SQRT`` divides the energy residual by :math:`\sqrt N`, the rule of
independent errors of the atoms, and ``ESIGMA`` is then in meV per
:math:`\sqrt{\text{atom}}`. At the same ``ESIGMA`` the energy of a 54-atom
cell costs 54 times what it costs under ``ENORM ATOM``, an error scale per
atom of :math:`\text{ESIGMA}/\sqrt{54}` (0.14 meV/atom for ``ESIGMA 1``).

Quantities left out of a fit
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``ENERGY``, ``FORCE`` and ``STRESS`` choose the quantities a fit observes
(``FORCE`` and ``STRESS`` are ``false`` by default, as a dataset may hold
placeholder forces or stresses). A fit refuses an error scale other than its
default for a quantity it leaves out, which would change nothing:
``ESIGMA`` or ``ENORM SQRT`` with ``ENERGY false``, ``FSIGMA`` with
``FORCE false``, ``SSIGMA`` with ``STRESS false``. It refuses all three
``false``. ``tadah predict`` does not check them: a potential trained with
``FORCE true`` predicts without ``-F``.

Priorities
----------

The priority of an observation is the product of two numbers, each >= 0:

- the ``eweight``, ``fweight`` or ``sweight`` of the structure, the second
  line of its entry in the data file (:ref:`dataset`);
- the :ref:`WDBFILE <WDBFILE>` value of its dataset (1 without ``WDBFILE``).

A per-structure value of 0 means that the structure has no such label: its
energy (forces, stress) is left out of the fit, of the metrics of ``tadah
predict -A`` and ``tadah hpo`` and of the data statistics, and an energy of
priority 0 takes no part in the derived ``ESHIFT``. ``WDBFILE 0`` leaves a
dataset out of the fit only: its structures still count for the energy
shift, the filters and the data statistics. A negative or non-finite value
is refused.

Priorities say which structures and datasets matter more; the error
scales set the balance between energies, forces and stresses.

LAMBDA and the coefficients
---------------------------

:ref:`LAMBDA` is the prior precision of the coefficients against the sum
above: a larger ``LAMBDA`` pulls the coefficients towards 0. Because the
sum counts every observation, ``LAMBDA`` acts more weakly on a fit of more
data. ``LAMBDA -1`` (with ``OALGO 3``) finds it by the evidence
approximation. A configuration written for Tadah! 1.5.0 may need another
``LAMBDA``: the error scales change the size of the sum.

.. _fit_balance:

The balance table
-----------------

The :term:`balance table` of a fit shows how the sum above is shared among
its quantities and datasets, and how many of the coefficients each of
them determines. ``tadah train`` prints it after its fit, and ``tadah hpo``
prints that of its best potential once, when the run ends; at ``-v 0``
neither prints it. The potentials of ``tadah train`` and ``best_pot.tadah``
keep it at the end of their :ref:`training record <training_record>`; the
:hpo:key:`DUMP <ROOT.DUMP>` and :hpo:key:`!BDUMP <ROOT.BDUMP>` archives of
``tadah hpo`` do not.

The first fit of :ref:`cli_example_4` fits the energies and forces of 100
cells of 54 atoms with nine coefficients and the default error scales:

.. literalinclude:: ../../examples/CLI/ex_4_fit_balance/default.balance.txt
   :language: text

The first line gives :ref:`LAMBDA <LAMBDA>`, the number of coefficients
the solve fitted and its rank. The rank counts the directions the solve
keeps: :ref:`OALGO <OALGO>` 2 and 4 keep all of them, ``OALGO`` 1 and 3
those above the rank threshold that their second value sets. With ``LAMBDA -1`` the
first line also gives the lambda that the evidence approximation found.
With :ref:`FIXINDEX <FIXINDEX>` it gives the number of coefficients pinned,
which the solve did not fit. Under ``ENORM SQRT`` it says so
(:ref:`ENORM <ENORM>`).

The error scales follow: :ref:`ESIGMA <ESIGMA>`, and :ref:`FSIGMA <FSIGMA>`
and :ref:`SSIGMA <SSIGMA>` with both values, a and b, each to 4 significant
digits. A line that does not fit in 78 characters continues on the next,
indented by four blanks.

One row follows per quantity over every dataset (``all``) and, with several
datasets, one per dataset (the k-th :ref:`DBFILE <DBFILE>` entry), then the
prior's row. The ``all`` row pools the entries of every dataset.

The rows are of the training data as the fit read it, after :ref:`EFILTER
<EFILTER>`, :ref:`FFILTER <FFILTER>` and :ref:`ZERO_COM_FORCE
<ZERO_COM_FORCE>`. A dataset of ``WDBFILE 0`` has no entry in them. In
``tadah hpo`` they are of its training data, not of the validation data
that its loss measures. The columns are:

``n``
   The observations, as the sum counts them: at most three per atom and
   six per stress tensor, as an entry zero by symmetry is not one.
``RMSE``, ``unit``
   The root mean square residual over every entry of priority above 0,
   those zero by symmetry included. The unit is that of the quantity's
   metric: meV/atom for the energy per atom (under ``ENORM SQRT`` too), eV/Å
   for the force components, GPa for the nine entries of each stress
   tensor. The row of a dataset is the RMSE that ``tadah predict -A`` gives
   for it with the same filters. The ``all`` row is not the mean over the
   datasets that the ``Mean`` column of ``tadah predict -A`` and the
   ``[avg]`` metrics of ``tadah hpo`` give.
``r/sigma``
   The root mean square of residual over error scale, over the same
   entries. Below 1 the residuals are within their error scale. Under
   ``ENORM SQRT`` the energy's residual is per square root of the atoms,
   and ``ESIGMA`` is in meV per √atom. For cells of N atoms, ``r/sigma`` is
   then the RMSE times √N over ``ESIGMA``.
``share``
   The part of the minimised sum; that of the prior is the ``LAMBDA``
   penalty.
``gamma``
   The :term:`effective parameters`, :math:`\mathrm{tr}(H^{-1} H_q)`
   with :math:`H` the matrix of the sum and :math:`H_q` the part of the
   quantity's rows: how many coefficients the quantity determines.

A row without an entry of priority above 0 has ``-`` for ``RMSE`` and
``r/sigma``. Such a row is that of a dataset of ``WDBFILE 0``, of a dataset
that the filters empty, or of a quantity whose every structure has
priority 0. Over the ``all`` rows and the prior's row, ``share`` sums to 1
and ``gamma`` to the rank. The rows of the datasets sum to their ``all``
row.

The table takes one pass over the n rows and p coefficients of the fit, of
the order of :math:`n p^2` like the solve. It reads the rows in blocks, so
its memory is of the order of :math:`p^2`, of 1024 rows and of three rows
per dataset, not a copy of the rows. ``tadah hpo`` computes it only for an
evaluation whose loss may make it the best potential, not for the others.
A fit of M_KRR implementation 2, and one whose ``FIXINDEX`` pins every
coefficient, have no table: the line reads ``Fit balance: none``, with the
reason.

Reading a quantity the others outweigh
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A quantity with a small share and few effective parameters has its
coefficients set by the other quantities, and its residuals are what they
leave. Above, the energies have 0.6 % of the sum and 1.056 effective
parameters. That is about the one coefficient that only they determine,
the constant energy per atom of :ref:`BIAS <BIAS>` ``true``. The forces
determine the other eight.

The energies' ``r/sigma`` of 0.1589 shows that this costs nothing at the
stated accuracy: the coefficients the forces set give energies well within
``ESIGMA`` 1 meV/atom. A quantity the others outweigh whose ``r/sigma`` is
above 1 misses the accuracy its error scale states. A smaller error scale
gives it more of the sum. The second fit of the example asks for energies
ten times as accurate, with ``ESIGMA 0.1``:

.. literalinclude:: ../../examples/CLI/ex_4_fit_balance/esigma.balance.txt
   :language: text

The energies have 34 % of the sum and 4.140 effective parameters. Their
RMSE falls from 0.1589 to 0.1520 meV/atom, and that of the forces rises
from 0.01645 to 0.01659 eV/Å. The energies' ``r/sigma`` of 1.520 shows
that the nine coefficients do not reach 0.1 meV/atom. A third of the sum
buys 4 % in energy RMSE, and the forces pay for it: :ref:`an error scale
adds no capacity to the model <fit_objective_wide_range>`.

.. _fit_objective_wide_range:

Data that spans a wide range
----------------------------

A training set that holds hot, liquid or strongly compressed structures
beside near-equilibrium ones has larger residuals on the hot and
compressed structures. With one absolute error scale they take most of the
sum, and the accuracy near equilibrium drops.

The recommended remedy is a dataset priority: a ``WDBFILE`` value below 1
for the datasets of large residuals (``WDBFILE 1 0.1`` gives the second
dataset a tenth of the priority of the first). It is explicit, and it keeps
the meaning of the error scales.

The relative part of ``FSIGMA`` and ``SSIGMA`` suits a dataset whose
structures differ widely in force (or stress) magnitude: with ``FSIGMA 0.1
0.2``, the error scale of a structure of RMS force well above 0.5 eV/Å is
about 20 % of that RMS force, while that of a structure of small forces
stays near 0.1 eV/Å. The default, ``b`` 0, gives every force component the
same error scale.

Neither adds capacity to the model: priorities and error scales move
accuracy between data. ``tadah hpo`` can search ``ESIGMA``, ``FSIGMA``,
``SSIGMA`` and ``WDBFILE`` against the validation metrics and properties of
a run (:hpo:key:`OPTIM.TARGET.ESIGMA`); its loss-term weights are a
separate thing, the weights of the outer loss (:ref:`loss_functions`).

.. _fit_objective_conversion:

Converting the retired keys
---------------------------

``EWEIGHT``, ``FWEIGHT``, ``SWEIGHT``, ``WDBFILE_AUTO`` and
``EWEIGHT_TEMP`` are retired. A potential that ``tadah train`` or ``tadah
hpo`` of Tadah! 1.0.0 to 1.5.0 wrote has ``EWEIGHT``, ``FWEIGHT`` and
``SWEIGHT`` lines, and from 1.3.0 a ``WDBFILE_AUTO`` line. Such a
potential loads wherever it is read (``tadah predict``, ``pair_style
tadah``, ``tadah analysis``, ``tadah properties``, ``tadah refit
--edit``), with one notice per load that lists the lines. Every fit refuses
a configuration that has one, naming each line and its conversion, and
marks a line that deleting it converts; ``tadah refit --edit`` keeps the
lines, so the edited potential is refused too. The conversions:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Line of Tadah! 1.5.0
     - Replacement
   * - ``EWEIGHT w``
     - ``ESIGMA`` = 1000 / (N √w) meV/atom for cells of N atoms. For cells
       of different sizes, ``ESIGMA`` = 1000 / √w with each structure's
       ``eweight`` multiplied by N² reproduces the sum of Tadah! 1.5.0.
   * - ``EWEIGHT 0``
     - ``ENERGY false``
   * - ``FWEIGHT w``
     - ``FSIGMA`` = 1 / √w eV/Å; with ``FORCE false``, the line deleted
       (``FSIGMA`` keeps its default)
   * - ``FWEIGHT 0``
     - ``FORCE false``
   * - ``SWEIGHT w``
     - ``SSIGMA`` = 160.2 / √w GPa, exact for the diagonal entries only (a
       fit counts each off-diagonal entry twice); with ``STRESS false``,
       the line deleted (``SSIGMA`` keeps its default)
   * - ``SWEIGHT 0``
     - ``STRESS false``
   * - ``WDBFILE_AUTO a``
     - each ``WDBFILE`` value (1 for every dataset without ``WDBFILE``)
       multiplied by :math:`n_d^{-a}`, :math:`n_d` the number of structures
       of dataset d after ``EFILTER`` and ``FFILTER``
   * - ``WDBFILE_AUTO 0``
     - the line deleted (``WDBFILE`` as it is)
   * - ``EWEIGHT_TEMP T``
     - no key: ``WDBFILE`` and the ``eweight`` of each structure
   * - any of them under M_KRR implementation 2 (``MODEL M_KRR <kernel> 2``)
     - the line deleted, with ``ESIGMA 1000`` (or ``LAMBDA`` × 10⁶ with
       ``ESIGMA 1``): the solve of Tadah! 1.5.0, :math:`(K + \lambda I)\,w
       = t` on the energies per atom in eV, which read none of them

A retrain does not read a potential's ``WDBFILE`` line
(:ref:`retrain_from_potential`). The ``WDBFILE`` values of a converted
potential, those of the ``WDBFILE_AUTO`` conversion or of its own
``WDBFILE`` line, are given on the command line (``--wdbfile``) or in a
file that includes the potential with ``CONFIG``. In the potential they
are not read (a WARNING says so), and the fit differs from that of Tadah!
1.5.0.

``tadah explain EWEIGHT`` (and so on) prints the same conversion. A
configuration that sets none of the five keys trains without a message, but
to another fit: the defaults of Tadah! 1.5.0 (``EWEIGHT``, ``FWEIGHT`` and
``SWEIGHT`` 1) are ``ESIGMA`` 1000/N meV/atom (18.5 at 54 atoms),
``FSIGMA`` 1 eV/Å and ``SSIGMA`` 160.2 GPa. ``tadah data balance`` shifts
energies only, and refuses its options ``--rescale``, ``--threshold``,
``--force`` and ``--stress``; in a target file of ``tadah hpo``,
:hpo:key:`PC_SRMSE_GPA` replaces ``PC_SRMSE``.

.. versionchanged:: 1.6.0
   A fit minimised the sum of ``EWEIGHT`` times the squared residual of each
   total energy, ``FWEIGHT`` times that of each force component and
   ``SWEIGHT`` times that of six stress entries in eV/Å³, with the
   per-structure weights.
