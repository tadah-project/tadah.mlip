#ifndef TADAH_MODELS_OLS_IMPL_H
#define TADAH_MODELS_OLS_IMPL_H

#include <tadah/core/lapack.h>
#include <tadah/models/ols.h>
#include <tadah/models/memory/ols_workspace.h>
#include <tadah/models/memory/svd_workspace.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace tadah {
namespace models {

// Helper function to convert Algorithm enum to string
inline std::string to_string_algorithm(Algorithm algo) {
    switch (algo) {
        case Algorithm::GELS:
            return "GELS";
        case Algorithm::GELSD:
            return "GELSD";
        case Algorithm::Pseudoinverse:
            return "Pseudoinverse";
        case Algorithm::NormalEquations:
            return "NormalEquations";
        default:
            return "Unknown Algorithm";
    }
}

// Overloaded operator<<
inline std::ostream& operator<<(std::ostream& os, Algorithm algo) {
    os << to_string_algorithm(algo);
    return os;
}

// Constructors and Destructor
inline OLS::OLS()
    : workspaceManager_(new memory::ModelsWorkspaceManager()), ownsWorkspaceManager_(true) {}

inline OLS::OLS(memory::IModelsWorkspaceManager& workspaceManager)
    : workspaceManager_(&workspaceManager), ownsWorkspaceManager_(false) {}

inline OLS::~OLS() {
    if (ownsWorkspaceManager_ && workspaceManager_) {
        delete workspaceManager_;
    }
}

// Public solve methods
template <typename M, typename V>
void OLS::solve(M& A, V& B, V& weights) {
    double rcond = -1.0;
    Algorithm algorithm = Algorithm::GELSD; // Default algorithm suitable for all systems
    solveImpl(A, B, weights, rcond, algorithm);
}

template <typename M, typename V>
void OLS::solve(M& A, V& B, V& weights, double rcond, Algorithm algorithm) {
    solveImpl(A, B, weights, rcond, algorithm);
}

// Implementation of solve methods
template <typename M, typename V>
void OLS::solveImpl(M& A, V& B, V& weights, double rcond, Algorithm algorithm) {
    int m = A.rows();
    int n = A.cols();
    int maxmn = std::max(m, n);
    int minmn = std::min(m, n);

    bool isUnderdetermined = (m < n);

    // Check if the algorithm is suitable for the system
    if (isUnderdetermined) {
        // Underdetermined system
        if (!(algorithm == Algorithm::GELSD || algorithm == Algorithm::Pseudoinverse)) {
            throw std::invalid_argument("The chosen algorithm (" + to_string_algorithm(algorithm) +
                                        ") does not solve an underdetermined system (m < n); "
                                        "GELSD and Pseudoinverse do.");
        }
    } else {
        // Overdetermined or square system
        // All algorithms are applicable, but Cholesky (NormalEquations) may not handle rank-deficient cases well
        if (algorithm == Algorithm::NormalEquations && m < n) {
            throw std::invalid_argument("The NormalEquations algorithm is not recommended for systems where m < n.");
        }
    }

    // Ensure that B is of sufficient size
    if (B.size() < static_cast<std::size_t>(m)) {
        throw std::invalid_argument("B size is insufficient. It must be at least the number of rows of A.");
    }

    // Get OLS workspace
    memory::OLSWorkspace* ws = workspaceManager_->getOLSWorkspace(m, n, algorithm);

    // Ensure dimensions match
    if (weights.size() != static_cast<std::size_t>(n)) {
        throw std::invalid_argument("weights size is incorrect. It must be of size n (number of columns of A).");
    }

    // Prepare the right-hand side vector
    V* b_work_ptr;  // Pointer to the vector to be used as B in computations
    V b_work;       // Temporary vector if needed

    // If B.size() >= maxmn, we can use B directly
    if (B.size() >= static_cast<std::size_t>(maxmn)) {
        b_work_ptr = &B;
    } else {
        // Create a temporary vector of size maxmn, initialize with B and zeros
        b_work.resize(maxmn);
        for (int i = 0; i < m; ++i) {
            b_work[i] = B[i];
        }
for (int i = m; i < maxmn; ++i) {
  b_work[i] = 0.0;
}
        b_work_ptr = &b_work;
    }

    int info;
    if (algorithm == Algorithm::GELSD) {
        // Use DGELSD for underdetermined or rank-deficient systems
        int nrhs = 1;
        double* a = A.ptr();
        int lda = m;
        double* b = b_work_ptr->ptr();  // Use the b_work vector
        int ldb = maxmn;  // Must be at least max(m,n)
        int rank;

        // Solve using DGELSD
    tadah::core::dgelsd_(&m, &n, &nrhs, a, &lda, b, &ldb, ws->s, &rcond, &rank,
                  ws->work, &ws->lwork, ws->iwork, &info);

        if (info > 0) {
            throw std::runtime_error(
                "DGELSD: the singular value decomposition of the design "
                "matrix (" + std::to_string(m) + " rows, " + std::to_string(n) +
                " columns) did not converge (info = " + std::to_string(info) +
                ": that many off-diagonal elements of an intermediate "
                "bidiagonal form did not converge to zero)");
        }
        if (info < 0) {
            throw std::logic_error("DGELSD: argument " + std::to_string(-info) +
                                   " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
        }

        // Copy solution to weights
        for (int i = 0; i < n; ++i) {
            weights[i] = b[i];
        }

    } else if (algorithm == Algorithm::GELS) {
        // Use DGELS for overdetermined systems
        if (isUnderdetermined) {
            throw std::invalid_argument("GELS does not solve an underdetermined system (m < n); GELSD and Pseudoinverse do.");
        }
        int nrhs = 1;
        double* a = A.ptr();
        int lda = m;
        double* b = b_work_ptr->ptr();
        int ldb = m;
        char trans = 'N';

        // Solve using DGELS
    tadah::core::dgels_(&trans, &m, &n, &nrhs, a, &lda, b, &ldb, ws->work, &ws->lwork, &info);

        if (info > 0) {
            throw RankDeficientError(
                "DGELS: the design matrix (" + std::to_string(m) + " rows, " +
                std::to_string(n) + " columns) does not have full column "
                "rank: the diagonal element " + std::to_string(info) +
                " of the triangular factor R of its QR factorisation is zero, "
                "so its column " + std::to_string(info) + " is zero or a "
                "linear combination of the columns before it",
                info);
        }
        if (info < 0) {
            throw std::logic_error("DGELS: argument " + std::to_string(-info) +
                                   " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
        }

        // Copy solution to weights
        for (int i = 0; i < n; ++i) {
            weights[i] = b[i];
        }
    } else if (algorithm == Algorithm::Pseudoinverse) {
        // Custom SVD-based pseudoinverse solution
        // Compute the pseudoinverse using SVD and solve x = A^+ B

        // Copy A into U (since A might be overwritten)
        /*std::copy(A.ptr(), A.ptr() + m * n, ws->U);*/

        // Perform SVD
        char jobz = 'S';  // Compute the singular vectors
        int lda = m;
        int ldu = m;
        int ldvt = n;  // For 'S', VT has dimensions (n x min(m, n))
    tadah::core::dgesdd_(&jobz, &m, &n, A.ptr(), &lda, ws->S, ws->U, &ldu,
              ws->VT, &ldvt, ws->work, &ws->lwork, ws->iwork, &info);

        if (info > 0) {
            throw std::runtime_error(
                "DGESDD: the singular value decomposition of the design "
                "matrix (" + std::to_string(m) + " rows, " + std::to_string(n) +
                " columns) did not converge (info = " + std::to_string(info) +
                ")");
        }
        if (info < 0) {
            throw std::logic_error("DGESDD: argument " + std::to_string(-info) +
                                   " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
        }

        // Compute U^T B
        char trans = 'T';
        double alpha = 1.0;
        double beta = 0.0;
        int incx = 1;
        int incy = 1;
    tadah::core::dgemv_(&trans, &m, &minmn, &alpha, ws->U, &lda, B.ptr(), &incx, &beta, ws->UtB, &incy);

        // Apply threshold to singular values (for handling rank deficiency)
        double smax = *std::max_element(ws->S, ws->S + minmn);
        double thresh = (rcond > 0.0) ? rcond * smax : 0.0;

    for (int i = 0; i < minmn; ++i) {
        if (ws->S[i] > thresh) {
            ws->UtB[i] /= ws->S[i];
        } else {
            ws->UtB[i] = 0.0;
        }
    }

        // Compute x = V * (pseudoinverse solution)
        trans = 'T'; // Use V (not transposed)
        /*int ldvt = n;*/
    tadah::core::dgemv_(&trans, &minmn, &n, &alpha, ws->VT, &ldvt, ws->UtB, &incx, &beta, weights.ptr(), &incy);

    } else if (algorithm == Algorithm::NormalEquations) {
        // Solve using normal equations (A^T A x = A^T B)
        // Compute A^T A
        char uplo = 'U';
        char trans = 'T';
        double alpha = 1.0;
        double beta = 0.0;
        int lda = m;
        int ldc = n;

    tadah::core::dsyrk_(&uplo, &trans, &n, &m, &alpha, A.ptr(), &lda, &beta, ws->AtA, &ldc);

        // Compute A^T B
        int incx = 1;
        int incy = 1;
    tadah::core::dgemv_(&trans, &m, &n, &alpha, A.ptr(), &lda, B.ptr(), &incx, &beta, ws->Atb, &incy);

        // Perform Cholesky factorization of A^T A
    tadah::core::dpotrf_(&uplo, &n, ws->AtA, &ldc, &info);
        if (info > 0) {
            throw RankDeficientError(
                "DPOTRF: the matrix A^T A of the design matrix A (" +
                std::to_string(m) + " rows, " + std::to_string(n) +
                " columns) is not positive definite: its leading minor of "
                "order " + std::to_string(info) + " is not, so A does not "
                "have full column rank or is too ill-conditioned for the "
                "normal equations",
                info);
        }
        if (info < 0) {
            throw std::logic_error("DPOTRF: argument " + std::to_string(-info) +
                                   " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
        }

        // Solve A^T A x = A^T B
        int nrhs = 1;
    tadah::core::dpotrs_(&uplo, &n, &nrhs, ws->AtA, &ldc, ws->Atb, &n, &info);
        if (info != 0) {
            throw std::logic_error("DPOTRS: argument " + std::to_string(-info) +
                                   " had an illegal value (info = " + std::to_string(info) + "; with a finite design matrix, an internal error)");
        }

        // Copy solution to weights
        for (int i = 0; i < n; ++i) {
            weights[i] = ws->Atb[i];
        }

    } else {
        throw std::invalid_argument("Unknown or unsupported algorithm.");
    }

    // No need to resize B back, since we didn't modify the user's B
}

// NormalEquations solve that additionally caches the Cholesky factor
// for IFT back-substitution. Mirrors the OALGO=4 branch of solveImpl
// byte-for-byte plus a final memcpy of the factor into the handle.
template <typename M, typename V>
void OLS::solveAndCacheFactor(M& A, V& B, V& weights,
                              FactorHandle& out_factor,
                              std::size_t n_bias) {
    int m = A.rows();
    int n = A.cols();

    if (m < n) {
        throw std::invalid_argument(
            "OLS::solveAndCacheFactor requires m >= n (overdetermined or "
            "square system) for the NormalEquations path. Got m=" +
            std::to_string(m) + ", n=" + std::to_string(n) + ".");
    }
    if (B.size() < static_cast<std::size_t>(m)) {
        throw std::invalid_argument(
            "OLS::solveAndCacheFactor: B size is insufficient (must be >= m).");
    }
    if (weights.size() != static_cast<std::size_t>(n)) {
        throw std::invalid_argument(
            "OLS::solveAndCacheFactor: weights size must equal n.");
    }

    memory::OLSWorkspace* ws =
        workspaceManager_->getOLSWorkspace(m, n, Algorithm::NormalEquations);

    char uplo = 'U';
    char trans = 'T';
    double alpha = 1.0;
    double beta = 0.0;
    int lda = m;
    int ldc = n;

    // AtA = Aᵀ A (upper triangle written by dsyrk_).
    tadah::core::dsyrk_(&uplo, &trans, &n, &m, &alpha,
                        A.ptr(), &lda, &beta, ws->AtA, &ldc);

    // Atb = Aᵀ B.
    int incx = 1;
    int incy = 1;
    tadah::core::dgemv_(&trans, &m, &n, &alpha, A.ptr(), &lda,
                        B.ptr(), &incx, &beta, ws->Atb, &incy);

    // Cholesky-factorise AtA in place. After this, the upper triangle
    // of ws->AtA holds U with U^T U = AᵀA.
    int info = 0;
    tadah::core::dpotrf_(&uplo, &n, ws->AtA, &ldc, &info);
    if (info != 0) {
        throw std::runtime_error(
            "OLS::solveAndCacheFactor: DPOTRF failed (matrix is not "
            "positive definite); info = " + std::to_string(info) +
            ". This typically means a rank-deficient design matrix or "
            "LAMBDA=0 with insufficient data; raise LAMBDA or switch "
            "to NUMERICAL.");
    }

    // Solve U^T U x = Atb in place; ws->Atb now contains the solution.
    int nrhs = 1;
    tadah::core::dpotrs_(&uplo, &n, &nrhs, ws->AtA, &ldc, ws->Atb, &n, &info);
    if (info != 0) {
        throw std::runtime_error(
            "OLS::solveAndCacheFactor: DPOTRS failed; info = " +
            std::to_string(info));
    }

    // Copy solution to weights.
    for (int i = 0; i < n; ++i) {
        weights[i] = ws->Atb[i];
    }

    // Snapshot the factor into the handle. Owning the buffer means the
    // workspace can be reused for the next train() without invalidating
    // any cached factor in flight.
    const std::size_t nn = static_cast<std::size_t>(n) *
                           static_cast<std::size_t>(n);
    out_factor.AtA.assign(ws->AtA, ws->AtA + nn);
    out_factor.n      = n;
    out_factor.n_bias = n_bias;
    out_factor.algo   = Algorithm::NormalEquations;
    out_factor.valid  = true;
}

// Solve with SVD workspace (if needed)
template <typename V1, typename V2>
void OLS::solveWithPseudoinverse(int m, int n, V1& B, V2& weights, memory::SVDWorkspace& svdWorkspace) {
  double rcond=-1;
  double lambda=0;
  OLS::solveWithPseudoinverse(m,n,B,weights,svdWorkspace,rcond,lambda);
}
template <typename V1, typename V2>
void OLS::solveWithPseudoinverse(int m, int n, V1& B, V2& weights, memory::SVDWorkspace& svdWorkspace, double rcond, double lambda) {
    // Function implementation without default argument
    memory::OLSWorkspace* ws = workspaceManager_->getOLSWorkspace(m, n, Algorithm::Pseudoinverse);

    // Ensure dimensions match
    if (weights.size() != static_cast<std::size_t>(n)) {
        throw std::invalid_argument("weights size is incorrect.");
    }

    int minmn = std::min(m, n);

    // Use svdWorkspace's U, S, VT directly
    double* U = svdWorkspace.U;
    double* S = svdWorkspace.S;
    double* VT = svdWorkspace.VT;

    // Compute U^T * B
    char trans = 'T';
    double alpha = 1.0;
    double beta = 0.0;
    int incx = 1;
    int incy = 1;
    int lda = m;
  tadah::core::dgemv_(&trans, &m, &minmn, &alpha, U, &lda, B.ptr(), &incx, &beta, ws->UtB, &incy);

    // Threshold for singular values (shared with
    // LinearRegressor::dropped_directions and the evidence approximation,
    // ea.h)
    const double thresh = pseudoinverse_threshold(S, minmn, rcond);

    // Compute the pseudoinverse solution
    for (int i = 0; i < minmn; ++i) {
        if (S[i] > thresh) {
            ws->UtB[i] *= S[i] / ( S[i] * S[i] + lambda );
        } else {
            ws->UtB[i] = 0.0;
        }
    }

    // Compute x = V * (pseudoinverse solution). V^T is min(m,n) x n with
    // leading dimension min(m,n) (SVD::compute, dgesdd's ldvt).
    trans = 'T'; // Use V
    int ldvt = minmn;
  tadah::core::dgemv_(&trans, &minmn, &n, &alpha, VT, &ldvt, ws->UtB, &incx, &beta, weights.ptr(), &incy);
}

} // namespace models
} // namespace tadah

#endif // TADAH_MODELS_OLS_IMPL_H
