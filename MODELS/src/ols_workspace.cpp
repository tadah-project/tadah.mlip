#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tadah/core/lapack.h>
#include <tadah/models/memory/ols_workspace.h>
namespace tadah {
namespace models {
namespace memory {

OLSWorkspace::OLSWorkspace()
    : work(nullptr), lwork(-1), m(0), n(0),
      algo(tadah::models::Algorithm::GELSD), s(nullptr), iwork(nullptr),
      lambda(0.0), U(nullptr), S(nullptr), VT(nullptr), iworkSVD(nullptr),
      UtB(nullptr), AtA(nullptr), Atb(nullptr) {}

OLSWorkspace::~OLSWorkspace() {
  // Deallocate all allocated memory
  delete[] U;
  delete[] S;
  delete[] VT;
  delete[] s;
  delete[] iwork;
  delete[] UtB;
  delete[] iworkSVD;
  delete[] work;
  delete[] AtA;
  delete[] Atb;

  // Reset pointers
  U = nullptr;
  S = nullptr;
  VT = nullptr;
  s = nullptr;
  iwork = nullptr;
  UtB = nullptr;
  iworkSVD = nullptr;
  work = nullptr;
  AtA = nullptr;
  Atb = nullptr;
}

void OLSWorkspace::allocate(int m_, int n_,
                            tadah::models::Algorithm algorithm) {
  // Release existing memory
  delete[] U;
  delete[] S;
  delete[] VT;
  delete[] s;
  delete[] iwork;
  delete[] UtB;
  delete[] iworkSVD;
  delete[] work;
  delete[] AtA;
  delete[] Atb;

  // Reset pointers
  U = nullptr;
  S = nullptr;
  VT = nullptr;
  s = nullptr;
  iwork = nullptr;
  UtB = nullptr;
  iworkSVD = nullptr;
  work = nullptr;
  AtA = nullptr;
  Atb = nullptr;

  m = m_;
  n = n_;
  algo = algorithm;

  int info = 0;

  if (algorithm == tadah::models::Algorithm::GELSD) {
    // Allocate workspace for DGELSD
    int minmn = std::min(m, n);
    int maxmn = std::max(m, n);
    int nlvl = std::max(0, static_cast<int>(std::log2(minmn / 25.0)) + 1);

    // Allocate s (singular values)
    s = new double[minmn];

    // Allocate iwork
    int liwork = 3 * minmn * nlvl + 11 * minmn;
    iwork = new int[liwork];

    // Workspace query
    double workQuery;
    int lworkQuery = -1;
    int nrhs = 1;
    double *aDummy = nullptr;
    double *bDummy = nullptr;
    double rcondDummy = -1.0;
    int rankDummy;
    ::tadah::core::dgelsd_(&m, &n, &nrhs, aDummy, &m, bDummy, &maxmn, s, &rcondDummy,
              &rankDummy, &workQuery, &lworkQuery, iwork, &info);
    lwork = static_cast<int>(workQuery);
    work = new double[lwork];
  } else if (algorithm == tadah::models::Algorithm::GELS) {
    // Allocate workspace for DGELS
    double workQuery;
    int lworkQuery = -1;
    int nrhs = 1;
    double *aDummy = nullptr;
    double *bDummy = nullptr;
    char trans = 'N';
    ::tadah::core::dgels_(&trans, &m, &n, &nrhs, aDummy, &m, bDummy, &m, &workQuery,
             &lworkQuery, &info);
    lwork = static_cast<int>(workQuery);
    work = new double[lwork];
  } else if (algorithm == tadah::models::Algorithm::Pseudoinverse) {
    // Allocate workspace for internal SVD computation in OLS

    int minMN = std::min(m, n);

    // Allocate U, S, VT
    U = new double[m * minMN];  // U is m x min(m,n)
    S = new double[minMN];      // Singular values
    VT = new double[minMN * n]; // VT is min(m,n) x n

    // Workspace for dgesdd_
    iworkSVD = new int[8 * minMN];

    // Query for optimal workspace size
    char jobz = 'S';
    int lda = m;
    int ldu = m;
    int ldvt = minMN;
    lwork = -1;
    double wkopt;
    double *aDummy = nullptr;
    int infoQuery;
    ::tadah::core::dgesdd_(&jobz, &m, &n, aDummy, &lda, S, U, &ldu, VT, &ldvt, &wkopt,
              &lwork, iworkSVD, &infoQuery);

    if (infoQuery != 0) {
      throw std::runtime_error("Error in DGESDD (workspace query): info = " +
                               std::to_string(infoQuery));
    }

    lwork = static_cast<int>(wkopt);
    work = new double[lwork];

    // Allocate UtB
    UtB = new double[minMN];
  } else if (algorithm == tadah::models::Algorithm::NormalEquations) {
    // Allocate space for AtA (n x n) and Atb (n)
    AtA = new double[n * n];
    Atb = new double[n];
    // No additional workspace required
    work = nullptr;
    lwork = 0;
  } else {
    throw std::invalid_argument(
        "Invalid algorithm specified in OLSWorkspace::allocate().");
  }
}

bool OLSWorkspace::isSufficient(int m_, int n_,
                                tadah::models::Algorithm algorithm) const {
  return (m == m_ && n == n_ && algo == algorithm);
}

} // namespace memory
} // namespace models
} // namespace tadah
