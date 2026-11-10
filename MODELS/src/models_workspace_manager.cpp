#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/models/memory/ols_workspace.h>
#include <tadah/models/memory/svd_workspace.h>

namespace tadah {
namespace models {
namespace memory {

ModelsWorkspaceManager::ModelsWorkspaceManager()
    : olsWorkspace_(nullptr), svdWorkspace_(nullptr) {}

ModelsWorkspaceManager::~ModelsWorkspaceManager() {
    releaseOLSWorkspace(olsWorkspace_);
    releaseSVDWorkspace(svdWorkspace_);
}

OLSWorkspace* ModelsWorkspaceManager::getOLSWorkspace(int m, int n, tadah::models::Algorithm algorithm) {
    if (!olsWorkspace_ || !olsWorkspace_->isSufficient(m, n, algorithm)) {
        releaseOLSWorkspace(olsWorkspace_);
        olsWorkspace_ = new OLSWorkspace();
        olsWorkspace_->allocate(m, n, algorithm);
    }
    return olsWorkspace_;
}

void ModelsWorkspaceManager::releaseOLSWorkspace(OLSWorkspace* workspace) {
    if (workspace) {
        delete workspace;
        workspace = nullptr;
    }
    olsWorkspace_ = nullptr;
}

SVDWorkspace* ModelsWorkspaceManager::getSVDWorkspace(int m, int n) {
    if (!svdWorkspace_ || !svdWorkspace_->isSufficient(m, n)) {
        releaseSVDWorkspace(svdWorkspace_);
        svdWorkspace_ = new SVDWorkspace();
        svdWorkspace_->allocate(m, n);
    }
    return svdWorkspace_;
}

void ModelsWorkspaceManager::releaseSVDWorkspace(SVDWorkspace* workspace) {
    if (workspace) {
        delete workspace;
        workspace = nullptr;
    }
    svdWorkspace_ = nullptr;
}

} // namespace memory
} // namespace models
} // namespace tadah
