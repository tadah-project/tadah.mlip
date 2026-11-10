// test_oalgo_requests.cpp
//
// What a fit does with OALGO. Its first value selects the solve: 1
// (DGELSD), 2 (DGELS, QR), 3 (the SVD solve) or 4 (the normal equations,
// Cholesky). Its second value M is the rank threshold of OALGO 1 and 3: a
// singular value at most M times the largest is left out of the
// coefficients for 0 < M < 1; below 0 OALGO 1 takes the machine precision
// of LAPACK, and at 0 or below OALGO 3 leaves out only a singular value of
// 0. Every fit refuses, before it solves, what is none of these: an M of 0
// or of 1 or more under OALGO 1 (DGELSD would take the machine precision
// for either), an M of 1 or more under OALGO 3 (it would leave out every
// direction: every coefficient 0), a second value under OALGO 2 and 4
// (they have no rank threshold), and a first value other than 1 to 4. (A
// value that is not a finite number the Context refuses when it reads it.) LinearRegressor::train,
// LinearRegressor::train_with_factor (OALGO 4, the analytical gradients of
// tadah hpo) and the check of the configuration alone that tadah hpo makes
// when a run starts (require_lambda_for_oalgo) apply the same rules.
//
// Design matrices laid out as DesignMatrix lays them out: 6 data rows, then
// one zero ridge row per column (2 columns).

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/factor_handle.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/memory/models_workspace_manager.h>

#include <stdexcept>
#include <string>
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

t_type target() {
  const double t[6] = {0.2, -1.42, 0.58, 1.6, 1.22, -0.6};
  t_type T(8);
  for (size_t i = 0; i < 8; ++i) T[i] = i < 6 ? t[i] : 0.0;
  return T;
}

/* LAMBDA 0 and OALGO "<values>". */
Context make_ctx(const std::vector<std::string> &oalgo) {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "0");
  for (const std::string &v : oalgo) c.add("OALGO", v);
  return c;
}

std::string line(const std::vector<std::string> &oalgo) {
  std::string s = "OALGO";
  for (const std::string &v : oalgo) s += " " + v;
  return s;
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

/* The message of the check of the configuration alone, or "no
 * exception". */
std::string config_refusal(const Context &c) {
  try {
    LinearRegressor::require_lambda_for_oalgo(c);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "no exception";
}

}  // namespace

/* Pins: OALGO 1 refuses an M of 0 and of 1 or more, naming the line, the
 * range 0 < M < 1, a negative M for the machine precision and why: DGELSD
 * takes the machine precision for either. The fit and the check of the configuration alone give the
 * same message.
 * Before the fix: each was fitted with the machine precision of LAPACK
 * (no exception). */
TEST_CASE("OALGO 1 refuses a rank threshold of 0 or of 1 or more",
          "[oalgo][refusal]") {
  for (const std::string M : {"0", "1", "1.5"}) {
    const std::vector<std::string> o = {"1", M};
    INFO(line(o));
    const std::string msg = train_refusal(make_ctx(o));
    CHECK(msg == line(o) +
                     ": the rank threshold M of OALGO 1 (DGELSD), its second "
                     "value, is a number 0 < M < 1 (a singular value at most "
                     "M times the largest is left out of the coefficients) "
                     "or below 0 for the machine precision of LAPACK, 2^-53 "
                     "(OALGO 1 -1); DGELSD takes the machine precision for "
                     "an M of 0 or of 1 or more.");
    CHECK(config_refusal(make_ctx(o)) == msg);
  }
}

/* Pins: OALGO 3 refuses an M of 1 or more, naming the line, the range
 * 0 < M < 1, M <= 0 for every singular value above 0, and why: an M of 1
 * or more leaves out every singular direction.
 * Before the fix: OALGO 3 1 and 3 2 fitted every coefficient 0, with a
 * WARNING (no exception). */
TEST_CASE("OALGO 3 refuses a rank threshold of 1 or more",
          "[oalgo][refusal]") {
  for (const std::string M : {"1", "2"}) {
    const std::vector<std::string> o = {"3", M};
    INFO(line(o));
    const std::string msg = train_refusal(make_ctx(o));
    CHECK(msg == line(o) +
                     ": the rank threshold M of OALGO 3, its second value, "
                     "is a number 0 < M < 1 (a singular value at most M "
                     "times the largest is left out of the coefficients) or "
                     "0 or below (only a singular value of 0 is left out); "
                     "an M of 1 or more leaves out every singular direction, "
                     "every coefficient 0.");
    CHECK(config_refusal(make_ctx(o)) == msg);
  }
}

/* Pins: OALGO 2 and 4 refuse a second value, naming the line and that only
 * OALGO 1 and 3 have a rank threshold; train_with_factor (OALGO 4) too.
 * Before the fix: the second value was not read (no exception). */
TEST_CASE("OALGO 2 and 4 refuse a second value", "[oalgo][refusal]") {
  for (const std::string oalgo : {"2", "4"})
    for (const std::string M : {"0.5", "1e-8", "-1"}) {
      const std::vector<std::string> o = {oalgo, M};
      INFO(line(o));
      const std::string name = oalgo == "2" ? "OALGO 2 (DGELS, QR)"
                                            : "OALGO 4 (normal equations, "
                                              "Cholesky)";
      const std::string msg = train_refusal(make_ctx(o));
      CHECK(msg == line(o) + ": " + name +
                       " takes one value: it keeps every direction and has "
                       "no rank threshold, the second value of OALGO 1 and "
                       "3.");
      CHECK(config_refusal(make_ctx(o)) == msg);
    }
  CHECK_THAT(factor_refusal(make_ctx({"4", "0.5"})),
             Contains("OALGO 4 0.5: OALGO 4 (normal equations, Cholesky) "
                      "takes one value"));
}

/* Pins: a first value other than 1 to 4 is refused by the check of the
 * configuration alone, naming the four solves.
 * Before the fix: the check accepted it; only the solve refused it
 * ('Unsupported OALGO: 5'). */
TEST_CASE("OALGO is 1, 2, 3 or 4", "[oalgo][refusal]") {
  for (const std::string oalgo : {"0", "5", "-3"}) {
    INFO("OALGO " << oalgo);
    const std::string msg = config_refusal(make_ctx({oalgo}));
    CHECK(msg == "OALGO " + oalgo +
                     ": OALGO is 1 (DGELSD), 2 (DGELS, QR), 3 (the SVD "
                     "solve) or 4 (normal equations, Cholesky).");
    CHECK(train_refusal(make_ctx({oalgo})) == msg);
  }
}

/* Pins: what the rules accept fits: OALGO 1 with M -1, 1e-8 and 0.5, OALGO
 * 3 with M -1, 0, 1e-8 and 0.5, OALGO 1 to 4 with one value. */
TEST_CASE("OALGO: the accepted values fit", "[oalgo][refusal]") {
  for (const std::vector<std::string> &o :
       std::vector<std::vector<std::string>>{{"1", "-1"},
                                             {"1", "1e-8"},
                                             {"1", "0.5"},
                                             {"3", "-1"},
                                             {"3", "0"},
                                             {"3", "1e-8"},
                                             {"3", "0.5"},
                                             {"1"},
                                             {"2"},
                                             {"3"},
                                             {"4"}}) {
    INFO(line(o));
    CHECK(config_refusal(make_ctx(o)) == "no exception");
    CHECK(train_refusal(make_ctx(o)) == "no exception");
  }
  CHECK(factor_refusal(make_ctx({"4"})) == "no exception");
}
