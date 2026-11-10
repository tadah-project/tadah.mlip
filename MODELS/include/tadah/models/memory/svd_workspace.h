#ifndef TADAH_MODELS_MEMORY_SVDWORKSPACE_H
#define TADAH_MODELS_MEMORY_SVDWORKSPACE_H

namespace tadah {
namespace models {
namespace memory {

/**
 * @class SVDWorkspace
 * @brief Manages memory allocations for SVD computations.
 *
 * The SVDWorkspace class handles the memory required for Singular Value Decomposition
 * computations, ensuring efficient memory usage and reusability.
 */
class SVDWorkspace {
public:
    /**
     * @brief Constructor.
     */
    SVDWorkspace();

    /**
     * @brief Destructor.
     *
     * Releases all allocated memory.
     */
    ~SVDWorkspace();

    /**
     * @brief Allocate workspace for the specified problem size.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     */
    void allocate(int m_, int n_);

    /**
     * @brief Check if the workspace is sufficient for the given problem size.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     * @return True if the workspace is sufficient, false otherwise.
     */
    bool isSufficient(int m_, int n_) const;

    // Member variables for SVD computations
    int m; ///< Number of rows in the matrix.
    int n; ///< Number of columns in the matrix.

    double* U;   ///< U matrix from SVD (m x min(m,n)).
    double* S;   ///< Singular values from SVD (min(m,n)).
    double* VT;  ///< V^T matrix from SVD (min(m,n) x n).

    // Workspace variables
    double* work; ///< Workspace array for computations.
    int lwork;    ///< Size of the work array.
    int* iwork;   ///< Integer workspace array.

private:
    // Disable copying
    SVDWorkspace(const SVDWorkspace&) = delete;
    SVDWorkspace& operator=(const SVDWorkspace&) = delete;
};

} // namespace memory
} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_MEMORY_SVDWORKSPACE_H

