#ifndef TADAH_MODELS_MEMORY_IMODELSWORKSPACEMANAGER_H
#define TADAH_MODELS_MEMORY_IMODELSWORKSPACEMANAGER_H

#include <tadah/core/memory/iworkspace_manager.h>
#include <tadah/models/algorithm.h> // Include the Algorithm enum

namespace tadah {
namespace models {
namespace memory {

class OLSWorkspace; ///< Forward declaration of OLSWorkspace
class SVDWorkspace; ///< Forward declaration of SVDWorkspace

/**
 * @interface IModelsWorkspaceManager
 * @brief Interface for managing workspaces specific to the MODELS module.
 *
 * Extends the core IWorkspaceManager interface and provides methods for
 * obtaining and releasing OLS and SVD workspaces.
 */
class IModelsWorkspaceManager : public virtual ::tadah::core::memory::IWorkspaceManager {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IModelsWorkspaceManager();

    /**
     * @brief Get an OLS workspace suitable for the given problem size and algorithm.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     * @param algorithm The algorithm to be used.
     * @return Pointer to an allocated OLSWorkspace.
     */
    virtual OLSWorkspace* getOLSWorkspace(int m, int n, tadah::models::Algorithm algorithm) = 0;

    /**
     * @brief Release an OLS workspace.
     *
     * @param workspace Pointer to the OLSWorkspace to be released.
     */
    virtual void releaseOLSWorkspace(OLSWorkspace* workspace) = 0;

    /**
     * @brief Get an SVD workspace suitable for the given problem size.
     *
     * @param m Number of rows in the matrix.
     * @param n Number of columns in the matrix.
     * @return Pointer to an allocated SVDWorkspace.
     */
    virtual SVDWorkspace* getSVDWorkspace(int m, int n) = 0;

    /**
     * @brief Release an SVD workspace.
     *
     * @param workspace Pointer to the SVDWorkspace to be released.
     */
    virtual void releaseSVDWorkspace(SVDWorkspace* workspace) = 0;
};

} // namespace memory
} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_MEMORY_IMODELSWORKSPACEMANAGER_H

