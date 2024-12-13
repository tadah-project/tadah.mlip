Command Line Tools
==================

Features
--------

- **Training**: Train models using configuration files to generate interatomic potentials.
- **Prediction**: Use trained models to predict energies and optionally forces and stresses.
- **Hyperparameter Optimization**: Optimize model architecture and hyperparameters.
- **Database Conversion**: Process computational datasets into Tadah! database format.
- **Database Management**: Identify duplicates, join, split, and sample datasets.
- **Descriptor Calculation**: Calculate descriptors for datasets using a potential file.
- **Plotting Toolkit**: Visualize basis functions, cutoffs, and two-body potentials.


Tools Description
-----------------

Training
.........

Train a model using a configuration file. The output is a ``pot.tadah`` file for use with
LAMMPS. The model can train on energies, forces, and stresses.

Examples:

  - ``tadah train -c config.tadah`` : Train with configurations provided in ``config.tadah``.
  - ``tadah train -c config.tadah -F`` : Include forces in training.
  - ``tadah train -c config.tadah -S -V`` : Include stresses and enable verbose output.

Prediction
..........

Predict properties using a trained model. Energies are always calculated, while forces
and stresses are optional.

Examples:

  - ``tadah predict -c config.tadah -p pot.tadah`` : Predict using a config file.
  - ``tadah predict -p pot.tadah -FS -d db1.tadah`` : Predict forces and stresses using datasets.
  - ``tadah predict -p pot.tadah -e -a`` : Predict model error and perform analytics.

Hyperparameter Optimization
...........................

Optimize model parameters and architecture against a custom loss function using a config
file and target constraints. See :ref:`hyperparameter_optimisation` for more details.

Example:

  - ``tadah hpo -c config.tadah -t targets -v valid.tadah`` : Optimize with initial parameters and validate against ``valid.tadah``.

Database Conversion
...................

Convert datasets from formats such as VASP and CASTEP into the Tadah! database format. This tool supports a variety of input file formats:

  - **VASP**: ``OUTCAR``, ``vasprun.xml``
  - **CASTEP**: ``.castep``, ``.md``, ``.geom``

The conversion process extracts essential data such as atomic positions, chemical elements, forces, potential energy, and the virial stress tensor if available. It is fairly robust and can handle many cases where input files are incomplete, damaged, or contain write errors.

This feature can be combined with bash tools for efficient workflow integration, allowing users to automate and streamline the conversion process.

Examples:

  - ``tadah convert -d run1.outcar -o mydata.tadah`` : Convert a single OUTCAR file into a Tadah! format.
  - ``tadah convert -d run1.md run2.geom -o combined.tadah`` : Combine multiple files into one Tadah! formatted file.
  - ``tadah convert -d $(find . -name "*.md") -o all_md_files.tadah`` : Use bash to efficiently find and convert all ``.md`` files in a directory.

Database Management
...................

Duplicates
~~~~~~~~~~

Identify duplicate structures considering atom counts and configurations.
    
Examples:

  - ``tadah db dups -d dataset1.tadah -o pruned_data.tadah`` : Identify and remove duplicates.
  - ``tadah db dups -d dataset1.tadah -dataset2.tadah -o pruned_data1.tadah pruned_data2.tadah`` : Identify and remove duplicates.
  - ``tadah db dups -d dataset1.tadah -dataset2.tadah -o pruned_data.tadah -m`` : Identify and remove duplicates and merge into single file.
  - ``tadah db dups -d dataset1.tadah`` : Provide a summary of duplicates without output file.

Join
~~~~

Merge multiple datasets into a unified dataset.
    
Example:

  - ``tadah db join -d dataset1.tadah dataset2.tadah -o merged_data.tadah`` : Merge datasets into ``merged_data.tadah``.

Split
~~~~~

Divide a large dataset using different algorithms.
    
Examples:

  - ``tadah db split -d large_dataset.tadah -o split1.tadah split2.tadah split3.tadah -e`` : Split equally.
  - ``tadah db split -d large_dataset.tadah -o split1.tadah split2.tadah split3.tadah -s 70 20 14`` : Split using sizes.
  - ``tadah db split -d large_dataset.tadah -o split1.tadah split2.tadah split3.tadah -p 25 25 50`` : Split using percentages.

Sample
~~~~~~

Create a dataset by sampling from existing datasets.
    
Examples:

  - ``tadah db sample -d dataset.tadah -o sample_data.tadah -r 100`` : Randomly sample 100 entries.
  - ``tadah db sample -d dataset1.tadah dataset2.tadah -o sample_data.tadah -e 10 -m`` : Evenly sample every 10 entries and merge.
  - ``tadah db sample -d dataset.tadah -o sample_data.tadah -i 1,2,5-10`` : Sample specific indices.

Descriptor Calculation
......................

Compute descriptors for specified datasets using a potential file. Supports indexing with single numbers, ranges, and steps. Output can be printed to the screen or saved to a file.

Examples:

  - ``tadah dcalc -d dataset.tadah -p pot.tadah -o descriptors.tadah``: Calculate descriptors using a potential file and save to a file.
  - ``tadah dcalc -d dataset.tadah -p pot.tadah -i 1,3,5-7``: Calculate specific structures.

Plotting Toolkit
................

Basis Functions Plotter
~~~~~~~~~~~~~~~~~~~~~~~

Plot two- or many-body basis functions (BF) from the existing potential file or a simple config file containing ``SGRID2B/SGRIDMB`` and ``CGRID2B/CGRIDMB`` keys. This tool can also plot cutoffs (requires ``RCYTYPE2B/RCYTYPEMB`` and ``RCUT2B/RCUTMB``) and optionally scale basis functions accordingly.

Examples:

  - ``tadah plot bf -c config -o output.dat -r 0.0 9.5 100 -t G 2b N`` : Plot Gaussian two-body BFs in the 0.0-9.5 range with 100 points
  - ``tadah plot bf -c config -o output.dat -r 0.0 9.5 100 -t B mb N`` : Plot Blip many-body BFs in the 0.0-9.5 range with 100 points
  - ``tadah plot bf -c config -o output.dat -r 0.0 9.5 100 -t B mb N -i 2-4`` : As above, but plot only 2, 3, 4 BFs
  - ``tadah plot bf -c config -o output.dat -r 0.0 9.5 100 -t B mb Y -i 2-4`` : As above, but plot cutoffs as well
  - ``tadah plot bf -c config -o output.dat -r 0.0 9.5 100 -t B mb Y -i 2-4 -s`` : As above, but scale blips by the cutoff value

Cutoff Plotter
~~~~~~~~~~~~~~

Plot cutoff functions in a specified range. The ``-t,--type`` option is one of :ref:`cutoffs`.

Examples:

  - ``tadah plot cutoff -o output.dat -r 0.0 9.5 100 -t Cut_Cos``: Plot Cut_Cos in the 0.0-9.5 range with 100 points.
  - ``tadah plot cutoff -o output.dat -r 0.0 9.5 100 -t Cut_Cos Cut_Tanh``: Plot Cut_Cos and Cut_Tanh.

Two Body Plotter
~~~~~~~~~~~~~~~~

Plot two-body potential energy and optionally force curves.

Examples:

  - ``tadah plot twobody -p pot.tadah -o output.dat -a Kr Kr -r 2.0 9.5 100``: Plot Kr-Kr in the 2.0-9.5 range with 100 points.
  - ``tadah plot twobody -p pot.tadah -o output.dat -a Kr Kr -r 2.0 9.5 100 -F`` : As above but also plot forces
