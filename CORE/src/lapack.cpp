#include <tadah/core/lapack.h>

namespace tadah {
namespace core {

// Some useful operations on vectors and matrices
aed_type T_dgemv(Matrix &mat, aed_type &vec, char trans) {
  int m = mat.rows();
  int n = mat.cols();
  double alpha = 1.0;
  int lda = m;
  double *a = &mat.data()[0];
  double *x = &vec.data()[0];
  int incx = 1;
  double beta = 0.0;

  aed_type res(trans=='N' ? mat.rows() : mat.cols());
  double *y = &res.data()[0];
  int incy = 1;

  dgemv_(&trans, &m, &n, &alpha, a, &lda, x, &incx, &beta, y, &incy);
  return res;
}
double norm_X(Matrix &A, char norm) {
  double *a = A.ptr();
  int m=(int)A.rows();
  int n=(int)A.cols();
  double *work = NULL;
  if (norm=='I' || norm=='i') work = new double[n];

  double res = dlange_(&norm, &m, &n, a, &m, work);

  if (norm=='I' || norm=='i') delete [] work;

  return 1.0/res;
}
aed_type solve_posv(Matrix &A, aed_type /*&*/T, char uplo) {
  double *a = A.ptr();
  double *b = T.ptr();
  int n = A.rows();   // A is square
  int nrhs = 1;
  int info;

  dposv_(&uplo, &n, &nrhs, a, &n, b, &n, &info);

  return T;
}
double condition_number(Matrix &A, char uplo) {
  double *a = A.ptr();
  int n = A.rows();   // A is square
  double anorm =norm_X(A,'1');
  double rcond;
  int info;
  double *work = new double[3*n];
  int  *iwork = new int[n];

  dpocon_(&uplo, &n, a, &n, &anorm, &rcond, work, iwork, &info);

  delete [] work;
  delete [] iwork;

  return 1/rcond;
}

} // namespace core
} // namespace tadah

