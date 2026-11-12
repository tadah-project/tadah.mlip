// test_zero_rows.cpp
//
// zero_rows(Phi, rows, factors): the zero rows of the rows of a fit, those
// of factor above 0. A row is zero when every value is at most
// kZeroRowTolerance (1e-10) times the largest magnitude of its column over
// the rows of factor above 0; a row of factor 0 or less, which the fit
// leaves out, is zero and does not set a column's largest value.
//
// The references are written out by hand.

#include "catch2/catch.hpp"

#include <tadah/core/core_types.h>
#include <tadah/models/zero_rows.h>

#include <limits>
#include <stdexcept>
#include <vector>

using tadah::core::phi_type;
using tadah::core::t_type;
using tadah::models::zero_rows;

namespace {

/* @p values row by row (@p cols per row), in a matrix with one more row
 * than @p values holds (a ridge row after the data rows). */
phi_type matrix(const std::vector<double> &values, std::size_t cols) {
  const std::size_t rows = values.size() / cols;
  phi_type Phi(rows + 1, cols);
  Phi.set_zero();
  for (std::size_t i = 0; i < rows; ++i)
    for (std::size_t j = 0; j < cols; ++j) Phi(i, j) = values[i * cols + j];
  return Phi;
}

t_type vec(const std::vector<double> &v) {
  t_type t(v.size());
  for (std::size_t i = 0; i < v.size(); ++i) t(i) = v[i];
  return t;
}

}  // namespace

/* Pins: rows (1, 0.5), (1e-9, 0) and (1e-17, 0) of factor 1 and (1e3, 0)
 * of factor 0. Over the rows of factor above 0 the largest value of column
 * 0 is 1: (1e-9, 0) is not zero, (1e-17, 0) is, and the row of factor 0 is
 * zero. Without the factors the largest value is 1e3, and (1e-9, 0) is
 * zero too. */
TEST_CASE("zero_rows: the rows of factor 0 set no column's largest value",
          "[zero_rows]") {
  const phi_type Phi =
      matrix({1.0, 0.5, 1e-9, 0.0, 1e-17, 0.0, 1e3, 0.0}, 2);
  CHECK(zero_rows(Phi, 4, vec({1, 1, 1, 0})) ==
        std::vector<char>({0, 0, 1, 1}));
  CHECK(zero_rows(Phi, 4) == std::vector<char>({0, 1, 1, 0}));
  /* A factor below 0 or not a number leaves its row out as 0 does. */
  CHECK(zero_rows(Phi, 4, vec({1, 1, 1, -1})) ==
        std::vector<char>({0, 0, 1, 1}));
  CHECK(zero_rows(Phi, 4,
                  vec({1, 1, 1, std::numeric_limits<double>::quiet_NaN()})) ==
        std::vector<char>({0, 0, 1, 1}));
  /* Every factor above 0: the rule of zero_rows(Phi, rows). */
  CHECK(zero_rows(Phi, 4, vec({2, 0.5, 1, 3})) == zero_rows(Phi, 4));
}

/* Pins: rows (0, 1), (1e-300, 0), (inf, 0) of factor 1 and (5, 0) of
 * factor 0. A value that is not finite makes its row non-zero and sets no
 * column's largest value, so 1e-300 is the largest of column 0 and its row
 * is not zero; only the first rows are read. */
TEST_CASE("zero_rows: values that are not finite and the rows read",
          "[zero_rows]") {
  const double inf = std::numeric_limits<double>::infinity();
  const phi_type Phi = matrix({0.0, 1.0, 1e-300, 0.0, inf, 0.0, 5.0, 0.0}, 2);
  CHECK(zero_rows(Phi, 4, vec({1, 1, 1, 0})) ==
        std::vector<char>({0, 0, 0, 1}));
  CHECK(zero_rows(Phi, 2, vec({1, 1, 1, 1})) == std::vector<char>({0, 0}));
}

/* Pins: a factor vector shorter than the rows read is a logic error. */
TEST_CASE("zero_rows: fewer factors than rows", "[zero_rows]") {
  const phi_type Phi = matrix({1.0, 0.0, 0.0, 1.0}, 2);
  CHECK_THROWS_AS(zero_rows(Phi, 2, vec({1})), std::logic_error);
}
