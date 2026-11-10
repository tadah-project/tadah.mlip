.. _nfp_unit_4:

Unit 4: Properties from LAMMPS
==============================

This unit adds six properties of the body-centred cubic (bcc) crystal to the
loss. LAMMPS computes them for every candidate potential, and the search also
covers the energy :term:`error scale` of the fit. It builds on :ref:`Unit 3
<nfp_unit_3>`, uses :ref:`the Ta data <nfp_ta_data>` and needs a :ref:`Nested
Fitting build <installation_nfp>`, which includes LAMMPS.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

What this unit adds
-------------------

The validation data of Unit 2, snapshots of solid MD, do not measure the
vacancy formation energy (Evf) or the elastic constants. The start of this
search, the potential of the training configuration, has an Evf of 0.838 eV
against the DFT 2.9873 eV and a C44 of 127 GPa against 74.8 GPa (``Start``
rows below). The unit therefore adds six properties to the three validation
terms of :ref:`Unit 2 <nfp_unit_2>`: the cohesive energy (Ecoh), the lattice
constant (a0), Evf, C11, C12 and C44. Compared with Unit 3, it uses:

- :hpo:key:`PC_LAMMPS` lines: a LAMMPS script with the candidate potential per
  line, a :term:`loss term` per property (:ref:`nfp_targets_scripts`);
- a time limit for each section of a script, :hpo:key:`PC_SCRIPT_TIMEOUT`;
- the energy error scale as a :term:`search parameter` (:hpo:key:`OPTIM ESIGMA
  <OPTIM.TARGET.ESIGMA>`);
- the optimiser :hpo:opt:`NLOPT:LN_SBPLX`.

Files
-----

The search reuses :download:`Unit 3's training configuration
<../files/tutorial/unit3/config.train>`, whose MD snapshots include cells with a
vacancy, the defect of Evf. The validation configuration is :download:`Unit 2's
<../files/tutorial/unit2/config.val>`.
:ref:`Unit 3 <nfp_unit_3>` and :ref:`Unit 2 <nfp_unit_2>` link their datasets.
The search starts from the training configuration, not from Unit 3's result.

:download:`config.hpo <../files/tutorial/unit4/config.hpo>`:

.. literalinclude:: ../files/tutorial/unit4/config.hpo
   :language: none

The scripts in ``lammps`` (:hpo:topic:`LAMMPS_SCRIPT`) compute each property as
the DFT value was computed (:ref:`nfp_ta_reference`). ``ecoh_a0.in`` relaxes a
two-atom crystal at zero pressure. ``vacancy.in`` removes one atom of a 54-site
cell at the DFT lattice constant and relaxes the ions only. ``elastic.in``
strains a crystal by ±1 % and takes the constants from the stresses.

:download:`ecoh_a0.in <../files/lammps/ecoh_a0.in>`:

.. literalinclude:: ../files/lammps/ecoh_a0.in
   :language: none

:download:`vacancy.in <../files/lammps/vacancy.in>`:

.. literalinclude:: ../files/lammps/vacancy.in
   :language: none

:download:`elastic.in <../files/lammps/elastic.in>`:

.. literalinclude:: ../files/lammps/elastic.in
   :language: none

Running the search
------------------

The search runs in ``tutorial/unit4`` and takes about two minutes on one
processor core:

.. code-block:: bash

   tadah hpo --config ../unit3/config.train --validation ../unit2/config.val \
       --hpotarget config.hpo

The journal, ``hpo_run.log``, warns that the step of :ref:`ESIGMA` turns inward:

.. output: unit4 hpo_run.log

.. code-block:: text

   WARNING   LN_SBPLX: ESIGMA: inward (start within one step of the upper bound)

Expected result
---------------

:download:`expected.csv <../files/tutorial/unit4/expected.csv>`:

.. csv-table::
   :file: ../files/tutorial/unit4/expected.csv
   :header-rows: 1
   :widths: 60 40

The loss falls from 21.33 at the start to 6.1. Evf, C11, C12 and C44 move
towards their DFT values: Evf rises from 0.838 to 2.37 eV, C12 falls from 208
to 164 GPa, and C44 from 127 to 60 GPa, below the DFT value but closer to it.
Ecoh stays within its accuracy, and a0 within about its accuracy.

The validation energy error falls from 17.5 to 7.59 meV/atom, while the force
error rises from 0.347 to 0.51 eV/Å and the stress error from 0.764 to 1.38
GPa, the trade that :ref:`ESIGMA` allows. Its best value, about 0.07 meV/atom,
lies near the lower bound of its box. The balance table in the training record
of ``best_pot.tadah`` shows how the fit shares its sum (:ref:`fit_balance`).

Reading the result
------------------

``outvar.tadah`` holds the properties of every evaluation, the start first
(:ref:`nfp_output_histories`). The ``RESULT`` part of the header of
``best_pot.tadah`` gives each property of the best potential with its loss term,
and its ``# HOT`` line the relaxed a0 (:ref:`nfp_output_best`). The journal's
last ``BEST`` entry names the five largest terms with their parts of the loss
and shares: the force term is the largest, and each property term is below each
validation term.

.. output: unit4 hpo_run.log tol=0.2
.. code-block:: text

   BEST       top: FRMSE_global=2.94e+00(48.0%)  ERMSE_global=1.60e+00(26.2%)
       SRMSE_GPA_global=8.43e-01(13.8%)  Evf=4.20e-01(6.9%)  C44=2.39e-01(3.9%)

:ref:`RCUTMB` ends next to the upper bound of its box, 7.0 Å: the minimum
may lie outside it, and the journal lists it (:ref:`nfp_search_space_bounds`):

.. output: unit4 hpo_run.log tol=best_rcutmb
.. code-block:: text

   dim                     side  value         bound         gap/width  since eval
   ...
   RCUTMB                  HI    6.9878e+00    7.0000e+00    4.07e-03   31

How the values were chosen
--------------------------

Each :hpo:key:`--varloss` names a variable of its script, its target (the DFT
value) and its :term:`loss-term weight`, :math:`1/a^2` for an accuracy
:math:`a` in the unit of the property (:ref:`nfp_targets_losses`). The
accuracies are 0.1 eV/atom for Ecoh, 0.01 Å for a0, 1/√10 eV (about 0.32 eV)
for Evf and 10 GPa for each elastic constant.

The accuracies of Ecoh and a0 are 1.2 % and 0.3 % of their DFT values: their
terms stay small for a crystal near DFT and grow for one whose energy or volume
drifts. Evf is a difference of two cell energies. Two cell energies off by 2
meV/atom each, the energy accuracy of the validation, can shift Evf by up to
0.212 eV. The accuracy of Evf lies above that, so its term asks no more of the
energies than the validation does.

The elastic constants keep the search away from an unstable crystal: bcc is
unstable for C11 below C12 or C44 below 0. A potential at either limit adds
more than 50 in these terms, against 1 for a constant at its accuracy.

The relaxed a0 is a :hpo:key:`--hotvar <PC_LAMMPS.--hotvar>` value. The
first relaxation starts at the DFT a0, later ones at the last a0 inside 3.1-3.5
Å, about 6 % either side of DFT. A crystal relaxed outside this range fails its
:term:`evaluation`. ``elastic.in`` reads a0 and so builds its crystal at the a0
of the same evaluation. ``vacancy.in`` takes the DFT lattice constant
(:hpo:key:`--invar <PC_LAMMPS.--invar>`), so all candidates relax the same cell.

The search keeps the three search parameters of :ref:`Unit 2 <nfp_unit_2>` with
their boxes and adds :ref:`ESIGMA`, the energy error scale of the fit. A smaller
value pulls the fit towards the energies (:ref:`fit_objective`), so the search
can give up force and stress accuracy for the cell energies on which Evf
depends. The box runs from 2/√1000 to 2 meV/atom, the value of the training
configuration. At fixed priorities, it spans a factor of 1000 in the energy part
of the fit relative to the forces, a range that suits :hpo:key:`LOG`
(:ref:`nfp_search_space_transforms`). The :hpo:key:`PC_ERMSE` term still
measures every candidate against 2 meV/atom: loss-term weights do not follow a
searched error scale (:ref:`nfp_targets_losses`).

LAMMPS results have no gradient, so a :hpo:key:`PC_LAMMPS` line needs a
gradient-free optimiser (:ref:`nfp_targets_scripts`). The a0 relaxation starts
at the value that an earlier evaluation reached, so a point evaluated again need
not give the same loss. :hpo:opt:`NLOPT:LN_SBPLX` compares losses instead of
modelling them, and so tolerates such changes (:ref:`nfp_choosing_local`).

:hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>` 100 gives 25 evaluations per search
parameter, as in Units 2 and 3. :hpo:key:`STEP <OPTIMIZER.STEP>` 0.25 is a
quarter of each box, as in Unit 2. :ref:`ESIGMA` starts at its upper bound, so
its first step points into the box and divides it by 1000\ :sup:`1/8`, about
2.4 (:ref:`nfp_choosing_step`).

What to try next
----------------

- Wider boxes for :ref:`RCUTMB` and :ref:`ESIGMA`, whose best values lie near a
  bound, from the best potential (:ref:`nfp_example_warm_start`).
- A larger :hpo:key:`MAXEVAL <OPTIMIZER.MAXEVAL>`: the best is the last
  evaluation, so the search has not finished (:ref:`nfp_choosing_budget`).
- Weight 0 for C11, C12 and C44, to see in ``outvar.tadah`` which the other
  terms leave unprotected; or Unit 3's training data without the vacancy cells.
- :hpo:opt:`NLOPT:LN_BOBYQA` of Unit 3, or :hpo:opt:`TADAH:ANNEAL_PT` as in
  :ref:`nfp_example_anneal_pt_full`.

Next: :ref:`nfp_unit_5`.
