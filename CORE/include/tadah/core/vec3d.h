// #ifndef TADAH_CORE_VEC3_H
// #define TADAH_CORE_VEC3_H
//
// #include <cmath>
// #include <iostream>
// #include <limits>
// #include <type_traits>
//
// namespace tadah {
// namespace core {
//
// template<class T = double>
// class Vec3
// {
// public:
//     T v[3] = {T(), T(), T()};
//
//     /* ------------------------------------------------------------------ */
//     Vec3() = default;
//
//     template<class U>
//     explicit Vec3(const U& rhs) { v[0]=rhs[0]; v[1]=rhs[1]; v[2]=rhs[2]; }
//
//     Vec3(T x, T y, T z) { v[0]=x; v[1]=y; v[2]=z; }
//
//     /* ------------------------------------------------------------------ */
//     /* element access                                                     */
//     inline       T& operator[](std::size_t i)       { return v[i]; }
//     inline const T& operator[](std::size_t i) const { return v[i]; }
//     inline       T& operator()(std::size_t i)       { return v[i]; }
//     inline const T& operator()(std::size_t i) const { return v[i]; }
//
//     /* ------------------------------------------------------------------ */
//     /* unary                                                                */
//     inline Vec3 operator-() const { return Vec3(-v[0],-v[1],-v[2]); }
//     inline void set_zero()        { v[0]=v[1]=v[2]=T(); }
//
//     /* ------------------------------------------------------------------ */
//     /* compound ops                                                        */
//     template<class U> Vec3& operator+=(const U& w)
//     { v[0]+=w[0]; v[1]+=w[1]; v[2]+=w[2]; return *this; }
//
//     template<class U> Vec3& operator-=(const U& w)
//     { v[0]-=w[0]; v[1]-=w[1]; v[2]-=w[2]; return *this; }
//
//     Vec3& operator*=(const T s) { v[0]*=s; v[1]*=s; v[2]*=s; return *this; }
//     Vec3& operator/=(const T s) { v[0]/=s; v[1]/=s; v[2]/=s; return *this; }
//
//     /* ------------------------------------------------------------------ */
//     /* binary ops with another Vec3 (any scalar types)                     */
//     template<class U>
//     auto operator+(const Vec3<U>& w) const
//         -> Vec3<decltype(T()+U())>
//     { return {v[0]+w[0], v[1]+w[1], v[2]+w[2]}; }
//
//     template<class U>
//     auto operator-(const Vec3<U>& w) const
//         -> Vec3<decltype(T()+U())>
//     { return {v[0]-w[0], v[1]-w[1], v[2]-w[2]}; }
//
//     template<class U>
//     auto operator*(const Vec3<U>& w) const        // dot product
//         -> decltype(T()*U())
//     { return v[0]*w[0] + v[1]*w[1] + v[2]*w[2]; }
//
//     /* ------------------------------------------------------------------ */
//     /* scalar ops                                                          */
//     template<class S,
//              typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
//     Vec3 operator*(S s) const { return {v[0]*s, v[1]*s, v[2]*s}; }
//
//     template<class S,
//              typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
//     Vec3 operator/(S s) const { return {v[0]/s, v[1]/s, v[2]/s}; }
//
//     template<class S,
//              typename = typename std::enable_if<std::is_arithmetic<S>::value>::type>
//     friend Vec3 operator*(S s, const Vec3& w) { return w*s; }
//
//     /* ------------------------------------------------------------------ */
//     /* misc                                                                */
//     template<class U>
//     Vec3& operator=(const U& w){ v[0]=w[0]; v[1]=w[1]; v[2]=w[2]; return *this;}
//
//     Vec3 cross(const Vec3& w) const
//     {
//         return { v[1]*w[2]-v[2]*w[1],
//                 -(v[0]*w[2]-v[2]*w[0]),
//                  v[0]*w[1]-v[1]*w[0] };
//     }
//
//     auto norm()   const -> decltype(sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]))
//     { return sqrt((*this)*(*this)); }               // ADL picks std::sqrt
//
//     auto squaredNorm() const -> decltype((*this)*(*this))
//     { return (*this)*(*this); }
//
//     friend std::ostream& operator<<(std::ostream& os, const Vec3& w)
//     { return os << w[0] << "  " << w[1] << "  " << w[2]; }
//
//     bool isApprox(const Vec3& w, double eps = 1e-12) const
//     { return std::fabs(v[0]-w[0])<eps &&
//              std::fabs(v[1]-w[1])<eps &&
//              std::fabs(v[2]-w[2])<eps; }
//
//     bool operator==(const Vec3& w) const { return isApprox(w); }
//     bool operator!=(const Vec3& w) const { return !(*this==w); }
// };
//
// /* keep old alias ---------------------------------------------------- */
// using Vec3d = Vec3<double>;
//
// } // namespace core
// } // namespace tadah
// #endif




#ifndef TADAH_CORE_VEC3D_H
#define TADAH_CORE_VEC3D_H

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <type_traits>
#include <utility>

namespace tadah {
namespace core {

template <typename D> class Matrix_Base;  // maths.h

namespace detail {
std::false_type is_matrix_base_ptr(...);
template <typename D>
std::true_type is_matrix_base_ptr(const Matrix_Base<D>*);
/** True for Matrix_Base<D> and every type derived from it (Matrix,
 *  Matrix3d, MatrixView). */
template <typename T>
struct is_matrix : decltype(is_matrix_base_ptr(
    std::declval<typename std::remove_cv<
        typename std::remove_reference<T>::type>::type*>())) {};
/** A type a Vec3d converts from, adds or subtracts element-wise: any type
 *  indexable as t[0], t[1], t[2], except a matrix. A matrix's operator[]
 *  indexes its column-major storage, so elements 0..2 are column 0 --
 *  for a Structure::cell (a0x, a1x, a2x), not a lattice vector. Read a
 *  row or a column with row() or col(). */
template <typename T>
using enable_if_vector_like =
    typename std::enable_if<!is_matrix<T>::value, int>::type;
template <typename T>
using enable_if_arithmetic =
    typename std::enable_if<std::is_arithmetic<T>::value, int>::type;
}  // namespace detail

class Vec3d {
public:
  double v[3] = { };
  Vec3d() {}

  template <typename T, detail::enable_if_vector_like<T> = 0>
  Vec3d(T &v1)
  {
    v[0]=v1[0]; v[1]=v1[1]; v[2]=v1[2];
  }
  Vec3d(const double x, const double y, const double z)
  {
    v[0]=x; v[1]=y; v[2]=z;
  }

  inline Vec3d operator-() const
  {
    return Vec3d(-v[0],-v[1],-v[2]);
  }
  inline double &operator[](size_t i)
  {
    return v[i];
  }
  inline double &operator()(size_t i)
  {
    return v[i];
  }
  inline const double &operator[](size_t i) const
  {
    return v[i];
  }
  inline const double &operator()(size_t i) const
  {
    return v[i];
  }
  template <typename T, detail::enable_if_vector_like<T> = 0>
  inline Vec3d &operator+=(const T &v1)
  {
    v[0]+=v1[0];
    v[1]+=v1[1];
    v[2]+=v1[2];
    return *this;
  }
  template <typename T, detail::enable_if_vector_like<T> = 0>
  inline Vec3d &operator-=(const T &v1)
  {
    v[0]-=v1[0];
    v[1]-=v1[1];
    v[2]-=v1[2];
    return *this;
  }
  inline Vec3d &operator*=(const double s)
  {
    v[0]*=s;
    v[1]*=s;
    v[2]*=s;
    return *this;
  }
  inline Vec3d &operator/=(const double s)
  {
    v[0]/=s;
    v[1]/=s;
    v[2]/=s;
    return *this;
  }
  inline Vec3d operator-(const Vec3d &v2) const
  {
    return Vec3d((*this)[0]-v2[0], (*this)[1]-v2[1], (*this)[2]-v2[2]);
  }
  inline Vec3d operator+(const Vec3d &v2) const
  {
    return Vec3d((*this)[0]+v2[0], (*this)[1]+v2[1], (*this)[2]+v2[2]);
  }
  inline double operator*(const Vec3d &v2) const
  {
    return (*this)[0]*v2[0] + (*this)[1]*v2[1] + (*this)[2]*v2[2];
  }
  inline double dot(const Vec3d &other) const {
    return v[0]*other[0] + v[1]*other[1] + v[2]*other[2];
  }
  template <typename T, detail::enable_if_arithmetic<T> = 0>
  inline Vec3d operator*(const T s) const
  {
    return Vec3d((*this)[0]*s, (*this)[1]*s, (*this)[2]*s);
  }
  template <typename T, detail::enable_if_arithmetic<T> = 0>
  inline Vec3d operator/(const T s) const
  {
    return Vec3d((*this)[0]/s, (*this)[1]/s, (*this)[2]/s);
  }
  template <typename T>
  inline friend Vec3d operator*(const double s, const T &v)
  {
    return Vec3d(v[0]*s, v[1]*s, v[2]*s);
  }
  template <typename T>
  inline friend Vec3d operator/(const double s, const T &v)
  {
    return Vec3d(s/v[0], s/v[1], s/v[2]);
  }
  inline void set_zero() { v[0]=0; v[1]=0; v[2]=0; }

  inline friend std::ostream &operator<<(std::ostream &os, const Vec3d &v)
  {
    os << v[0] << "  " << v[1] << "  " << v[2];
    return os;
  }
  bool isApprox(const Vec3d &v1, double EPS) const{
    if (std::abs(v[0]-v1[0])>EPS) return false;
    if (std::abs(v[1]-v1[1])>EPS) return false;
    if (std::abs(v[2]-v1[2])>EPS) return false;
    return true;
  }
  inline bool operator==(const Vec3d &v1) const
  {
    double EPS = std::numeric_limits<double>::epsilon();
    return this->isApprox(v1, EPS);
  }
  inline bool operator!=(const Vec3d &v1) const
  {
    return !(*this==v1);
  }
  template <typename T, detail::enable_if_vector_like<T> = 0>
  inline Vec3d &operator=(const T &v1)
  {
    v[0] = v1[0];
    v[1] = v1[1];
    v[2] = v1[2];
    return *this;
  }
  inline void operator()(const double x, const double y, const double z)
  {
    v[0]=x;
    v[1]=y;
    v[2]=z;
  }
  inline Vec3d cross(const Vec3d &v) const
  {
    Vec3d temp;
    temp[0] = (*this)[1] * v[2] - (*this)[2] * v[1];
    temp[1] = -((*this)[0] * v[2] - (*this)[2] * v[0]);
    temp[2] = (*this)[0] * v[1] - (*this)[1] * v[0];
    return temp;
  }

  inline double norm() const {
    return std::sqrt((*this)*(*this));
  }
  inline double squaredNorm() const {
    return (*this)*(*this);
  }
};
} // namespace core
} // namespace tadah
#endif
