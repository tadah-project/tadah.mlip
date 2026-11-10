#ifndef TADAH_MODELS_ALGORITHM_H
#define TADAH_MODELS_ALGORITHM_H

namespace tadah {
namespace models {

/**
 * @enum Algorithm
 * @brief Enum to select the algorithm used for solving computational problems.
 *
 * This enum is used by various classes (e.g., OLS) to specify the algorithm to be used.
 *
 * **Algorithms:**
 * - **GELS (`DGELS`):** Uses QR or LQ factorization, fast but less robust for ill-conditioned systems; suitable for overdetermined systems.
 * - **GELSD (`DGELSD`):** Uses Singular Value Decomposition (SVD) approach, handling rank-deficient and ill-conditioned systems; suitable for both overdetermined and underdetermined systems.
 * - **Pseudoinverse:** Custom implementation using SVD to compute the pseudoinverse; allows for explicit control over singular values and regularization; suitable for underdetermined systems.
 * - **NormalEquations:** Solves the normal equations \f$ A^\top A x = A^\top b \f$ using Cholesky decomposition.
 */
enum class Algorithm {
    GELS,           ///< Uses DGELS
    GELSD,          ///< Uses DGELSD
    Pseudoinverse,  ///< Uses custom SVD-based pseudoinverse implementation
    NormalEquations ///< Uses Cholesky decomposition to solve normal equations
};

} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_ALGORITHM_H

