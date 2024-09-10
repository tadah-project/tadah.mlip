## Introduction

**Ta-dah!** is a fast and modular machine learning software and C++ library specifically designed for developing interatomic potentials. Written in modern C++, it aims to offer an easy-to-use, modular, and extensible state-of-the-art toolkit.

Ta-dah! provides a LAMMPS interface compatible with all supplied descriptors and models. Users can operate it from the command line to train models or make predictions using pre-existing machine learning potentials, or incorporate it as a C++ library for more advanced applications.

## What are Machine Learning Interatomic Potentials?

Machine learning interatomic potentials (MLIPs) are computational models that predict the energy and forces within a system of atoms based on their positions. Traditional potentials often rely on simplified physical models, which can be limited in accuracy and flexibility. In contrast, MLIPs leverage machine learning to learn complex relationships from large datasets of atomic configurations, providing a more accurate and flexible approach to modeling atomic interactions.

### Advantages of MLIPs

- **Accuracy**: Capable of capturing complex physical interactions that traditional potentials might miss.
- **Efficiency**: Once trained, MLIPs can predict energies and forces much faster than ab initio calculations.
- **Transferability**: MLIPs can be trained on diverse datasets, making them applicable to a wide range of materials and conditions.

### Applications

- **Materials Science**: Predicting the properties of new materials.
- **Chemistry**: Modeling reactions and molecular dynamics.
- **Physics**: Studying condensed matter systems.

## Why Use Ta-dah!?

- **Community Driven**: New ideas are always welcomed and implemented if feasible.
- **Speed**: Accelerates the model development cycle, significantly reducing waiting times.
- **Continuous Improvement**: Regularly updated with new descriptors, models, bug fixes, and issue resolutions.
- **Open Source**: Freely available for community use and contribution.
- **Flexibility**: Supports the combination of various descriptors with different cutoffs and models. Trained models can be tested directly with LAMMPS.
- **Extensibility**: Easily extendable to include new descriptors, which will be compatible with existing code and the LAMMPS interface.

## Requirements

Ta-dah! does not require any external libraries for building or downloading.

## Obtaining Ta-dah!

The source code is hosted at:

[https://git.ecdf.ed.ac.uk/tadah/](https://git.ecdf.ed.ac.uk/tadah/)

To download, use git and clone from the stable branch:

```sh
git clone -b stable https://git.ecdf.ed.ac.uk/tadah/tadah.git
```

## Installation

Ta-dah! uses CMake to manage the configuration and compilation process. CMake will determine system-dependent variables based on the `CMakeLists.txt` file in the project root directory.

1. Navigate to the project directory:
   ```sh
   cd ta-dah
   ```
2. Create and navigate to a build directory:
   ```sh
   mkdir build && cd build
   ```
3. Configure with CMake:
   ```sh
   cmake ..
   ```
4. Compile and install:
   ```sh
   make && make install
   ```

To change the default library installation location, use the following command instead of `cmake ..`:

```sh
cmake .. -DCMAKE_INSTALL_PREFIX=/your/path
```

This is useful when you lack root privileges.

## Binary File

The executable file `ta-dah` will be installed in the `bin` directory within your chosen installation location. On most UNIX systems, the default path is `/usr/local/bin/ta-dah`.

If using the `-DCMAKE_INSTALL_PREFIX=/your/path` flag, the binary file will be located at `/your/path/bin/ta-dah`.

This concludes the installation process for most users. If you intend to use Ta-dah! as a C++ library, please continue to the next section.

## Using Ta-dah!

To use Ta-dah! from the command line:

1. **Train models:**
   ```sh
   ta-dah train -d <descriptor> -m <model> -i <input_file> -o <output_model>
   ```
   Replace `<descriptor>`, `<model>`, `<input_file>`, and `<output_model>` with your specific choices.

2. **Use pre-trained models for predictions:**
   ```sh
   ta-dah predict -m <model_file> -i <input_file> -o <output_file>
   ```
   Replace `<model_file>`, `<input_file>`, and `<output_file>` with your specific choices.

For more detailed usage and advanced options, please refer to the official documentation or the help command:

```sh
ta-dah --help
```

## Including Ta-dah! as a C++ Library

To use Ta-dah! as a C++ library in your project, you can include the necessary headers and link against the Ta-dah! library. Modify your project's `CMakeLists.txt` to find and link Ta-dah!:

```cmake
find_package(TaDah REQUIRED)
target_link_libraries(your_project_name PRIVATE TaDah)
```

Ensure the installation path of Ta-dah! is included in `CMAKE_PREFIX_PATH`:

```sh
cmake -DCMAKE_PREFIX_PATH=/path/to/ta-dah ..
```

For further instructions and examples, please consult the official Ta-dah! documentation and examples included in the repository.

---

For any questions, contributions or issues, please visit the [Ta-dah! repository](https://git.ecdf.ed.ac.uk/tadah/).
