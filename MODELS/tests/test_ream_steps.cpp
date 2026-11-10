/* FEAT-02: the integrated-bump step functions of RHOFORM MONOTONE
 * (d_basis_functions.h SB/SB5/SG and the basis_dispatch.h radial_step_*).
 *
 * S(x) = (integral of the bump from x outwards) / (integral over the whole
 * support). Checked against Gauss-Legendre quadrature of the bump itself
 * (exact for the polynomial pieces of B and B5), S(c) = 1/2, dS/dr and
 * d2S/dr2 against central differences, continuity at the knots, S in [0, 1]
 * and non-increasing to one ulp, and the fused Blip5 path bitwise against
 * the separate value and derivative.
 */
#include "catch2/catch.hpp"

#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/d_basis_functions.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

using namespace tadah::models;

namespace {

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

// 8-point Gauss-Legendre on [lo, hi] (exact for degree <= 15).
double gl8(const std::function<double(double)> &f, double lo, double hi) {
  static const double x[4] = {0.1834346424956498, 0.5255324099163290,
                              0.7966664774136267, 0.9602898564975363};
  static const double w[4] = {0.3626837833783620, 0.3137066458778873,
                              0.2223810344533745, 0.1012285362903763};
  const double h = 0.5 * (hi - lo), m = 0.5 * (hi + lo);
  double s = 0.0;
  for (int k = 0; k < 4; ++k) s += w[k] * (f(m - h * x[k]) + f(m + h * x[k]));
  return h * s;
}

// integral of a piecewise polynomial bump over [lo, hi], split at the
// integer knots so every piece is integrated exactly
double integrate_pieces(const std::function<double(double)> &f, double lo,
                        double hi) {
  double s = 0.0;
  double a = lo;
  while (a < hi) {
    double b = std::floor(a) + 1.0;
    if (b > hi) b = hi;
    s += gl8(f, a, b);
    a = b;
  }
  return s;
}

struct Family {
  const char *name;
  double w;  // support half-width in x
  std::function<double(double)> bump;
  std::function<double(double)> step;
};

const std::vector<Family> &families() {
  static const std::vector<Family> f = {
      {"Blip", 2.0, [](double x) { return B(x); },
       [](double x) { return SB(x); }},
      {"Blip5", 3.0, [](double x) { return B5(x); },
       [](double x) { return SB5(x); }},
  };
  return f;
}

}  // namespace

TEST_CASE("RHOFORM steps: S = (integral of the bump from x outwards) / "
          "area, against exact quadrature", "[ream][rhoform][steps]") {
  for (const auto &fam : families()) {
    INFO(fam.name);
    const double area = integrate_pieces(fam.bump, -fam.w, fam.w);
    REQUIRE(area == Approx(fam.w == 2.0 ? 1.5 : 20.0 / 11.0).epsilon(1e-15));
    double worst = 0.0;
    for (int k = -400; k <= 400; ++k) {
      const double x = fam.w * (k / 400.0) * 1.05;  // beyond both edges too
      const double lo = std::fmax(x, -fam.w);
      const double q = x >= fam.w ? 0.0
                                  : integrate_pieces(fam.bump, lo, fam.w) / area;
      worst = std::fmax(worst, std::fabs(fam.step(x) - q));
    }
    CHECK(worst < 2e-15);
  }
}

TEST_CASE("RHOFORM steps: S(c) = 1/2 exactly, 1 inside, 0 beyond the "
          "support", "[ream][rhoform][steps]") {
  CHECK(SB(0.0) == 0.5);
  CHECK(SB(-0.0) == 0.5);
  CHECK(SB5(0.0) == 0.5);
  CHECK(SB5(-0.0) == 0.5);
  CHECK(SG(2.5, 1.7, 2.5) == 0.5);
  CHECK(radial_step_value_bare(BasisKind::Blip5, 3.1, 1.3, 3.1) == 0.5);
  for (double x : {-2.0, -2.5, -1e3}) CHECK(SB(x) == 1.0);
  for (double x : {2.0, 2.5, 1e3}) CHECK(SB(x) == 0.0);
  for (double x : {-3.0, -3.5, -1e3}) CHECK(SB5(x) == 1.0);
  for (double x : {3.0, 3.5, 1e3}) CHECK(SB5(x) == 0.0);
  // the Gaussian step has no compact support; erfc keeps the tail exact
  CHECK(SG(20.0, 1.0, 0.0) > 0.0);
  CHECK(SG(20.0, 1.0, 0.0) == Approx(0.5 * std::erfc(20.0)).epsilon(1e-15));
  CHECK(SG(-20.0, 1.0, 0.0) == 1.0);
}

TEST_CASE("RHOFORM steps: the Gaussian step against quadrature of "
          "sqrt(eta/pi) exp(-eta (r-c)^2)", "[ream][rhoform][steps]") {
  const double eta = 2.3, c = 1.4;
  const auto g = [&](double r) {
    return std::sqrt(eta / 3.14159265358979323846) *
           std::exp(-eta * (r - c) * (r - c));
  };
  for (double r : {0.2, 0.9, 1.4, 1.7, 2.5, 3.3}) {
    double q = 0.0;
    const double hi = c + 12.0 / std::sqrt(eta);
    const int nseg = 400;
    for (int s = 0; s < nseg; ++s)
      q += gl8(g, r + (hi - r) * s / nseg, r + (hi - r) * (s + 1) / nseg);
    INFO("r = " << r);
    CHECK(SG(r, eta, c) == Approx(q).epsilon(1e-13));
  }
}

TEST_CASE("RHOFORM steps: dS/dr and d2S/dr2 against central differences",
          "[ream][rhoform][steps]") {
  const double h = 1e-6;
  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian}) {
    INFO(basis_name(k));
    const double s = 1.3, c = 2.9;
    for (int i = 0; i <= 120; ++i) {
      const double r = 0.3 + 5.2 * i / 120.0;
      INFO("r = " << r);
      const double fd = (radial_step_value_bare(k, r + h, s, c) -
                         radial_step_value_bare(k, r - h, s, c)) / (2 * h);
      CHECK(radial_step_deriv_bare(k, r, s, c) == Approx(fd).margin(1e-8));
      const double fd2 = (radial_step_deriv_bare(k, r + h, s, c) -
                          radial_step_deriv_bare(k, r - h, s, c)) / (2 * h);
      CHECK(radial_step_deriv2_bare(k, r, s, c) == Approx(fd2).margin(1e-6));
      // the step decreases: dS/dr = -eta bump / area <= 0
      CHECK(radial_step_deriv_bare(k, r, s, c) <= 0.0);
    }
    // cutoff-weighted: d/dr [S f_c] = f_c dS + f_c' S
    const double fcv = 0.7, fcp = -0.4, r = 2.2;
    CHECK(radial_step_deriv(k, r, fcv, fcp, s, c) ==
          fcv * radial_step_deriv_bare(k, r, s, c) +
              fcp * radial_step_value_bare(k, r, s, c));
    CHECK(radial_step_value(k, r, fcv, s, c) ==
          radial_step_value_bare(k, r, s, c) * fcv);
  }
}

TEST_CASE("RHOFORM steps: continuous at the knots, in [0, 1], non-increasing "
          "to one ulp", "[ream][rhoform][steps]") {
  const double ulp = std::numeric_limits<double>::epsilon();
  for (const auto &fam : families()) {
    INFO(fam.name);
    for (double knot = -fam.w; knot <= fam.w; knot += 1.0) {
      const double lo = std::nextafter(knot, -1e9);
      const double hi = std::nextafter(knot, 1e9);
      CHECK(std::fabs(fam.step(lo) - fam.step(knot)) < 4 * ulp);
      CHECK(std::fabs(fam.step(hi) - fam.step(knot)) < 4 * ulp);
    }
    double prev = fam.step(-fam.w - 0.5);
    double max_rise = 0.0;
    for (int i = 1; i <= 200000; ++i) {
      const double x = -fam.w - 0.5 + (2 * fam.w + 1.0) * i / 200000.0;
      const double v = fam.step(x);
      REQUIRE(v >= 0.0);
      REQUIRE(v <= 1.0);
      max_rise = std::fmax(max_rise, v - prev);
      prev = v;
    }
    CHECK(max_rise <= ulp);
  }
}

TEST_CASE("RHOFORM steps: the fused Blip5 value+derivative is bitwise the "
          "separate evaluation", "[ream][rhoform][steps]") {
  std::vector<double> xs = {0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 3.0, -3.0, 3.5,
                            -3.5, 1e-300, -1e-300};
  for (int i = -40000; i <= 40000; ++i) xs.push_back(3.2 * i / 40000.0 + 1e-7);
  for (double eta : {0.7, 1.0, 11.65}) {
    std::size_t bad = 0;
    for (double x : xs) {
      double v, d;
      SB5_value_deriv(x, eta, v, d);
      if (bits(v) != bits(SB5(x)) || bits(d) != bits(dSB5(x, eta))) ++bad;
    }
    CHECK(bad == 0);
  }
  // through the dispatcher, every step family, with a cutoff
  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian}) {
    std::size_t bad = 0;
    for (int i = 0; i <= 2000; ++i) {
      const double r = 0.1 + 6.0 * i / 2000.0;
      const double fcv = 0.5 + 0.5 * std::cos(r), fcp = -0.5 * std::sin(r);
      double v, d;
      radial_step_value_deriv(k, r, fcv, fcp, 1.7, 3.0, v, d);
      if (bits(v) != bits(radial_step_value(k, r, fcv, 1.7, 3.0)) ||
          bits(d) != bits(radial_step_deriv(k, r, fcv, fcp, 1.7, 3.0)))
        ++bad;
    }
    INFO(basis_name(k));
    CHECK(bad == 0);
  }
}

TEST_CASE("RHOFORM steps: the knot family has no step form",
          "[ream][rhoform][steps]") {
  using Catch::Matchers::Contains;
  REQUIRE_THROWS_WITH(radial_step_value(BasisKind::Knot5, 1.0, 1.0, 1.0, 2.0),
                      Contains("no integrated-bump step form"));
  REQUIRE_THROWS(radial_step_deriv(BasisKind::Knot5, 1.0, 1.0, 0.0, 1.0, 2.0));
  REQUIRE_THROWS(radial_step_value_bare(BasisKind::Knot5, 1.0, 1.0, 2.0));
  REQUIRE_THROWS(radial_step_deriv_bare(BasisKind::Knot5, 1.0, 1.0, 2.0));
  REQUIRE_THROWS(radial_step_deriv2_bare(BasisKind::Knot5, 1.0, 1.0, 2.0));
}
