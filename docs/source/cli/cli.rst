Command Line Interface
======================

Tadah!MLIP’s CLI follows one simple pattern; once you know the
conventions you can drive every tool—from dataset wrangling to
hyper-parameter optimisation.

Basic invocation
----------------

.. code-block:: console

   tadah <command> [<subcommand>] [OPTIONS]

* *command* – one of the top-level verbs listed in the map below
  (``analysis``, ``data`` …).
* *subcommand* – some commands are families that expose specialist
  sub-tools, e.g. ``analysis bfunc`` or ``data dedup``.  Commands
  without children omit this level.
* *OPTIONS* – long keys start with ``--``; single-letter aliases start
  with ``-``.  Boolean switches are always written in **CAPS** for
  their short form (``-F``, ``-S``, ``-A`` …).

Tasks vs. direct CLI
--------------------

Anything that can be written on the command line can be placed inside a
task file and executed with ``--task tasks.tadah``.  
A task file is simply the CLI translated into an INI-style block:

.. code-block:: ini

   # global options
   NUMERIC 14
   VERBOSE 2

   TASK predict
   DBFILE   data.tadah
   FORCE    true
   ANALYTICS true
   ...

Short-hand rules
----------------

* Boolean flags never take a value on the CLI: ``--force`` ✓  
  ``--force true`` ✗
* Lists accept space-separated tokens **or**
  comma/range syntax (``1,3-5,10-20:2``).
* Mutually exclusive options are flagged in the help (``Excludes``);
  Tadah!MLIP aborts if you combine them.
* Any multi-value option can be repeated:
  ``--dbfile file1 file2`` or ``--dbfile file1 --dbfile file2``.

Top-level command map
---------------------

==================  ============================================================
``analysis``        Visualise basis / cutoff functions or compute descriptors

                    * ``descriptor`` – calculate structure descriptors  
                    * ``bfunc`` – evaluate/plot basis functions  
                    * ``cutoff`` – evaluate/plot cutoff functions  

``data``            Create & manipulate datasets

                    * ``convert`` – DFT → Tadah! format  
                    * ``print`` – echo structure info  
                    * ``write`` – export structures  
                    * ``dedup`` – remove duplicates from a database
                    * ``merge`` – concatenate databases  
                    * ``split`` – partition a database  
                    * ``sample`` – sample a database  
                    * ``balance`` – re-weight / shift energies & forces  

``explain``         Detailed help for any command/option (dot-notation)

``hpo``             Global hyper-parameter optimisation of a model

``predict``         Run a trained potential on datasets or raw structures

``properties``      Evaluate benchmark properties

                    * ``pairwise`` – energy vs. distance curves  

``train``           Fit a new potential (``-c config.tadah``)
==================  ============================================================

Commands reference
------------------

.. include:: cli/cli_auto_gen.rst
