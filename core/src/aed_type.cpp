#include <tadah/core/aed_type.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace tadah {
namespace core {

namespace {

std::string view_note() {
  return " (a column of an fd_type or aeds_type2 is a view of its values)";
}

}  // namespace

void aed_type::assign_to_view(const aed_type& other) {
  if (other.size_ != size_)
    throw std::invalid_argument(
        "aed_type: cannot assign " + std::to_string(other.size_) +
        " values to a view of " + std::to_string(size_) + " values" +
        view_note());
  if (size_ == 0) return;
  // The two may share storage (an aeds_type2's columns, a view assigned to
  // itself): read every value before writing one.
  const double* const o_first = other.data_;
  const double* const o_last = other.data_ + (size_ - 1) * other.stride_;
  const double* const t_first = data_;
  const double* const t_last = data_ + (size_ - 1) * stride_;
  if (o_first <= t_last && t_first <= o_last) {
    std::vector<double> values(size_);
    for (size_t i = 0; i < size_; ++i) values[i] = other[i];
    for (size_t i = 0; i < size_; ++i) (*this)[i] = values[i];
  } else {
    for (size_t i = 0; i < size_; ++i) (*this)[i] = other[i];
  }
}

aed_type& aed_type::operator=(const aed_type& other) {
    if (this == &other) return *this;
    if (view_) {
        assign_to_view(other);
        return *this;
    }
    // The copy is made before the old buffer is freed: `other` may be a
    // view of this owner's values.
    double* const copy = compact_copy(other.data_, other.size_, other.stride_);
    if (allocated_) {
        free(data_);
    }
    data_ = copy;
    size_ = other.size_;
    stride_ = 1;
    allocated_ = true;
    return *this;
}

aed_type& aed_type::operator=(aed_type&& other) {
    if (this == &other) return *this;
    if (view_ || other.view_) {
        // Into a view: written through. From a view: an owning copy (the
        // view keeps its storage).
        return *this = static_cast<const aed_type&>(other);
    }
    if (allocated_) {
        free(data_);
    }
    data_ = other.data_;
    size_ = other.size_;
    stride_ = other.stride_;
    allocated_ = other.allocated_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.allocated_ = false;
    return *this;
}

void aed_type::rebind(double* data, size_t size, size_t stride) noexcept {
    if (allocated_) free(data_);
    data_ = data;
    size_ = size;
    stride_ = stride;
    allocated_ = false;
    view_ = true;
}

bool aed_type::isApprox(const aed_type& v1, double EPS) const {
    if (size() != v1.size()) return false;
    for (size_t i = 0; i < size(); ++i) {
        if (std::abs((*this)[i] - v1[i]) > EPS) return false;
    }
    return true;
}

double* aed_type::data() { return data_; }
const double* aed_type::data() const { return data_; }

void aed_type::resize(const size_t n) {
  if (view_) {
    if (n == size_) return;
    throw std::invalid_argument(
        "aed_type: cannot resize a view of " + std::to_string(size_) +
        " values to " + std::to_string(n) + view_note());
  }
  size_t size_orig = size_;
  if (n != size_) {
    if (allocated_) {
      data_ = static_cast<double*>(realloc(data_, stride_ * n * sizeof(double)));
    } else {
      data_ = static_cast<double*>(malloc(stride_ * n * sizeof(double)));
      allocated_ = true;
    }
    size_ = n;
  }
  // Zero-fill only when growing. On a shrink the old fill range
  // (data_+size_orig, data_+n) has first > last, and std::fill would run
  // off the end of the reallocated block (heap-buffer-overflow).
  if (n > size_orig) {
    std::fill(data_+stride_*size_orig, data_ + stride_ * n, 0.0);
  }
}

aed_type aed_type::copy(size_t nelements, size_t start) const {
    if (nelements + start > size()) {
        throw std::runtime_error("Out of range");
    }
    aed_type temp(nelements);
    for (size_t i = 0; i < nelements; ++i) {
        temp[i] = (*this)[start + i];
    }
    return temp;
}

double aed_type::mean() const {
  double total = 0;
  for (size_t i = 0; i < size(); ++i) {
    total += (*this)[i];
  }
  return total / size();
}

double aed_type::std_dev(double mean, double N) const {
  double variance = 0;
  for (size_t i = 0; i < size(); ++i) {
    double diff = (*this)[i] - mean;
    variance += diff * diff;
  }
  return std::sqrt(variance / N);
}

void aed_type::random(int seed, double lower, double upper) {
    std::uniform_real_distribution<double> unif(lower, upper);
    std::default_random_engine engine(seed);
    for (size_t i = 0; i < size(); ++i) {
        (*this)[i] = unif(engine);
    }
}

} // namespace core
} // namespace tadah

