Quick Start
===========

This section covers the fundamentals and command line interface (CLI) of Tadah!MLIP. The CLI provides necessary tools to train and test models. The trained model (interatomic potential) can be used to run molecular dynamics simulations with LAMMPS via the Tadah!LAMMPS plugin.

For installation instructions, see :ref:`installation`. For CLI examples, see :ref:`cli_examples`.

If you are interested in using it as a C++ library, have a look at the :ref:`api_examples` and browse through the API documentation. This section might prove useful.

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

Here, :ref:`DBFILE` specifies the dataset to be used for training. The path can be either absolute or relative, and the relative path is resolved with respect to the working directory. :ref:`INIT2B` controls whether the two-body descriptor is used. :ref:`TYPE2B`, :ref:`RCUT2B`, and :ref:`RCTYPE2B` refer to the two-body type, cutoff distance, and cutoff type, respectively. Finally, the regression model is selected; in this case, it is Kernel Ridge Regression with a linear kernel for simplicity.

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

- ``energy.pred``: Two columns where the first column lists dataset energy/atom and the second column lists predicted energy/atom. The ordering follows the dataset order in the config file or with the ``-d`` flag.
- ``forces.pred``: Similar idea as above, but now forces are listed. The first row is the force on the first atom in the x-direction from the first dataset, the second row is the force on the first atom in the y-direction, and so on.
- ``stress.pred``: The first six rows list components of the stress tensor from the first configuration, followed by six components from the second configuration, and so on. The ordering is xx, xy, xz, yy, yz, zz.


.. _units:

Units and Tadah!
----------------

In principle, Tadah! will work with any units. The units used are determined by the units in the training datasets. So if your dataset has energy units of electronvolts and distance in Angstroms, then the created model will have the same units. The unit of force must be eV/Å in this case. The virial stress tensor should be in units of pressure (eV/Å^3).

The units selected by LAMMPS must be consistent with the model units of energy and forces. In this case, they would correspond to *metal* units in LAMMPS.

.. note::

    Tadah! has been tested with units of eV and Angstrom. Unless you have a good reason to use different units, these are the recommended ones.

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

Support for Multi-species Systems
---------------------------------

Tadah! can generate machine-learned potentials for both single-component and multi-component systems. The interatomic potential adapts based on the species present in the training dataset, allowing for accurate modeling of different material compositions.

