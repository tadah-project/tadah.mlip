Quick Start
===========

This section covers the fundamentals and command line interface (CLI) of Tadah!MLIP. The CLI provides necessary tools to train and test models. The trained model (interatomic potential) can be used to run molecular dynamics simulations with LAMMPS via the Tadah!LAMMPS plugin.

For installation instructions, see :ref:`installation`. For CLI examples, see :ref:`cli_examples`.

If you plan to use Tadah! as a C++ library, we recommend completing this Quick-Start guide first; once you have the basics, head over to the :ref:`api_examples` and the full API reference.

Overview
--------

To train a model, a config file (see :ref:`config_file`) and a dataset (see :ref:`dataset`) are required.

For prediction, a potential (trained model) and a dataset are required. The potential file is an output of the training process. It is an ASCII text file containing key-value pairs that fully determine the potential.

Built-in Help
-------------

Tadah! provides some basic help. Try running it with the ``-h`` flag.

.. code-block:: bash

    tadah -h

To read more about a particular subcommand, try:

.. code-block:: bash

    tadah train -h

.. _training:

Training
--------

To train a model, run the following command in a terminal:

.. code-block:: bash

    tadah train -c config.train -v 2

- ``-v 2``: Enables verbosity INFO level, resulting in detailed output being printed to the screen during the training process.

The example ``config.train`` below fits energies and stresses (its ``STRESS true`` key). To also fit forces, add the ``-F`` flag, or the ``FORCE true`` key; for stresses, ``-S`` does what ``STRESS true`` does. Each changes the fitted potential (see the effect of the stresses below).

*Note*: Command line flags take precedence over those defined in the config file.

The ``config.train`` is type of a :ref:`config_file`. Training datasets, descriptors, cutoffs, model, and all parameters are specified in the config file. The output of this command is a trained model as a ``pot.tadah`` file.

:download:`Here <quickstart/config.train>` is the ``config.train`` file of this example:

.. literalinclude:: quickstart/config.train

An example dataset can be downloaded from :download:`here <quickstart/db.train>`. The data are bcc Ta configurations from LAMMPS molecular dynamics with the Ta1_Ravelo_2013 EAM potential: energies in eV, forces in eV/Å and the virial (configurational) Cauchy stress in eV/Å³, positive under tension. All 500 structures are 54-atom cells at one volume (lattice constant 3.322 Å).

The keys of the file:

- :ref:`DBFILE` specifies the dataset to be used for training. The path can be either absolute or relative, and the relative path is resolved with respect to the working directory.
- :ref:`STRESS` ``true`` fits the stresses as well as the energies (see below).
- The two-body (pair) block: :ref:`INIT2B` turns it on; :ref:`TYPE2B`, :ref:`RCUT2B` and :ref:`RCTYPE2B` set the descriptor (``D2_Blip``: four blip, i.e. cubic B-spline, radial functions), its cutoff distance and its cutoff function; :ref:`SGRID2B` and :ref:`CGRID2B` set the inverse widths and the centres of the blips. It is the pair block of :ref:`cli_example_1`.
- The many-body block: :ref:`INITMB`, :ref:`TYPEMB`, :ref:`RCUTMB` and :ref:`RCTYPEMB` likewise. ``DM_mBlipEnv`` gives each atom an energy :math:`F(\rho)` of a pair density :math:`\rho` summed over its neighbours: :ref:`CGRIDMB` and :ref:`SGRIDMB` set the Gaussian that the density is built from, :ref:`CEMBFUNC` and :ref:`SEMBFUNC` the blips that :math:`F` is built from, :ref:`RCUTENV` takes :math:`F` smoothly to zero at low density, and :ref:`WATOM` scales the density of this dataset to about 1. :ref:`cli_example_3` explains these keys and how they were chosen.
- Both blocks use the ``Cut_Cos`` cutoff (see :ref:`cutoffs`): the energy and the force of a pair go smoothly to zero at the cutoff distance, so the energy is conserved when a pair crosses it in molecular dynamics. (``Cut_Dummy`` truncates instead; it is for testing only.)
- :ref:`MODEL` selects Kernel Ridge Regression with a linear kernel, so the energy is linear in the descriptors, and :ref:`LAMBDA` sets its regularisation.

For a detailed explanation of KEYS and a general discussion about the configuration files, see :ref:`ConfigSection`.

Training takes about a second. On the prediction dataset below (``tadah predict -F -S -A``) the potential has an energy RMSE of 0.27 meV/atom, a force RMSE of 0.030 eV/Å and a stress RMSE of 4.3e-4 eV/Å³.

**Why the stresses.** All structures of ``db.train`` have one volume, so their energies say little about how the energy changes with volume; the stresses are its derivative with respect to strain. With ``STRESS true`` the bcc lattice constant of the potential (where the stress is zero, at 0 K) is 3.294 Å, close to the 3.304 Å of the EAM potential that generated the data. Fitted to energies only it is 3.380 Å, to energies and forces 3.389 Å, and to energies, forces and stresses (``-F`` with this file) 3.354 Å. These fits give a stable bcc crystal too (:math:`C' = (C_{11}-C_{12})/2 > 0`), which conserves energy in the LAMMPS example below, but the example's lattice, 3.30 Å, then starts at a pressure of 6.8 to 10.5 GPa instead of 0.1 GPa.

.. _prediction:

Prediction
----------

To predict energies on :download:`prediction dataset <quickstart/db.predict>` using an existing ``pot.tadah`` model, run:

.. code-block:: bash

    tadah predict -p pot.tadah -d db.predict -v 2 -A

- ``-v 2``: Enables verbosity INFO level, resulting in detailed output being printed to the screen.
- ``-A``: Provides some basic analysis of the prediction, such as the RMSE (Root Mean Square Error) between true and predicted values.

To also predict forces, add the ``-F`` flag; for stresses, add the ``-S`` flag.

The output of the ``predict`` subcommand includes three files:

- ``energy.pred``: Three columns where the first column is an index, second is dataset energy/atom and the third column lists predicted energy/atom. The ordering follows the dataset order in the config file or with the ``-d`` flag.
- ``forces.pred``: Similar idea as above, but now forces are listed. The first row is the force on the first atom in the x-direction from the first dataset, the second row is the force on the first atom in the y-direction, and so on.
- ``stress.pred``: The first six rows list components of the stress tensor from the first configuration, followed by six components from the second configuration, and so on. The ordering is xx, xy, xz, yy, yz, zz.

.. _predict_error:

Prediction errors
^^^^^^^^^^^^^^^^^

``tadah predict --error`` gives each predicted value its predictive
standard deviation. It needs a potential trained with ``OALGO 3``, which
writes the covariance of the weights :ref:`WCOV` and the noise variance of
each row type :ref:`WNOISE` (see :ref:`weight_covariance`):

.. math::

   \sigma_i = \sqrt{\phi_i^\top \Sigma_w \phi_i + s_t^2}

:math:`\phi_i` is the row of the value (its derivatives with respect to the
weights: an energy, a force component or a stress component), built as
``tadah predict`` evaluates the model, :math:`\Sigma_w` is ``WCOV`` and
:math:`s_t^2` the ``WNOISE`` of the value's type. The first term is the
variance of the predicted value, which the uncertainty of the weights gives;
the second is the noise variance of an observation, from the fit's
residuals. :math:`\sigma_i` is therefore the standard deviation of the
difference between the prediction and a reference value of the kind the
potential was fitted to. ``energy.pred``, ``forces.pred`` and
``stress.pred`` get a fourth column with it (the energy's per atom,
:math:`\sigma_E/N`), and ``error.pred`` lists for every structure its
energy's :math:`\sigma_E/N` (eV/atom) and, with ``-F`` and ``-S``, the
largest force (eV/Å) and stress (eV/Å³) :math:`\sigma` of the structure.

``--error`` refuses a potential without ``WCOV`` and ``WNOISE``: one fitted
with another ``OALGO``, and one of Tadah! 1.0.0 to 1.5.0 (and 1.6.0
development builds), whose ``SIGMA`` is ignored (retrain it with ``OALGO
3``). It refuses forces (``-F``) or stresses (``-S``) of a potential whose
fit had no force or stress rows with a non-zero weight, since its
``WNOISE`` has no noise variance of them. The noise variance is that of a
structure of weight 1; structures with their own weights (``EWEIGHT_TEMP``,
``WDBFILE``, data balancing) were fitted with other noise levels.

.. versionchanged:: 1.6.0
   The errors were computed from ``SIGMA``, a transposed and unscaled matrix,
   as :math:`\sqrt{\phi_i^\top \mathrm{diag}(\Sigma) \phi_i}` minus the
   square root of the mean of that over all rows, so most of them were
   negative and none was the standard deviation of a prediction; with
   ``NORM true`` and a linear model the predicted values themselves were
   off.

.. _task_file:

Task File
---------

A **task file** is a plain-text wrapper around Tadah!MLIP’s config syntax.  
You can use it for a **single** command (as an alternative to a CLI invocation)
*or* chain several ``TASK`` blocks in the same file to execute them
sequentially.  All key–value pairs inside the file follow the normal config
rules. The `CONFIG` key can be used to nest config files, allowing for reuse of settings or
to keep a full training or prediction setup in a dedicated config file.

Basic rules
^^^^^^^^^^^

#. Each command starts with a line beginning with the keyword ``TASK`` followed
   by the sub-command name exactly as you would type it on the CLI.

#. The lines that follow are ordinary KEY/VALUE pairs.  

   * Keys written **above** all ``TASK`` blocks act as defaults for every task.

   * Keys appearing **inside** a task block override global keys written **above** the first ``TASK`` line.  

#. A task file needs at least one ``TASK`` block: ``tadah --task`` refuses a
   file without one (``the task file 'run.task' has no TASK section``).

#. A ``TASK`` line needs no keys of its own: a ``TASK predict`` with no lines
   after it runs on the keys above the first ``TASK`` line (for example
   ``DBFILE`` and ``POTENTIAL``). Every task name is checked, with or without
   keys, and an unknown one stops the file before any task runs
   (``Unknown task section "BOGUS"``). So does a command that needs a
   subcommand: ``TASK data`` is refused, ``TASK data convert`` is a task
   (``TASK section "DATA" is a group of tasks, not a task``).

#. An error in a line names the line of the file and the file, whichever
   block it is in (``Invalid value for key EMBF0 on line 6 of run.task:
   'frei' (allowed: ZERO, FREE)``).

#. Boolean values are written in lowercase ``true`` / ``false``.

#. There is a one-to-one mapping between CLI flags/options and configuration
   keys:

  +-------------------------+--------------------+
  | CLI (short/long)        |  Config-file key   |
  +=========================+====================+
  | ``-c``, ``--config``    | ``CONFIG``         |
  +-------------------------+--------------------+
  | ``-d``, ``--dbfile``    | ``DBFILE``         |
  +-------------------------+--------------------+
  | ``-p``, ``--potential`` | ``POTENTIAL``      |
  +-------------------------+--------------------+
  | ``-v``, ``--verbose``   | ``VERBOSE``        |
  +-------------------------+--------------------+
  | ``-A``, ``--analytics`` | ``ANALYTICS``      |
  +-------------------------+--------------------+
  | ``-F``, ``--force``     | ``FORCE``          |
  +-------------------------+--------------------+
  | …                       | …                  |
  +-------------------------+--------------------+

In other words, *anything you can set on the command line can also be set with an upper-case key in the task file*.

Example
^^^^^^^

The small task file below trains a model and then immediately uses it for a
prediction run:

.. literalinclude:: quickstart/taskfile.txt

You would execute it with

.. code-block:: bash

    tadah --task taskfile.txt

and Tadah!MLIP will first run the training step, produce ``pot.tadah``, and
then feed that potential into the subsequent prediction step-all in one go.

Hints
^^^^^

* Use ``CONFIG other_file.txt`` inside a task block if you prefer to keep a
  full training or prediction setup in a dedicated config file, as the
  ``train`` task of the example does with ``config.train``.

* Indexing of positional items (e.g. ``--index 1`` -> ``INDEX 1``) starts
  from **1**.

* Comments start with ``#`` and run to the end of the line, just like in
  ordinary config files, on a ``TASK`` line too
  (``TASK train   # first task``).

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

• The generated model keeps those units.  When you couple it to LAMMPS you
  must pick a compatible `units` style—e.g. ``metal`` for eV / Å, ``real`` for
  kcal mol⁻¹ / Å, and so on.

• Some post-processing and analysis helpers (notably the ``--analytics`` flag)
  currently **assume eV / Å**.  Using a model trained in different units may
  trigger an error or, worse, silently produce meaningless numbers.

.. important::

   Unless you have a compelling reason to deviate, **use eV for energy and Å
   for length**.  That choice is tested across the entire Tadah! tool-chain and
   matches the *metal* unit style in LAMMPS out of the box.

.. _config_file:

Config File
-----------

The Tadah! configuration file manages the training process by specifying datasets, cutoff functions, radii, regression models, and descriptor choices. It supports two- and many-body descriptors. The file's structure consists of KEY/VALUE pairs, with each pair on a separate line. The KEY is a string followed by its VALUE, and the format of the VALUE depends on the KEY.

For a list and explanation of supported KEY-VALUE pairs, see :ref:`ConfigSection`.

.. _dataset:

Dataset Format
--------------

Datasets are included using the :ref:`DBFILE` key in a Config file. More than one dataset can be specified.

There is no restriction on the number of atoms in different structures, so it's okay to have a structure with 12 atoms and another one with 24 atoms in the same dataset.

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

- The first line is a comment line; it will be used as a label for a structure. Do not leave a blank line.
- ``eweight``, ``fweight``, ``sweight`` are optional weighting parameters used for training. If this line is missing, it defaults to 1.0 1.0 1.0. Do not leave a blank line. ``tadah data convert`` writes ``sweight`` 0 for a structure whose source file has no stress tensor, and (CASTEP ``.castep``) ``fweight`` 0 for one without forces, and says so in one INFO line, so that a ``STRESS true`` (``FORCE true``) fit ignores them instead of fitting zeros. The force and stress metrics (``tadah predict -A``, ``PC_FRMSE`` and ``PC_SRMSE`` in ``tadah hpo``) leave such a structure out too; a dataset in which every structure has that weight 0 is not measured (``nan``, left out of the average), and a run in which no dataset has the data is refused. ``tadah data balance --rescale`` keeps a weight of 0.
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

Also see :ref:`units`.

.. _dataset_cell_migration:

.. note::

   **Datasets written by older versions** may hold a wrong cell. Only
   structures whose 3×3 cell block is not symmetric can be affected, and
   ``tadah analysis dataset_stats`` lists them by index and label.
   Re-convert such a structure from its source file if it was written

   - before 1.3.0-beta.1 from a VASP OUTCAR or a CASTEP ``.castep``,
     ``.md`` or ``.geom`` file: the cell was stored transposed (the
     positions are right, so transposing the cell block repairs it);
   - before 1.3.0-beta.1 from a VASP ``vasprun.xml``: the cell is right but
     the positions are not
     (:math:`\mathbf{r}_\text{new} = C^\mathsf{T} C^{-1}\,\mathbf{r}_\text{old}`,
     with :math:`C` the cell block, repairs them).

   Structure files that ``tadah data write -s`` exported (VASP POSCAR,
   LAMMPS data, CASTEP ``.cell``) from a non-orthogonal CIF or COD entry
   before 1.6.0 hold a transposed cell; export them again. Potentials and
   ``tadah hpo`` results fitted to affected structures need to be
   regenerated.

.. _dataset_vasp_migration:

.. note::

   **Datasets converted from VASP by an earlier build** (an OUTCAR or a
   ``vasprun.xml``, with ``tadah data convert`` from Tadah 1.0.0 to 1.5.x,
   or from a 1.6.0 development build without this change; the version
   number does not tell them apart) hold the stress with the opposite
   sign — VASP's pressure, positive under compression — and an energy
   other than F. The OUTCAR reader stored the energy without entropy,
   which differs from F by the electronic entropy term −TS (18.7 meV/atom
   in a 54-atom Ta test file at ``SIGMA = 0.2`` with Methfessel–Paxton
   order 1). With VASP 5.4.4 the ``vasprun.xml`` reader stored
   energy(σ→0), which differs from F by a fraction of that term (a third
   for Methfessel–Paxton order 1, half for Gaussian smearing; 6.2 meV/atom
   for the same structures). Both vary by structure and grow with the
   smearing width ``SIGMA``. To tell which build converted a dataset,
   compare a stored stress with the OUTCAR's ``in kB`` line: a current
   build stores the ``in kB`` value times −6.241509074e-4 (eV/Å³), an
   earlier one times +6.241509074e-4.
   **Re-convert them** from the source files with ``tadah data convert``;
   neither can be repaired in place exactly (the entropy term is not
   stored). A fit with ``STRESS true`` on the old stress fitted its
   negative. Datasets from CASTEP, from the LAMMPS examples shipped with
   Tadah!, or written with ASE already have the tension-positive sign. The
   stress of a CASTEP ``.md`` file converted by such an earlier build still
   holds its kinetic part (about :math:`N k_B T / V`, e.g. 0.43 GPa at
   633 K for 8 Si atoms in 163 Å³); re-convert it too.

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

See :ref:`installation_lammps` for Tadah!LAMMPS interface installation instructions.

What to expect
^^^^^^^^^^^^^^

``in.ml`` builds 250 Ta atoms (5×5×5 bcc cells) at a lattice constant of 3.30 Å,
gives them random velocities for 1000 K and runs 1000 steps of 3 fs at constant
energy (NVE), printing every 100 steps. With the shipped ``pot.tadah``:

- the total energy (``TotEng``) stays constant to about 0.1 meV/atom (at most
  0.08 meV/atom from its initial value in the printed rows);
- the temperature falls from 1000 K to about 440 K: started from perfect
  lattice positions, about half of the initial kinetic energy goes into the
  potential energy (in a 10,000-step run it settles near 500 K);
- the pressure starts at about 0.1 GPa;
- the crystal stays bcc.

The lattice constant in ``in.ml`` must match the potential. This potential's own
bcc lattice constant is 3.294 Å (at 0 K); at 3.5 Å the same run starts at
−16 GPa. A crystal far from the potential's lattice can tear apart at the
fixed volume of an NVE run and turn the released energy into heat: the
D2_LJ potential that this guide shipped up to Tadah! 1.6.0 has its own bcc
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
up to Tadah! 1.6.0, which does not hold bcc Ta: see below) logs

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
Tadah! 1.6.0, and the same potential retrained from its configuration, whose
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
volume it describes bcc Ta reasonably: lattice constant 3.294 Å (EAM: 3.304 Å),
energy −8.102 eV/atom (EAM: −8.10 eV/atom), :math:`C' = (C_{11}-C_{12})/2`
of 60 GPa and bulk modulus of 132 GPa at that lattice, and fcc 0.123 eV/atom
above bcc. It is not a production Ta potential:

- The repulsion of the pair term is finite: the energy of a Ta dimer is
  12.6 eV at 0.2 Å and about 14 eV as the distance goes to zero, so atoms that
  collide hard enough (radiation damage, shocks) pass through each other.
- The density is built from a Gaussian of the distance centred at 2.88 Å, so
  it falls when neighbours come much closer than that: strongly compressed bcc
  has a spurious local energy minimum near a lattice constant of 2.25 Å
  (−2.36 eV/atom). Expanding the crystal beyond about 3.5 Å raises the energy
  far too fast (by 5.3 eV/atom from 3.29 Å to 3.8 Å).
- At the 3 fs timestep of ``in.ml``, runs started at 6000 K and 10000 K do not
  fail, but the total energy wanders by 0.70 and 1.01 meV/atom; at 1 fs, by
  0.08 and 0.10 meV/atom. Use a smaller timestep for hot systems.
- Other elastic constants, defects, surfaces, liquids and the melting point were
  not checked.

A Lennard-Jones pair potential cannot replace it. Refitted to the same data
(with the ``Cut_Dummy``, ``Cut_Cos`` or ``Cut_PolyS`` cutoff, to energies,
energies and stresses, or energies, forces and stresses), ``D2_LJ`` gives a bcc
crystal with :math:`C' < 0` for each of these nine fits (−78 to −178 GPa, each
at its own lattice constant of 2.93 to 3.06 Å): it is mechanically unstable.

Support for Multi-species Systems
---------------------------------

Tadah! can generate machine-learned potentials for both single-component and multi-component systems. The interatomic potential adapts based on the species present in the training dataset, allowing for accurate modeling of different material compositions.
