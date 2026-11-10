#ifndef TADAH_CORE_LAPACK_H
#define TADAH_CORE_LAPACK_H
#include <tadah/core/maths.h>

namespace tadah {
namespace core {

extern "C" {

#ifndef TADAH_USE_DLIB
  // this is to avoid conflicts with dlib lapack declarations

    /** LU decomposition of a general matrix */
    void dgetrf_(int *m, int *n, double *a, int *lda, int *ipiv, int *info);

    /**
     * @brief Computes the Cholesky factorization of a real symmetric positive definite matrix A.
     *
     * Computes the upper or lower triangular factorization A = U^T * U or A = L * L^T.
     *
     * @param UPLO  Specifies whether the upper or lower triangular part of A is stored:
     *              - 'U': Upper triangular part of A is stored.
     *              - 'L': Lower triangular part of A is stored.
     * @param N     The order of the matrix A. N >= 0.
     * @param A     On entry, the symmetric positive definite matrix A.
     *              On exit, the factor U or L from the Cholesky factorization.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, N).
     * @param INFO  Output status. INFO = 0 for successful exit.
     *              INFO < 0 indicates an illegal argument value.
     *              INFO > 0 indicates that the leading minor of order INFO is not positive definite.
     */
    void dpotrf_(char* UPLO, int* N, double* A, int* LDA, int* INFO);

    void dgesdd_(char *jobz, int *m, int *n, double *a, int *lda,
            double *s, double *u, int *ldu, double *vt, int *ldvt,
            double *work, int *lwork, int *iwork, int *info);

    /**
     * @brief Computes the singular value decomposition (SVD) of a real M-by-N matrix A.
     *
     * Computes the SVD of A as A = U * SIGMA * V^T. Depending on the JOBU and JOBVT parameters,
     * computes all or part of the matrices U and V^T.
     *
     * @param JOBU  Specifies options for computing all or part of the matrix U:
     *              - 'A': All M columns of U are returned in the array U.
     *              - 'S': The first min(M,N) columns of U (the left singular vectors) are returned in the array U.
     *              - 'O': The first min(M,N) columns of U are overwritten on the array A.
     *              - 'N': No columns of U are computed.
     * @param JOBVT Specifies options for computing all or part of the matrix V^T:
     *              - 'A': All N rows of V^T are returned in the array VT.
     *              - 'S': The first min(M,N) rows of V^T (the right singular vectors) are returned in the array VT.
     *              - 'O': The first min(M,N) rows of V^T are overwritten on the array A.
     *              - 'N': No rows of V^T are computed.
     * @param M     The number of rows of the input matrix A. M >= 0.
     * @param N     The number of columns of the input matrix A. N >= 0.
     * @param A     On entry, the M-by-N matrix A. On exit, contents depend on JOBU and JOBVT.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, M).
     * @param S     The singular values of A, sorted in decreasing order. Dimension at least min(M,N).
     * @param U     If JOBU = 'A', U contains the M-by-M orthogonal matrix U.
     *              If JOBU = 'S', U contains the M-by-min(M,N) matrix of left singular vectors.
     *              If JOBU = 'O' or 'N', U is not referenced.
     * @param LDU   The leading dimension of the array U. LDU >= max(1, M) if JOBU = 'A' or 'S'; otherwise LDU >= 1.
     * @param VT    If JOBVT = 'A', VT contains the N-by-N orthogonal matrix V^T.
     *              If JOBVT = 'S', VT contains the min(M,N)-by-N matrix of right singular vectors.
     *              If JOBVT = 'O' or 'N', VT is not referenced.
     * @param LDVT  The leading dimension of the array VT. LDVT >= max(1, N) if JOBVT = 'A' or 'S'; otherwise LDVT >= 1.
     * @param WORK  Workspace array of dimension LWORK.
     * @param LWORK The dimension of the array WORK. LWORK >= max(3*min(M,N)+max(M,N), 5*min(M,N)).
     * @param INFO  Output status. INFO = 0 for successful exit. INFO < 0 indicates an illegal argument value.
     */
    void dgesvd_(char* JOBU, char* JOBVT, int* M, int* N, double* A, int* LDA,
                 double* S, double* U, int* LDU, double* VT, int* LDVT,
                 double* WORK, int* LWORK, int* INFO);
#endif

    /**
     * @brief Computes the minimum-norm solution to a real linear least squares problem using the Singular Value Decomposition (SVD) of A.
     *
     * Solves the equation A * X = B, where A is an M-by-N matrix, and X and B are vectors or matrices.
     * This routine handles rank-deficient matrices and computes the singular values of A.
     *
     * @param M     The number of rows of the matrix A. M >= 0.
     * @param N     The number of columns of the matrix A. N >= 0.
     * @param NRHS  The number of right-hand sides, i.e., the number of columns of the matrices B and X. NRHS >= 0.
     * @param A     On entry, the M-by-N matrix A. On exit, the contents are destroyed.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, M).
     * @param B     On entry, the M-by-NRHS right-hand side matrix B. On exit, the solution matrix X.
     * @param LDB   The leading dimension of the array B. LDB >= max(1, max(M,N)).
     * @param S     The singular values of A in decreasing order. Dimension at least min(M,N).
     * @param RCOND The reciprocal of the condition number threshold. Used to determine the effective rank.
     * @param RANK  The effective rank of A, i.e., the number of singular values which are greater than RCOND*S(1).
     * @param WORK  Workspace array of dimension LWORK.
     * @param LWORK The dimension of the array WORK. LWORK >= max(1, min(M,N)*3 + max(M,N)*2).
     * @param IWORK Integer workspace array.
     * @param INFO  Output status. INFO = 0 for successful exit. INFO < 0 indicates an illegal argument value.
     */
    void dgelsd_(int* M, int* N, int* NRHS, double* A, int* LDA,
                 double* B, int* LDB, double* S, double* RCOND,
                 int* RANK, double* WORK, int* LWORK, int* IWORK, int* INFO);

    /**
     * @brief Solves overdetermined or underdetermined real linear systems involving an M-by-N matrix A, using a QR or LQ factorization of A.
     *
     * Solves the equation A * X = B or A^T * X = B, where A is an M-by-N matrix and X and B are vectors or matrices.
     * Assumes that A has full rank.
     *
     * @param TRANS Specifies the equations to be solved:
     *              - 'N': A * X = B (No transpose)
     *              - 'T': A^T * X = B (Transpose)
     * @param M     The number of rows of the matrix A. M >= 0.
     * @param N     The number of columns of the matrix A. N >= 0.
     * @param NRHS  The number of right-hand sides, i.e., the number of columns of the matrices B and X. NRHS >= 0.
     * @param A     On entry, the M-by-N matrix A. On exit, details of its QR or LQ factorization.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, M).
     * @param B     On entry, the right-hand side matrix B. On exit, the solution matrix X.
     * @param LDB   The leading dimension of the array B. LDB >= max(1, M, N).
     * @param WORK  Workspace array of dimension LWORK.
     * @param LWORK The dimension of the array WORK. LWORK >= max(1, N + max(N, NRHS)).
     * @param INFO  Output status. INFO = 0 for successful exit. INFO < 0 indicates an illegal argument value.
     */
    void dgels_(char* TRANS, int* M, int* N, int* NRHS, double* A, int* LDA,
                double* B, int* LDB, double* WORK, int* LWORK, int* INFO);

    /**
     * @brief Computes the QR factorisation A = Q R of a real M-by-N matrix A.
     *
     * @param M     The number of rows of A. M >= 0.
     * @param N     The number of columns of A. N >= 0.
     * @param A     On entry, the M-by-N matrix A. On exit, R in the upper
     *              triangle (its min(M,N) x N upper trapezoid), and below
     *              the diagonal the Householder vectors that with TAU
     *              represent Q.
     * @param LDA   The leading dimension of A. LDA >= max(1, M).
     * @param TAU   The min(M,N) scalar factors of the reflectors.
     * @param WORK  Workspace; WORK(1) returns the optimal LWORK.
     * @param LWORK The dimension of WORK, >= max(1, N); -1 queries it.
     * @param INFO  0 on success; -i when the i-th argument is illegal.
     *
     * The sizes are const, as in dlib's declaration of the same routine,
     * so a source that includes both declares one function.
     */
    void dgeqrf_(const int* M, const int* N, double* A, const int* LDA,
                 double* TAU, double* WORK, const int* LWORK, int* INFO);

    /**
     * @brief Solves a system of linear equations A * X = B with a symmetric positive definite matrix A using the Cholesky factorization computed by DPOTRF.
     *
     * @param UPLO  Specifies whether the upper or lower triangular factor is stored:
     *              - 'U': Upper triangular factor is stored.
     *              - 'L': Lower triangular factor is stored.
     * @param N     The order of the matrix A. N >= 0.
     * @param NRHS  The number of right-hand sides, i.e., the number of columns of the matrix B. NRHS >= 0.
     * @param A     The triangular factor U or L from the Cholesky factorization A = U^T * U or A = L * L^T, as computed by DPOTRF.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, N).
     * @param B     On entry, the right-hand side matrix B. On exit, the solution matrix X.
     * @param LDB   The leading dimension of the array B. LDB >= max(1, N).
     * @param INFO  Output status. INFO = 0 for successful exit.
     *              INFO < 0 indicates an illegal argument value.
     */
    void dpotrs_(char* UPLO, int* N, int* NRHS, double* A, int* LDA,
                 double* B, int* LDB, int* INFO);

    /**
     * @brief Performs a symmetric rank-k update of a symmetric matrix C.
     *
     * Computes C = alpha * A * A^T + beta * C, or C = alpha * A^T * A + beta * C.
     *
     * @param UPLO  Specifies whether the upper or lower triangular part of the symmetric matrix C is to be referenced and updated:
     *              - 'U': Upper triangular part of C is referenced and updated.
     *              - 'L': Lower triangular part of C is referenced and updated.
     * @param TRANS Specifies the operation:
     *              - 'N': C = alpha * A * A^T + beta * C.
     *              - 'T': C = alpha * A^T * A + beta * C.
     * @param N     The order of the matrix C. N >= 0.
     * @param K     If TRANS = 'N', the number of columns of A; if TRANS = 'T', the number of rows of A. K >= 0.
     * @param ALPHA The scalar alpha.
     * @param A     The matrix A. Dimensions depend on TRANS.
     * @param LDA   The leading dimension of A.
     * @param BETA  The scalar beta.
     * @param C     The symmetric matrix C. On exit, updated according to the operation.
     * @param LDC   The leading dimension of C. LDC >= max(1, N).
     */
    void dsyrk_(char* UPLO, char* TRANS, int* N, int* K, double* ALPHA,
                double* A, int* LDA, double* BETA, double* C, int* LDC);

    /**
     * @brief Performs one of the matrix-vector operations y = alpha * A * x + beta * y, or y = alpha * A^T * x + beta * y.
     *
     * @param TRANS Specifies the operation:
     *              - 'N': y = alpha * A * x + beta * y.
     *              - 'T': y = alpha * A^T * x + beta * y.
     * @param M     Number of rows of the matrix A.
     * @param N     Number of columns of the matrix A.
     * @param ALPHA The scalar alpha.
     * @param A     The M-by-N matrix A.
     * @param LDA   The leading dimension of the array A. LDA >= max(1, M).
     * @param X     The vector x.
     * @param INCX  The increment for the elements of X. INCX != 0.
     * @param BETA  The scalar beta.
     * @param Y     The vector y.
     * @param INCY  The increment for the elements of Y. INCY != 0.
     */
    void dgemv_(char* TRANS, int* M, int* N, double* ALPHA,
                const double* A, int* LDA, const double* X, int* INCX,
                double* BETA, double* Y, int* INCY);

    /** Compute inverse using the Cholesky factorisation from dpotrf */
    void dpotri_(char *uplo, int *n, double *a, int *lda, int *info);

    /** Cholesky decomposition for packed matrices*/
    void dpptrf_(char *uplo, int *n, double *ap, int *info);

    /** Compute inverse of the matrix from its  LU factorisation */
    void dgetri_(int *n, double *a, int *lda, int *ipiv,
            double *work, int *lwork, int *info);

    void dgemm_(char *transa, char *transb, int *m, int *n,
            int *k, double *alpha, double *a, int *lda, double *b, int *ldb,
            double *beta, double *c, int *ldc);

    /** Balance a general real matrix a */
    void dgebal_(char *job, int *n, double *a, int *lda, int *ilo, int *ihi,
            double *scale, int *info);

    void dposv_(char *uplo, int *n, int *nrhs, double *a, int *lda, double *b,
            int *ldb, int *info);

    void dpocon_(char *uplo, int *n, double *a, int *lda, double *anorm,
            double *rcond, double *work, int *iwork, int *info);

    double dlange_(char *norm, int *m, int *n, double *a, int *lda, double *work);

}


// Some useful operations on vectors and matrices
aed_type T_dgemv(Matrix &mat, aed_type &vec, char trans='N');
double norm_X(Matrix &A, char norm='1');

aed_type solve_posv(Matrix &A, aed_type /*&*/T, char uplo='U');
double condition_number(Matrix &A, char uplo='U');

} // namespace core
} // namespace tadah
#endif
