#include <tadah/core/normaliser_core.h>
#include <tadah/core/intercept_layout.h>

namespace tadah {
namespace core {

Normaliser_Core::Normaliser_Core() : bias(0) {}
Normaliser_Core::Normaliser_Core(const Context &c)
    : bias(n_intercept_columns(c)) {
  if (c.contains("NORM") && c.get<bool>("NORM")) {
    if (c.contains("NSTDEV")) {
      std_dev.resize(c.size("NSTDEV"));
      mean.resize(c.size("NMEAN"));
      c.get<v_type>("NSTDEV", std_dev);
      c.get<v_type>("NMEAN", mean);
    }
  }
}

/** Normalise AED — skips the first `bias` columns (intercepts). */
void Normaliser_Core::normalise(aed_type &aed) {
  const size_t n = aed.size();
  for (size_t i = bias; i < n; ++i) {
    aed[i] = (aed[i] - mean[i]) / std_dev[i];
  }
}

/** Normalise FD — skips the first `bias` rows (intercepts have zero gradient). */
void Normaliser_Core::normalise(fd_type &fd) {
  const size_t n = fd.rows();
  for (size_t k = 0; k < 3; ++k) {
    for (size_t i = bias; i < n; ++i) {
      fd(i, k) = fd(i, k) / std_dev[i];
    }
  }
}
void Normaliser_Core::normalise(fd_type &fd, size_t k) {
  const size_t n = fd.rows();
  for (size_t i = bias; i < n; ++i) {
    fd(i, k) = fd(i, k) / std_dev[i];
  }
}

} // namespace core
} // namespace tadah
