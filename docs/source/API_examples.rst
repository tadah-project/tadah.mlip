.. _api_examples:

API Examples
============

This page shows two C++ programs that use the Tadah! library: one trains a
potential and predicts with it, the other predicts with an existing
potential. Building Tadah! is described in :doc:`installation`; the format of
the training configuration is described in :ref:`config_file`.

.. _ex1:

Example 1: training and a simple prediction
-------------------------------------------

Example 1 c++ file:

.. literalinclude:: ../../examples/API/ex_1/ex1.cpp
   :language: c++

Training configuration of Example 1:

.. literalinclude:: ../../examples/API/ex_1/config
   :language: bash

Config file used for prediction:

.. literalinclude:: ../../examples/API/ex_1/config_pred
   :language: bash

.. _ex2:

Example 2: prediction with an existing potential
------------------------------------------------

Example 2 c++ file:

.. literalinclude:: ../../examples/API/ex_2/ex2.cpp
   :language: c++

Trained model used for prediction:

.. literalinclude:: ../../examples/API/ex_2/pot.tadah
   :language: bash

Config file used for prediction:

.. literalinclude:: ../../examples/API/ex_2/config_pred
   :language: bash
