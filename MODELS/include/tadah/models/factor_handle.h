#ifndef TADAH_MODELS_FACTOR_HANDLE_H
#define TADAH_MODELS_FACTOR_HANDLE_H

#include <tadah/models/algorithm.h>

#include <cstddef>
#include <vector>

namespace tadah {
namespace models {

/**
 * @class FactorHandle
 * @brief Cached LAPACK factorisation of (Aug(Phi))^T Aug(Phi) for IFT
 *        back-substitution by HPO analytical / hybrid gradients.
 *
 * Owns a copy of the upper-triangular Cholesky factor `U` of the
 * normal-equations matrix, where U^T U = Phi_aug^T Phi_aug. For ridge
 * regression with fixed LAMBDA, this is equivalent to (Phi^T Phi + λ
 * I_no_bias) where the intercept columns (the first `n_bias`) are
 * unregularised.
 *
 * Only `Algorithm::NormalEquations` (OALGO=4) is supported; other
 * algorithms throw at construction time. SVD-based IFT (for OALGO=1/2/3)
 * is not implemented.
 *
 * Lifetime: the handle owns its own buffer (std::vector<double>), so
 * the workspace that produced it can be freely reused / reallocated
 * for subsequent train() calls without invalidating the handle.
 *
 * @see LinearRegressor::train_with_factor
 * @see LinearRegressor::backsolve
 */
struct FactorHandle {
  /** Cholesky factor stored in column-major upper-triangular form
   *  (LAPACK dpotrf_ output with uplo='U'). Size n*n. The lower
   *  triangle is left as-is from `dsyrk_`'s Hermitian fill (LAPACK
   *  doesn't overwrite it; we don't read it either). */
  std::vector<double> AtA;

  /** Dimension n = number of regression weights. */
  int n = 0;

  /** Number of unregularised intercept columns (leading columns of
   *  Phi). Used by the IFT chain rule when assembling dΛ/dθ — for
   *  ridge with fixed LAMBDA, dΛ/dθ is zero, but the bias-aware
   *  augmentation pattern is recorded for future-proofing. */
  std::size_t n_bias = 0;

  /** Algorithm used to produce the factor. Only NormalEquations is
   *  supported. */
  Algorithm algo = Algorithm::NormalEquations;

  /** True after a successful solveAndCacheFactor; false on default-
   *  constructed handles or when invalidated (e.g. after a descriptor
   *  size change in HPO). */
  bool valid = false;

  /** Reset to default state. Cheap — does not deallocate the vector. */
  void invalidate() { valid = false; }
};

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_FACTOR_HANDLE_H
