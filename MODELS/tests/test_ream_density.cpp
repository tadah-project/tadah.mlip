/* FEAT-02: the DM_REAM density forms (RHOFORM SIGNED | NONNEG | MONOTONE).
 *
 *  1. ReamDensity scans against its point evaluator: scan_grid is bitwise
 *     value() point by point, rhobar_scan is the shell sum of value().
 *  2. DM_REAM delegates: pair_density and eam_density equal calc_rho's
 *     increment bitwise, for every form and family; value_deriv equals
 *     value/deriv bitwise.
 *  3. The guarantees, with tolerance: NONNEG rho >= 0; MONOTONE rho >= 0,
 *     non-increasing, and rhobar(a) non-increasing on bcc/fcc/hcp.
 *  4. Validation messages.
 *  5. The rhoform.h helpers (parse, stored/require, normalise, precedence,
 *     cutoff whitelist, OPTIM lower bound) and the key contract (an absent
 *     RHOFORM is never materialised).
 *  6. eam_lattice::shells multiplicities; D_Base WATOM parsing.
 */
#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/eam/eam_lattice.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;
using Catch::Matchers::Contains;

namespace {

struct DensityFixture {
  DensityFixture() { PeriodicTable::initialize(); }
};

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

std::string join(const std::vector<double> &v) {
  std::ostringstream os;
  os.precision(17);
  for (std::size_t i = 0; i < v.size(); ++i) os << (i ? " " : "") << v[i];
  return os.str();
}

// A single-species (Ta) DM_REAM density layout.
struct Layout {
  BasisKind basis;
  std::vector<double> c, s, a;
  std::vector<double> deg;  // KDEGMB (knot family only; empty = all 5)
};

const double kRcut = 5.5;

// Mixed-sign amplitudes; supports inside and spilling past kRcut.
Layout layout(BasisKind k, bool nonneg) {
  Layout L;
  L.basis = k;
  switch (k) {
    case BasisKind::Blip:
      L.c = {1.5, 2.2, 2.9, 3.6, 4.3, 4.8};
      L.s = {1.3, 1.1, 1.7, 0.9, 1.5, 2.1};
      break;
    case BasisKind::Blip5:
      L.c = {1.2, 2.0, 2.8, 3.5, 4.1, 4.6};
      L.s = {1.4, 1.2, 1.9, 1.1, 1.6, 2.4};
      break;
    case BasisKind::Gaussian:
      L.c = {1.0, 1.8, 2.6, 3.4, 4.2, 5.0};
      L.s = {2.0, 1.5, 3.0, 1.2, 2.5, 4.0};
      break;
    case BasisKind::Knot5:
      L.c = {2.5, 3.1, 3.7, 4.2, 4.9, 5.5};
      L.deg = {5, 3, 5, 3, 5, 5};
      break;
  }
  L.a = {0.9, -0.35, 0.6, -0.2, 0.15, -0.05};
  if (k == BasisKind::Knot5)
    for (double &x : L.a) x *= 0.1;
  if (nonneg)
    for (double &x : L.a) x = std::fabs(x);
  return L;
}

Context ream_ctx(const Layout &L, const std::string &form,
                 const std::string &cutoff, double watom = 0.8125) {
  const bool knot = L.basis == BasisKind::Knot5;
  const int n = static_cast<int>(L.c.size());
  Context c;
  c.add("VERBOSE", 0);
  c.add("ATOM", "Ta");
  c.add("WATOM", watom);
  c.add("INITMB", "true");
  c.add("RCUTMB", kRcut);
  c.add("RCTYPEMB", cutoff);
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", basis_code(L.basis));
  c.add("TYPEMB", n); c.add("TYPEMB", knot ? 0 : n); c.add("TYPEMB", n);
  c.add("TYPEMB", 3); c.add("TYPEMB", knot ? 0 : 3);
  c.add("TYPEMB", knot ? n : 0); c.add("TYPEMB", 0);
  c.add("TYPEMB", "Ta");
  c.add("TYPEMB", "Ta");
  c.add("CGRIDMB", join(L.c));
  if (!knot) c.add("SGRIDMB", join(L.s));
  c.add("AMPGRIDMB", join(L.a));
  if (knot) c.add("KDEGMB", join(L.deg));
  c.add("CEMBFUNC", "0.0 0.5 1.0");
  if (!knot) c.add("SEMBFUNC", "1.0 1.0 1.0");
  if (!form.empty()) c.add("RHOFORM", form);
  return c;
}

std::unique_ptr<Cut_Base> make_cut(const std::string &label, double rc) {
  double r = rc;
  return std::unique_ptr<Cut_Base>(factory<Cut_Base, double &>(label, r));
}

const std::vector<std::string> kCuts = {"Cut_Dummy", "Cut_Cos", "Cut_Poly",
                                        "Cut_PolyS", "Cut_Tanh", "Cut_PT",
                                        "Cut_CosS"};

struct FormCase {
  BasisKind basis;
  std::string form;
  bool nonneg;
};

// Every (basis, form) combination that validates.
std::vector<FormCase> form_cases() {
  std::vector<FormCase> out;
  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian,
                      BasisKind::Knot5}) {
    out.push_back({k, "SIGNED", false});
    out.push_back({k, "SIGNED", true});
    out.push_back({k, "NONNEG", true});
    if (k != BasisKind::Knot5) out.push_back({k, "MONOTONE", true});
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
//  1. scans against the point evaluator
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(DensityFixture,
                 "ReamDensity::scan_grid is bitwise value() point by point",
                 "[ream][rhoform][density]") {
  for (const FormCase &fc : form_cases()) {
    for (const std::string &cut : {std::string("Cut_Dummy"),
                                   std::string("Cut_Poly"),
                                   std::string("Cut_Cos")}) {
      INFO(basis_name(fc.basis) << " " << fc.form << " " << cut
                                << (fc.nonneg ? " a>=0" : " signed a"));
      const Context ctx = ream_ctx(layout(fc.basis, fc.nonneg), fc.form, cut);
      const ReamDensity d = ReamDensity::from_context(ctx);
      auto fcut = make_cut(cut, kRcut);
      REQUIRE_NOTHROW(d.validate(fcut.get()));
      // windows inside, ending at and reaching past the cutoff
      for (const auto &win : {std::pair<double, double>{0.4, kRcut},
                              std::pair<double, double>{1.7168, 4.3},
                              std::pair<double, double>{0.0, 6.2}}) {
        const std::size_t n = 4001;
        std::vector<double> rho, tabs;
        const DensityScan sc =
            d.scan_grid(win.first, win.second, n, *fcut, &rho, &tabs);
        const std::vector<double> r =
            ReamDensity::linspace(win.first, win.second, n);
        std::size_t bad = 0;
        double mn = std::numeric_limits<double>::infinity(), mx = -mn;
        std::size_t imn = 0, imx = 0;
        for (std::size_t i = 0; i < n; ++i) {
          const double want = r[i] > kRcut ? 0.0 : d.value(r[i], fcut->calc(r[i]));
          if (bits(rho[i]) != bits(want)) ++bad;
          if (rho[i] < mn) { mn = rho[i]; imn = i; }
          if (rho[i] > mx) { mx = rho[i]; imx = i; }
          REQUIRE(tabs[i] >= std::fabs(rho[i]) * (1.0 - 1e-15));
        }
        CHECK(bad == 0);
        CHECK(sc.finite);
        CHECK(bits(sc.rho_min) == bits(mn));
        CHECK(bits(sc.rho_max) == bits(mx));
        CHECK(sc.i_min == imn);
        CHECK(sc.i_max == imx);
        CHECK(sc.r_at_min == r[imn]);
        CHECK(sc.r_at_max == r[imx]);
        // the same summary without the optional outputs
        const DensityScan sc2 = d.scan_grid(win.first, win.second, n, *fcut);
        CHECK(bits(sc2.rho_min) == bits(sc.rho_min));
        CHECK(sc2.i_max == sc.i_max);
      }
    }
  }
}

TEST_CASE("ReamDensity::linspace is numpy.linspace, and scan_grid argument "
          "checks", "[ream][rhoform][density]") {
  const auto x = ReamDensity::linspace(1.7168, 5.58190742024, 20001);
  REQUIRE(x.size() == 20001);
  const double step = (5.58190742024 - 1.7168) / 20000.0;
  CHECK(x[0] == 1.7168);
  CHECK(x[12345] == 12345.0 * step + 1.7168);
  CHECK(x.back() == 5.58190742024);
  CHECK(ReamDensity::linspace(1.0, 2.0, 1) == std::vector<double>{1.0});
  CHECK(ReamDensity::linspace(1.0, 2.0, 0).empty());

  ReamDensity d;
  d.crho = {1.0};
  d.srho = {1.0};
  d.arho = {1.0};
  Cut_Dummy cut(3.0);
  CHECK_THROWS_AS(d.scan_grid(1.0, 2.0, 1, cut), std::invalid_argument);
  CHECK_THROWS_AS(d.scan_grid(2.0, 1.0, 10, cut), std::invalid_argument);
  CHECK_THROWS_AS(d.scan_grid(1.0, std::nan(""), 10, cut),
                  std::invalid_argument);
  // ties go to the first index: rho == 0 beyond the only bump
  const DensityScan sc = d.scan_grid(3.5, 4.5, 11, cut);
  CHECK(sc.rho_min == 0.0);
  CHECK(sc.i_min == 0);
  CHECK(sc.i_max == 0);
}

TEST_CASE("ReamDensity::scan_grid is bitwise value() for a non-finite "
          "SIGNED amplitude too", "[ream][rhoform][density]") {
  // SIGNED accepts any amplitude, as tadah did before 1.6.0. An infinite a_n makes
  // its term a_n * 0 = NaN outside the bump as well, so the scan must not
  // skip it there (rho = 0.082 was found where value() is NaN).
  ReamDensity d;
  d.basis = BasisKind::Blip5;
  d.form = RhoForm::Signed;
  d.crho = {1.0, 3.0};
  d.srho = {4.0, 1.0};  // bump 1 ends at r = 1.75, left of the window
  d.arho = {std::numeric_limits<double>::infinity(), 0.5};
  Cut_Cos cut(5.0);
  const std::size_t n = 11;
  std::vector<double> rho;
  const DensityScan sc = d.scan_grid(2.0, 5.0, n, cut, &rho);
  const std::vector<double> r = ReamDensity::linspace(2.0, 5.0, n);
  for (std::size_t i = 0; i < n; ++i) {
    INFO("r = " << r[i]);
    CHECK(bits(rho[i]) == bits(d.value(r[i], cut.calc(r[i]))));
  }
  CHECK_FALSE(sc.finite);
}

TEST_CASE_METHOD(DensityFixture,
                 "ReamDensity::rhobar_scan is the ascending shell sum of "
                 "value()", "[ream][rhoform][density]") {
  const Layout L = layout(BasisKind::Blip5, false);
  const Context ctx = ream_ctx(L, "SIGNED", "Cut_Dummy");
  const ReamDensity d = ReamDensity::from_context(ctx);
  Cut_Dummy cut(kRcut);
  const double a0 = 3.3;
  const auto sh = eam_lattice::shells("bcc", kRcut / (0.8 * a0) * (1 + 1e-12));
  std::vector<double> a = ReamDensity::linspace(0.8, 1.1, 301);
  for (double &x : a) x *= a0;
  const RhoBarScan rb = d.rhobar_scan(sh, a, cut);
  REQUIRE(rb.rhobar.size() == a.size());
  std::size_t rises = 0, first = 0;
  for (std::size_t k = 0; k < a.size(); ++k) {
    double want = 0.0;
    for (const auto &s : sh) {
      const double r = s.d * a[k];
      if (r > kRcut) continue;
      want += static_cast<double>(s.m) * d.value(r, cut.calc(r));
    }
    REQUIRE(bits(rb.rhobar[k]) == bits(want));
    if (k > 0 && rb.rhobar[k] >= rb.rhobar[k - 1]) {
      if (rises == 0) first = k - 1;
      ++rises;
    }
  }
  CHECK(rb.rises == rises);
  if (rises) CHECK(rb.first_rise == first);
  CHECK(rb.finite);
}

// ---------------------------------------------------------------------------
//  2. DM_REAM delegation
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(DensityFixture,
                 "DM_REAM: pair_density and eam_density are calc_rho's "
                 "increment bitwise, every form and family",
                 "[ream][rhoform][density]") {
  const int Z = PeriodicTable::find_by_symbol("Ta").Z;
  const Vec3d unit(1.0, 0.0, 0.0);
  for (const FormCase &fc : form_cases()) {
    for (const std::string &cut : kCuts) {
      INFO(basis_name(fc.basis) << " " << fc.form << " " << cut);
      Context ctx = ream_ctx(layout(fc.basis, fc.nonneg), fc.form, cut);
      DM_REAM dm(ctx);
      const ReamDensity &d = dm.density();
      std::size_t bad = 0;
      for (int i = 1; i <= 600; ++i) {
        const double r = kRcut * i / 600.0 * 1.02;  // a few beyond rcut
        rho_type rho;
        dm.init_rhoi(rho);
        rho.set_zero();
        dm.calc_rho(Z, Z, r, r * r, unit, rho, 1.0);
        const double inc = rho(dm.get_rfidx());
        double er, erp;
        dm.eam_density(Z, Z, r, er, erp);
        if (bits(dm.pair_density(Z, Z, r)) != bits(inc)) ++bad;
        if (bits(er) != bits(inc)) ++bad;
        // value_deriv's pieces are value() and deriv() bitwise
        auto fcut = make_cut(cut, kRcut);
        const double fv = fcut->calc(r), fp = fcut->calc_prime(r);
        double v, dd;
        d.value_deriv(r, fv, fp, v, dd);
        if (bits(v) != bits(d.value(r, fv))) ++bad;
        if (bits(dd) != bits(d.deriv(r, fv, fp))) ++bad;
      }
      CHECK(bad == 0);
      CHECK(dm.pair_density(Z, Z, kRcut * 1.01) == 0.0);
    }
  }
}

TEST_CASE_METHOD(DensityFixture,
                 "DM_REAM MONOTONE: forces match finite differences of the "
                 "density", "[ream][rhoform][density]") {
  const int Z = PeriodicTable::find_by_symbol("Ta").Z;
  for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian}) {
    for (const std::string &cut : {std::string("Cut_Cos"),
                                   std::string("Cut_Poly")}) {
      INFO(basis_name(k) << " " << cut);
      Context ctx = ream_ctx(layout(k, true), "MONOTONE", cut);
      DM_REAM dm(ctx);
      const double h = 1e-6;
      for (double r : {0.7, 1.9, 2.8, 3.3, 4.4, 5.2}) {
        double rp, rpp, rm, rmm, rho, rhop;
        dm.eam_density(Z, Z, r + h, rp, rpp);
        dm.eam_density(Z, Z, r - h, rm, rmm);
        dm.eam_density(Z, Z, r, rho, rhop);
        CHECK(rhop == Approx((rp - rm) / (2 * h)).margin(1e-8));
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  3. the guarantees
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(DensityFixture,
                 "RHOFORM guarantees on random non-negative amplitudes: "
                 "NONNEG rho >= 0, MONOTONE rho >= 0 and non-increasing",
                 "[ream][rhoform][density]") {
  std::mt19937_64 rng(20260928);
  std::uniform_real_distribution<double> U(0.0, 1.0);
  const double eps = std::numeric_limits<double>::epsilon();
  for (int draw = 0; draw < 40; ++draw) {
    for (BasisKind k : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian,
                        BasisKind::Knot5}) {
      Layout L;
      L.basis = k;
      const int n = 3 + static_cast<int>(U(rng) * 18);
      for (int j = 0; j < n; ++j) {
        L.c.push_back(k == BasisKind::Knot5 ? 1.0 + 4.5 * U(rng)
                                            : 0.5 + 5.5 * U(rng));
        if (k != BasisKind::Knot5) L.s.push_back(0.6 + 12.0 * U(rng));
        // exact zeros are common in NNLS fits
        L.a.push_back(U(rng) < 0.15 ? 0.0 : std::pow(10.0, -6.0 * U(rng)));
      }
      for (const std::string &form : {std::string("NONNEG"),
                                      std::string("MONOTONE")}) {
        if (form == "MONOTONE" && k == BasisKind::Knot5) continue;
        for (const std::string &cut : kCuts) {
          INFO("draw " << draw << " " << basis_name(k) << " " << form << " "
                       << cut);
          const Context ctx = ream_ctx(L, form, cut);
          const ReamDensity d = ReamDensity::from_context(ctx);
          auto fcut = make_cut(cut, kRcut);
          REQUIRE_NOTHROW(d.validate(fcut.get()));
          std::vector<double> rho, tabs;
          const DensityScan sc =
              d.scan_grid(0.0, kRcut, 20001, *fcut, &rho, &tabs);
          CHECK(sc.finite);
          CHECK(sc.rho_min >= 0.0);
          if (form != "MONOTONE") continue;
          double scale = 0.0, rise = 0.0;
          for (std::size_t i = 0; i < rho.size(); ++i) {
            scale = std::fmax(scale, tabs[i]);
            if (i) rise = std::fmax(rise, rho[i] - rho[i - 1]);
          }
          CHECK(rise <= 8.0 * eps * scale);
          // rho' <= 0 to rounding (f_c' of Cut_Poly is positive at the
          // 1e-15 level just below rcut)
          for (int i = 0; i <= 200; ++i) {
            const double r = kRcut * i / 200.0;
            const double dv = d.deriv(r, fcut->calc(r), fcut->calc_prime(r));
            REQUIRE(dv <= 64.0 * eps * (scale + 1.0) * 10.0);
          }
          // the lattice density rhobar(a) is non-increasing on every
          // lattice, to the rounding of its M = sum_s m_s terms
          for (const char *lat : {"bcc", "fcc", "hcp"}) {
            std::vector<double> a = ReamDensity::linspace(1.5, 6.0, 301);
            const auto sh = eam_lattice::shells(lat, kRcut / 1.5 * (1 + 1e-12));
            double M = 0.0;
            for (const auto &x : sh) M += x.m;
            const RhoBarScan rb = d.rhobar_scan(sh, a, *fcut);
            double bmax = 0.0, brise = 0.0;
            for (std::size_t q = 0; q < rb.rhobar.size(); ++q) {
              bmax = std::fmax(bmax, std::fabs(rb.rhobar[q]));
              if (q) brise = std::fmax(brise, rb.rhobar[q] - rb.rhobar[q - 1]);
            }
            INFO(lat);
            CHECK(brise <= 16.0 * eps * (M * scale + bmax));
          }
        }
      }
    }
  }
}

TEST_CASE_METHOD(DensityFixture,
                 "RHOFORM NONNEG/MONOTONE clamp a negative cutoff value at 0, "
                 "SIGNED does not", "[ream][rhoform][density]") {
  ReamDensity d;
  d.basis = BasisKind::Blip5;
  d.crho = {3.0};
  d.srho = {1.0};
  d.arho = {2.0};
  d.form = RhoForm::Signed;
  CHECK(d.value(3.0, -1e-15) == 2.0 * (B5(0.0) * -1e-15));
  CHECK(d.value(3.0, -1e-15) < 0.0);
  d.form = RhoForm::NonNeg;
  CHECK(d.value(3.0, -1e-15) == 0.0);
  CHECK(d.value(3.0, 0.25) == 2.0 * (B5(0.0) * 0.25));
  d.form = RhoForm::Monotone;
  CHECK(d.value(3.0, -1e-15) == 0.0);
  CHECK(d.value(3.0, 0.25) == 2.0 * (0.5 * 0.25));
  CHECK(d.value(0.0, 1.0) == 2.0);  // S = 1 from the inner edge inwards
  // f_c' is never clamped: rho' = a (S' max(f_c, 0) + S f_c')
  CHECK(d.deriv(0.0, -1e-15, -0.5) == -1.0);
}

// ---------------------------------------------------------------------------
//  4. validation
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(DensityFixture, "DM_REAM: RHOFORM is mandatory",
                 "[ream][rhoform][validation]") {
  Context c = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Dummy");
  REQUIRE_THROWS_AS(DM_REAM(c), DensityFormError);
  REQUIRE_THROWS_WITH(DM_REAM(c),
                      Contains("DM_REAM: RHOFORM is required: SIGNED") &&
                          Contains("which a RHOFORM SIGNED line declares for "
                                   "an existing potential") &&
                          Contains("NONNEG") && Contains("MONOTONE"));
  CHECK_FALSE(c.has_stored("RHOFORM"));  // nothing was materialised
  REQUIRE_THROWS_AS(ReamDensity::from_context(c), DensityFormError);
  // an invalid value names itself and lists the forms (the key is present,
  // so it does not claim the key is missing), without an instruction (it
  // said "is not a density form: use SIGNED ...")
  c.add("RHOFORM", "SQUARE");
  REQUIRE_THROWS_WITH(DM_REAM(c),
                      Contains("RHOFORM 'SQUARE' is not a density form; the "
                               "density forms are SIGNED") &&
                          !Contains("RHOFORM is required"));
  // INITMB false reads nothing
  Context off = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Dummy");
  off.add("INITMB", "false");
  REQUIRE_NOTHROW(DM_REAM(off));
  // case-insensitive
  Context low = ream_ctx(layout(BasisKind::Blip5, true), "nonneg", "Cut_Dummy");
  DM_REAM dm(low);
  CHECK(dm.density().form == RhoForm::NonNeg);
}

TEST_CASE_METHOD(DensityFixture, "DM_REAM: density form validation messages",
                 "[ream][rhoform][validation]") {
  SECTION("NONNEG / MONOTONE: a negative or non-finite amplitude") {
    for (const char *form : {"NONNEG", "MONOTONE"}) {
      Context c = ream_ctx(layout(BasisKind::Blip5, false), form, "Cut_Dummy");
      INFO(form);
      REQUIRE_THROWS_AS(DM_REAM(c), DensityFormError);
      REQUIRE_THROWS_WITH(
          DM_REAM(c), Contains(std::string("RHOFORM ") + form) &&
                          Contains("every AMPGRIDMB entry to be finite and >= 0") &&
                          Contains("entry 2 is -0.35"));
      ReamDensity d;
      d.form = parse_rhoform(form);
      d.basis = BasisKind::Blip5;
      d.crho = {1.0, 2.0, 3.0, 4.0, 5.0};
      d.srho = {1.0, 1.0, 1.0, 1.0, 1.0};
      d.arho = {0.1, 0.1, 0.1, 0.1, std::nan("")};
      Cut_Dummy cut(kRcut);
      REQUIRE_THROWS_WITH(d.validate(&cut), Contains("entry 5 is nan"));
    }
  }
  SECTION("NONNEG / MONOTONE: a non-finite centre or width") {
    // each would make rho NaN (pair_density(2.5) = nan was accepted)
    const double inf = std::numeric_limits<double>::infinity();
    for (const FormCase &fc : {FormCase{BasisKind::Blip5, "NONNEG", true},
                               FormCase{BasisKind::Knot5, "NONNEG", true},
                               FormCase{BasisKind::Blip5, "MONOTONE", true}}) {
      for (double bad : {std::nan(""), inf}) {
        Layout L = layout(fc.basis, true);
        L.c[1] = bad;
        INFO(basis_name(fc.basis) << " " << fc.form << " CGRIDMB[2] = " << bad);
        Context c = ream_ctx(L, fc.form, "Cut_Cos");
        REQUIRE_THROWS_AS(DM_REAM(c), DensityFormError);
        REQUIRE_THROWS_WITH(DM_REAM(c),
                            Contains("RHOFORM " + fc.form) &&
                                Contains("every CGRIDMB entry to be finite; "
                                         "entry 2 is "));
        // SIGNED is unchanged: tadah accepted it before 1.6.0
        REQUIRE_NOTHROW(DM_REAM(ream_ctx(L, "SIGNED", "Cut_Cos")));
      }
    }
    Layout L = layout(BasisKind::Blip5, true);
    L.s[3] = inf;
    REQUIRE_THROWS_WITH(DM_REAM(ream_ctx(L, "NONNEG", "Cut_Cos")),
                        Contains("RHOFORM NONNEG requires every SGRIDMB entry "
                                 "to be finite; entry 4 is inf"));
    L.s[3] = std::nan("");
    REQUIRE_THROWS_AS(DM_REAM(ream_ctx(L, "NONNEG", "Cut_Cos")),
                      DensityFormError);
    // MONOTONE keeps its own width message (finite and > 0)
    REQUIRE_THROWS_WITH(DM_REAM(ream_ctx(L, "MONOTONE", "Cut_Cos")),
                        Contains("every SGRIDMB entry to be finite and > 0"));
    REQUIRE_NOTHROW(DM_REAM(ream_ctx(L, "SIGNED", "Cut_Cos")));
  }
  SECTION("MONOTONE: the knot family is refused (any KDEGMB)") {
    Context c = ream_ctx(layout(BasisKind::Knot5, true), "MONOTONE", "Cut_Dummy");
    REQUIRE_THROWS_WITH(
        DM_REAM(c), Contains("knot family") &&
                        Contains("use RHOFORM NONNEG: a non-negative knot "
                                 "density is already non-increasing with a "
                                 "non-increasing cutoff"));
    // NONNEG takes it
    Context ok = ream_ctx(layout(BasisKind::Knot5, true), "NONNEG", "Cut_Dummy");
    REQUIRE_NOTHROW(DM_REAM(ok));
  }
  SECTION("MONOTONE: SGRIDMB must be finite and > 0") {
    Layout L = layout(BasisKind::Blip, true);
    L.s[2] = -1.7;
    Context c = ream_ctx(L, "MONOTONE", "Cut_Dummy");
    REQUIRE_THROWS_WITH(DM_REAM(c),
                        Contains("every SGRIDMB entry to be finite and > 0") &&
                            Contains("entry 3 is -1.7"));
    // SIGNED and NONNEG accept a negative width (psi is even in x)
    REQUIRE_NOTHROW(DM_REAM(ream_ctx(L, "SIGNED", "Cut_Dummy")));
    REQUIRE_NOTHROW(DM_REAM(ream_ctx(L, "NONNEG", "Cut_Dummy")));
    // a zero width is refused by every form (existing check, text unchanged)
    L.s[2] = 0.0;
    REQUIRE_THROWS_WITH(DM_REAM(ream_ctx(L, "SIGNED", "Cut_Dummy")),
                        Contains("DM_REAM: zero entry in SGRIDMB."));
  }
  SECTION("NONNEG: the Gaussian basis needs SGRIDMB > 0 as well") {
    // exp(-s (r - c)^2) is not even in s: a negative width grows without
    // bound (the Blip bumps above are even, so NONNEG keeps accepting them)
    Layout L = layout(BasisKind::Gaussian, true);
    L.s[4] = -2.5;
    Context c = ream_ctx(L, "NONNEG", "Cut_Cos");
    REQUIRE_THROWS_AS(DM_REAM(c), DensityFormError);
    REQUIRE_THROWS_WITH(
        DM_REAM(c), Contains("RHOFORM NONNEG requires every SGRIDMB entry to "
                             "be finite and > 0 for the Gaussian basis; "
                             "entry 5 is -2.5"));
    REQUIRE_THROWS_WITH(DM_REAM(ream_ctx(L, "MONOTONE", "Cut_Cos")),
                        Contains("every SGRIDMB entry to be finite and > 0") &&
                            Contains("entry 5 is -2.5"));
    REQUIRE_NOTHROW(DM_REAM(ream_ctx(L, "SIGNED", "Cut_Cos")));  // as before 1.6.0
    L.s[4] = std::numeric_limits<double>::infinity();
    REQUIRE_THROWS_WITH(DM_REAM(ream_ctx(L, "NONNEG", "Cut_Cos")),
                        Contains("for the Gaussian basis; entry 5 is inf"));
    // the direct evaluator refuses it the same way
    ReamDensity d = ReamDensity::from_context(ream_ctx(L, "NONNEG", "Cut_Cos"));
    L.s[4] = -2.5;
    d.srho[4] = -2.5;
    REQUIRE_THROWS_AS(d.validate(nullptr), DensityFormError);
    d.srho[4] = 2.5;
    REQUIRE_NOTHROW(d.validate(nullptr));
  }
  SECTION("MONOTONE: the cutoff whitelist (Cut_SinInv refused)") {
    Context c = ream_ctx(layout(BasisKind::Blip5, true), "MONOTONE", "Cut_SinInv");
    REQUIRE_THROWS_WITH(DM_REAM(c), Contains("RCTYPEMB must be one of") &&
                                        Contains("got Cut_SinInv"));
    for (const std::string &cut : kCuts)
      REQUIRE_NOTHROW(DM_REAM(ream_ctx(layout(BasisKind::Blip5, true),
                                       "MONOTONE", cut)));
    // NONNEG and SIGNED do not restrict the cutoff
    REQUIRE_NOTHROW(DM_REAM(ream_ctx(layout(BasisKind::Blip5, true), "NONNEG",
                                     "Cut_SinInv")));
  }
  SECTION("positive forms: every WATOM pair weight > 0") {
    for (const char *form : {"NONNEG", "MONOTONE"}) {
      INFO(form);
      Context c = ream_ctx(layout(BasisKind::Blip5, true), form, "Cut_Dummy", 0.0);
      REQUIRE_THROWS_WITH(DM_REAM(c),
                          Contains("every density pair weight must be finite "
                                   "and > 0") &&
                              Contains("WATOM entry 1 (Ta) is 0"));
      Context n = ream_ctx(layout(BasisKind::Blip5, true), form, "Cut_Dummy", -0.5);
      REQUIRE_THROWS_AS(DM_REAM(n), DensityFormError);
    }
    // SIGNED keeps any weight
    REQUIRE_NOTHROW(DM_REAM(
        ream_ctx(layout(BasisKind::Blip5, true), "SIGNED", "Cut_Dummy", -0.5)));
    // pair-triple WATOM
    Context t = ream_ctx(layout(BasisKind::Blip5, true), "NONNEG", "Cut_Dummy");
    t.remove("WATOM");
    t.add("WATOM", "Ta Ta -1.0");
    REQUIRE_THROWS_WITH(DM_REAM(t), Contains("WATOM pair 1 (Ta Ta) is -1"));
  }
  SECTION("unknown basis codes are refused before RHOFORM is read") {
    Context c = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Dummy");
    c.remove("TYPEMB");
    c.add("TYPEMB", "DM_REAM 9 6 6 6 3 3 0 0 Ta Ta");
    REQUIRE_THROWS_WITH(DM_REAM(c), Contains("unknown basis code"));
  }
}

/* Pins: the DM_REAM constructor refuses RHOFORM NONNEG with negative amplitudes and a
 * context without RHOFORM (DensityFormError), a zero entry in SEMBFUNC
 * ("zero entry in SEMBFUNC"); RHOFORM NONNEG with non-negative amplitudes
 * builds a NONNEG density. A caller with a new context builds a new
 * descriptor, and these checks apply.
 * Before the fix: passes as well (a pin of the constructor's checks). */
TEST_CASE_METHOD(DensityFixture,
                 "DM_REAM: the constructor refuses a context whose density "
                 "form or embedding it cannot evaluate",
                 "[ream][rhoform][validation]") {
  Context neg = ream_ctx(layout(BasisKind::Blip5, false), "NONNEG", "Cut_Cos");
  CHECK_THROWS_AS(DM_REAM(neg), DensityFormError);

  Context keyless = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Cos");
  CHECK_THROWS_AS(DM_REAM(keyless), DensityFormError);

  Context bad_emb = ream_ctx(layout(BasisKind::Blip5, true), "SIGNED", "Cut_Cos");
  bad_emb.remove("SEMBFUNC");
  bad_emb.add("SEMBFUNC", "1.0 0.0 1.0");
  CHECK_THROWS_WITH(DM_REAM(bad_emb), Contains("zero entry in SEMBFUNC"));

  Context pos = ream_ctx(layout(BasisKind::Blip5, true), "NONNEG", "Cut_Cos");
  DM_REAM dm(pos);
  CHECK(dm.density().form == RhoForm::NonNeg);
}

// ---------------------------------------------------------------------------
//  5. rhoform.h helpers
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(DensityFixture, "rhoform: names, parse and the key contract",
                 "[ream][rhoform][helpers]") {
  CHECK(std::string(rhoform_name(RhoForm::Signed)) == "SIGNED");
  CHECK(std::string(rhoform_name(RhoForm::NonNeg)) == "NONNEG");
  CHECK(std::string(rhoform_name(RhoForm::Monotone)) == "MONOTONE");
  CHECK(parse_rhoform("signed") == RhoForm::Signed);
  CHECK(parse_rhoform("NonNeg") == RhoForm::NonNeg);
  CHECK(parse_rhoform("MONOTONE") == RhoForm::Monotone);
  CHECK_THROWS_AS(parse_rhoform("SQUARE"), DensityFormError);
  CHECK_THROWS_WITH(parse_rhoform("x"),
                    Contains("SIGNED, NONNEG or MONOTONE") && Contains("'x'"));
  CHECK_FALSE(rhoform_is_positive(RhoForm::Signed));
  CHECK(rhoform_is_positive(RhoForm::NonNeg));
  CHECK(rhoform_is_positive(RhoForm::Monotone));

  CHECK(rhoform_absent_message() ==
        "DM_REAM: RHOFORM is required: SIGNED (rho = W f_c sum a_n psi_n with "
        "signed a_n: the density before tadah 1.6.0, which a RHOFORM SIGNED "
        "line declares for an existing potential, with an EMBF0 FREE line for "
        "its embedding reference), NONNEG (the same with every a_n >= 0: rho "
        ">= 0 by construction) or MONOTONE (rho = W f_c sum a_n S_n(r), S_n "
        "the integrated bump, a_n >= 0: rho >= 0 and non-increasing by "
        "construction).");

  // the blueprint gives RHOFORM no default: nothing ever materialises it
  Context c;
  CHECK_FALSE(stored_rhoform(c).has_value());
  CHECK_FALSE(c.contains("RHOFORM"));
  CHECK_FALSE(c.has_stored("RHOFORM"));
  CHECK_THROWS_WITH(require_rhoform(c, "tadah hpo: -c pot.tadah"),
                    Contains("tadah hpo: -c pot.tadah: DM_REAM: RHOFORM is "
                             "required"));
  CHECK_THROWS_WITH(require_rhoform(c, ""),
                    Catch::Matchers::StartsWith("DM_REAM: RHOFORM is required"));
  c.add("RHOFORM", "monotone");  // the STRING grammar accepts lower case
  CHECK(stored_rhoform(c) == RhoForm::Monotone);
  CHECK(require_rhoform(c, "x") == RhoForm::Monotone);
  c.add("RHOFORM", "bogus");
  CHECK_THROWS_AS(stored_rhoform(c), DensityFormError);
  CHECK_THROWS_WITH(require_rhoform(c, ""), Contains("'bogus'"));

  for (const char *ok : {"Cut_Dummy", "Cut_Cos", "Cut_CosS", "Cut_Tanh",
                         "Cut_PT", "Cut_Poly", "Cut_PolyS"})
    CHECK(rhoform_cutoff_whitelisted(ok));
  for (const char *no : {"Cut_SinInv", "cut_cos", "Cut_Foo", ""})
    CHECK_FALSE(rhoform_cutoff_whitelisted(no));

  CHECK(optim_key_lower_bound("AMPGRIDMB", RhoForm::NonNeg) ==
        OptimLowerBound::NonNegative);
  CHECK(optim_key_lower_bound("ampgridmb", RhoForm::Monotone) ==
        OptimLowerBound::NonNegative);
  CHECK(optim_key_lower_bound("AMPGRIDMB", RhoForm::Signed) ==
        OptimLowerBound::Free);
  CHECK(optim_key_lower_bound("AMPGRIDMB", std::nullopt) ==
        OptimLowerBound::Free);
  CHECK(optim_key_lower_bound("SGRIDMB", RhoForm::Monotone) ==
        OptimLowerBound::Free);
}

TEST_CASE_METHOD(DensityFixture, "rhoform: normalise_rhoform_key (writers)",
                 "[ream][rhoform][helpers]") {
  SECTION("DM_REAM potential: canonical upper case") {
    Context c = ream_ctx(layout(BasisKind::Blip5, true), "nonneg", "Cut_Dummy");
    normalise_rhoform_key(c);
    CHECK(c.stored_value("RHOFORM").value_or("") == "NONNEG");
    CHECK(c.size("RHOFORM") == 1);
  }
  SECTION("DM_REAM potential without the key: throws, inserts nothing") {
    Context c = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Dummy");
    CHECK_THROWS_WITH(normalise_rhoform_key(c),
                      Contains("RHOFORM is required"));
    CHECK_FALSE(c.has_stored("RHOFORM"));
  }
  SECTION("no DM_REAM block: a stray key is removed") {
    Context c;
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_Blip 0 2 2 Ta Ta");
    c.add("RHOFORM", "SIGNED");
    normalise_rhoform_key(c);
    CHECK_FALSE(c.has_stored("RHOFORM"));
    Context none;
    REQUIRE_NOTHROW(normalise_rhoform_key(none));
    CHECK_FALSE(none.has_stored("RHOFORM"));
  }
  SECTION("DM_mJoin with a DM_REAM child counts as DM_REAM") {
    Context c;
    c.add("INITMB", "true");
    c.add("TYPEMB", "DM_mJoin");
    c.add("TYPEMB", "DM_Blip 0 2 2 Ta Ta");
    c.add("TYPEMB", "DM_REAM");
    CHECK_THROWS_AS(normalise_rhoform_key(c), DensityFormError);
    c.add("RHOFORM", "Signed");
    normalise_rhoform_key(c);
    CHECK(c.stored_value("RHOFORM").value_or("") == "SIGNED");
  }
}

/* Pins: check_rhoform_precedence, for an engine that merges a run's
 * configuration over a potential (tadah refit --edit with -c): the
 * potential's form, or the same form in the configuration (any case), is
 * accepted; another form in the configuration is refused, "RHOFORM
 * conflict: pot.tadah declares RHOFORM SIGNED and the configuration file
 * sets RHOFORM MONOTONE. RHOFORM is a property of the potential (...): the
 * potential file sets it, not the configuration file."; a DM_REAM potential
 * without the key is refused, "pot.tadah: the RHOFORM of the configuration
 * file does not apply (...); the RHOFORM line belongs in pot.tadah.", with
 * the migration text, and so it is without a configuration key; without a
 * DM_REAM block a key on either side is ignored.
 * Before the fix: the messages said "... but the task configuration sets
 * RHOFORM MONOTONE ... edit the potential file instead." and "pot.tadah:
 * the task configuration's RHOFORM does not apply (...); add the RHOFORM
 * line to pot.tadah.": the two message checks fail. */
TEST_CASE_METHOD(DensityFixture,
                 "rhoform: check_rhoform_precedence (engines merging a "
                 "configuration over a potential)", "[ream][rhoform][helpers]") {
  const Context with = ream_ctx(layout(BasisKind::Blip5, true), "SIGNED", "Cut_Dummy");
  const Context without = ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Dummy");
  Context task_none;
  Context task_signed;
  task_signed.add("RHOFORM", "signed");
  Context task_mono;
  task_mono.add("RHOFORM", "MONOTONE");

  REQUIRE_NOTHROW(check_rhoform_precedence(with, task_none, "pot.tadah"));
  REQUIRE_NOTHROW(check_rhoform_precedence(with, task_signed, "pot.tadah"));
  CHECK_THROWS_WITH(check_rhoform_precedence(with, task_mono, "pot.tadah"),
                    Contains("RHOFORM conflict: pot.tadah declares RHOFORM "
                             "SIGNED and the configuration file sets RHOFORM "
                             "MONOTONE. RHOFORM is a property of the "
                             "potential (its density amplitudes were fitted "
                             "in that form): the potential file sets it, not "
                             "the configuration file."));
  // a key-less DM_REAM potential cannot borrow the configuration's form,
  // and the message says the configuration's key was not used
  CHECK_THROWS_WITH(check_rhoform_precedence(without, task_signed, "pot.tadah"),
                    Contains("pot.tadah: the RHOFORM of the configuration "
                             "file does not apply (the form belongs to the "
                             "potential, whose density amplitudes were fitted "
                             "in it); the RHOFORM line belongs in "
                             "pot.tadah.") &&
                        Contains("DM_REAM: RHOFORM is required"));
  CHECK_THROWS_WITH(check_rhoform_precedence(without, task_none, "pot.tadah"),
                    Contains("pot.tadah: DM_REAM: RHOFORM is required"));
  CHECK_THROWS_AS(check_rhoform_precedence(without, task_none, "pot.tadah"),
                  DensityFormError);
  // no DM_REAM block: a key on either side is ignored (WARNING)
  Context blip;
  blip.add("INITMB", "true");
  blip.add("TYPEMB", "DM_Blip 0 2 2 Ta Ta");
  REQUIRE_NOTHROW(check_rhoform_precedence(blip, task_mono, "pot.tadah"));
  blip.add("RHOFORM", "SIGNED");
  REQUIRE_NOTHROW(check_rhoform_precedence(blip, task_none, "pot.tadah"));
}

// ---------------------------------------------------------------------------
//  6. lattice shells and WATOM parsing
// ---------------------------------------------------------------------------

TEST_CASE("eam_lattice::shells: bcc/fcc/hcp distances and multiplicities",
          "[ream][rhoform][shells]") {
  struct Want { double d2; int m; };
  auto check = [](const std::vector<eam_lattice::Shell> &got,
                  const std::vector<Want> &want) {
    REQUIRE(got.size() == want.size());
    for (std::size_t k = 0; k < want.size(); ++k) {
      INFO("shell " << k);
      CHECK(got[k].d == Approx(std::sqrt(want[k].d2)).epsilon(1e-14));
      CHECK(got[k].m == want[k].m);
      if (k) CHECK(got[k].d > got[k - 1].d);
    }
  };
  // bcc: n4 = |2R/a|^2 = 3, 4, 8, 11, 12, 16, 19, 20
  const auto bcc = eam_lattice::shells("bcc", 2.3);
  check(bcc, {{0.75, 8}, {1.0, 6}, {2.0, 12}, {2.75, 24}, {3.0, 8},
              {4.0, 6}, {4.75, 24}, {5.0, 24}});
  CHECK(bcc[0].d == std::sqrt(3.0) / 2.0);  // exactly the n4 formula
  // fcc: n4 = 2, 4, ..., 20
  check(eam_lattice::shells("FCC", 2.3),
        {{0.5, 12}, {1.0, 6}, {1.5, 24}, {2.0, 12}, {2.5, 24}, {3.0, 8},
         {3.5, 48}, {4.0, 6}, {4.5, 36}, {5.0, 24}});
  // ideal hcp (a = in-plane constant): d^2 = 1, 2, 8/3, 3, 11/3, 4, 5
  check(eam_lattice::shells("hcp", 2.3),
        {{1.0, 12}, {2.0, 6}, {8.0 / 3.0, 2}, {3.0, 18}, {11.0 / 3.0, 12},
         {4.0, 6}, {5.0, 12}});
  // the neighbour count within dmax is the multiplicity sum
  for (const char *lat : {"bcc", "fcc", "hcp"}) {
    int total = 0;
    for (const auto &s : eam_lattice::shells(lat, 2.3)) total += s.m;
    CHECK(static_cast<std::size_t>(total) ==
          eam_lattice::neighbour_distances(lat, 1.0, 2.3).size());
  }
  // a shell exactly at dmax is included (n4 <= 4 dmax^2)
  CHECK(eam_lattice::shells("bcc", 1.0).back().d == 1.0);
  CHECK(eam_lattice::shells("bcc", 0.0).empty());
  CHECK_THROWS(eam_lattice::shells("sc", 2.0));
  CHECK_THROWS(eam_lattice::shells("bcc", std::nan("")));
}

TEST_CASE_METHOD(DensityFixture,
                 "D_Base::parse_watom drives update_weights, and density "
                 "weights positivity", "[ream][rhoform][watom]") {
  const int Ta = PeriodicTable::find_by_symbol("Ta").Z;
  const int W = PeriodicTable::find_by_symbol("W").Z;
  SECTION("per species: geometric mean off the diagonal") {
    Context c;
    c.add("ATOM", "Ta");
    c.add("ATOM", "W");
    c.add("WATOM", 0.5);
    c.add("WATOM", 2.0);
    const auto spec = D_Base::parse_watom(c);
    CHECK(spec.mode == D_Base::WatomMode::PerSpecies);
    CHECK(spec.values == std::vector<double>{0.5, 2.0});
    CHECK(spec.Zs == std::vector<int>{Ta, W});
    REQUIRE(spec.writes.size() == 3);
    CHECK(spec.writes[1].w == std::sqrt(0.5 * 2.0));
    c.add("VERBOSE", 0);
    DM_REAM dm;  // any descriptor: update_weights is D_Base's
    dm.update_weights(c);
    CHECK(dm.get_weight(Ta, Ta) == 0.5);
    CHECK(dm.get_weight(W, W) == 2.0);
    CHECK(dm.get_weight(Ta, W) == 1.0);
    CHECK(dm.get_weight(W, Ta) == 1.0);
    CHECK(dm.get_atom_Z_for_arg_pos(2) == W);
    REQUIRE_NOTHROW(D_Base::require_positive_density_weights(c, "x"));
    c.remove("WATOM");
    c.add("WATOM", 0.5);
    c.add("WATOM", -2.0);
    CHECK_THROWS_WITH(D_Base::require_positive_density_weights(c, "who"),
                      Contains("who: every density pair weight must be finite "
                               "and > 0; WATOM entry 2 (W) is -2."));
  }
  SECTION("pair triples") {
    Context c;
    c.add("WATOM", "Ta W 0.25 W W 3.0");
    const auto spec = D_Base::parse_watom(c);
    CHECK(spec.mode == D_Base::WatomMode::PairTriple);
    CHECK(spec.values == std::vector<double>{0.25, 3.0});
    DM_REAM dm;
    dm.update_weights(c);
    CHECK(dm.get_weight(Ta, W) == 0.25);
    CHECK(dm.get_weight(W, Ta) == 0.25);
    CHECK(dm.get_weight(W, W) == 3.0);
    CHECK(dm.get_weight(Ta, Ta) == 1.0);
    CHECK(dm.has_watom_arg_pos(2));
    CHECK_FALSE(dm.has_watom_arg_pos(3));
    Context bad;
    bad.add("WATOM", "Ta W");
    CHECK_THROWS_WITH(D_Base::parse_watom(bad), Contains("triples"));
  }
  SECTION("absent WATOM: all weights 1, nothing to check") {
    Context c;
    CHECK(D_Base::parse_watom(c).mode == D_Base::WatomMode::Empty);
    REQUIRE_NOTHROW(D_Base::require_positive_density_weights(c, "x"));
  }
}

TEST_CASE_METHOD(DensityFixture,
                 "DM_REAM: a TYPEMB basis code that does not convert is an "
                 "error, not basis code 0 (Blip)",
                 "[ream][rhoform][basis_code]") {
  // TYPEMB token 1 is the basis code. A token that is not an integer
  // literal was read as 0 (Blip) by DM_REAM (a failed Context::get<int>)
  // and by ReamDensity (a failed istringstream), or as its integer prefix
  // ("2.5" -> 2, the knot family). Files cannot hold such a token (the
  // TYPE line is checked when it is read); a Context built in code can.
  auto with_code = [](const std::string &code) {
    Context c = ream_ctx(layout(BasisKind::Blip, true), "SIGNED", "Cut_Cos");
    std::vector<std::string> t(c.size("TYPEMB"));
    c.get("TYPEMB", t);
    c.remove("TYPEMB");
    for (std::size_t i = 0; i < t.size(); ++i)
      if (i != 1 || !code.empty()) c.add("TYPEMB", i == 1 ? code : t[i]);
    return c;
  };
  for (const std::string bad : {"x", "2.5", "0x0", "1e0", "+"}) {
    INFO("basis code '" << bad << "'");
    const Context c = with_code(bad);
    REQUIRE(c.stored_value("TYPEMB", 1).value_or("") == bad);
    CHECK_THROWS_AS(ReamDensity::from_context(c), DensityFormError);
    CHECK_THROWS_WITH(ReamDensity::from_context(c),
                      Contains("DM_REAM density: TYPEMB basis code '" + bad +
                               "' is not an integer"));
    CHECK_THROWS_WITH(DM_REAM(c), Contains("DM_REAM: TYPEMB basis code '" +
                                           bad + "' is not an integer"));
  }
  // An absent basis code (a bare "TYPEMB DM_REAM"): the density refuses it;
  // DM_REAM refuses the whole TYPE line first.
  Context bare = ream_ctx(layout(BasisKind::Blip, true), "SIGNED", "Cut_Cos");
  bare.remove("TYPEMB");
  bare.add("TYPEMB", "DM_REAM");
  CHECK_THROWS_WITH(ReamDensity::from_context(bare),
                    Contains("DM_REAM density: TYPEMB has no basis code"));
  // an unknown integer code is refused as before
  CHECK_THROWS_WITH(ReamDensity::from_context(with_code("7")),
                    Contains("DM_REAM density: "));
  // every known code still loads (Blip written as "0" and "+0")
  REQUIRE_NOTHROW(ReamDensity::from_context(with_code("0")));
  CHECK(ReamDensity::from_context(with_code("+0")).basis == BasisKind::Blip);
  REQUIRE_NOTHROW(DM_REAM(with_code("0")));
}

TEST_CASE_METHOD(DensityFixture,
                 "rhoform: the OPTIM low-bound rules follow the density basis, "
                 "as ReamDensity::validate does",
                 "[ream][rhoform][optim_rule]") {
  const std::vector<BasisKind> bump = {BasisKind::Blip, BasisKind::Blip5,
                                       BasisKind::Gaussian};
  SECTION("NONNEG: SGRIDMB > 0 for the Gaussian basis only") {
    const auto g =
        optim_low_bound_rule("SGRIDMB", RhoForm::NonNeg, BasisKind::Gaussian);
    REQUIRE(g);
    CHECK(g->strict);
    CHECK_FALSE(g->admits(-0.5));
    CHECK_FALSE(g->admits(0.0));
    CHECK_FALSE(g->admits(std::nan("")));
    CHECK(g->admits(1e-3));
    CHECK_THAT(g->requirement, Contains("every SGRIDMB width > 0") &&
                                   Contains("Gaussian"));
    CHECK(optim_low_bound_rule("sgridmb", RhoForm::NonNeg,
                               BasisKind::Gaussian));
    for (BasisKind b : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Knot5})
      CHECK_FALSE(optim_low_bound_rule("SGRIDMB", RhoForm::NonNeg, b));
    // an unknown basis (an hpotarget file names no potential): no basis rule
    CHECK_FALSE(optim_low_bound_rule("SGRIDMB", RhoForm::NonNeg, std::nullopt));
  }
  SECTION("MONOTONE: SGRIDMB > 0 for every basis; SIGNED: no rule") {
    for (BasisKind b : bump) {
      INFO(basis_name(b));
      const auto m = optim_low_bound_rule("SGRIDMB", RhoForm::Monotone, b);
      REQUIRE(m);
      CHECK(m->strict);
      CHECK_FALSE(optim_low_bound_rule("SGRIDMB", RhoForm::Signed, b));
      CHECK_FALSE(optim_low_bound_rule("SGRIDMB", std::nullopt, b));
    }
    CHECK(optim_low_bound_rule("SGRIDMB", RhoForm::Monotone, std::nullopt));
  }
  SECTION("AMPGRIDMB and WATOM do not depend on the basis") {
    for (const auto b : {std::optional<BasisKind>(), std::optional<BasisKind>(
                                                         BasisKind::Gaussian)}) {
      for (RhoForm f : {RhoForm::NonNeg, RhoForm::Monotone}) {
        const auto a = optim_low_bound_rule("AMPGRIDMB", f, b);
        REQUIRE(a);
        CHECK_FALSE(a->strict);
        CHECK(a->admits(0.0));
        const auto w = optim_low_bound_rule("WATOM", f, b);
        REQUIRE(w);
        CHECK(w->strict);
      }
      CHECK_FALSE(optim_low_bound_rule("AMPGRIDMB", RhoForm::Signed, b));
      CHECK_FALSE(optim_low_bound_rule("CGRIDMB", RhoForm::Monotone, b));
    }
  }
  SECTION("a box low bound the rule admits is a width the density accepts") {
    // The single source of truth: for every form and bump basis, a width
    // w as the low bound of an SGRIDMB box passes the rule exactly when
    // ReamDensity::validate accepts a density with that width.
    for (const char *form : {"SIGNED", "NONNEG", "MONOTONE"}) {
      for (BasisKind b : bump) {
        for (double w : {-0.5, 1.0}) {
          INFO(form << " " << basis_name(b) << " SGRIDMB width " << w);
          Layout L = layout(b, true);
          L.s[0] = w;
          const Context c = ream_ctx(L, form, "Cut_Cos");
          bool density_ok = true;
          try {
            ReamDensity::validate_context(c);
          } catch (const DensityFormError &) {
            density_ok = false;
          }
          const auto rule = optim_low_bound_rule(
              "SGRIDMB", parse_rhoform(form), optim_rule_basis(c));
          CHECK((!rule || rule->admits(w)) == density_ok);
        }
      }
    }
  }
  SECTION("optim_rule_basis reads the seed's DM_REAM basis") {
    CHECK_FALSE(optim_rule_basis(Context()).has_value());
    for (BasisKind b : {BasisKind::Blip, BasisKind::Blip5, BasisKind::Gaussian,
                        BasisKind::Knot5})
      CHECK(optim_rule_basis(ream_ctx(layout(b, true), "NONNEG",
                                      "Cut_Cos")) == b);
    // a DM_mJoin: Gaussian when a DM_REAM child is (a joined SGRIDMB box
    // does not say whose entries it holds), else the first DM_REAM child's
    auto mjoin = [](const char *second_code) {
      Context c;
      c.add("VERBOSE", 0);
      for (int k = 0; k < 3; ++k) c.add("RCTYPEMB", "Cut_Cos");
      for (int k = 0; k < 3; ++k) c.add("RCUTMB", 5.0);
      c.add("INITMB", "true");
      c.add("TYPEMB", "DM_mJoin");
      c.add("TYPEMB", "DM_Blip 0 2 2 Ta Ta");
      c.add("TYPEMB", "DM_REAM 3 2 2 2 2 2 0 0 Ta Ta");
      c.add("TYPEMB", std::string("DM_REAM ") + second_code +
                          " 2 2 2 2 2 0 0 Ta Ta");
      c.add("CGRIDMB", "1.7 2.3 2.0 3.0 2.5 3.5");
      c.add("SGRIDMB", "0.5 0.7 1.0 1.0 1.0 1.0");
      c.add("AMPGRIDMB", "0.4 0.2 0.3 0.1");
      c.add("RHOFORM", "NONNEG");
      c.add("CEMBFUNC", "0.0 0.5 0.0 0.5");
      c.add("SEMBFUNC", "1.0 1.0 1.0 1.0");
      return c;
    };
    CHECK(optim_rule_basis(mjoin("1")) == BasisKind::Gaussian);
    CHECK(optim_rule_basis(mjoin("0")) == BasisKind::Blip5);
    // a basis code that does not convert is an error here too
    CHECK_THROWS_WITH(optim_rule_basis(mjoin("x")),
                      Contains("TYPEMB basis code 'x' is not an integer"));
  }
}

TEST_CASE_METHOD(DensityFixture,
                 "rhoform: the OPTIM low-bound rule of a configuration adds "
                 "every radial bump width > 0 to the density-form rules",
                 "[ream][rhoform][optim_rule][bump_width]") {
  // A radial bump channel -- SGRID2B of a
  // D2_Blip/D2_Blip5 pair term, SGRIDMB of a DM_REAM Blip/Blip5 density --
  // needs every width > 0 in every RHOFORM: a bump spans [c - w/s, c + w/s],
  // and a width <= 0 has no support edge. The density-form rules
  // (optim_low_bound_rule(key, form, basis), what ReamDensity::validate
  // refuses) come first and say which RHOFORM sets them.
  SECTION("SGRIDMB of a DM_REAM bump density, every form") {
    for (BasisKind b : {BasisKind::Blip, BasisKind::Blip5}) {
      for (const char *form : {"SIGNED", "NONNEG", "MONOTONE"}) {
        INFO(basis_name(b) << " " << form);
        const Context c = ream_ctx(layout(b, true), form, "Cut_Cos");
        const auto r = optim_low_bound_rule("SGRIDMB", c);
        REQUIRE(r);
        CHECK(r->strict);
        CHECK_FALSE(r->admits(0.0));
        CHECK_FALSE(r->admits(-1.0));
        CHECK_FALSE(r->admits(std::nan("")));
        CHECK(r->admits(1e-3));
        CHECK_THAT(r->requirement, Contains("every SGRIDMB width > 0"));
        if (std::string(form) == "MONOTONE")
          CHECK(r->by == "RHOFORM MONOTONE");
        else
          CHECK(r->by == std::string("the DM_REAM ") + basis_name(b) +
                             " density basis");
        CHECK(optim_low_bound_rule("sgridmb", c));
        CHECK_FALSE(optim_low_bound_rule("CGRIDMB", c));
      }
    }
  }
  SECTION("the density-form rules come first and name their RHOFORM") {
    const Context g = ream_ctx(layout(BasisKind::Gaussian, true), "NONNEG",
                               "Cut_Cos");
    const auto rg = optim_low_bound_rule("SGRIDMB", g);
    REQUIRE(rg);
    CHECK(rg->by == "RHOFORM NONNEG");
    CHECK_THAT(rg->requirement, Contains("Gaussian"));
    // a Gaussian density is not a bump channel: no width rule under SIGNED
    CHECK_FALSE(optim_low_bound_rule(
        "SGRIDMB",
        ream_ctx(layout(BasisKind::Gaussian, true), "SIGNED", "Cut_Cos")));
    const auto ra = optim_low_bound_rule("AMPGRIDMB", g);
    REQUIRE(ra);
    CHECK(ra->by == "RHOFORM NONNEG");
    CHECK_FALSE(ra->strict);
    // the (key, form, basis) rules carry the same subject
    const auto rf = optim_low_bound_rule("WATOM", RhoForm::Monotone,
                                         std::nullopt);
    REQUIRE(rf);
    CHECK(rf->by == "RHOFORM MONOTONE");
  }
  SECTION("SGRID2B of a D2_Blip or D2_Blip5 pair term") {
    for (const char *label : {"D2_Blip", "D2_Blip5"}) {
      INFO(label);
      Context c;
      c.add("ATOM", "Ta");
      c.add("INIT2B", "true");
      c.add("INITMB", "false");
      c.add("TYPE2B", std::string(label) + " 3 3 Ta Ta");
      c.add("CGRID2B", "2.2 3.2 4.2");
      c.add("SGRID2B", "1.0 1.0 1.0");
      c.add("RCTYPE2B", "Cut_Cos");
      c.add("RCUT2B", 5.0);
      const auto r = optim_low_bound_rule("SGRID2B", c);
      REQUIRE(r);
      CHECK(r->strict);
      CHECK_FALSE(r->admits(0.0));
      CHECK(r->admits(0.25));
      CHECK_THAT(r->requirement, Contains("every SGRID2B width > 0"));
      CHECK(r->by == std::string("the ") + label + " pair basis");
      CHECK(optim_low_bound_rule("sgrid2b", c));
      CHECK_FALSE(optim_low_bound_rule("CGRID2B", c));
      CHECK_FALSE(optim_low_bound_rule("SGRIDMB", c));  // no density
    }
  }
  /* Pins: a D2_mJoin with a D2_Blip or D2_Blip5 descriptor holds its
   * SGRID2B widths > 0, as a line of one D2_Blip does, by the rule of the
   * bump descriptor ("the D2_Blip pair basis of the D2_mJoin line requires
   * each of its SGRID2B widths > 0"): the rule of the key is the rule of
   * the bump descriptor's entries, the entry form below says whose entry
   * an entry is. It held every SGRID2B entry of the line ("every SGRID2B
   * width > 0"), the D2_BP's included, until the entry form. A D2_mJoin
   * without a bump descriptor has no width rule.
   * Before the fix: no rule for a D2_mJoin (bump_channel_halfwidth reads
   * the line's first value), so a seed width of -1 and a width box from 0
   * were accepted. */
  SECTION("SGRID2B of a D2_mJoin with a D2_Blip or D2_Blip5 descriptor") {
    for (const char *label : {"D2_Blip", "D2_Blip5"}) {
      INFO(label);
      Context c;
      c.add("ATOM", "Ta");
      c.add("INIT2B", "true");
      c.add("INITMB", "false");
      c.add("TYPE2B",
            std::string("D2_mJoin D2_BP 2 2 Ta Ta ") + label + " 3 3 Ta Ta");
      c.add("CGRID2B", "0 0 2.2 3.2 4.2");
      c.add("SGRID2B", "0.5 1.0 1.0 1.0 1.0");
      c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
      c.add("RCUT2B", "5.0 5.0");
      const auto r = optim_low_bound_rule("SGRID2B", c);
      REQUIRE(r);
      CHECK(r->strict);
      CHECK_FALSE(r->admits(0.0));
      CHECK(r->admits(0.25));
      CHECK_THAT(r->requirement, Contains("each of its SGRID2B widths > 0"));
      CHECK(r->by ==
            std::string("the ") + label + " pair basis of the D2_mJoin line");
      CHECK_FALSE(optim_low_bound_rule("CGRID2B", c));
    }
    Context c;
    c.add("ATOM", "Ta");
    c.add("INIT2B", "true");
    c.add("INITMB", "false");
    c.add("TYPE2B", "D2_mJoin D2_BP 2 2 Ta Ta D2_BP 1 1 Ta Ta");
    c.add("CGRID2B", "0 0 0");
    c.add("SGRID2B", "0.5 1.0 1.0");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "5.0 5.0");
    CHECK_FALSE(optim_low_bound_rule("SGRID2B", c));
  }
  /* Pins: the rule of an entry of SGRID2B on a D2_mJoin line
   * (optim_low_bound_rule(key, ctx, entry), 0-based) is that of the
   * descriptor whose entry it is: on D2_mJoin of D2_BP 2 2 (SGRID2B
   * entries 0 and 1) and D2_Blip 3 3 (entries 2 to 4), entries 0 and 1 have
   * no rule, as SGRID2B of a line of one D2_BP has none, and entries 2 to 4
   * have the bump rule, "the D2_Blip pair basis of the D2_mJoin line
   * requires each of its SGRID2B widths > 0 (...)"; an entry past the
   * line's values has none. A line of one D2_Blip gives every entry its
   * rule, and an entry of SGRIDMB the rule of the key.
   * Before the fix: there was no entry form (the case does not compile
   * against the header without it); the rule of the key held every
   * SGRID2B entry of the line, the D2_BP's included. */
  SECTION("SGRID2B entries of a D2_mJoin: the rule of their descriptor") {
    Context c;
    c.add("ATOM", "Ta");
    c.add("INIT2B", "true");
    c.add("INITMB", "false");
    c.add("TYPE2B", "D2_mJoin D2_BP 2 2 Ta Ta D2_Blip 3 3 Ta Ta");
    c.add("CGRID2B", "0 0 2.2 3.2 4.2");
    c.add("SGRID2B", "0.5 1.0 1.0 1.0 1.0");
    c.add("RCTYPE2B", "Cut_Cos Cut_Cos");
    c.add("RCUT2B", "5.0 5.0");
    for (std::size_t i : {0u, 1u, 5u}) {
      INFO(i);
      CHECK_FALSE(optim_low_bound_rule("SGRID2B", c, i));
    }
    for (std::size_t i : {2u, 3u, 4u}) {
      INFO(i);
      const auto r = optim_low_bound_rule("SGRID2B", c, i);
      REQUIRE(r);
      CHECK(r->strict);
      CHECK_FALSE(r->admits(0.0));
      CHECK(r->by == "the D2_Blip pair basis of the D2_mJoin line");
      CHECK(std::string(r->requirement) ==
            "each of its SGRID2B widths > 0 (a radial bump spans [c - w/s, "
            "c + w/s]; a width <= 0 has no support edge)");
    }
    CHECK_FALSE(optim_low_bound_rule("CGRID2B", c, 2));
    Context lone;
    lone.add("ATOM", "Ta");
    lone.add("INIT2B", "true");
    lone.add("INITMB", "false");
    lone.add("TYPE2B", "D2_Blip 3 3 Ta Ta");
    lone.add("CGRID2B", "2.2 3.2 4.2");
    lone.add("SGRID2B", "1.0 1.0 1.0");
    lone.add("RCTYPE2B", "Cut_Cos");
    lone.add("RCUT2B", "5.0");
    for (std::size_t i : {0u, 2u}) {
      const auto r = optim_low_bound_rule("SGRID2B", lone, i);
      REQUIRE(r);
      CHECK(r->by == "the D2_Blip pair basis");
      CHECK_THAT(r->requirement, Contains("every SGRID2B width > 0"));
    }
    const Context ream = ream_ctx(layout(BasisKind::Blip, true), "SIGNED",
                                  "Cut_Cos");
    const auto whole = optim_low_bound_rule("SGRIDMB", ream);
    const auto entry = optim_low_bound_rule("SGRIDMB", ream, 1);
    REQUIRE(whole);
    REQUIRE(entry);
    CHECK(entry->by == whole->by);
    CHECK(std::string(entry->requirement) == whole->requirement);
  }
  SECTION("a DM_REAM configuration without RHOFORM: the migration text") {
    CHECK_THROWS_WITH(
        optim_low_bound_rule(
            "SGRIDMB", ream_ctx(layout(BasisKind::Blip5, true), "", "Cut_Cos")),
        Contains("RHOFORM"));
  }
}

TEST_CASE_METHOD(DensityFixture,
                 "WATOM: the model and the density pair weights read one "
                 "number grammar",
                 "[ream][rhoform][watom]") {
  // The model (parse_watom, update_weights) read WATOM with istringstream,
  // the density pair weights (the HPO gates) with strtod: "0x1p-1" was 0 in
  // the model and 0.5 in a gate. Both now read the decimal grammar of the
  // potential and configuration files and refuse anything else.
  auto per_species = [](const std::string &w) {
    Context c;
    c.add("ATOM", "Ta");
    c.add("WATOM", w);
    return c;
  };
  auto triple = [](const std::string &w) {
    Context c;
    c.add("WATOM", "Ta Ta " + w);
    return c;
  };
  for (const std::string bad : {"0x1p-1", "inf", "-inf", "nan", "1e400"}) {
    for (const bool pair : {false, true}) {
      INFO("WATOM value '" << bad << "'" << (pair ? " (pair triple)" : ""));
      const Context c = pair ? triple(bad) : per_species(bad);
      const std::string entry = pair ? "WATOM entry 3" : "WATOM entry 1";
      CHECK_THROWS_WITH(D_Base::parse_watom(c),
                        Contains(entry + " ('" + bad +
                                 "') is not a decimal number"));
      CHECK_THROWS_WITH(D_Base::density_pair_weights(c),
                        Contains(entry + " ('" + bad +
                                 "') is not a decimal number"));
      CHECK_THROWS_WITH(D_Base::require_positive_density_weights(c, "who"),
                        Contains("is not a decimal number"));
      DM_REAM dm;
      CHECK_THROWS_WITH(dm.update_weights(c),
                        Contains("is not a decimal number"));
    }
  }
  // the decimal grammar, a subnormal included: the same value on both paths
  for (const std::string ok : {"0.5", "+2", "5.", ".25", "7.5e-3", "1e-310"}) {
    for (const bool pair : {false, true}) {
      INFO("WATOM value '" << ok << "'" << (pair ? " (pair triple)" : ""));
      const Context c = pair ? triple(ok) : per_species(ok);
      const auto spec = D_Base::parse_watom(c);
      const std::vector<double> w = D_Base::density_pair_weights(c);
      REQUIRE(spec.values.size() == 1);
      REQUIRE(w.size() == 1);
      CHECK(spec.values[0] == w[0]);
      CHECK(w[0] == std::strtod(ok.c_str(), nullptr));
    }
  }
}
