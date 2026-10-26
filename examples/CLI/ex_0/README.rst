Example 0 - Basics: Tadah!MLIP
------------------------------

``tadah`` is a command-line tool designed to simplify training and evaluation of machine learning models. It provides a user-friendly interface for managing configurations, datasets, and training processes. ``tadah`` is the main entry point for subcommands that perform specific tasks within a machine learning workflow.

This example shows:
  
- how the ``tadah`` binary gives help information;
- how the ``explain`` subcommand explains the purpose of different subcommands and their options.

The example needs **Tadah!** installed and accessible through the ``$PATH``. A binary that is not on the ``$PATH`` is either added to it or called by its full path in the commands.

.. note::

   :ref:`installation` gives the installation instructions and system prerequisites.

Accessing Tadah! Help
.....................

The ``tadah`` binary provides a help command to display information about available subcommands and their options. This is especially useful for first-time users or those needing a refresher on Tadah!’s capabilities.

.. code-block:: bash

   tadah --help

or:

.. code-block:: bash

   tadah -h

These commands will print a list of subcommands and their short descriptions.

Subcommand Hierarchy
....................

Tadah! supports multiple levels of subcommands for better organisation and clarity:

- **Top-level subcommands**: These include commands like ``train`` or ``data``.
- **Second-level subcommands**: Certain commands (like ``data``) have subcommands of their own (e.g., ``split``, ``balance``).

For instance, ``tadah data split`` is an example of a second-level subcommand where ``data`` is the top-level subcommand and ``split`` is a subset of data-related operations.

Subcommand Help
...............

For help with a specific subcommand, such as ``train``:

.. code-block:: bash

   tadah train --help

Tadah! explain command
......................

Tadah! offers an ``explain`` subcommand that explains the purpose of each subcommand and its options. The ``explain`` subcommand takes a single argument:

- The argument represents a query about either a particular (sub)command or an option.
- It prints an extended description, including usage, purpose, and any additional helpful information.
- The subcommand or option to explain is given in a *dot-separated* format. Each dot separates the subcommand(s), and the specific option of interest.

For example, the ``train`` command’s ``--verbose`` option:

.. code-block:: bash

   tadah explain train.verbose

For a second-level subcommand, such as ``tadah data split``:

.. code-block:: bash

   tadah explain data.split

And the ``--dbfile`` option of ``tadah data split``:

.. code-block:: bash

   tadah explain data.split.dbfile

Runtime Errors
..............

Tadah! provides helpful runtime errors, such as when a configuration file is missing a necessary key. No code is entirely bug-free, and issue reports are welcome on **Tadah!’s GitLab**:

- `Tadah! GitLab Issues <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>`_

