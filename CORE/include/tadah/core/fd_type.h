#ifndef TADAH_CORE_FD_TYPE_H
#define TADAH_CORE_FD_TYPE_H
#include <tadah/core/aed_type.h>
#include <tadah/core/vec3d.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <random>
#include <cstring>
#include <limits>

namespace tadah {
namespace core {

/**
 * @brief Derivatives of a descriptor with respect to the three Cartesian
 *        directions: rows() values per direction, stored column by column.
 *
 * The element accessors, fd(i, k), fd[k] and fd(k), are the hot path and
 * are not checked: i < rows() and k < 3 are the caller's preconditions, for
 * every fd_type. A default-constructed or moved-from fd_type has rows() 0
 * and three empty columns (fd[k].size() is 0), as fd_type(0) has.
 * A column is a view of the fd_type's values (aed_type::is_view()): an
 * assignment to it, fd[k] = v, writes v's values into fd(i, k) (v must
 * have rows() values; std::invalid_argument otherwise), and it is not
 * resized.
 * The whole-object operations check their operands' sizes once per call:
 * +, - and * with a vector need an operand of rows() entries and throw
 * std::invalid_argument naming both sizes otherwise; == and isApprox are
 * false for fd_types of different sizes.
 */
class fd_type {
  size_t rows_ = 0;
  double* data_ = nullptr;
  // The three columns: views of data_ (they own no storage, see
  // aed_type), empty for a default-constructed or moved-from fd_type.
  aed_type cols_[3];

  // Points the three columns at data_, rows_ values each.
  void view_columns() noexcept;

public:

  inline fd_type() { view_columns(); }

  inline fd_type(size_t dim) : rows_(dim) {
    data_ = static_cast<double*>(malloc(3 * dim * sizeof(double)));
    std::fill(data_, data_ + 3 * rows_, 0.0);
    view_columns();
  }

  inline ~fd_type() {
    free(data_);
  }
  fd_type(const fd_type& other);
  fd_type(fd_type&& other) noexcept;

  fd_type& operator=(const fd_type& other);
  fd_type& operator=(fd_type&& other) noexcept;

  /// Row i of direction k; unchecked: requires i < rows() and k < 3.
  inline double& operator()(size_t i, size_t k) {
    return data_[i + k * rows_];
  }

  /// Row i of direction k; unchecked: requires i < rows() and k < 3.
  inline const double& operator()(size_t i, size_t k) const {
    return data_[i + k * rows_];
  }

  /// The rows() values of direction k, a view of this fd_type's values
  /// (empty when rows() is 0); unchecked: requires k < 3.
  aed_type& operator[](size_t k);
  /// The rows() values of direction k, a view of this fd_type's values
  /// (empty when rows() is 0); unchecked: requires k < 3.
  const aed_type& operator()(size_t k) const;
  /// The rows() values of direction k, a view of this fd_type's values
  /// (empty when rows() is 0); unchecked: requires k < 3.
  aed_type& operator()(size_t k);

  inline size_t rows() const { return rows_; }
  inline void set_zero() {
    std::fill(data_, data_ + 3 * rows_, 0.0);
  }
  double* ptr();
  const double* ptr() const;

  friend std::ostream& operator<<(std::ostream& os, const fd_type& v);

  /// (sum_i fd(i, k) v2[i]) for k = 0, 1, 2; v2 must have rows() values
  /// (std::invalid_argument otherwise).
  template <typename T>
  Vec3d operator*(const T& v2) const {
    require_vector_size(v2.size());
    Vec3d temp;
    for (size_t k = 0; k < 3; ++k) {
      for (size_t i = 0; i < v2.size(); ++i) {
        temp[k] += (*this)(i, k) * v2[i];
      }
    }
    return temp;
  }

  /// Elementwise; v2 must have rows() rows (std::invalid_argument
  /// otherwise).
  fd_type operator-(const fd_type& v2) const;
  /// Elementwise; v2 must have rows() rows (std::invalid_argument
  /// otherwise).
  fd_type operator+(const fd_type& v2) const;

  void operator*=(double s);
  void random(int seed = std::chrono::system_clock::now().time_since_epoch().count(),
              double lower = -1e4, double upper = 1e4);

  /// Every value within EPS of v1's; false when v1 has another size.
  bool isApprox(const fd_type& v1, double EPS) const;

  /// isApprox with the machine epsilon; false for another size.
  bool operator==(const fd_type& v1) const;

private:
  void require_same_rows(const fd_type& v2, const char* op) const;
  void require_vector_size(size_t n) const;
};

} // namespace core
} // namespace tadah
#endif // FD_TYPE_H

