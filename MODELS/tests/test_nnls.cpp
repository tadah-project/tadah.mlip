/* FEAT-02: the Lawson-Hanson NNLS solver (tadah/models/nnls.h) against
 * brute-force active-set enumeration: the NNLS optimum is the unconstrained
 * least-squares solution on its passive set, so the best feasible subset
 * solution is the reference. The subset solves use the normal equations in
 * long double, an independent method from nnls's Householder QR. Includes
 * row weights, column masks, the lambda rows, zero columns, and
 * ill-conditioned layouts of integrated-bump (RHOFORM MONOTONE) columns:
 * the theta_star_ON steps of the FEAT-02 campaign (cond 5e7), and exactly
 * representable fits (cond 1e8 to 1e13) that must reach the rounding floor,
 * which a dual formed from b - A x does not (it stopped at |r|/|b| ~ 1e-9).
 * The Kuhn-Tucker conditions are checked as well.
 */
#include "catch2/catch.hpp"

#include "ream_campaign_fixtures.h"

#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/nnls.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace tadah::models;

namespace {

struct Problem {
  std::size_t m = 0, n = 0;
  std::vector<double> A;  // column-major
  std::vector<double> b;
  std::vector<double> w;             // empty = unweighted
  std::vector<unsigned char> mask;   // empty = every column free
  double lambda = 0.0;

  double a(std::size_t i, std::size_t j) const { return A[j * m + i]; }
  double wi(std::size_t i) const { return w.empty() ? 1.0 : w[i]; }
  bool free_col(std::size_t j) const { return mask.empty() || mask[j]; }
};

long double objective(const Problem &p, const std::vector<long double> &x) {
  long double f = 0.0L;
  for (std::size_t i = 0; i < p.m; ++i) {
    long double r = -static_cast<long double>(p.b[i]);
    for (std::size_t j = 0; j < p.n; ++j) r += p.a(i, j) * x[j];
    f += p.wi(i) * r * r;
  }
  for (std::size_t j = 0; j < p.n; ++j) f += p.lambda * x[j] * x[j];
  return f;
}

// Unconstrained weighted/regularised least squares on the columns S; false
// when singular.
bool ls_subset(const Problem &p, const std::vector<std::size_t> &S,
               std::vector<long double> &x) {
  const std::size_t k = S.size();
  std::vector<long double> N(k * k, 0.0L), rhs(k, 0.0L);
  for (std::size_t u = 0; u < k; ++u) {
    for (std::size_t v = 0; v < k; ++v) {
      long double s = 0.0L;
      for (std::size_t i = 0; i < p.m; ++i)
        s += p.wi(i) * static_cast<long double>(p.a(i, S[u])) * p.a(i, S[v]);
      N[u * k + v] = s + (u == v ? p.lambda : 0.0L);
    }
    long double s = 0.0L;
    for (std::size_t i = 0; i < p.m; ++i)
      s += p.wi(i) * static_cast<long double>(p.a(i, S[u])) * p.b[i];
    rhs[u] = s;
  }
  for (std::size_t c = 0; c < k; ++c) {  // Gauss, partial pivoting
    std::size_t piv = c;
    for (std::size_t r = c + 1; r < k; ++r)
      if (std::fabs(N[r * k + c]) > std::fabs(N[piv * k + c])) piv = r;
    if (N[piv * k + c] == 0.0L) return false;
    if (piv != c) {
      for (std::size_t q = 0; q < k; ++q) std::swap(N[c * k + q], N[piv * k + q]);
      std::swap(rhs[c], rhs[piv]);
    }
    for (std::size_t r = c + 1; r < k; ++r) {
      const long double f = N[r * k + c] / N[c * k + c];
      for (std::size_t q = c; q < k; ++q) N[r * k + q] -= f * N[c * k + q];
      rhs[r] -= f * rhs[c];
    }
  }
  std::vector<long double> y(k);
  for (std::size_t c = k; c-- > 0;) {
    long double s = rhs[c];
    for (std::size_t q = c + 1; q < k; ++q) s -= N[c * k + q] * y[q];
    y[c] = s / N[c * k + c];
  }
  x.assign(p.n, 0.0L);
  for (std::size_t u = 0; u < k; ++u) x[S[u]] = y[u];
  return true;
}

// The best feasible subset solution over every subset of the free columns.
std::vector<long double> brute_force(const Problem &p) {
  std::vector<std::size_t> cols;
  for (std::size_t j = 0; j < p.n; ++j)
    if (p.free_col(j)) cols.push_back(j);
  std::vector<long double> best(p.n, 0.0L);
  long double fbest = objective(p, best);
  for (unsigned long mask = 1; mask < (1UL << cols.size()); ++mask) {
    std::vector<std::size_t> S;
    for (std::size_t q = 0; q < cols.size(); ++q)
      if (mask & (1UL << q)) S.push_back(cols[q]);
    std::vector<long double> x;
    if (!ls_subset(p, S, x)) continue;
    bool feasible = true;
    for (std::size_t j : S) feasible = feasible && x[j] >= 0.0L;
    if (!feasible) continue;
    const long double f = objective(p, x);
    if (f < fbest) { fbest = f; best = x; }
  }
  return best;
}

NnlsResult solve(const Problem &p, std::vector<double> &x) {
  x.assign(p.n, -1.0);
  return nnls(p.A.data(), p.m, p.n, p.b.data(), x.data(),
              p.w.empty() ? nullptr : p.w.data(),
              p.mask.empty() ? nullptr : p.mask.data(), p.lambda);
}

std::vector<long double> widen(const std::vector<double> &x) {
  return std::vector<long double>(x.begin(), x.end());
}

// Kuhn-Tucker conditions of the solution x: x >= 0, the gradient
// g = A^T W (A x - b) + lambda x vanishes on the passive set and is >= 0 on
// the zero set, to a tolerance scaled by |A| |r|, with the double-precision
// rounding floor eps |A| (|A| |x| + |b|) of a (near-)exact fit.
void check_kkt(const Problem &p, const std::vector<double> &x,
               const NnlsResult &res, double rel) {
  long double normA = 0.0L, rr = 0.0L, bb = 0.0L, xx = 0.0L;
  std::vector<long double> r(p.m);
  for (std::size_t i = 0; i < p.m; ++i) {
    long double s = -static_cast<long double>(p.b[i]);
    for (std::size_t j = 0; j < p.n; ++j) {
      s += p.a(i, j) * static_cast<long double>(x[j]);
      normA += p.wi(i) * p.a(i, j) * p.a(i, j);
    }
    r[i] = s;
    rr += p.wi(i) * s * s;
    bb += p.wi(i) * static_cast<long double>(p.b[i]) * p.b[i];
  }
  for (double v : x) xx += static_cast<long double>(v) * v;
  const double nA = std::sqrt(static_cast<double>(normA));
  const double eps = std::numeric_limits<double>::epsilon();
  const double tol =
      rel * nA * std::sqrt(static_cast<double>(rr)) +
      64.0 * eps * nA *
          (nA * std::sqrt(static_cast<double>(xx)) +
           std::sqrt(static_cast<double>(bb)));
  std::size_t npos = 0;
  for (std::size_t j = 0; j < p.n; ++j) {
    INFO("column " << j);
    REQUIRE(x[j] >= 0.0);
    if (!p.free_col(j)) { REQUIRE(x[j] == 0.0); continue; }
    long double g = p.lambda * static_cast<long double>(x[j]);
    for (std::size_t i = 0; i < p.m; ++i) g += p.wi(i) * p.a(i, j) * r[i];
    if (x[j] > 0.0) {
      ++npos;
      CHECK(std::fabs(static_cast<double>(g)) <= tol);
    } else {
      CHECK(static_cast<double>(g) >= -tol);
    }
  }
  CHECK(res.passive.size() == npos);
  for (std::size_t j : res.passive) CHECK(x[j] > 0.0);
  CHECK(res.converged);
}

Problem random_problem(std::mt19937_64 &rng, std::size_t m, std::size_t n) {
  std::uniform_real_distribution<double> U(-1.0, 1.0);
  Problem p;
  p.m = m;
  p.n = n;
  p.A.resize(m * n);
  for (double &v : p.A) v = U(rng);
  p.b.resize(m);
  for (double &v : p.b) v = U(rng);
  return p;
}

// An exactly representable fit b = A x* of evenly spaced Blip5 steps
// S_j(r) = SB5(eta (r - c0 - spacing j)) on m points of [1.5, 5.5].
Problem step_problem(std::size_t m, const std::vector<double> &xt, double eta,
                     double c0, double spacing) {
  Problem p;
  p.m = m;
  p.n = xt.size();
  p.A.resize(m * p.n);
  p.b.assign(m, 0.0);
  for (std::size_t i = 0; i < m; ++i) {
    const double r = 1.5 + 4.0 * i / (m - 1);
    for (std::size_t j = 0; j < p.n; ++j) {
      p.A[j * m + i] = SB5(eta * (r - (c0 + spacing * j)));
      p.b[i] += p.A[j * m + i] * xt[j];
    }
  }
  return p;
}

// |A x - b| / |b| (unweighted, no lambda), accumulated in long double.
double rel_residual(const Problem &p, const std::vector<double> &x) {
  long double rr = 0.0L, bb = 0.0L;
  for (std::size_t i = 0; i < p.m; ++i) {
    long double s = -static_cast<long double>(p.b[i]);
    for (std::size_t j = 0; j < p.n; ++j)
      s += p.a(i, j) * static_cast<long double>(x[j]);
    rr += s * s;
    bb += static_cast<long double>(p.b[i]) * p.b[i];
  }
  return std::sqrt(static_cast<double>(rr / bb));
}

std::vector<double> parse_list(const char *text) {
  std::istringstream is(text);
  std::vector<double> out;
  double v;
  while (is >> v) out.push_back(v);
  return out;
}

}  // namespace

TEST_CASE("nnls: matches brute-force active-set enumeration on random "
          "problems", "[nnls]") {
  std::mt19937_64 rng(42);
  std::uniform_real_distribution<double> U(0.0, 1.0);
  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t n = 2 + trial % 7;               // 2..8 columns
    const std::size_t m = trial % 5 == 0 ? n + 1 : 25;  // tall and near-square
    Problem p = random_problem(rng, m, n);
    if (trial % 3 == 1) {  // row weights, some zero
      p.w.resize(m);
      for (double &v : p.w) v = U(rng) < 0.1 ? 0.0 : 2.0 * U(rng);
    }
    if (trial % 4 == 2) {  // a masked-out column
      p.mask.assign(n, 1);
      p.mask[trial % n] = 0;
    }
    if (trial % 5 == 3) p.lambda = 0.3 * U(rng);
    INFO("trial " << trial << " m " << m << " n " << n);
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    const std::vector<long double> xb = brute_force(p);
    const long double f = objective(p, widen(x)), fb = objective(p, xb);
    CHECK(f <= fb * (1.0L + 1e-12L) + 1e-24L);
    for (std::size_t j = 0; j < n; ++j)
      CHECK(x[j] == Approx(static_cast<double>(xb[j])).margin(1e-9));
    check_kkt(p, x, res, 1e-12);
  }
}

TEST_CASE("nnls: recovers an exact non-negative solution, and the passive "
          "set is the brute-force support", "[nnls]") {
  std::mt19937_64 rng(7);
  Problem p = random_problem(rng, 40, 6);
  for (double &v : p.A) v = std::fabs(v);
  const std::vector<double> xt = {0.5, 0.0, 1.25, 0.0, 2.0, 0.75};
  for (std::size_t i = 0; i < p.m; ++i) {
    double s = 0.0;
    for (std::size_t j = 0; j < p.n; ++j) s += p.a(i, j) * xt[j];
    p.b[i] = s;
  }
  std::vector<double> x;
  NnlsResult res = solve(p, x);
  for (std::size_t j = 0; j < p.n; ++j)
    CHECK(x[j] == Approx(xt[j]).margin(1e-12));
  CHECK(res.converged);

  // push b off the exact fit: the support follows the true optimum
  for (std::size_t i = 0; i < p.m; ++i)
    p.b[i] -= 0.2 * (p.a(i, 1) + p.a(i, 3)) + 0.05 * std::cos(3.0 * i);
  res = solve(p, x);
  const std::vector<long double> xb = brute_force(p);
  std::vector<std::size_t> support;
  for (std::size_t j = 0; j < p.n; ++j)
    if (xb[j] > 0.0L) support.push_back(j);
  CHECK(res.passive == support);
  CHECK(support.size() < p.n);  // the bound is active
  check_kkt(p, x, res, 1e-12);
}

TEST_CASE("nnls: zero columns, masks, lambda rows, degenerate inputs",
          "[nnls]") {
  SECTION("an all-zero column (and one zero after the row weights) stays 0") {
    Problem p;
    p.m = 4;
    p.n = 3;
    p.A = {1, 2, 3, 4,  0, 0, 0, 0,  0, 0, 5, 0};
    p.b = {1, 2, 3, 4};
    p.w = {1, 1, 0, 1};  // row 2 carries column 2's only entry
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    CHECK(x[0] == Approx(1.0).epsilon(1e-14));
    CHECK(x[1] == 0.0);
    CHECK(x[2] == 0.0);
    CHECK(res.passive == std::vector<std::size_t>({0}));
  }
  SECTION("duplicate columns: the dependent copy is not admitted") {
    Problem p;
    p.m = 5;
    p.n = 3;
    p.A = {1, 2, 3, 4, 5,  1, 2, 3, 4, 5,  1, 0, 1, 0, 1};
    p.b = {2, 2, 4, 4, 6};
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    CHECK(x[0] + x[1] == Approx(1.0).epsilon(1e-12));
    CHECK(x[2] == Approx(1.0).epsilon(1e-12));
    CHECK(res.passive.size() == 2);
    CHECK(res.converged);
  }
  SECTION("lambda: ridge rows on the free columns") {
    Problem p;
    p.m = 3;
    p.n = 1;
    p.A = {1, 1, 1};
    p.b = {2, 2, 2};
    p.lambda = 3.0;  // min 3 (x - 2)^2 + 3 x^2 -> x = 1
    std::vector<double> x;
    solve(p, x);
    CHECK(x[0] == Approx(1.0).epsilon(1e-14));
  }
  SECTION("b == 0, no columns, and invalid arguments") {
    Problem p;
    p.m = 3;
    p.n = 2;
    p.A = {1, 2, 3, 4, 5, 6};
    p.b = {0, 0, 0};
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    CHECK(x == std::vector<double>({0.0, 0.0}));
    CHECK(res.passive.empty());
    CHECK(res.iterations == 0);
    double none = 0.0;
    REQUIRE_NOTHROW(nnls(nullptr, 0, 0, nullptr, &none));
    p.b = {1, 1, 1};
    CHECK_THROWS_AS(nnls(p.A.data(), 3, 2, p.b.data(), x.data(), nullptr,
                         nullptr, -1.0), std::invalid_argument);
    const double bad_w[3] = {1.0, -1.0, 1.0};
    CHECK_THROWS_AS(nnls(p.A.data(), 3, 2, p.b.data(), x.data(), bad_w),
                    std::invalid_argument);
    p.A[3] = std::nan("");
    CHECK_THROWS_AS(solve(p, x), std::invalid_argument);
  }
}

TEST_CASE("nnls: ill-conditioned integrated-bump (MONOTONE) columns",
          "[nnls]") {
  // Blip5 steps S_n(r) = SB5(s_n (r - c_n)) on a window of the campaign's
  // shape: every column is 1 below its inner edge, so the columns share a
  // common block (the normal equations square a condition number ~1e8).
  const std::size_t m = 400, n = 8;
  Problem p;
  p.m = m;
  p.n = n;
  p.A.resize(m * n);
  p.b.resize(m);
  const double r_lo = 1.7168, r_hi = 5.5;
  for (std::size_t i = 0; i < m; ++i) {
    const double r = r_lo + (r_hi - r_lo) * i / (m - 1);
    for (std::size_t j = 0; j < n; ++j) {
      const double c = 2.2 + 0.4 * j, s = 1.0 + 0.1 * j;
      p.A[j * m + i] = SB5(s * (r - c));
    }
    // a non-increasing target with a feature the steps cannot follow
    p.b[i] = std::exp(-1.3 * r) + 0.02 * std::exp(-40.0 * (r - 3.1) * (r - 3.1));
  }
  std::vector<double> x;
  const NnlsResult res = solve(p, x);
  check_kkt(p, x, res, 1e-9);
  const std::vector<long double> xb = brute_force(p);
  const long double f = objective(p, widen(x)), fb = objective(p, xb);
  CHECK(f <= fb * (1.0L + 1e-8L));
  CHECK(!res.passive.empty());
  // some steps are at their bound: the positivity constraint is active
  CHECK(res.passive.size() < n);
}

TEST_CASE("nnls: exact fits of ill-conditioned step columns reach the "
          "rounding floor", "[nnls]") {
  // b = A x* with x* >= 0, so the optimum residual is rounding. A dual
  // formed from b - A x flips sign under the forward error of x and stopped
  // these fits at |r|/|b| ~ 1e-9 while reporting converged.
  SECTION("18 steps at spacing 0.2, eta 0.8 (cond 1.2e8)") {
    // a case that stopped at |r|/|b| ~ 1e-9 under the old dual, rebuilt bit
    // for bit: 12 of 18 columns active
    const std::vector<double> xt = {
        0x1.a20d5544c577bp-1, 0.0, 0x1.eb9b2d75b7ad7p-1, 0x1.4ed7546ef8d36p-1,
        0x1.94ef526749602p-3, 0.0, 0x1.e2ac13d7f93d8p-1, 0.0,
        0x1.48ece44b69295p-1, 0x1.b6185d28af758p-1, 0x1.08a967d0c2414p+0,
        0x1.3c502f2c29acbp-1, 0x1.d5073e54e39f7p-1, 0.0, 0x1.a31a9d9291626p-1,
        0.0, 0x1.e349f00eb2673p-1, 0.0};
    const Problem p = step_problem(75, xt, 0.8, 1.8, 0.2);
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    CHECK(res.converged);
    CHECK(rel_residual(p, x) <= 1e-12);
    // The optimum x* is unique, but x is determined only to its forward
    // error (cond eps |x*| ~ 1e-8): the true support must be passive, while
    // a zero column may hold a coefficient of that size.
    for (std::size_t j = 0; j < p.n; ++j) {
      INFO("column " << j);
      CHECK(std::fabs(x[j] - xt[j]) <= 1e-7);
      if (xt[j] > 0.0) CHECK(x[j] > 0.0);
    }
    check_kkt(p, x, res, 1e-9);
  }
  SECTION("200 layouts: eta 0.5 to 2, spacing 0.05 to 0.35 (cond up to "
          "1e13)") {
    std::mt19937_64 rng(2026);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    for (int trial = 0; trial < 200; ++trial) {
      const std::size_t m = 40 + trial % 80, n = 8 + trial % 13;
      const double eta = 0.5 + 1.5 * U(rng), spacing = 0.05 + 0.3 * U(rng);
      std::vector<double> xt(n, 0.0);
      for (double &v : xt)
        if (U(rng) < 0.6) v = 0.1 + U(rng);
      const Problem p = step_problem(m, xt, eta, 1.8, spacing);
      INFO("trial " << trial << " m " << m << " n " << n << " eta " << eta
                    << " spacing " << spacing);
      std::vector<double> x;
      const NnlsResult res = solve(p, x);
      CHECK(res.converged);
      CHECK(rel_residual(p, x) <= 1e-12);
    }
  }
}

TEST_CASE("nnls: the theta_star_ON step layout of the FEAT-02 campaign "
          "(cond 5e7)", "[nnls]") {
  // The 20 Blip5 bumps of theta_star_ON (ream_campaign_fixtures.h) as
  // integrated-bump columns S_n(r) = SB5(s_n (r - c_n)) on the scan window
  // [1.7168, RCUTMB], 2001 points: cond 5.0e7, with the near-duplicate
  // centres 3.5873 / 3.5945 / 3.5946 (columns 2, 5 and 7).
  const ream_campaign::Fixture &fx = ream_campaign::kFixtures[0];
  REQUIRE(std::strcmp(fx.name, "theta_star_ON") == 0);
  const std::vector<double> c = parse_list(fx.cgrid);
  const std::vector<double> s = parse_list(fx.sgrid);
  REQUIRE(c.size() == 20);
  REQUIRE(s.size() == 20);
  const double r_lo = 1.7168, r_hi = std::stod(fx.rcutmb);
  const std::size_t m = 2001;
  // positive and non-increasing, with a shoulder the steps cannot follow
  const auto columns = [&](const std::vector<std::size_t> &cols) {
    Problem p;
    p.m = m;
    p.n = cols.size();
    p.A.resize(m * p.n);
    p.b.resize(m);
    for (std::size_t i = 0; i < m; ++i) {
      const double r = r_lo + (r_hi - r_lo) * i / (m - 1);
      for (std::size_t j = 0; j < p.n; ++j)
        p.A[j * m + i] = SB5(s[cols[j]] * (r - c[cols[j]]));
      p.b[i] = std::exp(-1.3 * (r - r_lo)) * 0.5 * std::erfc(3.0 * (r - 5.0)) +
               0.002 * std::exp(-30.0 * (r - 2.9) * (r - 2.9));
    }
    return p;
  };

  SECTION("all 20 steps: a Kuhn-Tucker point with active bounds") {
    std::vector<std::size_t> all(20);
    for (std::size_t j = 0; j < all.size(); ++j) all[j] = j;
    const Problem p = columns(all);
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    check_kkt(p, x, res, 1e-9);
    CHECK(!res.passive.empty());
    CHECK(res.passive.size() < p.n);
  }
  SECTION("10 steps including the near-duplicate centres: brute force") {
    const Problem p = columns({0, 1, 2, 4, 5, 6, 7, 10, 12, 17});
    std::vector<double> x;
    const NnlsResult res = solve(p, x);
    check_kkt(p, x, res, 1e-9);
    const std::vector<long double> xb = brute_force(p);
    const long double f = objective(p, widen(x)), fb = objective(p, xb);
    CHECK(f <= fb * (1.0L + 1e-10L));
    std::vector<std::size_t> support;
    for (std::size_t j = 0; j < p.n; ++j)
      if (xb[j] > 0.0L) support.push_back(j);
    CHECK(res.passive == support);
  }
}

TEST_CASE("nnls: a stop because every column with a dual above the tolerance "
          "was refused is not convergence",
          "[nnls]") {
  // a1 = (1, 0), a2 = (1 - d, d), b = (1, 1/2), d = 60 eps. a1 enters first
  // (its dual 1 is the larger), x = (1, 0) and r = (0, 1/2). a2's dual is then
  // d / 2 = 30 eps, above the tolerance 10 eps |M|_F |r| ~ 7.1 eps, but its
  // component off a1 is d < 100 eps |a2|: a2 is refused as numerically
  // dependent, and no other column can enter. x is feasible but not a
  // Kuhn-Tucker point (the face x1 = 0 is lower by about d), so the solver
  // must not report convergence.
  const double eps = std::numeric_limits<double>::epsilon();
  const double d = 60.0 * eps;
  Problem p;
  p.m = 2;
  p.n = 2;
  p.A = {1.0, 0.0, 1.0 - d, d};
  p.b = {1.0, 0.5};
  std::vector<double> x;
  const NnlsResult res = solve(p, x);
  CHECK(x[0] == 1.0);
  CHECK(x[1] == 0.0);
  CHECK(res.passive == std::vector<std::size_t>({0}));
  CHECK_FALSE(res.converged);

  // the same system with a2 a clear column: a Kuhn-Tucker point, converged
  Problem q = p;
  q.A = {1.0, 0.0, 0.5, 0.5};
  const NnlsResult ok = solve(q, x);
  CHECK(ok.converged);
}
