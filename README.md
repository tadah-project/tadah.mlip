# Desktop and Massively Parallel version of Tadah!MLIP

This project can be compiled either as desktop or MPI version.

# For developers

- use develop branch
- Make use of CMake FETCHCONTENT_SOURCE_DIR_<uppercaseName> such that it points to the local copy of a MODULE:
  
    - TADAH_PATH=...
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.CORE=${TADAH_PATH}/CORE
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MODELS=${TADAH_PATH}/MODELS
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MLIP=${TADAH_PATH}/MLIP
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.MD=${TADAH_PATH}/MD
    - -DFETCHCONTENT_SOURCE_DIR_TADAH.HPO=${TADAH_PATH}/HPO
