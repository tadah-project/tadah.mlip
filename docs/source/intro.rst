.. _introduction:

Introduction
============

Tadah! is a modular C++17 framework for developing and deploying machine
learning interatomic potentials (MLIPs). It ships:

- a command-line driver, ``tadah``, for training, prediction, dataset
  manipulation, and hyperparameter optimisation (``tadah hpo``); and
- a LAMMPS pair style, ``pair_style tadah``, that runs trained
  potentials in MD without a Python or Tadah! runtime dependency.

The two pieces correspond to two repositories — Tadah!MLIP (training
side) and Tadah!LAMMPS (deployment side) — that share a common ``pot.tadah``
file format.

Key features
------------

- **Pluggable descriptors and regressors** — descriptor families
  (2-body, many-body, EAM-style) combine with linear or kernel regressors.
- **Nested fitting (HPO)** — an outer optimiser drives the training loop
  and scores trial potentials against physics-informed constraints
  (elastic constants, equilibrium volume, surface energies, …;
  :ref:`nested_fitting`).
- **LAMMPS interface** — every descriptor and model exposed through
  ``pair_style tadah``; ``LSCALE`` and ``ESHIFT`` round-trip through the
  potential file.
- **OpenMP build** — desktop parallelism via ``OMP_NUM_THREADS``.
  An MPI build (``TADAH_HPO_MPI``) spreads the parallel annealing
  optimisers of ``tadah hpo`` over several nodes
  (:ref:`installation_hpo_mpi`); training itself runs in one process.
- **C++ API** — ``libtadah.mlip`` / ``libtadah.core`` link into other
  programs for embedded use (:ref:`compiling_and_linking`).

CLI surface
-----------

The ``tadah`` driver exposes the following top-level commands (each
documented in :doc:`cli/cli`):

- ``tadah train`` / ``tadah predict`` — fits and applies potentials.
- ``tadah hpo`` — nested fitting (hyperparameter optimisation).
- ``tadah data`` — dataset and structure tools: convert, ``balance``,
  ``dedup``, ``merge``, ``sample``, ``split``, ``print``, ``write``
  (CIF, VASP, CASTEP, LAMMPS; online sources MP/COD/NOMAD via
  ``--structure``).
- ``tadah analysis`` — plots basis functions, cutoffs, descriptors.
- ``tadah properties pairwise`` — pairwise energy between two atoms.
- ``tadah explain <command>.<option>`` — prints the help text of a command
  or of one of its options (``tadah explain refit.refit_mode``), or of any
  configuration key, alone or after a command (``tadah explain SETFL``,
  ``tadah explain refit.rhoform``), each name in any letter case; a
  command's own subcommand or option comes first (``tadah explain
  data.MERGE`` is the merge subcommand, not the ``MERGE`` key), and a name
  that is none of these is an error. :ref:`ConfigSection` describes
  every configuration key.

.. _obtaining_tadah:

Obtaining Tadah!
----------------

Tadah!MLIP and Tadah!LAMMPS are hosted at:

    https://github.com/tadah-project

:ref:`installation` gives the build instructions. Selected published
potentials produced with Tadah! by |gja_link| are listed in
:ref:`mlips`.

.. |gja_link| raw:: html

   <a href="https://www.ph.ed.ac.uk/people/graeme-ackland" target="_blank" style="text-decoration:none;">
       <span style="vertical-align:middle;">Prof. Ackland's group</span>
   </a>
