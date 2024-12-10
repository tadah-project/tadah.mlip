.. _mlips:

Trained MLIPs
=============

Here you can find interatomic potentials created by our group and gain insight into our current projects.

N2
--

Machine-learned interatomic potential specifically designed to study the phase diagram of N₂ up to 10 GPa. The model requires the ``Tadah!LAMMPS`` plugin to be installed (:ref:`installation_lammps`).

:download:`Tadah!N2 <mlips/tadah.n2>`

`Understanding solid nitrogen through molecular dynamics simulations with a machine-learning potential <https://doi.org/10.1103/PhysRevB.110.184107>`_

Kr
--

A two-body interatomic potential that performs exceptionally well given its simplicity. Two versions are provided. The first model (``Tadah!Kr1``) was fitted to CCSD(T) dimers, while the second model's energies were corrected using equilateral Kr trimers (``Tadah!Kr2``).

Both potentials are provided in two formats: the native Tadah! format, which requires the ``Tadah!LAMMPS`` plugin (:ref:`installation_lammps`), and a tabulated potential formatted for LAMMPS ``pair_style table``.

:download:`Tadah!Kr1 <mlips/tadah.kr1>`
:download:`Tadah!Kr1 tabulated <mlips/tadah.kr1.tab>`
:download:`Tadah!Kr2 <mlips/tadah.kr2>`
:download:`Tadah!Kr2 tabulated <mlips/tadah.kr2.tab>`

**Under review**

`This link will be updated once published <https://doi.org/>`_

Ta
--

General-purpose and transferable machine learning interatomic potential for tantalum.

**Work in progress**

H2O
---

**Work in progress**

CH4
---

**Work in progress**
