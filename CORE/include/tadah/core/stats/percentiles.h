#ifndef TADAH_CORE_STATS_PERCENTILES_H
#define TADAH_CORE_STATS_PERCENTILES_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace tadah {
namespace core {
namespace stats {

/** \brief Linear-interpolation percentile of a numeric sample.
 *
 * Implements the same definition NumPy uses with method="linear":
 * given n sorted values v[0] <= v[1] <= ... <= v[n-1] and a quantile
 * q in [0, 1], compute h = q * (n - 1), then linearly interpolate
 * between v[floor(h)] and v[ceil(h)].
 *
 * The input range is copied internally so the caller's data is not
 * disturbed. For repeated queries on the same sample, prefer the
 * \ref multi_percentile overload which sorts once.
 *
 * Throws std::invalid_argument when n == 0 or q is outside [0, 1].
 *
 * \param data pointer to n consecutive doubles (must not be null when n>0)
 * \param n    number of doubles
 * \param q    quantile in [0, 1]
 */
inline double percentile(const double* data, std::size_t n, double q) {
  if (n == 0) {
    throw std::invalid_argument("percentile: empty sample");
  }
  if (!(q >= 0.0 && q <= 1.0)) {
    throw std::invalid_argument("percentile: q must be in [0, 1]");
  }
  std::vector<double> buf(data, data + n);
  std::sort(buf.begin(), buf.end());
  if (n == 1) return buf[0];
  const double h = q * static_cast<double>(n - 1);
  const std::size_t lo = static_cast<std::size_t>(std::floor(h));
  const std::size_t hi = static_cast<std::size_t>(std::ceil(h));
  if (lo == hi) return buf[lo];
  const double frac = h - static_cast<double>(lo);
  return buf[lo] + frac * (buf[hi] - buf[lo]);
}

/** Convenience wrapper for std::vector<double>. */
inline double percentile(const std::vector<double>& v, double q) {
  return percentile(v.data(), v.size(), q);
}

/** \brief Compute many percentiles at once with a single sort.
 *
 * Returns one value per requested q in qs, in the same order. The
 * underlying buffer is sorted once; each q lookup is O(log n).
 */
inline std::vector<double>
multi_percentile(const double* data, std::size_t n,
                 const std::vector<double>& qs) {
  if (n == 0) {
    throw std::invalid_argument("multi_percentile: empty sample");
  }
  std::vector<double> buf(data, data + n);
  std::sort(buf.begin(), buf.end());
  std::vector<double> out;
  out.reserve(qs.size());
  for (double q : qs) {
    if (!(q >= 0.0 && q <= 1.0)) {
      throw std::invalid_argument("multi_percentile: q must be in [0, 1]");
    }
    if (n == 1) { out.push_back(buf[0]); continue; }
    const double h = q * static_cast<double>(n - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(h));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(h));
    if (lo == hi) { out.push_back(buf[lo]); continue; }
    const double frac = h - static_cast<double>(lo);
    out.push_back(buf[lo] + frac * (buf[hi] - buf[lo]));
  }
  return out;
}

inline std::vector<double>
multi_percentile(const std::vector<double>& v,
                 const std::vector<double>& qs) {
  return multi_percentile(v.data(), v.size(), qs);
}

/** \brief Canonical 11-percentile summary used throughout the audit
 *  pipeline (DataStats etc.): {p1, p5, p10, p25, p50, p75, p90, p95, p99,
 *  mean, std}.
 *
 *  Indices match the IDX_* constants of DataStats (data_stats.h), which
 *  the descriptors' metadata use: e.g. p10 is index 2 (IDX_P10).
 */
inline std::array<double, 11>
summary_percentiles(const double* data, std::size_t n) {
  if (n == 0) {
    throw std::invalid_argument("summary_percentiles: empty sample");
  }
  static const std::vector<double> qs{0.01, 0.05, 0.10, 0.25, 0.50,
                                       0.75, 0.90, 0.95, 0.99};
  auto pcts = multi_percentile(data, n, qs);
  // Compute mean / std in a numerically stable way (Welford).
  double mean = 0.0;
  double m2   = 0.0;
  std::size_t k = 0;
  for (std::size_t i = 0; i < n; ++i) {
    ++k;
    const double x = data[i];
    const double dx = x - mean;
    mean += dx / static_cast<double>(k);
    m2   += dx * (x - mean);
  }
  const double var = (n > 1) ? (m2 / static_cast<double>(n - 1)) : 0.0;
  const double sd  = std::sqrt(var);
  std::array<double, 11> out{};
  for (std::size_t i = 0; i < 9; ++i) out[i] = pcts[i];
  out[9]  = mean;
  out[10] = sd;
  return out;
}

inline std::array<double, 11>
summary_percentiles(const std::vector<double>& v) {
  return summary_percentiles(v.data(), v.size());
}

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_PERCENTILES_H
