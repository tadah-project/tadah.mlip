#include <tadah/core/fd_type.h>
#include <tadah/core/aed_type.h>

#include <stdexcept>
#include <string>

namespace tadah {
namespace core {

void fd_type::view_columns() noexcept {
  // rebind, not assignment: an assignment to a column writes through.
  for (size_t k = 0; k < 3; ++k) {
    cols_[k].rebind(data_ == nullptr ? nullptr : data_ + k * rows_, rows_, 1);
  }
}

fd_type::fd_type(const fd_type& other) : rows_(other.rows_) {
  data_ = static_cast<double*>(malloc(3 * rows_ * sizeof(double)));
  std::memcpy(data_, other.data_, 3 * rows_ * sizeof(double));
  view_columns();
}

fd_type& fd_type::operator=(const fd_type& other) {
  if (this != &other) {
    free(data_);
    rows_ = other.rows_;
    data_ = static_cast<double*>(malloc(3 * rows_ * sizeof(double)));
    std::memcpy(data_, other.data_, 3 * rows_ * sizeof(double));
    view_columns();
  }
  return *this;
}

// The source keeps no storage: rows() 0 and three empty columns.
fd_type::fd_type(fd_type&& other) noexcept : rows_(other.rows_), data_(other.data_) {
  view_columns();
  other.data_ = nullptr;
  other.rows_ = 0;
  other.view_columns();
}

fd_type& fd_type::operator=(fd_type&& other) noexcept {
  if (this != &other) {
    free(data_);
    rows_ = other.rows_;
    data_ = other.data_;
    view_columns();
    other.data_ = nullptr;
    other.rows_ = 0;
    other.view_columns();
  }
  return *this;
}

aed_type& fd_type::operator[](size_t k) {
  return cols_[k];
}

const aed_type& fd_type::operator()(size_t k) const {
  return cols_[k];
}

aed_type& fd_type::operator()(size_t k) {
  return cols_[k];
}


double* fd_type::ptr() {
  return data_;
}

const double* fd_type::ptr() const {
  return data_;
}

std::ostream& operator<<(std::ostream& os, const fd_type& v) {
  // An fd_type without storage (default-constructed or moved from) prints
  // nothing; any other prints its three columns, one per line.
  if (v.data_ == nullptr) return os;
  for (const auto& c : v.cols_) {
    os << c << std::endl;
  }
  return os;
}

void fd_type::require_same_rows(const fd_type& v2, const char* op) const {
  if (v2.rows_ != rows_)
    throw std::invalid_argument(std::string("fd_type: operator") + op +
                                " of fd_types of " + std::to_string(rows_) +
                                " and " + std::to_string(v2.rows_) + " rows");
}

void fd_type::require_vector_size(size_t n) const {
  if (n != rows_)
    throw std::invalid_argument("fd_type: operator* of an fd_type of " +
                                std::to_string(rows_) +
                                " rows and a vector of " + std::to_string(n) +
                                " values");
}

fd_type fd_type::operator-(const fd_type& v2) const {
  require_same_rows(v2, "-");
  fd_type temp(rows_);
  for (size_t i = 0; i < 3 * rows_; ++i) {
    temp.data_[i] = data_[i] - v2.data_[i];
  }
  return temp;
}

fd_type fd_type::operator+(const fd_type& v2) const {
  require_same_rows(v2, "+");
  fd_type temp(rows_);
  for (size_t i = 0; i < 3 * rows_; ++i) {
    temp.data_[i] = data_[i] + v2.data_[i];
  }
  return temp;
}

void fd_type::operator*=(double s) {
  for (size_t i = 0; i < 3 * rows_; ++i) {
    data_[i] *= s;
  }
}

void fd_type::random(int seed, double lower, double upper) {
  std::uniform_real_distribution<double> unif(lower, upper);
  std::default_random_engine engine(seed);
  for (size_t i = 0; i < rows(); ++i) {
    (*this)(i, 0) = unif(engine);
    (*this)(i, 1) = unif(engine);
    (*this)(i, 2) = unif(engine);
  }
}

bool fd_type::isApprox(const fd_type& v1, double EPS) const {
  if (v1.rows_ != rows_) return false;  // different shapes are not equal
  for (size_t i = 0; i < 3 * rows_; ++i) {
    if (std::abs(data_[i] - v1.data_[i]) > EPS) return false;
  }
  return true;
}

bool fd_type::operator==(const fd_type& v1) const {
  double EPS = std::numeric_limits<double>::epsilon();
  return isApprox(v1, EPS);
}


} // namespace core
} // namespace tadah

