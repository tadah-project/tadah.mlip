Quick Start
===========

This section covers the fundamentals and command line interface (CLI) of Tadah!MLIP. The CLI provides necessary tools to train and test models. The trained model (interatomic potential) can be used to run molecular dynamics simulations with LAMMPS via the Tadah!LAMMPS plugin.

:ref:`installation` gives the installation instructions, and :ref:`cli_examples` the CLI examples.

For Tadah! as a C++ library, the recommended order is this Quick-Start guide first, for the basics, then the :ref:`api_examples` and the full API reference.

Overview
--------

Training a model requires a config file (:ref:`config_file`) and a dataset (:ref:`dataset`).

For prediction, a potential (trained model) and a dataset are required. The potential file is an output of the training process. It is an ASCII text file containing key-value pairs that fully determine the potential.

Built-in Help
-------------

Tadah! provides some basic help, printed with the ``-h`` flag:

.. code-block:: bash

    tadah -h

Each subcommand has its own help:

.. code-block:: bash

    tadah train -h

.. _training:

Training
--------

The following command trains a model:

.. code-block:: bash

    tadah train -c config.train -v 2

- ``-v 2``: Enables verbosity INFO level, resulting in detailed output being printed to the screen during the training process.

The example ``config.train`` below fits energies and stresses (its ``STRESS true`` key). The ``-F`` flag, or the ``FORCE true`` key, also fits forces; for stresses, ``-S`` does what ``STRESS true`` does. Each changes the fitted potential (the effect of the stresses is shown below).

An option on the command line overrides the same key of the configuration
file (:ref:`config_file`).

The ``config.train`` is type of a :ref:`config_file`. Training datasets, descriptors, cutoffs, model, and all parameters are specified in the config file. The output of this command is a trained model as a ``pot.tadah`` file.

:download:`Here <quickstart/config.train>` is the ``config.train`` file of this example:

.. literalinclude:: quickstart/config.train

An example dataset can be downloaded from :download:`here <quickstart/db.train>`. The data are bcc Ta configurations from LAMMPS molecular dynamics with the Ta1_Ravelo_2013 EAM potential: energies in eV, forces in eV/Å and the virial (configurational) Cauchy stress in eV/Å³, positive under tension. All 500 structures are 54-atom cells at one volume (lattice constant 3.322 Å).

The keys of the file:

- :ref:`DBFILE` specifies the dataset to be used for training. The path can be either absolute or relative, and the relative path is resolved with respect to the working directory.
- :ref:`STRESS` ``true`` fits the stresses as well as the energies (as described below). The fit weights them with the default :term:`error scales <error scale>`, 1 meV/atom for the energy per atom and 0.5 GPa for the stress (:ref:`ESIGMA`, :ref:`SSIGMA`; :ref:`fit_objective` describes the sum a fit minimises).
- The two-body (pair) block: :ref:`INIT2B` turns it on; :ref:`TYPE2B`, :ref:`RCUT2B` and :ref:`RCTYPE2B` set the descriptor (``D2_Blip``: four blip, i.e. cubic B-spline, radial functions), its cutoff distance and its cutoff function; :ref:`SGRID2B` and :ref:`CGRID2B` set the inverse widths and the centres of the blips. It is the pair block of :ref:`cli_example_1`.
- The many-body block: :ref:`INITMB`, :ref:`TYPEMB`, :ref:`RCUTMB` and :ref:`RCTYPEMB` likewise. ``DM_mBlipEnv`` gives each atom an energy :math:`F(\rho)` of a pair density :math:`\rho` summed over its neighbours: :ref:`CGRIDMB` and :ref:`SGRIDMB` set the Gaussian that the density is built from, :ref:`CEMBFUNC` and :ref:`SEMBFUNC` the blips that :math:`F` is built from, :ref:`RCUTENV` takes :math:`F` smoothly to zero at low density, and :ref:`WATOM` scales the density of this dataset to about 1. :ref:`cli_example_3` explains these keys and how they were chosen.
- Both blocks use the ``Cut_Cos`` cutoff (:ref:`cutoffs`): the energy and the force of a pair go smoothly to zero at the cutoff distance, so the energy is conserved when a pair crosses it in molecular dynamics. (``Cut_Dummy`` truncates instead; it is for testing only.)
- :ref:`MODEL` selects Kernel Ridge Regression with a linear kernel, so the energy is linear in the descriptors, and :ref:`LAMBDA` sets its regularisation.

:ref:`ConfigSection` explains every key in detail and discusses the configuration files in general.

Training takes about a second. On the prediction dataset below (``tadah predict -F -S -A``) the potential has an energy RMSE of 0.29 meV/atom, a force RMSE of 0.032 eV/Å and a stress RMSE of 0.049 GPa.

**Why the stresses.** All structures of ``db.train`` have one volume, so their energies say little about how the energy changes with volume; the stresses are its derivative with respect to strain. With ``STRESS true`` the bcc lattice constant of the potential (where the stress is zero, at 0 K) is 3.288 Å, close to the 3.304 Å of the EAM potential that generated the data, and fitted to energies, forces and stresses (``-F`` with this file) it is 3.294 Å. Fitted to energies only it is 3.381 Å, and to energies and forces 3.389 Å. These two fits give a stable bcc crystal too (:math:`C' = (C_{11}-C_{12})/2 > 0`), which conserves energy in the LAMMPS example below, but the example's lattice, 3.30 Å, then starts at a pressure of 9.6 and 10.5 GPa instead of −0.5 GPa.

.. _prediction:

Prediction
----------

The following command predicts energies on the :download:`prediction dataset <quickstart/db.predict>` with an existing ``pot.tadah`` model:

.. code-block:: bash

    tadah predict -p pot.tadah -d db.predict -v 2 -A

- ``-v 2``: Enables verbosity INFO level, resulting in detailed output being printed to the screen.
- ``-A``: Provides some basic analysis of the prediction, such as the RMSE (Root Mean Square Error) between true and predicted values.

The ``-F`` flag also predicts forces, and the ``-S`` flag stresses.

The output of the ``predict`` subcommand includes three files:

- ``energy.pred``: Three columns where the first column is an index, second is dataset energy/atom and the third column lists predicted energy/atom. The ordering follows the order of the datasets given with ``-d``.
- ``forces.pred``: Similar idea as above, but now forces are listed. The first row is the force on the first atom in the x-direction from the first dataset, the second row is the force on the first atom in the y-direction, and so on.
- ``stress.pred``: The first six rows list components of the stress tensor from the first configuration, followed by six components from the second configuration, and so on. The ordering is xx, xy, xz, yy, yz, zz.

.. versionchanged:: 1.6.0
   ``--error`` is refused: uncertainty output is removed.
   In Tadah! 1.0.0 to 1.5.0 its errors were not standard deviations
   (negative ones among them), and with any ``EWEIGHT``, ``FWEIGHT``,
   ``SWEIGHT`` or :term:`priority` of a structure other than 1 it scaled the
   predicted values too.

.. _units:

Units and Tadah!
----------------

Tadah! is *numerically unit-agnostic* during **training**: whatever numbers
appear in the dataset are learned as-is, so any *internally consistent* set of
units (e.g. Hartree/Bohr, kJ mol⁻¹/nm, …) will yield a valid potential.  
In day-to-day work, however, most companion tools assume the “standard
materials-science” system of

* energy in electron-volts (eV)  
* length in Ångström (Å)  
* force in eV Å⁻¹  
* stress in eV Å⁻³.

These are therefore the **units of choice**.

Practical rules
^^^^^^^^^^^^^^^

• Training on a dataset expressed in *any* units still works, provided *all*
  quantities in *all* structures use the same unit system.

• The error scales of a fit are given for data in eV and Å: ``ESIGMA`` in
  meV/atom, ``FSIGMA`` in eV/Å and ``SSIGMA`` in GPa (:ref:`fit_objective`).
  With data in other units a fit reads ``ESIGMA``/1000 and ``FSIGMA`` in the
  energy and length units of the data, and ``SSIGMA`` times
  6.241509074e-3 in its stress unit, so their values need converting.

• The generated model keeps those units.  Coupled to LAMMPS, it needs a
  compatible `units` style—e.g. ``metal`` for eV / Å, ``real`` for
  kcal mol⁻¹ / Å, and so on.

• Some post-processing and analysis helpers (notably the ``--analytics`` flag)
  currently **assume eV / Å**.  Using a model trained in different units may
  trigger an error or, worse, silently produce meaningless numbers.

.. important::

   **eV for energy and Å for length** is the recommended choice, unless there
   is a compelling reason to deviate.  That choice is tested across the entire Tadah! tool-chain and
   matches the *metal* unit style in LAMMPS out of the box.

.. _config_file:

Config File
-----------

The Tadah! configuration file manages the training process by specifying datasets, cutoff functions, radii, regression models, and descriptor choices. It supports two- and many-body descriptors. The file's structure consists of KEY/VALUE pairs, with each pair on a separate line. The KEY is a string followed by its VALUE, and the format of the VALUE depends on the KEY.

:ref:`ConfigSection` lists and explains the supported KEY-VALUE pairs.

A configuration file holds keys for the command that reads it with ``-c``
(``--config``): ``tadah train``, ``tadah hpo``, ``tadah refit`` and the
``tadah analysis`` commands ``bfunc``, ``check_bounds``, ``dataset_stats``
and ``suggest_bounds``. Its lines follow these rules:

- Comments start with ``#`` and run to the end of the line.
- Boolean values are written in lowercase: ``true``, ``false``.
- ``CONFIG other.cfg`` includes another configuration file, so several
  configuration files can share settings.
- An option of the command and the configuration key of the same name set
  the same thing, and the option on the command line overrides the line of
  the file:

  .. list-table::
     :header-rows: 1
     :widths: 50 50

     * - Command-line option
       - Configuration key
     * - ``-v``, ``--verbose``
       - ``VERBOSE``
     * - ``-F``, ``--force``
       - ``FORCE``
     * - ``-S``, ``--stress``
       - ``STRESS``
     * - ``-o``, ``--outfile``
       - ``OUTFILE``

- An error of a line names the line and the file (for a line of a file
  included with ``CONFIG``, that file): a key that is no configuration key,
  a value that the key does not take (with the values the key takes, the
  line to write, or the key's schema, format and examples), more values
  than the key takes, a block such as ``DESCRIPTOR`` ... ``ENDDESCRIPTOR``
  that is not closed, and a ``CONFIG`` line whose file does not open:

  .. code-block:: text

     Invalid key FOOBAR on line 3 of config.train (closest key: FORMAT)
     Invalid value for key RCUT2B on line 6 of config.train: 'abc'

.. versionchanged:: 1.6.0
   A ``TASK`` line is refused as an unknown key at its line: task files and
   ``tadah --task`` are removed. Each ``TASK`` section becomes a command of
   its own: its lines go into the configuration file of a command that
   takes ``-c``, the options ``-c`` needs stay on the command line, and the
   lines of another command become its options.

.. _dataset:

Dataset Format
--------------

Datasets are included using the :ref:`DBFILE` key in a Config file. More than one dataset can be specified.

There is no restriction on the number of atoms in different structures, so one dataset can hold a structure with 12 atoms and another with 24 atoms.

The dataset has the following structure:

::

    Comment line
    eweight fweight sweight
    ENERGY
    cell vector a
    cell vector b
    cell vector c
    stress tensor row s_1
    stress tensor row s_2
    stress tensor row s_3
    Element px py pz fx fy fz
    ...
    <blank line>

- The first line is a comment line; it is used as a label for a structure. It must not be a blank line.
- ``eweight``, ``fweight``, ``sweight`` are the priorities of the structure's energy, forces and stress in a fit (:ref:`fit_objective`). If this line is missing, it defaults to 1.0 1.0 1.0; a blank line is not allowed in its place. ``tadah data convert`` writes ``sweight`` 0 for a structure whose source file has no stress tensor, and (CASTEP ``.castep``, VASP ``vasprun.xml``) ``fweight`` 0 for one without forces, and says so in one INFO line, so that a ``STRESS true`` (``FORCE true``) fit ignores them instead of fitting zeros. A stress or force line that cannot be read (a value the code could not print, ``*****``; VASP OUTCAR ``in kB``, ``vasprun.xml``, CASTEP ``.castep``) gives that priority 0 too, with a WARNING naming the file and the line. Any other value a converter cannot read refuses the file, naming it and the line (or the calculation); a file that ends inside a step (a run stopped while writing it: inside one of the step's blocks, during its SCF or between its blocks) keeps the steps before it and skips that step, with a WARNING naming the file and the step, and a ``vasprun.xml`` that ends after a complete calculation, without its closing tag, is read whole, with a WARNING that nothing was skipped. The force and stress metrics (``tadah predict -A``, ``PC_FRMSE`` and ``PC_SRMSE_GPA`` in ``tadah hpo``) leave such a structure out too; a dataset in which every structure has that priority 0 is not measured (``nan``, left out of the average), and a run in which no dataset has the data is refused. A priority is a number >= 0: a negative one is refused as the file is read, naming the file and the structure. A structure with ``eweight`` 0 has no energy of its own. The fit, the energy metrics, the shift ``ESHIFT_ATOM`` and ``ESHIFT_DBATOM`` derive, ``EFILTER`` and ``tadah analysis dataset_stats`` leave it out. The energy metrics of ``tadah hpo`` (``PC_EMAE``, ``PC_ERMSE``, ``PC_ErRMSE`` and ``PC_ERSQ``) do not measure a validation dataset of only such structures, and refuse a run in which no validation dataset has an energy. ``tadah predict -A`` does not measure such a dataset either (``nan``, left out of the mean), and prints ``nan`` Energy lines, without a refusal, when no dataset has an energy. ``FFILTER`` and ``dataset_stats`` leave out the forces of a structure with ``fweight`` 0.
- The three cell lines are the lattice vectors **a**, **b** and **c**, one
  vector per line, as Cartesian components in Å (the lattice vectors are the
  *rows* of the 3×3 block). A vector is **not** written down a column.
  The vectors may be given in either handedness; the cell volume is
  :math:`|\mathbf{a}\cdot(\mathbf{b}\times\mathbf{c})|`.
- ``ENERGY`` is the total energy of the structure, in eV. For DFT data
  it is the energy whose gradient the forces are: the free energy F
  (VASP's ``free  energy   TOTEN``, CASTEP's ``Final free energy``), which
  ``tadah data convert`` stores. The steps of an OUTCAR that VASP's
  machine-learned force field computed (``ML_MODE = train``; their blocks
  are marked ``ML``) are not DFT data: ``tadah data convert`` skips them
  and says so in one warning.
- The six lines after ``ENERGY`` hold exactly three numbers each, three
  cell lines and then three stress lines, as shown; a structure whose cell or
  stress is written otherwise (a cell on one line of nine numbers, a fourth
  number on a line, a blank line inside the blocks) is refused, naming the
  file, the structure and the line.
- Each stress tensor row contains 3 numbers: the **Cauchy stress** in
  eV Å⁻³, **positive under tension** — :math:`\sigma = \frac{1}{V}
  \frac{\partial E}{\partial \varepsilon}`, what the model predicts and
  ASE's ``get_stress()`` gives. It is the negative of the pressure tensor
  that VASP prints (``in kB``) and that LAMMPS reports, and it holds no
  kinetic (ideal-gas) part. ``tadah data convert`` converts every reader
  to it.
- The number of lines beginning with Element is equal to the number of atoms in a structure.
- Every value must be a finite number: a dataset with ``nan``, ``inf``, an
  out-of-range number such as ``1e309``, or an atom line with fewer than
  the seven fields ``Element px py pz fx fy fz`` is refused, naming the file
  and the structure. Fields after the seventh are ignored. A line of only
  whitespace separates structures like an empty one.
- Element is an atom label, a chemical element symbol, such as Fe, Ti.
- ``px``, ``py``, and ``pz`` are Cartesian coordinates of the atom position,
  in Å; its fractional coordinates :math:`f_1, f_2, f_3` are defined by
  :math:`\mathbf{r} = f_1\mathbf{a} + f_2\mathbf{b} + f_3\mathbf{c}`.
  An atom may lie outside the cell (a fractional coordinate below 0 or above
  1): it stands for all its periodic images, and nothing needs to be
  wrapped. A cell may also be thinner than the cutoff; every image within the
  cutoff is used.
- ``fx``, ``fy``, and ``fz`` are components of the force vector acting on the atom.
- Each configuration is separated by a blank line.

If forces and/or stresses are not available, they can be set to zero to satisfy the parser.

Units are described in :ref:`units`.

.. _dataset_cell_migration:

.. note::

   **Datasets written by older versions** may hold a wrong cell. Only
   structures whose 3×3 cell block is not symmetric can be affected, and
   ``tadah analysis dataset_stats`` lists them by index and label.
   Such a structure needs re-converting from its source file if it was written

   - before 1.3.0-beta.1 from a VASP OUTCAR or a CASTEP ``.castep``,
     ``.md`` or ``.geom`` file: the cell was stored transposed (the
     positions are right, so transposing the cell block repairs it);
   - before 1.3.0-beta.1 from a VASP ``vasprun.xml``: the cell is right but
     the positions are not
     (:math:`\mathbf{r}_\text{new} = C^\mathsf{T} C^{-1}\,\mathbf{r}_\text{old}`,
     with :math:`C` the cell block, repairs them).

   Structure files that ``tadah data write -s`` exported (VASP POSCAR,
   LAMMPS data, CASTEP ``.cell``) from a non-orthogonal CIF or COD entry
   before 1.6.0 hold a transposed cell; they need exporting again. Potentials and
   ``tadah hpo`` results fitted to affected structures need to be
   regenerated.

.. _dataset_vasp_migration:

.. note::

   **Datasets converted from VASP by an earlier build** (an OUTCAR or a
   ``vasprun.xml``, with ``tadah data convert`` from Tadah 1.0.0 to 1.5.x) hold the stress with the opposite
   sign — VASP's pressure, positive under compression — and an energy
   other than F. The OUTCAR reader stored the energy without entropy,
   which differs from F by the electronic entropy term −TS (18.7 meV/atom
   in a 54-atom Ta test file at ``SIGMA = 0.2`` with Methfessel–Paxton
   order 1). With VASP 5.4.4 the ``vasprun.xml`` reader stored
   energy(σ→0), which differs from F by a fraction of that term (a third
   for Methfessel–Paxton order 1, half for Gaussian smearing; 6.2 meV/atom
   for the same structures). Both vary by structure and grow with the
   smearing width ``SIGMA``. A stored stress compared with the OUTCAR's
   ``in kB`` line tells which build converted a dataset: a current
   build stores the ``in kB`` value times −6.241509074e-4 (eV/Å³), an
   earlier one times +6.241509074e-4.
   **They need re-converting** from the source files with ``tadah data convert``;
   neither can be repaired in place exactly (the entropy term is not
   stored). A fit with ``STRESS true`` on the old stress fitted its
   negative. An MD OUTCAR with a step at 10000 K or more, converted by any
   earlier build, gave structures
   that are several steps in one: VASP writes such a temperature joined to
   its word (``(temperature10147.82 K)``), which the reader did not read,
   so the step did not end at its ``EKIN_LAT`` line and was stored with the
   steps after it, up to the next temperature it could read (a 6580 K run
   of 5000 steps of 54 atoms gave 4939 structures, one of 3348 atoms: 62
   steps in one). A structure whose atom count is a multiple of the run's
   ``NIONS`` tells such a dataset; it needs re-converting too. Datasets from CASTEP,
   from the LAMMPS examples shipped with Tadah!, or written with ASE
   already have the tension-positive sign. The
   stress of a CASTEP ``.md`` file converted by such an earlier build still
   holds its kinetic part (about :math:`N k_B T / V`, e.g. 0.43 GPa at
   633 K for 8 Si atoms in 163 Å³); it needs re-converting too.

Tadah! and LAMMPS
-----------------

Once trained, the ``pot.tadah`` file can be used with LAMMPS like any other pair potential.

::

    pair_style      tadah
    pair_coeff      * * pot.tadah ELEMENT1 ELEMENT2

Here is an example LAMMPS :download:`script file <quickstart/in.ml>` and :download:`pot.file <quickstart/pot.tadah>`.

The elements after the file name map the LAMMPS atom types, in order, to
elements of the potential (``NULL`` leaves a type out). A potential that
cannot be loaded, or an element it does not have, stops LAMMPS with an
``ERROR`` that gives the reason, on the screen and in ``log.lammps``, e.g.
``ERROR: pair_coeff: failed to initialise Tadah potential 'pot.tadah':
Symbol not in potential file: W``.

Each ``pair_coeff`` line reads the file it names, so one script can run
several potentials: a later ``pair_coeff * * other.tadah ELEMENT1 ...``
(with or without ``pair_style tadah`` again, which keeps the pair style)
replaces the potential, and the runs after it use ``other.tadah``, whose
notices and fast-path line (below) it logs. A ``pair_coeff`` that
``pair_style tadah`` refuses (a file it cannot load, an element the
potential does not have, a number of elements other than the number of
atom types, no file) leaves the pair style without a potential: LAMMPS
stops at its ``ERROR``, and a program that drives LAMMPS as a library (the
LAMMPS Python module) and goes on after that error gets ``ERROR:
pair_style tadah has no potential: ...`` from its next ``run``. A line
LAMMPS refuses before it reaches the pair style, such as ``pair_coeff 1 1
other.tadah ELEMENT1`` (``ERROR: Pair_coeff must start with * * for pair
style tadah``), changes nothing: the pair style keeps what it held.
Tadah! 1.5.0 and earlier read only the first file a pair style was
given: a later ``pair_coeff`` went on with that potential without a
message, under the notices of the new file.

:ref:`installation_lammps` gives the installation instructions of the Tadah!LAMMPS interface.

What to expect
^^^^^^^^^^^^^^

``in.ml`` builds 250 Ta atoms (5×5×5 bcc cells) at a lattice constant of 3.30 Å,
gives them random velocities for 1000 K and runs 1000 steps of 3 fs at constant
energy (NVE), printing every 100 steps. With the shipped ``pot.tadah``:

- the total energy (``TotEng``) stays constant to about 0.1 meV/atom (it
  moves by up to 0.08 meV/atom from its initial value in the printed rows);
- the temperature falls from 1000 K to about 500 K: started from perfect
  lattice positions, about half of the initial kinetic energy goes into the
  potential energy;
- the pressure starts at about −0.5 GPa;
- the crystal stays bcc.

The lattice constant in ``in.ml`` must match the potential. This potential's own
bcc lattice constant is 3.288 Å (at 0 K); at 3.5 Å the same run starts at
−19 GPa. A crystal far from the potential's lattice can tear apart at the
fixed volume of an NVE run and turn the released energy into heat: the
D2_LJ potential that this guide shipped up to Tadah! 1.5.0 has its own bcc
lattice near 2.9 Å, and with the 3.5 Å lattice of the ``in.ml`` of that
time it heated from 1000 K to about 8500 K. Its hard cutoff (``Cut_Dummy``)
also let the total energy wander by about 35 meV/atom.

The fast path
^^^^^^^^^^^^^

``pair_style tadah`` has a faster kernel, controlled by :ref:`EAMFAST` (on by
default), for linear single-species potentials that collapse to scalar radial
functions: a pure two-body potential, or a two-body block plus a ``DM_REAM``
many-body block. The example potential has a ``DM_mBlipEnv`` many-body block,
for which the fast path is not implemented, so ``pair_style tadah`` evaluates
the descriptors of every atom (the generic kernel), here in about a second per
1000 steps on one core, and its log has no fast-path line.

A potential that takes the fast path states in the LAMMPS log how accurate
it is. For example, a D2_LJ potential (``TYPE2B D2_LJ Ta Ta``, ``RCUT2B 6.5``,
``RCTYPE2B Cut_Dummy``, ``MODEL M_KRR Kern_Linear``; the Quick Start potential
up to Tadah! 1.5.0, which does not hold bcc Ta, as described below) logs

::

    Tadah! pair-collapse fast path ON (2b-only, nr=8192): table certified for r in [2.80125, 6.5] (rel tol 1e-9: energy 3e-13, force 1e-9); analytic below r=2.80125

The potential is linear and has a single species, so ``pair_style tadah`` does not
evaluate the descriptors for every pair of atoms: it looks the pair energy and its
derivative up in a table of 8192 points (``nr``) between 0 and the cutoff ``RCUT2B``.
The line reports how accurate the table is:

- ``table certified for r in [2.80125, 6.5]``: in this range the table gives the pair
  energy and force of the potential to a relative tolerance of ``1e-9`` (``rel tol``).
  When the potential is loaded, the error of each interval between two table points is
  bounded from 31 points inside it. Each error is measured against the largest size of
  the function from that distance to the cutoff, and at least over the outer half of the
  cutoff range, so it stays meaningful where the energy crosses zero or falls to zero at
  the cutoff. ``energy`` and ``force`` are the largest of these error bounds.
- ``analytic below r=2.80125``: pairs closer than this are evaluated from the potential
  itself. Closer in, the :math:`r^{-12}` term changes too quickly for a table of this
  size to give the force to the tolerance; for this potential the limit is reached just
  outside the minimum of the pair energy, at 2.68 Å.

So the fast path gives the same energies and forces as the full calculation
(``EAMFAST false``) to ``1e-9`` inside the certified range and exactly below it.
Distances are in the units of the potential (Å here). The range depends on the
last digits of the weights: the line above is for the potential shipped with
Tadah! 1.5.0, and the same potential retrained from its configuration, whose
weights differ only in their last digits, reports ``[2.80045, 6.5]``. A model
with a ``DM_REAM`` many-body block also reports a density (``rho``) table. The
certified range is one interval that ends at the cutoff: for a potential with
basis-function knots (for example Blip functions) or a switching cutoff such as
``Cut_PolyS``, a single interval that misses the tolerance ends it, and closer
pairs are evaluated from the potential. If a table misses the tolerance already
next to the cutoff, it has no certified range: the log says so and the full
calculation is used.

What this example potential is and is not
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is a small model for learning the tools, fitted to thermal snapshots of
one crystal at one volume, generated by another potential. Near the training
volume it describes bcc Ta reasonably: lattice constant 3.288 Å (EAM: 3.304 Å),
energy −8.104 eV/atom (EAM: −8.10 eV/atom), :math:`C' = (C_{11}-C_{12})/2`
of 57 GPa and bulk modulus of 115 GPa at that lattice, and fcc 0.117 eV/atom
above bcc. It is not a production Ta potential:

- The repulsion of the pair term is finite: the energy of a Ta dimer is
  12.3 eV at 0.2 Å and about 13.6 eV as the distance goes to zero, so atoms
  that collide hard enough (radiation damage, shocks) pass through each other.
- The density is built from a Gaussian of the distance centred at 2.88 Å, so
  it falls when neighbours come much closer than that: strongly compressed bcc
  has a spurious local energy minimum near a lattice constant of 2.25 Å
  (−2.72 eV/atom). Expanding the crystal beyond about 3.5 Å raises the energy
  far too fast (by 5.2 eV/atom from 3.29 Å to 3.8 Å).
- At the 3 fs timestep of ``in.ml``, runs started at 6000 K and 10000 K do not
  fail, but the total energy wanders by 0.73 and 1.06 meV/atom (every 10
  steps of the 1000); at 1 fs, by 0.08 and 0.14 meV/atom. Hot systems need a
  smaller timestep.
- Other elastic constants, defects, surfaces, liquids and the melting point were
  not checked.

A Lennard-Jones pair potential cannot replace it. Refitted to the same data
(with the ``Cut_Dummy``, ``Cut_Cos`` or ``Cut_PolyS`` cutoff, to energies,
energies and stresses, or energies, forces and stresses), ``D2_LJ`` gives a bcc
crystal with :math:`C' < 0` at its own lattice constant for each of these nine
fits: it is mechanically unstable.

Support for Multi-species Systems
---------------------------------

Tadah! can generate machine-learned potentials for both single-component and multi-component systems. The interatomic potential adapts based on the species present in the training dataset, allowing for accurate modelling of different material compositions.
