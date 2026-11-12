#ifndef TADAH_MODELS_SVD_H
#define TADAH_MODELS_SVD_H

#include <tadah/models/memory/imodels_workspace_manager.h>
#include <tadah/models/memory/svd_workspace.h>

namespace tadah {
namespace models {

/**
 * @class SVD
 * @brief Provides functionality for computing the Singular Value Decomposition (SVD) of a matrix.
 *
 * The SVD class computes the SVD of a given matrix \f$ A \f$, decomposing it into
 * \f$ A = U \Sigma V^\top \f$, where \f$ U \f$ and \f$ V \f$ are orthogonal matrices,
 * and \f$ \Sigma \f$ is a diagonal matrix containing the singular values.
 *
 * **Usage:**
 * ```cpp
 * tadah::models::SVD svd;
 * svd.compute(A);
 * double* U = svd.getU();
 * double* S = svd.getS();
 * double* VT = svd.getVT();
 * ```
 */
class SVD {
public:
    /**
     * @brief Default constructor.
     *
     * Creates an instance of SVD with its own workspace manager.
     */
    SVD();

    /**
     * @brief Constructor with external workspace manager.
     *
     * @param workspaceManager Reference to an external IModelsWorkspaceManager.
     */
    explicit SVD(memory::IModelsWorkspaceManager& workspaceManager);

    /**
     * @brief Destructor.
     *
     * Deletes the workspace manager if owned.
     */
    ~SVD();

    /**
     * @brief Compute the SVD of a given matrix A.
     *
     * @tparam M Type of the matrix A.
     * @param A The matrix A to decompose.
     *
     * **Example:**
     * ```cpp
     * tadah::core::Matrix A; // Initialize your matrix
     * tadah::models::SVD svd;
     * svd.compute(A);
     * ```
     */
    template <typename M>
    void compute(M& A);

    /**
     * @brief Get the U matrix from the SVD.
     *
     * @return Pointer to the U matrix (size m x min(m, n)).
     */
    double* getU() const;

    /**
     * @brief Get the singular values from the SVD.
     *
     * @return Pointer to the singular values array (size min(m, n)).
     */
    double* getS() const;

    /**
     * @brief Get the V^T matrix from the SVD.
     *
     * @return Pointer to the V^T matrix (size min(m, n) x n).
     */
    double* getVT() const;

    /**
     * @brief Get the SVDWorkspace used in the computation.
     *
     * @return Reference to the SVDWorkspace.
     */
    memory::SVDWorkspace& getSVDWorkspace();

    /**
     * @brief Get the number of rows of the original matrix.
     *
     * @return Number of rows.
     */
    int rows() const;

    /**
     * @brief Get the number of columns of the original matrix.
     *
     * @return Number of columns.
     */
    int cols() const;

private:
    memory::IModelsWorkspaceManager* workspaceManager_; ///< Pointer to the workspace manager.
    bool ownsWorkspaceManager_; ///< Indicates if this instance owns the workspace manager.
    memory::SVDWorkspace* svdWorkspace_; ///< Pointer to the SVDWorkspace.

    int m_; ///< Number of rows in the original matrix.
    int n_; ///< Number of columns in the original matrix.

    // Disable copying
    SVD(const SVD&) = delete;
    SVD& operator=(const SVD&) = delete;
};

} // namespace models
} // namespace tadah

#include <tadah/models/svd_impl.h> // Include implementation

#endif // TADAH_MODELS_SVD_H

