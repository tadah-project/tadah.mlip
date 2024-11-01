.. _examples:

Examples
========

.. include:: ../../examples/CLI/ex_1/README.rst

API Examples
------------

Examples of using the C++ Tadah! library.

.. _ex1:

Example 1 - Traininig Process and Simple Prediction
...................................................
.. doxygenfile:: ex1.cpp
   :no-link:

Example 1 c++ file:

.. literalinclude:: ../../examples/API/ex_1/ex1.cpp
   :language: c++

Config file used for training:

.. literalinclude:: ../../examples/API/ex_1/config
   :language: bash

Config file used for prediction:

.. literalinclude:: ../../examples/API/ex_1/config_pred
   :language: bash

.. _ex2:

Example 2 - Prediction using existing model
...........................................
.. doxygenfile:: ex2.cpp
   :no-link:

Example 2 c++ file:

.. literalinclude:: ../../examples/API/ex_2/ex2.cpp
   :language: c++

Trained model used for prediction:

.. literalinclude:: ../../examples/API/ex_2/pot.tadah
   :language: bash

Config file used for prediction:

.. literalinclude:: ../../examples/API/ex_2/config_pred
   :language: bash
