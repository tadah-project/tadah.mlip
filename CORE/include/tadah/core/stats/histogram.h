#ifndef TADAH_CORE_STATS_HISTOGRAM_H
#define TADAH_CORE_STATS_HISTOGRAM_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include <tadah/core/stats/percentiles.h>

namespace tadah {
namespace core {
namespace stats {

/** \brief Fixed-bin histogram on a finite real interval.
 *
 *  The lower edge of bin i is `low + i * width`. The upper edge of the
 *  last bin coincides with `high`. Values strictly outside [low, high)
 *  are dropped (the histogram counts samples; it is not concerned with
 *  out-of-range diagnostics, which are the caller's responsibility).
 */
struct Histogram {
  double low{0.0};
  double high{0.0};
  std::size_t n_bins{0};
  std::vector<std::size_t> counts;   // size == n_bins

  double bin_width() const {
    return n_bins ? (high - low) / static_cast<double>(n_bins) : 0.0;
  }
  double left_edge(std::size_t i) const {
    return low + static_cast<double>(i) * bin_width();
  }
  double centre(std::size_t i) const {
    return low + (static_cast<double>(i) + 0.5) * bin_width();
  }
  std::size_t total_count() const {
    std::size_t s = 0;
    for (auto c : counts) s += c;
    return s;
  }
};

/** \brief Build a fixed-bin histogram with explicit n_bins. */
inline Histogram
histogram(const double* data, std::size_t n,
          double low, double high, std::size_t n_bins) {
  if (n_bins == 0) {
    throw std::invalid_argument("histogram: n_bins must be > 0");
  }
  if (!(high > low)) {
    throw std::invalid_argument("histogram: high must exceed low");
  }
  Histogram h;
  h.low = low;
  h.high = high;
  h.n_bins = n_bins;
  h.counts.assign(n_bins, 0);
  const double w = (high - low) / static_cast<double>(n_bins);
  for (std::size_t i = 0; i < n; ++i) {
    const double x = data[i];
    if (x < low || x >= high) continue;
    const std::size_t b = static_cast<std::size_t>((x - low) / w);
    h.counts[std::min(b, n_bins - 1)] += 1;
  }
  return h;
}

inline Histogram
histogram(const std::vector<double>& v,
          double low, double high, std::size_t n_bins) {
  return histogram(v.data(), v.size(), low, high, n_bins);
}

/** \brief Freedman-Diaconis-binned histogram.
 *
 *  Bin width  h = 2 * IQR / N^(1/3),
 *  Bin count  k = ceil((max - min) / h).
 *
 *  Falls back to Sturges' rule (k = ceil(log2(n) + 1)) when IQR == 0
 *  (e.g. the sample is constant); both rules are formula-derived, no
 *  magic numbers escape this header.
 *
 *  When the input is empty an empty Histogram (n_bins=0) is returned.
 */
inline Histogram histogram_fd(const double* data, std::size_t n) {
  if (n == 0) return Histogram{};
  // min / max
  double mn = data[0];
  double mx = data[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (data[i] < mn) mn = data[i];
    if (data[i] > mx) mx = data[i];
  }
  if (!(mx > mn)) {
    // constant sample -> a single bin centred on the value
    Histogram h;
    h.low = mn;
    h.high = mn + 1.0;          // unit window; convention only
    h.n_bins = 1;
    h.counts.assign(1, n);
    return h;
  }
  const double q25 = percentile(data, n, 0.25);
  const double q75 = percentile(data, n, 0.75);
  const double iqr = q75 - q25;
  std::size_t k;
  if (iqr > 0.0) {
    const double w = 2.0 * iqr / std::cbrt(static_cast<double>(n));
    if (!(w > 0.0)) {
      k = 1;
    } else {
      k = static_cast<std::size_t>(std::ceil((mx - mn) / w));
      if (k == 0) k = 1;
    }
  } else {
    // Sturges' fallback
    k = static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(n)) + 1.0));
    if (k == 0) k = 1;
  }
  // The histogram() upper-bound test is `x < high`. Bump `high` by a
  // small fraction of the range so the maximum sample lands inside the
  // last bin without distorting bin width. The fraction is fixed by the
  // bin count itself: we extend by half a bin width.
  const double w = (mx - mn) / static_cast<double>(k);
  const double pad = (w > 0.0) ? 0.5 * w
                               : std::nextafter(mx, std::numeric_limits<double>::infinity()) - mx;
  return histogram(data, n, mn, mx + pad, k);
}

inline Histogram histogram_fd(const std::vector<double>& v) {
  return histogram_fd(v.data(), v.size());
}

/** \brief Find local-maximum bins of a histogram.
 *
 *  A bin i is a peak if counts[i] > counts[i-1] and counts[i] >=
 *  counts[i+1] and counts[i] >= prominence_threshold. Returns
 *  bin centres and counts of detected peaks.
 *
 *  prominence_threshold defaults to 0 (i.e. no filter). Callers
 *  derive a data-driven threshold (e.g. (p99 - p1) / N_bins) and pass
 *  it explicitly.
 *
 *  min_separation_bins enforces a gap between peaks: when two peaks
 *  are within min_separation_bins of each other, the lower one is
 *  dropped. Default 0 keeps all candidates.
 */
struct HistogramPeak {
  std::size_t bin;
  double centre;
  std::size_t count;
};

inline std::vector<HistogramPeak>
histogram_peaks(const Histogram& h,
                std::size_t prominence_threshold = 0,
                std::size_t min_separation_bins = 0) {
  std::vector<HistogramPeak> peaks;
  if (h.n_bins < 3) return peaks;
  for (std::size_t i = 1; i + 1 < h.n_bins; ++i) {
    const auto c = h.counts[i];
    if (c >= prominence_threshold &&
        c > h.counts[i - 1] &&
        c >= h.counts[i + 1]) {
      peaks.push_back({i, h.centre(i), c});
    }
  }
  if (min_separation_bins > 0 && peaks.size() > 1) {
    std::vector<HistogramPeak> filtered;
    filtered.push_back(peaks.front());
    for (std::size_t i = 1; i < peaks.size(); ++i) {
      auto& last = filtered.back();
      if (peaks[i].bin >= last.bin + min_separation_bins) {
        filtered.push_back(peaks[i]);
      } else if (peaks[i].count > last.count) {
        last = peaks[i];
      }
    }
    peaks.swap(filtered);
  }
  return peaks;
}

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_HISTOGRAM_H
