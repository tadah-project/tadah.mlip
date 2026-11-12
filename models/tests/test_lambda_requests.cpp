// test_lambda_requests.cpp
//
// What a fit does with LAMBDA. LAMBDA 0 fits without a penalty, LAMBDA > 0
// is the ridge penalty, and LAMBDA -1 requests the evidence approximation,
// which only OALGO 3 (the SVD solve) runs. LAMBDA takes one value: the rank
// threshold of OALGO 1 and 3 is OALGO's second value. Every fit refuses
// what is none of these, before it solves: a negative LAMBDA other than -1
// (OALGO 1, 2 and 4 fitted it as LAMBDA 0, OALGO 3 ran the evidence
// approximation with it), LAMBDA -1 under another OALGO, and a second
// LAMBDA value (never read). LinearRegressor::train (M_BLR, and M_KRR
// implementation 1), LinearRegressor::train_with_factor (the analytical
// gradients of tadah hpo) and M_KRR_Train::train2 (M_KRR implementation 2)
// apply the same rules.
//
// Design matrices laid out as DesignMatrix lays them out: 6 data rows, then
// one zero ridge row per column (2 columns).

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/factor_handle.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/m_krr_train.h>
#include <tadah/models/memory/models_workspace_manager.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;
using Catch::Matchers::Contains;

namespace {

/* 6 data rows of 2 columns (full rank), then 2 zero ridge rows. */
phi_type design() {
  const double rows[6][2] = {{1, 1}, {-1, 3}, {0, -2}, {2, -2}, {3, 1}, {-3, -3}};
  phi_type Phi(8, 2);
  Phi.set_zero();
  for (size_t i = 0; i < 6; ++i)
    for (size_t j = 0; j < 2; ++j) Phi(i, j) = rows[i][j];
  return Phi;
}

/* Targets of design(): 0.5 x column 1 - 0.3 x column 2 plus a little
 * noise, 0 on the ridge rows. */
t_type target() {
  const double t[6] = {0.2, -1.42, 0.58, 1.6, 1.22, -0.6};
  t_type T(8);
  for (size_t i = 0; i < 8; ++i) T[i] = i < 6 ? t[i] : 0.0;
  return T;
}

Context make_ctx(const std::vector<std::string> &lambda,
                 const std::vector<std::string> &oalgo) {
  Context c;
  c.add("VERBOSE", "0");
  for (const std::string &v : lambda) c.add("LAMBDA", v);
  for (const std::string &v : oalgo) c.add("OALGO", v);
  return c;
}

/* The message LinearRegressor::train throws, or "no exception". */
std::string train_refusal(Context c) {
  phi_type Phi = design();
  t_type T = target();
  t_type w;
  try {
    LinearRegressor::train(c, Phi, T, w);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

/* The message train_with_factor throws, or "no exception". */
std::string factor_refusal(Context c) {
  phi_type Phi = design();
  t_type T = target();
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

/* The message M_KRR_Train<Kern_RBF>::train2 throws on three basis vectors,
 * or "no exception". */
std::string train2_refusal(Context c) {
  c.add("MPARAMS", "1.0");
  Kern_RBF k(c);
  M_KRR_Train<Kern_RBF> m(k, c);
  Matrix B(2, 3);
  const double b[2][3] = {{1.0, 0.2, -0.4}, {0.3, 1.1, 0.5}};
  for (size_t i = 0; i < 2; ++i)
    for (size_t j = 0; j < 3; ++j) B(i, j) = b[i][j];
  t_type T(3);
  T[0] = 0.1;
  T[1] = -0.2;
  T[2] = 0.3;
  try {
    m.train2(B, T);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

}  // namespace

/* LAMBDA -2 and -0.5 are refused under every OALGO, by every fit: the
 * message names the value, says that only -1 requests the evidence
 * approximation, with OALGO 3, and what 0 and positive values do.
 * Before the fix: OALGO 1, 2 and 4 fitted them as LAMBDA 0 and OALGO 3
 * ran the evidence approximation (no exception), train_with_factor said
 * only 'LAMBDA must be >= 0', and train2 called each 'LAMBDA -1
 * (Evidence Approximation)'. */
TEST_CASE("A negative LAMBDA other than -1 is refused by every fit",
          "[lambda][refusal]") {
  for (const std::string lambda : {"-2", "-0.5"}) {
    for (const std::string oalgo : {"1", "2", "3", "4"}) {
      INFO("LAMBDA " << lambda << ", OALGO " << oalgo);
      const std::string msg = train_refusal(make_ctx({lambda}, {oalgo}));
      CHECK_THAT(msg, Contains("LAMBDA " + lambda) &&
                          Contains("only LAMBDA -1 requests the evidence "
                                   "approximation") &&
                          Contains("OALGO 3"));
    }
    INFO("train_with_factor, LAMBDA " << lambda);
    CHECK_THAT(factor_refusal(make_ctx({lambda}, {"4"})),
               Contains("LAMBDA " + lambda) &&
                   Contains("only LAMBDA -1 requests the evidence approximation"));
    INFO("train2, LAMBDA " << lambda);
    CHECK_THAT(train2_refusal(make_ctx({lambda}, {"1"})),
               Contains("LAMBDA " + lambda) &&
                   Contains("only LAMBDA -1 requests the evidence approximation"));
  }
}

/* LAMBDA -1 under OALGO 1, 2 and 4 is refused naming the OALGO of the
 * configuration and OALGO 3; under OALGO 3 it fits.
 * Before the fix: 'LAMBDA -1 requires OALGO 3', without the OALGO of the
 * configuration. */
TEST_CASE("LAMBDA -1 needs OALGO 3", "[lambda][refusal]") {
  for (const std::string oalgo : {"1", "2", "4"}) {
    INFO("OALGO " << oalgo);
    CHECK_THAT(train_refusal(make_ctx({"-1"}, {oalgo})),
               Contains("LAMBDA -1") && Contains("OALGO 3") &&
                   Contains("OALGO " + oalgo));
  }
  CHECK(train_refusal(make_ctx({"-1"}, {"3"})) == "no exception");
}

/* A second LAMBDA value is refused by every fit, naming OALGO's second
 * value as the rank threshold; one value fits.
 * Before the fix: the second value was never read (no exception). */
TEST_CASE("A second LAMBDA value is refused, naming OALGO's second value",
          "[lambda][refusal]") {
  for (const std::string oalgo : {"1", "2", "3", "4"}) {
    INFO("OALGO " << oalgo);
    CHECK_THAT(train_refusal(make_ctx({"0", "1e-12"}, {oalgo})),
               Contains("LAMBDA 0 1e-12") && Contains("one value") &&
                   Contains("OALGO's second value"));
    CHECK(train_refusal(make_ctx({"0"}, {oalgo})) == "no exception");
  }
  CHECK_THAT(factor_refusal(make_ctx({"0.1", "1e-12"}, {"4"})),
             Contains("LAMBDA 0.1 1e-12") && Contains("OALGO's second value"));
  CHECK_THAT(train2_refusal(make_ctx({"0", "1e-12"}, {"1"})),
             Contains("LAMBDA 0 1e-12") && Contains("OALGO's second value"));
  CHECK(train2_refusal(make_ctx({"0.01"}, {"1"})) == "no exception");
}

/* Pins: M_KRR implementation 2 refuses LAMBDA -1 stating its solve,
 * (K + LAMBDA sigma^2 I) w = t with sigma = ESIGMA in eV, which takes a
 * fixed LAMBDA, and naming implementation 1 with OALGO 3, which runs the
 * evidence approximation.
 * Before the fix: the message wrote the solve (K + LAMBDA I) w = t,
 * without the error scale that train2 applies. */
TEST_CASE("M_KRR implementation 2 refuses LAMBDA -1 stating its solve",
          "[lambda][refusal][krr]") {
  for (const std::string oalgo : {"1", "3"}) {
    INFO("OALGO " << oalgo);
    CHECK(train2_refusal(make_ctx({"-1"}, {oalgo})) ==
          "MODEL M_KRR Kern_RBF 2 cannot use LAMBDA -1: implementation 2 "
          "solves (K + LAMBDA sigma^2 I) w = t, sigma = ESIGMA in eV, with a "
          "fixed LAMBDA and does not run the evidence approximation. "
          "Implementation 1 (MODEL M_KRR Kern_RBF 1) with OALGO 3 runs it; "
          "LAMBDA >= 0 fits with either.");
  }
}

/* The rules above can be checked before any fit, on the configuration
 * alone (tadah hpo checks them when the run starts, and every evaluation
 * would refuse them otherwise): LinearRegressor::require_lambda_for_oalgo
 * refuses what LinearRegressor::train refuses, with its message, and
 * require_krr_implementation2_lambda what M_KRR_Train::train2 refuses.
 * Before the fix: neither function existed (the test does not compile). */
TEST_CASE("The LAMBDA rules of a fit are checked without fitting",
          "[lambda][refusal]") {
  const std::vector<std::pair<std::vector<std::string>,
                              std::vector<std::string>>> cases = {
      {{"-2"}, {"1"}},  {{"-0.5"}, {"3"}}, {{"-1"}, {"1"}},
      {{"-1"}, {"2"}},  {{"-1"}, {"4"}},   {{"0", "1e-12"}, {"3"}},
      {{"-1"}, {"3"}},  {{"0"}, {"1"}},    {{"1e-3"}, {"4"}},
      {{"0.1"}, {"3"}}};
  for (const auto &[lambda, oalgo] : cases) {
    const Context c = make_ctx(lambda, oalgo);
    std::string given = "LAMBDA";
    for (const std::string &v : lambda) given += " " + v;
    INFO(given << ", OALGO " << oalgo[0]);
    std::string checked = "no exception";
    try {
      LinearRegressor::require_lambda_for_oalgo(c);
    } catch (const std::exception &e) {
      checked = e.what();
    }
    CHECK(checked == train_refusal(c));
  }
  for (const std::vector<std::string> lambda :
       {std::vector<std::string>{"-1"}, {"-2"}, {"0", "1e-12"}, {"0.01"},
        {"0"}}) {
    const Context c = make_ctx(lambda, {"1"});
    std::string given = "LAMBDA";
    for (const std::string &v : lambda) given += " " + v;
    INFO(given);
    std::string checked = "no exception";
    try {
      require_krr_implementation2_lambda(c, "Kern_RBF");
    } catch (const std::exception &e) {
      checked = e.what();
    }
    CHECK(checked == train2_refusal(c));
  }
}
