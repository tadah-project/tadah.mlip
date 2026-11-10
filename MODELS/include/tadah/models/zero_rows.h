#ifndef TADAH_MODELS_ZERO_ROWS_H
#define TADAH_MODELS_ZERO_ROWS_H

#include <tadah/core/core_types.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** The size, relative to the largest magnitude of its column, up to which
 *  a value of a design-matrix row is rounding: a row whose values are all
 *  that small is zero (zero_rows()). The force rows of an atom at a centre
 *  of symmetry are sums over its neighbours that cancel, and their rounding
 *  leaves values of about 1e-16 of the column's largest; a row that the
 *  data determine lies many orders of magnitude above 1e-10. */
constexpr double kZeroRowTolerance = 1e-10;

namespace detail {

/* A test on the bits, which -ffast-math does not optimise away. */
inline bool finite_value(double v) {
  std::uint64_t u = 0;
  std::memcpy(&u, &v, sizeof u);
  return (u & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
}

/* zero_rows() over the rows @p counted (a vector of @p rows flags): the
 * largest magnitude of each column is taken over those rows only, and a
 * row that is not counted is zero. */
inline std::vector<char> zero_rows_of(const tadah::core::phi_type &Phi,
                                      std::size_t rows,
                                      const std::vector<char> &counted) {
  std::vector<double> largest(Phi.cols(), 0.0);
  for (std::size_t j = 0; j < Phi.cols(); ++j)
    for (std::size_t i = 0; i < rows; ++i) {
      if (!counted[i]) continue;
      const double v = Phi(i, j);
      if (finite_value(v) && std::fabs(v) > largest[j])
        largest[j] = std::fabs(v);
    }
  std::vector<char> zero(rows, 1);
  for (std::size_t j = 0; j < Phi.cols(); ++j) {
    const double limit = kZeroRowTolerance * largest[j];
    for (std::size_t i = 0; i < rows; ++i) {
      if (!counted[i]) continue;
      const double v = Phi(i, j);
      if (zero[i] && (!finite_value(v) || std::fabs(v) > limit)) zero[i] = 0;
    }
  }
  return zero;
}

}  // namespace detail

/** Which of the first @p rows rows of @p Phi are zero in every column: row
 *  i is zero when |Phi(i, j)| <= kZeroRowTolerance * max_k |Phi(k, j)| for
 *  every column j, k over the same rows and their finite values (in a
 *  column whose values are all 0 only an exact 0 is zero). A zero row
 *  carries no information: it adds nothing to the rank of the design
 *  matrix and is not an observation of a fit. Every count of zero rows
 *  (the observations of a DesignMatrix build, the rank check of
 *  LinearRegressor) takes it from here, so the rounding of sums that
 *  cancel does not decide whether a row counts. A value that is not
 *  finite makes its row non-zero. */
inline std::vector<char> zero_rows(const tadah::core::phi_type &Phi,
                                   std::size_t rows) {
  rows = std::min(rows, Phi.rows());
  return detail::zero_rows_of(Phi, rows, std::vector<char>(rows, 1));
}

/** zero_rows() of the rows of a fit: the first @p rows rows of @p Phi,
 *  without their factors, of which those of factor above 0 (@p factors)
 *  are the fit's. The largest magnitude of each column is taken over the
 *  fit's rows only, and a row of factor 0 or less (or not a number) is
 *  zero: a row of priority 0, which the fit leaves out, such as those of a
 *  dataset of WDBFILE 0, does not decide whether another row is zero. The
 *  observations of a DesignMatrix build and the rows of its balance table
 *  take them from here.
 *  @throws std::logic_error for fewer factors than rows. */
inline std::vector<char> zero_rows(const tadah::core::phi_type &Phi,
                                   std::size_t rows,
                                   const tadah::core::t_type &factors) {
  rows = std::min(rows, Phi.rows());
  if (factors.size() < rows)
    throw std::logic_error("zero_rows: " + std::to_string(factors.size()) +
                           " factors for " + std::to_string(rows) + " rows");
  std::vector<char> counted(rows, 0);
  for (std::size_t i = 0; i < rows; ++i) counted[i] = factors(i) > 0.0;
  return detail::zero_rows_of(Phi, rows, counted);
}

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_ZERO_ROWS_H
