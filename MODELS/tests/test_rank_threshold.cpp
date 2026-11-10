// test_rank_threshold.cpp
//
// The rank threshold of OALGO 1 and 3, OALGO's second value M, as the help
// of OALGO states it:
//
//  - OALGO 1 (DGELSD) leaves out of the solution a singular direction
//    whose singular value is at most M times the largest for 0 < M < 1,
//    and at most the machine precision of LAPACK, 2^-53, times the largest
//    for M < 0;
//  - OALGO 3 (the SVD solve) leaves out one whose singular value is at
//    most M times the largest for 0 < M < 1, and only one of singular
//    value 0 for M <= 0; its WARNING names that threshold.
//
// The other values of M are refused (test_oalgo_requests.cpp).
//
// The design matrix: two data rows, (1, 0) and (0, t), with targets 1 and
// 1, and the two zero ridge-augmentation rows of the DesignMatrix layout
// (LAMBDA 0). Its singular values are 1 and t, and its weights are
// (1, 1/t) with both directions kept, (1, 0) without the second.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/logger.h>
#include <tadah/models/linear_regressor.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

namespace {

/* The weights of a LAMBDA 0 fit with OALGO "<oalgo> <M>" of the design
 * matrix above. */
t_type fit(int oalgo, const std::string &M, double t) {
  Context c;
  c.add("LAMBDA", "0");
  c.add("OALGO", std::to_string(oalgo) + " " + M);
  phi_type Phi(4, 2);
  Phi.set_zero();
  Phi(0, 0) = 1.0;
  Phi(1, 1) = t;
  t_type T(4);
  T[0] = 1.0;
  T[1] = 1.0;
  T[2] = 0.0;
  T[3] = 0.0;
  t_type w;
  LinearRegressor::train(c, Phi, T, w);
  return w;
}

struct Case {
  double t;
  const char *M;
  bool first_kept;
  bool second_kept;
};

void check(int oalgo, const Case &k) {
  DYNAMIC_SECTION("OALGO " << oalgo << " " << k.M << ", t " << k.t) {
    const t_type w = fit(oalgo, k.M, k.t);
    REQUIRE(w.size() == 2);
    if (k.first_kept)
      CHECK(w[0] == Approx(1.0).epsilon(1e-12));
    else
      CHECK(w[0] == 0.0);
    if (k.second_kept)
      CHECK(w[1] == Approx(1.0 / k.t).epsilon(1e-12));
    else
      CHECK(w[1] == 0.0);
  }
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

}  // namespace

/* Pins: OALGO 1 with M 1e-8 or 0.5 leaves out the singular value 1e-12;
 * with M -1 it keeps it, and 1.5e-16, and leaves out 1e-16: DGELSD's
 * threshold is then 2^-53 (1.11e-16) times the largest.
 * A guard (DGELSD's rule, which the help of OALGO states): a fit that
 * passed DGELSD an M below 0 as the smallest positive double fails its
 * case t 1e-16 with M -1. */
TEST_CASE("OALGO 1: an M below 0 is the machine precision",
          "[oalgo1][rank_threshold]") {
  for (const Case &k :
       {Case{1e-12, "1e-8", true, false}, Case{1e-12, "0.5", true, false},
        Case{1e-6, "1e-8", true, true}, Case{1e-12, "-1", true, true},
        Case{1.5e-16, "-1", true, true}, Case{1e-16, "-1", true, false}})
    check(1, k);
}

/* Pins: OALGO 3 with M -1 or 0 keeps the singular value 1e-17, below the
 * machine precision times the largest (the coefficient 1e17), and with M
 * 1e-8 or 0.5 leaves it out.
 * A guard (the rule of OLS::pseudoinverse_threshold, which the help of
 * OALGO states): a threshold of 2^-53 times the largest for an M of 0 or
 * less fails its cases t 1e-17 with M -1 and 0. */
TEST_CASE("OALGO 3: an M of 0 or less keeps every singular value above 0",
          "[oalgo3][rank_threshold]") {
  for (const Case &k :
       {Case{1e-17, "-1", true, true}, Case{1e-17, "0", true, true},
        Case{1e-12, "-1", true, true}, Case{1e-17, "1e-8", true, false},
        Case{1e-12, "1e-8", true, false}, Case{1e-6, "1e-8", true, true},
        Case{0.2, "0.5", true, false}})
    check(3, k);
}

/* Pins: OALGO 3 with M -1 or 0 and an exactly zero column (t 0) leaves out
 * its direction, and the one WARNING names the threshold 0:
 * "OALGO 3: 1 of 2 singular directions of the design matrix are at or
 * below the rank threshold (0, as OALGO's rcond -1 is not above 0): the
 * solve leaves them out of the coefficients."
 * Before the fix: the WARNING named the threshold "OALGO's rcond -1 times
 * the largest singular value", a negative threshold the solve does not
 * use; then it began "LinearRegressor (OALGO 3): ", a class name, where it
 * begins "OALGO 3: ": the text check fails on that. */
TEST_CASE("OALGO 3: the WARNING of an M of 0 or less names the threshold 0",
          "[oalgo3][rank_threshold]") {
  for (const std::string M : {"-1", "0"}) {
    DYNAMIC_SECTION("OALGO 3 " << M) {
      t_type w;
      std::string log;
      {
        LogCapture cap;
        w = fit(3, M, 0.0);
        log = cap.text();
      }
      INFO(log);
      REQUIRE(w.size() == 2);
      CHECK(w[0] == Approx(1.0).epsilon(1e-12));
      CHECK(w[1] == 0.0);
      const std::string warning =
          "WARNING: OALGO 3: 1 of 2 singular directions of the design "
          "matrix are at or below the rank threshold (0, as OALGO's rcond " +
          M + " is not above 0): the solve leaves them out of the "
          "coefficients.\n";
      REQUIRE(log.size() >= warning.size());
      CHECK(log.compare(log.size() - warning.size(), warning.size(),
                        warning) == 0);
      CHECK(log.find("WARNING") == log.rfind("WARNING"));
    }
  }
}

/* Pins: OALGO 3 at LAMBDA 0 with M 0, -1 or 1e-300 keeps a singular
 * value of 1e-200 (1e-200 times the largest, 1), whose square underflows
 * to 0 in double precision: its coefficient, 1e-200 / (1e-200)^2, is not
 * finite, and the fit is refused, the message naming the OALGO line, the
 * singular value, that a LAMBDA above 0 keeps the coefficient finite and
 * that an M of at least 1e-200 leaves the direction out, as the default M
 * of OALGO 3 does ("... OALGO 3 0 ... keeps a singular direction of the
 * design matrix of singular value 1e-200, whose square underflows to 0 in
 * double precision ... a rank threshold M (OALGO's second value) of at
 * least 1e-200 ... (OALGO 3 with its default M, 1e-8, does)"). With M 1e-8
 * the direction is left out and the coefficients are (1, 0).
 * Before the fix: the message said that the square was "below the smallest
 * double" (the smallest double is the most negative one) and ended "(OALGO
 * 3 1e-8, the default, does)", which reads as if OALGO 3 1e-8 were the
 * default of OALGO, which is 1. */
TEST_CASE("OALGO 3: a kept singular direction that gives coefficients that "
          "are not finite is refused",
          "[oalgo3][rank_threshold][nonfinite]") {
  for (const std::string M : {"0", "-1", "1e-300"}) {
    DYNAMIC_SECTION("OALGO 3 " << M) {
      std::string what;
      try {
        (void)fit(3, M, 1e-200);
      } catch (const std::runtime_error &e) {
        what = e.what();
      }
      INFO(what);
      CHECK(what.find("OALGO 3 " + M + ":") == 0);
      CHECK(what.find("not finite") != std::string::npos);
      CHECK(what.find("singular value 1e-200, whose square underflows to "
                      "0 in double precision, so that its coefficient is "
                      "not finite at LAMBDA 0") != std::string::npos);
      CHECK(what.find("LAMBDA above 0") != std::string::npos);
      CHECK(what.find("at least 1e-200") != std::string::npos);
      CHECK(what.find("(OALGO 3 with its default M, 1e-8, does)") !=
            std::string::npos);
    }
  }
  const t_type w = fit(3, "1e-8", 1e-200);
  REQUIRE(w.size() == 2);
  CHECK(w[0] == Approx(1.0).epsilon(1e-12));
  CHECK(w[1] == 0.0);
}

namespace {

/* What a fit with OALGO @p oalgo and LAMBDA @p lambda refuses ("" when it
 * fits) of the design matrix rows (1, 0), (1, 0) and (0, 1) with the
 * targets 1.5e308, 1.5e308 and 1, and the two ridge-augmentation rows of
 * the DesignMatrix layout: U^T T, the targets on the first singular
 * direction (singular value sqrt(2)), overflows. */
std::string overflow_refusal(const std::string &oalgo,
                             const std::string &lambda) {
  Context c;
  c.add("LAMBDA", lambda);
  c.add("OALGO", oalgo);
  phi_type Phi(5, 2);
  Phi.set_zero();
  Phi(0, 0) = 1.0;
  Phi(1, 0) = 1.0;
  Phi(2, 1) = 1.0;
  t_type T(5);
  T[0] = 1.5e308;
  T[1] = 1.5e308;
  T[2] = 1.0;
  T[3] = 0.0;
  T[4] = 0.0;
  t_type w;
  try {
    LinearRegressor::train(c, Phi, T, w);
  } catch (const std::runtime_error &e) {
    return e.what();
  }
  return "";
}

}  // namespace

/* Pins: coefficients that are not finite for another reason than a kept
 * singular value whose square underflows to 0 -- here the
 * targets, 1.5e308 on rows 1 and 2, overflow the solve, whose singular
 * values are sqrt(2) and 1 -- are refused under OALGO 3 with the message
 * of every OALGO, which names no singular direction: OALGO 3 1e-8 at
 * LAMBDA 0.001 and at LAMBDA 0 gives "OALGO 3 1e-8: the solve gave
 * coefficients that are not finite (nan or inf; the first is coefficient
 * ... of 2). No coefficients were fitted.", as OALGO 4 does ("OALGO 4: the
 * solve gave ...").
 * Before the fix: under OALGO 3 the message always added that the solve
 * keeps a singular direction of singular value 1 whose square is below
 * the smallest double at LAMBDA 0 and that a positive M leaves such
 * directions out: none of it true here (LAMBDA 0.001, M 1e-8, 1 squared is
 * 1). */
TEST_CASE("OALGO 3: coefficients that are not finite for another reason "
          "are refused without naming a singular direction",
          "[oalgo3][rank_threshold][nonfinite]") {
  const std::string tail = "). No coefficients were fitted.";
  for (const char *lambda : {"0.001", "0"}) {
    DYNAMIC_SECTION("LAMBDA " << lambda) {
      for (const std::string oalgo : {"3 1e-8", "4"}) {
        const std::string what = overflow_refusal(oalgo, lambda);
        INFO("OALGO " << oalgo << ": " << what);
        CHECK(what.find("OALGO " + oalgo + ": the solve gave coefficients "
                        "that are not finite (nan or inf; the first is "
                        "coefficient ") == 0);
        REQUIRE(what.size() >= tail.size());
        CHECK(what.compare(what.size() - tail.size(), tail.size(), tail) ==
              0);
        CHECK(what.find("singular") == std::string::npos);
        CHECK(what.find("positive M") == std::string::npos);
      }
    }
  }
}
