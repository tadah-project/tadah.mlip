Example 1 - Basic Training and Prediction
-----------------------------------------

This example demonstrates how to train a model using the Tadah! software. The ``config.train`` file defines
the necessary parameters for the model.

To use this example, ensure you have access to all required files available in the Git repository at:
`https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/tree/main/examples/CLI/ex_1 <https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/tree/main/examples/CLI/ex_1>`_

Training
........

To train the model, use the following command:

.. code-block:: bash

   tadah train -c config.train -v 2

- ``-v 2``: Enables verbosity for detailed output during the training process.

Prediction
..........

Once the model is trained, you can run predictions on the same dataset. 

.. note::

   This approach is generally not recommended but is used here to illustrate the functionality of the Tadah! software.

Use the following command to predict:

.. code-block:: bash

   tadah predict -p pot.tadah -d tdata.db -v 2 -A

- ``-A``: Generates useful statistics.
- ``-v 2``: Enables verbosity for detailed output during prediction.

Additional Notes
................

- The dataset contains over 10,000 structures, so the process may take a while when using the ``-F`` or 
  ``-S`` flags to train or predict with forces and stresses, respectively.

