Example 0 - Basics: Tadah!MLIP
------------------------------

``tadah`` is a command-line tool designed to simplify training and evaluation of machine learning models. It provides a user-friendly interface for managing configurations, datasets, and training processes. ``tadah`` is the main entry point for subcommands that perform specific tasks within your machine learning workflow.

In this example, we demonstrate how to:
  
- Access help information using the ``tadah`` binary.
- Use the ``explain`` subcommand to understand the purpose of different subcommands and their options.

Before getting started, ensure that **Tadah!** is installed and accessible through your ``$PATH``. If it is not, either add it to your ``$PATH`` or use the full path to the Tadah! binary in your commands.

.. note::

   If you need installation instructions or system prerequisites, refer to :ref:`installation`.

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

Tadah! supports multiple levels of subcommands for better organization and clarity:

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

Tadah! offers an ``explain`` subcommand to help you understand the purpose of each subcommand and its options. The ``explain`` subcommand takes a single argument:

- The argument represents a query about either a particular (sub)command or an option.
- It prints an extended description, including usage, purpose, and any additional helpful information.
- To specify which subcommand or option to explain use a *dot-separated* format. Each dot separates the subcommand(s), and the specific option of interest.

For example, to learn about the ``train`` command’s ``--task`` option:

.. code-block:: bash

   tadah explain train.task

For a second-level subcommand, such as ``tadah data split``:

.. code-block:: bash

   tadah explain data.split

And to learn more about the ``--dbfile`` option of ``tadah data split``:

.. code-block:: bash

   tadah explain data.split.dbfile

Runtime Errors
..............

Tadah! provides helpful runtime errors, such as when a configuration file is missing a necessary key. While we strive for reliability, no code is entirely bug-free. If you encounter any issues, please report them on **Tadah!’s GitLab**:

- `Tadah! GitLab Issues <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues>`_

