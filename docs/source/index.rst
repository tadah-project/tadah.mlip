Welcome to the Tadah! Website
=============================

.. image:: images/tadah_cli.png
   :align: center

.. admonition:: Tadah! 1.4.0
   :class: note

   The CLI surface (subcommand names, flags, configuration-file keys,
   and the ``pot.tadah`` file format) is considered stable for the 1.x
   series; the C++ API (``libtadah.*`` headers and class layouts) is
   **not yet stable** and may change between minor versions until 2.0.
   See the `changelog`_ for what shipped in this release, the
   `migration notes`_ for upgrade caveats from earlier releases, and
   please `report any issues`_ you hit.

   .. _changelog: https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/blob/main/CHANGELOG.md
   .. _migration notes: https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/blob/main/CHANGELOG.md#migrating-from-v120-to-130-beta1
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
   cutoffs.rst
   hpo.rst
   hpo_examples.rst
   refit.rst

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
