// test_evidence_approximation_status.cpp
//
// The evidence approximation of LAMBDA -1 (OALGO 3) iterates alpha =
// gamma / |w|^2 and beta = (n_obs - gamma) / RSS to a fixed point. Two
// outcomes were not reported:
//
//  - alpha or beta not finite. Targets that are all zero give w = 0 and
//    RSS = 0, so alpha and beta are infinite, lambda = alpha / beta is nan
//    and so is every weight, which the fit returned. Such a fit is refused,
//    naming the value, the iteration and what made it.
//  - no convergence. The iteration stops after 41 steps; the fit then uses
//    the last alpha and beta. A WARNING says so, naming the limit and the
//    last relative changes of alpha and beta (the convergence needs both
//    below 5e-12).
//
//    A design matrix and targets whose scales are too far apart (a column
//    of 1e-300 fitted to a target of 1e10) give weights too small to square
//    (|w|^2 = 0): alpha is nan, refused too.
//
// The design matrices are laid out as DesignMatrix lays them out: 6 data
// rows of 2 columns, then 2 zero ridge rows (n_obs 6). The cases were found
// with an independent implementation of the iteration (numpy, the update
// rules above): `noisy` stops at the limit with alpha growing by a factor of
// about 2.5 per step (the evidence prefers ever smaller weights for these
// targets), `linear` converges in 6 steps.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/logger.h>
#include <tadah/models/ea.h>
#include <tadah/models/fit_warnings.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/models/svd.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;
using Catch::Matchers::Contains;
using Catch::Matchers::StartsWith;

namespace {

/* Whether @p v is finite, from its bits: the build may use -ffast-math,
 * under which std::isfinite can be folded to true. */
bool finite_bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return (u & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
}

/* Captures std::cout (where the Logger prints) at @p level, WARNING by
 * default, and restores the stream and the level afterwards. */
class LogCapture {
public:
  explicit LogCapture(LogLevel level = LogLevel::WARNING)
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())) {
    Logger::instance().setLogLevel(level);
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

/* 6 data rows (each a row of @p rows), then 2 zero ridge rows. */
phi_type design(const double (&rows)[6][2]) {
  phi_type Phi(8, 2);
  Phi.set_zero();
  for (size_t i = 0; i < 6; ++i)
    for (size_t j = 0; j < 2; ++j) Phi(i, j) = rows[i][j];
  return Phi;
}

t_type target(const double (&t)[6]) {
  t_type T(8);
  for (size_t i = 0; i < 8; ++i) T[i] = i < 6 ? t[i] : 0.0;
  return T;
}

const double kNoisyRows[6][2] = {{3, 1}, {3, -1}, {-2, 0}, {0, 1}, {3, -2}, {2, -3}};
const double kNoisyT[6] = {-0.2, 0.3, -0.3, 0.2, 0.0, 0.0};
const double kLinearRows[6][2] = {{1, 1}, {-1, 3}, {0, -2}, {2, -2}, {3, 1}, {-3, -3}};
const double kLinearT[6] = {0.2, -1.42, 0.58, 1.6, 1.22, -0.6};

Context ea_ctx() {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "-1");
  c.add("OALGO", "3");
  c.add("ALPHA", "1");
  c.add("BETA", "1");
  return c;
}

/* LinearRegressor::train of (Phi, T) in @p c with n_obs 6: the message it
 * throws, or "no exception". */
std::string fit(Context &c, phi_type Phi, t_type T, t_type &w) {
  tadah::models::memory::ModelsWorkspaceManager wm;
  try {
    LinearRegressor::train(c, Phi, T, w, wm, {}, 6,
                           LinearRegressor::Layout::of(Phi));
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

}  // namespace

/* Targets that are all zero: refused, naming the evidence approximation,
 * the values that are not finite and why (every weight 0 and no residual),
 * and LAMBDA >= 0 as the alternative.
 * Before the fix: no exception, every weight nan. */
TEST_CASE("The evidence approximation refuses alpha and beta that are not "
          "finite",
          "[ea][refusal]") {
  Context c = ea_ctx();
  const double zero[6] = {0, 0, 0, 0, 0, 0};
  t_type w;
  const std::string msg = fit(c, design(kLinearRows), target(zero), w);
  CHECK_THAT(msg, Contains("LAMBDA -1 (evidence approximation)") &&
                      Contains("not finite") && Contains("alpha") &&
                      Contains("beta") && Contains("every target is zero") &&
                      Contains("LAMBDA >= 0"));
}

/* A fit that stops at the iteration limit says so in a WARNING naming the
 * limit, the last relative changes of alpha and beta and the tolerance; it
 * keeps the weights of the last alpha and beta (finite). A fit that
 * converges prints no such WARNING (a pin).
 * Before the fix: no WARNING (the status of the iteration was dropped). */
TEST_CASE("The evidence approximation reports a stop at its iteration limit",
          "[ea][warning]") {
  SECTION("noisy targets: stops at the limit") {
    Context c = ea_ctx();
    t_type w;
    std::string log;
    {
      LogCapture cap;
      REQUIRE(fit(c, design(kNoisyRows), target(kNoisyT), w) == "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK_THAT(log, Contains("WARNING") &&
                        Contains("LAMBDA -1 (evidence approximation)") &&
                        Contains("41 iterations") &&
                        Contains("last relative changes of alpha") &&
                        Contains("5e-12"));
    REQUIRE(w.size() == 2);
    CHECK(finite_bits(w[0]));
    CHECK(finite_bits(w[1]));
  }
  SECTION("linear targets: converges (a pin)") {
    Context c = ea_ctx();
    t_type w;
    std::string log;
    {
      LogCapture cap;
      REQUIRE(fit(c, design(kLinearRows), target(kLinearT), w) == "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK(log.find("evidence approximation") == std::string::npos);
  }
}

/* The relative changes of alpha and beta of the last iteration of @p ea,
 * as stopped_at_limit_text() gives them (3 significant digits). */
static std::pair<double, double> last_changes(const EA &ea) {
  const std::string t = ea.stopped_at_limit_text();
  const std::string::size_type p = t.find("were ");
  const std::string::size_type q = t.find(" and ", p);
  REQUIRE(p != std::string::npos);
  REQUIRE(q != std::string::npos);
  return {std::stod(t.substr(p + 5)), std::stod(t.substr(q + 5))};
}

/* A run whose last iteration, the 41st, brings both relative changes below
 * the tolerance has converged: run() returns 1 and the fit prints no
 * WARNING. A run whose last changes are not both below it still returns 0
 * (a pin). The problems are the noisy targets of the case above plus a
 * signal a * (0.5 x1 - 0.3 x2); between a = 0.05555 and 0.05575 the
 * iteration converges on its 41st step for some a (a = 0.05565 among them,
 * last changes 4.99e-12 and 5.58e-13), and stops without converging for
 * others. A change printed to 3 digits below 5e-12 is below the tolerance;
 * one at 5.00e-12 is left undecided.
 * Before the fix: every run of 41 iterations returned 0 and the fit printed
 * the WARNING, 'stopped at its iteration limit, 41 iterations, without
 * converging: the last relative changes of alpha and beta were 4.99e-12 and
 * 5.58e-13 (convergence needs both below 5e-12)'. */
TEST_CASE("The evidence approximation that converges on its last iteration "
          "is not reported as stopped",
          "[ea][warning]") {
  size_t converged_last = 0, stopped = 0;
  for (int k = 0; k <= 200; ++k) {
    const double a = 0.05555 + k * 1e-6;
    double t[6];
    for (size_t i = 0; i < 6; ++i)
      t[i] = kNoisyT[i] + a * (0.5 * kNoisyRows[i][0] - 0.3 * kNoisyRows[i][1]);
    Context c = ea_ctx();
    phi_type Phi = design(kNoisyRows);
    t_type T = target(t);
    SVD svd;
    svd.compute(Phi);
    EA ea(c, svd, T, 6);
    double alpha = 1.0, beta = 1.0;
    const int status = ea.run(alpha, beta);
    if (ea.iterations() != EA::max_iterations) continue;
    const std::pair<double, double> ch = last_changes(ea);
    CAPTURE(a, status, ch.first, ch.second);
    if (ch.first < EA::tolerance && ch.second < EA::tolerance) {
      ++converged_last;
      CHECK(status == 1);
      /* The fit of the same problem prints no WARNING. */
      Context cf = ea_ctx();
      t_type w;
      std::string log;
      {
        LogCapture cap;
        REQUIRE(fit(cf, design(kNoisyRows), target(t), w) == "no exception");
        log = cap.text();
      }
      INFO(log);
      CHECK(log.find("evidence approximation") == std::string::npos);
    } else if (ch.first > 5.001e-12 || ch.second > 5.001e-12) {
      ++stopped;
      CHECK(status == 0);
    }
  }
  /* The scan has both kinds of run. */
  CHECK(converged_last > 0);
  CHECK(stopped > 0);
}

/* A design matrix and targets whose scales are too far apart are refused
 * by the evidence approximation: one data row of 1e-300 fitted to 1e10
 * (n_obs 1). The weight of the first iteration (lambda 1), 1e-290, is too
 * small to square, so |w|^2 = 0 and alpha = gamma / |w|^2 is not finite.
 * The message names iteration 1, alpha, why, and that no weights were
 * fitted.
 * Before the fix: refused as 'the fit has 1 observations (rows with a
 * non-zero weight) for an effective -nan weights', which names neither the
 * value that is not finite nor why. */
TEST_CASE("The evidence approximation refuses weights too small to square",
          "[ea][refusal]") {
  Context c = ea_ctx();
  phi_type Phi(2, 1);
  Phi.set_zero();
  Phi(0, 0) = 1e-300;
  t_type T(2);
  T[0] = 1e10;
  T[1] = 0.0;
  t_type w;
  tadah::models::memory::ModelsWorkspaceManager wm;
  std::string msg = "no exception";
  try {
    LinearRegressor::train(c, Phi, T, w, wm, {}, 1,
                           LinearRegressor::Layout::of(Phi));
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg, Contains("LAMBDA -1 (evidence approximation)") &&
                      Contains("iteration 1") && Contains("alpha") &&
                      Contains("too small to square") &&
                      Contains("no weights were fitted"));
}

/* Pins: the evidence approximation refuses a fit of no more observations
 * than effective parameters (n_obs 1 for a 3 x 2 design matrix of full
 * rank, whose effective parameters at the first iteration are above 1),
 * naming the observations as the rows of priority above 0 that are not
 * zero in every column and gamma as the effective parameters.
 * Before the fix: '... 1 observations (rows with a non-zero weight) for
 * an effective 1.99... weights', priorities called weights and the
 * effective parameters weights. */
TEST_CASE("The evidence approximation refuses no more observations than "
          "effective parameters",
          "[ea][refusal]") {
  Context c = ea_ctx();
  phi_type Phi(5, 2);
  Phi.set_zero();
  Phi(0, 0) = 1.0;
  Phi(1, 1) = 1.0;
  Phi(2, 0) = 1.0;
  Phi(2, 1) = 1.0;
  t_type T(5);
  T[0] = 1.0;
  T[1] = 2.0;
  T[2] = 3.5;
  T[3] = 0.0;
  T[4] = 0.0;
  t_type w;
  tadah::models::memory::ModelsWorkspaceManager wm;
  std::string msg = "no exception";
  try {
    LinearRegressor::train(c, Phi, T, w, wm, {}, 1,
                           LinearRegressor::Layout::of(Phi));
  } catch (const std::exception &e) {
    msg = e.what();
  }
  CHECK_THAT(msg, StartsWith("LAMBDA -1 (evidence approximation): the fit "
                             "has 1 observations (rows of priority above 0 "
                             "that are not zero in every column) for ") &&
                      Contains(" effective parameters; it needs more "
                               "observations than effective parameters to "
                               "estimate the noise: more data, or LAMBDA "
                               ">= 0, fits."));
}

/* The refusals of the evidence approximation have a type of their own,
 * EvidenceApproximationError (a std::runtime_error): a caller that fits
 * many candidates (tadah hpo) fails the candidate whose targets the
 * approximation cannot estimate, and tells that apart from an error of
 * its own. Both refusals: alpha and beta that are not finite (targets all
 * zero), and weights too small to square.
 * Before the fix: the type did not exist (the test does not compile). */
TEST_CASE("The refusals of the evidence approximation are "
          "EvidenceApproximationError",
          "[ea][refusal]") {
  SECTION("targets all zero") {
    Context c = ea_ctx();
    const double zero[6] = {0, 0, 0, 0, 0, 0};
    phi_type Phi = design(kLinearRows);
    t_type T = target(zero), w;
    tadah::models::memory::ModelsWorkspaceManager wm;
    CHECK_THROWS_AS(LinearRegressor::train(c, Phi, T, w, wm, {}, 6,
                                           LinearRegressor::Layout::of(Phi)),
                    EvidenceApproximationError);
  }
  SECTION("weights too small to square") {
    Context c = ea_ctx();
    phi_type Phi(2, 1);
    Phi.set_zero();
    Phi(0, 0) = 1e-300;
    t_type T(2), w;
    T[0] = 1e10;
    T[1] = 0.0;
    tadah::models::memory::ModelsWorkspaceManager wm;
    CHECK_THROWS_AS(LinearRegressor::train(c, Phi, T, w, wm, {}, 1,
                                           LinearRegressor::Layout::of(Phi)),
                    EvidenceApproximationError);
  }
}

/* While a FitWarningCollector lives on the calling thread, the WARNING of
 * a fit whose evidence approximation stops at its iteration limit goes to
 * it, with its kind, and the Logger prints nothing: a caller that fits many
 * candidates (tadah hpo) says it once. Without a collector, and after one
 * is gone, the Logger prints it (the case above). A collector of another
 * thread collects nothing of this one.
 * Before the fix: FitWarningCollector did not exist (the test does not
 * compile). */
TEST_CASE("A FitWarningCollector takes the WARNING of the evidence "
          "approximation instead of the Logger",
          "[ea][warning]") {
  Context c = ea_ctx();
  t_type w;
  std::string log;
  std::vector<FitWarning> got;
  {
    LogCapture cap;
    {
      FitWarningCollector collect;
      REQUIRE(fit(c, design(kNoisyRows), target(kNoisyT), w) == "no exception");
      got = collect.warnings();
    }
    log = cap.text();
  }
  INFO(log);
  CHECK(log.find("evidence approximation") == std::string::npos);
  REQUIRE(got.size() == 1);
  CHECK(got[0].kind == FitWarningKind::EvidenceApproximationLimit);
  CHECK_THAT(got[0].text, Contains("LAMBDA -1 (evidence approximation)") &&
                              Contains("41 iterations"));
  /* The collector is gone, and another thread's collector collects
   * nothing of this thread: the Logger prints the WARNING again. */
  {
    std::promise<void> fitted;
    std::promise<std::size_t> other_count;
    std::promise<void> ready;
    std::thread other([&] {
      FitWarningCollector theirs;
      ready.set_value();
      fitted.get_future().wait();
      other_count.set_value(theirs.warnings().size());
    });
    ready.get_future().wait();
    Context c2 = ea_ctx();
    {
      LogCapture cap;
      REQUIRE(fit(c2, design(kNoisyRows), target(kNoisyT), w) ==
              "no exception");
      log = cap.text();
    }
    fitted.set_value();
    CHECK(other_count.get_future().get() == 0);
    other.join();
  }
  CHECK_THAT(log, Contains("WARNING") && Contains("41 iterations"));
}

/* The lines of @p text that hold @p what. */
static size_t lines_with(const std::string &text, const std::string &what) {
  size_t n = 0;
  std::istringstream is(text);
  for (std::string line; std::getline(is, line);)
    if (line.find(what) != std::string::npos) ++n;
  return n;
}

/* With VERBOSE 1, the default of tadah, and the Logger at WARNING (the
 * level VERBOSE 1 sets), a fit whose evidence approximation stops at its
 * iteration limit writes nothing on standard output but its WARNING, and
 * one that converges writes nothing. The iterations of the evidence
 * approximation (the relative changes of alpha and beta, alpha, beta,
 * lambda and gamma of each) and how it ended are one INFO message of the
 * Logger, printed at VERBOSE 2. A caller that fits many candidates (tadah
 * hpo) then prints nothing per fit at the default verbosity: it says the
 * WARNING once (FitWarningCollector).
 * Before the fix: EA::run printed its table of iterations (printf) and
 * 'Max number of steps reached: 41' or 'Number of steps for convergence:
 * 6' on standard output whenever VERBOSE was not 0, whatever the level of
 * the Logger, and the Logger had none of it. */
TEST_CASE("The evidence approximation logs its iterations at INFO, not on "
          "standard output",
          "[ea][output]") {
  auto verbose1 = [] {
    Context c = ea_ctx();
    c.remove("VERBOSE");
    c.add("VERBOSE", "1");
    return c;
  };
  SECTION("WARNING: a stop at the iteration limit is its WARNING alone") {
    Context c = verbose1();
    t_type w;
    std::string log;
    {
      LogCapture cap;
      REQUIRE(fit(c, design(kNoisyRows), target(kNoisyT), w) == "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK(log.find("Max number of steps") == std::string::npos);
    CHECK(lines_with(log, "WARNING: LAMBDA -1 (evidence approximation): "
                          "stopped at its iteration limit") == 1);
    CHECK(lines_with(log, "") == 1);
  }
  SECTION("WARNING: a fit that converges writes nothing") {
    Context c = verbose1();
    t_type w;
    std::string log;
    {
      LogCapture cap;
      REQUIRE(fit(c, design(kLinearRows), target(kLinearT), w) == "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK(log.empty());
  }
  SECTION("INFO: the iterations and how the approximation ended") {
    Context c = verbose1();
    t_type w;
    std::string log;
    {
      LogCapture cap(LogLevel::INFO);
      REQUIRE(fit(c, design(kLinearRows), target(kLinearT), w) == "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK(lines_with(log, "INFO: LAMBDA -1 (evidence approximation): "
                          "converged in 6 iterations") == 1);
    CHECK(lines_with(log, "del alpha") == 1);
    CHECK(log.find("Number of steps") == std::string::npos);
    Context cs = verbose1();
    {
      LogCapture cap(LogLevel::INFO);
      REQUIRE(fit(cs, design(kNoisyRows), target(kNoisyT), w) ==
              "no exception");
      log = cap.text();
    }
    INFO(log);
    CHECK(lines_with(log, "INFO: LAMBDA -1 (evidence approximation): "
                          "stopped at its iteration limit, 41 iterations") ==
          1);
    CHECK(lines_with(log, "WARNING: LAMBDA -1 (evidence approximation)") ==
          1);
    CHECK(log.find("Max number of steps") == std::string::npos);
  }
}
