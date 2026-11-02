Welcome to the Tadah! Website
=============================

.. image:: images/tadah_cli.png
   :align: center

.. admonition:: Tadah! |release|
   :class: note

   This release is ready to use: the Tadah! authors fit and run potentials
   with this version, and feedback on it is welcome. The command-line
   interface and the ``pot.tadah`` format are stable across 1.x, except for
   the breaking changes listed in the :doc:`changelog`. The C++ API is
   **not** stable and may change before 2.0.

   The :doc:`changelog` lists the changes of this release. An upgrade starts
   with its breaking changes, notably the error scales and priorities of a
   fit, which replace ``EWEIGHT``, ``FWEIGHT`` and ``SWEIGHT``
   (:ref:`fit_objective`), and, from 1.4 or earlier, the ``ON``/``OFF``/``AUTO``
   vocabulary for optimiser ``PARAM`` keys and 1-based indices in the output
   of ``tadah hpo``.
   `Issue reports <report any issues_>`_ are welcome.

   .. _report any issues: https://github.com/tadah-project/tadah.mlip/issues


This is the home of Tadah! software, used for developing and deploying machine learning interatomic potentials. This website provides documentation with examples to help users quickly get up to speed, along with an associated API for developers.

This site also publishes selected interatomic potentials developed by |gja_link| using Tadah! software, listed in :ref:`mlips`.

.. |gja_link| raw:: html

   <a href="https://www.ph.ed.ac.uk/people/graeme-ackland" target="_blank" style="text-decoration:none;">
       <span style="vertical-align:middle;">Prof. Ackland's group</span>
   </a>


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
   fit_objective.rst
   fit_balance.rst
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
   nfp/reference/index.rst

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
