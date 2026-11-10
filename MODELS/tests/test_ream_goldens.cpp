/* FEAT-02 goldens: the DM_REAM density paths against the bits of the density
 * before RHOFORM (every tadah before 1.6.0).
 *
 * ream_goldens_data.h holds, for 5 families (Blip, Blip5, Knot5, Knot3,
 * Gaussian) x 3 cutoffs (Cut_Dummy, Cut_Cos, Cut_Poly) x 2 amplitude sets
 * (signed, all non-negative), calc_rho's increment, the calc_dXijdri force
 * prefactor and eam_density's rho and rho' at 56-64 distances in (0, rcut],
 * captured as hex floats from the tree before the density forms (MODELS
 * 4c4144b) on x86-64. ream_density_before_rhoform.cpp is a
 * frozen copy of that tree's code: it gives the density before RHOFORM on
 * the platform the test runs on. RHOFORM SIGNED must reproduce it bit for
 * bit on both amplitude sets, and RHOFORM NONNEG on the non-negative set
 * (every cutoff here is >= 0, so the max(f_c, 0) clamp is inert).
 *
 * The density calls the C library's cos (Cut_Cos), sin (its derivative)
 * and exp (Gaussian), whose results the C standard does not fix to the
 * bit. C libraries, and the code paths one C library takes on different
 * CPUs, round some arguments one ulp apart, which moves a density value by
 * one ulp, or by more where the terms of the sum cancel: calc_rho of the
 * Blip Cut_Cos signed case at r = 0x1.e60aa64c2f838p-1 is
 * 0x1.4f8f2a2a6d567p-2 with musl (Alpine 3.19) against the stored
 * 0x1.4f8f2a2a6d566p-2 (1 ulp), and that of the Gaussian Cut_Cos signed
 * case at r = 0x1.064f765fd8adap+1 is -0x1.8a1e1fc8fd51p-14 with the cos,
 * sin and exp that glibc 2.43 takes on an x86-64 CPU without AVX2 and FMA
 * against the stored -0x1.8a1e1fc8fd5ep-14 (208 ulp, from one exp result
 * one ulp apart), for the code before RHOFORM and for SIGNED alike.
 * Hence the density before RHOFORM is computed here, on the platform, with
 * the C library's functions, for the cases that compare RHOFORM with it.
 * The stored goldens anchor the frozen copy with the results of cos, sin
 * and exp recorded in ream_goldens_libm.h, which the copy is given in
 * place of the C library's: bit for bit whatever the C library.
 */
#include "tadah_test_skip.hpp"  // includes catch2/catch.hpp
#include "ream_density_before_rhoform.h"
#include "ream_goldens_data.h"
#include "ream_goldens_libm.h"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/number_format.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/dm_ream.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

namespace before = ream_before_rhoform;

struct GoldenFixture {
  GoldenFixture() { PeriodicTable::initialize(); }
};

std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof u);
  return u;
}

std::string hex(double v) {
  std::ostringstream os;
  os << std::hexfloat << v;
  return os.str();
}

int count_tokens(const char *s) {
  std::istringstream is(s);
  std::string t;
  int n = 0;
  while (is >> t) ++n;
  return n;
}

std::vector<double> numbers(const char *s) {
  std::istringstream is(s);
  std::vector<double> out;
  std::string t;
  while (is >> t) {
    double v = 0.0;
    REQUIRE(parse_number(t, v));  // the reader of DM_REAM's grids
    out.push_back(v);
  }
  return out;
}

// The context goldens/ream_goldens.cpp built (make_ctx), plus RHOFORM.
Context golden_ctx(const ream_goldens::Case &gc, const char *form) {
  const bool knot = gc.bcode == 2;
  const int n = count_tokens(gc.cgrid);
  Context c;
  c.add("VERBOSE", 0);
  c.add("ATOM", "Ta");
  c.add("WATOM", 0.8125);
  c.add("INITMB", "true");
  c.add("RCUTMB", 5.5);
  c.add("RCTYPEMB", gc.cutoff);
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", gc.bcode);
  c.add("TYPEMB", n); c.add("TYPEMB", knot ? 0 : n); c.add("TYPEMB", n);
  c.add("TYPEMB", 3); c.add("TYPEMB", knot ? 0 : 3);
  c.add("TYPEMB", knot ? n : 0); c.add("TYPEMB", 0);
  c.add("TYPEMB", "Ta");
  c.add("TYPEMB", "Ta");
  c.add("CGRIDMB", gc.cgrid);
  if (!knot) c.add("SGRIDMB", gc.sgrid);
  c.add("AMPGRIDMB", gc.amp);
  c.add("CEMBFUNC", "0.0 0.5 1.0");
  if (!knot) c.add("SEMBFUNC", "1.0 1.0 1.0");
  if (knot) c.add("KDEGMB", gc.kdeg);
  c.add("RHOFORM", form);
  return c;
}

// The same density for the frozen copy of the code before RHOFORM.
before::Density golden_density(const ream_goldens::Case &gc) {
  before::Density d;
  d.bcode = gc.bcode;
  d.cutoff = gc.cutoff;
  d.rcut = 5.5;
  d.w = 0.8125;
  d.c = numbers(gc.cgrid);
  d.s = numbers(gc.sgrid);
  d.a = numbers(gc.amp);
  d.kdeg = numbers(gc.kdeg);
  return d;
}

// The density before RHOFORM at every golden point of a case, on this
// platform.
std::vector<before::Values> before_rhoform(const ream_goldens::Case &gc) {
  const before::Density d = golden_density(gc);
  const before::Libm lm = before::platform_libm();
  std::vector<before::Values> out;
  for (std::size_t k = 0; k < gc.npts; ++k)
    out.push_back(before::evaluate(d, gc.pts[k].r, lm));
  return out;
}

// Every golden point of one case through a DM_REAM in @p form against the
// density before RHOFORM on this platform; returns the number of values
// whose bits differ and describes the first one.
std::size_t mismatches(const ream_goldens::Case &gc, const char *form,
                       std::string &first) {
  Context c = golden_ctx(gc, form);
  DM_REAM dm(c);
  const std::vector<before::Values> ref = before_rhoform(gc);
  const int Z = PeriodicTable::find_by_symbol("Ta").Z;
  const Vec3d unit(1.0, 0.0, 0.0);
  std::size_t bad = 0;
  for (std::size_t k = 0; k < gc.npts; ++k) {
    const double r = gc.pts[k].r;

    rho_type rho;
    dm.init_rhoi(rho);
    rho.set_zero();
    dm.calc_rho(Z, Z, r, r * r, unit, rho, 1.0);

    rho_type rhoi;
    dm.init_rhoi(rhoi);
    rhoi.set_zero();
    for (std::size_t m = 0; m < dm.size(); ++m) rhoi(dm.get_drfidx() + m) = 1.0;
    fd_type fd(dm.size());
    fd.set_zero();
    dm.calc_dXijdri(Z, Z, r, r * r, unit, rhoi, fd, 1.0);

    double er, erp;
    dm.eam_density(Z, Z, r, er, erp);

    const double got[4] = {rho(dm.get_rfidx()), fd(0, 0), er, erp};
    const double want[4] = {ref[k].rho, ref[k].pref, ref[k].eam_rho,
                            ref[k].eam_rhop};
    const char *what[4] = {"calc_rho", "calc_dXijdri pref", "eam_density rho",
                           "eam_density rho'"};
    for (int q = 0; q < 4; ++q) {
      if (bits(got[q]) == bits(want[q])) continue;
      if (bad++ == 0)
        first = std::string(what[q]) + " at r = " + hex(r) + ": got " +
                hex(got[q]) + ", before RHOFORM " + hex(want[q]);
    }
  }
  return bad;
}

/* A C library that gives the results recorded in ream_goldens_libm.h: each
 * function looks its argument up by its bits and returns the recorded
 * result. It counts the calls at an argument that has no recorded result
 * (and returns a NaN for them) and marks the results it gave, so that the
 * anchor sees a copy that calls a function at another argument, or that
 * does not reach it through before::Libm. */
struct Replay {
  const char *name;
  const ream_goldens_libm::Result *tab;
  std::size_t n;
  std::unordered_map<std::uint64_t, std::size_t> at;  // bits of x -> entry
  std::vector<bool> used;
  std::size_t missing = 0;
  std::string first_missing;

  Replay(const char *nm, const ream_goldens_libm::Result *t, std::size_t k)
      : name(nm), tab(t), n(k) {
    for (std::size_t i = 0; i < k; ++i) at.emplace(bits(t[i].x), i);
    reset();
  }
  void reset() {
    used.assign(n, false);
    missing = 0;
    first_missing.clear();
  }
  double operator()(double x) {
    const auto it = at.find(bits(x));
    if (it == at.end()) {
      if (missing++ == 0) first_missing = std::string(name) + "(" + hex(x) + ")";
      return std::numeric_limits<double>::quiet_NaN();
    }
    used[it->second] = true;
    return tab[it->second].y;
  }
  std::size_t unused() const {
    return static_cast<std::size_t>(std::count(used.begin(), used.end(), false));
  }
};

#define TADAH_REAM_TABLE(t) (t), sizeof(t) / sizeof((t)[0])
Replay &replay_cos() {
  static Replay r("cos", TADAH_REAM_TABLE(ream_goldens_libm::kCos));
  return r;
}
Replay &replay_sin() {
  static Replay r("sin", TADAH_REAM_TABLE(ream_goldens_libm::kSin));
  return r;
}
Replay &replay_exp() {
  static Replay r("exp", TADAH_REAM_TABLE(ream_goldens_libm::kExp));
  return r;
}
#undef TADAH_REAM_TABLE
double recorded_cos(double x) { return replay_cos()(x); }
double recorded_sin(double x) { return replay_sin()(x); }
double recorded_exp(double x) { return replay_exp()(x); }

// The results of one function of ream_goldens_libm.h against @p f, the C
// library's: the number of arguments at which they differ, and the first.
std::size_t differing(const ream_goldens_libm::Result *tab, std::size_t n,
                      double (*f)(double), const char *name,
                      std::string &first) {
  std::size_t d = 0;
  for (std::size_t i = 0; i < n; ++i) {
    volatile double x = tab[i].x;  // the call happens here, not at compile time
    const double y = f(x);
    if (bits(y) == bits(tab[i].y)) continue;
    ++d;
    if (first.empty())
      first = std::string(name) + "(" + hex(tab[i].x) + ") = " + hex(y) +
              ", recorded " + hex(tab[i].y);
  }
  return d;
}

}  // namespace

/* Pins: the C library this program runs with gives one or more of the
 * results of cos, sin and exp recorded in ream_goldens_libm.h differently
 * (the functions the frozen copy calls, before::platform_libm()). Run only
 * by the CTest entry tadah.models_ream_goldens_glibc_no_fma_libm
 * (tests/CMakeLists.txt), next to tadah.models_ream_goldens_glibc_no_fma,
 * which runs the [goldens] cases under the same setting: it shows that
 * those cases ran with other results than the recorded ones, and is
 * Skipped where the C library gives every recorded result (the setting
 * then changed nothing).
 * Before the fix: new; passes with glibc 2.43 on x86-64 under
 * GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA (1 result of 501 differs:
 * exp(-0x1.1839f3fdcf5f8p+1) = 0x1.cabf14e03b9b2p-4, recorded
 * 0x1.cabf14e03b9b3p-4), and is Skipped without it. */
TEST_CASE("DM_REAM goldens: this C library's cos, sin and exp differ from "
          "the recorded results",
          "[.ream_goldens_libm]") {
  namespace lib = ream_goldens_libm;
  const before::Libm lm = before::platform_libm();
  const std::size_t ncos = sizeof(lib::kCos) / sizeof(lib::kCos[0]);
  const std::size_t nsin = sizeof(lib::kSin) / sizeof(lib::kSin[0]);
  const std::size_t nexp = sizeof(lib::kExp) / sizeof(lib::kExp[0]);
  std::string first;
  const std::size_t n = differing(lib::kCos, ncos, lm.cos, "cos", first) +
                        differing(lib::kSin, nsin, lm.sin, "sin", first) +
                        differing(lib::kExp, nexp, lm.exp, "exp", first);
  if (n == 0) {
    TADAH_SKIP("this C library gives every recorded result of cos, sin and "
               "exp (" << ncos + nsin + nexp << "): the golden cases run "
               "with it checked no other results");
  }
  SUCCEED(n << " of " << ncos + nsin + nexp
            << " recorded results differ; the first: " << first);
}

/* Pins: the frozen copy of the code before RHOFORM, given the results of
 * cos, sin and exp recorded in ream_goldens_libm.h in place of the C
 * library's, gives every stored golden value (1770 points, 4 values each)
 * bit for bit, and calls cos, sin and exp at the recorded arguments only,
 * each of them. The C library's own cos, sin and exp enter none of these
 * values, so the case holds with any C library: a copy whose arithmetic or
 * arguments drift from the code it copies fails here on every platform.
 * The goldens are the bits of that arithmetic without fused multiply-adds,
 * as MODELS' flags compile the copy on x86-64 (never -march=native,
 * tests/CMakeLists.txt). The cases below compare RHOFORM with the copy run
 * with the C library's own cos, sin and exp.
 * Before the fix: the copy ran with the C library's cos, sin and exp, and
 * had to give the stored goldens bit for bit wherever __GLIBC__ is defined
 * (within 32 eps of a scale of each value elsewhere); it failed with glibc
 * 2.43 on x86-64 under GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA, the
 * cos, sin and exp of a CPU without AVX2 and FMA, which give one exp
 * result differently: '8 == 0', 8 values differ, the first 'Gaussian
 * Cut_Cos signed, calc_rho at r = 0x1.064f765fd8adap+1: computed
 * -0x1.8a1e1fc8fd51p-14, stored -0x1.8a1e1fc8fd5ep-14'. */
TEST_CASE_METHOD(GoldenFixture,
                 "DM_REAM goldens: the density before RHOFORM computed here "
                 "from the recorded C library results is the stored goldens "
                 "bit for bit",
                 "[ream][rhoform][goldens]") {
  Replay *const libm[3] = {&replay_cos(), &replay_sin(), &replay_exp()};
  for (Replay *f : libm) {
    INFO(f->name << ": one recorded result per argument");
    REQUIRE(f->at.size() == f->n);
    f->reset();
  }
  const before::Libm recorded{&recorded_cos, &recorded_sin, &recorded_exp};
  std::size_t values = 0, differ = 0;
  std::string first;
  for (const auto &gc : ream_goldens::kCases) {
    const before::Density d = golden_density(gc);
    for (std::size_t k = 0; k < gc.npts; ++k) {
      const ream_goldens::Point &p = gc.pts[k];
      const before::Values v = before::evaluate(d, p.r, recorded);
      const double got[4] = {v.rho, v.pref, v.eam_rho, v.eam_rhop};
      const double want[4] = {p.rho, p.pref, p.eam_rho, p.eam_rhop};
      const char *what[4] = {"calc_rho", "calc_dXijdri pref",
                             "eam_density rho", "eam_density rho'"};
      for (int q = 0; q < 4; ++q) {
        ++values;
        if (bits(got[q]) == bits(want[q])) continue;
        if (differ++ == 0)
          first = std::string(gc.family) + " " + gc.cutoff +
                  (gc.nonneg_amps ? " non-negative" : " signed") + ", " +
                  what[q] + " at r = " + hex(p.r) + ": computed " +
                  hex(got[q]) + ", stored " + hex(want[q]);
      }
    }
  }
  REQUIRE(values > 4 * 1500);  // the table is present and complete
  for (Replay *f : libm) {
    INFO(f->name << ": " << f->missing
                 << " calls at an argument with no recorded result; the "
                    "first: "
                 << f->first_missing);
    CHECK(f->missing == 0);
    INFO(f->name << ": " << f->unused() << " of " << f->n
                 << " recorded results not used");
    CHECK(f->unused() == 0);
  }
  INFO("first value that differs: " << first);
  CHECK(differ == 0);
}

/* Pins: RHOFORM SIGNED gives the density before RHOFORM, computed here on
 * this platform by the frozen copy, bit for bit at every golden point, on
 * both amplitude sets.
 * Before the fix: passes as well (a pin of DM_REAM; the defect was this
 * test's reference, the stored goldens, the density with the cos, sin and
 * exp results of ream_goldens_libm.h, which differs from the density with
 * a C library that rounds one of those arguments differently: with musl,
 * Alpine 3.19, it failed for every Cut_Cos case and the Gaussian Cut_Poly
 * cases, 'calc_rho at r = 0x1.e60aa64c2f838p-1: got 0x1.4f8f2a2a6d567p-2,
 * before RHOFORM 0x1.4f8f2a2a6d566p-2', and with glibc 2.43 on x86-64
 * under GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA for Gaussian Cut_Cos
 * signed and both Gaussian Cut_Poly cases, at r = 0x1.064f765fd8adap+1). */
TEST_CASE_METHOD(GoldenFixture,
                 "DM_REAM goldens: RHOFORM SIGNED is bitwise the density before RHOFORM "
                 "(signed and non-negative amplitudes)",
                 "[ream][rhoform][goldens]") {
  std::size_t npts = 0;
  for (const auto &gc : ream_goldens::kCases) {
    INFO(gc.family << " " << gc.cutoff
                   << (gc.nonneg_amps ? " non-negative" : " signed")
                   << " amplitudes");
    std::string first;
    CHECK(mismatches(gc, "SIGNED", first) == 0);
    INFO(first);
    CHECK(first.empty());
    npts += gc.npts;
  }
  REQUIRE(npts > 1500);  // the table is present and complete
}

/* Pins: RHOFORM NONNEG gives the density before RHOFORM, computed here on
 * this platform by the frozen copy, bit for bit at every golden point of
 * the 15 non-negative amplitude sets.
 * Before the fix: passes as well (a pin; against the stored goldens it
 * failed with musl, Alpine 3.19, for every Cut_Cos case and Gaussian
 * Cut_Poly, and with glibc 2.43 under
 * GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA for Gaussian Cut_Poly). */
TEST_CASE_METHOD(GoldenFixture,
                 "DM_REAM goldens: RHOFORM NONNEG is bitwise the density before RHOFORM "
                 "on non-negative amplitudes",
                 "[ream][rhoform][goldens]") {
  std::size_t ncases = 0;
  for (const auto &gc : ream_goldens::kCases) {
    if (!gc.nonneg_amps) continue;
    ++ncases;
    INFO(gc.family << " " << gc.cutoff);
    std::string first;
    CHECK(mismatches(gc, "NONNEG", first) == 0);
    INFO(first);
    CHECK(first.empty());
  }
  REQUIRE(ncases == 15);
}

TEST_CASE_METHOD(GoldenFixture,
                 "DM_REAM goldens: NONNEG refuses the signed amplitude sets",
                 "[ream][rhoform][goldens]") {
  for (const auto &gc : ream_goldens::kCases) {
    if (gc.nonneg_amps) continue;
    INFO(gc.family << " " << gc.cutoff);
    Context c = golden_ctx(gc, "NONNEG");
    REQUIRE_THROWS_AS(DM_REAM(c), DensityFormError);
  }
}

/* Pins: the density a program gets from ReamDensity is the code of
 * src/ream_density.cpp, compiled with Tadah.MODELS' flags, also when the
 * program calls ReamDensity itself (DM_REAM::density()) rather than
 * through DM_REAM: for every golden case under RHOFORM SIGNED, with f_c and
 * f_c' of a cutoff of the case's RCTYPEMB (RCUTMB 5.5) built here,
 * w * value(r, f_c) is calc_rho's increment before RHOFORM, and
 * value_deriv(r, f_c, f_c') times w gives eam_density's rho and rho'
 * before RHOFORM, bit for bit (w = 0.8125, golden_ctx's WATOM; the density
 * before RHOFORM computed here on this platform by the frozen copy). In
 * tadah.models_ream_goldens_native_link this unit is compiled with
 * -march=native: a ReamDensity method moved into ream_density.h would be
 * compiled here, with those flags, and fuse its multiply-adds into FMAs on
 * an FMA machine.
 * Before the fix: passes as well (a pin). With deg, width, value, deriv
 * and value_deriv moved into ream_density.h as inline functions it fails in
 * the native-link program at every golden case (GCC 15.2, LTO), and so do
 * the other golden cases there: the link keeps one copy of an inline
 * function, this unit's -march=native one, and DM_REAM's methods call it.
 * Without this case, whose unit calls the density itself, that move
 * passed. Its reference was the stored goldens, with which it failed with
 * musl (Alpine 3.19) for every Cut_Cos case and Gaussian Cut_Poly, and
 * with glibc 2.43 under GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA for
 * Gaussian Cut_Cos signed and both Gaussian Cut_Poly cases. */
TEST_CASE_METHOD(GoldenFixture,
                 "DM_REAM goldens: ReamDensity called by the program gives the "
                 "density before RHOFORM",
                 "[ream][rhoform][goldens]") {
  const double w = 0.8125;
  std::size_t npts = 0;
  for (const auto &gc : ream_goldens::kCases) {
    INFO(gc.family << " " << gc.cutoff
                   << (gc.nonneg_amps ? " non-negative" : " signed")
                   << " amplitudes");
    Context c = golden_ctx(gc, "SIGNED");
    const DM_REAM dm(c);
    const std::vector<before::Values> ref = before_rhoform(gc);
    double rcut = 5.5;
    const std::unique_ptr<Cut_Base> cut(
        factory<Cut_Base, double &>(gc.cutoff, rcut));
    std::size_t bad = 0;
    std::string first;
    for (std::size_t k = 0; k < gc.npts; ++k) {
      const double r = gc.pts[k].r;
      const double fc = cut->calc(r), fcp = cut->calc_prime(r);
      double v = 0.0, d = 0.0;
      dm.density().value_deriv(r, fc, fcp, v, d);
      const double got[3] = {w * dm.density().value(r, fc), w * v, w * d};
      const double want[3] = {ref[k].rho, ref[k].eam_rho, ref[k].eam_rhop};
      const char *what[3] = {"w value", "w value_deriv rho",
                             "w value_deriv rho'"};
      for (int q = 0; q < 3; ++q) {
        if (bits(got[q]) == bits(want[q])) continue;
        if (bad++ == 0)
          first = std::string(what[q]) + " at r = " + hex(r) + ": got " +
                  hex(got[q]) + ", before RHOFORM " + hex(want[q]);
      }
    }
    INFO(first);
    CHECK(bad == 0);
    npts += gc.npts;
  }
  REQUIRE(npts > 1500);
}
