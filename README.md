```markdown
# Ta-dah! Documentation

## Introduction

**Ta-dah!** is a modular and fast machine learning software and C++ library specifically designed for interatomic potential development. Written in modern C++, it aims to provide an easy-to-use, modular, and extensible state-of-the-art toolkit.

Ta-dah! offers a LAMMPS interface compatible with all provided descriptors and models. Users can either operate it from the command line for training models or predictions using pre-existing machine learning potentials or incorporate it as a C++ library for advanced applications.

## Why Use Ta-dah!?

- **Community Driven**: New ideas are always welcome and implemented if feasible.
- **Speed**: Accelerates the model development cycle, reducing waiting times significantly.
- **Continuous Improvement**: Regularly updated with new descriptors, models, bug fixes, and issue resolutions.
- **Open Source**: Freely available for community use and contribution.
- **Flexibility**: Combination of various descriptors with different cutoffs and models is supported. Trained models can be tested directly with LAMMPS.
- **Extensibility**: Easily extendable to include new descriptors, compatible with existing code and LAMMPS interface.

## Requirements

Ta-dah! does not require any external libraries for building or downloading.

## Obtaining Ta-dah!

The source code is hosted at:

[https://git.ecdf.ed.ac.uk/s1351949/ta-dah](https://git.ecdf.ed.ac.uk/s1351949/ta-dah)

To download, use git and clone from the stable branch:

```sh
git clone -b stable https://git.ecdf.ed.ac.uk/s1351949/ta-dah.git
```

## Installation

Ta-dah! uses CMake to manage the configuration and compilation process. CMake will determine system-dependent variables based on the `CMakeList.txt` file in the project root directory.

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

To change the default library installation location, use the following command instead of `cmake ..`. This is useful when you lack root privileges:

```sh
cmake .. -DCMAKE_INSTALL_PREFIX=/your/path
```

## Binary File

The executable file `ta-dah` will be installed in the `bin` directory within your chosen installation location. On most UNIX systems, the default path is `/usr/local/bin/ta-dah`.

If using the `-DCMAKE_INSTALL_PREFIX=/your/path` flag, the binary file will be located at `/your/path/bin/ta-dah`.

This concludes the installation process for most users. If you intend to use Ta-dah! as a C++ library, please continue to the next section.

## Using Ta-dah!

To use Ta-dah! from the command line:

1. Train models:
   ```sh
   ta-dah train -d <descriptor> -m <model> -i <input_file> -o <output_model>
   ```
   Replace `<descriptor>`, `<model>`, `<input_file>`, and `<output_model>` with your specific choices.

2. Use pre-trained models for predictions:
   ```sh
   ta-dah predict -m <model_file> -i <input_file> -o <output_file>
   ```
   Replace `<model_file>`, `<input_file>`, and `<output_file>` with your specific choices.

For more detailed usage and advanced options, please refer to the official documentation or the help command:
```sh
ta-dah --help
```

## Including Ta-dah! as a C++ Library

To use Ta-dah! as a C++ library in your project, you can include the necessary headers and link against the Ta-dah! library. Modify your project's CMakeLists.txt to find and link Ta-dah!:

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
For any questions or contributions, please visit the [Ta-dah! repository](https://git.ecdf.ed.ac.uk/s1351949/ta-dah) or submit an issue.
```
