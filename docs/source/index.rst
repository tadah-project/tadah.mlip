Welcome to the Tadah! Website
=============================

.. image:: images/tadah_cli.png
   :align: center

.. admonition:: Tadah! |release| — work in progress, ready to use
   :class: note

   This release is a **usable work in progress**: it is the version we fit
   and run potentials with, and we want your feedback on it. The CLI
   (subcommands, flags, configuration keys) and the ``pot.tadah`` format
   are stable across 1.x; the C++ API is **not** yet stable and may change
   before 2.0. Highlights since the 1.3 beta:

   * **Nested fitting** (``tadah hpo``) — new optimisers: simulated
     annealing (``TADAH:ANNEAL`` with Corana steps), parallel tempering
     (``ANNEAL_PT``) and population annealing (``ANNEAL_PA``), Ingber's
     Adaptive Simulated Annealing (``LIB ASA``) and Ceres gradient solvers;
     a unit-cube search-space scaling layer (``OPTIM_SCALE AUTO``,
     ``OPTIM ... AUTO`` boxes); per-dataset validation targets and weights;
     graded ``FAILSCORE`` and process-isolated ``PC_LAMMPS`` scripts.
   * **Refit** (``tadah refit``) — convert tabulated EAM potentials into
     native Tadah! potentials, with a fast EAM-collapse path in LAMMPS.
   * **Descriptors** — rotationally invariant power-spectrum family
     (``DM_PS_*``, ``DM_GPS_*``, ``DM_OW_*``) and bump-support checks for
     blip channels.
   * **Data** — load-time dataset transforms: ``LSCALE`` and the resolved
     ``ESHIFT`` are part of the potential and apply in ``tadah predict``,
     ``tadah hpo`` and a retrain; ``EFILTER``/``FFILTER``, ``WDBFILE``, …
     prepare the training data, are kept in the potential's training record
     and apply to prediction only when given to ``tadah predict``
     (:ref:`load_transforms_predict`).

   Upgrading from 1.4 or earlier? Read the breaking changes in the
   :doc:`changelog` first — notably the ``ON``/``OFF``/``AUTO`` vocabulary
   for optimiser ``PARAM`` keys and 1-based indices in HPO output. Please
   `report any issues`_ you hit.

   .. _report any issues: https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/issues


This is the home of Tadah! software, used for developing and deploying machine learning interatomic potentials. This website provides documentation with examples to help users quickly get up to speed, along with an associated API for developers.

Here we also publish selected interatomic potentials developed by |gja_link| using Tadah! software.

.. |gja_link| raw:: html

   <a href="https://www.ph.ed.ac.uk/people/graeme-ackland" target="_blank" style="text-decoration:none;">
       <span style="vertical-align:middle;">Prof. Ackland's group</span>
   </a>


.. seealso:: 
   For our interatomic potentials, see :ref:`mlips`.

.. toctree::
   :maxdepth: 2
   :caption: General Information

   intro.rst
   installation.rst
   mlips.rst
   contribute.rst
   glossary.rst
   changelog.md
   LICENSE.rst

----

.. toctree::
   :maxdepth: 2
   :caption: How to Use Tadah!

   quickstart.rst
   cli/cli.rst
   tutorials.rst
   examples.rst
   config/config_keys.rst
   training.rst
   regressors.rst
   descriptors.rst
   dm_embed.rst
   cutoffs.rst
   refit.rst

----

.. toctree::
   :maxdepth: 2
   :caption: Nested Fitting
   :titlesonly:

   nfp_background.rst
   nfp_quickstart.rst
   nfp_configuration.rst
   nfp_block_keys.rst
   nfp_optimizer.rst
   nfp_reference.rst
   hpo_examples.rst
   nfp_review_pending.rst

----

.. toctree::
   :maxdepth: 2
   :caption: API
   :hidden:

   API_quickstart.rst
   API_examples.rst

.. toctree::
   :maxdepth: 2
   :caption: API Reference
   :hidden:

   API/atom.rst
   API/basisfunctions.rst
   API/config.rst
   API/cutoffs.rst
   API/descriptors.rst
   API/descriptors_calc.rst
   API/design_matrix.rst
   API/func_base.rst
   API/kernels.rst
   API/models.rst
   API/nn_finder.rst
   API/structure.rst
   API/structure_db.rst
   API/st_descriptors.rst
   API/st_descriptors_db.rst
