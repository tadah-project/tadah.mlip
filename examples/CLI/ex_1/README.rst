Example 1 - Basics of Training and Prediction
---------------------------------------------

This example demonstrates how to train a model using Tadah!MLIP. The ``config.train`` file defines
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

Once the model is trained, you can run predictions on any Tadah!MLIP dataset.
Here we simply reuse the training dataset for demonstration purposes.

.. code-block:: bash

   tadah predict -p pot.tadah -d tdata.db -v 2 -A

- ``-A``: Generates useful analytics such as RMSE and R^2.
- ``-v 2``: Enables verbosity for detailed output during prediction.

Additional Notes
................

- The dataset contains over 10,000 structures, so the process may take a while when using the ``-F`` or 
  ``-S`` flags to train or predict with forces and stresses, respectively.
