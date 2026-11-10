#ifndef TADAH_CORE_MATHS_H
#define TADAH_CORE_MATHS_H
#include <type_traits>
#include <tadah/core/aed_type.h>
#include <tadah/core/vec3d.h>

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>
#include <limits>
#include <random>
#include <iostream>
#include <random>
#include <chrono>
#include <cstring> // for std::memcpy
#include <sstream>

#include <array>
#include <cstdint>
#include <string>

namespace tadah {
namespace core {

namespace detail {
/** True unless @p x is an infinity or a NaN, tested on the exponent bits:
 *  under -ffast-math the compiler may fold std::isfinite to true. */
inline bool maths_is_finite(double x) {
  static_assert(sizeof(double) == sizeof(std::uint64_t),
                "double must be 64-bit");
  std::uint64_t bits;
  std::memcpy(&bits, &x, sizeof(bits));
  return (bits & UINT64_C(0x7ff0000000000000)) !=
         UINT64_C(0x7ff0000000000000);
}
}  // namespace detail

/** A dense matrix. operator()(i, j) is row i, column j; the storage behind
 *  operator[], data() and ptr() is column-major: element (i, j) is
 *  m_[i + rows()*j]. load() takes its values in row-major reading order
 *  (row 0 first), so a Structure::cell is loaded as
 *  a0x a0y a0z  a1x a1y a1z  a2x a2y a2z and row i is the lattice vector
 *  a_i. */
template <typename D>
class Matrix_Base {
public:
  size_t rows_;
  size_t cols_;
  D m_;

  inline Matrix_Base<D>(): rows_(0), cols_(0) {};
  inline Matrix_Base<D>(D data, size_t rows, size_t cols)
  : rows_(rows), cols_(cols), m_(data) {}

  inline void swap_columns(size_t col1, size_t col2) {
    // double *matrix = this->ptr();
    for (size_t i = 0; i < rows(); ++i) {
      std::swap(m_[i + col1 * rows()], m_[i + col2 * rows()]);
    }
  }

  inline double& operator()(size_t i, size_t j)
  {
    return m_[i + rows_*j];
  }

  inline double operator()(size_t i, size_t j) const
  {
    return m_[i + rows_*j];
  }
  inline double& operator[](size_t i)
  {
    return m_[i];
  }

  inline double operator[](size_t i) const
  {
    return m_[i];
  }

  inline size_t rows() const {
    return rows_;
  }
  inline size_t cols() const {
    return cols_;
  }
  inline bool exists(size_t row, size_t col) const
  {
    return (row < rows_ && col < cols_);
  }
  // row() and col() return their aed_type as a prvalue, which C++17 builds
  // in the caller's object: a view stays a view. A named one would rely on
  // NRVO, which no compiler must apply, and a move from a view copies.
  /** Provide a view of the r row of the matrix (cols() values rows()
   *  apart). */
  inline aed_type row(size_t r) {
    return aed_type(&m_[r], cols(), rows());
  }
  /** Provide a view of the c column of the matrix */
  inline aed_type col(size_t c) {
    return aed_type(&m_[rows()*c], rows());
  }
  /** Provide a copy of the c column of the matrix */
  inline aed_type col(size_t c) const {
    return aed_type(&m_[rows()*c], rows());
  }
  /** Provide a copy of the r row of the matrix */
  inline aed_type row(size_t r) const {
    return aed_type(&m_[r], cols(), rows());
  }
  inline void set_zero() {
    for (size_t i=0;i<m_.size();++i) {
      m_[i]=0;
    }
  }
  inline size_t size() const {
    // Number of coefficients in the matrix
    return m_.size();
  }
  //inline t_pred operator*(const aed_type) {
  //}
  inline aed_type diagonal() const {
    aed_type temp(rows());
    for (size_t i=0; i<rows(); ++i) {
      temp(i)=(*this)(i,i);
    }
    return temp;
  }
  inline friend std::ostream &operator<<(std::ostream &os,
                                         const Matrix_Base<D> &m)
  {
    for (size_t r=0;r<m.rows();++r) {
      for (size_t c=0;c<m.cols()-1;++c) {
        os << m(r,c) << "  ";
      }
      os << m(r,m.cols()-1) << std::endl;
    }
    return os;
  }
  template <typename T>
  inline aed_type operator*(const T &v) const
  {
    // naive matrix vector product
    //m is M x N matrix and v is N x 1 column vector
    aed_type temp((*this).rows());
    temp.set_zero();
    for (size_t i=0; i<(*this).rows(); i++){
      for (size_t j=0; j<(*this).cols(); j++) {
        temp(i) += (*this)(i,j)*v(j);
      }
    }
    return temp;
  }
  inline void set_rows(const size_t r)
  {
    rows_=r;
  }
  inline void set_cols(const size_t c)
  {
    cols_=c;
  }
  // Maths
  inline Matrix_Base<D> operator-() const
  {
    Matrix_Base<D> temp(*this);
    for (size_t i=0;i<temp.size();++i) {
      temp[i]*=-1;
    }
    return temp;;
  }
  template <typename T>
  inline Matrix_Base<D> &operator+=(const T &m)
  {
    for (size_t i=0;i<m_.size();++i) {
      m_[i]+=m[i];
    }
    return *this;
  }
  template <typename T>
  inline Matrix_Base<D> &operator-=(const T &m)
  {
    for (size_t i=0;i<m_.size();++i) {
      m_[i]-=m[i];
    }
    return *this;
  }
  inline double trace() const {
    double temp=0;
    for (size_t i=0; i<rows(); ++i)
      temp+= (*this)(i,i);
    return temp;
  }
  bool isApprox(const Matrix_Base<D> &m, double EPS) const {
    for (size_t i=0;i<size();++i) {
      if (std::abs((*this)[i]-m[i])>EPS) return false;
    }
    return true;
  }
  inline bool operator==(const aed_type &v1) const
  {
    double EPS = std::numeric_limits<double>::epsilon();
    return this->isApprox(v1, EPS);
  }
  //  template <typename... T>
  //      friend Matrix_Base<D> operator<<(Matrix_Base<D> &mat,T... list)
  //      {
  //          size_t i=0;
  //          for( const auto elem : {list...} )
  //          {
  //              mat.data()[i++]=elem ;
  //          }
  //          return mat;
  //      }
  /** Set the elements from values in row-major reading order: row 0
   *  first, i.e. (0,0), (0,1), ..., (0,cols-1), (1,0), ...
   *  @throws std::invalid_argument, changing no element, unless exactly
   *  rows() x cols() values are given (more would be written past the
   *  storage or over elements already set, fewer leave elements as they
   *  were). */
  template <typename... T>
  void load(T... list)
  {
    if (sizeof...(T) != rows() * cols())
      throw std::invalid_argument(
          "Matrix load: " + std::to_string(sizeof...(T)) + " values, but a " +
          std::to_string(rows()) + " x " + std::to_string(cols()) +
          " matrix takes " + std::to_string(rows() * cols()));
    size_t i=0;
    for( const auto elem : {list...} )
    {
      m_[rows()*(i%cols()) + i/cols()]=elem ;
      i++;
    }
  }
  void random(
    int seed=std::chrono::system_clock::now().time_since_epoch().count(),
    double lower=-1e4, double upper=1e4
  ) {
    std::uniform_real_distribution<double> unif(lower,upper);
    std::default_random_engine engine(seed);
    for (size_t i=0; i<size(); ++i)
      (*this)[i] = unif(engine);
  }
  inline double *data() {
    return &(*this)[0];
  }
  const double *data() const {
    return &(*this)[0];
  }
  /** Return the pointer to the first element of the data array */
  inline double *ptr() {
    return &(*this)[0];
  }
  /** Return the pointer to the c column */
  inline double *ptr(size_t c) {
    return ptr() + c*rows();
  }
  /** Return the pointer to the (r,c) */
  inline double *ptr(size_t r, size_t c) {
    return ptr() + c*rows() + r;
  }
};
template<typename T=std::vector<double>>
class Matrix_T: public Matrix_Base<T> {
public:
  Matrix_T() {};
  Matrix_T(T &d, size_t r, size_t c):
    Matrix_Base<T>(d,r,c)
  {}
  Matrix_T(double *d, size_t r, size_t c):
    Matrix_Base<T>(T(d,d+r*c),r,c)
  {};
  Matrix_T(size_t r, size_t c):
    Matrix_Base<T>(T(r*c),r,c)
  {}
  Matrix_T(int r, int c, std::initializer_list<double> values):
    Matrix_Base<T>(values,r,c)
  {}
  inline void resize(size_t r, size_t c)
  {
    Matrix_Base<T>::m_.resize(r*c);
    Matrix_Base<T>::rows_= r;
    Matrix_Base<T>::cols_= c;
  }
};
template <typename D>
Matrix_T<D> transpose(const Matrix_T<D>& mat) {
  Matrix_T<D> result(mat.cols(), mat.rows());
  for (size_t i = 0; i < mat.rows(); ++i) {
    for (size_t j = 0; j < mat.cols(); ++j) {
      result(j, i) = mat(i, j);
    }
  }
  return result;
}
typedef Matrix_T<> Matrix;

class MatrixView: public Matrix_Base<double*> {
public:
  inline MatrixView(double* d, size_t rows, size_t cols):
    Matrix_Base<double*>(d, rows, cols) {}

  // Full view of the matrix
  template <typename D>
  inline MatrixView(Matrix_Base<D>& other):
    Matrix_Base<double*>(other.ptr(), other.rows, other.cols)
  {}

  // View to a matrix starting at column c
  template <typename D>
  inline MatrixView(Matrix_Base<D>& other, size_t c):
    Matrix_Base<double*>(other.ptr(c), other.rows(), other.cols()-c)  {}
};


// typedef Matrix_Base<double*> MatrixView;

/** A 3x3 matrix, stored column-major (see Matrix_Base).
 *
 *  As a Structure::cell, row i is the lattice vector a_i, and
 *  operator*(Vec3d) is the product with a column vector, so the Cartesian
 *  position of fractional coordinates f is cell.transpose() * f
 *  (= f0 a0 + f1 a1 + f2 a2) and f = cell.transpose().inverse() * r. */
template<typename T=std::array<double,9>>
class Matrix3d_T: public Matrix_Base<T> {
public:
  Matrix3d_T()
  {
    Matrix_Base<T>::set_rows(3);
    Matrix_Base<T>::set_cols(3);
    Matrix_Base<T>::set_zero();
  }
  Matrix3d_T(T &d, size_t r, size_t c):
    Matrix_Base<T>(d,r,c)
  {}
  /** A copy of row r. The copy is const, so `m.row(r) = v` (which would
   *  assign to the copy and leave m unchanged) does not compile; write
   *  elements through operator()(i, j). */
  inline const Vec3d row(size_t r) const
  {
    return Vec3d((*this)[r], (*this)[r+3], (*this)[r+6]);
  }
  /** A copy of column c; const for the same reason as row(). */
  inline const Vec3d col(size_t c) const
  {
    size_t s=3*c;
    return Vec3d((*this)[s], (*this)[s+1], (*this)[s+2]);
  }
  inline Matrix3d_T<T> transpose() const {
    Matrix3d_T<T> temp;
    for (size_t i=0; i<3; i++){
      for (size_t j=0; j<3; j++) {
        temp(j,i) = (*this)(i,j);
      }
    }
    return temp;
  }
  inline Vec3d operator*(const Vec3d &v) const
  {
    // naive matrix vector product
    //m is M x N matrix and v is N x 1 column vector
    Vec3d temp;
    temp.set_zero();
    for (size_t i=0; i<3; i++){
      for (size_t j=0; j<3; j++) {
        temp(i) += (*this)(i,j)*v(j);
      }
    }
    return temp;
  }
  /** The inverse, from the cofactors and the determinant.
   *  @throws std::domain_error, naming the determinant, when the
   *  determinant is 0 or not a finite number, or its reciprocal is not
   *  (the matrix has no inverse in double precision). */
  inline Matrix3d_T<T> inverse() const
  {
    // computes the inverse of a 3x3 matrix
    double det = (*this)(0, 0) * ((*this)(1, 1) * (*this)(2, 2) -
      (*this)(2, 1) * (*this)(1, 2)) -
      (*this)(0, 1) * ((*this)(1, 0) * (*this)(2, 2) -
      (*this)(1, 2) * (*this)(2, 0)) +
      (*this)(0, 2) * ((*this)(1, 0) * (*this)(2, 1) -
      (*this)(1, 1) * (*this)(2, 0));

    if (!detail::maths_is_finite(det) || det == 0.0 ||
        !detail::maths_is_finite(1 / det)) {
      std::ostringstream os;
      os.precision(17);
      os << "Matrix3d inverse: the determinant is " << det
         << ", so the matrix has no inverse in double precision";
      throw std::domain_error(os.str());
    }
    double invdet = 1 / det;

    Matrix3d_T<T> minv;
    minv(0, 0) = ((*this)(1, 1) * (*this)(2, 2) -
      (*this)(2, 1) * (*this)(1, 2)) * invdet;
    minv(0, 1) = ((*this)(0, 2) * (*this)(2, 1) -
      (*this)(0, 1) * (*this)(2, 2)) * invdet;
    minv(0, 2) = ((*this)(0, 1) * (*this)(1, 2) -
      (*this)(0, 2) * (*this)(1, 1)) * invdet;
    minv(1, 0) = ((*this)(1, 2) * (*this)(2, 0) -
      (*this)(1, 0) * (*this)(2, 2)) * invdet;
    minv(1, 1) = ((*this)(0, 0) * (*this)(2, 2) -
      (*this)(0, 2) * (*this)(2, 0)) * invdet;
    minv(1, 2) = ((*this)(1, 0) * (*this)(0, 2) -
      (*this)(0, 0) * (*this)(1, 2)) * invdet;
    minv(2, 0) = ((*this)(1, 0) * (*this)(2, 1) -
      (*this)(2, 0) * (*this)(1, 1)) * invdet;
    minv(2, 1) = ((*this)(2, 0) * (*this)(0, 1) -
      (*this)(0, 0) * (*this)(2, 1)) * invdet;
    minv(2, 2) = ((*this)(0, 0) * (*this)(1, 1) -
      (*this)(1, 0) * (*this)(0, 1)) * invdet;

    return minv;
  }
  // Element-wise sum with another 3x3 matrix. There is no scalar +=/-=:
  // adding a number to all nine entries is never meant for a cell or a
  // stress (s.stress -= p shifted the shear components too); write
  // m(k,k) -= p for each k.
  template <typename D, std::enable_if_t<!std::is_arithmetic<D>::value, int> = 0>
  inline Matrix3d_T<T> &operator+=(const D &m)
  {
    for (size_t i=0;i<9;++i) {
      Matrix_Base<T>::m_[i]+=m[i];
    }
    return *this;
  }
  template <typename D, std::enable_if_t<!std::is_arithmetic<D>::value, int> = 0>
  inline Matrix3d_T<T> &operator-=(const D &m)
  {
    for (size_t i=0;i<9;++i) {
      Matrix_Base<T>::m_[i]-=m[i];
    }
    return *this;
  }
  template <typename D, detail::enable_if_arithmetic<D> = 0>
  inline Matrix3d_T<T> &operator*=(const D fac)
  {
    for (size_t i=0;i<9;++i) {
      Matrix_Base<T>::m_[i]*=fac;
    }
    return *this;
  }
  template <typename D, detail::enable_if_arithmetic<D> = 0>
  inline Matrix3d_T<T> &operator/=(const D fac)
  {
    for (size_t i=0;i<9;++i) {
      Matrix_Base<T>::m_[i]/=fac;
    }
    return *this;
  }
  // Compute the Frobenius norm: sqrt( sum_{i=1..9} (m_[i])^2 ).
  inline double norm() const {
    double sum = 0.0;
    for (size_t i = 0; i < 9; ++i) {
      sum += Matrix_Base<T>::m_[i] * Matrix_Base<T>::m_[i];
    }
    return std::sqrt(sum);
  }
  static Matrix3d_T<T> Identity() {
    Matrix3d_T I;
    I.set_zero();
    I(0,0) = 1.0;
    I(1,1) = 1.0;
    I(2,2) = 1.0;
    return I;
  }
  inline friend Matrix3d_T<T> operator+(const Matrix3d_T<T> &m1, const Matrix3d_T<T> &m2)
  {
    Matrix3d_T<T> temp;
    for (size_t i=0;i<9;++i) {
      temp[i] = m1[i] + m2[i];
    }
    return temp;
  }
  inline friend Matrix3d_T<T> operator-(const Matrix3d_T<T> &m1, const Matrix3d_T<T> &m2)
  {
    Matrix3d_T<T> temp;
    for (size_t i=0;i<9;++i) {
      temp[i] = m1[i] - m2[i];
    }
    return temp;
  }
  /* The matrix product, row by column: (A*B)(i,j) = sum_k A(i,k) B(k,j). */
  inline friend Matrix3d_T<T> operator*(const Matrix3d_T<T> &A, const Matrix3d_T<T> &B)
  {
    Matrix3d_T<T> C;
    for (size_t i=0;i<3;++i)
      for (size_t j=0;j<3;++j)
        C(i,j) = A(i,0)*B(0,j) + A(i,1)*B(1,j) + A(i,2)*B(2,j);
    return C;
  }
  template <typename D, detail::enable_if_arithmetic<D> = 0>
  inline friend Matrix3d_T<T> operator*(const D &fac, const Matrix3d_T<T> &m1)
  {
    Matrix3d_T<T> temp;
    for (size_t i=0;i<9;++i) {
      temp[i] = m1[i] * fac;
    }
    return temp;
  }
  template <typename D, detail::enable_if_arithmetic<D> = 0>
  inline friend Matrix3d_T<T> operator/(const Matrix3d_T<T> &m1, const D &fac)
  {
    Matrix3d_T<T> temp;
    for (size_t i=0;i<9;++i) {
      temp[i] = m1[i] / fac;
    }
    return temp;
  }
  static Matrix3d_T<T> Zero() {
    Matrix3d_T<T> temp;
    temp.set_zero();
    return temp;
  }
};

class Mat6R3C {
public:
  static const int rows = 6;
  static const int cols = 3;
  double m[rows * cols] = {}; // Zero-initialize

  inline double &operator()(const size_t i, const size_t j) {
    return m[i * cols + j];
  }

  inline const double &operator()(const size_t i, const size_t j) const {
    return m[i * cols + j];
  }

  /** A copy of row r. The copy is const, so `m.row(r) = v` does not
   *  compile: it would assign to the copy and leave m unchanged (the MLIP
   *  DIMER descriptors were once computed from a zero matrix that way).
   *  Write a row with set_row(). */
  inline const Vec3d row(const size_t r) const {
    return Vec3d(m[r * cols], m[r * cols + 1], m[r * cols + 2]);
  }

  /** Set row r to v. */
  inline void set_row(const size_t r, const Vec3d &v) {
    m[r * cols]     = v[0];
    m[r * cols + 1] = v[1];
    m[r * cols + 2] = v[2];
  }

};

typedef Matrix3d_T<> Matrix3d;
} // namespace core
} // namespace tadah
#endif
