#ifndef LINEAR_REGRESSOR_H
#define LINEAR_REGRESSOR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/intercept_layout.h>
#include <tadah/core/lapack.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>
#include <tadah/core/utils.h>
#include <tadah/models/ea.h>
#include <tadah/models/factor_handle.h>
#include <tadah/models/fit_warnings.h>
#include <tadah/models/ols.h>
#include <tadah/models/svd.h>
#include <tadah/models/zero_rows.h>
#include <tadah/models/memory/imodels_workspace_manager.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/**
 * @class LinearRegressor
 * @brief Performs linear regression using Singular Value Decomposition (SVD).
 *
 * Uses SVD for training, supporting both ordinary least squares and ridge
 * regression.
 */
class LinearRegressor {
public:
  /**
   * @brief The rows and columns of a design matrix given to train().
   *
   * Rows 0 .. data_rows-1 of Phi hold the data; the rows after them, at
   * least Phi.cols(), are ridge rows (zero; train() writes the LAMBDA
   * penalty into the last Phi.cols() of them). Column j of Phi fits weight
   * column_weight[j] (0-based) of the model's n_weights, or weight j when
   * column_weight is empty. train()'s checks and messages count these.
   */
  struct Layout {
    size_t data_rows = 0;               ///< rows that hold data
    size_t n_weights = 0;               ///< the model's weights, pinned ones included
    std::vector<size_t> column_weight;  ///< the weight of each column; empty: column j is weight j

    /** DesignMatrix's layout of @p Phi: its data rows, then one ridge row
     *  per column (Phi.rows() - Phi.cols() data rows), column j weight j. */
    static Layout of(const tadah::core::phi_type &Phi) {
      Layout L;
      L.data_rows = Phi.rows() >= Phi.cols() ? Phi.rows() - Phi.cols() : 0;
      L.n_weights = Phi.cols();
      return L;
    }

    /** The layout of a design matrix laid out as DesignMatrix lays it out,
     *  with @p data_rows data rows and move_map.size() weights, after
     *  M_Train::prep_train_with_residuals moved the FIXINDEX columns to the
     *  right and they were removed: its first @p n_free columns fit weights
     *  move_map[0 .. n_free-1], and every ridge row is kept. */
    static Layout fixed(size_t data_rows, const std::vector<size_t> &move_map,
                        size_t n_free) {
      Layout L;
      L.data_rows = data_rows;
      L.n_weights = move_map.size();
      L.column_weight.assign(move_map.begin(),
                             move_map.begin() +
                                 static_cast<std::ptrdiff_t>(
                                     std::min(n_free, move_map.size())));
      return L;
    }

    /** The weights FIXINDEX pins: those that have no column. */
    size_t pinned(size_t cols) const {
      return n_weights > cols ? n_weights - cols : 0;
    }
  };

private:
  static std::string plural(size_t n, const std::string &word) {
    return std::to_string(n) + " " + word + (n == 1 ? "" : "s");
  }

  /* "these 4 weights", or with pinned weights "these 4 free weights
   * (FIXINDEX pins 1 of the 5 weights)". */
  static std::string weights_text(size_t cols, const Layout &L) {
    const size_t k = L.pinned(cols);
    if (k == 0) return "these " + plural(cols, "weight");
    return "these " + std::to_string(cols) + " free weights (FIXINDEX pins " +
           std::to_string(k) + " of the " + plural(L.n_weights, "weight") + ")";
  }

  /* The weight (1-based) that column `c` (1-based) fits. */
  static size_t weight_of(size_t c, const Layout &L) {
    return L.column_weight.empty() || c == 0 || c > L.column_weight.size()
               ? c
               : L.column_weight[c - 1] + 1;
  }

  /* Checks that `L` describes `Phi` (a logic error of the caller). */
  static void require_layout(const tadah::core::phi_type &Phi, const Layout &L) {
    if (Phi.rows() < L.data_rows + Phi.cols() ||
        (!L.column_weight.empty() && L.column_weight.size() != Phi.cols()) ||
        L.n_weights < Phi.cols())
      throw std::logic_error(
          "LinearRegressor::train: a layout of " + plural(L.data_rows, "data row") +
          ", " + plural(L.column_weight.size(), "column weight") + " and " +
          plural(L.n_weights, "weight") + " does not describe a design "
          "matrix of " + std::to_string(Phi.rows()) + " rows and " +
          std::to_string(Phi.cols()) + " columns");
  }

  /* True for a value that is not NaN or +-inf; a test on the bits, which
   * -ffast-math does not optimise away. */
  static bool finite_bits(double v) {
    std::uint64_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    return (u & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
  }

  /* Refuses a design matrix or target with a value that is not finite,
   * naming its row and column, before LAPACK is called: on NaN or inf
   * LAPACK stops with an argument or convergence error (DGELSD info -4,
   * DGELS -5, DGESDD 3), DPOTRF calls the matrix not positive definite, and
   * an infinite target is fitted without an error. With pinned weights the
   * target is the residual, less the pinned weights' part. */
  static void require_finite(const tadah::core::phi_type &Phi,
                             const tadah::core::t_type &T, const Layout &L) {
    const size_t data = L.data_rows;
    const bool pins = L.pinned(Phi.cols()) > 0;
    const std::string rows =
        plural(data, "data row") + ", then " +
        plural(Phi.rows() - data, "ridge row");
    for (size_t j = 0; j < Phi.cols(); ++j)
      for (size_t i = 0; i < Phi.rows(); ++i)
        if (!finite_bits(Phi(i, j)))
          throw std::runtime_error(
              "LinearRegressor::train: the design matrix holds a value that "
              "is not finite (nan or inf) at row " + std::to_string(i + 1) +
              ", column " + std::to_string(j + 1) + " (of " + rows + ", and " +
              plural(Phi.cols(), "column") +
              (pins ? ", the free weights'; column " + std::to_string(j + 1) +
                          " fits weight " +
                          std::to_string(weight_of(j + 1, L))
                    : std::string()) +
              "): a descriptor or basis function gave it; no weights were "
              "fitted");
    for (size_t i = 0; i < T.size() && i < Phi.rows(); ++i)
      if (!finite_bits(T[i]))
        throw std::runtime_error(
            std::string("LinearRegressor::train: the target vector") +
            (pins ? ", less the part of the FIXINDEX weights," : "") +
            " holds a value that is not finite (nan or inf) at row " +
            std::to_string(i + 1) + " (of " + rows +
            "); no weights were fitted");
  }

  /* Refuses coefficients of a solve that are not finite (nan or inf),
   * naming the OALGO line and the first such coefficient (its weight, when
   * FIXINDEX pins some): a fit never returns them, and tadah train would
   * write them into a potential no command reads. The design matrix and
   * the targets are finite (require_finite()), so a product of the solve
   * overflowed. @p svd is the SVD of the OALGO 3 solve (null for the
   * others), with its rank threshold @p rcond and the LAMBDA @p lambda it
   * solved with: when it keeps a singular direction whose factor S / (S^2 +
   * LAMBDA) is not finite (S^2 underflows to 0 at LAMBDA 0), the
   * message names the smallest such S and what leaves it out or keeps its
   * coefficient finite (a LAMBDA above 0; a rank threshold M of at least
   * its ratio to the largest singular value, below 1, which the default
   * 1e-8 is when the ratio is at most 1e-8); otherwise it names none. */
  static void require_finite_weights(const tadah::core::Context &context,
                                     const tadah::core::t_type &weights,
                                     const Layout &L,
                                     tadah::models::SVD *svd = nullptr,
                                     double rcond = 0.0,
                                     double lambda = 0.0) {
    size_t bad = weights.size();
    for (size_t j = 0; j < weights.size(); ++j)
      if (!finite_bits(weights[j])) {
        bad = j;
        break;
      }
    if (bad == weights.size()) return;
    std::string line = "OALGO";
    for (const std::string &v : context("OALGO")) line += " " + v;
    const bool pins = L.pinned(weights.size()) > 0;
    std::string msg =
        line + ": the solve gave coefficients that are not finite (nan or "
        "inf; the first is coefficient " + std::to_string(bad + 1) + " of " +
        std::to_string(weights.size()) +
        (pins ? ", weight " + std::to_string(weight_of(bad + 1, L)) : "") +
        ")";
    if (svd) {
      const double *S = svd->getS();
      const int k = std::min(svd->rows(), svd->cols());
      const double thresh = OLS::pseudoinverse_threshold(S, k, rcond);
      double largest = 0.0;
      for (int i = 0; i < k; ++i) largest = std::max(largest, S[i]);
      // The smallest kept singular value whose factor in the solve is not
      // finite (OLS::solveWithPseudoinverse: S / (S^2 + LAMBDA)).
      double culprit = 0.0;
      for (int i = 0; i < k; ++i)
        if (S[i] > thresh && !finite_bits(S[i] / (S[i] * S[i] + lambda)) &&
            (culprit == 0.0 || S[i] < culprit))
          culprit = S[i];
      if (culprit > 0.0) {
        const double ratio = culprit / largest;
        std::ostringstream value, share;
        value << culprit;
        share << ratio;
        msg += ": it keeps a singular direction of the design matrix of "
               "singular value " + value.str() + ", whose square underflows "
               "to 0 in double precision, so that its coefficient is not "
               "finite at LAMBDA 0. A LAMBDA above 0 keeps that coefficient "
               "finite";
        if (ratio < 1.0)
          msg += ", and a rank threshold M (OALGO's second value) of at "
                 "least " + share.str() + ", the ratio of that singular "
                 "value to the largest, leaves the direction out" +
                 std::string(ratio <= 1e-8
                                 ? " (OALGO 3 with its default M, 1e-8, "
                                   "does)"
                                 : "");
      }
    }
    throw std::runtime_error(msg + ". No coefficients were fitted.");
  }

  static std::string oalgo_name(int oalgo) {
    return oalgo == 2 ? "OALGO 2 (DGELS, QR)"
                      : "OALGO 4 (normal equations, Cholesky)";
  }

  /* What makes a fit OALGO `oalgo` (2 or 4) cannot make, for its LAMBDA
   * and its `n_bias` intercept columns. With LAMBDA 0: OALGO 1 or 3, a
   * ridge or more data. With LAMBDA > 0, which is already a ridge: OALGO 1
   * (and, for OALGO 4, OALGO 2, which does not square the condition
   * number), OALGO 3 only without intercept columns (with them it refuses
   * LAMBDA != 0), a larger LAMBDA, or more data. */
  static std::string remedies(int oalgo, double lambda, size_t n_bias) {
    const std::string data =
        "or with more data (structures, FORCE or STRESS rows).";
    if (!(lambda > 0))
      return " A fit is possible with OALGO 1 (DGELSD) or OALGO 3 (SVD), which "
             "give the minimum-norm weights, with LAMBDA > 0 (a ridge penalty "
             "makes every column it penalises full rank; it does not "
             "penalise the intercept columns of BIAS), " + data;
    std::string algos;
    if (oalgo == 4)
      algos = n_bias == 0 ? "OALGO 1 (DGELSD), 2 (QR) or 3 (SVD), which do "
                            "not form Phi^T Phi"
                          : "OALGO 1 (DGELSD) or 2 (QR), which do not form "
                            "Phi^T Phi";
    else
      algos = n_bias == 0 ? "OALGO 1 (DGELSD) or 3 (SVD)"
                          : "OALGO 1 (DGELSD), which gives the minimum-norm "
                            "weights";
    return " A fit is possible with " + algos + ", with a larger LAMBDA (the "
           "ridge penalty makes every column it penalises full rank and "
           "better conditioned" +
           (n_bias == 0 ? std::string()
                        : "; it does not penalise the " +
                              plural(n_bias, "intercept column") + " of BIAS") +
           "), " + data;
  }

  /* What the messages say of the zero data rows: a row of priority 0 is
   * one, but not the only one. */
  static std::string zero_rows_text(size_t cols, const Layout &L) {
    return std::string(" (a data row that is zero in every column adds "
                       "nothing to the rank: a row of priority 0, ") +
           (L.pinned(cols) > 0 ? "a row whose values are all in FIXINDEX "
                                 "columns, or "
                               : "or ") +
           "a row the descriptors make zero, such as the y and z force rows "
           "of a linear molecule along x)";
  }

  /* The data rows of Phi (its first L.data_rows) that are not zero in
   * every column (zero_rows(): no value above kZeroRowTolerance of the
   * largest magnitude of its column, so a row of rounding residues is
   * zero). A zero row adds nothing to the rank, so the rank is at most
   * their number plus the ridge rows that carry a penalty; a row of
   * priority 0 (a structure's eweight, fweight or sweight 0, a WDBFILE value
   * of 0, or an energy under ENERGY false) is zero in the design matrix
   * DesignMatrix builds, and so is a row whose values are all in pinned
   * (removed) columns, or a force row of an atom at a centre of
   * symmetry. */
  static size_t nonzero_data_rows(const tadah::core::phi_type &Phi,
                                  const Layout &L) {
    const std::vector<char> zero = zero_rows(Phi, L.data_rows);
    return static_cast<size_t>(std::count(zero.begin(), zero.end(), 0));
  }

  /* "6 data rows", or with zero data rows "4 non-zero data rows of 6". */
  static std::string data_rows_text(size_t nonzero, const Layout &L) {
    if (nonzero >= L.data_rows) return plural(L.data_rows, "data row");
    return plural(nonzero, "non-zero data row") + " of " +
           std::to_string(L.data_rows);
  }

  /* Refuses, for OALGO 2 and 4, a design matrix whose rows cannot give it
   * full column rank, before LAPACK runs: its rank is at most its
   * `nonzero` non-zero data rows (nonzero_data_rows) plus the ridge rows
   * that carry a penalty (LAMBDA > 0: one for each column but the n_bias
   * intercept columns). LAPACK's own tests depend on rounding: DGELS stops
   * at an exactly zero diagonal element of R, DPOTRF at a pivot that is
   * not positive, and the rounded Phi^T Phi of fewer data rows than
   * weights is often factored. */
  static void require_enough_rows(int oalgo, const tadah::core::phi_type &Phi,
                                  const Layout &L, double lambda,
                                  size_t n_bias, size_t nonzero) {
    const size_t n = Phi.cols();
    const size_t bias = std::min(n_bias, n);
    const bool ridge = lambda > 0;
    if (nonzero + (ridge ? n - bias : 0) >= n) return;
    const bool zero_rows = nonzero < L.data_rows;
    const std::string rows = zero_rows ? "non-zero data rows" : "data rows";
    std::string msg = oalgo_name(oalgo) + " cannot fit " + weights_text(n, L) +
                      ": the design matrix has " + data_rows_text(nonzero, L) +
                      (zero_rows ? zero_rows_text(n, L) : std::string()) +
                      " for " + plural(n, "weight") + ", LAMBDA " +
                      tadah::core::format_roundtrip(lambda);
    if (ridge)
      msg += ", which does not penalise its " + plural(bias, "intercept column") +
             ": fewer " + rows + " than intercept columns";
    else
      msg += ": fewer " + rows + " than weights";
    msg += ", so it does not have full column rank and the least-squares "
           "weights are not unique.";
    // With LAMBDA > 0 the intercept columns are short of rows: a larger
    // LAMBDA, OALGO 2 or 4 and OALGO 3 (which refuses them) do not help.
    msg += ridge ? " A fit is possible with OALGO 1 (DGELSD), which gives the "
                   "minimum-norm weights, or with more structures."
                 : remedies(oalgo, lambda, bias);
    throw std::runtime_error(msg);
  }

  /* The message of a fit OALGO 2 or 4 cannot make as LAPACK finds it: what
   * `e` found, the fit's data rows (`nonzero` of them not zero), weights
   * and LAMBDA, and the remedies for its LAMBDA and `n_bias` intercept
   * columns. */
  [[noreturn]] static void throw_rank_deficient(int oalgo,
                                                const RankDeficientError &e,
                                                const tadah::core::phi_type &Phi,
                                                const Layout &L,
                                                double lambda, size_t nonzero,
                                                size_t n_bias) {
    const size_t n = Phi.cols();
    const bool pins = L.pinned(n) > 0;
    const size_t c = static_cast<size_t>(e.column());
    const std::string fit = data_rows_text(nonzero, L) + " for " +
                            plural(n, "weight") + ", LAMBDA " +
                            tadah::core::format_roundtrip(lambda);
    std::string msg;
    if (oalgo == 2) {
      msg = oalgo_name(2) + " cannot fit " + weights_text(n, L) +
            ": the design matrix (" + fit +
            ") does not have full column rank: its column " +
            std::to_string(c) +
            (pins ? " (weight " + std::to_string(weight_of(c, L)) + ")"
                  : std::string()) +
            " is zero or a linear combination of the columns before it, so "
            "the least-squares weights are not unique.";
    } else {
      msg = oalgo_name(4) + " cannot fit " + weights_text(n, L) +
            ": the matrix Phi^T Phi of the design matrix (" + fit +
            ") is not positive definite (its leading minor of order " +
            std::to_string(c) + " is not" +
            (pins ? "; column " + std::to_string(c) + " fits weight " +
                        std::to_string(weight_of(c, L))
                  : std::string()) +
            "), so the design matrix does not have full column rank or is "
            "too ill-conditioned for the normal equations.";
    }
    throw std::runtime_error(msg + remedies(oalgo, lambda,
                                            std::min(n_bias, n)));
  }

public:
  /**
   * @brief Refuses a LAMBDA that requests nothing: a second value, and a
   *        negative value other than -1.
   *
   * LAMBDA takes one value: 0 fits without a penalty, a positive value is
   * the ridge penalty, and -1 requests the evidence approximation (OALGO
   * 3; the caller checks its OALGO). A second value was documented as the
   * rank threshold but was never read: the threshold of OALGO 1 and 3 is
   * OALGO's second value. Every fit calls this first (train(),
   * train_with_factor(), M_KRR_Train::train2()). The messages give the
   * values as @p context holds them.
   *
   * @throws std::runtime_error naming the values and what they mean.
   */
  static void require_lambda_request(const tadah::core::Context &context) {
    const double lambda = context.get<double>("LAMBDA", 0);  // the default if absent
    const std::vector<std::string> &given = context("LAMBDA");
    std::string text = "LAMBDA";
    for (const std::string &v : given) text += " " + v;
    if (given.size() > 1)
      throw std::runtime_error(
          text + ": LAMBDA takes one value, the ridge penalty (0 for none, "
          "-1 for the evidence approximation of OALGO 3); a second value is "
          "not read. The rank threshold of OALGO 1 and 3 is OALGO's second "
          "value (OALGO 1 " + given[1] + ", OALGO 3 " + given[1] + ").");
    if (lambda < 0 && std::abs(lambda + 1) >= std::numeric_limits<double>::min())
      throw std::runtime_error(
          text + ": only LAMBDA -1 requests the evidence approximation, "
          "which OALGO 3 runs; another negative value requests nothing. "
          "LAMBDA 0 fits without a penalty, and a positive LAMBDA is the "
          "ridge penalty.");
  }

  /**
   * @brief Refuses an OALGO that requests nothing: a first value other
   *        than 1 to 4, and a second value no solve reads as it is meant.
   *
   * OALGO's first value selects the solve: 1 (DGELSD), 2 (DGELS, QR), 3
   * (the SVD solve) or 4 (the normal equations, Cholesky). Its second
   * value M is the rank threshold of OALGO 1 and 3 (1e-8 when it is left
   * out): a singular value at most M times the largest is left out of the
   * coefficients for 0 < M < 1. Below 0 OALGO 1 takes the machine
   * precision of LAPACK (DGELSD's rule, OALGO 1 -1), and at 0 or below
   * OALGO 3 leaves out only a singular value of 0. Refused: under OALGO 1
   * an M of 0 or of 1 or more (DGELSD replaces either by the machine
   * precision), under OALGO 3 an M of 1 or more (it leaves out every
   * direction: every coefficient 0), and any second value under OALGO 2
   * and 4, which keep every direction.
   * Every fit calls this first (require_lambda_for_oalgo(),
   * train_with_factor()). The messages give the line as @p context holds
   * it.
   *
   * @throws std::runtime_error naming the line and the values it takes.
   */
  static void require_oalgo_request(const tadah::core::Context &context) {
    const std::vector<std::string> &given = context("OALGO");
    std::string text = "OALGO";
    for (const std::string &v : given) text += " " + v;
    const int oalgo = context.get<int>("OALGO", 0);
    if (oalgo < 1 || oalgo > 4)
      throw std::runtime_error(
          "OALGO " + std::to_string(oalgo) +
          ": OALGO is 1 (DGELSD), 2 (DGELS, QR), 3 (the SVD solve) or 4 "
          "(normal equations, Cholesky).");
    if (given.size() < 2) return;
    if (oalgo == 2 || oalgo == 4)
      throw std::runtime_error(
          text + ": " + oalgo_name(oalgo) +
          " takes one value: it keeps every direction and has no rank "
          "threshold, the second value of OALGO 1 and 3.");
    // A value that is not a finite number the Context refuses to read.
    const double M = context.get<double>("OALGO", 1);
    if (oalgo == 1 && (M == 0.0 || M >= 1.0))
      throw std::runtime_error(
          text + ": the rank threshold M of OALGO 1 (DGELSD), its second "
          "value, is a number 0 < M < 1 (a singular value at most M times "
          "the largest is left out of the coefficients) or below 0 for the "
          "machine precision of LAPACK, 2^-53 (OALGO 1 -1); DGELSD takes the "
          "machine precision for an M of 0 or of 1 or more.");
    if (oalgo == 3 && M >= 1.0)
      throw std::runtime_error(
          text + ": the rank threshold M of OALGO 3, its second value, is a "
          "number 0 < M < 1 (a singular value at most M times the largest is "
          "left out of the coefficients) or 0 or below (only a singular "
          "value of 0 is left out); an M of 1 or more leaves out every "
          "singular direction, every coefficient 0.");
  }

  /** The OALGO and LAMBDA rules of train(), checked on the configuration
   *  alone, with its messages: require_oalgo_request(),
   *  require_lambda_request(), then LAMBDA -1 under an OALGO other than
   *  3, which alone runs the evidence approximation. tadah hpo applies
   *  them when a run starts, so that no evaluation is refused for them.
   *  @throws std::runtime_error naming the value (and the OALGO). */
  static void require_lambda_for_oalgo(const tadah::core::Context &context) {
    require_oalgo_request(context);
    require_lambda_request(context);
    const double lambda = context.get<double>("LAMBDA", 0);
    const int oalgo = context.get<int>("OALGO", 0);
    if (std::abs(lambda + 1) < std::numeric_limits<double>::min() &&
        oalgo != 3) {
      throw std::runtime_error(
          "LAMBDA -1 requests the evidence approximation, which only OALGO 3 "
          "(the SVD solve) runs; the configuration has OALGO " +
          std::to_string(oalgo) + " (LAMBDA >= 0 fits with any OALGO).");
    }
  }

  /** The intercept rule of OALGO 3, checked on the configuration and the
   *  number of intercept columns the fit has, with train()'s messages:
   *  OALGO 3 penalises every coefficient alike, while the ridge of OALGO 1,
   *  2 and 4 leaves the intercept columns unpenalised, so with
   *  @p n_intercepts > 0 it refuses a LAMBDA
   *  other than 0. LAMBDA -1 requests the evidence approximation, which
   *  only OALGO 3 runs: its message says what it needs (BIAS false, or
   *  every intercept coefficient fixed with FIXINDEX and FIXWEIGHT) and what
   *  fits with BIAS true. @p n_intercepts counts the intercept columns the
   *  fit fits (those FIXINDEX pins leave the design matrix). train() calls
   *  it; tadah hpo applies it when a run starts (MLIP
   *  require_oalgo3_intercepts counts the columns of a configuration).
   *  @throws std::runtime_error naming the number of intercept columns. */
  static void require_oalgo3_intercepts(const tadah::core::Context &context,
                                        size_t n_intercepts) {
    if (n_intercepts == 0 || context.get<int>("OALGO", 0) != 3) return;
    const double lambda = context.get<double>("LAMBDA", 0);
    if (lambda == 0.0) return;
    const std::string n = std::to_string(n_intercepts);
    if (std::abs(lambda + 1) < std::numeric_limits<double>::min())
      throw std::runtime_error(
          "LAMBDA -1 requests the evidence approximation, which only OALGO 3 "
          "runs; OALGO 3 penalises every coefficient alike, and the model "
          "has " + n + " intercept column(s) (BIAS true with ATOM), which the "
          "ridge of OALGO 1, 2 and 4 leaves unpenalised and OALGO 3 cannot. "
          "The evidence approximation needs BIAS "
          "false, or every intercept coefficient fixed with FIXINDEX and "
          "FIXWEIGHT. "
          "With BIAS true, LAMBDA 0 fits with any OALGO, and LAMBDA > 0 with "
          "OALGO 1, 2 or 4.");
    throw std::runtime_error(
        "OALGO 3 (SVD with a uniform ridge) penalises every coefficient "
        "alike, and the model has " + n + " intercept column(s) (BIAS true "
        "with ATOM), which the ridge of OALGO 1, 2 and 4 leaves unpenalised "
        "and OALGO 3 cannot, so OALGO 3 takes LAMBDA 0 only; OALGO 1, 2 or 4 "
        "fits such a model with a LAMBDA other than 0.");
  }

private:
  /**
   * @brief Train the model using the provided configuration and data matrices.
   *
   * This method supports both ordinary least squares and ridge regression based
   * on the lambda value.
   *
   * @param context Context object containing training parameters.
   * @param Phi Design matrix.
   * @param T Target matrix.
   * @param weights Vector to store the resultant weights.
   * @param workspaceManager Workspace provider.
   * @param intercepts The intercept columns of Phi, 0-based indices into
   *        its columns (Function_Base::intercept_columns() of the function
   *        that built Phi, mapped to Phi's columns when FIXINDEX removed
   *        some): the LAMBDA ridge penalty leaves them out and penalises
   *        every other column, and OALGO 3, whose ridge penalises every
   *        column alike, refuses LAMBDA != 0 when there is one.
   * @param n_obs The number of observations: the data rows of Phi of
   *        priority above 0 that are not zero in every column
   *        (zero_rows(): no value above 1e-10 of the largest magnitude of
   *        its column), without its
   *        ridge-augmentation rows (the DesignMatrix appends one per
   *        column) and without the rows of priority 0 (zero rows of the
   *        weighted matrix). LAMBDA -1 (OALGO 3) estimates the evidence
   *        with it.
   * @param layout Phi's data rows and the weight each column fits
   *        (Layout::of(Phi) for DesignMatrix's layout, Layout::fixed()
   *        after FIXINDEX removed columns): the checks and messages count
   *        them.
   * Nothing is written into @p context but the ALPHA and BETA of LAMBDA
   * -1. With OALGO 3, the singular directions the solve leaves out of the
   * weights (dropped_directions()) are named in a WARNING.
   * @throws std::logic_error, before anything is written into @p Phi,
   *         for a @p layout that does not describe it (fewer rows than
   *         its data rows and one ridge row per column, or another number
   *         of column weights) or an intercept column past its columns.
   * @throws std::runtime_error for a value of Phi or T that is not finite
   *         (every OALGO), for coefficients of the solve that are not
   *         finite (require_finite_weights(): the solve overflowed; under
   *         OALGO 3 at LAMBDA 0 the message names a kept singular value
   *         whose square underflows to 0, when one is the cause), and
   *         under OALGO 2 and 4 for a design matrix
   *         whose non-zero data rows (a data row that is zero in every
   *         column, such as a row of priority 0, adds nothing to the rank)
   *         and, with LAMBDA > 0, ridge rows cannot give it full column
   *         rank, before LAPACK runs, or whose QR or Cholesky factorisation
   *         LAPACK cannot complete; each names the rows (and how many are
   *         not zero), the weights and LAMBDA, and the remedies that apply
   *         to its LAMBDA and intercept columns. A design matrix whose
   *         columns are dependent but not exactly so in floating point can
   *         still be fitted by OALGO 2 and 4, with weights as large as the
   *         rounding makes them. Also, before anything is solved, for a
   *         LAMBDA that requests nothing (require_lambda_request(): a
   *         second value, a negative value other than -1) and for LAMBDA
   *         -1 under another OALGO than 3; and for an evidence
   *         approximation whose alpha or beta is not finite (EA::run(),
   *         before its lambda is solved with). An evidence
   *         approximation that stops at its iteration limit is not refused:
   *         a WARNING says so (EA::stopped_at_limit_text()).
   */
public:
  static void train(tadah::core::Context &context,
                    tadah::core::phi_type &Phi,
                    tadah::core::t_type &T,
                    tadah::core::t_type &weights,
                    tadah::models::memory::IModelsWorkspaceManager &workspaceManager,
                    const std::vector<size_t> &intercepts,
                    std::size_t n_obs,
                    const Layout &layout) {

    require_lambda_for_oalgo(context);
    double lambda = context.get<double>("LAMBDA", 0);
    int oalgo = context.get<int>("OALGO", 0);
    double rcond =
        context.size("OALGO") == 2 ? context.get<double>("OALGO", 1) : 1e-8;
    weights.resize(Phi.cols());

    // The caller's layout and intercept columns are checked before
    // anything is written into Phi: the ridge rows below are at
    // Phi.rows() - Phi.cols() + j, which wraps for a Phi without room for
    // them.
    require_layout(Phi, layout);
    const size_t n_bias = intercepts.size();
    for (size_t j : intercepts)
      if (j >= Phi.cols())
        throw std::logic_error(
            "LinearRegressor::train: intercept column " + std::to_string(j) +
            " of a design matrix of " + std::to_string(Phi.cols()) +
            " columns");

    if (lambda > 0 && oalgo != 3 ) {
      // Augment the design matrix with the regularization term: row
      // aug_start + j penalises column j, except for the intercept columns,
      // which stay unpenalised (standard practice: regularising an
      // intercept biases the fit toward zero mean).
      const size_t aug_start = Phi.rows() - Phi.cols();
      std::vector<bool> intercept(Phi.cols(), false);
      for (size_t j : intercepts) intercept[j] = true;
      for (size_t j = 0; j < Phi.cols(); ++j)
        if (!intercept[j]) Phi(aug_start + j, j) = std::sqrt(lambda);
    }

    tadah::models::OLS ols(workspaceManager);
    // tadah::models::OLS ols;
    require_finite(Phi, T, layout);
    // The non-zero data rows, counted before LAPACK overwrites Phi.
    size_t nonzero = layout.data_rows;
    if (oalgo == 2 || oalgo == 4) {
      nonzero = nonzero_data_rows(Phi, layout);
      require_enough_rows(oalgo, Phi, layout, lambda, n_bias, nonzero);
    }
    if (oalgo == 1) {
      ols.solve(Phi, T, weights, rcond, tadah::models::Algorithm::GELSD);
      require_finite_weights(context, weights, layout);
    } else if (oalgo == 2) {
      try {
        ols.solve(Phi, T, weights, rcond, tadah::models::Algorithm::GELS);
      } catch (const RankDeficientError &e) {
        throw_rank_deficient(2, e, Phi, layout, lambda, nonzero, n_bias);
      }
      require_finite_weights(context, weights, layout);
    } else if (oalgo == 3) {
      // The SVD path applies a uniform ridge across all singular values;
      // there is no per-column penalty, so it cannot leave the intercept
      // columns out of it (require_oalgo3_intercepts).
      require_oalgo3_intercepts(context, n_bias);
      tadah::models::SVD svd;
      svd.compute(Phi);  // Phi is overwritten: the residuals come from the SVD
      if (lambda < 0) {
        double alpha = context.get<double>("ALPHA");
        double beta = context.get<double>("BETA");
        EA ea(context, svd, T, n_obs);
        if (!ea.run(alpha, beta))
          fit_warning(FitWarningKind::EvidenceApproximationLimit,
                      ea.stopped_at_limit_text());
        lambda = alpha / beta;
        context.remove("ALPHA");
        context.remove("BETA");
        context.add("ALPHA", alpha);
        context.add("BETA", beta);
      }

      core::Logger::instance().log(tadah::core::LogLevel::INFO,
          "LinearRegressor::train -> Regularization lambda: ", lambda);

      ols.solveWithPseudoinverse(Phi.rows(), Phi.cols(), T, weights,
                                 svd.getSVDWorkspace(), rcond, lambda);
      require_finite_weights(context, weights, layout, &svd, rcond, lambda);
      const std::size_t n_dropped = dropped_directions(svd, rcond);
      // The threshold as OLS::pseudoinverse_threshold sets it: rcond
      // times the largest singular value, 0 for an rcond of 0 or less.
      // A fit WARNING: a caller that fits many candidates (tadah hpo)
      // collects it and says it once, tadah train prints it.
      if (n_dropped > 0)
        fit_warning(FitWarningKind::SingularDirectionsDropped,
            "OALGO 3: " + std::to_string(n_dropped) +
            " of " + std::to_string(Phi.cols()) + " singular directions of "
            "the design matrix are at or below the rank threshold " +
            (rcond > 0.0
                 ? "(OALGO's rcond " + core::to_string(rcond) +
                       " times the largest singular value)"
                 : "(0, as OALGO's rcond " + core::to_string(rcond) +
                       " is not above 0)") +
            ": the solve leaves them out of the coefficients.");
    } else if (oalgo == 4) {
      try {
        ols.solve(Phi, T, weights, rcond,
                  tadah::models::Algorithm::NormalEquations);
      } catch (const RankDeficientError &e) {
        throw_rank_deficient(4, e, Phi, layout, lambda, nonzero, n_bias);
      }
      require_finite_weights(context, weights, layout);
    } else {
      throw std::runtime_error("Unsupported OALGO: " + std::to_string(oalgo));
    }
  }
  /** @brief train() of a design matrix laid out as DesignMatrix lays it
   *  out (Layout::of(Phi): its data rows, then one ridge row per column)
   *  with no rows of priority 0: n_obs = rows - cols (0 when Phi has no more
   *  rows than columns). A design matrix without some of its columns
   *  (FIXINDEX) takes the full overload, with Layout::fixed(). */
  static void train(tadah::core::Context &context,
                    tadah::core::phi_type &Phi,
                    tadah::core::t_type &T,
                    tadah::core::t_type &weights,
                    tadah::models::memory::IModelsWorkspaceManager &workspaceManager,
                    const std::vector<size_t> &intercepts) {
    train(context, Phi, T, weights, workspaceManager, intercepts,
          augmented_observations(Phi), Layout::of(Phi));
  }
  /** @brief train() of a linear model on a design matrix laid out as
   *  DesignMatrix lays it out (Layout::of(Phi)): its intercept columns are
   *  the intercept columns of the descriptor, 0 .. N-1 with N =
   *  tadah::core::n_intercept_columns() (Function_Base::intercept_columns()
   *  of BF_Linear and Kern_Linear); n_obs = rows - cols. */
  static void train(tadah::core::Context &context,
                    tadah::core::phi_type &Phi,
                    tadah::core::t_type &T,
                    tadah::core::t_type &weights,
                    tadah::models::memory::IModelsWorkspaceManager &workspaceManager) {
    std::vector<size_t> intercepts(tadah::core::n_intercept_columns(context));
    for (size_t k = 0; k < intercepts.size(); ++k) intercepts[k] = k;
    train(context, Phi, T, weights, workspaceManager, intercepts);
  }
  /** @brief train() of a linear model with a temporary workspace manager. */
  static void train(tadah::core::Context &context, tadah::core::phi_type &Phi,
                    tadah::core::t_type &T, tadah::core::t_type &weights) {
    tadah::models::memory::ModelsWorkspaceManager workspaceManager;
    train(context, Phi, T, weights, workspaceManager);
  }

  /** @brief The observations of a design matrix with its cols
   *  ridge-augmentation rows (the DesignMatrix contract, which the LAMBDA
   *  ridge of OALGO 1 and 2 writes into): rows - cols, or 0 when Phi has
   *  no more rows than columns. A caller that knows the rows of priority 0
   *  passes their exclusion to train() itself. */
  static std::size_t augmented_observations(const tadah::core::phi_type &Phi) {
    return Phi.rows() > Phi.cols() ? Phi.rows() - Phi.cols() : 0;
  }

  /**
   * @brief Train + cache the Cholesky factor for IFT-based gradients.
   *
   * Mirrors @ref train but additionally exposes the cached
   * factorisation through @p factor for HPO analytical/hybrid
   * gradients to consume via @ref backsolve. Restricted to OALGO=4
   * (NormalEquations) with LAMBDA >= 0 — the supported configuration
   * for IFT.
   *
   * The forward path of @ref train is byte-identical and unaffected by
   * this overload; HPO callers that want analytical gradients must
   * opt in by calling this method instead.
   *
   * @param context     Training parameters; must declare OALGO=4.
   * @param Phi         Design matrix (m+n rows; ridge-augmentation
   *                    block written in place if LAMBDA>0).
   * @param T           Target vector.
   * @param weights     Receives the OLS solution.
   * @param factor      Output: receives the cached Cholesky factor.
   * @param workspaceManager  Workspace provider.
   * @throws std::runtime_error, before the factorisation, for what train()
   *         refuses under OALGO 4 before LAPACK runs: a LAMBDA that
   *         requests nothing (require_lambda_request()), a value of Phi or
   *         T that is not finite, and fewer non-zero data rows than weights
   *         (LAMBDA 0) or than intercept columns (LAMBDA > 0).
   * @throws std::logic_error (an internal error of the caller: the fit
   *         serves an analytical gradient that reuses its factor, which a
   *         caller asks for with OALGO 4 and a fixed LAMBDA) for an OALGO
   *         other than 4 and for LAMBDA -1.
   */
  static void train_with_factor(
      tadah::core::Context &context,
      tadah::core::phi_type &Phi,
      tadah::core::t_type &T,
      tadah::core::t_type &weights,
      FactorHandle &factor,
      tadah::models::memory::IModelsWorkspaceManager &workspaceManager) {

    require_oalgo_request(context);
    require_lambda_request(context);
    const double lambda = context.get<double>("LAMBDA", 0);
    const int    oalgo  = context.get<int>("OALGO", 0);
    const double rcond  =
        context.size("OALGO") == 2 ? context.get<double>("OALGO", 1) : 1e-8;
    (void)rcond;  // NormalEquations does not consume rcond.

    // A fit for an analytical gradient that reuses its factor, which a
    // caller asks for with OALGO 4 and a fixed LAMBDA (tadah hpo builds its
    // own factor and does not call it): internal errors of the caller.
    if (oalgo != 4) {
      throw std::logic_error(
          "internal error: the analytical gradient fits with OALGO 4 (the "
          "Cholesky normal equations), whose factor it reuses, and was "
          "given OALGO " + std::to_string(oalgo));
    }
    if (std::abs(lambda + 1) < std::numeric_limits<double>::min()) {
      throw std::logic_error(
          "internal error: the analytical gradient fits with a fixed LAMBDA "
          ">= 0, and was given LAMBDA -1 (the evidence approximation, which "
          "changes the fit inside the solve)");
    }
    weights.resize(Phi.cols());

    const std::size_t n_bias = tadah::core::n_intercept_columns(context);

    // DesignMatrix's layout, checked before the ridge rows are written (at
    // Phi.rows() - Phi.cols() + j, which wraps for a Phi without room for
    // them), as train() checks it.
    const Layout layout = Layout::of(Phi);
    require_layout(Phi, layout);

    // Fill the ridge-augmentation block exactly as the existing
    // train() does — same byte pattern so a parity test is meaningful.
    if (lambda > 0) {
      const std::size_t aug_start = Phi.rows() - Phi.cols();
      std::size_t j = n_bias;
      for (std::size_t i = aug_start + n_bias; i < Phi.rows(); ++i) {
        Phi(i, j++) = std::sqrt(lambda);
      }
    }

    require_finite(Phi, T, layout);
    require_enough_rows(4, Phi, layout, lambda, n_bias,
                        nonzero_data_rows(Phi, layout));
    tadah::models::OLS ols(workspaceManager);
    ols.solveAndCacheFactor(Phi, T, weights, factor, n_bias);
    require_finite_weights(context, weights, layout);
  }

  /// Convenience overload using a temporary workspace manager.
  static void train_with_factor(
      tadah::core::Context &context,
      tadah::core::phi_type &Phi,
      tadah::core::t_type &T,
      tadah::core::t_type &weights,
      FactorHandle &factor) {
    tadah::models::memory::ModelsWorkspaceManager workspaceManager;
    train_with_factor(context, Phi, T, weights, factor, workspaceManager);
  }

  /**
   * @brief Solve U^T U x = rhs using the cached Cholesky factor.
   *
   * Used by HPO analytical/hybrid gradients to compute dW_star/dtheta
   * via the implicit-function-theorem chain rule. With fixed lambda:
   *
   *     (Phi^T Phi + lambda I_no_bias) . dW_star/dtheta
   *         = (dPhi^T/dtheta)(T - Phi W_star)
   *           - Phi^T (dPhi/dtheta) W_star
   *
   * One back-sub per theta-partial; the factor is shared across all
   * calls within a single HPO gradient evaluation.
   *
   * @param f    Cached factor (must be valid and NormalEquations).
   * @param rhs  Right-hand side; size must equal f.n.
   * @param sol  Output; resized to f.n if needed.
   */
  static void backsolve(const FactorHandle &f,
                        const tadah::core::t_type &rhs,
                        tadah::core::t_type &sol) {
    if (!f.valid) {
      throw std::logic_error(
          "internal error: the factor of the normal equations of a "
          "back-substitution is not valid: it was not computed, or the "
          "descriptor size changed after it was");
    }
    if (f.algo != tadah::models::Algorithm::NormalEquations) {
      throw std::runtime_error(
          "LinearRegressor::backsolve: only NormalEquations factors are "
          "supported; got algo " +
          tadah::models::to_string_algorithm(f.algo));
    }
    if (rhs.size() != static_cast<std::size_t>(f.n)) {
      throw std::invalid_argument(
          "LinearRegressor::backsolve: rhs size (" +
          std::to_string(rhs.size()) + ") does not match factor "
          "dimension (" + std::to_string(f.n) + ").");
    }

    // dpotrs_ overwrites the rhs with the solution, so copy first.
    sol.resize(static_cast<std::size_t>(f.n));
    for (int i = 0; i < f.n; ++i) sol[i] = rhs[i];

    // dpotrs_ takes a non-const factor pointer for LAPACK signature
    // compatibility but does not modify it. Same const-cast pattern as
    // the existing OLS::solveImpl call site.
    char uplo = 'U';
    int  n    = f.n;
    int  nrhs = 1;
    int  info = 0;
    double *factor_ptr = const_cast<double *>(f.AtA.data());
    tadah::core::dpotrs_(&uplo, &n, &nrhs, factor_ptr, &n,
                         sol.ptr(), &n, &info);
    if (info != 0) {
      throw std::runtime_error(
          "LinearRegressor::backsolve: DPOTRS failed; info = " +
          std::to_string(info));
    }
  }

  /**
   * @brief The directions of the weights the SVD solve of @p svd leaves
   *        out: the singular values at or below
   *        OLS::pseudoinverse_threshold(S, min(m,n), @p rcond), the rule
   *        solveWithPseudoinverse leaves them out of the weights with, and
   *        the n - min(m,n) directions of a matrix with fewer rows than
   *        columns (m x n, the design matrix of @p svd).
   */
  static std::size_t dropped_directions(tadah::models::SVD &svd,
                                        double rcond) {
    const double *S = svd.getS();
    const int n = svd.cols();
    const int k = std::min(svd.rows(), n);
    const double thresh = OLS::pseudoinverse_threshold(S, k, rcond);
    int kept = 0;
    for (int i = 0; i < k; ++i)
      if (S[i] > thresh) ++kept;
    return static_cast<std::size_t>(n - kept);
  }
};
} // namespace models
} // namespace tadah
#endif
