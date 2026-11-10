#ifndef TADAH_MODELS_MEMORY_MODELSWORKSPACEMANAGER_H
#define TADAH_MODELS_MEMORY_MODELSWORKSPACEMANAGER_H

#include <tadah/models/memory/imodels_workspace_manager.h>
#include <tadah/models/algorithm.h>

namespace tadah {
namespace models {
namespace memory {

/**
 * @class ModelsWorkspaceManager
 * @brief Implements the IModelsWorkspaceManager interface to manage workspaces.
 *
 * Manages OLS and SVD workspaces, providing methods to obtain and release them.
 */
class ModelsWorkspaceManager : public virtual IModelsWorkspaceManager {
public:
    /**
     * @brief Constructor.
     */
    ModelsWorkspaceManager();

    /**
     * @brief Destructor.
     *
     * Releases all allocated workspaces.
     */
    ~ModelsWorkspaceManager() override;

    // OLS workspace methods

    /**
     * @brief Get an OLS workspace.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     * @param algorithm The OLS algorithm to be used.
     * @return Pointer to an allocated OLSWorkspace.
     */
    OLSWorkspace* getOLSWorkspace(int m, int n, tadah::models::Algorithm algorithm) override;

    /**
     * @brief Release an OLS workspace.
     *
     * @param workspace Pointer to the OLSWorkspace to be released.
     */
    void releaseOLSWorkspace(OLSWorkspace* workspace) override;

    // SVD workspace methods

    /**
     * @brief Get an SVD workspace.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     * @return Pointer to an allocated SVDWorkspace.
     */
    SVDWorkspace* getSVDWorkspace(int m, int n) override;

    /**
     * @brief Release an SVD workspace.
     *
     * @param workspace Pointer to the SVDWorkspace to be released.
     */
    void releaseSVDWorkspace(SVDWorkspace* workspace) override;

private:
    OLSWorkspace* olsWorkspace_; ///< Pointer to the OLSWorkspace.
    SVDWorkspace* svdWorkspace_; ///< Pointer to the SVDWorkspace.
};

} // namespace memory
} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_MEMORY_MODELSWORKSPACEMANAGER_H

