#ifndef TADAH_MODELS_SVD_IMPL_H
#define TADAH_MODELS_SVD_IMPL_H

#include <tadah/models/svd.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/core/lapack.h>
#include <algorithm>
#include <stdexcept>

namespace tadah {
namespace models {

inline SVD::SVD()
    : workspaceManager_(new memory::ModelsWorkspaceManager()), ownsWorkspaceManager_(true), svdWorkspace_(nullptr), m_(0), n_(0) {}

inline SVD::SVD(memory::IModelsWorkspaceManager& workspaceManager)
    : workspaceManager_(&workspaceManager), ownsWorkspaceManager_(false), svdWorkspace_(nullptr), m_(0), n_(0) {}

inline SVD::~SVD() {
    if (ownsWorkspaceManager_ && workspaceManager_) {
        delete workspaceManager_;
    }
}

template <typename M>
void SVD::compute(M& A) {
    int m = A.rows();
    int n = A.cols();
    m_ = m; // Store the number of rows
    n_ = n; // Store the number of columns
    svdWorkspace_ = workspaceManager_->getSVDWorkspace(m, n);

    int minMN = std::min(m, n);

    // Prepare parameters for LAPACK dgesdd_
    char jobz = 'S'; // Compute singular values and vectors
    int lda = m;
    int ldu = m;
    int ldvt = minMN;
    int info;

    // Perform SVD
  tadah::core::dgesdd_(&jobz, &m, &n, A.ptr(), &lda, svdWorkspace_->S, svdWorkspace_->U, &ldu,
              svdWorkspace_->VT, &ldvt, svdWorkspace_->work, &svdWorkspace_->lwork, svdWorkspace_->iwork, &info);

    if (info > 0) {
        throw std::runtime_error(
            "DGESDD: the singular value decomposition of the design matrix (" +
            std::to_string(m) + " rows, " + std::to_string(n) +
            " columns) did not converge (info = " + std::to_string(info) + ")");
    }
    if (info < 0) {
        throw std::logic_error("DGESDD: argument " + std::to_string(-info) +
                               " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
    }
}

inline double* SVD::getU() const {
    return svdWorkspace_->U;
}

inline double* SVD::getS() const {
    return svdWorkspace_->S;
}

inline double* SVD::getVT() const {
    return svdWorkspace_->VT;
}

inline memory::SVDWorkspace& SVD::getSVDWorkspace() {
    return *svdWorkspace_;
}

inline int SVD::rows() const {
    return m_;
}

inline int SVD::cols() const {
    return n_;
}

} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_SVD_IMPL_H

