#ifndef TADAH_CORE_STATS_COLUMN_STATS_H
#define TADAH_CORE_STATS_COLUMN_STATS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace tadah {
namespace core {
namespace stats {

/** \brief 2-norm of a single column of a row-major matrix.
 *
 *  Storage convention: matrix has n_rows * n_cols doubles, with
 *  matrix[i * n_cols + j] being entry (i, j). The column index j
 *  must be < n_cols.
 */
inline double column_norm(const double* matrix,
                          std::size_t n_rows, std::size_t n_cols,
                          std::size_t col) {
  if (col >= n_cols) {
    throw std::out_of_range("column_norm: col >= n_cols");
  }
  double s = 0.0;
  for (std::size_t i = 0; i < n_rows; ++i) {
    const double x = matrix[i * n_cols + col];
    s += x * x;
  }
  return std::sqrt(s);
}

/** \brief Per-column 2-norms across the whole matrix. */
inline std::vector<double>
column_norms(const double* matrix,
             std::size_t n_rows, std::size_t n_cols) {
  std::vector<double> out(n_cols, 0.0);
  for (std::size_t i = 0; i < n_rows; ++i) {
    const double* row = matrix + i * n_cols;
    for (std::size_t j = 0; j < n_cols; ++j) {
      out[j] += row[j] * row[j];
    }
  }
  for (auto& x : out) x = std::sqrt(x);
  return out;
}

/** \brief Median 2-norm of a contiguous column block.
 *
 *  Inputs:
 *    matrix     row-major, n_rows*n_cols doubles
 *    col_offset first column of the block
 *    col_count  number of columns in the block (>= 1)
 *
 *  Returns the median over the per-column 2-norms of columns in
 *  [col_offset, col_offset + col_count).
 */
inline double
block_median_column_norm(const double* matrix,
                         std::size_t n_rows, std::size_t n_cols,
                         std::size_t col_offset, std::size_t col_count) {
  if (col_count == 0) {
    throw std::invalid_argument("block_median_column_norm: empty block");
  }
  if (col_offset + col_count > n_cols) {
    throw std::out_of_range("block_median_column_norm: block overflow");
  }
  std::vector<double> norms;
  norms.reserve(col_count);
  for (std::size_t j = 0; j < col_count; ++j) {
    norms.push_back(column_norm(matrix, n_rows, n_cols, col_offset + j));
  }
  const std::size_t mid = norms.size() / 2;
  std::nth_element(norms.begin(), norms.begin() + mid, norms.end());
  if (norms.size() % 2) return norms[mid];
  // even: average of the two middle values
  const double upper = norms[mid];
  std::nth_element(norms.begin(), norms.begin() + mid - 1, norms.begin() + mid);
  const double lower = norms[mid - 1];
  return 0.5 * (lower + upper);
}

/** \brief Ratio of one block's median column norm to the *largest*
 *  block-median column norm in `block_norms`.
 *
 *  This is the ratio used in the pre-flight audit: a block whose median
 *  column norm is `1e-6` times smaller than the largest block in the
 *  same matrix is flagged as "dead" (per
 *  DESIGN_GENERALITY_ACROSS_MATERIALS.md §"What is universal"). The
 *  threshold itself is per-descriptor metadata, declared at the
 *  descriptor's meta() site; this helper just produces the ratio.
 */
inline double
block_norm_ratio_to_max(double block_median, const std::vector<double>& all_block_medians) {
  if (all_block_medians.empty()) return 1.0;
  double mx = 0.0;
  for (double v : all_block_medians) {
    if (v > mx) mx = v;
  }
  if (!(mx > 0.0)) return 1.0;
  return block_median / mx;
}

/** \brief Effective rank from singular values, given a relative cutoff
 *  rcond.
 *
 *  Counts s_i with s_i / s_0 > rcond. The s array must already be
 *  sorted descending (the LAPACK convention for GELSD/GESDD outputs).
 *  rcond is the user-facing knob already wired in OALGO=2 (see
 *  MODELS/include/tadah/models/linear_regressor.h:51) — re-using it
 *  ensures this helper is not introducing a new threshold.
 */
inline std::size_t
effective_rank(const double* singular_values, std::size_t n,
               double rcond) {
  if (n == 0) return 0;
  const double s0 = singular_values[0];
  if (!(s0 > 0.0)) return 0;
  const double cutoff = rcond * s0;
  std::size_t r = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (singular_values[i] > cutoff) ++r;
    else break;     // descending: once below, all below
  }
  return r;
}

inline std::size_t
effective_rank(const std::vector<double>& singular_values, double rcond) {
  return effective_rank(singular_values.data(), singular_values.size(), rcond);
}

/** \brief Detect whether a column block of a row-major matrix is
 *  exactly zero (every entry below `floor`).
 *
 *  `floor` defaults to 1e-15 — the IEEE denormal floor (NOT a tuning
 *  threshold; it exists to guard against signed-zero / denormal
 *  weirdness when comparing to literal 0).
 */
inline bool
block_is_exact_zero(const double* matrix,
                    std::size_t n_rows, std::size_t n_cols,
                    std::size_t col_offset, std::size_t col_count,
                    double floor = 1e-15) {
  for (std::size_t i = 0; i < n_rows; ++i) {
    const double* row = matrix + i * n_cols;
    for (std::size_t j = col_offset; j < col_offset + col_count; ++j) {
      if (std::fabs(row[j]) > floor) return false;
    }
  }
  return true;
}

}  // namespace stats
}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_STATS_COLUMN_STATS_H
