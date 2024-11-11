# Quick Start

This section introduces the basics of Tadah! and its command line interface (CLI), which provides tools for training and testing models. The trained model (interatomic potential) can be used for molecular dynamics simulations with LAMMPS via the Tadah.LAMMPS plugin.

For installation instructions, see :ref:`installation`. For CLI examples, see :ref:`cli_examples`.

If you want to use Tadah! as a C++ library, refer to :ref:`api_examples` and browse the API documentation. This section might still be helpful.

## Overview

To train a model, you need a configuration file (see :ref:`config_file`) and a dataset (see :ref:`dataset`).

For prediction, a trained model (potential file) and a dataset are required. The potential file, output from the training process, is an ASCII text file with key-value pairs that define the potential.

## Training

To train a model, use the following command in a terminal:

```bash
tadah train -c config.train
```

This command trains a model using energies only. To include forces or stresses, add the `-F` or `-S` flags, or set `FORCE true` and `STRESS true` in the config file.

*Note*: Command line flags override config file settings.

The `config.train` file specifies datasets, descriptors, cutoffs, model, and parameters. The output is a trained model as a `pot.tadah` file.

:download:`Here <quickstart/config.train>` is a minimal `config.train` example:

.. literalinclude:: quickstart/config.train

For an explanation of KEYS, see :ref:`Config`.

An example dataset can be downloaded from :download:`here <quickstart/db.train>`.

## Prediction

To predict energies using a `pot.tadah` model, run:

```bash
tadah predict -p pot.tadah -d db.predict
```

To include forces, add the `-F` flag; for stresses, the `-S` flag.

Alternatively, use a :download:`config file <quickstart/config.predict>` to specify prediction datasets (:ref:`DBFILE`) and whether to calculate forces and stresses (FORCE and STRESS keys).

`config.predict` example:

.. literalinclude:: quickstart/config.predict

To predict using `pot.tadah` and `config.predict`:

```bash
tadah predict -p pot.tadah -c config.predict
```

The `predict` subcommand outputs:

- `energy.pred`: Lists dataset and predicted energy/atom in two columns.
- `forces.pred`: Lists forces for each atom and direction.
- `stress.pred`: Lists stress tensor components ordered as xx, xy, xz, yy, yz, zz.

## Built-in Help

Tadah! provides basic help. Use the `-h` flag:

```bash
tadah -h
```

For specific subcommand details:

```bash
tadah train -h
```

## Units and Tadah!

Tadah! will work with units from the training datasets. If your dataset uses electronvolts and Angstroms, the model will also. Force units must match (e.g., eV/Å). The stress tensor units are consistent with energy (pressure*volume).

Ensure LAMMPS units match these model units, typically *metal* units in LAMMPS.

*Note*: Tadah! is tested with eV and Angstrom. These are the recommended units.

## Config File

For a list and explanation of supported KEY-VALUE pairs, see :ref:`CONFIG`.

## Dataset Format

Include datasets using the :ref:`DBFILE` key in a config file. Multiple datasets with varying atom counts are supported.

Dataset structure:

```
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
```

- The first line is a comment used as a structure label.
- `eweight`, `fweight`, `sweight` are optional weights (default is 1.0 each).
- Cell vectors and stress tensor rows have 3 numbers each.
- Element is the atom's chemical symbol; `px, py, pz` are positions; `fx, fy, fz` are force components.
- Each configuration is separated by a blank line.

If forces/stresses are unavailable, set them to zero to satisfy the parser.

## Tadah! and LAMMPS

Once trained, use the `pot.tadah` file with LAMMPS:

```lammps
pair_style      tadah
pair_coeff      * * pot.tadah ELEMENT1 ELEMENT2
```

Example LAMMPS :download:`script file <quickstart/in.ml>` and :download:`pot.file <quickstart/pot.tadah>`.

See :ref:`installation_lammps` for Tadah.LAMMPS interface installation instructions.

## Support for Multi-species Systems

Tadah! supports generating potentials for mono- and multi-component systems. The training dataset determines the species spaces for the interatomic potential.
