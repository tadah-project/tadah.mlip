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

An example dataset can be downloaded from :download:`here <quickstart/db.train>`.

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
- ``eweight``, ``fweight``, ``sweight`` are optional weighting parameters used for training. If this line is missing, it defaults to 1.0 1.0 1.0. Do not leave a blank line.
- Each cell vector contains 3 numbers.
- Each stress tensor row contains 3 numbers.
- The number of lines beginning with Element is equal to the number of atoms in a structure.
- Element is an atom label, a chemical element symbol, such as Fe, Ti.
- ``px``, ``py``, and ``pz`` are Cartesian coordinates of the atom position.
- ``fx``, ``fy``, and ``fz`` are components of the force vector acting on the atom.
- Each configuration is separated by a blank line.

If forces and/or stresses are not available, they can be set to zero to satisfy the parser.

Also see :ref:`units`.

Tadah! and LAMMPS
-----------------

Once trained, the ``pot.tadah`` file can be used with LAMMPS like any other pair potential.

::

    pair_style      tadah
    pair_coeff      * * pot.tadah ELEMENT1 ELEMENT2

Here is an example LAMMPS :download:`script file <quickstart/in.ml>` and :download:`pot.file <quickstart/pot.tadah>`.

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
