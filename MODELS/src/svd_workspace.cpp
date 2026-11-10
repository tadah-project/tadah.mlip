#include <tadah/models/memory/svd_workspace.h>
#include <tadah/core/lapack.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tadah {
namespace models {
namespace memory {

SVDWorkspace::SVDWorkspace()
    : m(0), n(0),
      U(nullptr), S(nullptr), VT(nullptr),
      work(nullptr), lwork(-1), iwork(nullptr) {}

SVDWorkspace::~SVDWorkspace() {
    delete[] U;
    delete[] S;
    delete[] VT;
    delete[] iwork;
    delete[] work;
    U = nullptr; S = nullptr; VT = nullptr;
    iwork = nullptr; work = nullptr;
}

void SVDWorkspace::allocate(int m_, int n_) {
    // Release existing memory
    delete[] U;
    delete[] S;
    delete[] VT;
    delete[] iwork;
    delete[] work;

    U = nullptr; S = nullptr; VT = nullptr;
    iwork = nullptr; work = nullptr;

    m = m_;
    n = n_;

    int minMN = std::min(m, n);

    // Allocate U, S, VT
    U = new double[m * minMN];      // U is m x min(m,n)
    S = new double[minMN];          // Singular values
    VT = new double[minMN * n];     // VT is min(m,n) x n

    // Workspace for dgesdd_
    iwork = new int[8 * minMN];

    // Query for optimal workspace size
    char jobz = 'S';
    int lda = m;
    int ldu = m;
    int ldvt = minMN;
    lwork = -1;
    double wkopt;
    double* aDummy = nullptr;
    int infoQuery;
  ::tadah::core::dgesdd_(&jobz, &m, &n, aDummy, &lda, S, U, &ldu, VT, &ldvt, &wkopt, &lwork, iwork, &infoQuery);

    if (infoQuery != 0) {
        throw std::runtime_error("Error in DGESDD (workspace query in SVDWorkspace): info = " + std::to_string(infoQuery));
    }

    lwork = static_cast<int>(wkopt);
    work = new double[lwork];
}

bool SVDWorkspace::isSufficient(int m_, int n_) const {
    return (m == m_ && n == n_);
}

} // namespace memory
} // namespace models
} // namespace tadah
