Command Line Interface
======================

Tadah!MLIP’s CLI follows one simple pattern; the same conventions
drive every tool—from dataset wrangling to hyper-parameter
optimisation.

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
``--task`` takes no other option (``tadah hpo --task tasks.tadah
--hpotarget t.hpo`` is refused, ``--hpotarget excludes --task``): the task
file gives every key of its tasks.
A task file is the CLI translated into an INI-style block:

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

* Boolean flags never take a value on the CLI: the flag is ``--force``,
  not ``--force true``.
* Lists accept space-separated tokens **or**
  comma/range syntax (``1,3-5,10-20:2``).
* Mutually exclusive options are flagged in the help (``Excludes``);
  Tadah!MLIP aborts if they are combined.
* Any multi-value option can be repeated:
  ``--dbfile file1 file2`` or ``--dbfile file1 --dbfile file2``.

Top-level command map
---------------------

==================  ============================================================
``analysis``        Visualises basis / cutoff functions or computes descriptors

                    * ``descriptor`` – calculates structure descriptors  
                    * ``bfunc`` – evaluates/plots basis functions  
                    * ``cutoff`` – evaluates/plots cutoff functions  

``data``            Creates & manipulates datasets

                    * ``convert`` – DFT → Tadah! format  
                    * ``print`` – echoes structure info  
                    * ``write`` – exports structures  
                    * ``dedup`` – removes duplicates from a database
                    * ``merge`` – concatenates databases  
                    * ``split`` – partitions a database  
                    * ``sample`` – samples a database  
                    * ``balance`` – re-weights / shifts energies & forces  

``explain``         Detailed help for any command/option (dot-notation)

``hpo``             Global hyper-parameter optimisation of a model

``predict``         Runs a trained potential on datasets or raw structures

``properties``      Evaluates benchmark properties

                    * ``pairwise`` – energy vs. distance curves  

``train``           Fits a new potential (``-c config.tadah``)
==================  ============================================================

Commands reference
------------------

.. include:: cli_auto_gen.rst
