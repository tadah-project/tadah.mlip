#ifndef TADAH_CORE_STATS_KDE_H
#define TADAH_CORE_STATS_KDE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <tadah/core/stats/percentiles.h>

namespace tadah {
namespace core {
namespace stats {

/** \brief Silverman's rule-of-thumb bandwidth for a 1-D Gaussian KDE.
 *
 *  h = 0.9 * min(stddev, IQR / 1.34) * n^(-1/5)
 *
 *  This is the "rule of thumb" form Silverman recommends as a default;
 *  the IQR / 1.34 term is the IQR-equivalent standard deviation of a
 *  unit-variance Gaussian. The 0.9 prefactor is a textbook constant
 *  (Silverman 1986) — it is NOT a tuning threshold and is documented
 *  here at point of use.
 *
 *  Returns 0.0 for empty / constant samples.
 */
inline double silverman_bandwidth(const double* data, std::size_t n) {
  if (n < 2) return 0.0;
  // Welford for std dev
  double mean = 0.0, m2 = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double dx = data[i] - mean;
    mean += dx / static_cast<double>(i + 1);
    m2   += dx * (data[i] - mean);
  }
  const double sd = std::sqrt(m2 / static_cast<double>(n - 1));
  const double q25 = percentile(data, n, 0.25);
  const double q75 = percentile(data, n, 0.75);
  const double iqr = q75 - q25;
  const double iqr_eq_sd = iqr / 1.34;
  double sigma_eff;
  if (sd > 0.0 && iqr_eq_sd > 0.0) {
    sigma_eff = std::min(sd, iqr_eq_sd);
  } else if (sd > 0.0) {
    sigma_eff = sd;
  } else if (iqr_eq_sd > 0.0) {
    sigma_eff = iqr_eq_sd;
  } else {
    return 0.0;
  }
  // 0.9 * sigma * n^(-1/5)  --- Silverman (1986)
  return 0.9 * sigma_eff * std::pow(static_cast<double>(n), -0.2);
}

inline double silverman_bandwidth(const std::vector<double>& v) {
  return silverman_bandwidth(v.data(), v.size());
}

/** \brief Evaluate a 1-D Gaussian KDE at the given grid points. */
inline std::vector<double>
kde_evaluate(const double* data, std::size_t n,
             const std::vector<double>& grid, double bandwidth) {
  if (!(bandwidth > 0.0)) {
    throw std::invalid_argument("kde_evaluate: bandwidth must be > 0");
  }
  std::vector<double> out(grid.size(), 0.0);
  const double inv_h = 1.0 / bandwidth;
  const double norm  = 1.0 / (static_cast<double>(n) * bandwidth
                              * std::sqrt(2.0 * M_PI));
  for (std::size_t g = 0; g < grid.size(); ++g) {
    double s = 0.0;
    const double xg = grid[g];
    for (std::size_t i = 0; i < n; ++i) {
      const double z = (xg - data[i]) * inv_h;
      s += std::exp(-0.5 * z * z);
    }
    out[g] = s * norm;
  }
  return out;
}

inline std::vector<double>
kde_evaluate(const std::vector<double>& v,
             const std::vector<double>& grid, double bandwidth) {
  return kde_evaluate(v.data(), v.size(), grid, bandwidth);
}

/** \brief A KDE peak: position, density value, and the FWHM derived
 *  from the half-maximum width on the supplied grid.
 *
 *  FWHM is estimated by linear interpolation of the KDE on the
 *  evaluation grid: it is the width of the contiguous interval around
 *  the peak where density >= 0.5 * peak_density. When the peak is at
 *  the edge of the grid, the FWHM falls back to the full grid extent.
 */
struct KdePeak {
  double position;
  double density;
  double fwhm;
};

/** \brief Detect peaks of a 1-D KDE evaluated on a regular grid.
 *
 *  A peak is a strict local maximum (density[i] > density[i-1] and
 *  density[i] >= density[i+1]) with density >= prominence_threshold.
 *
 *  Caller-derived `prominence_threshold` should reflect dataset
 *  characteristics — e.g. a fraction of the maximum density. No
 *  default magic number is used here; callers pass an explicit value.
 *
 *  Returns peaks sorted by position (left to right).
 */
inline std::vector<KdePeak>
kde_peaks(const std::vector<double>& grid,
          const std::vector<double>& density,
          double prominence_threshold = 0.0) {
  if (grid.size() != density.size()) {
    throw std::invalid_argument("kde_peaks: grid and density size mismatch");
  }
  std::vector<KdePeak> out;
  if (grid.size() < 3) return out;
  for (std::size_t i = 1; i + 1 < grid.size(); ++i) {
    if (density[i] >= prominence_threshold &&
        density[i] >  density[i - 1] &&
        density[i] >= density[i + 1]) {
      KdePeak p;
      p.position = grid[i];
      p.density  = density[i];
      // FWHM: scan left + right for half-max crossing.
      const double half = 0.5 * density[i];
      double left  = grid.front();
      double right = grid.back();
      for (std::size_t j = i; j > 0; --j) {
        if (density[j - 1] < half) {
          // linear interp between grid[j-1] and grid[j]
          const double d0 = density[j - 1];
          const double d1 = density[j];
          const double t  = (d1 - half) / (d1 - d0);
          left = grid[j] - t * (grid[j] - grid[j - 1]);
          break;
        }
      }
      for (std::size_t j = i; j + 1 < grid.size(); ++j) {
        if (density[j + 1] < half) {
          const double d0 = density[j];
          const double d1 = density[j + 1];
          const double t  = (d0 - half) / (d0 - d1);
          right = grid[j] + t * (grid[j + 1] - grid[j]);
          break;
        }
      }
      p.fwhm = right - left;
      out.push_back(p);
    }
  }
  return out;
}

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_KDE_H
