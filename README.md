# Massively Parallel version of Tadah!MLIP

This project is currently under development. The goal is to enable the training of MLIPs using large datasets distributed across numerous HPC (supercomputer) nodes via MPI. The optimization of weights is performed using ScaLAPACK, facilitating efficient and scalable model training.

# For developers

- use develop branch
- Make use of CMake FETCHCONTENT_SOURCE_DIR_<uppercaseName> such that it points to the local copy of a MODULE:
  
    - TADAH_PATH=...
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${TADAH_PATH}/CORE
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=${TADAH_PATH}/MODELS
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=${TADAH_PATH}/MLIP
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MD=${TADAH_PATH}/MD
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=${TADAH_PATH}/HPO