.. _nfp_tutorial:

Tutorial
========

The tutorial builds nested fitting searches for a tantalum (Ta) potential
step by step, from one :term:`search parameter` on a grid to global searches,
annealing and larger training data. It needs a
:ref:`Nested Fitting build <installation_nfp>` and the format of the
:term:`training configuration` (:ref:`config_file`); the :ref:`nfp_quickstart`
runs the search of Unit 1.

.. contents::
   :local:
   :depth: 1
   :backlinks: none

How the units build on each other
---------------------------------

Each unit adds one step and explains the values it chooses.
:ref:`nfp_unit_1` fits energies with a pair term and searches its cutoff.
:ref:`nfp_unit_2` adds forces, stresses and a many-body term; its
training and :term:`validation configurations <validation configuration>`
are the base of the later units. :ref:`nfp_unit_3` starts from the values
that Unit 2's search finds and scores each candidate on several validation
datasets, and :ref:`nfp_unit_4` adds crystal properties computed by LAMMPS
to the loss, with Unit 3's training configuration. :ref:`nfp_unit_5`,
:ref:`nfp_unit_6` and :ref:`nfp_unit_7` search Unit 2's potential again:
with a global optimiser, with annealing and with more training data.

.. toctree::
   :maxdepth: 1

   unit1
   unit2
   unit3
   unit4
   unit5
   unit6
   unit7

.. _nfp_tutorial_files:

The files
---------

The :download:`files archive <../nfp-files.zip>` holds every file of the
tutorial and of the :ref:`nfp_examples`. It extracts into ``files/``, with
``tutorial/``, ``examples/``, ``data/`` and ``lammps/`` beneath it. Each
unit runs in ``files/tutorial/unitN`` (``N`` is the unit number), with the
other directories in place, and states its working directory. The paths in
its commands and files are relative to that directory; the directories its
text names are relative to ``files/``. Each unit page also offers its own
files for download.

A variation of a unit runs in a copy of the unit's directory beside it, such
as ``tutorial/unit2-sbplx``. The relative paths still hold, the unit's own run
keeps its files for comparison, and later units that read the unit's files,
such as :ref:`nfp_unit_7`, read them unchanged.

.. _nfp_ta_data:

The Ta data
-----------

The units and the examples use DFT data for body-centred cubic (bcc)
tantalum: energies, forces and stresses of every structure, in the
:ref:`dataset format <dataset>` and the :ref:`recommended units <units>`.

Calculations
~~~~~~~~~~~~

Code
   VASP (Vienna Ab initio Simulation Package) 5.4.4.
Functional and pseudopotential
   PBE (Perdew-Burke-Ernzerhof), with the projector augmented-wave (PAW)
   potential ``PAW_PBE Ta_pv 07Sep2000``: 5p, 6s and 5d valence, 11
   electrons.
Plane-wave cut-off
   630 eV (``ENCUT``), with ``PREC = Accurate``, projections in
   reciprocal space (``LREAL = .FALSE.``) and no spin polarisation.
Smearing
   Methfessel-Paxton, first order, width 0.2 eV.
k-points
   Monkhorst-Pack grids: 6x6x6 for the 53- and 54-atom cells; 18x18x18,
   13x13x18 and 21x13x7 for the 2-, 4- and 6-atom cells. At the
   equilibrium lattice constant each grid gives a density
   :math:`\min_i N_i |\mathbf{a}_i|` of 57 to 60 Å. A compressed cell keeps
   its grid: the density falls to 48.5 Å for the MD cells at 400 GPa and to
   36 Å along the loading axis of the most compressed strained cells.
Convergence
   Electronic energy to 1e-6 eV (1e-9 eV for the elastic-response
   cells). Every structure is a converged electronic ground state at
   fixed atomic positions.
MD structures
   Snapshots of molecular dynamics (MD) at constant number of atoms,
   volume and temperature, with the Γ point only. The cubic cell is
   fixed; its lattice constant gives the named pressure at 0 K. Each
   snapshot is recomputed with the settings above.

Every calculation of a structure in the files was checked against these
settings: each run finished, every electronic loop converged, and the
cut-off, smearing, pseudopotential and k-point grid are those listed. The
label of a structure starts with the path of the VASP output it comes
from. Its directory names give the set and the lattice constant (EOS), the
loading direction and strain (strained crystals), or the pressure,
temperature and step (MD). The elastic-response cells come from one
output, whose path labels the first of them.

Energy reference
~~~~~~~~~~~~~~~~

The energy of a structure is its free energy (VASP ``free energy TOTEN``)
minus that of its free atoms. The free energy of an isolated Ta atom is
-3.69807734 eV. It comes from one spin-polarised atom in a 16.6 Å cubic
box, with the same pseudopotential and cut-off. That calculation uses the
Γ point, Gaussian smearing of 0.002 eV and an electronic energy converged
to 1e-4 eV. The energy per atom of a structure is therefore minus its
cohesive energy:
-8.11398 eV/atom for the perfect crystal (:ref:`nfp_ta_reference`).

The files were converted from the VASP outputs with ``tadah data
convert`` and shifted by the free-atom energy with ``tadah data balance``,
with 12 significant digits. ``SHA256SUMS`` holds the checksum of each
file.

How the structures were chosen
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Equation of state (EOS)
   54-atom bcc cells (3x3x3) with one atom displaced by 0.015 Å along x,
   lattice constants 2.70 to 3.49 Å in steps of 0.01 Å. Alternate lattice
   constants go to training (2.70, 2.72, ..., 3.48 Å) and to validation
   (2.71, ..., 3.49 Å). The DFT pressure of the cells falls from 395 GPa at
   2.70 Å to 0 at 3.32 Å.
Solid MD
   54-atom snapshots at 0, 25 and 100 GPa (300, 1645 and 2961 K) and at
   250 GPa (300 and 1645 K). The first 1000 MD steps are equilibration
   and are left out. Training takes steps 1000, 1500, ..., 4500 of each
   run; validation takes steps 1700, 2700, 3700 and 4700, 200 steps after
   a training step.
Solid MD test
   Pressures absent from training (10, 50 and 150 GPa), 300, 1645 and
   2961 K, steps 2500 and 4500.
Vacancy MD
   53-atom snapshots (54 sites, one vacancy) at 0 GPa (300 and 2961 K)
   and at 100 GPa (300, 1645 and 2961 K), with the steps of solid MD.
Extended MD
   54-atom snapshots above 2961 K at the training pressures (4935 and
   6580 K at 0, 25 and 100 GPa; 4935, 8225 and 11515 K at 250 GPa), solid
   and liquid, and at 400 GPa (300, 1645, 4935, 8225 and 11515 K), with
   the steps of solid MD.
Strained crystals
   Perfect crystals under uniaxial compression along [100], [110] and
   [111] (2-, 4- and 6-atom cells), strain 0 to 0.40 in steps of 0.02,
   with the lateral cell vectors and the fractional positions unchanged:
   the geometry of Ravelo et al., Phys. Rev. B 88, 134101 (2013). Every
   atom is a centre of inversion, so the forces are zero by symmetry. The
   largest strains reach +5.2 eV/atom and about 1300 GPa, far outside the
   MD data, and dominate an RMSE over this file. A subset keeps the
   strains up to 0.20 (stresses up to about 150 GPa).
Elastic response
   2-atom cells at a = 3.320087 Å from one finite-difference run. The set
   holds the reference cell, normal strains of ±1 % and ±2 % along x, y
   and z, and shear strains of the same sizes in xy, yz and zx. It also
   holds the reference cell with one atom moved by ±0.01 and ±0.02 Å along x.

The first two [100] cells of the strained crystals (strain 0 and 0.02)
have the geometry of structures 1 and 5 of the elastic-response file, in
separate calculations. A potential trained on one of these files is not
validated independently on the other.

Files
~~~~~

:download:`ta_eos_train.tadahdb <../files/data/ta_eos_train.tadahdb>`
   EOS, training: 40 cells, 2160 atoms.
:download:`ta_eos_val.tadahdb <../files/data/ta_eos_val.tadahdb>`
   EOS, validation: 40 cells, 2160 atoms.
:download:`ta_md_train.tadahdb <../files/data/ta_md_train.tadahdb>`
   Solid MD, training: 88 cells, 4752 atoms.
:download:`ta_md_val.tadahdb <../files/data/ta_md_val.tadahdb>`
   Solid MD, validation: 44 cells, 2376 atoms.
:download:`ta_md_test.tadahdb <../files/data/ta_md_test.tadahdb>`
   Solid MD, test: 18 cells, 972 atoms.
:download:`ta_vac_train.tadahdb <../files/data/ta_vac_train.tadahdb>`
   Vacancy MD, training: 40 cells, 2120 atoms.
:download:`ta_vac_val.tadahdb <../files/data/ta_vac_val.tadahdb>`
   Vacancy MD, validation: 20 cells, 1060 atoms.
:download:`ta_md_ext_train.tadahdb <../files/data/ta_md_ext_train.tadahdb>`
   Extended MD, training: 112 cells, 6048 atoms.
:download:`ta_md_ext_val.tadahdb <../files/data/ta_md_ext_val.tadahdb>`
   Extended MD, validation: 56 cells, 3024 atoms.
:download:`ta_strain.tadahdb <../files/data/ta_strain.tadahdb>`
   Strained crystals: 63 cells, 252 atoms.
:download:`ta_strain_max20.tadahdb <../files/data/ta_strain_max20.tadahdb>`
   Strained crystals up to strain 0.20: 33 cells, 132 atoms.
:download:`ta_elastic.tadahdb <../files/data/ta_elastic.tadahdb>`
   Elastic response: 29 cells, 58 atoms.
:download:`README.md <../files/data/README.md>`
   Credit, licence, file list and units of the data files.
:download:`SHA256SUMS <../files/data/SHA256SUMS>`
   SHA-256 checksums of the data files.

.. _nfp_ta_reference:

Reference values
~~~~~~~~~~~~~~~~

The DFT targets of the target files come from
:download:`dft_reference.csv <../files/dft_reference.csv>`, which also
gives the source and the method of each value. Each crystal value uses
the pseudopotential, cut-off and smearing above; the cell, k-points and
convergence are given with each value, and the free atom is described
under Energy reference.

Lattice constant ``a0``: 3.32011 Å
   Relaxation of the cell shape, volume and ions of the 2-atom cell at
   zero pressure, 18x18x18 k-points, electronic energy converged to
   1e-9 eV.
Cohesive energy ``Ecoh``: 8.11398 eV/atom
   Free energy per atom of the 2-atom crystal at a = 3.320087 Å against
   the free atom, 18x18x18 k-points, electronic energy converged to
   1e-9 eV.
Vacancy formation energy ``Evf``: 2.9873 eV
   Free energy of the 53-atom cell, ions relaxed and cell fixed, minus
   53/54 of that of the perfect 54-atom cell, both at 3 x 3.320087 Å with
   Γ-centred 6x6x6 k-points and electronic energy converged to 1e-6 eV.
   Without relaxation of the ions it is 3.0111 eV.
Elastic constants ``C11``: 264.2 GPa, ``C12``: 161.5 GPa
   Central differences of the stress for normal strains of ±0.01 of the
   2-atom cell at a = 3.3200 Å (pressure 0.019 GPa), 18x18x18 k-points,
   electronic energy converged to 1e-8 eV.
Elastic constant ``C44``: 74.8 GPa
   As ``C11``, for shear strains :math:`\gamma_{yz}` of ±0.01.
Bulk modulus ``B``: 195.7 GPa
   (``C11`` + 2 ``C12``) / 3. A third-order Birch-Murnaghan fit of the
   free energy per atom of 35 EOS cells (a = 3.15 to 3.49 Å) gives
   195.4 GPa.

Credit and licence
~~~~~~~~~~~~~~~~~~

The DFT data were computed by the Tadah! authors. The data files are
distributed under the licence of Tadah!, the GNU General Public License,
version 3 (GPL-3.0).
