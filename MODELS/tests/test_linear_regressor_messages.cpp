// test_linear_regressor_messages.cpp
//
// What LinearRegressor::train says when LAPACK cannot fit the weights:
// a design matrix with fewer data rows than weights, or fewer non-zero
// ones (a row of priority 0 is zero), refused by OALGO 2 (DGELS, QR) and
// OALGO 4 (normal equations, Cholesky) before LAPACK runs, a
// rank-deficient design matrix that LAPACK reports, and a design matrix
// or target with a value that is not finite, under every OALGO; with
// FIXINDEX (M_BLR_Train and M_KRR_Train), the free weights, the data rows,
// the free intercept columns of BIAS and the weight of each column. The
// design matrices are laid out as DesignMatrix lays them out: the data
// rows, then one ridge row per column (zero with LAMBDA 0).

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/factor_handle.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>
#include <tadah/models/memory/models_workspace_manager.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

Context make_ctx(const std::string &oalgo) {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "0");
  c.add("OALGO", oalgo);
  return c;
}

/* `data` rows of N columns, then N zero ridge rows; column j of data row i
 * is 1 + i + 2 j + i j % 3 for j < `nonzero`, 0 from there. */
phi_type design(size_t data, size_t n, size_t nonzero) {
  phi_type Phi(data + n, n);
  Phi.set_zero();
  for (size_t i = 0; i < data; ++i)
    for (size_t j = 0; j < nonzero; ++j)
      Phi(i, j) = 1.0 + static_cast<double>(i + 2 * j + (i * j) % 3);
  return Phi;
}

t_type target(size_t data, size_t n) {
  t_type T(data + n);
  for (size_t i = 0; i < T.size(); ++i)
    T[i] = i < data ? 0.5 * static_cast<double>(i + 1) : 0.0;
  return T;
}

/* The message LinearRegressor::train throws, or "no exception". */
std::string fit_refusal(const std::string &oalgo, phi_type Phi, t_type T) {
  Context c = make_ctx(oalgo);
  t_type w;
  try {
    LinearRegressor::train(c, Phi, T, w);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* The message LinearRegressor::train throws for the context `c`, or "no
 * exception". */
std::string fit_refusal(Context c, phi_type Phi, t_type T) {
  t_type w;
  try {
    LinearRegressor::train(c, Phi, T, w);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* The message train_with_factor (OALGO 4) throws, or "no exception". */
std::string factor_refusal(phi_type Phi, t_type T) {
  Context c = make_ctx("4");
  t_type w;
  FactorHandle factor;
  tadah::models::memory::ModelsWorkspaceManager wm;
  try {
    LinearRegressor::train_with_factor(c, Phi, T, w, factor, wm);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* `data` rows of N columns, then N zero ridge rows; column j of data row i
 * is sin(1 + 0.1 i + 0.1 j + 0.05 i j): not integers, so that the rounded
 * Phi^T Phi of fewer data rows than weights need not be singular. */
phi_type sine_design(size_t data, size_t n) {
  phi_type Phi(data + n, n);
  Phi.set_zero();
  for (size_t i = 0; i < data; ++i)
    for (size_t j = 0; j < n; ++j)
      Phi(i, j) = std::sin(1.0 + 0.1 * static_cast<double>(i) +
                           0.1 * static_cast<double>(j) +
                           0.05 * static_cast<double>(i * j));
  return Phi;
}

/* The message M_BLR_Train<BF_Linear>::train throws with FIXINDEX `fixindex`
 * and FIXWEIGHT `fixweights` under `oalgo`, or "no exception". */
std::string blr_fix_refusal(const std::string &oalgo, const std::string &fixindex,
                            const std::vector<std::string> &fixweights,
                            phi_type Phi, t_type T) {
  Context c = make_ctx(oalgo);
  c.add("FIXINDEX", fixindex);
  for (const std::string &w : fixweights) c.add("FIXWEIGHT", w);
  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  try {
    m.train(Phi, T);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* The message M_KRR_Train<Kern_Linear>::train throws with FIXINDEX
 * `fixindex` and FIXWEIGHT `fixweights` under `oalgo`, or "no exception".
 * With Kern_Linear, column j of Phi is weight j, as for BF_Linear. */
std::string krr_fix_refusal(const std::string &oalgo, const std::string &fixindex,
                            const std::vector<std::string> &fixweights,
                            phi_type Phi, t_type T) {
  Context c = make_ctx(oalgo);
  c.add("FIXINDEX", fixindex);
  for (const std::string &w : fixweights) c.add("FIXWEIGHT", w);
  Kern_Linear k;
  M_KRR_Train<Kern_Linear> m(k, c);
  try {
    m.train(Phi, T);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* `c` with LAMBDA 0.1 and BIAS true over ATOM Ta Nb: 2 intercept columns,
 * the first 2 of a BF_Linear or Kern_Linear design matrix. */
Context with_two_species_bias(Context c) {
  c.remove("LAMBDA");
  c.add("LAMBDA", "0.1");
  c.add("BIAS", "true");
  c.add("ATOM", "Ta");
  c.add("ATOM", "Nb");
  return c;
}

/* The message M_BLR_Train<BF_Linear> (krr false) or M_KRR_Train<Kern_Linear>
 * (krr true) throws when trained in the context `c`, or "no exception". */
std::string model_refusal(bool krr, Context c, phi_type Phi, t_type T) {
  try {
    if (krr) {
      Kern_Linear k;
      M_KRR_Train<Kern_Linear> m(k, c);
      m.train(Phi, T);
    } else {
      BF_Linear bf;
      M_BLR_Train<BF_Linear> m(bf, c);
      m.train(Phi, T);
    }
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* `Phi` and `T` with data row `i` (0-based) zero, as a row of priority 0 is
 * in DesignMatrix (its row and its target are multiplied by 0). */
void zero_row(phi_type &Phi, t_type &T, size_t i) {
  for (size_t j = 0; j < Phi.cols(); ++j) Phi(i, j) = 0.0;
  T[i] = 0.0;
}

/* `data` rows of N columns, every value 1 (the columns are equal), then N
 * zero ridge rows. */
phi_type ones_design(size_t data, size_t n) {
  phi_type Phi(data + n, n);
  Phi.set_zero();
  for (size_t i = 0; i < data; ++i)
    for (size_t j = 0; j < n; ++j) Phi(i, j) = 1.0;
  return Phi;
}

/* What the messages say of the zero data rows (their counts and weights
 * name them): without FIXINDEX, and with FIXINDEX pinning a weight. */
const char *const kZeroRows =
    " (a data row that is zero in every column adds nothing to the rank: a "
    "row of priority 0, or a row the descriptors make zero, such as the y "
    "and z force rows of a linear molecule along x)";
const char *const kZeroRowsFixed =
    " (a data row that is zero in every column adds nothing to the rank: a "
    "row of priority 0, a row whose values are all in FIXINDEX columns, or "
    "a row the descriptors make zero, such as the y and z force rows of a "
    "linear molecule along x)";

const char *const kRemedies =
    " A fit is possible with OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the "
    "minimum-norm weights, with LAMBDA > 0 (a ridge penalty makes every "
    "column it penalises full rank; it does not penalise the intercept "
    "columns of BIAS), or with more data (structures, FORCE or STRESS rows).";

}  // namespace

/* Pins: with LAMBDA 0, a fit whose design matrix does not have full column
 * rank, as LAPACK finds it, is refused by OALGO 2 and OALGO 4 with a
 * message that says so, the column where it shows, the data rows and
 * weights, and the remedies: OALGO 2 and OALGO 4, 6 data rows for 4 weights
 * whose columns 3 and 4 are zero (DGELS: R(3,3) is exactly 0; DPOTRF: the
 * third pivot is 0): "OALGO 2 (DGELS, QR) cannot fit these 4 weights: the
 * design matrix (6 data rows for 4 weights, LAMBDA 0) does not have full
 * column rank: its column 3 is zero or a linear combination of the columns
 * before it, so the least-squares weights are not unique." + the remedies,
 * and for OALGO 4 "the matrix Phi^T Phi of the design matrix (...) is not
 * positive definite (its leading minor of order 3 is not)". OALGO 1 and
 * OALGO 3 fit it, and 2 data rows for 4 weights (the minimum-norm weights:
 * the zero columns' weights are 0 to 1e-12). (Pins as well.) Fewer data
 * rows than weights under OALGO 2 and 4: the next case.
 * Before the fix: "Error in DGELS: info = 3" and "Error in DPOTRF:
 * tadah::core::Matrix is not positive definite; info = 3".
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: OALGO 2 and 4 refuse a rank-deficient design "
          "matrix, saying why and what to do",
          "[linear_regressor][lapack_messages]") {
  const std::string zero_cols =
      "the design matrix (6 data rows for 4 weights, LAMBDA 0) does not have "
      "full column rank: its column 3 is zero or a linear combination of the "
      "columns before it, so the least-squares weights are not unique.";
  CHECK(fit_refusal("2", design(6, 4, 2), target(6, 4)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 weights: " + zero_cols +
            kRemedies);
  CHECK(fit_refusal("4", design(6, 4, 2), target(6, 4)) ==
        std::string("OALGO 4 (normal equations, Cholesky) cannot fit these 4 "
                    "weights: the matrix Phi^T Phi of the design matrix (6 "
                    "data rows for 4 weights, LAMBDA 0) is not positive "
                    "definite (its leading minor of order 3 is not), so the "
                    "design matrix does not have full column rank or is too "
                    "ill-conditioned for the normal equations.") +
            kRemedies);

  for (const char *oalgo : {"1", "3"}) {
    INFO("OALGO " << oalgo);
    CHECK(fit_refusal(oalgo, design(2, 4, 4), target(2, 4)) == "no exception");
    Context c = make_ctx(oalgo);
    phi_type Phi = design(6, 4, 2);
    t_type T = target(6, 4);
    t_type w;
    REQUIRE_NOTHROW(LinearRegressor::train(c, Phi, T, w));
    REQUIRE(w.size() == 4);
    CHECK(std::abs(w[2]) < 1e-12);
    CHECK(std::abs(w[3]) < 1e-12);
  }
}

/* Pins: OALGO 2 and OALGO 4 refuse, before LAPACK runs, a design matrix
 * whose rows cannot give it full column rank: with LAMBDA 0, fewer data
 * rows than weights; with LAMBDA > 0, which penalises every column but the
 * intercepts, fewer data rows than intercept columns. The message names the
 * data rows, the weights, LAMBDA and the remedies:
 *  - OALGO 2, 2 data rows for 4 weights: "OALGO 2 (DGELS, QR) cannot fit
 *    these 4 weights: the design matrix has 2 data rows for 4 weights,
 *    LAMBDA 0: fewer data rows than weights, so it does not have full
 *    column rank and the least-squares weights are not unique." + the
 *    remedies;
 *  - OALGO 4, 2 data rows for 3 weights of values that are not integers
 *    (sine_design), the same with "OALGO 4 (normal equations, Cholesky)",
 *    from train() and from train_with_factor();
 *  - LAMBDA 0.1, BIAS true with ATOM Ta Nb (2 intercept columns), 1 data row
 *    for 4 weights, OALGO 2 and 4: "... the design matrix has 1 data row for
 *    4 weights, LAMBDA 0.1, which does not penalise its 2 intercept columns:
 *    fewer data rows than intercept columns, so it does not have full column
 *    rank and the least-squares weights are not unique. A fit is possible with
 *    OALGO 1 (DGELSD), which gives the minimum-norm weights, or with more
 *    structures.".
 * The refusal does not depend on rounding. With 2 data rows for the 2
 * intercept columns the fit is made (a pin).
 * Before the fix: whether OALGO 4 stopped depended on the rounding of
 * Phi^T Phi: it fitted the 2 sine rows for 3 weights without an error, in
 * train() and train_with_factor() (DPOTRF factored the rounded matrix, as
 * 'tadah train' fitted 2 energies with D2_BP 3 3 with weights of 1e5), and
 * stopped in DPOTRF on the 2 integer rows for 4 weights ("the matrix Phi^T
 * Phi of the design matrix (2 data rows for 4 weights, LAMBDA 0) is not
 * positive definite (its leading minor of order 3 is not), ..."); OALGO 2
 * said "the design matrix (2 data rows for 4 weights, LAMBDA 0) does not
 * have full column rank: its column 3 is zero or a linear combination of
 * the columns before it (fewer data rows than weights), ..."; with LAMBDA
 * 0.1, OALGO 2 said "... (1 data rows for 4 weights, LAMBDA 0.1) ... its
 * column 2 is zero ... (fewer data rows than weights) ..." and OALGO 4 "...
 * is not positive definite (its leading minor of order 2 is not) ...", both
 * with the LAMBDA 0 remedies.
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: OALGO 2 and 4 refuse fewer data rows than "
          "weights before LAPACK, whatever the rounding",
          "[linear_regressor][lapack_messages]") {
  const std::string few =
      " cannot fit these 4 weights: the design matrix has 2 data rows for 4 "
      "weights, LAMBDA 0: fewer data rows than weights, so it does not have "
      "full column rank and the least-squares weights are not unique.";
  CHECK(fit_refusal("2", design(2, 4, 4), target(2, 4)) ==
        "OALGO 2 (DGELS, QR)" + few + kRemedies);
  CHECK(fit_refusal("4", design(2, 4, 4), target(2, 4)) ==
        "OALGO 4 (normal equations, Cholesky)" + few + kRemedies);

  const std::string three =
      "OALGO 4 (normal equations, Cholesky) cannot fit these 3 weights: the "
      "design matrix has 2 data rows for 3 weights, LAMBDA 0: fewer data rows "
      "than weights, so it does not have full column rank and the "
      "least-squares weights are not unique." + std::string(kRemedies);
  CHECK(fit_refusal("4", sine_design(2, 3), target(2, 3)) == three);
  CHECK(factor_refusal(sine_design(2, 3), target(2, 3)) == three);
  CHECK(fit_refusal("2", sine_design(2, 3), target(2, 3)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 3 weights: the design matrix "
        "has 2 data rows for 3 weights, LAMBDA 0: fewer data rows than "
        "weights, so it does not have full column rank and the least-squares "
        "weights are not unique." + std::string(kRemedies));

  const std::string intercepts =
      " cannot fit these 4 weights: the design matrix has 1 data row for 4 "
      "weights, LAMBDA 0.1, which does not penalise its 2 intercept columns: "
      "fewer data rows than intercept columns, so it does not have full "
      "column rank and the least-squares weights are not unique. A fit is "
      "possible with OALGO 1 (DGELSD), which gives the minimum-norm "
      "weights, or with more structures.";
  for (const char *oalgo : {"2", "4"}) {
    INFO("OALGO " << oalgo);
    Context c = make_ctx(oalgo);
    c.remove("LAMBDA");
    c.add("LAMBDA", "0.1");
    c.add("BIAS", "true");
    c.add("ATOM", "Ta");
    c.add("ATOM", "Nb");
    CHECK(fit_refusal(c, sine_design(1, 4), target(1, 4)) ==
          std::string(oalgo[0] == '2' ? "OALGO 2 (DGELS, QR)"
                                      : "OALGO 4 (normal equations, Cholesky)") +
              intercepts);
  }
  // 2 data rows for the 2 intercept columns: the ridge makes the other 2
  // full rank, and the fit is made.
  Context c = make_ctx("4");
  c.remove("LAMBDA");
  c.add("LAMBDA", "0.1");
  c.add("BIAS", "true");
  c.add("ATOM", "Ta");
  c.add("ATOM", "Nb");
  CHECK(fit_refusal(c, sine_design(2, 4), target(2, 4)) == "no exception");
}

/* Pins: with FIXINDEX, M_BLR_Train fits the free weights on the design
 * matrix without the pinned columns, and the messages count the data rows
 * of the design matrix (not its ridge rows), the free weights and the
 * pinned ones, and name the weight of a column:
 *  - 2 data rows for 5 weights, FIXINDEX 5 (OALGO 2): "OALGO 2 (DGELS, QR)
 *    cannot fit these 4 free weights (FIXINDEX pins 1 of the 5 weights):
 *    the design matrix has 2 data rows for 4 weights, LAMBDA 0: fewer data
 *    rows than weights, ..." + the remedies; FIXINDEX 1-2, "these 3 free
 *    weights (FIXINDEX pins 2 of the 5 weights): the design matrix has 2
 *    data rows for 3 weights, ...";
 *  - 6 data rows for 5 weights whose weight 2's column is zero, FIXINDEX 1:
 *    OALGO 2 "... these 4 free weights (FIXINDEX pins 1 of the 5 weights):
 *    the design matrix (6 data rows for 4 weights, LAMBDA 0) does not have
 *    full column rank: its column 1 (weight 2) is zero or a linear
 *    combination of the columns before it, ...", and OALGO 4 "... is not
 *    positive definite (its leading minor of order 1 is not; column 1 fits
 *    weight 2), ...";
 *  - 6 data rows, FIXINDEX 1, OALGO 1: a NaN in weight 5's column at row 2
 *    "... at row 2, column 4 (of 6 data rows, then 5 ridge rows, and 4
 *    columns, the free weights'; column 4 fits weight 5): ...", and a NaN
 *    in the pinned weight 1's column, which the target less the pinned
 *    part holds: "LinearRegressor::train: the target vector, less the part
 *    of the FIXINDEX weights, holds a value that is not finite (nan or inf)
 *    at row 2 (of 6 data rows, then 5 ridge rows); no weights were
 *    fitted".
 * Before the fix: the counts took the ridge rows of the pinned columns for
 * data rows and the free columns for the weights: "OALGO 2 (DGELS, QR)
 * cannot fit these 4 weights: the design matrix (3 data rows for 4
 * weights, LAMBDA 0) does not have full column rank: its column 3 is zero
 * or a linear combination of the columns before it (fewer data rows than
 * weights), ..." for FIXINDEX 5; "these 3 weights: the design matrix (4 data
 * rows for 3 weights, LAMBDA 0) ... its column 3 is zero or a linear
 * combination of the columns before it, so ...", without '(fewer data rows
 * than weights)', for FIXINDEX 1-2; "(7 data rows for 4 weights, LAMBDA 0)
 * ... its column 1 is zero ..." and "... (its leading minor of order 1 is
 * not), ..."; "at row 2, column 4 (of 7 data rows, then 4 ridge rows, and 4
 * columns)"; and "the target vector holds a value that is not finite (nan
 * or inf) at row 2 (of 7 data rows, then 4 ridge rows)".
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: with FIXINDEX the messages count the data rows "
          "and name the weights",
          "[linear_regressor][lapack_messages][fixed_weights]") {
  CHECK(blr_fix_refusal("2", "5", {"0.1"}, design(2, 5, 5), target(2, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 free weights (FIXINDEX pins 1 "
        "of the 5 weights): the design matrix has 2 data rows for 4 weights, "
        "LAMBDA 0: fewer data rows than weights, so it does not have full "
        "column rank and the least-squares weights are not unique." +
            std::string(kRemedies));
  CHECK(blr_fix_refusal("2", "1-2", {"0.1", "0.2"}, design(2, 5, 5),
                        target(2, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 3 free weights (FIXINDEX pins 2 "
        "of the 5 weights): the design matrix has 2 data rows for 3 weights, "
        "LAMBDA 0: fewer data rows than weights, so it does not have full "
        "column rank and the least-squares weights are not unique." +
            std::string(kRemedies));

  phi_type zero2 = design(6, 5, 5);
  for (size_t i = 0; i < 6; ++i) zero2(i, 1) = 0.0;
  CHECK(blr_fix_refusal("2", "1", {"0.1"}, zero2, target(6, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 free weights (FIXINDEX pins 1 "
        "of the 5 weights): the design matrix (6 data rows for 4 weights, "
        "LAMBDA 0) does not have full column rank: its column 1 (weight 2) is "
        "zero or a linear combination of the columns before it, so the "
        "least-squares weights are not unique." + std::string(kRemedies));
  CHECK(blr_fix_refusal("4", "1", {"0.1"}, zero2, target(6, 5)) ==
        "OALGO 4 (normal equations, Cholesky) cannot fit these 4 free weights "
        "(FIXINDEX pins 1 of the 5 weights): the matrix Phi^T Phi of the "
        "design matrix (6 data rows for 4 weights, LAMBDA 0) is not positive "
        "definite (its leading minor of order 1 is not; column 1 fits weight "
        "2), so the design matrix does not have full column rank or is too "
        "ill-conditioned for the normal equations." + std::string(kRemedies));

  const double nan = std::numeric_limits<double>::quiet_NaN();
  phi_type bad5 = design(6, 5, 5);
  bad5(1, 4) = nan;
  CHECK(blr_fix_refusal("1", "1", {"0.1"}, bad5, target(6, 5)) ==
        "LinearRegressor::train: the design matrix holds a value that is not "
        "finite (nan or inf) at row 2, column 4 (of 6 data rows, then 5 ridge "
        "rows, and 4 columns, the free weights'; column 4 fits weight 5): a "
        "descriptor or basis function gave it; no weights were fitted");
  phi_type bad1 = design(6, 5, 5);
  bad1(1, 0) = nan;
  CHECK(blr_fix_refusal("1", "1", {"0.1"}, bad1, target(6, 5)) ==
        "LinearRegressor::train: the target vector, less the part of the "
        "FIXINDEX weights, holds a value that is not finite (nan or inf) at "
        "row 2 (of 6 data rows, then 5 ridge rows); no weights were fitted");
}

/* Pins: with FIXINDEX, M_KRR_Train (MODEL M_KRR Kern_Linear) removes the
 * pinned columns as M_BLR_Train does, and its messages count the data rows
 * of the design matrix (not the ridge rows of the pinned columns), the free
 * weights and the pinned ones, and name the weight of a column, as the
 * previous case pins for M_BLR_Train, on the same design matrices and with
 * the same messages:
 *  - 2 data rows for 5 weights, FIXINDEX 5 (OALGO 2): "OALGO 2 (DGELS, QR)
 *    cannot fit these 4 free weights (FIXINDEX pins 1 of the 5 weights):
 *    the design matrix has 2 data rows for 4 weights, LAMBDA 0: fewer data
 *    rows than weights, ..." + the remedies; FIXINDEX 1-2, "these 3 free
 *    weights (FIXINDEX pins 2 of the 5 weights): the design matrix has 2
 *    data rows for 3 weights, ...";
 *  - 6 data rows for 5 weights whose weight 2's column is zero, FIXINDEX 1:
 *    OALGO 2 "... its column 1 (weight 2) is zero or a linear combination
 *    of the columns before it, ...", and OALGO 4 "... (its leading minor of
 *    order 1 is not; column 1 fits weight 2), ...";
 *  - 6 data rows, FIXINDEX 1, OALGO 1: a NaN in weight 5's column at row 2
 *    "... at row 2, column 4 (of 6 data rows, then 5 ridge rows, and 4
 *    columns, the free weights'; column 4 fits weight 5): ...", and a NaN
 *    in the pinned weight 1's column: "LinearRegressor::train: the target
 *    vector, less the part of the FIXINDEX weights, holds a value that is
 *    not finite (nan or inf) at row 2 (of 6 data rows, then 5 ridge rows);
 *    no weights were fitted".
 * Before the fix: not run before it, since the case is the merge of v1-dev
 * into fix/input-robustness. On that branch M_KRR_Train kept the pinned
 * columns, set to 0 (DesignMatrix's layout, Layout::of(Phi)); v1-dev's
 * M_KRR_Train removes them and its call takes no layout. With
 * Layout::of(Phi) in the FIXINDEX call, the layout of the matrix without
 * the pinned columns, it fails as M_BLR_Train did before the fix: "these 4
 * weights: the design matrix has 3 data rows for 4 weights, ..." for
 * FIXINDEX 5; "these 3 weights: the design matrix (4 data rows for 3
 * weights, LAMBDA 0) ... its column 3 is zero or a linear combination of
 * the columns before it, ..." for FIXINDEX 1-2; "(7 data rows for 4
 * weights, LAMBDA 0) ... its column 1 is zero ..." and "... (its leading
 * minor of order 1 is not), ..."; "at row 2, column 4 (of 7 data rows,
 * then 4 ridge rows, and 4 columns)"; and "the target vector holds a value
 * that is not finite (nan or inf) at row 2 (of 7 data rows, then 4 ridge
 * rows)".
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: with FIXINDEX M_KRR_Train's messages count the "
          "data rows and name the weights",
          "[linear_regressor][lapack_messages][fixed_weights][krr]") {
  CHECK(krr_fix_refusal("2", "5", {"0.1"}, design(2, 5, 5), target(2, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 free weights (FIXINDEX pins 1 "
        "of the 5 weights): the design matrix has 2 data rows for 4 weights, "
        "LAMBDA 0: fewer data rows than weights, so it does not have full "
        "column rank and the least-squares weights are not unique." +
            std::string(kRemedies));
  CHECK(krr_fix_refusal("2", "1-2", {"0.1", "0.2"}, design(2, 5, 5),
                        target(2, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 3 free weights (FIXINDEX pins 2 "
        "of the 5 weights): the design matrix has 2 data rows for 3 weights, "
        "LAMBDA 0: fewer data rows than weights, so it does not have full "
        "column rank and the least-squares weights are not unique." +
            std::string(kRemedies));

  phi_type zero2 = design(6, 5, 5);
  for (size_t i = 0; i < 6; ++i) zero2(i, 1) = 0.0;
  CHECK(krr_fix_refusal("2", "1", {"0.1"}, zero2, target(6, 5)) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 free weights (FIXINDEX pins 1 "
        "of the 5 weights): the design matrix (6 data rows for 4 weights, "
        "LAMBDA 0) does not have full column rank: its column 1 (weight 2) is "
        "zero or a linear combination of the columns before it, so the "
        "least-squares weights are not unique." + std::string(kRemedies));
  CHECK(krr_fix_refusal("4", "1", {"0.1"}, zero2, target(6, 5)) ==
        "OALGO 4 (normal equations, Cholesky) cannot fit these 4 free weights "
        "(FIXINDEX pins 1 of the 5 weights): the matrix Phi^T Phi of the "
        "design matrix (6 data rows for 4 weights, LAMBDA 0) is not positive "
        "definite (its leading minor of order 1 is not; column 1 fits weight "
        "2), so the design matrix does not have full column rank or is too "
        "ill-conditioned for the normal equations." + std::string(kRemedies));

  const double nan = std::numeric_limits<double>::quiet_NaN();
  phi_type bad5 = design(6, 5, 5);
  bad5(1, 4) = nan;
  CHECK(krr_fix_refusal("1", "1", {"0.1"}, bad5, target(6, 5)) ==
        "LinearRegressor::train: the design matrix holds a value that is not "
        "finite (nan or inf) at row 2, column 4 (of 6 data rows, then 5 ridge "
        "rows, and 4 columns, the free weights'; column 4 fits weight 5): a "
        "descriptor or basis function gave it; no weights were fitted");
  phi_type bad1 = design(6, 5, 5);
  bad1(1, 0) = nan;
  CHECK(krr_fix_refusal("1", "1", {"0.1"}, bad1, target(6, 5)) ==
        "LinearRegressor::train: the target vector, less the part of the "
        "FIXINDEX weights, holds a value that is not finite (nan or inf) at "
        "row 2 (of 6 data rows, then 5 ridge rows); no weights were fitted");
}

/* Pins: with FIXINDEX and BIAS true over two species (ATOM Ta Nb, intercept
 * columns 1 and 2 of 4 weights) and LAMBDA 0.1, the OALGO 2 and 4
 * pre-check of M_BLR_Train and M_KRR_Train (Kern_Linear) counts the free
 * intercept columns, which the ridge leaves unpenalised: 1 data row
 * (sine_design(1, 4)),
 *  - without FIXINDEX is refused: "... cannot fit these 4 weights: the design
 *    matrix has 1 data row for 4 weights, LAMBDA 0.1, which does not penalise
 *    its 2 intercept columns: fewer data rows than intercept columns, so it
 *    does not have full column rank and the least-squares weights are not
 *    unique. A fit is possible with OALGO 1 (DGELSD), which gives the
 *    minimum-norm weights, or with more structures.";
 *  - with FIXINDEX 1 (Ta's intercept pinned) is fitted (no exception): 1
 *    free intercept column for the 1 data row;
 *  - with FIXINDEX 4 (a descriptor weight pinned) is refused: "... cannot
 *    fit these 3 free weights (FIXINDEX pins 1 of the 4 weights): the
 *    design matrix has 1 data row for 3 weights, LAMBDA 0.1, which does not
 *    penalise its 2 intercept columns: fewer data rows than intercept
 *    columns, ...".
 * Only the number of free intercept columns is asserted here: the
 * pre-check reads their number, and the fit of FIXINDEX 1 is made whichever
 * free column the ridge leaves out. Their positions among the free columns
 * and the ridge on the others: test_intercept_ridge.cpp ("M_BLR_Train and
 * M_KRR_Train with FIXINDEX pinning an intercept ...").
 * Before the fix: passes as well (a pin of the merge of v1-dev into
 * fix/input-robustness: v1-dev's free_intercept_columns() gives train() the
 * free intercept columns, the branch's Layout counts the data rows and
 * names the pinned weights). With the intercept columns of the full matrix
 * passed unmapped (bf.intercept_columns(context) in the FIXINDEX call of
 * M_BLR_Train) it fails for FIXINDEX 1: "... these 3 free weights (FIXINDEX
 * pins 1 of the 4 weights): the design matrix has 1 data row for 3
 * weights, LAMBDA 0.1, which does not penalise its 2 intercept columns:
 * fewer data rows than intercept columns, ...".
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: with FIXINDEX and BIAS the OALGO 2 and 4 "
          "pre-check counts the free intercept columns",
          "[linear_regressor][lapack_messages][fixed_weights][bias]") {
  for (const bool krr : {false, true}) {
    for (const char *oalgo : {"2", "4"}) {
      INFO((krr ? "M_KRR_Train" : "M_BLR_Train") << ", OALGO " << oalgo);
      const std::string name = oalgo[0] == '2'
                                   ? "OALGO 2 (DGELS, QR)"
                                   : "OALGO 4 (normal equations, Cholesky)";
      const std::string rest =
          "LAMBDA 0.1, which does not penalise its 2 intercept columns: fewer "
          "data rows than intercept columns, so it does not have full column "
          "rank and the least-squares weights are not unique. A fit is "
          "possible with OALGO 1 (DGELSD), which gives the minimum-norm "
          "weights, or "
          "with more structures.";
      const Context c = with_two_species_bias(make_ctx(oalgo));
      CHECK(model_refusal(krr, c, sine_design(1, 4), target(1, 4)) ==
            name + " cannot fit these 4 weights: the design matrix has 1 data "
                   "row for 4 weights, " + rest);

      Context pin_ta = c;
      pin_ta.add("FIXINDEX", "1");
      pin_ta.add("FIXWEIGHT", "0.1");
      CHECK(model_refusal(krr, pin_ta, sine_design(1, 4), target(1, 4)) ==
            "no exception");

      Context pin_d = c;
      pin_d.add("FIXINDEX", "4");
      pin_d.add("FIXWEIGHT", "0.1");
      CHECK(model_refusal(krr, pin_d, sine_design(1, 4), target(1, 4)) ==
            name + " cannot fit these 3 free weights (FIXINDEX pins 1 of the "
                   "4 weights): the design matrix has 1 data row for 3 "
                   "weights, " + rest);
    }
  }
}

/* Pins: OALGO 2 and 4 count the non-zero data rows of the design matrix,
 * which bound its rank: a row of priority 0 (a structure's eweight,
 * fweight or sweight 0, a WDBFILE value of 0, or an energy under ENERGY
 * false) is zero in the design matrix DesignMatrix builds, and so is its
 * target. With zero data rows:
 *  - 3 data rows of sine values for 3 weights, row 3 zero, LAMBDA 0: OALGO
 *    4 in train() and train_with_factor(), and OALGO 2, refuse before
 *    LAPACK: "OALGO 4 (normal equations, Cholesky) cannot fit these 3
 *    weights: the design matrix has 2 non-zero data rows of 3 (a data row
 *    that is zero in every column adds nothing to the rank: a row of
 *    priority 0, or a row the descriptors make zero, such as the y and z
 *    force rows of a linear molecule along x) for 3 weights, LAMBDA 0:
 *    fewer non-zero data rows than weights, so it does not have full
 *    column rank and the least-squares weights are not unique." + the
 *    remedies (OALGO 2 the same with "OALGO 2 (DGELS, QR)");
 *  - LAMBDA 0.1, BIAS true with ATOM Ta Nb (2 intercept columns), 2 data rows
 *    for 4 weights, row 2 zero, OALGO 2 and 4: "... the design matrix has 1
 *    non-zero data row of 2 (a data row that is zero ...) for 4 weights,
 *    LAMBDA 0.1, which does not penalise its 2 intercept columns: fewer
 *    non-zero data rows than intercept columns, so ... A fit is possible with
 *    OALGO 1 (DGELSD), which gives the minimum-norm weights, or with more
 *    structures.";
 *  - 6 data rows for 4 weights whose columns 3 and 4 are zero, rows 5 and 6
 *    zero: the 4 non-zero rows pass the pre-check and LAPACK finds the
 *    zero column: "the design matrix (4 non-zero data rows of 6 for 4
 *    weights, LAMBDA 0) does not have full column rank: its column 3 is
 *    zero ..." (OALGO 2) and "the matrix Phi^T Phi of the design matrix (4
 *    non-zero data rows of 6 for 4 weights, LAMBDA 0) is not positive
 *    definite (its leading minor of order 3 is not), ..." (OALGO 4);
 *  - FIXINDEX 5 (M_BLR_Train and M_KRR_Train), OALGO 2, 4 data rows for 5
 *    weights, rows 3 and 4 zero: "OALGO 2 (DGELS, QR) cannot fit these 4
 *    free weights (FIXINDEX pins 1 of the 5 weights): the design matrix
 *    has 2 non-zero data rows of 4 (a data row that is zero in every
 *    column adds nothing to the rank: a row of priority 0, a row whose
 *    values are all in FIXINDEX columns, or a row the descriptors make
 *    zero, ...) for 4 weights, LAMBDA 0: fewer non-zero data rows than
 *    weights, ...".
 * Without a zero data row the messages are those of the cases above. Rows
 * that are zero for another reason than priority 0: the next case. (Until
 * the zero-row wording fix the parenthesis was "(a row of weight 0 is
 * zero)", whatever made the rows zero; until the priority wording fix it
 * said "a row of weight 0".)
 * Before the fix: the pre-check counted every data row, a zero one too, so
 * whether the fit was refused depended on the rounding again: OALGO 4
 * fitted the 2 non-zero sine rows for 3 weights without an error, in
 * train() and train_with_factor() (as it fitted 2 energies for 3 weights
 * before the pre-check), and OALGO 2 said "the design matrix (3 data rows
 * for 3 weights, LAMBDA 0) does not have full column rank: its column 3 is
 * zero or a linear combination of the columns before it, ..."; with LAMBDA
 * 0.1, OALGO 2 said "... the design matrix (2 data rows for 4 weights,
 * LAMBDA 0.1) does not have full column rank: its column 2 is zero ..."
 * and OALGO 4 "... (2 data rows for 4 weights, LAMBDA 0.1) is not positive
 * definite (its leading minor of order 2 is not) ...", both with the
 * LAMBDA 0 remedies; the LAPACK messages said "(6 data rows for 4
 * weights, LAMBDA 0)"; with FIXINDEX 5, "... the design matrix (4 data
 * rows for 4 weights, LAMBDA 0) does not have full column rank: its column
 * 3 (weight 3) is zero or a linear combination of the columns before it,
 * ...".
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: OALGO 2 and 4 count the non-zero data rows, "
          "a row of priority 0 being zero",
          "[linear_regressor][lapack_messages][weight_zero]") {
  phi_type sine3 = sine_design(3, 3);
  t_type t3 = target(3, 3);
  zero_row(sine3, t3, 2);
  const std::string two_of_three =
      " cannot fit these 3 weights: the design matrix has 2 non-zero data "
      "rows of 3" + std::string(kZeroRows) + " for 3 weights, LAMBDA 0: "
      "fewer non-zero data rows than weights, so it does not have full column "
      "rank and the least-squares weights are not unique." +
      std::string(kRemedies);
  CHECK(fit_refusal("4", sine3, t3) ==
        "OALGO 4 (normal equations, Cholesky)" + two_of_three);
  CHECK(factor_refusal(sine3, t3) ==
        "OALGO 4 (normal equations, Cholesky)" + two_of_three);
  CHECK(fit_refusal("2", sine3, t3) == "OALGO 2 (DGELS, QR)" + two_of_three);

  phi_type sine2 = sine_design(2, 4);
  t_type t2 = target(2, 4);
  zero_row(sine2, t2, 1);
  for (const char *oalgo : {"2", "4"}) {
    INFO("OALGO " << oalgo);
    CHECK(fit_refusal(with_two_species_bias(make_ctx(oalgo)), sine2, t2) ==
          std::string(oalgo[0] == '2' ? "OALGO 2 (DGELS, QR)"
                                      : "OALGO 4 (normal equations, Cholesky)") +
              " cannot fit these 4 weights: the design matrix has 1 non-zero "
              "data row of 2" + kZeroRows + " for 4 weights, LAMBDA 0.1, "
                                            "which does not penalise its 2 "
                                            "intercept columns: fewer "
                                            "non-zero data rows than "
                                            "intercept columns, so it does "
                                            "not have full column rank and "
                                            "the least-squares weights are "
                                            "not unique. A fit is possible "
                                            "with OALGO 1 (DGELSD), which "
                                            "gives the minimum-norm weights, "
                                            "or "
                                            "with more structures.");
  }

  phi_type cols2 = design(6, 4, 2);
  t_type t6 = target(6, 4);
  zero_row(cols2, t6, 4);
  zero_row(cols2, t6, 5);
  CHECK(fit_refusal("2", cols2, t6) ==
        "OALGO 2 (DGELS, QR) cannot fit these 4 weights: the design matrix (4 "
        "non-zero data rows of 6 for 4 weights, LAMBDA 0) does not have full "
        "column rank: its column 3 is zero or a linear combination of the "
        "columns before it, so the least-squares weights are not unique." +
            std::string(kRemedies));
  CHECK(fit_refusal("4", cols2, t6) ==
        "OALGO 4 (normal equations, Cholesky) cannot fit these 4 weights: the "
        "matrix Phi^T Phi of the design matrix (4 non-zero data rows of 6 for "
        "4 weights, LAMBDA 0) is not positive definite (its leading minor of "
        "order 3 is not), so the design matrix does not have full column rank "
        "or is too ill-conditioned for the normal equations." +
            std::string(kRemedies));

  phi_type fix4 = design(4, 5, 5);
  t_type tf = target(4, 5);
  zero_row(fix4, tf, 2);
  zero_row(fix4, tf, 3);
  const std::string fixed =
      "OALGO 2 (DGELS, QR) cannot fit these 4 free weights (FIXINDEX pins 1 "
      "of the 5 weights): the design matrix has 2 non-zero data rows of 4" +
      std::string(kZeroRowsFixed) + " for 4 weights, LAMBDA 0: fewer "
      "non-zero data rows than weights, so it does not have full column rank "
      "and the least-squares weights are not unique." + std::string(kRemedies);
  CHECK(blr_fix_refusal("2", "5", {"0.1"}, fix4, tf) == fixed);
  CHECK(krr_fix_refusal("2", "5", {"0.1"}, fix4, tf) == fixed);
}

/* Pins: a data row that is zero for another reason than priority 0 is
 * counted as zero too, and the message does not blame priority 0: with
 * FIXINDEX 1 of 3 weights (M_BLR_Train and M_KRR_Train, OALGO 2 and 4,
 * LAMBDA 0), 2 data rows of which the second has a value in the pinned
 * column 1 only, so it is zero in the 2 free columns: "OALGO 2 (DGELS, QR)
 * cannot fit these 2 free weights (FIXINDEX pins 1 of the 3 weights): the
 * design matrix has 1 non-zero data row of 2 (a data row that is zero in
 * every column adds nothing to the rank: a row of priority 0, a row whose
 * values are all in FIXINDEX columns, or a row the descriptors make zero,
 * such as the y and z force rows of a linear molecule along x) for 2
 * weights, LAMBDA 0: fewer non-zero data rows than weights, so it does not
 * have full column rank and the least-squares weights are not unique." +
 * the remedies. (Tadah.MLIP's CLI tests: a dimer's y and z force rows and
 * an isolated atom's energy, of priority 1.)
 * Before the fix: the rows were counted alike, but the message said "the
 * design matrix has 1 non-zero data row of 2 (a row of weight 0 is zero)
 * for 2 weights, ...", whatever made the row zero.
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: OALGO 2 and 4 count a zero data row of a "
          "priority above 0, not blaming priority 0",
          "[linear_regressor][lapack_messages][weight_zero][fixed_weights]") {
  phi_type Phi = design(2, 3, 3);
  Phi(1, 1) = 0.0;
  Phi(1, 2) = 0.0;
  const t_type T = target(2, 3);
  for (const bool krr : {false, true}) {
    for (const char *oalgo : {"2", "4"}) {
      INFO((krr ? "M_KRR_Train" : "M_BLR_Train") << ", OALGO " << oalgo);
      const std::string expected =
          std::string(oalgo[0] == '2' ? "OALGO 2 (DGELS, QR)"
                                      : "OALGO 4 (normal equations, Cholesky)") +
          " cannot fit these 2 free weights (FIXINDEX pins 1 of the 3 "
          "weights): the design matrix has 1 non-zero data row of 2" +
          kZeroRowsFixed + " for 2 weights, LAMBDA 0: fewer non-zero data "
          "rows than weights, so it does not have full column rank and the "
          "least-squares weights are not unique." + kRemedies;
      CHECK((krr ? krr_fix_refusal(oalgo, "1", {"0.1"}, Phi, T)
                 : blr_fix_refusal(oalgo, "1", {"0.1"}, Phi, T)) == expected);
    }
  }
}

/* Pins: a data row whose values are rounding -- at most 1e-10 of the
 * largest magnitude of their column among the data rows, as the force
 * rows of an atom at a centre of symmetry come out of their cancelling
 * sums -- is a zero row: 3 data rows of 3 sine columns whose third row is
 * 1e-17 times the sines (its target 1.5), OALGO 2 and 4, LAMBDA 0,
 * are refused as 2 non-zero data rows of 3 for 3 weights, the message of
 * an exactly zero third row. A third row of 1e-8 times the sines is a data
 * row: OALGO 2 fits it.
 * Before the fix: a row counted unless every value was exactly 0, so the
 * rounded row made 3 data rows and LAPACK fitted weights of the order of
 * 1e17 (no exception).
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: OALGO 2 and 4 count a data row of rounding "
          "residues as zero",
          "[linear_regressor][lapack_messages][weight_zero]") {
  auto scaled_third_row = [](double scale) {
    phi_type Phi = sine_design(3, 3);
    for (size_t j = 0; j < 3; ++j)
      Phi(2, j) = scale * std::sin(2.0 + 0.3 * static_cast<double>(j));
    return Phi;
  };
  const t_type T = target(3, 3);
  const std::string two_of_three =
      " cannot fit these 3 weights: the design matrix has 2 non-zero data "
      "rows of 3" + std::string(kZeroRows) + " for 3 weights, LAMBDA 0: "
      "fewer non-zero data rows than weights, so it does not have full column "
      "rank and the least-squares weights are not unique." +
      std::string(kRemedies);
  const phi_type rounded = scaled_third_row(1e-17);
  CHECK(fit_refusal("4", rounded, T) ==
        "OALGO 4 (normal equations, Cholesky)" + two_of_three);
  CHECK(factor_refusal(rounded, T) ==
        "OALGO 4 (normal equations, Cholesky)" + two_of_three);
  CHECK(fit_refusal("2", rounded, T) == "OALGO 2 (DGELS, QR)" + two_of_three);
  CHECK(fit_refusal("2", scaled_third_row(1e-8), T) == "no exception");
}

/* Pins: with LAMBDA > 0 the message of a fit LAPACK cannot make names the
 * remedies that apply to it: 4 data rows for 2 equal columns (every value
 * 1), LAMBDA 1e-20, whose ridge 1e-20 vanishes against Phi^T Phi's 4, so
 * DPOTRF meets the pivot 4 - 2 * 2 = 0 at order 2:
 *  - BIAS false, OALGO 4: "OALGO 4 (normal equations, Cholesky) cannot fit
 *    these 2 weights: the matrix Phi^T Phi of the design matrix (4 data rows
 *    for 2 weights, LAMBDA 1e-20) is not positive definite (its leading minor
 *    of order 2 is not), so the design matrix does not have full column rank
 *    or is too ill-conditioned for the normal equations. A fit is possible with
 *    OALGO 1 (DGELSD), 2 (QR) or 3 (SVD), which do not form Phi^T Phi, with a
 *    larger LAMBDA (the ridge penalty makes every column it penalises full
 *    rank and better conditioned), or with more data (structures, FORCE or
 *    STRESS rows)."; OALGO 1, 2 and 3 fit it (pins);
 *  - BIAS true with ATOM Ta (intercept column 1 of 3 equal columns), OALGO 4:
 *    "... cannot fit these 3 weights: ... (4 data rows for 3 weights, LAMBDA
 *    1e-20) is not positive definite (its leading minor of order 2 is not),
 *    ... A fit is possible with OALGO 1 (DGELSD) or 2 (QR), which do not form
 *    Phi^T Phi, with a larger LAMBDA (the ridge penalty makes every column it
 *    penalises full rank and better conditioned; it does not penalise the 1
 *    intercept column of BIAS), or with more data (structures, FORCE or
 *    STRESS rows)." (OALGO 3 is not named: it refuses the intercept columns
 *    with LAMBDA != 0, a pin); OALGO 1 and 2 fit it (pins).
 * With LAMBDA 0 the remedies are those of the cases above.
 * Before the fix: both messages ended in the LAMBDA 0 remedies, " Use
 * OALGO 1 (DGELSD) or OALGO 3 (SVD), which give the minimum-norm weights,
 * LAMBDA > 0 (...), or more data (...)", which name OALGO 3 with BIAS and
 * LAMBDA > 0 with LAMBDA already 1e-20.
 * Before the fix that states the remedies: they read "Use OALGO 1 (DGELSD)
 * ...", "a larger LAMBDA (...)" and "or more data (...)" ("Use OALGO 1
 * (DGELSD), which gives the minimum-norm weights, or more structures." with
 * LAMBDA > 0), an instruction: the checks of the remedies fail. */
TEST_CASE("LinearRegressor: with LAMBDA > 0 the OALGO 4 message names the "
          "remedies that apply",
          "[linear_regressor][lapack_messages][bias]") {
  auto ctx = [](const char *oalgo, bool bias) {
    Context c = make_ctx(oalgo);
    c.remove("LAMBDA");
    c.add("LAMBDA", "1e-20");
    if (bias) {
      c.add("BIAS", "true");
      c.add("ATOM", "Ta");
    }
    return c;
  };
  const std::string head =
      "OALGO 4 (normal equations, Cholesky) cannot fit these ";
  const std::string not_pd =
      " is not positive definite (its leading minor of order 2 is not), so "
      "the design matrix does not have full column rank or is too "
      "ill-conditioned for the normal equations.";

  CHECK(fit_refusal(ctx("4", false), ones_design(4, 2), target(4, 2)) ==
        head + "2 weights: the matrix Phi^T Phi of the design matrix (4 data "
               "rows for 2 weights, LAMBDA 1e-20)" + not_pd +
            " A fit is possible with OALGO 1 (DGELSD), 2 (QR) or 3 (SVD), "
            "which do not form Phi^T Phi, with a larger LAMBDA (the ridge "
            "penalty makes every column it penalises full rank and better "
            "conditioned), or with more data (structures, FORCE or STRESS "
            "rows).");
  for (const char *oalgo : {"1", "2", "3"}) {
    INFO("BIAS false, OALGO " << oalgo);
    CHECK(fit_refusal(ctx(oalgo, false), ones_design(4, 2), target(4, 2)) ==
          "no exception");
  }

  CHECK(fit_refusal(ctx("4", true), ones_design(4, 3), target(4, 3)) ==
        head + "3 weights: the matrix Phi^T Phi of the design matrix (4 data "
               "rows for 3 weights, LAMBDA 1e-20)" + not_pd +
            " A fit is possible with OALGO 1 (DGELSD) or 2 (QR), which do not "
            "form Phi^T Phi, with a larger LAMBDA (the ridge penalty makes "
            "every column it penalises full rank and better conditioned; it "
            "does not penalise the 1 intercept column of BIAS), or with more "
            "data (structures, FORCE or STRESS rows).");
  for (const char *oalgo : {"1", "2"}) {
    INFO("BIAS true, OALGO " << oalgo);
    CHECK(fit_refusal(ctx(oalgo, true), ones_design(4, 3), target(4, 3)) ==
          "no exception");
  }
  CHECK(fit_refusal(ctx("3", true), ones_design(4, 3), target(4, 3)) ==
        "OALGO 3 (SVD with a uniform ridge) penalises every coefficient "
        "alike, and the model has 1 intercept column(s) (BIAS true with "
        "ATOM), which the ridge of OALGO 1, 2 and 4 leaves unpenalised and "
        "OALGO 3 cannot, so OALGO 3 takes LAMBDA 0 only; OALGO 1, 2 or 4 "
        "fits such a model with a LAMBDA other than 0.");
  /* It said "..., which a ridge penalty leaves out, so OALGO 3 takes
   * LAMBDA 0 only", which read as if every ridge left the intercepts out,
   * OALGO 3's too. Before that it said "OALGO=3 (SVD with uniform ridge)
   * does not support intercept exclusion from regularisation. The model
   * has 1 intercept column(s)
   * (BIAS=true with ATOM) and LAMBDA != 0; OALGO 1, 2 or 4 fits such a
   * model with LAMBDA != 0." */
}

/* OALGO 3 penalises every coefficient alike, so a fit with an intercept column
 * (BIAS true with ATOM Ta: one) refuses a LAMBDA other than 0. For LAMBDA
 * -1, the evidence approximation, which only OALGO 3 runs, the message
 * says what the evidence approximation needs (BIAS false, or every
 * intercept coefficient fixed with FIXINDEX and FIXWEIGHT; it said
 * "weight" and "pinned") and what fits with
 * BIAS true; it does not send the configuration to OALGO 1, 2 or 4, which
 * refuse LAMBDA -1 ("LAMBDA -1 requests the evidence approximation, which
 * only OALGO 3 (the SVD solve) runs; ..."). A positive LAMBDA keeps its
 * message (the case above). LinearRegressor::require_oalgo3_intercepts,
 * which a caller (tadah hpo) checks before any fit, throws the fit's
 * message for the configuration and the number of intercept columns the
 * fit has, and nothing with LAMBDA 0, another OALGO or no intercept column.
 * Before the fix: the test does not compile (require_oalgo3_intercepts
 * does not exist); the fit of LAMBDA -1 said "... The model has 1
 * intercept column(s) (BIAS=true with ATOM) and LAMBDA != 0: use OALGO=1,
 * 2 or 4 instead." */
TEST_CASE("LinearRegressor: OALGO 3 with LAMBDA -1 and an intercept column "
          "names what the evidence approximation needs",
          "[linear_regressor][lapack_messages][bias]") {
  auto ctx = [](const char *lambda, const char *oalgo) {
    Context c = make_ctx(oalgo);
    c.remove("LAMBDA");
    c.add("LAMBDA", lambda);
    c.add("BIAS", "true");
    c.add("ATOM", "Ta");
    c.add("ALPHA", "1");
    c.add("BETA", "1");
    return c;
  };
  const std::string ea =
      fit_refusal(ctx("-1", "3"), ones_design(4, 3), target(4, 3));
  CHECK(ea ==
        "LAMBDA -1 requests the evidence approximation, which only OALGO 3 "
        "runs; OALGO 3 penalises every coefficient alike, and the model has 1 "
        "intercept column(s) (BIAS true with ATOM), which the ridge of "
        "OALGO 1, 2 and 4 leaves unpenalised and OALGO 3 cannot. The "
        "evidence approximation needs BIAS false, or every "
        "intercept coefficient fixed with FIXINDEX and FIXWEIGHT. With BIAS "
        "true, LAMBDA 0 fits with any OALGO, and LAMBDA > 0 with OALGO 1, 2 "
        "or 4.");
  const std::string ridge =
      fit_refusal(ctx("1e-3", "3"), ones_design(4, 3), target(4, 3));
  REQUIRE_THAT(ridge, Catch::Matchers::Contains(
                          "; OALGO 1, 2 or 4 fits such a model with a LAMBDA "
                          "other than 0."));

  auto refusal = [](const Context &c, size_t n) {
    try {
      LinearRegressor::require_oalgo3_intercepts(c, n);
    } catch (const std::exception &e) {
      return std::string(e.what());
    }
    return std::string("no exception");
  };
  CHECK(refusal(ctx("-1", "3"), 1) == ea);
  CHECK(refusal(ctx("1e-3", "3"), 1) == ridge);
  CHECK(refusal(ctx("0", "3"), 1) == "no exception");
  CHECK(refusal(ctx("-1", "3"), 0) == "no exception");
  CHECK(refusal(ctx("1e-3", "1"), 1) == "no exception");
}

/* Pins: a layout that does not describe the design matrix (a logic error
 * of the C++ caller) is refused before train() or train_with_factor()
 * writes anything into it: a design matrix of 2 rows and 4 columns, which
 * has no room for the 4 ridge rows, with LAMBDA 0.1, under OALGO 1, 2 and
 * 4 (train(), Layout::of(Phi): 0 data rows) and train_with_factor() with
 * BIAS true over ATOM Ta Nb: std::logic_error "LinearRegressor::train: a
 * layout of 0 data rows, 0 column weights and 4 weights does not describe a
 * design matrix of 2 rows and 4 columns", and the matrix is as it was.
 * Before the fix: train() wrote the ridge before it checked the layout, at
 * row Phi.rows() - Phi.cols() + j, which wraps: column 1's at element -2
 * of the storage, before it (AddressSanitizer: heap-buffer-overflow, WRITE
 * of size 8), the others into the data rows (Phi(2, 1), Phi(1, 3) and
 * Phi(2, 4) became 0.316227766, sqrt(0.1)), and only then threw the
 * logic_error; the test process then aborted in malloc ("malloc():
 * mismatching next->prev_size (unsorted)"). train_with_factor() checked no
 * layout: it wrote sqrt(0.1) into Phi(1, 3) and Phi(2, 4) and threw the
 * pre-check's runtime_error ("... the design matrix has 0 data rows for 4
 * weights, LAMBDA 0.1, which does not penalise its 2 intercept columns:
 * ..."). */
TEST_CASE("LinearRegressor: a layout that does not describe the design "
          "matrix is refused before the ridge rows are written",
          "[linear_regressor][lapack_messages]") {
  phi_type Phi0(2, 4);
  for (size_t i = 0; i < 2; ++i)
    for (size_t j = 0; j < 4; ++j)
      Phi0(i, j) = 1.0 + static_cast<double>(i + 2 * j);
  t_type T(2);
  T[0] = 0.5;
  T[1] = 1.5;
  const std::string refusal =
      "LinearRegressor::train: a layout of 0 data rows, 0 column weights and "
      "4 weights does not describe a design matrix of 2 rows and 4 columns";
  auto unchanged = [&](const phi_type &Phi) {
    for (size_t i = 0; i < 2; ++i)
      for (size_t j = 0; j < 4; ++j) {
        INFO("Phi(" << i + 1 << ", " << j + 1 << ")");
        CHECK(Phi(i, j) == Phi0(i, j));
      }
  };
  for (const char *oalgo : {"1", "2", "4"}) {
    INFO("OALGO " << oalgo);
    Context c = make_ctx(oalgo);
    c.remove("LAMBDA");
    c.add("LAMBDA", "0.1");
    phi_type Phi = Phi0;
    t_type T1 = T;
    t_type w;
    std::string msg = "no exception";
    try {
      LinearRegressor::train(c, Phi, T1, w);
    } catch (const std::logic_error &e) {
      msg = e.what();
    }
    CHECK(msg == refusal);
    unchanged(Phi);
  }
  Context c = with_two_species_bias(make_ctx("4"));
  phi_type Phi = Phi0;
  t_type T1 = T;
  t_type w;
  FactorHandle factor;
  tadah::models::memory::ModelsWorkspaceManager wm;
  std::string msg = "no exception";
  try {
    LinearRegressor::train_with_factor(c, Phi, T1, w, factor, wm);
  } catch (const std::logic_error &e) {
    msg = e.what();
  } catch (const std::exception &e) {
    msg = std::string("not a logic_error: ") + e.what();
  }
  CHECK(msg == refusal);
  unchanged(Phi);
}

/* Pins: a design matrix or a target that holds a value that is not finite
 * (NaN, +-inf) is refused before LAPACK is called, under every OALGO,
 * naming the value's row and column (1-based) and that nothing was fitted:
 * "LinearRegressor::train: the design matrix holds a value that is not
 * finite (nan or inf) at row 2, column 3 (of 6 data rows, then 4 ridge
 * rows, and 4 columns): a descriptor or basis function gave it; no weights
 * were fitted", and for the target "the target vector holds a value that is
 * not finite (nan or inf) at row 5 (...)". The tests compare bits, not
 * std::isnan (the -ffast-math builds).
 * Before the fix: a NaN in the design matrix stopped in LAPACK with "Error
 * in DGELSD: info = -4" (OALGO 1), "Error in DGELS: info = -5" (OALGO 2),
 * "Error in DGESDD during SVD computation: info = 3" (OALGO 3) and "Error
 * in DPOTRF: tadah::core::Matrix is not positive definite; info = 3" (OALGO
 * 4); an infinity with "Error in DGELSD: info = 3", "Error in DGELS: info =
 * 1" and the same OALGO 3 and 4 messages; a NaN target with "Error in
 * DGELSD: info = -6" and "Error in DGELS: info = -7", and OALGO 3 and 4
 * fitted it without an error, as every OALGO fitted an infinite target. */
TEST_CASE("LinearRegressor: a design matrix or target that is not finite is "
          "refused under every OALGO, naming the row and column",
          "[linear_regressor][lapack_messages]") {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (const char *oalgo : {"1", "2", "3", "4"}) {
    INFO("OALGO " << oalgo);
    for (const double bad : {nan, inf, -inf}) {
      INFO("value bits " << bits(bad));
      phi_type Phi = design(6, 4, 4);
      Phi(1, 2) = bad;
      CHECK(fit_refusal(oalgo, Phi, target(6, 4)) ==
            "LinearRegressor::train: the design matrix holds a value that is "
            "not finite (nan or inf) at row 2, column 3 (of 6 data rows, then "
            "4 ridge rows, and 4 columns): a descriptor or basis function "
            "gave it; no weights were fitted");
      t_type T = target(6, 4);
      T[4] = bad;
      CHECK(fit_refusal(oalgo, design(6, 4, 4), T) ==
            "LinearRegressor::train: the target vector holds a value that is "
            "not finite (nan or inf) at row 5 (of 6 data rows, then 4 ridge "
            "rows); no weights were fitted");
    }
  }
}

/* Pins: train_with_factor (OALGO 4 with the Cholesky factor cached for a
 * caller's analytical gradient) refuses a non-finite design matrix as
 * train() does: "LinearRegressor::train: the design matrix holds a value
 * that is not finite (nan or inf) at row 2, column 3 (...)".
 * Before the fix: "OLS::solveAndCacheFactor: DPOTRF failed (matrix is not
 * positive definite); info = 3. This typically means a rank-deficient
 * design matrix or LAMBDA=0 with insufficient data; ...". */
TEST_CASE("LinearRegressor::train_with_factor refuses a design matrix that "
          "is not finite",
          "[linear_regressor][lapack_messages]") {
  Context c = make_ctx("4");
  phi_type Phi = design(6, 4, 4);
  Phi(1, 2) = std::numeric_limits<double>::quiet_NaN();
  t_type T = target(6, 4);
  t_type w;
  FactorHandle factor;
  tadah::models::memory::ModelsWorkspaceManager wm;
  std::string msg = "no exception";
  try {
    LinearRegressor::train_with_factor(c, Phi, T, w, factor, wm);
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK(msg ==
        "LinearRegressor::train: the design matrix holds a value that is not "
        "finite (nan or inf) at row 2, column 3 (of 6 data rows, then 4 ridge "
        "rows, and 4 columns): a descriptor or basis function gave it; no "
        "weights were fitted");
}
