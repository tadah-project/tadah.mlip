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

This will train a model using energies only. To also train on forces, add the ``-F`` flag; for stresses, add the ``-S`` flag, or include ``FORCE true`` and ``STRESS true`` keys in the config file.

*Note*: Command line flags take precedence over those defined in the config file.

The ``config.train`` is type of a :ref:`config_file`. Training datasets, descriptors, cutoffs, model, and all parameters are specified in the config file. The output of this command is a trained model as a ``pot.tadah`` file.

:download:`Here <quickstart/config.train>` is a minimal example of the ``config.train`` file:

.. literalinclude:: quickstart/config.train

The :ref:`DBFILE` key specifies the dataset to be used for training. The path can be either absolute or relative, and the relative path is resolved with respect to the working directory. :ref:`INIT2B` controls whether the two-body descriptor is used. :ref:`TYPE2B`, :ref:`RCUT2B`, and :ref:`RCTYPE2B` refer to the two-body type, cutoff distance, and cutoff type, respectively. Finally, the regression model is selected; in this case, it is Kernel Ridge Regression with a linear kernel for simplicity.

For a detailed explanation of KEYS and a general discussion about the configuration files, see :ref:`ConfigSection`.

An example dataset can be downloaded from :download:`here <quickstart/db.train>`. The data are bcc Ta configurations from LAMMPS molecular dynamics with the Ta1_Ravelo_2013 EAM potential: energies in eV, forces in eV/Å and the virial (configurational) Cauchy stress in eV/Å³, positive under tension.

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
  full training or prediction setup in a dedicated config file.

* Indexing of positional items (e.g. ``--index 1`` -> ``INDEX 1``) starts
  from **1**.

* Comments start with ``#`` and run to the end of the line, just like in
  ordinary config files.

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
- ``eweight``, ``fweight``, ``sweight`` are optional weighting parameters used for training. If this line is missing, it defaults to 1.0 1.0 1.0. Do not leave a blank line. ``tadah data convert`` writes ``sweight`` 0 for a structure whose source file has no stress tensor, and (CASTEP ``.castep``) ``fweight`` 0 for one without forces, and says so in one INFO line, so that a ``STRESS true`` (``FORCE true``) fit ignores them instead of fitting zeros. The force and stress metrics (``tadah predict -A``, ``PC_FRMSE`` and ``PC_SRMSE`` in ``tadah hpo``) leave such a structure out too, and refuse a dataset in which every structure has that weight 0. ``tadah data balance --rescale`` keeps a weight of 0.
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

When LAMMPS loads this potential, its log contains the line

::

    Tadah! pair-collapse fast path ON (2b-only, nr=8192): table certified for r in [2.80125, 6.5] (rel tol 1e-9: energy 3e-13, force 1e-9); analytic below r=2.80125

The potential is linear and has a single species, so ``pair_style tadah`` does not
evaluate the descriptors for every pair of atoms: it looks the pair energy and its
derivative up in a table of 8192 points (``nr``) between 0 and the cutoff ``RCUT2B``.
This is the fast path controlled by :ref:`EAMFAST`. The line reports how accurate the
table is:

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
Distances are in the units of the potential (Å here). A model with an EAM-form
many-body descriptor also reports a density (``rho``) table. The certified range is
one interval that ends at the cutoff: for a potential with basis-function knots (for
example Blip functions) or a switching cutoff such as ``Cut_PolyS``, a single interval
that misses the tolerance ends it, and closer pairs are evaluated from the potential.
If a table misses the tolerance already next to the cutoff, it has no certified range:
the log says so and the full calculation is used.

Support for Multi-species Systems
---------------------------------

Tadah! can generate machine-learned potentials for both single-component and multi-component systems. The interatomic potential adapts based on the species present in the training dataset, allowing for accurate modeling of different material compositions.
