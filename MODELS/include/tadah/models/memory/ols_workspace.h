#ifndef TADAH_MODELS_MEMORY_OLSWORKSPACE_H
#define TADAH_MODELS_MEMORY_OLSWORKSPACE_H

#include <tadah/models/algorithm.h> // For Algorithm

namespace tadah {
namespace models {
namespace memory {

/**
 * @class OLSWorkspace
 * @brief Manages memory allocations for OLS computations.
 *
 * The OLSWorkspace class handles the memory required for various OLS
 * algorithms. It ensures that memory is allocated and deallocated appropriately
 * and provides storage for intermediate computations.
 */
class OLSWorkspace {
public:
  /**
   * @brief Constructor.
   */
  OLSWorkspace();

  /**
   * @brief Destructor.
   *
   * Releases all allocated memory.
   */
  ~OLSWorkspace();

  /**
   * @brief Allocate workspace for the specified problem size and algorithm.
   *
   * @param m Number of rows in the matrix.
   * @param n Number of columns in the matrix.
   * @param algorithm The OLS algorithm to be used.
   */
  void allocate(int m_, int n_, tadah::models::Algorithm algorithm);

  /**
   * @brief Check if the workspace is sufficient for the given problem size and
   * algorithm.
   *
   * @param m Number of rows in the matrix.
   * @param n Number of columns in the matrix.
   * @param algorithm The OLS algorithm to be used.
   * @return True if the workspace is sufficient, false otherwise.
   */
  bool isSufficient(int m_, int n_,
                    tadah::models::Algorithm algorithm) const;

  // Member variables
  double *work; ///< Workspace array for computations.
  int lwork;    ///< Size of the work array.
  int m;        ///< Number of rows in the matrix.
  int n;        ///< Number of columns in the matrix.
  tadah::models::Algorithm
      algo; ///< Algorithm for which workspace was allocated.

  // Additional variables for specific algorithms

  // For GELSD (DGELSD)
  double *s;  ///< Singular values array.
  int *iwork; ///< Integer workspace array.

  // For internal SVD computation in OLS (when Algorithm::SVD is used)
  double lambda; ///< Regularization parameter.
  double *U;     ///< U matrix from SVD.
  double *S;     ///< Singular values from SVD.
  double *VT;    ///< V^T matrix from SVD.
  int *iworkSVD; ///< Integer workspace array for SVD.
  double *UtB;   ///< Intermediate vector for U^T * B.

  // For Cholesky
  double *AtA; ///< tadah::core::Matrix to store A^T * A.
  double *Atb; ///< Vector to store A^T * b.

private:
  // Disable copying
  OLSWorkspace(const OLSWorkspace &) = delete;
  OLSWorkspace &operator=(const OLSWorkspace &) = delete;
};

} // namespace memory
} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_MEMORY_OLSWORKSPACE_H
