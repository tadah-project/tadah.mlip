#ifndef EKM_H
#define EKM_H

#include <tadah/core/core_types.h>
#include <tadah/core/lapack.h>
#include <tadah/models/svd.h>

#include <vector>

namespace tadah {
namespace models {

/** \brief Empirical kernel map of a set of basis vectors.
 *
 *  configure(basis) computes the kernel matrix K(b_i, b_j) of the basis
 *  vectors (the columns of \p basis), balances it (LAPACK dgebal) and
 *  stores in EKM_mat the transposed right singular vectors V^T of the
 *  result: an n x n orthogonal matrix for n basis vectors, whatever the
 *  rank of K (a basis vector with K(b, b) = 0, such as a zero descriptor
 *  under a dot-product kernel, keeps its column). The basis is not
 *  changed, and configure() keeps no other state: calling it again on
 *  the same basis gives the same EKM_mat.
 *
 *  M_KRR_Train does not use this class: it trains on the kernel columns
 *  as they are.
 */
template <typename Kern>
class EKM {
    private:
        Kern kernel;
    public:
        tadah::core::Matrix EKM_mat;
        template <typename T>
            EKM(T &t):
                kernel(t)
    {}
        void configure(const tadah::core::Matrix &basis) {

            const size_t n = basis.cols();
            tadah::core::Matrix K(n, n);
            for (size_t i=0; i<n; i++) {
                for (size_t j=0; j<n; j++) {
                    K(i,j) = kernel(basis.col(i),basis.col(j));
                }
            }
            if (n == 0) {
                EKM_mat = tadah::core::Matrix();
                return;
            }

            // tadah::core::Matrix balancing
            double *a = K.ptr();
            int ni = static_cast<int>(n);
            char job='B';
            int ilo; int ihi; int info;
            std::vector<double> scale(n);
    tadah::core::dgebal_(&job, &ni, a, &ni, &ilo, &ihi, scale.data(), &info);

            tadah::models::SVD svd;
            svd.compute(K);

            double *vt = svd.getVT();
            const tadah::core::Matrix VT(vt,svd.cols(), svd.cols());

            // Every right singular vector, also those of zero singular
            // values: EKM_mat stays square and orthogonal.
            EKM_mat = tadah::core::Matrix(n, n);
            for (size_t i=0; i<n; ++i) {
                for (size_t j=0; j<n; j++) {
                    EKM_mat(i,j) = VT(i,j); // /s[i];
                }
            }

        }
        void inverse(tadah::core::Matrix &A) {
            double *a = &A.data()[0];
            int n = A.rows();   // A is square
            int *ipiv = new int[n];
            int lwork = n*n;
            double *work = new double[lwork];
            int info;

    tadah::core::dgetrf_(&n,&n,a,&n,ipiv,&info);
    tadah::core::dgetri_(&n,a,&n,ipiv,work,&lwork,&info);

            delete[] ipiv;
            delete[] work;
        }
        void cholesky(tadah::core::Matrix &A, char uplo) {
            // matrix must be either U or L
            int n = A.cols(); // A is square
            double *a = &A.data()[0];
            int info;
    tadah::core::dpotrf_(&uplo, &n, a, &n, &info);
        }

        void project(tadah::core::phi_type &Phi) {
            // Phi'=Phi*EKM_mat
            char transa = 'N';
            char transb = 'N';
            int m = Phi.rows();
            int n = Phi.cols();
            double *a = &Phi.data()[0];
            double *b = &EKM_mat.data()[0];
            double *c = new double[m*n];
            double alpha = 1.0;
            double beta = 0.0;
    tadah::core::dgemm_(&transa, &transb, &m, &n, &n, &alpha, a, &m, b, &n,
                    &beta, c, &m);

            Phi = tadah::core::Matrix(c,m,n);

            delete[] c;

        }
        tadah::core::aed_type project(tadah::core::aed_type &aed, tadah::core::Matrix &basis) {
            tadah::core::aed_type temp(basis.cols());
            for (size_t b=0; b<basis.cols(); b++) {
                temp(b) = kernel(aed,basis[b]);
            }

            return  EKM_mat*temp;
        }
};
} // namespace models
} // namespace tadah
#endif
