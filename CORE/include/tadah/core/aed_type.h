#ifndef TADAH_CORE_AED_TYPE2_H
#define TADAH_CORE_AED_TYPE2_H
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <limits>
#include <random>
#include <vector>
#include <iostream>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace tadah {
namespace core {

/**
 * @brief A vector of doubles that owns its values, or a view of values it
 *        does not own (a column of an fd_type or an aeds_type2).
 *
 * A view is made by the non-owning pointer constructor (allocated false)
 * or rebind(). An assignment to a view writes the other's values into the
 * viewed storage, whatever the other is (copy or move): the sizes must
 * match (std::invalid_argument otherwise), and a view is not resized to
 * another size. An assignment to an owning (or default-constructed)
 * aed_type replaces its values. Moving from a view copies its values (a
 * view owns nothing to give) and leaves it as it is.
 *
 * A copy (the copy constructor, an assignment into an owner, a move from
 * a view, and the const double* constructor) is an owner of the size()
 * values one after another (stride 1), read through the source's stride:
 * a strided source (Matrix::row, size() values rows() apart) holds only
 * the values it views, so a copy of size() * stride doubles from its first
 * value would read past them.
 *
 * begin() to end() visit the size() values through the stride, as
 * operator[] does (random-access iterators); data() and ptr() are the
 * first value only: of a strided aed_type the next value is not data()[1].
 */
class aed_type {
public:
  /** A random-access iterator over values `stride` apart from `base`:
   *  element i is base[i * stride]. It holds the base and an index, so an
   *  end() of a strided view forms no pointer past the viewed storage. */
  template <typename T>
  class strided_iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;  ///< Random access.
    using value_type = std::remove_const_t<T>;  ///< double.
    using difference_type = std::ptrdiff_t;     ///< A count of values.
    using pointer = T*;                         ///< To a value.
    using reference = T&;                       ///< To a value.

    strided_iterator() = default;
    /// Element @p index of the values @p stride apart from @p base.
    strided_iterator(T* base, size_t stride, difference_type index) noexcept
    : base_(base), stride_(static_cast<difference_type>(stride)),
      index_(index) {}
    /// An iterator over const values from one over the same values.
    template <typename U,
              typename = std::enable_if_t<std::is_same_v<const U, T>>>
    strided_iterator(const strided_iterator<U>& o) noexcept
    : base_(o.base_), stride_(o.stride_), index_(o.index_) {}

    /// The value at the iterator.
    reference operator*() const { return base_[index_ * stride_]; }
    /// Its address.
    pointer operator->() const { return base_ + index_ * stride_; }
    /// The value @p n elements on.
    reference operator[](difference_type n) const {
      return base_[(index_ + n) * stride_];
    }

    /// The next element.
    strided_iterator& operator++() noexcept { ++index_; return *this; }
    /// The next element; returns this one.
    strided_iterator operator++(int) noexcept {
      strided_iterator t = *this; ++index_; return t;
    }
    /// The previous element.
    strided_iterator& operator--() noexcept { --index_; return *this; }
    /// The previous element; returns this one.
    strided_iterator operator--(int) noexcept {
      strided_iterator t = *this; --index_; return t;
    }
    /// @p n elements on.
    strided_iterator& operator+=(difference_type n) noexcept {
      index_ += n; return *this;
    }
    /// @p n elements back.
    strided_iterator& operator-=(difference_type n) noexcept {
      index_ -= n; return *this;
    }
    /// @p n elements on from @p it.
    friend strided_iterator operator+(strided_iterator it,
                                      difference_type n) noexcept {
      return it += n;
    }
    /// @p n elements on from @p it.
    friend strided_iterator operator+(difference_type n,
                                      strided_iterator it) noexcept {
      return it += n;
    }
    /// @p n elements back from @p it.
    friend strided_iterator operator-(strided_iterator it,
                                      difference_type n) noexcept {
      return it -= n;
    }
    /// Iterators over the same values: the number of values between them.
    friend difference_type operator-(const strided_iterator& a,
                                     const strided_iterator& b) noexcept {
      return a.index_ - b.index_;
    }
    /// The same element of the same values.
    friend bool operator==(const strided_iterator& a,
                           const strided_iterator& b) noexcept {
      return a.base_ == b.base_ && a.index_ == b.index_;
    }
    /// Not the same element.
    friend bool operator!=(const strided_iterator& a,
                           const strided_iterator& b) noexcept {
      return !(a == b);
    }
    /// An earlier element of the same values.
    friend bool operator<(const strided_iterator& a,
                          const strided_iterator& b) noexcept {
      return a.index_ < b.index_;
    }
    /// A later element of the same values.
    friend bool operator>(const strided_iterator& a,
                          const strided_iterator& b) noexcept {
      return b < a;
    }
    /// Not a later element of the same values.
    friend bool operator<=(const strided_iterator& a,
                           const strided_iterator& b) noexcept {
      return !(b < a);
    }
    /// Not an earlier element of the same values.
    friend bool operator>=(const strided_iterator& a,
                           const strided_iterator& b) noexcept {
      return !(a < b);
    }

  private:
    template <typename> friend class strided_iterator;
    T* base_ = nullptr;
    difference_type stride_ = 1;
    difference_type index_ = 0;
  };
  using iterator = strided_iterator<double>;              ///< Over the values.
  using const_iterator = strided_iterator<const double>;  ///< Over const values.

private:
  double *data_ = nullptr;
  size_t size_ = 0;
  size_t stride_ = 1;
  bool allocated_ = false;
  bool view_ = false;   // values of storage another object owns

  // The values of `other` into this view; sizes must match.
  void assign_to_view(const aed_type& other);

  // A new (malloc'ed) buffer of the `n` values `stride` apart in `src`,
  // one after another.
  static double* compact_copy(const double* src, size_t n, size_t stride) {
    double* out = static_cast<double*>(malloc(n * sizeof(double)));
    if (stride == 1) {
      if (n > 0) std::memcpy(out, src, n * sizeof(double));
    } else {
      for (size_t i = 0; i < n; ++i) out[i] = src[i * stride];
    }
    return out;
  }

public:

  inline aed_type(std::initializer_list<double> init) {
    size_ = init.size();
    data_ = static_cast<double*>(malloc(size_ * sizeof(double)));
    allocated_=true;
    size_t i = 0;
    for (double value : init) {
      data_[i++] = value;
    }
  }

  inline aed_type() : data_(nullptr), size_(0), stride_(1), allocated_(false) {}

  inline aed_type(size_t n, size_t s = 1)
  : size_(n), stride_(s), allocated_(true) {
    data_ = static_cast<double*>(malloc(s * n * sizeof(double)));
    std::fill(data_, data_ + s * n, 0.0);
  }

  /// An owning copy of @p size values of @p arr, @p stride apart (stride
  /// 1 in the copy).
  inline aed_type(const double* arr, size_t size, size_t stride = 1)
  : data_(compact_copy(arr, size, stride)), size_(size), stride_(1),
    allocated_(true) {}

  /// A view of @p size values of @p arr, @p stride apart (allocated
  /// false), or, with allocated true, an owner of @p arr: a buffer of
  /// size * stride doubles from malloc (or calloc/realloc), which it frees
  /// with free() and resize() reallocates to n * stride doubles. Only a
  /// null buffer of size > 0 can be checked (std::invalid_argument); its
  /// size and origin cannot be. No Tadah! module adopts a buffer.
  inline aed_type(double* arr, size_t size, size_t stride = 1, bool allocated = false)
  : data_(arr), size_(size), stride_(stride), allocated_(allocated),
    view_(!allocated) {
    if (allocated && arr == nullptr && size > 0)
      throw std::invalid_argument(
          "aed_type: cannot own a null buffer of " + std::to_string(size) +
          " values, " + std::to_string(stride) + " apart (the adopting "
          "constructor takes a buffer of size * stride doubles from malloc)");
  }

  /// An owning copy (stride 1), also of a view.
  inline aed_type(const aed_type& other)
  : data_(compact_copy(other.data_, other.size_, other.stride_)),
    size_(other.size_), stride_(1), allocated_(true) {}

  /// Takes the other's buffer, leaving it empty; from a view, an owning
  /// copy (stride 1; the view keeps viewing its storage).
  inline aed_type(aed_type&& other) noexcept
  : data_(other.data_), size_(other.size_), stride_(other.stride_), allocated_(other.allocated_) {
    if (other.view_) {
      allocated_ = true;
      stride_ = 1;
      data_ = compact_copy(other.data_, size_, other.stride_);
      return;
    }
    other.data_ = nullptr;
    other.size_ = 0;
    other.allocated_ = false;
  }

  inline ~aed_type() {
    if (allocated_) {
      free(data_);
    }
  }

  /// Into a view: the other's values written through (sizes must match,
  /// std::invalid_argument otherwise); into an owner: an owning copy
  /// (stride 1), also of a view of the owner's own values.
  aed_type& operator=(const aed_type& other);
  /// Into a view: as the copy assignment. Into an owner: takes the
  /// other's buffer (an owning copy of a view, stride 1), leaving it empty.
  aed_type& operator=(aed_type&& other);

  /// Makes this aed_type a view of @p size values of @p data, @p stride
  /// apart, freeing what it owned (fd_type and aeds_type2 point their
  /// columns at their storage with it).
  void rebind(double* data, size_t size, size_t stride = 1) noexcept;

  /// True for a view (rebind(), or the non-owning pointer constructor).
  bool is_view() const noexcept { return view_; }

  inline double& operator[](size_t i) {
    return data_[i * stride_];
  }

  inline const double& operator[](size_t i) const {
    return data_[i * stride_];
  }

  inline double& operator()(size_t i) {
    return data_[i * stride_];
  }

  inline const double& operator()(size_t i) const {
    return data_[i * stride_];
  }

  template <typename T>
  inline aed_type& operator+=(const T& v1) {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] += v1[i];
    }
    return *this;
  }

  inline aed_type& operator+=(const double d) {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] += d;
    }
    return *this;
  }

  inline aed_type operator+(const double d) const {
    aed_type temp(*this);
    for (size_t i = 0; i < size(); ++i) {
      temp[i] += d;
    }
    return temp;
  }

  template <typename T>
  inline aed_type& operator-=(const T& v1) {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] -= v1[i];
    }
    return *this;
  }

  inline aed_type& operator-=(const double d) {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] -= d;
    }
    return *this;
  }

  inline aed_type operator-(const double d) const {
    aed_type temp(*this);
    for (size_t i = 0; i < size(); ++i) {
      temp[i] -= d;
    }
    return temp;
  }

  inline aed_type& operator*=(const double s) {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] *= s;
    }
    return *this;
  }

  template <typename T>
  inline aed_type operator-(const T& v2) const {
    aed_type temp(*this);
    for (size_t i = 0; i < size(); ++i) {
      temp[i] -= v2[i];
    }
    return temp;
  }

  template <typename T>
  inline aed_type operator+(const T& v2) const {
    aed_type temp(*this);
    for (size_t i = 0; i < size(); ++i) {
      temp[i] += v2[i];
    }
    return temp;
  }

  template <typename T>
  inline double operator*(const T& v2) const {
    double result = 0;
    for (size_t i = 0; i < size(); ++i) {
      result += (*this)[i] * v2[i];
    }
    return result;
  }

  inline aed_type operator*(const double s) const {
    aed_type temp(*this);
    for (size_t i = 0; i < size(); ++i) {
      temp[i] *= s;
    }
    return temp;
  }

  friend inline aed_type operator*(const double s, const aed_type& v) {
    aed_type temp(v);
    for (size_t i = 0; i < v.size(); ++i) {
      temp[i] *= s;
    }
    return temp;
  }

  inline void set_zero() {
    for (size_t i = 0; i < size(); ++i) {
      (*this)[i] = 0;
    }
  }

  inline size_t size() const {
    return size_;
  }

  inline size_t rows() const {
    return size_;
  }

  bool isApprox(const aed_type& v1, double EPS) const;

  inline bool operator==(const aed_type& v1) const {
    if (size() != v1.size()) return false;
    double EPS = std::numeric_limits<double>::epsilon();
    return isApprox(v1, EPS);
  }

  inline bool operator!=(const aed_type& v1) const {
    return !(*this == v1);
  }

  /// Resizes an owner (new values 0). A view keeps its size: resizing it
  /// to another size throws std::invalid_argument.
  void resize(const size_t n);

  double* data();
  const double* data() const;

  aed_type copy(size_t nelements, size_t start = 0) const;

  inline double* ptr() {
    return data_;
  }

  double mean() const;
  double std_dev(double mean, double N) const;

  void random(int seed = std::chrono::system_clock::now().time_since_epoch().count(),
              double lower = -1e4, double upper = 1e4);

  /// The first value (begin() to end(): operator[]'s values, through the
  /// stride).
  inline iterator begin() noexcept { return iterator(data_, stride_, 0); }
  /// Past the last value.
  inline iterator end() noexcept {
    return iterator(data_, stride_, static_cast<std::ptrdiff_t>(size_));
  }
  /// The first value.
  inline const_iterator begin() const noexcept {
    return const_iterator(data_, stride_, 0);
  }
  /// Past the last value.
  inline const_iterator end() const noexcept {
    return const_iterator(data_, stride_, static_cast<std::ptrdiff_t>(size_));
  }
  /// The first value.
  inline const_iterator cbegin() const noexcept { return begin(); }
  /// Past the last value.
  inline const_iterator cend() const noexcept { return end(); }
};

inline std::ostream& operator<<(std::ostream& os, const aed_type& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        os << v[i];
        if (i < v.size() - 1) os << "  ";
    }
    return os;
}


// class aeds_type2 {
// public:
//     std::vector<aed_type> data_;
//
//     inline aeds_type2() {}
//
//     inline aeds_type2(size_t dim, size_t natoms)
//         : data_(natoms, aed_type(dim)) {}
//
//     inline double& operator()(size_t r, size_t c) {
//         return data_[c][r];
//     }
//
//     inline aed_type& operator[](size_t i) {
//         return data_[i];
//     }
//
//     inline size_t cols() const {
//         return data_.size();
//     }
//
//     inline size_t rows() const {
//         return data_[0].size();
//     }
//
//     inline size_t size() const {
//         return data_.size();
//     }
//
//     inline aed_type& col(size_t i) {
//         return data_[i];
//     }
//
//     inline const aed_type& col(size_t i) const {
//         return data_[i];
//     }
//
//     void resize(size_t dim, size_t natoms);
//     void set_zero();
//
//     friend std::ostream& operator<<(std::ostream& os, const aeds_type2& v) {
//         for (const aed_type& a : v.data_) os << a << std::endl;
//         return os;

//     }
// };

} // namespace core
} // namespace tadah
#endif // AED_TYPE2_H

