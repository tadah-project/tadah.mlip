// test_oalgo3_fit.cpp
//
// The OALGO 3 fit (the SVD solve) and what it logs.
//
// A guard of the OALGO 3 fit (the SVD solve) of M_BLR_Train and
// M_KRR_Train: its weights, and the ALPHA and BETA of the evidence
// approximation (LAMBDA -1), are those of the solve alone, bit for bit:
//
//     SVD of the design matrix; with LAMBDA -1, EA::run from the Context's
//     ALPHA and BETA and lambda = alpha / beta; the weights of
//     OLS::solveWithPseudoinverse(rows, cols, T, w, svd, rcond, lambda),
//
// computed here with those classes on a copy of the design matrix (the
// free columns of a fit with FIXINDEX, and T minus the pinned columns times
// their FIXWEIGHT, as the model forms them). Nothing else the fit computes
// may change them: a fit with LAMBDA -1, LAMBDA > 0, LAMBDA 0 with a
// singular direction the solve drops (rcond), and FIXINDEX.
//
// The fit logs a WARNING for the singular directions the solve leaves out
// of the weights, and nothing about an uncertainty of the weights: no fit
// computes one.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/logger.h>
#include <tadah/models/ea.h>
#include <tadah/models/fit_warnings.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/models/ols.h>
#include <tadah/models/svd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
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

/* A deterministic m x n design matrix as the DesignMatrix gives it to a
 * fit: m data rows (entries in [-0.5, 0.5) plus 2 on the diagonal; with
 * @p repeat_last, the last column a copy of the one before it, a singular
 * direction) and one zero ridge-augmentation row per column. */
phi_type design(size_t m, size_t n, double seed, bool repeat_last) {
  phi_type Phi(m + n, n);
  Phi.set_zero();
  double v = seed;
  for (size_t j = 0; j < n; ++j)
    for (size_t i = 0; i < m; ++i) {
      v = std::fmod(v * 1.618 + 0.31 * static_cast<double>(j + 1), 1.0) - 0.5;
      Phi(i, j) = v + (i == j ? 2.0 : 0.0);
    }
  if (repeat_last)
    for (size_t i = 0; i < m; ++i) Phi(i, n - 1) = Phi(i, n - 2);
  return Phi;
}

/* Targets of the m data rows (zero on the augmentation rows): a linear
 * function of the columns plus a perturbation. */
t_type targets(const phi_type &Phi, size_t m) {
  t_type T(Phi.rows());
  for (size_t i = 0; i < Phi.rows(); ++i) {
    double t = 0.0;
    if (i < m) {
      for (size_t j = 0; j < Phi.cols(); ++j)
        t += Phi(i, j) * 0.5 * static_cast<double>(j + 1) * (j % 2 ? -1 : 1);
      t += 0.05 * std::sin(1.7 * static_cast<double>(i) + 0.3);
    }
    T[i] = t;
  }
  return T;
}

/* Captures std::cout (where the Logger prints) at WARNING level and
 * restores the stream and the level afterwards. */
class LogCapture {
public:
  LogCapture()
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())) {
    Logger::instance().setLogLevel(LogLevel::WARNING);
  }
  ~LogCapture() {
    std::cout.rdbuf(old_out_);
    Logger::instance().setLogLevel(old_level_);
  }
  LogCapture(const LogCapture &) = delete;
  LogCapture &operator=(const LogCapture &) = delete;
  std::string text() const { return out_.str(); }

private:
  std::ostringstream out_;
  LogLevel old_level_;
  std::streambuf *old_out_;
};

Context oalgo3(const std::string &lambda, const std::string &rcond = "") {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", lambda);
  c.add("OALGO", rcond.empty() ? std::string("3") : "3 " + rcond);
  return c;
}

/* The solve alone on @p Phi (copied) and @p T, with @p n_obs
 * observations, as the context @p c asks: the weights, and alpha and beta
 * of LAMBDA -1. */
struct Solve {
  t_type w;
  double alpha = 0.0;
  double beta = 0.0;
};
Solve solve(const Context &c, phi_type Phi, t_type T, size_t n_obs) {
  Solve s;
  double lambda = c.get<double>("LAMBDA", 0);
  const double rcond =
      c.size("OALGO") == 2 ? c.get<double>("OALGO", 1) : 1e-8;
  SVD svd;
  svd.compute(Phi);
  if (lambda < 0) {
    s.alpha = c.get<double>("ALPHA");
    s.beta = c.get<double>("BETA");
    EA ea(c, svd, T, n_obs);
    ea.run(s.alpha, s.beta);
    lambda = s.alpha / s.beta;
  }
  tadah::models::memory::ModelsWorkspaceManager wsm;
  OLS ols(wsm);
  s.w.resize(Phi.cols());
  ols.solveWithPseudoinverse(static_cast<int>(Phi.rows()),
                             static_cast<int>(Phi.cols()), T, s.w,
                             svd.getSVDWorkspace(), rcond, lambda);
  return s;
}

/* The design matrix of the free columns (all but the 0-based @p pinned,
 * in their order) and the targets minus the pinned columns times
 * @p values: the regression of a fit with FIXINDEX. */
void free_regression(const phi_type &Phi, const t_type &T,
                     const std::vector<size_t> &pinned,
                     const std::vector<double> &values, phi_type &Phi_f,
                     t_type &T_r, std::vector<size_t> &free_cols) {
  T_r = T;
  for (size_t k = 0; k < pinned.size(); ++k)
    for (size_t i = 0; i < Phi.rows(); ++i)
      T_r[i] -= Phi(i, pinned[k]) * values[k];
  free_cols.clear();
  for (size_t j = 0; j < Phi.cols(); ++j)
    if (std::find(pinned.begin(), pinned.end(), j) == pinned.end())
      free_cols.push_back(j);
  Phi_f = phi_type(Phi.rows(), free_cols.size());
  for (size_t a = 0; a < free_cols.size(); ++a)
    for (size_t i = 0; i < Phi.rows(); ++i) Phi_f(i, a) = Phi(i, free_cols[a]);
}

/* The model @p M (a BF_Linear M_BLR_Train or a Kern_Linear M_KRR_Train)
 * fitted to @p Phi and @p T with @p n_obs observations under @p c: its
 * weights and the Context's ALPHA and BETA. */
template <class M, class F>
Solve model_fit(Context c, const phi_type &Phi, const t_type &T,
                size_t n_obs) {
  F f;
  M m(f, c);
  phi_type P = Phi;
  t_type t = T;
  m.train(P, t, n_obs);
  Solve s;
  s.w = m.get_weights();
  if (c.get<double>("LAMBDA") < 0) {
    s.alpha = c.get<double>("ALPHA");
    s.beta = c.get<double>("BETA");
  }
  return s;
}

void check_bits(const Solve &got, const Solve &ref, bool ea) {
  REQUIRE(got.w.size() == ref.w.size());
  for (size_t j = 0; j < ref.w.size(); ++j) {
    INFO("weight " << j << ": " << got.w[j] << " against " << ref.w[j]);
    CHECK(bits(got.w[j]) == bits(ref.w[j]));
  }
  if (ea) {
    INFO("alpha " << got.alpha << " against " << ref.alpha << ", beta "
                  << got.beta << " against " << ref.beta);
    CHECK(bits(got.alpha) == bits(ref.alpha));
    CHECK(bits(got.beta) == bits(ref.beta));
  }
}

}  // namespace

/* Pins (a guard): the weights of an OALGO 3 fit of M_BLR_Train<BF_Linear>
 * and M_KRR_Train<Kern_Linear> on a 9 x 4 design matrix (9 observations,
 * 4 augmentation rows), and with LAMBDA -1 the ALPHA and BETA the fit
 * writes into the Context, equal bit for bit those of the solve alone
 * (SVD, EA, solveWithPseudoinverse) for LAMBDA -1, LAMBDA 1e-3, and LAMBDA
 * 0 with OALGO 3 1e-6 on a matrix whose last two columns are equal (one
 * direction dropped).
 * Before the fix: passes as well (a guard: nothing but the solve sets the
 * weights); a perturbation of lambda by one part in 10^15 inside
 * LinearRegressor::train makes it fail. */
TEST_CASE("OALGO 3: the weights and the evidence approximation are those "
          "of the solve alone",
          "[oalgo3][guard]") {
  struct Case {
    const char *name;
    const char *lambda;
    const char *rcond;
    bool singular;
  };
  for (const Case &k : {Case{"LAMBDA -1", "-1", "", false},
                        Case{"LAMBDA 1e-3", "1e-3", "", false},
                        Case{"LAMBDA 0, one direction dropped", "0", "1e-6",
                             true}}) {
    DYNAMIC_SECTION(k.name) {
      const phi_type Phi = design(9, 4, 0.37, k.singular);
      const t_type T = targets(Phi, 9);
      const Context c = oalgo3(k.lambda, k.rcond);
      const bool ea = std::string(k.lambda) == "-1";
      const Solve ref = solve(c, Phi, T, 9);
      check_bits(model_fit<M_BLR_Train<BF_Linear>, BF_Linear>(c, Phi, T, 9),
                 ref, ea);
      check_bits(
          model_fit<M_KRR_Train<Kern_Linear>, Kern_Linear>(c, Phi, T, 9),
          ref, ea);
    }
  }
}

/* Pins (a guard): with FIXINDEX 2 4 and FIXWEIGHT 0.7 -0.3 (1-based) on a
 * 9 x 5 design matrix, the OALGO 3 fit of M_BLR_Train<BF_Linear> and
 * M_KRR_Train<Kern_Linear> with LAMBDA -1 and with LAMBDA 1e-3 has the
 * pinned values at weights 2 and 4 and, at weights 1, 3 and 5, those of the
 * solve alone of the free columns against T minus the pinned columns
 * times their values, bit for bit (and the ALPHA and BETA of LAMBDA -1).
 * Before the fix: passes as well (a guard). */
TEST_CASE("OALGO 3 with FIXINDEX: the free weights are those of the solve "
          "alone of the free columns",
          "[oalgo3][guard][fixed_weights]") {
  const phi_type Phi = design(9, 5, 0.59, false);
  const t_type T = targets(Phi, 9);
  const std::vector<size_t> pinned = {1, 3};
  const std::vector<double> values = {0.7, -0.3};
  phi_type Phi_f;
  t_type T_r;
  std::vector<size_t> free_cols;
  free_regression(Phi, T, pinned, values, Phi_f, T_r, free_cols);
  for (const char *lambda : {"-1", "1e-3"}) {
    DYNAMIC_SECTION("LAMBDA " << lambda) {
      Context c = oalgo3(lambda);
      c.add("FIXINDEX", "2 4");
      c.add("FIXWEIGHT", "0.7 -0.3");
      const bool ea = std::string(lambda) == "-1";
      const Solve free_fit = solve(c, Phi_f, T_r, 9);
      Solve ref;
      ref.alpha = free_fit.alpha;
      ref.beta = free_fit.beta;
      ref.w.resize(5);
      for (size_t a = 0; a < free_cols.size(); ++a)
        ref.w[free_cols[a]] = free_fit.w[a];
      for (size_t k = 0; k < pinned.size(); ++k) ref.w[pinned[k]] = values[k];
      check_bits(model_fit<M_BLR_Train<BF_Linear>, BF_Linear>(c, Phi, T, 9),
                 ref, ea);
      check_bits(
          model_fit<M_KRR_Train<Kern_Linear>, Kern_Linear>(c, Phi, T, 9),
          ref, ea);
    }
  }
}

/* Pins: an OALGO 3 fit (M_BLR_Train<BF_Linear>, LAMBDA 0, OALGO 3 1e-6) of
 * a 9 x 4 design matrix whose last two columns are equal logs one WARNING
 * for the direction the solve drops, about the weights alone and without a
 * class name: "OALGO 3: 1 of 4 singular directions of the design matrix
 * are at or below the rank threshold (OALGO's rcond 1e-06 times the
 * largest singular value): the solve leaves them out of the coefficients."
 * Before the fix: the WARNING went on "and the weight covariance WCOV
 * leaves them out too (no uncertainty along them)"; then it began
 * "LinearRegressor (OALGO 3): ", a class name, where it begins "OALGO 3:
 * ": the text check fails on that. */
TEST_CASE("OALGO 3: a direction the solve drops is a WARNING about the "
          "weights",
          "[oalgo3][uncertainty]") {
  const phi_type Phi = design(9, 4, 0.37, true);
  const t_type T = targets(Phi, 9);
  Context c = oalgo3("0", "1e-6");
  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  phi_type P = Phi;
  t_type t = T;
  std::string log;
  {
    LogCapture cap;
    m.train(P, t, 9);
    log = cap.text();
  }
  INFO(log);
  const std::string warning =
      "WARNING: OALGO 3: 1 of 4 singular directions of the design matrix "
      "are at or below the rank threshold (OALGO's rcond 1e-06 times the "
      "largest singular value): the solve leaves them out of the "
      "coefficients.\n";
  CHECK(log.size() >= warning.size());
  CHECK(log.compare(log.size() - warning.size(), warning.size(), warning) ==
        0);
  CHECK(log.find("WARNING") == log.rfind("WARNING"));
  CHECK(log.find("WCOV") == std::string::npos);
}

/* Pins: under a FitWarningCollector (tadah hpo makes one around the fits of
 * its evaluations and journals each kind of WARNING once per run), the
 * WARNING of the singular directions an OALGO 3 fit drops goes to the
 * collector, not the Logger: two such fits of the 9 x 4 design matrix
 * above collect two WARNINGs, each of a kind other than that of the
 * evidence approximation and with the text "OALGO 3: 1 of 4 singular
 * directions of the design matrix are at or below the rank threshold
 * (...)", and the Logger prints nothing. Without a collector each fit
 * prints it once (the case above), as tadah train needs.
 * Before the fix: the fit logged it to the Logger whatever the collector,
 * so a search printed it once per evaluation (75 lines in a run of 75
 * evaluations): the collector held nothing and the Logger printed two
 * WARNINGs. */
TEST_CASE("OALGO 3: a direction the solve drops is a fit WARNING that a "
          "collector takes",
          "[oalgo3][fit_warnings]") {
  const phi_type Phi = design(9, 4, 0.37, true);
  const t_type T = targets(Phi, 9);
  Context c = oalgo3("0", "1e-6");
  BF_Linear bf;
  std::string log;
  std::vector<FitWarning> collected;
  {
    LogCapture cap;
    FitWarningCollector collector;
    for (int fit = 0; fit < 2; ++fit) {
      M_BLR_Train<BF_Linear> m(bf, c);
      phi_type P = Phi;
      t_type t = T;
      m.train(P, t, 9);
    }
    collected = collector.warnings();
    log = cap.text();
  }
  INFO(log);
  CHECK(log.find("WARNING") == std::string::npos);
  REQUIRE(collected.size() == 2u);
  for (const FitWarning &w : collected) {
    CHECK(w.kind != FitWarningKind::EvidenceApproximationLimit);
    CHECK(w.text ==
          "OALGO 3: 1 of 4 singular directions of the design matrix are at "
          "or below the rank threshold (OALGO's rcond 1e-06 times the "
          "largest singular value): the solve leaves them out of the "
          "coefficients.");
  }
}

/* Pins: an OALGO 3 fit with no more observations than effective weights
 * (the 4 observations of a 4 x 4 design matrix of full rank at LAMBDA 0)
 * fits and logs no WARNING: the fit estimates no noise variance (no
 * uncertainty of the weights).
 * Before the fix: it logged "LinearRegressor (OALGO 3): no weight
 * covariance: the fit has 4 observations ... so its noise variance is
 * undefined: add data of priority above 0 ...". */
TEST_CASE("OALGO 3 with no more observations than weights: no WARNING",
          "[oalgo3][uncertainty]") {
  const phi_type Phi = design(4, 4, 0.47, false);
  const t_type T = targets(Phi, 4);
  Context c = oalgo3("0");
  BF_Linear bf;
  M_BLR_Train<BF_Linear> m(bf, c);
  phi_type P = Phi;
  t_type t = T;
  std::string log;
  {
    LogCapture cap;
    m.train(P, t, 4);
    log = cap.text();
  }
  INFO(log);
  CHECK(m.trained);
  CHECK(log.find("WARNING") == std::string::npos);
}
