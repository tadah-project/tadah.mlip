Example 4 - Reading the Fit Balance Table
-----------------------------------------

This example shows what the :term:`balance table` of a fit tells about the balance
between energies and forces, and what a smaller :ref:`ESIGMA <ESIGMA>`
changes. It applies when a fit's energies, forces or stresses miss the
accuracy asked for, and when choosing ``ESIGMA``, :ref:`FSIGMA <FSIGMA>` and
:ref:`SSIGMA <SSIGMA>`. It fits the energies and forces of the training data
of :ref:`cli_example_2` twice: with the default error scales, and with
``ESIGMA`` ten times smaller. The columns of the table and how to read them
are described in :ref:`fit_balance`, which shows the tables of both fits.

Files
.....

The example reads the dataset :download:`tdata.db
<../../examples/CLI/ex_2/tdata.db>` of Example 2: 100 snapshots of bcc Ta
cells of 54 atoms from LAMMPS molecular dynamics with the Ta1_Ravelo_2013
EAM potential. Its training configurations read it as ``../ex_2/tdata.db``,
from a directory ``ex_2`` beside the example's directory.

The first fit, :download:`config.train
<../../examples/CLI/ex_4_fit_balance/config.train>`, fits energies and
forces with nine coefficients and the default error scales:

.. literalinclude:: ../../examples/CLI/ex_4_fit_balance/config.train
   :language: ini

The second, :download:`esigma.train
<../../examples/CLI/ex_4_fit_balance/esigma.train>`, is the first with
``ESIGMA 0.1``:

.. literalinclude:: ../../examples/CLI/ex_4_fit_balance/esigma.train
   :language: ini

Running the example
...................

The two fits run in the example's directory, each in under a second:

.. code-block:: bash

   tadah train -c config.train -o default.tadah
   tadah train -c esigma.train -o esigma.tadah

Each fit prints its balance table and the legend of its columns, and its
potential keeps the table at the end of its training record. The tables
are those of :download:`default.balance.txt
<../../examples/CLI/ex_4_fit_balance/default.balance.txt>` and
:download:`esigma.balance.txt
<../../examples/CLI/ex_4_fit_balance/esigma.balance.txt>`, within two units
of the last digit printed.

How the values were chosen
..........................

:ref:`LAMBDA <LAMBDA>` ``0`` leaves the prior out of the balance, so the
energies and the forces share the whole sum and every effective parameter.
:ref:`BIAS <BIAS>` ``true`` adds a constant energy per atom, a coefficient
only the energies can determine: no force depends on it. ``ESIGMA 0.1``
asks for energies ten times as accurate as the default does, at the same
force error scale.

Variations
..........

A smaller ``FSIGMA`` moves the balance to the forces instead.
:ref:`STRESS <STRESS>` ``true`` adds a stress row with its own share and
:term:`effective parameters`. A second dataset in :ref:`DBFILE <DBFILE>` adds a row
per quantity and dataset; a :ref:`WDBFILE <WDBFILE>` value below 1 for it
lowers its share and its effective parameters.
