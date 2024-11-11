.. Tadah! documentation master file, created by
   sphinx-quickstart on Sat Aug 14 23:30:23 2021.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to Tadah!'s documentation!
===================================

.. exec_code::

    import os
    current_directory = os.getcwd()
    print(current_directory)
    print(os.listdir(current_directory)+"/source")

.. Generate documentation with config KEY/VALUE pairs
.. exec_code::
    :hide_code:
    :filename: config/read_toml.py

.. exec_code::

    import os
    current_directory = os.getcwd()
    print(current_directory)
    print(os.listdir(current_directory)+"/source")

.. toctree::
   :maxdepth: 2
   :caption: General

   intro.rst
   quickstart.rst
   config/config_keys.rst
   glossary.rst
   examples.rst
   contribute.rst
   LICENCE.rst

.. raw:: html

   <hr>

.. toctree::
   :maxdepth: 2
   :caption: Getting Started


.. raw:: html

   <hr>

.. toctree::
   :maxdepth: 2
   :caption: API

   atom.rst
   structure.rst
   structure_db.rst
   st_descriptors.rst
   st_descriptors_db.rst
   nn_finder.rst
   descriptors_calc.rst
   cutoffs.rst
   config.rst
   descriptors.rst
   kernels.rst
   basisfunctions.rst
   models.rst
   design_matrix.rst
   func_base.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
