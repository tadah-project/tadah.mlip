.. _fit_balance:

The Balance Table
=================

The :term:`balance table` of a fit shows how the sum that the fit
minimises (:ref:`fit_objective`) is shared among its quantities and
datasets, and how many of the coefficients each of them determines. This
page describes where the table is, its columns and how to read it, with
the fits of :ref:`cli_example_4`.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

Where the table is
------------------

``tadah train`` prints the table after its fit. ``tadah hpo`` prints that
of its best potential once, when the run ends. At ``-v 0`` neither prints
it. The potentials of ``tadah train`` and ``best_pot.tadah`` keep it at the
end of their :ref:`training record <training_record>`; the :hpo:key:`DUMP
<ROOT.DUMP>` and :hpo:key:`!BDUMP <ROOT.BDUMP>` archives of ``tadah hpo``
do not.

A fit of M_KRR implementation 2, and one whose :ref:`FIXINDEX <FIXINDEX>`
pins every coefficient, have no table: the line reads ``Fit balance:
none``, with the reason.

The table takes one pass over the n rows and p coefficients of the fit, of
the order of :math:`n p^2` like the solve. It reads the rows in blocks, so
its memory is of the order of :math:`p^2`, of 1024 rows and of three rows
per dataset, not a copy of the rows. ``tadah hpo`` computes it only for an
evaluation whose loss may make it the best potential.

The lines of the table
----------------------

The first fit of :ref:`cli_example_4` fits the energies and forces of 100
cells of 54 atoms with nine coefficients and the default error scales:

.. literalinclude:: ../../examples/CLI/ex_4_fit_balance/default.balance.txt
   :language: text

The first line gives :ref:`LAMBDA <LAMBDA>`, the number of coefficients
the solve fitted and its rank. The rank counts the directions the solve
keeps: :ref:`OALGO <OALGO>` 2 and 4 keep all of them, ``OALGO`` 1 and 3
those above the rank threshold that their second value sets. With
``LAMBDA -1`` the first line also gives the lambda that the evidence
approximation found. With ``FIXINDEX`` it gives the number of coefficients
pinned, which the solve did not fit. Under ``ENORM SQRT`` it says so
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
that its loss measures.

The columns
-----------

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
and ``gamma`` to the rank. The ``n``, ``share`` and ``gamma`` of the rows of
the datasets sum to those of their ``all`` row.

Reading a quantity the others outweigh
--------------------------------------

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
