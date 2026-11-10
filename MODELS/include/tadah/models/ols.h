#ifndef TADAH_MODELS_OLS_H
#define TADAH_MODELS_OLS_H

#include <tadah/models/algorithm.h> // Include the Algorithm enum
#include <tadah/models/factor_handle.h>
#include <tadah/models/memory/imodels_workspace_manager.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace tadah {
namespace models {

/**
 * @brief OLS found the design matrix rank-deficient: DGELS (Algorithm::GELS)
 * stopped at a zero diagonal element of R, or DPOTRF
 * (Algorithm::NormalEquations) at a leading minor of A^T A that is not
 * positive definite.
 *
 * column() is that column (1-based), the order of that minor; what() says
 * what it means. LinearRegressor::train adds the fit's numbers and the
 * remedies.
 */
class RankDeficientError : public std::runtime_error {
public:
  RankDeficientError(const std::string &what, int column)
      : std::runtime_error(what), column_(column) {}
  /// The column of the zero diagonal element of R / the order of the minor.
  int column() const noexcept { return column_; }

private:
  int column_;
};

/**
 * @class OLS
 * @brief Provides functionality for solving Ordinary Least Squares (OLS) problems.
 *
 * The OLS class implements various algorithms to solve linear systems where
 * the goal is to minimize the Euclidean norm of residuals.
 *
 * **Algorithms:**
 * - **GELS (`DGELS`):** Uses QR or LQ factorization, fast but less robust for ill-conditioned systems.
 * - **GELSD (`DGELSD`):** Uses Singular Value Decomposition (SVD) approach, handling rank-deficient and ill-conditioned systems; suitable for both overdetermined and underdetermined systems.
 * - **Pseudoinverse:** Custom implementation using SVD to compute the pseudoinverse; allows for explicit control over singular values and regularization; suitable for underdetermined systems.
 * - **NormalEquations:** Solves the normal equations \f$ A^\top A x = A^\top b \f$ using Cholesky decomposition.
 */
class OLS {
public:
    /**
     * @brief Overload the << operator for the Algorithm enum class.
     *
     * @param os The output stream.
     * @param algo The algorithm to output.
     * @return Reference to the output stream.
     */
    friend std::ostream& operator<<(std::ostream& os, Algorithm algo);

    /**
     * @brief Default constructor.
     *
     * Creates an instance of OLS with its own workspace manager.
     */
    OLS();

    /**
     * @brief Constructor with external workspace manager.
     *
     * @param workspaceManager Reference to an external IModelsWorkspaceManager.
     */
    explicit OLS(memory::IModelsWorkspaceManager& workspaceManager);

    /**
     * @brief Destructor.
     *
     * Deletes the workspace manager if owned.
     */
    ~OLS();

    /**
     * @brief Solve the OLS problem using the default algorithm (GELSD).
     *
     * @tparam M Type of the matrix A.
     * @tparam V Type of the vectors B and weights.
     * @param A The matrix A.
     * @param B The vector B.
     * @param weights Vector to store the solution weights.
     */
    template <typename M, typename V>
    void solve(M& A, V& B, V& weights);

    /**
     * @brief Solve the OLS problem using the specified algorithm.
     *
     * @tparam M Type of the matrix A.
     * @tparam V Type of the vectors B and weights.
     * @param A The matrix A.
     * @param B The vector B.
     * @param weights Vector to store the solution weights.
     * @param rcond Reciprocal condition number; used to determine effective rank.
     * @param algorithm The algorithm to use for solving.
     */
    template <typename M, typename V>
    void solve(M& A, V& B, V& weights, double rcond, Algorithm algorithm);

    /**
     * @brief Solve the OLS problem using a precomputed SVD (Pseudoinverse method).
     *
     * @tparam V1 Type of the vector B.
     * @tparam V2 Type of the vector weights.
     * @param m Number of rows in the matrix A.
     * @param n Number of columns in the matrix A.
     * @param B The vector B.
     * @param weights Vector to store the solution weights.
     * @param svdWorkspace Reference to an SVDWorkspace containing precomputed SVD.
     * @param rcond Reciprocal condition number; used to determine effective rank (default is -1.0).
     */
template <typename V1, typename V2>
void solveWithPseudoinverse(int m, int n, V1& B, V2& weights, memory::SVDWorkspace& svdWorkspace, double rcond, double lambda);

    /**
     * @brief The singular-value threshold of solveWithPseudoinverse: a
     *        direction i with S[i] <= the threshold is left out of the
     *        solution (its weight component is 0).
     *
     * rcond * max(S[0..k-1]) for rcond > 0, else 0 (only exact zeros are
     * left out). LinearRegressor::dropped_directions counts the directions it
     * leaves out.
     *
     * @param S The k = min(m, n) singular values.
     */
    static double pseudoinverse_threshold(const double *S, int k, double rcond) {
      if (k <= 0 || !(rcond > 0.0)) return 0.0;
      double smax = S[0];
      for (int i = 1; i < k; ++i) smax = std::max(smax, S[i]);
      return rcond * smax;
    }
template <typename V1, typename V2>
void solveWithPseudoinverse(int m, int n, V1& B, V2& weights, memory::SVDWorkspace& svdWorkspace);

    /**
     * @brief Solve the OLS problem via NormalEquations and cache the
     *        Cholesky factor for IFT back-substitution.
     *
     * Runs the same `dsyrk_ + dgemv_ + dpotrf_ + dpotrs_` sequence as
     * the OALGO=4 path of @ref solve, additionally copying the
     * factorised (uplo='U') AtA into @p out_factor.AtA so the caller
     * can run any number of subsequent back-substitutions via
     * @ref LinearRegressor::backsolve without re-factorising.
     *
     * Only NormalEquations is supported. Pseudoinverse / GELS / GELSD
     * paths require an SVD-based IFT, which is not implemented.
     *
     * @tparam M Type of the matrix A.
     * @tparam V Type of the vectors B and weights.
     * @param A The (possibly ridge-augmented) design matrix.
     * @param B The (possibly ridge-augmented) target vector.
     * @param weights Vector to receive the OLS solution.
     * @param out_factor Handle to be populated with the cached factor.
     * @param n_bias Number of unregularised intercept columns
     *               (recorded into the handle).
     */
    template <typename M, typename V>
    void solveAndCacheFactor(M& A, V& B, V& weights,
                             FactorHandle& out_factor,
                             std::size_t n_bias = 0);

private:
    memory::IModelsWorkspaceManager* workspaceManager_; ///< Pointer to the workspace manager.
    bool ownsWorkspaceManager_; ///< Indicates if this instance owns the workspace manager.

    // Internal implementation methods

    /**
     * @brief Internal implementation of the solve method.
     *
     * @tparam M Type of the matrix A.
     * @tparam V Type of the vectors B and weights.
     * @param A The matrix A.
     * @param B The vector B.
     * @param weights Vector to store the solution weights.
     * @param rcond Reciprocal condition number; used to determine effective rank.
     * @param algorithm The algorithm to use for solving.
     */
    template <typename M, typename V>
    void solveImpl(M& A, V& B, V& weights, double rcond, Algorithm algorithm);

};

} // namespace models
} // namespace tadah

#include <tadah/models/ols_impl.h> // Include implementation

#endif // TADAH_MODELS_OLS_H

