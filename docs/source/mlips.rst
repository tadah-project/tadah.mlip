.. _mlips:

Trained MLIPs
=============

This page offers the published potentials for download, with the papers that describe them. The native Tadah! files run with ``tadah predict`` and, in LAMMPS, with ``pair_style tadah``, which needs the Tadah!LAMMPS plugin (:ref:`installation_lammps`). The tabulated files run with ``pair_style table``.

The native Tadah! files below (Tadah!N2, Tadah!Kr1 and Tadah!Kr2) keep the keys that weighted their fit, :ref:`EWEIGHT`, :ref:`FWEIGHT` and :ref:`SWEIGHT` (retired: :ref:`fit_objective_conversion`), and the settings of the run that wrote them in their :ref:`training record <training_record>`, a comment block from which no key is read. ``tadah predict`` and ``pair_style tadah`` load them without a notice of retired keys.

N2
--

A machine-learned interatomic potential specifically designed to study the phase diagram of N₂ up to 10 GPa. The model uses ``pair_style tadah`` which requires the ``Tadah!LAMMPS`` plugin to be installed (:ref:`installation_lammps`). Its performance above 10 GPa is unknown. At higher pressure, its predictions are expected to diverge gradually from experimental results. It won't support high-pressure polymerisation or bond breaking in general due to its rigid nature.

It describes rigid molecules (a ``DIMER`` model): in LAMMPS each N₂ molecule must be two bonded atoms (for example with ``atom_style bond``) kept at the bond length of the potential's ``DIMER`` key, 1.1014 Å, and ``pair_style tadah`` requires ``newton off`` for this potential. Each pair of molecules whose centres of mass are within the cutoff less the bond length is evaluated as one cluster of four atoms; ``compute pe/atom`` gives each atom the sum of its energies in the clusters of its molecule, so the per-atom energies sum to the potential energy.

A ``DIMER`` potential is evaluated with every atom as Z = 1, whatever its element (``tadah train``, ``tadah predict`` and ``pair_style tadah`` alike). Every pair therefore gets the weight of the Z = 1 (H–H) pair, which is 1 unless ``WATOM`` sets a weight for H; the ``WATOM`` weight of the potential's own element (N) is not applied. Its two-body and many-body descriptors must be declared for the element pairs ``* *``, as in this file (``TYPE2B D2_Blip 4 4 * *``, ``TYPEMB DM_Blip 1 4 4 * *``). A ``DIMER`` potential whose ``TYPE2B`` or ``TYPEMB`` names specific element pairs, such as ``N N``, applies to no pair the evaluation sees; it is refused when it is loaded: ``DIMER evaluates every atom as Z=1; declare TYPE2B/TYPEMB with * * element pairs``. :ref:`DIMER` describes the key.

:download:`Tadah!N2 <mlips/tadah.n2>`

`Understanding solid nitrogen through molecular dynamics simulations with a machine-learning potential <https://doi.org/10.1103/PhysRevB.110.184107>`_

Kr
--

A two-body interatomic potential that performs exceptionally well given its simplicity. Two versions are provided. The first model (``Tadah!Kr1``) was fitted to dimers computed with coupled-cluster theory with single, double and perturbative triple excitations (CCSD(T)), while the second model's energies were corrected using equilateral Kr trimers (``Tadah!Kr2``). The latter model is recommended.

Both potentials show very good agreement with experimental data for the equation of state, melting point, and neutron scattering of the fluid. They offer significant improvements over many empirical potentials at higher pressures while retaining applicability at ambient conditions.

Potentials are provided in both the native Tadah! format, which uses ``pair_style tadah``, and the LAMMPS tabulated format, used with ``pair_style table``.

Both potentials are optimised with :ref:`nested fitting <nested_fitting>`. They include the classical ZBL potential for energies above 1 keV, smoothly joined to the CCSD(T)-based model. In other words, the Tadah! potentials are stiffer in the core electron region, where the ZBL screening function is often inadequate.

Each tabulated file is the corresponding native potential written out by LAMMPS ``pair_write`` at 1000 separations from 0.5 to 12 Å.

- :download:`Tadah!Kr1 <mlips/Tadah.Kr1>`
- :download:`Tadah!Kr2 <mlips/Tadah.Kr2>`
- :download:`Tadah!Kr1 tabulated <mlips/Tadah.Kr1.tab>`
- :download:`Tadah!Kr2 tabulated <mlips/Tadah.Kr2.tab>`

The two original potentials of the publication cited below are in the file format of the program Ta-dah! (``pair_style tadah/tadah``, documented at `Ta-dah! <https://ta-dah.readthedocs.io/en/latest/>`_). Tadah! does not read this format.

- :download:`Ta-dah!Kr1 <mlips/ta-dah.kr1>`
- :download:`Ta-dah!Kr2 <mlips/ta-dah.kr2>`

`An Accurate Machine-Learned Potential for Krypton under Extreme Conditions <https://doi.org/10.1021/acs.jpclett.4c03272>`_
