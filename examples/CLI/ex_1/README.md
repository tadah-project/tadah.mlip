# Example 1 - Training Process and Simple Prediction

This example demonstrates how to train a model using the Tadah! software. The `config.train` file defines the necessary parameters for the model.

To use this example, ensure you have access to all required files available in the Git repository at [https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/edit/main/examples/CLI/ex_1](https://git.ecdf.ed.ac.uk/tadah/tadah.mlip/-/edit/main/examples/CLI/ex_1)

## Training

To train the model, use the following command:

```bash
tadah train -c config.train -V
```

- `-V`: Enables verbosity for detailed output during the training process.

## Prediction

Once the model is trained, you can run predictions on the same dataset. Note: this approach is generally not recommended but is used here to illustrate the functionality of the Tadah! software.

Use the following command to predict:

```bash
tadah predict -p pot.tadah -d tdata.db -aV
```

- `-a`: Generates useful statistics.
- `-V`: Enables verbosity for detailed output during prediction.

## Important Notes

- The dataset contains over 10,000 structures, so the process may take a while when using the `-F` or `-S` flags to train or predict with forces and stresses, respectively.
