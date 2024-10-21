# Massively Parallel version of Tadah!MLIP

This project is currently under development. The goal is to enable the training of MLIPs using large datasets distributed across numerous HPC (supercomputer) nodes via MPI. The optimization of weights is performed using ScaLAPACK, facilitating efficient and scalable model training.

# For developers

- use develop branch
- Make use of CMake FETCHCONTENT_SOURCE_DIR_<uppercaseName> such that it points to the local copy of a MODULE:
  
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.LIBS=/full/local/path/to/TADAH/LIBS
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=/full/local/path/to/TADAH/CORE
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=/full/local/path/to/TADAH/MODELS
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=/full/local/path/to/TADAH/MLIP
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MD=/full/local/path/to/TADAH/MD
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=/full/local/path/to/TADAH/HPO