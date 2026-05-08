Welcome to the Tadah! Website
=============================

.. image:: images/tadah_cli.png
   :align: center

.. admonition:: Heads-up – Tadah! 1.0.0 has landed 🎉
   :class: warning

   We have just tagged **the first official public release, v1.0.0**, and the
   *main* branch has already been fast-forwarded to this version.  
   The documentation you are reading is being updated in step, but you may still
   encounter pages that lag behind the code, broken links, or typos while we
   finish the polish.

   New releases inevitably bring teething issues; if something looks off,
   crashes, or is simply confusing, please let us know.  
   A short issue or e-mail is far more helpful than silent frustration and we'll
   gladly guide you through any rough edges.

   Thank you for your patience, happy modelling, and welcome to Tadah! 1.0.0.


This is the home of Tadah! software, used for developing and deploying machine learning interatomic potentials. This website provides documentation with examples to help users quickly get up to speed, along with an associated API for developers.

Here we also publish selected interatomic potentials developed by |gja_link| using Tadah! software.

.. |gja_link| raw:: html

   <a href="https://www.ph.ed.ac.uk/people/graeme-ackland" target="_blank" style="text-decoration:none;">
       <span style="vertical-align:middle;">Prof. Ackland’s group</span> 
       <span style="font-size:small; vertical-align:middle;">↗️</span>
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
   regressors.rst
   descriptors.rst
   cutoffs.rst
   hpo.rst
   hpo_examples.rst
   bound_validation.rst

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
