# Desktop and Massively Parallel Version of Tadah!MLIP

Tadah!MLIP provides the command-line tool `tadah`, which can be used for developing machine learning interatomic potentials.

This project can be compiled as either a desktop or MPI version.

Please refer to the online documentation for installation and usage instructions.

[https://tadah.readthedocs.io/](https://tadah.readthedocs.io/)

## For Developers Only

For the `Develop` build, manual configuration is required to use local copies of `CORE`, `MLIP`, and other modules. This allows you to modify the code locally without needing to commit changes to the GitLab repository. For the `Release` build, all modules will be cloned automatically from the repository. Below configuration is not necessary in this case.

- Add new branch or use `develop` branch.
- Set `-DCMAKE_BUILD_TYPE=Develop` when building.

### Required Directory Structure

Your root `TADAH` directory should contain the following subdirectories (cloned manually from git):

- Tadah.MLIP
- CORE 
- MODELS 
- MLIP 
- HPO

Export the system variable: `TADAH_PATH=/path/to/TADAH`.

### Manual Configuration

Instead of using the Develop build, configure manually by using `CMake`'s `FETCHCONTENT_SOURCE_DIR_<uppercaseName>` to point to the local copy of a module:

```bash
export TADAH_PATH=...
-DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${TADAH_PATH}/CORE
-DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=${TADAH_PATH}/MODELS
-DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=${TADAH_PATH}/MLIP
-DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=${TADAH_PATH}/HPO