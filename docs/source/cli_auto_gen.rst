analysis
~~~~~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Visual analysis commands.
    
    **LONG DESCRIPTION**:
      Visual analysis commands (e.g., descriptor, bfunc, cutoff).
    
    **OPTIONS**:
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah analysis descriptor -d dataset.tadah -p pot.tadah -o out.txt

analysis bfunc
""""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Evaluate and plot basis functions.
    
    **LONG DESCRIPTION**:
      Long description: Evaluate and plot basis functions.
    
    **OPTIONS**:
      --type  <string> [<string> ...]
        Basis function type. "2b Y" or "mb N".
        **Number of arguments**: Min 1, Max 2
        **Description**:
          Choice of basis function. For example: "2b Y" or "mb N".
          Where Y/N controls computation of the cutoff function.
        **Examples**:
          - string1 string2
          - string1 /path/to/file
    
      --rescale  
        Rescale the basis function by the cutoff function. This is useful for visualisation purposes.
        **Default**: false
    
      --outfile, -o  <string>
        String value.
        **Number of arguments**: Min 1, Max 1
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - validString
          - /path/to/file
    
      --range, -r  <START> <STOP> <NPOINTS>
        Plotting range [start stop npoints].
        **Number of arguments**: Min 3, Max 3
        **Examples**:
          - 0.1 9.5 100
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --derivative  
        Calculate derivative of the function.
        **Default**: false
    
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
    
    **OPTIONS::input**
      Provide either a potential file, a configuration file, or a task file.
      --potential, -p  <file>
        Trained model file.
        **Number of arguments**: Min 1, Max 1
        **Needs**: range, type
        **Excludes**: task, config
        **Examples**:
          - pot.tadah
    
      --config, -c  <file>
        Path to a configuration file.
        **Number of arguments**: Min 1, Max 1
        **Needs**: range, type
        **Excludes**: task, potential
        **Examples**:
          - config.tadah
          - ../config.tadah
          - /path/to/config.tadah
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah analysis bfunc -c config.tadah -o bfunc.txt -r 0 5 500 -t "2b Y"

analysis cutoff
"""""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Evaluate and plot cutoff functions.
    
    **LONG DESCRIPTION**:
      Long description: Evaluate and plot cutoff functions.
    
    **OPTIONS**:
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --range, -r  <START> <STOP> <NPOINTS>
        Plotting range [start stop npoints].
        **Number of arguments**: Min 3, Max 3
        **Examples**:
          - 0.1 9.5 100
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --derivative  
        Calculate derivative of the function.
        **Default**: false
    
    
    **OPTIONS::input**
      Provide either a cutoff type or a task file.
      --type  <string> [<string> ...]
        Generic types.
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: range
        **Excludes**: task
        **Examples**:
          - string1 string2
          - string1 /path/to/file
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah analysis cutoff -o cutoff.txt -r 0 3.14 100 -t "CutCos" --derivative

analysis descriptor
"""""""""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Calculate structure descriptors.
    
    **LONG DESCRIPTION**:
      Calculate structure descriptors.
    
    **OPTIONS**:
      --potential, -p  <file>
        Trained model file.
        **Number of arguments**: Min 1, Max 1
        **Examples**:
          - pot.tadah
    
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
      --force, -F  
        Include forces.
        **Default**: false
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --merge  
        Merge deduplication results into one file.
        **Default**: false
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
    
    **OPTIONS::input**
      Structure source or a task file. Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile, potential
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --structure, -s  <string> [<string> ...]
        Unified structural input(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile, potential
        **Excludes**: task, index
        **Description**:
          Supported file formats: .cif (Crystallographic Information File), VASP
          (POSCAR/CONTCAR), and CASTEP (.cell). The online option fetches structures
          from databases (MP, COD, NOMAD). Multiple structures can be space-separated
          or repeated. A mix of files and online sources is allowed.
        **Examples**:
          - crystal.cif
          - crystal1.cif crystal2.cell
          - mp-42 crystal.cif
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah analysis descriptor -d dataset.tadah -p pot.tadah -o out.txt --index 1,3,5

data
~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Dataset management commands.
    
    **LONG DESCRIPTION**:
      Dataset management commands (convert, print, write, merge, split, dedup, sample, balance).
    
    **OPTIONS**:
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data --task tasks.tadah

data balance
""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Apply energy shifts and/or rescaling to a dataset.
    
    **LONG DESCRIPTION**:
      Modify a dataset by applying element-specific energy shifts or rescaling
      the weights for energies, forces, and stresses. The procedure can include
      threshold checks, forcing near-zero values to a default weight, while large
      magnitudes are inversely weighted. The updated dataset is then written to
      one or more output files, which can be merged if desired. This approach
      enhances the dataset's consistency and is especially useful when refining
      potential parameters or emphasizing certain configurations during training.
    
    **OPTIONS**:
      --force, -F  
        Apply energy shift and/or rescaling to forces.
        **Default**: false
    
      --stress, -S  
        Apply energy shift and/or rescaling to stresses.
        **Default**: false
    
      --threshold  <double> [<double> ...]
        Floating point thresholds for energy, force, and stress.
        **Number of arguments**: Min 1, Max 3
        **Default**: 1e-1, 2e-2, 3e-3
        **Description**:
          If energy or the sum of force norms or the stress matrix norm exceeds the
          corresponding threshold, the relevant quantity will be rescaled using an
          inverse weighting..
        **Examples**:
          - 2.0 -4.65 0.4
          - -1.0
    
      --rescale  
        Rescale structure weights.
        **Default**: false
        **Description**:
          Applies inverse weighting to energies, forces, and stresses based on their
          magnitudes. Specifically:
          • Energies: 1 / | energy |
          • Total force: 1 / Σ‖ force_i ‖
          • Stress: 1 / ‖stress‖
          This ensures smaller magnitudes keep their weights, while large values are
          downweighted to mitigate numerical instability.
    
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --eshift  <double> [<double> ...]
        Energy shift per atom.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          If multispecies, the number of values must match the number of species.
          The shift is added to the energy of each atom.
        **Examples**:
          - 0.5
          - 0.5 0.1
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
      --merge  
        Merge deduplication results into one file.
        **Default**: false
    
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
    
    **OPTIONS::input**
      Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data balance -d db.tadah --rescale --eshift 0.5 --numeric 16 --outfile balanced.tadah
      - tadah data balance -d db1.tadah db2.tadah --rescale --outfile balanced1.tadah balanced2.tadah

data convert
""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Convert DFT output file(s) to Tadah! dataset format.
    
    **LONG DESCRIPTION**:
      Convert DFT output file(s) to Tadah! dataset format.
    
    **OPTIONS**:
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
    
    **OPTIONS::input**
      Input sources for printing. Select from the following options:
      --dft-file  <string> [<string> ...]
        Input DFT file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile
        **Excludes**: task
        **Description**:
          A single file or multiple files (space-separated). Used to extract reference
          data for training. Supported formats: VASP (OUTCAR, vasprun.xml), CASTEP
          (.castep, .md, .geom).
        **Examples**:
          - run1.outcar
          - run1.outcar run2.outcar
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data convert --dft-file run1.outcar -o output.tadah

data dedup
""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Remove duplicate structures from a dataset.
    
    **LONG DESCRIPTION**:
      Remove duplicate structures from Tadah! dataset(s). The merge option combines output into single file.
    
    **OPTIONS**:
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --threshold  <double>
        Floating point comparision threshold.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1e-6
        **Examples**:
          - 1e-4
    
      --merge  
        Merge deduplication results into one file.
        **Default**: false
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
    
    **OPTIONS::input**
      Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data dedup -d db1.tadah db2.tadah -o dedup1.tadah dedup2.tadah
      - tadah data dedup -d db1.tadah db2,tadah -o db_merged.tadah --merge

data merge
""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Merge multiple dataset files into a single file.
    
    **LONG DESCRIPTION**:
      Merge multiple dataset files into a single file.
    
    **OPTIONS**:
      --outfile, -o  <string>
        String value.
        **Number of arguments**: Min 1, Max 1
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - validString
          - /path/to/file
    
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
    
    **OPTIONS::input**
      Sources for merging. Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data merge -d db1.tadah db2.tadah -o merged.tadah

data print
""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Print structure information to screen.
    
    **LONG DESCRIPTION**:
      Print structure(s) information from dataset or structure files.
    
    **OPTIONS**:
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
    
    **OPTIONS::input**
      Input sources for printing. Select from the following options:
      --dbfile, -d  <string>
        String value.
        **Number of arguments**: Min 1, Max 1
        **Needs**: index
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - validString
          - /path/to/file
    
      --structure, -s  <string> [<string> ...]
        Unified structural input(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Excludes**: task, index
        **Description**:
          Supported file formats: .cif (Crystallographic Information File), VASP
          (POSCAR/CONTCAR), and CASTEP (.cell). The online option fetches structures
          from databases (MP, COD, NOMAD). Multiple structures can be space-separated
          or repeated. A mix of files and online sources is allowed.
        **Examples**:
          - crystal.cif
          - crystal1.cif crystal2.cell
          - mp-42 crystal.cif
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data print -d dataset.tadah
      - tadah data print -s crystal.cif

data sample
"""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Sample configurations from a dataset.
    
    **LONG DESCRIPTION**:
      Sample a subset of configurations from an existing dataset to create a new,
      single output dataset. For the --index option, ensure requested sample size
      does not exceed the number of available configurations.
    
    **OPTIONS**:
      --outfile, -o  <string>
        String value.
        **Number of arguments**: Min 1, Max 1
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - validString
          - /path/to/file
    
      --uniform  <unsigned integer>
        Sample uniformly every N-th entry.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: random, index
        **Examples**:
          - 10
    
      --random  <unsigned integer>
        Randomly sample N entries.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: uniform, index
        **Examples**:
          - 5
    
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Excludes**: random, uniform
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
    
    **OPTIONS::input**
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data sample -d full.tadah -o sample.tadah

data split
""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Split a dataset into parts.
    
    **LONG DESCRIPTION**:
      Split a dataset into parts.
    
    **OPTIONS**:
      --even  
        Split dataset into equal-size partitions. The number of partitions is determined by the number of output files provided. The last partition may be smaller if the dataset size is not divisible by the number of partitions.
        **Excludes**: chunk, percent
        **Default**: false
    
      --chunk  <unsigned integer> [<unsigned integer> ...]
        Specify chunk sizes.
        **Number of arguments**: Min 1, Max MAX_INT
        **Excludes**: even, percent
        **Examples**:
          - 20 5 3
          - 10
    
      --percent  <unsigned integer> [<unsigned integer> ...]
        Specify percentage partition.
        **Number of arguments**: Min 1, Max MAX_INT
        **Excludes**: even, chunk
        **Examples**:
          - 20 5 3
          - 10
    
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --shuffle  
        Randomize entries before splitting.
        **Default**: false
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --append  
        Append to the existing file.
        **Needs**: outfile
        **Default**: false
    
    
    **OPTIONS::input**
      Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data split -d db.tadah -o part1.tadah part2.tadah

data write
""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Write structures into a chosen format.
    
    **LONG DESCRIPTION**:
      Write structures from a dataset or a structure file or online source into a chosen format.
    
    **OPTIONS**:
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --index, -i  <index>[,<index>...]
                   <start>-<stop>
                   <start>-<stop>:<step>
        Index pattern.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          Allows flexible selection of dataset indices. Supports single indices, ranges
          (e.g., start-stop), lists, or intervals (start-stop:step). Indices are 1-based.
          Repeated indices are removed automatically.
        **Examples**:
          - 1,3,5
          - 1-4,7,9
          - 1-10:2
    
      --format, -f  <fmt>
        Output format (e.g., cif, cell, poscar).
        **Number of arguments**: Min 1, Max 1
        **Examples**:
          - cell
          - cif
          - poscar
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
    
    **OPTIONS::input**
      Structure sources for writing. Select from the following options:
      --dbfile, -d  <string>
        String value.
        **Number of arguments**: Min 1, Max 1
        **Needs**: outfile, index, format
        **Excludes**: task
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - validString
          - /path/to/file
    
      --structure, -s  <string> [<string> ...]
        Unified structural input(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: outfile, format
        **Excludes**: task, index
        **Description**:
          Supported file formats: .cif (Crystallographic Information File), VASP
          (POSCAR/CONTCAR), and CASTEP (.cell). The online option fetches structures
          from databases (MP, COD, NOMAD). Multiple structures can be space-separated
          or repeated. A mix of files and online sources is allowed.
        **Examples**:
          - crystal.cif
          - crystal1.cif crystal2.cell
          - mp-42 crystal.cif
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah data write -d dataset.tadah -o output.cell -f cell -i 7

explain
~~~~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Explain tadah command or option in more detail.
    
    **LONG DESCRIPTION**:
      This is a help command that provides detailed information about a specific command or option in the Tadah! software. It is useful for users who want to understand the purpose and usage of a particular command or option.
    
    **OPTIONS**:
      OPTION  <command>
              <command>.<option>
              <command>.<subcommand>.<option>
        Command or option to explain in a dot separated format.
        Required: true
        **Number of arguments**: Min 1, Max 1
        **Description**:
          The command or option to explain. The format is dot-separated, where each part represents a level of the command hierarchy. For example, to explain the 'verbose' option of the 'train' command, you would use 'train.verbose'.
        **Examples**:
          - tadah explain train.task
          - tadah explain data.split.even
    
    
    **EXAMPLES**:
      - tadah explain task
      - tadah explain train.verbose
      - tadah explain data.split.even

hpo
~~~

.. parsed-literal::

    **DESCRIPTION**:
      Optimize the model architecture and hyperparameters.
    
    **LONG DESCRIPTION**:
      Refine your model's architecture and hyperparameters using Tadah!'s nested
      fitting procedure, an iterative approach that goes beyond standard force-
      and energy-focused methods. By evaluating trial potentials with LAMMPS
      scripts, this framework allows you to incorporate performance constraints
      such as surface energies or phase stability, significantly enhancing model
      transferability. The global optimization algorithm systematically explores
      the parameter space, producing a robust interatomic potential tailored to
      your priorities for accuracy, speed, and other metrics. This flexible
      workflow enables users to define search space constraints, assign weights
      to different objectives, and tune performance for a wide range of
      applications.
    
    **OPTIONS**:
      --force, -F  
        Include forces.
        **Default**: false
    
      --stress, -S  
        Include stresses.
        **Default**: false
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
    
    **OPTIONS::input**
      Input sources for hpo. Select from the following options:
      --config, -c  <file>
        Path to a configuration file.
        **Number of arguments**: Min 1, Max 1
        **Needs**: validation, hpotarget
        **Excludes**: task
        **Examples**:
          - config.tadah
          - ../config.tadah
          - /path/to/config.tadah
    
      --validation  <string> [<string> ...]
        Validation dataset file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Examples**:
          - valid.tadah
    
      --hpotarget  <file>
        HPO target file.
        **Number of arguments**: Min 1, Max 1
        **Examples**:
          - hpotargets.txt
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah hpo -c config.tadah --hpotarget hpotargets.txt --validation valid.tadah

predict
~~~~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Predict properties using a trained model.
    
    **LONG DESCRIPTION**:
      Predict using an already trained model. Energy per atom is always calculated,
      while forces and stresses are optional. By default, energies are written to
      energy.pred, and if forces or stresses are calculated, they are written to
      forces.pred and stress.pred, respectively. You can use a task file with a DBFILE
      key to list prediction datasets or STRUCTURE to list files or online structures
      (see tadah explain predict.structure for supported types). Alternatively, you
      can provide Tadah! datasets or individual structure files or online sources via
      the command line.
    
    **OPTIONS**:
      --analytics, -A  
        Perform analytics.
        **Excludes**: structure
        **Default**: false
    
      --outfile, -o  <energy_file> [<force_file>] [<stress_file>]
        Output file name(s) for predicted values. The first file is for energies, followed by forces/stresses if requested. Specify filename for every requested output.
        **Number of arguments**: Min 1, Max 3
        **Default**: energy.pred, forces.pred, stress.pred
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --potential, -p  <file>
        Trained model file.
        **Number of arguments**: Min 1, Max 1
        **Examples**:
          - pot.tadah
    
      --force, -F  
        Include forces.
        **Default**: false
    
      --stress, -S  
        Include stresses.
        **Default**: false
    
      --error  
        Generate error estimates.
        **Default**: false
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
    
    **OPTIONS::input**
      Input sources for prediction. Select from the following options:
      --dbfile, -d  <string> [<string> ...]
        Path(s) to Tadah! database file(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: potential
        **Description**:
          Absolute or relative path to the Tadah! database file(s). The relative path
          is interpreted relative to the current working directory. Multiple dataset
          paths can be provided either as space-separated tokens or by repeating
          this key.
        **Examples**:
          - dbfile /path/to/dbfile
          - dbfile /path/to/dbfile1 /path/to/dbfile2
    
      --structure, -s  <string> [<string> ...]
        Unified structural input(s).
        **Number of arguments**: Min 1, Max MAX_INT
        **Needs**: potential
        **Description**:
          Supported file formats: .cif (Crystallographic Information File), VASP
          (POSCAR/CONTCAR), and CASTEP (.cell). The online option fetches structures
          from databases (MP, COD, NOMAD). Multiple structures can be space-separated
          or repeated. A mix of files and online sources is allowed.
        **Examples**:
          - crystal.cif
          - crystal1.cif crystal2.cell
          - mp-42 crystal.cif
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah predict -p pot.tadah -s crystal1.cif crystal2.cif --force
      - tadah predict -p pot.tadah -d db1.tadah --numeric 12
      - tadah predict -p pot.tadah -d db.tadah --stress --outfile predicted_energy.dat predicted_stresses.dat

properties
~~~~~~~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Evaluate physical properties for MLIP target evaluation.
    
    **LONG DESCRIPTION**:
      Evaluate physical properties for MLIP target evaluation (e.g., pairwise, ecurve, eos, defect, surface, mechanics).
    
    **OPTIONS**:
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah properties pairwise -p pot.tadah -o out.txt -r 0 10 100 -a "Kr Kr"

properties pairwise
"""""""""""""""""""

.. parsed-literal::

    **DESCRIPTION**:
      Compute E vs. r for a given potential.
    
    **OPTIONS**:
      --eshift  <double>
        Shift the energy curve by this value. If zero is provided the curve will be shifted by the furthest right value of the curve. Provide single double for --eshift if potential is either two- or many-body. If potential is both two- and many-body, provide three doubles for --eshift. The first double if for total energy second double is for two-body, the third for many-body.
        **Number of arguments**: Min 1, Max 3
        **Description**:
          If multispecies, the number of values must match the number of species.
          The shift is added to the energy of each atom.
        **Examples**:
          - 2.0
          - -1.0
    
      --outfile, -o  <string> [<string> ...]
        Output file.
        **Number of arguments**: Min 1, Max MAX_INT
        **Description**:
          The output file to be written. Multiple files can be specified if the command
          produces more than one output.
        **Examples**:
          - output.tadah
    
      --range, -r  <START> <STOP> <NPOINTS>
        Plotting range [start stop npoints].
        **Number of arguments**: Min 3, Max 3
        **Examples**:
          - 0.1 9.5 100
    
      --atompair, -a  <element1> <element2>
        Pair of chemical elements.
        **Number of arguments**: Min 1, Max 2
        **Examples**:
          - "Kr Kr"
    
      --force, -F  
        Include forces.
        **Default**: false
    
      --numeric  <unsigned integer>
        Numeric output precision.
        **Number of arguments**: Min 1, Max 1
        **Default**: 12
        **Description**:
          Sets the number of decimal places for output.
        **Examples**:
          - 12
    
      --error  
        Generate error estimates.
        **Default**: false
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --bondenergy  
        Calculate bond energy instead of per atom value.
        **Default**: false
    
    
    **OPTIONS::input**
      Select from the following options:
      --potential, -p  <file>
        Trained model file.
        **Number of arguments**: Min 1, Max 1
        **Needs**: outfile, range, atompair
        **Excludes**: task
        **Examples**:
          - pot.tadah
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah properties pairwise -p pot.tadah -o out.txt -r 0 10 100 -a "Kr Kr"

train
~~~~~

.. parsed-literal::

    **DESCRIPTION**:
      Train a model.
    
    **LONG DESCRIPTION**:
      Train a model using either a configuration file or a task file. By default,
      it trains on energies; however, forces and stresses can also be included.
      This command requires a configuration file and yields a trained interatomic
      potential which can be used with Tadah! or for molecular
      dynamics simulations in LAMMPS via pair_style tadah.
    
    **OPTIONS**:
      --outfile, -o  <file>
        Output file name for the trained model.
        **Number of arguments**: Min 1, Max 1
        **Default**: pot.tadah
        **Description**:
          Output file name for the trained model.
        **Examples**:
          - output.tadah
    
      --verbose, -v  <unsigned integer>
        Verbosity level. 0-2: ERROR, WARNING, INFO.
        **Number of arguments**: Min 1, Max 1
        **Default**: 1
        **Description**:
          Verbosity level. 0: ERROR, 1: WARNING, 2: INFO. The verbosity level controls
          the amount of information printed during execution. Higher levels provide
          more detailed output.
        **Examples**:
          - 2
    
      --force, -F  
        Include forces.
        **Default**: false
    
      --stress, -S  
        Include stresses.
        **Default**: false
    
      --uncertainty  
        Output uncertainty estimates.
        **Default**: false
    
    
    **OPTIONS::input**
      Provide a configuration file for a single task or a complete tasks file.
      --config, -c  <file>
        Path to a configuration file.
        **Number of arguments**: Min 1, Max 1
        **Examples**:
          - config.tadah
          - ../config.tadah
          - /path/to/config.tadah
    
      --task, -t  <file>
        A file containing task(s) to be executed.
        **Number of arguments**: Min 1, Max 1
        **Excludes**: ALL
        **Description**:
          The task file is a convenient way to specify multiple tasks without having to
          provide all the command-line arguments for each task. The task file should be
          in the same format as the configuration file, but it can also include additional
          information such as the task name and any specific parameters for that task.
          A task in a task file begins with the keyword 'TASK' followed by the task name.
          The task name is simply a command to be executed or both command and subcommand.
          The lines following the `TASK` keyword should contain parameters required for
          the task specified above. For example, CLI `--verbose 2` is 'VERBOSE 2' in the
          task file.
          
          # Example TASK file containing two tasks:
          # Global options
          NUMERIC 14 # output precision
          VERBOSE 2  # verbosity level
          
          TASK predict
          DBFILE db1.tadah db2.tadah db3.tadah
          DBFILE db4.tadah db5.tadah db6.tadah
          FORCE true
          ANALYTICS true
          
          TASK data print
          STRUCTURE crystal1.cif crystal2.cif
        **Examples**:
          - path/to/tasks.tadah
    
    
    **EXAMPLES**:
      - tadah train -c config.tadah
      - tadah train -c config.tadah --force --stress
      - tadah train -c config.tadah --verbose 2 --outfile potential.tadah
      - tadah train --task tasks.tadah
