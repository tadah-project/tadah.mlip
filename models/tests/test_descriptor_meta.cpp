// Catch2 v2 tests for the per-descriptor metadata schema of the HPO
// bound-validation pipeline. Verifies:
//   * D_Base::meta() default returns nullopt
//   * D2_Blip::meta() returns a DescriptorMeta with the expected
//     parameter set, suggester output, and column counter
//   * DM_Blip::meta() likewise + cross-param size check
//   * DM_mEAD<F_mEnv<Cut_SinInv>>::meta() (i.e. DM_mBlipEnv) flags a
//     collapsed ρ: a radial GTO centre (CGRIDMB) outside the data's
//     neighbour distances makes every atom's density vanish

#include "catch2/catch.hpp"

#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/dm/dm_blip.h>
#include <tadah/models/descriptors/dm/dm_mEAD.h>
#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_all.h>
#include <tadah/models/cutoffs.h>

#include <tadah/core/context.h>
#include <tadah/core/stats/data_stats.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>

using namespace tadah::models;
using tadah::core::Context;
using tadah::core::stats::DataStats;
using tadah::core::stats::PerPairStats;

// ----------------------------------------------------------------------
// Helper: build a synthetic DataStats with a single Ta-Ta pair whose
// r_ij percentiles bracket typical bcc Ta neighbour distances.
// ----------------------------------------------------------------------
static DataStats make_ta_like_stats() {
  DataStats d;
  d.n_atoms = 1000;
  d.n_structures = 50;
  d.elements = {"Ta"};
  PerPairStats pp;
  pp.elements = {"Ta", "Ta"};
  pp.n_pairs  = 50000;
  pp.r_min    = 1.61;
  pp.r_max    = 6.5;
  // p1 .. p99 (mean, std)
  pp.r_percentiles = {2.33, 2.55, 2.85, 3.0, 5.03, 5.5, 6.30, 6.4, 6.48,
                      4.5, 1.5};
  // One KDE peak near 2.86 with FWHM ~ 0.3
  tadah::core::stats::KdePeak peak;
  peak.position = 2.86;
  peak.density  = 1.0;
  peak.fwhm     = 0.30;
  pp.r_peaks    = {peak};
  d.per_pair    = {pp};
  return d;
}

static DataStats make_ta_like_stats_with_collapsed_rho() {
  auto d = make_ta_like_stats();
  // ρ-collapse: every percentile near zero
  d.rho_percentiles_per_descriptor["DM_mBlipEnv"] = {
      0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0e-10, 0.0, 0.0};
  return d;
}

static DataStats make_ta_like_stats_with_healthy_rho() {
  auto d = make_ta_like_stats();
  d.rho_percentiles_per_descriptor["DM_mBlipEnv"] = {
      // p1, p5, p10, p25, p50, p75, p90, p95, p99, mean, std
      1.0, 2.0, 4.6, 5.5, 5.81, 6.2, 6.68, 6.9, 7.2,
      5.5, 1.0};
  return d;
}

static Context make_context_with_optim_keys() {
  Context c;
  c.add<bool>("INIT2B", true);
  c.add<bool>("INITMB", true);
  c.add<double>("RCUT2B", 5.6);
  c.add<double>("RCUTMB", 5.6);
  c.add<double>("RCUTENV", 5.0);
  c.add<std::string>("RCTYPE2B", "Cut_Poly");
  c.add<std::string>("RCTYPEMB", "Cut_Poly");
  // Vector grids — 7 entries each.
  for (int i = 0; i < 7; ++i) {
    c.add<double>("CGRID2B", 0.5 * (i + 1));
    c.add<double>("SGRID2B", 1.0);
    c.add<double>("CGRIDMB", 0.5 * (i + 1));
    c.add<double>("SGRIDMB", 1.0);
    c.add<double>("CEMBFUNC", 1.1 + 0.1 * i);
    c.add<double>("SEMBFUNC", 1.0);
  }
  c.add<std::string>("TYPE2B", "D2_Blip"); c.add<int>("TYPE2B", 7);
  c.add<int>("TYPE2B", 7); c.add<std::string>("TYPE2B", "Ta");
  c.add<std::string>("TYPE2B", "Ta");
  c.add<std::string>("TYPEMB", "DM_Blip"); c.add<int>("TYPEMB", 0);
  return c;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

TEST_CASE("D_Base::meta default is nullopt for descriptors that do not override") {
  // DM_Dummy is registered but its metadata has not been ported.
  // factory<>() requires a definition of registry — DM_Dummy lives under
  // Registry<D_Base> which is already defined in d2_all.cpp. Construct
  // and assert default behaviour.
  auto* p = tadah::core::factory<D_Base>("DM_Dummy");
  REQUIRE(p != nullptr);
  REQUIRE_FALSE(p->meta().has_value());
  delete p;
}

TEST_CASE("D2_Blip::meta returns a populated DescriptorMeta") {
  D2_Blip d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  REQUIRE(m->label == "D2_Blip");
  REQUIRE(m->params.size() == 3);
  // RCUT2B, CGRID2B, SGRID2B in this order
  REQUIRE(m->params[0].key == "RCUT2B");
  REQUIRE(m->params[1].key == "CGRID2B");
  REQUIRE(m->params[2].key == "SGRID2B");
  // Hard limits: SGRID2B must be > 0
  REQUIRE(m->params[2].hard.lo.has_value());
  REQUIRE(m->params[2].hard.lo.value() > 0.0);
  // RCUT2B is critical
  REQUIRE(m->params[0].crit == Criticality::Critical);
  // CGRID2B is per-pair
  REQUIRE(m->params[1].scope == PairScope::PerElementPair);
  // expected_columns lambda is set
  REQUIRE(static_cast<bool>(m->expected_columns));
}

TEST_CASE("D2_Blip suggester anchors RCUT2B to the data's r_ij percentiles") {
  D2_Blip d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  Context ctx = make_context_with_optim_keys();
  SeedView sv(ctx);
  auto data = make_ta_like_stats();
  auto bracket = m->params[0].suggest(data, sv, -1);
  // Lower bound should sit between 2NN peak (none in our synthetic
  // KdePeaks; only peak at 2.86) and p99.
  REQUIRE(bracket.first  > 0.0);
  REQUIRE(bracket.second  > bracket.first);
  REQUIRE(bracket.second <= data.per_pair[0].r_percentiles[8]);  // <= p99
}

TEST_CASE("D2_Blip cross-param check fails on size mismatch") {
  D2_Blip d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  REQUIRE(m->cross_param_checks.size() == 1);

  Context ctx = make_context_with_optim_keys();
  // Make CGRID2B 8 entries while SGRID2B is 7 -> mismatch
  ctx.add<double>("CGRID2B", 99.0);
  SeedView sv(ctx);
  auto v = m->cross_param_checks[0](DataStats{}, sv);
  REQUIRE(is_fail(v));
  REQUIRE(verdict_message(v).find("CGRID2B") != std::string::npos);
}

TEST_CASE("DM_Blip::meta returns expected_columns scaling with TYPEMB Lmax") {
  DM_Blip d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  REQUIRE(m->label == "DM_Blip");
  Context ctx = make_context_with_optim_keys();
  SeedView sv(ctx);
  // 7 CGRIDMB entries x (Lmax+1) = 7 * 1 = 7 (Lmax=0 in the ctx above)
  REQUIRE(m->expected_columns(sv) == 7);
}

TEST_CASE("DM_mBlipEnv::meta: CGRIDMB is critical and the cross-param check "
          "fails a collapsed ρ") {
  // The descriptor type is DM_mEAD<F_mEnv<Cut_SinInv>>. We exercise it
  // via meta() through a DM_mEAD<F_mEnv<Cut_SinInv>> instance directly.
  DM_mEAD<F_mEnv<Cut_SinInv>> d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  REQUIRE(m->label == "DM_mBlipEnv");
  REQUIRE(m->params.size() == 6);     // RCUTMB, CGRIDMB, SGRIDMB, RCUTENV, CEMBFUNC, SEMBFUNC

  // CGRIDMB is Critical: the pre-flight audit warns when it is left out
  // of the OPTIM search.
  auto cgridmb = std::find_if(m->params.begin(), m->params.end(),
      [](const ParamMeta& p){ return p.key == "CGRIDMB"; });
  REQUIRE(cgridmb != m->params.end());
  REQUIRE(cgridmb->crit == Criticality::Critical);

  // Cross-param check fires Fail when ρ percentiles all collapsed.
  REQUIRE(!m->cross_param_checks.empty());
  Context ctx = make_context_with_optim_keys();
  SeedView sv(ctx);
  auto v_collapse = m->cross_param_checks[0](make_ta_like_stats_with_collapsed_rho(), sv);
  REQUIRE(is_fail(v_collapse));
  REQUIRE(verdict_message(v_collapse).find("collapse") != std::string::npos);

  // And returns Ok when ρ is healthy.
  auto v_ok = m->cross_param_checks[0](make_ta_like_stats_with_healthy_rho(), sv);
  REQUIRE(is_ok(v_ok));
}

/* Pins: a narrow radial GTO at the origin fails the CGRIDMB check: with
 * CGRIDMB 0 and SGRIDMB 6, exp(-6 r_min^2) is 1.8e-7 at the smallest
 * neighbour distance (r_min 1.61), so ρ is ~0 for every atom; the message
 * names CGRIDMB and ρ.
 * Before the fix: passes as well (a pin: the check compared the centre
 * alone with [r_min, r_max], so a centre at 0 failed whatever its width;
 * this case had no SGRIDMB). */
TEST_CASE("DM_mBlipEnv per-param check rejects out-of-range CGRIDMB") {
  DM_mEAD<F_mEnv<Cut_SinInv>> d;
  auto m = d.meta();
  REQUIRE(m.has_value());

  auto cgridmb = std::find_if(m->params.begin(), m->params.end(),
      [](const ParamMeta& p){ return p.key == "CGRIDMB"; });
  REQUIRE(cgridmb != m->params.end());
  REQUIRE(static_cast<bool>(cgridmb->check));

  // CGRIDMB = 0.0 with a narrow width: below 1e-6 at r_min = 1.61
  Context ctx;
  ctx.add<double>("CGRIDMB", 0.0);
  ctx.add<double>("SGRIDMB", 6.0);
  SeedView sv(ctx);
  auto v = cgridmb->check(make_ta_like_stats(), sv);
  REQUIRE(is_fail(v));
  REQUIRE(verdict_message(v).find("CGRIDMB") != std::string::npos);
  REQUIRE(verdict_message(v).find("ρ") != std::string::npos);
}

/* Pins: without a seed centre (no CGRIDMB) the CGRIDMB suggestion of
 * DM_mBlipEnv is the first KDE peak +- its FWHM, 2.56 to 3.16.
 * Before the fix: passes as well (a pin; the case read the seed's CGRIDMB
 * 0.5, which got the first-peak range too: the seed's centre was not
 * read, and a centre of 0.5 now gets a range about it, as the case below
 * pins). */
TEST_CASE("DM_mBlipEnv suggester places CGRIDMB on the first KDE peak") {
  DM_mEAD<F_mEnv<Cut_SinInv>> d;
  auto m = d.meta();
  REQUIRE(m.has_value());

  auto cgridmb = std::find_if(m->params.begin(), m->params.end(),
      [](const ParamMeta& p){ return p.key == "CGRIDMB"; });
  REQUIRE(cgridmb != m->params.end());
  REQUIRE(static_cast<bool>(cgridmb->suggest));

  Context ctx = make_context_with_optim_keys();
  ctx.remove("CGRIDMB");
  SeedView sv(ctx);
  auto bracket = cgridmb->suggest(make_ta_like_stats(), sv, -1);
  // Peak at 2.86, fwhm 0.30 -> bracket [2.56, 3.16]
  REQUIRE(bracket.first  == Approx(2.56).margin(0.01));
  REQUIRE(bracket.second == Approx(3.16).margin(0.01));
}

TEST_CASE("DM_mBlipEnv expected_columns equals CEMBFUNC size") {
  DM_mEAD<F_mEnv<Cut_SinInv>> d;
  auto m = d.meta();
  REQUIRE(m.has_value());
  Context ctx = make_context_with_optim_keys();    // 7 CEMBFUNC entries
  SeedView sv(ctx);
  REQUIRE(m->expected_columns(sv) == 7);
}

// ----------------------------------------------------------------
// Coverage audit
//
// Assert that every registered D_Base subclass either has a populated
// meta() or is on the exempt list (Dummy / mJoin meta-descriptors).
// This is the regression that catches future descriptor additions
// being landed without a `meta()` declaration.
// ----------------------------------------------------------------

#include <set>
#include <tadah/core/registry.h>

TEST_CASE("descriptor coverage: every registered descriptor has meta()") {
  // Descriptors that intentionally return `nullopt` (placeholders /
  // meta-descriptors that delegate to nested children).
  const std::set<std::string> exempt = {
    "D2_Dummy", "D2_mJoin",
    "DM_Dummy", "DM_mJoin",
  };

  auto names = tadah::core::Registry<D_Base>::names();
  REQUIRE(!names.empty());                     // sanity

  std::vector<std::string> missing;
  for (const auto& name : names) {
    if (exempt.count(name)) continue;
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(name));
    if (!d->meta().has_value()) missing.push_back(name);
  }

  CAPTURE(missing);
  REQUIRE(missing.empty());
}

// ----------------------------------------------------------------
// Width bounds follow the units of the width coordinate. The compact
// bump families (D2_Blip, D2_Blip5, DM_Blip) evaluate b(eta (r - c)) with
// support |eta (r - c)| < 2: eta is an inverse distance (Å^-1), so the
// suggested bounds scale as 1/length when every distance in the data is
// scaled. The Gaussian families (D2_BP, DM_EAD) evaluate exp(-eta (r -
// c)^2): eta is an inverse squared distance (Å^-2), scaling as 1/length^2.
// ----------------------------------------------------------------

namespace {
DataStats scaled_stats(const DataStats &d, double s) {
  DataStats out = d;
  for (auto &pp : out.per_pair) {
    pp.r_min *= s;
    pp.r_max *= s;
    for (auto &v : pp.r_percentiles) v *= s;
    for (auto &peak : pp.r_peaks) {
      peak.position *= s;
      peak.fwhm *= s;
    }
  }
  return out;
}
}  // namespace

/* Pins: for each family the suggested SGRID2B/SGRIDMB bounds of data whose
 * distances are doubled are the bounds of the original data divided by 2
 * (bump families, Å^-1) or by 4 (Gaussian families, Å^-2), matching the
 * units the metadata declares. Before the fix the bump families declared
 * Å^-1 but returned 0.25/s^2 .. 25/s^2 (D2) and 0.25/w^2 .. 9/w^2 (DM), so
 * doubling the distances divided their bounds by 4. The DM_Poly families
 * follow their radial basis: DM_PS_SQ (blip) Å^-1, DM_GPS_SQ (Gaussian)
 * Å^-2; before that fix DM_PS_SQ declared the Gaussian's Å^-2 and returned
 * its 0.25/w^2 .. 9/w^2 for a blip width. */
TEST_CASE("Suggested width bounds scale with the units of the width coordinate",
          "[unified-dm][metadata]") {
  const DataStats base = make_ta_like_stats();
  const DataStats twice = scaled_stats(base, 2.0);
  Context ctx = make_context_with_optim_keys();
  SeedView sv(ctx);
  struct Family { const char *label; const char *key; const char *units; int power; };
  for (const Family &f : {Family{"D2_Blip", "SGRID2B", "Å^-1", 1},
                          Family{"D2_Blip5", "SGRID2B", "Å^-1", 1},
                          Family{"DM_Blip", "SGRIDMB", "Å^-1", 1},
                          Family{"D2_BP", "SGRID2B", "Å^-2", 2},
                          Family{"DM_EAD", "SGRIDMB", "Å^-2", 2},
                          Family{"DM_PS_SQ", "SGRIDMB", "Å^-1", 1},
                          Family{"DM_GPS_SQ", "SGRIDMB", "Å^-2", 2}}) {
    CAPTURE(f.label);
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(f.label));
    auto meta = d->meta();
    REQUIRE(meta.has_value());
    const ParamMeta *width = nullptr;
    for (const auto &p : meta->params)
      if (p.key == f.key) width = &p;
    REQUIRE(width != nullptr);
    REQUIRE(width->units == f.units);
    const auto b1 = width->suggest(base, sv, 0);
    const auto b2 = width->suggest(twice, sv, 0);
    REQUIRE(b1.first > 0.0);
    REQUIRE(b1.second > b1.first);
    const double factor = std::pow(2.0, f.power);
    REQUIRE(b2.first == Approx(b1.first / factor).epsilon(1e-12));
    REQUIRE(b2.second == Approx(b1.second / factor).epsilon(1e-12));
  }
}

// ----------------------------------------------------------------
// The reach of a radial density channel. A density sums its radial
// function over the neighbours within its cutoff, so a channel is judged
// by what it gives at the neighbour distances of the data, not by where
// its centre lies: a Gaussian exp(-SGRIDMB (r - CGRIDMB)^2) misses them
// when it is below 1e-6 (the floor of the ρ-collapse check) at every
// distance from r_min to the smaller of r_max and RCUTMB, and a blip
// b(SGRIDMB (r - CGRIDMB)) when its support, |r - CGRIDMB| < 2/SGRIDMB,
// holds none of them.
// ----------------------------------------------------------------

namespace {

/* The CGRIDMB check of the descriptor @p label on make_ta_like_stats()
 * (r_min 1.61, r_max 6.5) for one channel: centre @p c, width @p eta,
 * RCUTMB 5.6. */
HealthVerdict reach_verdict(const std::string &label, double c, double eta) {
  std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
  auto meta = d->meta();
  REQUIRE(meta.has_value());
  const ParamMeta *centre = nullptr;
  for (const auto &p : meta->params)
    if (p.key == "CGRIDMB") centre = &p;
  REQUIRE(centre != nullptr);
  REQUIRE(static_cast<bool>(centre->check));
  Context ctx;
  ctx.add<double>("CGRIDMB", c);
  ctx.add<double>("SGRIDMB", eta);
  ctx.add<double>("RCUTMB", 5.6);
  SeedView sv(ctx);
  return centre->check(make_ta_like_stats(), sv);
}

}  // namespace

/* Pins: for the Gaussian densities (DM_mBlipEnv, DM_EAD, DM_mRLR, DM_GPS_SQ,
 * DM_GOW_SQ) the CGRIDMB check passes a centre at the origin whose width
 * reaches the neighbours (CGRIDMB 0, SGRIDMB 0.2: exp(-0.2 * 1.61^2) = 0.6,
 * the Ta seed of the HPO tests) and a centre beyond the cutoff whose tail
 * reaches them (CGRIDMB 8, SGRIDMB 0.2: 0.32 at RCUTMB 5.6), and fails a
 * narrow Gaussian at the origin (SGRIDMB 6: 1.8e-7 at r_min) and a centre
 * far outside (CGRIDMB 30), each message naming CGRIDMB and SGRIDMB.
 * Before the fix: CGRIDMB 0 and 8 failed whatever the width ("CGRIDMB=0.000000
 * is outside the dataset's r_ij range [...] -- ρ will be ~0 for every atom";
 * the centre alone was compared with [r_min, r_max]), and no message named
 * SGRIDMB. */
TEST_CASE("Density reach: a Gaussian channel fails only when it is below "
          "1e-6 at every neighbour distance",
          "[metadata][density_reach]") {
  for (const char *label :
       {"DM_mBlipEnv", "DM_EAD", "DM_mRLR", "DM_GPS_SQ", "DM_GOW_SQ"}) {
    CAPTURE(label);
    CHECK(is_ok(reach_verdict(label, 0.0, 0.2)));
    CHECK(is_ok(reach_verdict(label, 8.0, 0.2)));
    for (const auto &[c, eta] : {std::pair{0.0, 6.0}, std::pair{30.0, 0.2}}) {
      CAPTURE(c, eta);
      const HealthVerdict v = reach_verdict(label, c, eta);
      CHECK(is_fail(v));
      CHECK(verdict_message(v).find("CGRIDMB") != std::string::npos);
      CHECK(verdict_message(v).find("SGRIDMB") != std::string::npos);
    }
  }
}

/* Pins: for the blip densities (DM_Blip, DM_PS_SQ, DM_OW_SQ) the CGRIDMB
 * check passes a centre below r_min whose support reaches it (CGRIDMB 1,
 * SGRIDMB 1: |r - 1| < 2 holds 1.61 to 3) and one inside the data (CGRIDMB
 * 3), and fails a blip at the origin whose support ends before r_min
 * (SGRIDMB 2: |r| < 1) and one centred far outside (CGRIDMB 30), each
 * message naming CGRIDMB and SGRIDMB.
 * Before the fix: CGRIDMB 1 failed (below r_min 1.61, its support
 * ignored), and no message named SGRIDMB. */
TEST_CASE("Density reach: a blip channel fails only when its support holds "
          "no neighbour distance",
          "[metadata][density_reach]") {
  for (const char *label : {"DM_Blip", "DM_PS_SQ", "DM_OW_SQ"}) {
    CAPTURE(label);
    CHECK(is_ok(reach_verdict(label, 1.0, 1.0)));
    CHECK(is_ok(reach_verdict(label, 3.0, 1.0)));
    for (const auto &[c, eta] : {std::pair{0.0, 2.0}, std::pair{30.0, 1.0}}) {
      CAPTURE(c, eta);
      const HealthVerdict v = reach_verdict(label, c, eta);
      CHECK(is_fail(v));
      CHECK(verdict_message(v).find("CGRIDMB") != std::string::npos);
      CHECK(verdict_message(v).find("SGRIDMB") != std::string::npos);
    }
  }
}

// ----------------------------------------------------------------
// The suggested ranges of a Gaussian channel whose seed centre is not at
// the origin: the centre is searched about the seed's own centre, with the
// width of the neighbour shell nearest to it.
// ----------------------------------------------------------------

namespace {

/* Neighbour distances with three shells, at 2.86 A (FWHM 0.3), 3.30 A
 * (FWHM 0.24) and 4.67 A (FWHM 0.36); r_min 2.25 A, r_max 6.5 A. */
DataStats three_shell_stats() {
  DataStats d = make_ta_like_stats();
  auto &pp = d.per_pair[0];
  pp.r_min = 2.25;
  tadah::core::stats::KdePeak a, b, c;
  a.position = 2.86;
  a.density = 1.0;
  a.fwhm = 0.30;
  b.position = 3.30;
  b.density = 0.7;
  b.fwhm = 0.24;
  c.position = 4.67;
  c.density = 0.5;
  c.fwhm = 0.36;
  pp.r_peaks = {a, b, c};
  return d;
}

const ParamMeta *param_of(const DescriptorMeta &m, const char *key) {
  for (const auto &p : m.params)
    if (p.key == key) return &p;
  return nullptr;
}

}  // namespace

/* Pins: for a Gaussian channel of DM_EAD whose seed centre is not 0, the
 * CGRIDMB suggestion is the seed's centre +- the FWHM of the neighbour
 * shell nearest to it, its low end not below r_min and its high end not
 * above RCUTMB unless the seed's centre is (then the range ends at the
 * centre): on three_shell_stats() with CGRIDMB 1 2.9 3.3 4.7 5.9 (SGRIDMB
 * 4, RCUTMB 5.6) the ranges are 1 to 1.3, 2.6 to 3.2, 3.06 to 3.54, 4.34
 * to 5.06 and 5.54 to 5.9, each holding its seed's centre, and each
 * rationale names the seed's centre and the nearest shell; the SGRIDMB
 * suggestion of an entry is 0.25/w^2 to 9/w^2 with w the FWHM of that
 * shell (4.7: w = 0.36, 1.929 to 69.44), its rationale naming the shell.
 * Before the fix: every centre got the first shell's range, 2.56 to 3.16
 * (the first peak +- its FWHM), which holds none of the seed's centres but
 * 2.9, and every width that of the first shell (2.78 to 100), each
 * rationale naming the first neighbour shell. */
TEST_CASE("Gaussian density suggestions are centred on the seed's centre",
          "[metadata][density_reach][suggest]") {
  std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>("DM_EAD"));
  const auto meta = d->meta();
  REQUIRE(meta.has_value());
  const ParamMeta *centre = param_of(*meta, "CGRIDMB");
  const ParamMeta *width = param_of(*meta, "SGRIDMB");
  REQUIRE(centre != nullptr);
  REQUIRE(width != nullptr);
  Context ctx;
  ctx.add("CGRIDMB", "1 2.9 3.3 4.7 5.9");
  ctx.add("SGRIDMB", "4 4 4 4 4");
  ctx.add<double>("RCUTMB", 5.6);
  const SeedView sv(ctx);
  const DataStats data = three_shell_stats();
  struct Want { double c, lo, hi, shell; };
  const Want wants[] = {{1.0, 1.0, 1.3, 2.86},
                        {2.9, 2.6, 3.2, 2.86},
                        {3.3, 3.06, 3.54, 3.30},
                        {4.7, 4.34, 5.06, 4.67},
                        {5.9, 5.54, 5.9, 4.67}};
  for (int i = 0; i < 5; ++i) {
    const Want &w = wants[i];
    CAPTURE(i, w.c);
    const BoundSuggestion s = centre->suggest(data, sv, i);
    INFO(s.rationale);
    CHECK_FALSE(s.none);
    CHECK(s.first == Approx(w.lo).epsilon(1e-12));
    CHECK(s.second == Approx(w.hi).epsilon(1e-12));
    CHECK(s.first <= w.c);
    CHECK(s.second >= w.c);
    std::ostringstream shell;
    shell << w.shell;
    CHECK(s.rationale.find("seed's centre") != std::string::npos);
    CHECK(s.rationale.find("nearest") != std::string::npos);
    CHECK(s.rationale.find(shell.str()) != std::string::npos);
  }
  const BoundSuggestion sw = width->suggest(data, sv, 3);
  INFO(sw.rationale);
  CHECK(sw.first == Approx(0.25 / (0.36 * 0.36)).epsilon(1e-12));
  CHECK(sw.second == Approx(9.0 / (0.36 * 0.36)).epsilon(1e-12));
  CHECK(sw.rationale.find("nearest") != std::string::npos);
  CHECK(sw.rationale.find("4.67") != std::string::npos);
}

// ----------------------------------------------------------------
// The suggested ranges of a blip channel read the seed's centre, as those
// of a Gaussian channel do: a blip centred at the origin is searched by
// its width alone, whose range its reach sets; any other centre about
// itself, with the width of the neighbour shell nearest to it.
// ----------------------------------------------------------------

/* Pins: for the blip densities (DM_Blip, DM_PS_SQ, DM_OW_SQ) on
 * three_shell_stats() (r_min 2.25 A; shells at 2.86, 3.30 and 4.67 A,
 * FWHM 0.30, 0.24 and 0.36 A) with CGRIDMB 0 4.7, SGRIDMB 1 1, RCUTMB 5.6:
 *  - CGRIDMB(1), centred at the origin: no range (none), its rationale
 *    naming the origin;
 *  - SGRIDMB(1): the widths whose reach, the distance at which b(SGRIDMB
 *    r) falls to 1e-6 (x_f / SGRIDMB, x_f = 2 - (4e-6)^(1/3)), runs from
 *    r_min to the farthest neighbour distance within RCUTMB: x_f / 5.6 to
 *    x_f / 2.25, its rationale naming the origin and the reach;
 *  - CGRIDMB(2) = 4.7: the seed's centre +- the FWHM of the shell nearest
 *    to it (4.67 A, 0.36): 4.34 to 5.06, its rationale naming the seed's
 *    centre and the nearest shell;
 *  - SGRIDMB(2): 1/w to 6/w with w = 0.36, the FWHM of that shell, its
 *    rationale naming the shell.
 * Before the fix: the seed was not read: both centres got the first
 * shell's range, 2.56 to 3.16, and both widths 1/0.3 to 6/0.3 from the
 * first shell's FWHM, whatever the centre. */
TEST_CASE("Blip density suggestions read the seed's centre",
          "[metadata][density_reach][suggest]") {
  const double x_f = 2.0 - std::cbrt(4.0e-6);
  Context ctx;
  ctx.add("CGRIDMB", "0 4.7");
  ctx.add("SGRIDMB", "1 1");
  ctx.add<double>("RCUTMB", 5.6);
  const SeedView sv(ctx);
  const DataStats data = three_shell_stats();
  for (const char *label : {"DM_Blip", "DM_PS_SQ", "DM_OW_SQ"}) {
    CAPTURE(label);
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    const ParamMeta *centre = param_of(*meta, "CGRIDMB");
    const ParamMeta *width = param_of(*meta, "SGRIDMB");
    REQUIRE(centre != nullptr);
    REQUIRE(width != nullptr);

    const BoundSuggestion c0 = centre->suggest(data, sv, 0);
    INFO("CGRIDMB(1): " << c0.first << " .. " << c0.second << ": "
                        << c0.rationale);
    CHECK(c0.none);
    CHECK(c0.rationale.find("origin") != std::string::npos);

    const BoundSuggestion w0 = width->suggest(data, sv, 0);
    INFO("SGRIDMB(1): " << w0.first << " .. " << w0.second << ": "
                        << w0.rationale);
    CHECK_FALSE(w0.none);
    CHECK(w0.first == Approx(x_f / 5.6).epsilon(1e-12));
    CHECK(w0.second == Approx(x_f / 2.25).epsilon(1e-12));
    CHECK(w0.rationale.find("origin") != std::string::npos);
    CHECK(w0.rationale.find("reach") != std::string::npos);

    const BoundSuggestion c1 = centre->suggest(data, sv, 1);
    INFO("CGRIDMB(2): " << c1.first << " .. " << c1.second << ": "
                        << c1.rationale);
    CHECK_FALSE(c1.none);
    CHECK(c1.first == Approx(4.7 - 0.36).epsilon(1e-12));
    CHECK(c1.second == Approx(4.7 + 0.36).epsilon(1e-12));
    CHECK(c1.rationale.find("seed's centre") != std::string::npos);
    CHECK(c1.rationale.find("nearest") != std::string::npos);
    CHECK(c1.rationale.find("4.67") != std::string::npos);

    const BoundSuggestion w1 = width->suggest(data, sv, 1);
    INFO("SGRIDMB(2): " << w1.first << " .. " << w1.second << ": "
                        << w1.rationale);
    CHECK(w1.first == Approx(1.0 / 0.36).epsilon(1e-12));
    CHECK(w1.second == Approx(6.0 / 0.36).epsilon(1e-12));
    CHECK(w1.rationale.find("nearest") != std::string::npos);
    CHECK(w1.rationale.find("4.67") != std::string::npos);
  }
}

// ----------------------------------------------------------------
// The cutoff of every descriptor: its check fails when no pair of atoms of
// the data lies within it (the descriptor sees no neighbour), and its
// suggested range comes from the neighbour distances, which a suggestion
// takes up to a radius past the seed's cutoff.
// ----------------------------------------------------------------

namespace {

/* make_ta_like_stats() without a pair of atoms (no distance within the
 * cutoff of the statistics), its 1000 atoms kept. */
DataStats no_pair_stats() {
  DataStats d = make_ta_like_stats();
  d.per_pair.clear();
  return d;
}

}  // namespace

/* Pins: for every registered descriptor whose metadata has a cutoff
 * (RCUT2B or RCUTMB):
 *  - the cutoff has a check, which fails for a cutoff of 1 A, below the
 *    smallest neighbour distance (r_min 1.61 A of make_ta_like_stats()),
 *    and for data with atoms but no pair within the cutoff, each message
 *    naming the key, and passes a cutoff of 5.6 A;
 *  - its suggestion (where it has a suggester) on make_ta_like_stats() is
 *    a range of positive width whose rationale names the 99th percentile
 *    of the neighbour distances, and on data without a pair no range
 *    (none), its rationale saying that no pair of atoms is within reach;
 *  - a density's CGRIDMB check passes a seed whose RCUTMB (1 A) holds no
 *    neighbour distance: the RCUTMB check reports it, once.
 * Before the fix: D2_Knot5 (RCUT2B), DM_REAM, DM_Blip and DM_mBlipEnv
 * (RCUTMB) had no cutoff check, DM_EAD's passed data without a pair, the
 * cutoff checks named r_min of the first element pair only, every cutoff
 * suggestion was a pair without a rationale ((0, 0) on data without a
 * pair: a range of zero width), and the CGRIDMB check of a density failed
 * as well ("RCUTMB = 1 is below the smallest neighbour distance"), a
 * second finding of the same cutoff. */
TEST_CASE("Every cutoff fails when it holds no neighbour distance, and its "
          "suggestion names the distances it comes from",
          "[metadata][density_reach][cutoff]") {
  const DataStats data = make_ta_like_stats();
  const DataStats none = no_pair_stats();
  std::size_t cutoffs = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      if (pm.key != "RCUT2B" && pm.key != "RCUTMB") continue;
      ++cutoffs;
      CAPTURE(label, pm.key);
      CHECK(static_cast<bool>(pm.check));
      if (pm.check) {
        Context below;
        below.add<double>(pm.key, 1.0);
        const HealthVerdict v_below = pm.check(data, SeedView(below));
        INFO("1 A: " << verdict_message(v_below));
        CHECK(is_fail(v_below));
        CHECK(verdict_message(v_below).find(pm.key) != std::string::npos);
        Context wide;
        wide.add<double>(pm.key, 5.6);
        const HealthVerdict v_none = pm.check(none, SeedView(wide));
        INFO("no pair: " << verdict_message(v_none));
        CHECK(is_fail(v_none));
        CHECK(verdict_message(v_none).find(pm.key) != std::string::npos);
        CHECK(is_ok(pm.check(data, SeedView(wide))));
      }
      if (pm.suggest) {
        Context seed;
        seed.add<double>(pm.key, 5.6);
        const BoundSuggestion s = pm.suggest(data, SeedView(seed), -1);
        INFO("suggestion: " << s.first << " .. " << s.second << ": "
                            << s.rationale);
        CHECK_FALSE(s.none);
        CHECK(s.first < s.second);
        CHECK(s.rationale.find("99th percentile") != std::string::npos);
        const BoundSuggestion s_none = pm.suggest(none, SeedView(seed), -1);
        INFO("no pair: " << s_none.first << " .. " << s_none.second << ": "
                         << s_none.rationale);
        CHECK(s_none.none);
        CHECK(s_none.rationale.find("no pair of atoms") != std::string::npos);
      }
    }
    const ParamMeta *centre = param_of(*meta, "CGRIDMB");
    if (centre && centre->check && param_of(*meta, "RCUTMB")) {
      CAPTURE(label);
      Context seed;
      seed.add<double>("RCUTMB", 1.0);
      seed.add<double>("CGRIDMB", 0.0);
      seed.add<double>("SGRIDMB", 0.2);
      const HealthVerdict v = centre->check(data, SeedView(seed));
      INFO("CGRIDMB check: " << verdict_message(v));
      CHECK(is_ok(v));
    }
  }
  CHECK(cutoffs > 10u);
}

namespace {

/* make_ta_like_stats() on Ta-W data: the (Ta, Ta) pairs first (DataStats
 * sorts per_pair by element pair), from r_min 2.86 A, then the (Ta, W)
 * pairs, from 2.4 A. */
DataStats ta_w_stats() {
  DataStats d = make_ta_like_stats();
  d.elements = {"Ta", "W"};
  PerPairStats tata = d.per_pair[0];
  tata.r_min = 2.86;
  PerPairStats taw = d.per_pair[0];
  taw.elements = {"Ta", "W"};
  taw.r_min = 2.4;
  d.per_pair = {tata, taw};
  return d;
}

}  // namespace

/* Pins: the check of every cutoff (RCUT2B, RCUTMB) of every registered
 * descriptor compares the cutoff with the smallest neighbour distance of
 * every element pair of the data: on Ta-W data whose (Ta, Ta) pairs, the
 * first element pair, start at 2.86 A and whose (Ta, W) pairs start at
 * 2.4 A (ta_w_stats()), a cutoff of 2.6 A passes, as it holds the (Ta, W)
 * pairs from 2.4 A, and a cutoff of 2.3 A fails, its message naming 2.4 A,
 * the smallest neighbour distance of the dataset. The check judges the
 * statistics it is given, every element pair of them, and a view without
 * the element pairs of the descriptor's line names no pair: its callers
 * give it the statistics of those pairs (LinePairs::stats(), the next
 * cases), so no FAIL finding, which AUDIT error turns into a refusal, is
 * given for a cutoff that holds a pair the descriptor sums over.
 * Before the fix: each check compared the cutoff with r_min of the first
 * element pair, 2.86 A: the cutoff of 2.6 A failed ("RCUT2B=2.600000 is
 * below the dataset r_min=2.860000 ...") and the message of 2.3 A named
 * 2.86, not 2.4; D2_Knot5, DM_REAM, DM_Blip and DM_mBlipEnv had no cutoff
 * check. */
TEST_CASE("Every cutoff is compared with the smallest neighbour distance of "
          "every element pair",
          "[metadata][density_reach][cutoff]") {
  const DataStats data = ta_w_stats();
  std::size_t cutoffs = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      if (pm.key != "RCUT2B" && pm.key != "RCUTMB") continue;
      ++cutoffs;
      CAPTURE(label, pm.key);
      CHECK(static_cast<bool>(pm.check));
      if (!pm.check) continue;
      Context between;
      between.add<double>(pm.key, 2.6);
      const HealthVerdict v_between = pm.check(data, SeedView(between));
      INFO("2.6 A: " << verdict_message(v_between));
      CHECK(is_ok(v_between));
      Context below;
      below.add<double>(pm.key, 2.3);
      const HealthVerdict v_below = pm.check(data, SeedView(below));
      INFO("2.3 A: " << verdict_message(v_below));
      CHECK(is_fail(v_below));
      CHECK(verdict_message(v_below).find("(2.4 Å)") != std::string::npos);
    }
  }
  CHECK(cutoffs > 10u);
}

/* Pins: a suggestion taken from the shells, peaks or percentiles of the
 * neighbour distances names the element pair whose distances they are, the
 * first element pair of the statistics (DataStats sorts per_pair by element
 * pair): on Ta-W data (ta_w_stats(), the (Ta, Ta) pairs first)
 *  - the cutoff suggestion of every descriptor that has one names "the
 *    99th percentile of the (Ta, Ta) neighbour distances";
 *  - for DM_EAD and DM_Blip, a centre at 2.9 A gets a CGRIDMB and an
 *    SGRIDMB suggestion naming "the (Ta, Ta) neighbour shell nearest to
 *    it", and a seed without CGRIDMB a CGRIDMB suggestion naming "the first
 *    (Ta, Ta) neighbour shell" and an SGRIDMB suggestion naming "(Ta, Ta)";
 *  - DM_EAD's Gaussian at the origin gets an SGRIDMB suggestion naming the
 *    (Ta, Ta) neighbour distances its width is taken from.
 * A pair is named as the checks and the rows without an OPTIM line name it
 * ("no (Ta, Ta) pair of atoms").
 * Before the fix: each of these rationales named "the neighbour distances"
 * or "the neighbour shell" alone, while the check of a cutoff reads every
 * element pair.
 * Before the fix of the notation: the rationales named the pair "Ta-Ta"
 * ("the 99th percentile of the (Ta, Ta) neighbour distances"). */
TEST_CASE("A suggestion names the element pair whose distances it comes "
          "from",
          "[metadata][density_reach][suggest]") {
  const DataStats data = ta_w_stats();
  std::size_t cutoffs = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      if (pm.key != "RCUT2B" && pm.key != "RCUTMB") continue;
      if (!pm.suggest) continue;
      ++cutoffs;
      CAPTURE(label, pm.key);
      Context seed;
      seed.add<double>(pm.key, 5.6);
      const BoundSuggestion s = pm.suggest(data, SeedView(seed), -1);
      INFO(s.rationale);
      CHECK(s.rationale.find(
                "the 99th percentile of the (Ta, Ta) neighbour distances") !=
            std::string::npos);
    }
  }
  CHECK(cutoffs > 10u);

  for (const char *label : {"DM_EAD", "DM_Blip"}) {
    CAPTURE(label);
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    const ParamMeta *centre = param_of(*meta, "CGRIDMB");
    const ParamMeta *width = param_of(*meta, "SGRIDMB");
    REQUIRE(centre != nullptr);
    REQUIRE(width != nullptr);
    Context seeded;
    seeded.add("CGRIDMB", "0 2.9");
    seeded.add("SGRIDMB", "1 1");
    seeded.add<double>("RCUTMB", 5.6);
    const SeedView sv(seeded);
    const BoundSuggestion c1 = centre->suggest(data, sv, 1);
    INFO("CGRIDMB(2): " << c1.rationale);
    CHECK(c1.rationale.find("the (Ta, Ta) neighbour shell nearest to it") !=
          std::string::npos);
    const BoundSuggestion w1 = width->suggest(data, sv, 1);
    INFO("SGRIDMB(2): " << w1.rationale);
    CHECK(w1.rationale.find("the (Ta, Ta) neighbour shell nearest to it") !=
          std::string::npos);
    Context bare;
    bare.add<double>("RCUTMB", 5.6);
    const BoundSuggestion c0 = centre->suggest(data, SeedView(bare), 0);
    INFO("CGRIDMB without a seed: " << c0.rationale);
    CHECK(c0.rationale.find("the first (Ta, Ta) neighbour shell") !=
          std::string::npos);
    const BoundSuggestion w0 = width->suggest(data, SeedView(bare), 0);
    INFO("SGRIDMB without a seed: " << w0.rationale);
    CHECK(w0.rationale.find("(Ta, Ta)") != std::string::npos);
  }

  std::unique_ptr<D_Base> ead(tadah::core::factory<D_Base>("DM_EAD"));
  const auto meta = ead->meta();
  REQUIRE(meta.has_value());
  const ParamMeta *width = param_of(*meta, "SGRIDMB");
  REQUIRE(width != nullptr);
  Context origin;
  origin.add<double>("CGRIDMB", 0.0);
  origin.add<double>("SGRIDMB", 0.2);
  origin.add<double>("RCUTMB", 5.6);
  const BoundSuggestion w = width->suggest(data, SeedView(origin), 0);
  INFO("SGRIDMB at the origin: " << w.rationale);
  CHECK_FALSE(w.none);
  CHECK(w.rationale.find("(Ta, Ta) neighbour distances") != std::string::npos);
}

/* Pins: every Critical parameter of every registered descriptor has a
 * suggester, so that tadah analysis suggest_bounds, which the
 * critical-parameter warning of tadah hpo names (Tadah.MLIP), gives it a
 * row: the cutoff of D2_Knot5 (RCUT2B) included, which has the suggestion
 * of every cutoff (cutoff_suggestion) and is a fixed value of a search on a
 * D2_mJoin line, where tadah hpo takes the stated cutoff.
 * Before the fix: D2_Knot5's RCUT2B, Critical, had no suggester, and tadah
 * analysis suggest_bounds gave it no row, on a D2_mJoin line too, where
 * tadah hpo warns that it is fixed. */
TEST_CASE("Every critical parameter has a suggestion",
          "[metadata][suggest][cutoff]") {
  std::size_t critical = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      if (pm.crit != Criticality::Critical) continue;
      ++critical;
      CAPTURE(label, pm.key);
      CHECK(static_cast<bool>(pm.suggest));
    }
  }
  CHECK(critical > 20u);
}

/* Pins: the grids of DM_REAM (CGRIDMB, SGRIDMB, AMPGRIDMB, CEMBFUNC,
 * SEMBFUNC) get a row without an OPTIM line on data with distances, its
 * rationale saying that the neighbour distances give no range for a value
 * of a DM_REAM grid, and naming no other command.
 * Before the fix: the rationale said that "tadah refit writes an OPTIM line
 * of +-OPTIM_PCT percent about each value of a DM_REAM potential", which
 * tadah refit of an EAM file does not do (it writes a DM_Embed component);
 * only tadah refit --edit of a DM_REAM potential writes such lines, and it
 * writes them commented out. */
TEST_CASE("The grids of DM_REAM get no range, the neighbour distances "
          "giving none",
          "[metadata][suggest][dm_ream]") {
  std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>("DM_REAM"));
  const auto meta = d->meta();
  REQUIRE(meta.has_value());
  const DataStats data = make_ta_like_stats();
  for (const char *key :
       {"CGRIDMB", "SGRIDMB", "AMPGRIDMB", "CEMBFUNC", "SEMBFUNC"}) {
    CAPTURE(key);
    const ParamMeta *p = param_of(*meta, key);
    REQUIRE(p != nullptr);
    REQUIRE(static_cast<bool>(p->suggest));
    const BoundSuggestion s = p->suggest(data, SeedView(Context{}), 0);
    INFO(s.rationale);
    CHECK(s.none);
    CHECK(s.rationale.find("the neighbour distances give no range for a "
                           "value of a DM_REAM grid") != std::string::npos);
    CHECK(s.rationale.find("refit") == std::string::npos);
  }
}

#include <regex>

/* Pins: the role text of every parameter of every registered descriptor,
 * which tadah analysis suggest_bounds prints as the rationale of a range
 * that has none, numbers an entry as an OPTIM line does ("CGRID2B (i)"),
 * never with a bracket index ("CGRID2B[i]").
 * Before the fix: the centres of D2_Blip, D2_Blip5 and D2_BP read "Centre
 * of the i-th radial blip (CGRID2B[i])." above an OPTIM CGRID2B (1-7)
 * line. */
TEST_CASE("A role text numbers an entry as an OPTIM line does",
          "[metadata][suggest]") {
  const std::regex bracket(R"([A-Z][A-Z0-9_]*\[[^\]]*\])");
  std::size_t roles = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      ++roles;
      CAPTURE(label, pm.key, pm.role);
      CHECK_FALSE(std::regex_search(pm.role, bracket));
    }
  }
  CHECK(roles > 50u);
  for (const char *label : {"D2_Blip", "D2_Blip5", "D2_BP"}) {
    CAPTURE(label);
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    const ParamMeta *centre = param_of(*meta, "CGRID2B");
    REQUIRE(centre != nullptr);
    CAPTURE(centre->role);
    CHECK(centre->role.find("CGRID2B (i)") != std::string::npos);
  }
}

/* Pins: the role texts of the radial grids, which tadah analysis
 * suggest_bounds prints beside a range and in the comment of a written
 * OPTIM block, state the basis and its support as the descriptor computes
 * it, number the entry as an OPTIM line does, and give no instruction:
 *  - the inverse width of D2_Blip and of DM_Blip, b(eta (r - c)), has the
 *    support |r - c| < 2/eta, and that of D2_Blip5, the quintic blip
 *    b5(eta (r - c)), |r - c| < 3/eta (its half-width, 3, in
 *    basis_kind.h);
 *  - the width of D2_BP is eta of the Gaussian exp(-eta (r - c)^2);
 *  - the knot of D2_Knot5 says that RCUT2B is the largest knot on a TYPE2B
 *    line of D2_Knot5 alone (on a D2_mJoin line RCUT2B keeps its value);
 *  - every grid entry (CGRID2B, SGRID2B, CGRIDMB, SGRIDMB, CEMBFUNC,
 *    SEMBFUNC) of these descriptors and of DM_mBlipEnv is "KEY (i)".
 * Before the fix: D2_Blip5's width read "whose support is |r - c| <
 * 2/eta", D2_BP's "Inverse Gaussian width of the i-th radial.", D2_Knot5's
 * knot ended "keep r_s <= RCUT2B.", and no width, no DM_Blip centre and no
 * embedding blip of DM_mBlipEnv named its entry; D2_Knot5's knot then said
 * "the cutoff RCUT2B is the largest knot." without its condition. */
TEST_CASE("The role texts of the radial grids state their basis and "
          "support",
          "[metadata][suggest]") {
  auto role = [](const char *label, const char *key) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    const ParamMeta *p = param_of(*meta, key);
    REQUIRE(p != nullptr);
    return p->role;
  };
  const std::string blip = role("D2_Blip", "SGRID2B");
  const std::string blip5 = role("D2_Blip5", "SGRID2B");
  const std::string bp = role("D2_BP", "SGRID2B");
  const std::string knot = role("D2_Knot5", "CGRID2B");
  const std::string mblip = role("DM_Blip", "SGRIDMB");
  CAPTURE(blip, blip5, bp, knot, mblip);
  CHECK(blip.find("b(eta (r - c))") != std::string::npos);
  CHECK(blip.find("|r - c| < 2/eta") != std::string::npos);
  CHECK(mblip.find("|r - c| < 2/eta") != std::string::npos);
  CHECK(blip5.find("b5(eta (r - c))") != std::string::npos);
  CHECK(blip5.find("|r - c| < 3/eta") != std::string::npos);
  CHECK(blip5.find("2/eta") == std::string::npos);
  CHECK(bp.find("exp(-eta (r - c)^2)") != std::string::npos);
  CHECK(knot ==
        "Knot position r_s of the i-th truncated power (r_s - r)_+^p, "
        "CGRID2B (i), with p = 5 unless KDEG2B (i) gives 3; on a TYPE2B "
        "line of D2_Knot5 alone the cutoff RCUT2B is the largest knot.");
  const std::vector<std::pair<const char *, const char *>> grids = {
      {"D2_Blip", "CGRID2B"},  {"D2_Blip", "SGRID2B"},
      {"D2_Blip5", "CGRID2B"}, {"D2_Blip5", "SGRID2B"},
      {"D2_BP", "CGRID2B"},    {"D2_BP", "SGRID2B"},
      {"D2_Knot5", "CGRID2B"}, {"DM_Blip", "CGRIDMB"},
      {"DM_Blip", "SGRIDMB"},  {"DM_mBlipEnv", "CEMBFUNC"},
      {"DM_mBlipEnv", "SEMBFUNC"}};
  for (const auto &[label, key] : grids) {
    const std::string text = role(label, key);
    CAPTURE(label, key, text);
    CHECK(text.find(std::string(key) + " (i)") != std::string::npos);
    CHECK(text.find("keep ") == std::string::npos);
  }
}

/* Pins: the health checks print a value of the seed as its shortest
 * round-trip text: the cutoff check of D2_Blip with RCUT2B 5.61234567 on
 * data without a pair says "RCUT2B = 5.61234567: no pair of atoms ...", the
 * CGRIDMB check of DM_EAD with CGRIDMB 3.14159265 and no SGRIDMB says
 * "CGRIDMB(1) = 3.14159265 has no width SGRIDMB(1): ...", and the check of
 * D2_MIE 6 12 says "D2_MIE needs n > m for a finite prefactor (got n = 6,
 * m = 12)."
 * Before the fix: six significant digits, "RCUT2B = 5.61235" and
 * "CGRIDMB(1) = 3.14159", and std::to_string, "D2_MIE requires n > m for
 * finite prefactor (got n=6.000000, m=12.000000)." */
TEST_CASE("The health checks print the seed's values as written",
          "[metadata][density_reach][cutoff]") {
  auto meta_of = [](const char *label) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    return *meta;
  };
  const DescriptorMeta blip = meta_of("D2_Blip");
  const ParamMeta *rcut = param_of(blip, "RCUT2B");
  REQUIRE(rcut != nullptr);
  REQUIRE(static_cast<bool>(rcut->check));
  Context cut;
  cut.add("RCUT2B", "5.61234567");
  const std::string no_pair =
      verdict_message(rcut->check(no_pair_stats(), SeedView(cut)));
  CHECK(no_pair.rfind("RCUT2B = 5.61234567: no pair of atoms", 0) == 0);

  const DescriptorMeta ead = meta_of("DM_EAD");
  const ParamMeta *centre = param_of(ead, "CGRIDMB");
  REQUIRE(centre != nullptr);
  REQUIRE(static_cast<bool>(centre->check));
  Context grid;
  grid.add("CGRIDMB", "3.14159265");
  const std::string no_width =
      verdict_message(centre->check(make_ta_like_stats(), SeedView(grid)));
  CHECK(no_width.rfind("CGRIDMB(1) = 3.14159265 has no width SGRIDMB(1)", 0) ==
        0);

  const DescriptorMeta mie = meta_of("D2_MIE");
  Context lines;
  lines.add("TYPE2B", "D2_MIE");
  lines.add("TYPE2B", "6");
  lines.add("TYPE2B", "12");
  lines.add("TYPE2B", "Ta");
  lines.add("TYPE2B", "Ta");
  std::vector<std::string> fails;
  for (const auto &check : mie.cross_param_checks) {
    const HealthVerdict v = check(make_ta_like_stats(), SeedView(lines));
    if (is_fail(v)) fails.push_back(verdict_message(v));
  }
  REQUIRE(fails.size() == 1u);
  CHECK(fails[0] ==
        "D2_MIE needs n > m for a finite prefactor (got n = 6, m = 12).");
}

// ----------------------------------------------------------------
// The element pairs of a descriptor's line: a descriptor sums over the
// neighbours of the element pairs its TYPE2B or TYPEMB line names, and its
// checks and suggestions are given the statistics of those pairs.
// ----------------------------------------------------------------

#include <tadah/models/descriptors/density_reach.h>
#include <tadah/models/descriptors/line_pairs.h>

#include <stdexcept>

namespace {

/* ta_w_stats() with (W, W) pairs too, from r_min 2.5 A to r_max 2.7 A; the
 * (Ta, Ta) pairs run from 2.86 A to 6.5 A and the (Ta, W) pairs from 2.4
 * A. */
DataStats ta_w_ww_stats() {
  DataStats d = ta_w_stats();
  PerPairStats ww = d.per_pair[0];
  ww.elements = {"W", "W"};
  ww.r_min = 2.5;
  ww.r_max = 2.7;
  d.per_pair.push_back(ww);
  return d;
}

/* The element pairs of each per_pair entry of @p d, "Ta-W", in order. */
std::vector<std::string> pair_names(const DataStats &d) {
  std::vector<std::string> out;
  for (const auto &pp : d.per_pair)
    out.push_back(pp.elements.first + "-" + pp.elements.second);
  return out;
}

}  // namespace

/* Pins: the element pairs of a descriptor's line (LinePairs, from the
 * element tokens of the line, D_Base::get_init_atoms()):
 *  - "Ta Ta Ta W" names (Ta, Ta) and (Ta, W), each in either order, and not
 *    (W, W); its text is "(Ta, Ta) or (Ta, W)"; stats() of the Ta-W-W data
 *    (ta_w_ww_stats()) keeps the Ta-Ta and Ta-W entries alone, in their
 *    order, with the other fields (n_atoms 1000, elements Ta W);
 *  - "W Ta" names (Ta, W), its text "(W, Ta)", as the line writes it;
 *  - "Ta Ta Ta Ta" is "(Ta, Ta)", once;
 *  - "Ta *" names every pair with Ta, (Ta, Ta) and (Ta, W) of the data, its
 *    text "(Ta, *)"; "* *" names every pair (every()), its text empty (a
 *    message then names no pair), and stats() keeps every entry;
 *  - no token names no pair, its text empty, and stats() keeps none;
 *  - an odd number of tokens, "Ta Ta W", is refused (std::invalid_argument)
 *    naming them, as a descriptor's line is refused ("the element types
 *    of a descriptor's line come in pairs, and the line has 3 of them, Ta
 *    Ta W").
 * Before the fix: does not compile (no LinePairs: every check and
 * suggestion read the statistics of every element pair of the data, or of
 * the first). */
TEST_CASE("The element pairs of a descriptor's line select its neighbour "
          "statistics",
          "[metadata][density_reach][line_pairs]") {
  const DataStats data = ta_w_ww_stats();
  const LinePairs two({"Ta", "Ta", "Ta", "W"});
  CHECK(two.names("Ta", "Ta"));
  CHECK(two.names("Ta", "W"));
  CHECK(two.names("W", "Ta"));
  CHECK_FALSE(two.names("W", "W"));
  CHECK_FALSE(two.every());
  CHECK(two.text() == "(Ta, Ta) or (Ta, W)");
  const DataStats kept = two.stats(data);
  CHECK(pair_names(kept) == std::vector<std::string>{"Ta-Ta", "Ta-W"});
  CHECK(kept.n_atoms == 1000u);
  CHECK(kept.elements == std::vector<std::string>{"Ta", "W"});

  const LinePairs reversed({"W", "Ta"});
  CHECK(reversed.names("Ta", "W"));
  CHECK(reversed.text() == "(W, Ta)");
  CHECK(pair_names(reversed.stats(data)) == std::vector<std::string>{"Ta-W"});

  CHECK(LinePairs({"Ta", "Ta", "Ta", "Ta"}).text() == "(Ta, Ta)");

  const LinePairs with_ta({"Ta", "*"});
  CHECK(with_ta.names("Ta", "W"));
  CHECK(with_ta.names("W", "Ta"));
  CHECK_FALSE(with_ta.names("W", "W"));
  CHECK(with_ta.text() == "(Ta, *)");
  CHECK(pair_names(with_ta.stats(data)) ==
        std::vector<std::string>{"Ta-Ta", "Ta-W"});

  const LinePairs every({"*", "*"});
  CHECK(every.every());
  CHECK(every.names("W", "W"));
  CHECK(every.text().empty());
  CHECK(pair_names(every.stats(data)) ==
        std::vector<std::string>{"Ta-Ta", "Ta-W", "W-W"});

  const LinePairs none(std::vector<std::string>{});
  CHECK_FALSE(none.names("Ta", "Ta"));
  CHECK(none.text().empty());
  CHECK(none.stats(data).per_pair.empty());

  try {
    LinePairs odd({"Ta", "Ta", "W"});
    FAIL("an odd number of element tokens was accepted");
  } catch (const std::invalid_argument &e) {
    CHECK(std::string(e.what()) ==
          "the element types of a descriptor's line come in pairs, and the "
          "line has 3 of them, Ta Ta W");
  }
}

/* Pins: given the statistics of the element pairs of its line
 * (LinePairs::stats()) and a view of the seed with those pairs, a check and
 * a suggestion name them:
 *  - the cutoff check of D2_Blip, RCUT2B 2.6 on the (Ta, Ta) pairs of the
 *    Ta-W-W data (from 2.86 A): "RCUT2B = 2.6 is below the smallest (Ta,
 *    Ta) neighbour distance of the dataset (2.86 Å): ..."; on (Ta, Ta) or
 *    (W, W), RCUT2B 2.45: "... smallest (Ta, Ta) or (W, W) neighbour
 *    distance of the dataset (2.5 Å) ...";
 *  - on (W, W) of the Ta-W data, which has none: "RCUT2B = 2.6: no (W, W)
 *    pair of atoms of the dataset lies within it, ...";
 *  - the CGRIDMB check of DM_EAD (CGRIDMB 3.1, SGRIDMB 100, RCUTMB 3.5) on
 *    the (W, W) pairs (2.5 to 2.7 A): "... below 1e-6 at every (W, W)
 *    neighbour distance within RCUTMB = 3.5 (2.5 to 2.7 Å) ...";
 *  - the cutoff suggestion of D2_Blip on (W, W) of the Ta-W data: no OPTIM
 *    line, "no (W, W) pair of atoms of the data lies within the reach of
 *    the neighbour statistics ...";
 *  - a view without the pairs keeps the words of the data as a whole: "no
 *    pair of atoms of the dataset lies within it".
 * Before the fix: does not compile (no LinePairs, and SeedView takes no
 * element pairs). */
TEST_CASE("The checks and suggestions name the element pairs of the "
          "descriptor's line",
          "[metadata][density_reach][line_pairs]") {
  auto meta_of = [](const char *label) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    return *meta;
  };
  const DescriptorMeta blip = meta_of("D2_Blip");
  const ParamMeta *rcut = param_of(blip, "RCUT2B");
  REQUIRE(rcut != nullptr);
  REQUIRE(static_cast<bool>(rcut->check));
  REQUIRE(static_cast<bool>(rcut->suggest));
  const DataStats data = ta_w_ww_stats();

  Context cut;
  cut.add("RCUT2B", "2.6");
  const LinePairs tata({"Ta", "Ta"});
  const std::string below = verdict_message(
      rcut->check(tata.stats(data), SeedView(cut, {}, tata)));
  CHECK(below ==
        "RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance of "
        "the dataset (2.86 Å): the descriptor sums over no neighbour and is "
        "0 for every atom.");

  Context near;
  near.add("RCUT2B", "2.45");
  const LinePairs both({"Ta", "Ta", "W", "W"});
  const std::string two = verdict_message(
      rcut->check(both.stats(data), SeedView(near, {}, both)));
  CHECK(two ==
        "RCUT2B = 2.45 is below the smallest (Ta, Ta) or (W, W) neighbour "
        "distance of the dataset (2.5 Å): the descriptor sums over no "
        "neighbour and is 0 for every atom.");

  const LinePairs ww({"W", "W"});
  const std::string none = verdict_message(
      rcut->check(ww.stats(ta_w_stats()), SeedView(cut, {}, ww)));
  CHECK(none ==
        "RCUT2B = 2.6: no (W, W) pair of atoms of the dataset lies within "
        "it, so the descriptor sums over no neighbour and is 0 for every "
        "atom.");

  const DescriptorMeta ead = meta_of("DM_EAD");
  const ParamMeta *centre = param_of(ead, "CGRIDMB");
  REQUIRE(centre != nullptr);
  REQUIRE(static_cast<bool>(centre->check));
  Context grid;
  grid.add("CGRIDMB", "3.1");
  grid.add("SGRIDMB", "100");
  grid.add("RCUTMB", "3.5");
  const std::string reach = verdict_message(
      centre->check(ww.stats(data), SeedView(grid, {}, ww)));
  CHECK(reach.find("is below 1e-6 at every (W, W) neighbour distance "
                   "within RCUTMB = 3.5 (2.5 to 2.7 Å)") != std::string::npos);

  Context seed;
  seed.add("RCUT2B", "5.6");
  const BoundSuggestion s =
      rcut->suggest(ww.stats(ta_w_stats()), SeedView(seed, {}, ww), -1);
  CHECK(s.none);
  CHECK(s.rationale.rfind("no (W, W) pair of atoms of the data lies within "
                          "the reach of the neighbour statistics", 0) == 0);

  const std::string whole = verdict_message(
      rcut->check(no_pair_stats(), SeedView(cut)));
  CHECK(whole.rfind("RCUT2B = 2.6: no pair of atoms of the dataset lies "
                    "within it", 0) == 0);
}

/* Pins: the checks of a descriptor whose line names several element pairs
 * judge each pair of the line on its own, by the statistics of the element
 * pairs of the data it names, on the Ta-W-W data (ta_w_ww_stats(): (Ta,
 * Ta) from 2.86 to 6.5 A, (Ta, W) from 2.4 A, (W, W) from 2.5 to 2.7 A):
 *  - the cutoff check of D2_Blip, RCUT2B 2.6, on the line Ta Ta W W: a
 *    FAIL, "RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance
 *    of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the
 *    descriptor, which sums over its other element pairs alone.";
 *  - on Ta Ta Mo Mo W W at 2.6, the data having no (Mo, Mo) pair: "RCUT2B
 *    = 2.6 is below the smallest (Ta, Ta) neighbour distance of the dataset
 *    (2.86 Å) and holds no (Mo, Mo) pair of atoms of the dataset: the (Ta,
 *    Ta) and (Mo, Mo) pairs add nothing to the descriptor, which sums over
 *    its other element pairs alone.";
 *  - on Ta Ta W W at 2.45, below every pair of the line: the FAIL of the
 *    line as a whole, "... below the smallest (Ta, Ta) or (W, W) neighbour
 *    distance of the dataset (2.5 Å): the descriptor sums over no neighbour
 *    and is 0 for every atom." (a pin);
 *  - on Ta * at 2.6: the FAIL of Ta Ta W W at 2.6, "RCUT2B = 2.6 is below
 *    the smallest (Ta, Ta) neighbour distance of the dataset (2.86 Å): the
 *    (Ta, Ta) pair adds nothing to the descriptor, ...", the pair (Ta, *)
 *    standing for the (Ta, Ta) and (Ta, W) pairs of the data, each judged
 *    on its own;
 *  - the CGRIDMB check of DM_EAD, CGRIDMB 3.1, SGRIDMB 100, RCUTMB 3.5, on
 *    Ta Ta W W, the channel reaching the (Ta, Ta) distances and not the (W,
 *    W) ones: "CGRIDMB(1) = 3.1 with SGRIDMB(1) = 100: the Gaussian
 *    exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every (W, W) neighbour
 *    distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from
 *    the (W, W) pairs of atoms (a smaller SGRIDMB, or a centre nearer the
 *    neighbours, reaches them).";
 *  - CGRIDMB 2.78 with SGRIDMB 3000 on Ta Ta W W, 0.08 A from the (Ta, Ta)
 *    distances and from the (W, W) ones and reaching neither: "... is below
 *    1e-6 at every (Ta, Ta) neighbour distance within RCUTMB = 3.5 (2.86 to
 *    3.5 Å) and at every (W, W) one (2.5 to 2.7 Å), so its channel of ρ is
 *    ~0 for every atom (...)";
 *  - the CGRIDMB check of DM_Blip, CGRIDMB 3.1, SGRIDMB 10 (a support 0.2
 *    A wide each side), on Ta Ta W W: "... the support of the blip, |r -
 *    CGRIDMB| < 2/SGRIDMB = 0.2 Å, holds no (W, W) neighbour distance
 *    within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from the (W, W)
 *    pairs of atoms (a smaller SGRIDMB widens the support).".
 * Before the fix: the pairs of a line were judged together, by the
 * smallest and the largest of their distances: none of the five FAILs was
 * given (RCUT2B 2.6 holds the (W, W) distances; the channels at 3.1 reach
 * the (Ta, Ta) ones; the range 2.5 to 3.5 A holds the centre 2.78), and
 * AUDIT error trained descriptors that are 0 for every Ta atom.
 * Before a pair with * stood for the element pairs of the data it matches:
 * Ta * at 2.6 gave no finding, the pair (Ta, *) judged as one pair, by the
 * (Ta, Ta) and (Ta, W) distances together (from 2.4 A).
 * Before the message named the channel's part of ρ, a channel that reaches
 * no pair said "..., so ρ is ~0 for every atom (...)", which reads as the
 * whole density, whatever the other channels reach. */
TEST_CASE("Each element pair of a descriptor's line is judged on its own",
          "[metadata][density_reach][line_pairs]") {
  auto meta_of = [](const char *label) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    REQUIRE(meta.has_value());
    return *meta;
  };
  const DataStats data = ta_w_ww_stats();
  const DescriptorMeta blip = meta_of("D2_Blip");
  const ParamMeta *rcut = param_of(blip, "RCUT2B");
  REQUIRE(rcut != nullptr);
  REQUIRE(static_cast<bool>(rcut->check));
  auto cutoff = [&](const std::string &rc,
                    const std::vector<std::string> &tokens) {
    Context c;
    c.add("RCUT2B", rc);
    const LinePairs pairs(tokens);
    const HealthVerdict v = rcut->check(pairs.stats(data),
                                        SeedView(c, {}, pairs));
    return std::string(is_fail(v) ? "FAIL " : is_warn(v) ? "WARN " : "") +
           verdict_message(v);
  };
  CHECK(cutoff("2.6", {"Ta", "Ta", "W", "W"}) ==
        "FAIL RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance "
        "of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the "
        "descriptor, which sums over its other element pairs alone.");
  CHECK(cutoff("2.6", {"Ta", "Ta", "Mo", "Mo", "W", "W"}) ==
        "FAIL RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance "
        "of the dataset (2.86 Å) and holds no (Mo, Mo) pair of atoms of the "
        "dataset: the (Ta, Ta) and (Mo, Mo) pairs add nothing to the "
        "descriptor, which sums over its other element pairs alone.");
  CHECK(cutoff("2.45", {"Ta", "Ta", "W", "W"}) ==
        "FAIL RCUT2B = 2.45 is below the smallest (Ta, Ta) or (W, W) "
        "neighbour distance of the dataset (2.5 Å): the descriptor sums over "
        "no neighbour and is 0 for every atom.");
  CHECK(cutoff("2.6", {"Ta", "*"}) ==
        "FAIL RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance "
        "of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the "
        "descriptor, which sums over its other element pairs alone.");

  auto channel = [&](const char *label, const std::string &c,
                     const std::string &s) {
    const DescriptorMeta m = meta_of(label);
    const ParamMeta *centre = param_of(m, "CGRIDMB");
    REQUIRE(centre != nullptr);
    REQUIRE(static_cast<bool>(centre->check));
    Context grid;
    grid.add("CGRIDMB", c);
    grid.add("SGRIDMB", s);
    grid.add("RCUTMB", "3.5");
    const LinePairs pairs({"Ta", "Ta", "W", "W"});
    const HealthVerdict v = centre->check(pairs.stats(data),
                                          SeedView(grid, {}, pairs));
    return std::string(is_fail(v) ? "FAIL " : is_warn(v) ? "WARN " : "") +
           verdict_message(v);
  };
  CHECK(channel("DM_EAD", "3.1", "100") ==
        "FAIL CGRIDMB(1) = 3.1 with SGRIDMB(1) = 100: the Gaussian "
        "exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every (W, W) "
        "neighbour distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets "
        "nothing from the (W, W) pairs of atoms (a smaller SGRIDMB, or a "
        "centre nearer the neighbours, reaches them).");
  CHECK(channel("DM_EAD", "2.78", "3000") ==
        "FAIL CGRIDMB(1) = 2.78 with SGRIDMB(1) = 3000: the Gaussian "
        "exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every (Ta, Ta) "
        "neighbour distance within RCUTMB = 3.5 (2.86 to 3.5 Å) and at every "
        "(W, W) one (2.5 to 2.7 Å), so its channel of ρ is ~0 for every atom "
        "(a smaller SGRIDMB, or a centre nearer the neighbours, reaches "
        "them).");
  CHECK(channel("DM_Blip", "3.1", "10") ==
        "FAIL CGRIDMB(1) = 3.1 with SGRIDMB(1) = 10: the support of the "
        "blip, |r - CGRIDMB| < 2/SGRIDMB = 0.2 Å, holds no (W, W) neighbour "
        "distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from "
        "the (W, W) pairs of atoms (a smaller SGRIDMB widens the support).");
}

namespace {

/* The verdict of the check of @p key of the descriptor @p label on @p data,
 * for the seed @p seed and the line of element tokens @p tokens:
 * "FAIL <message>", "WARN <message>", or empty. */
std::string line_verdict(const char *label, const char *key,
                         const DataStats &data, const Context &seed,
                         const std::vector<std::string> &tokens) {
  std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
  const auto meta = d->meta();
  REQUIRE(meta.has_value());
  const ParamMeta *p = param_of(*meta, key);
  REQUIRE(p != nullptr);
  REQUIRE(static_cast<bool>(p->check));
  const LinePairs pairs(tokens);
  const HealthVerdict v = p->check(pairs.stats(data), SeedView(seed, {}, pairs));
  return std::string(is_fail(v) ? "FAIL " : is_warn(v) ? "WARN " : "") +
         verdict_message(v);
}

}  // namespace

/* Pins: a pair of a descriptor's line written with "*" stands for each
 * element pair it matches, of the elements of the data and those the line
 * names, each judged on its own, as the descriptor sums over each of them:
 * the verdict is that of the line that names those pairs, on the Ta-W-W
 * data (ta_w_ww_stats(): (Ta, Ta) from 2.86 A, (Ta, W) from 2.4 A, (W, W)
 * from 2.5 to 2.7 A):
 *  - the cutoff check of D2_Blip, RCUT2B 2.6, on Ta * and on Ta Ta Ta W:
 *    "FAIL RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance
 *    of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the
 *    descriptor, which sums over its other element pairs alone.";
 *  - RCUT2B 2.45 on * * and on Ta Ta Ta W W W: "FAIL RCUT2B = 2.45 is below
 *    the smallest (Ta, Ta) neighbour distance of the dataset (2.86 Å) and
 *    the smallest (W, W) one (2.5 Å): the (Ta, Ta) and (W, W) pairs add
 *    nothing to the descriptor, which sums over its other element pairs
 *    alone.";
 *  - RCUT2B 2.45 on * * with statistics taken up to 2.45 A, which hold the
 *    (Ta, W) pairs alone and list the elements Ta and W: "FAIL RCUT2B =
 *    2.45 holds no (Ta, Ta) or (W, W) pair of atoms of the dataset: the
 *    (Ta, Ta) and (W, W) pairs add nothing to the descriptor, which sums
 *    over its other element pairs alone." (the elements of the data give
 *    the pairs * stands for, whether the statistics hold their distances or
 *    not);
 *  - RCUT2B 5 on Ta Ta Mo * and on Ta Ta Mo Mo Mo Ta Mo W, the data having
 *    no Mo atom: "FAIL RCUT2B = 5 holds no (Mo, Mo), (Mo, Ta) or (Mo, W)
 *    pair of atoms of the dataset: the (Mo, Mo), (Mo, Ta) and (Mo, W) pairs
 *    add nothing to the descriptor, which sums over its other element pairs
 *    alone." (an element the line names is one * stands for);
 *  - the CGRIDMB check of DM_EAD, CGRIDMB 3.1, SGRIDMB 100, RCUTMB 3.5, on
 *    * * and on Ta Ta Ta W W W: "FAIL CGRIDMB(1) = 3.1 with SGRIDMB(1) =
 *    100: the Gaussian exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every
 *    (W, W) neighbour distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ
 *    gets nothing from the (W, W) pairs of atoms (a smaller SGRIDMB, or a
 *    centre nearer the neighbours, reaches them).".
 * Before the fix: a pair with * was judged as one pair, by the distances of
 * every pair it matches together, from 2.4 A: Ta * at 2.6, * * at 2.45 and
 * the channel of * * gave no finding, the lines that name the pairs a FAIL;
 * and two pairs below the cutoff were named each with its own clause, "...
 * (2.86 Å) and is below the smallest (W, W) neighbour distance of the
 * dataset (2.5 Å): ...".
 * Before * stood for the elements the line names: Ta Ta Mo * at 5 gave no
 * finding, the pair (Mo, *) standing for no pair, as Mo is no element of
 * the data. */
TEST_CASE("A pair of a descriptor's line with * stands for each element pair "
          "it matches",
          "[metadata][density_reach][line_pairs]") {
  const DataStats data = ta_w_ww_stats();
  auto cutoff = [&](const char *rc, const std::vector<std::string> &tokens,
                    const DataStats &d) {
    Context c;
    c.add("RCUT2B", rc);
    return line_verdict("D2_Blip", "RCUT2B", d, c, tokens);
  };
  const std::string ta_ta =
      "FAIL RCUT2B = 2.6 is below the smallest (Ta, Ta) neighbour distance "
      "of the dataset (2.86 Å): the (Ta, Ta) pair adds nothing to the "
      "descriptor, which sums over its other element pairs alone.";
  CHECK(cutoff("2.6", {"Ta", "*"}, data) == ta_ta);
  CHECK(cutoff("2.6", {"Ta", "Ta", "Ta", "W"}, data) == ta_ta);
  const std::string both =
      "FAIL RCUT2B = 2.45 is below the smallest (Ta, Ta) neighbour distance "
      "of the dataset (2.86 Å) and the smallest (W, W) one (2.5 Å): the (Ta, "
      "Ta) and (W, W) pairs add nothing to the descriptor, which sums over "
      "its other element pairs alone.";
  CHECK(cutoff("2.45", {"*", "*"}, data) == both);
  CHECK(cutoff("2.45", {"Ta", "Ta", "Ta", "W", "W", "W"}, data) == both);

  DataStats within = data;
  within.per_pair.erase(
      std::remove_if(within.per_pair.begin(), within.per_pair.end(),
                     [](const PerPairStats &pp) {
                       return pp.elements.first == pp.elements.second;
                     }),
      within.per_pair.end());
  REQUIRE(within.per_pair.size() == 1u);
  REQUIRE(within.elements == std::vector<std::string>{"Ta", "W"});
  CHECK(cutoff("2.45", {"*", "*"}, within) ==
        "FAIL RCUT2B = 2.45 holds no (Ta, Ta) or (W, W) pair of atoms of the "
        "dataset: the (Ta, Ta) and (W, W) pairs add nothing to the "
        "descriptor, which sums over its other element pairs alone.");
  const std::string mo =
      "FAIL RCUT2B = 5 holds no (Mo, Mo), (Mo, Ta) or (Mo, W) pair of atoms "
      "of the dataset: the (Mo, Mo), (Mo, Ta) and (Mo, W) pairs add nothing "
      "to the descriptor, which sums over its other element pairs alone.";
  CHECK(cutoff("5", {"Ta", "Ta", "Mo", "*"}, data) == mo);
  CHECK(cutoff("5", {"Ta", "Ta", "Mo", "Mo", "Mo", "Ta", "Mo", "W"}, data) ==
        mo);

  Context grid;
  grid.add("CGRIDMB", "3.1");
  grid.add("SGRIDMB", "100");
  grid.add("RCUTMB", "3.5");
  const std::string ww =
      "FAIL CGRIDMB(1) = 3.1 with SGRIDMB(1) = 100: the Gaussian "
      "exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every (W, W) neighbour "
      "distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from "
      "the (W, W) pairs of atoms (a smaller SGRIDMB, or a centre nearer the "
      "neighbours, reaches them).";
  CHECK(line_verdict("DM_EAD", "CGRIDMB", data, grid, {"*", "*"}) == ww);
  CHECK(line_verdict("DM_EAD", "CGRIDMB", data, grid,
                     {"Ta", "Ta", "Ta", "W", "W", "W"}) == ww);
}

/* Pins: the channels of a many-body density are judged together: a channel
 * fails when it reaches no pair of the descriptor's line (its channel of ρ
 * is 0 for every atom), and a pair of the line fails when no channel reaches
 * it (ρ gets nothing from it); a channel that misses one pair while another
 * channel reaches that pair is no finding. On the Ta-W-W data
 * (ta_w_ww_stats()), the line Ta Ta W W:
 *  - DM_Blip, CGRIDMB 2.4 3.0 3.6 4.2, SGRIDMB 5 5 5 5 (supports 0.4 A
 *    wide each side), RCUTMB 4.5: no finding, the first channel reaching
 *    the (W, W) distances and the second the (Ta, Ta) ones;
 *  - DM_EAD, CGRIDMB 3.1 3.3, SGRIDMB 100 100, RCUTMB 3.5, both channels
 *    below 1e-6 at every (W, W) distance: "FAIL the channels CGRIDMB(1) and
 *    CGRIDMB(2) with their widths SGRIDMB(1) and SGRIDMB(2): each Gaussian
 *    exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at every (W, W) neighbour
 *    distance within RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from
 *    the (W, W) pairs of atoms (a smaller SGRIDMB, or a centre nearer the
 *    neighbours, reaches them).";
 *  - DM_Blip, CGRIDMB 2.6 3.1 9, SGRIDMB 10 10 10, RCUTMB 3.5, the third
 *    channel reaching no pair: "FAIL CGRIDMB(3) = 9 with SGRIDMB(3) = 10:
 *    the support of the blip, |r - CGRIDMB| < 2/SGRIDMB = 0.2 Å, holds no
 *    (Ta, Ta) neighbour distance within RCUTMB = 3.5 (2.86 to 3.5 Å) and no
 *    (W, W) one (2.5 to 2.7 Å), so its channel of ρ is 0 for every atom (a
 *    smaller SGRIDMB widens the support).".
 * Before the fix: each channel was judged by each pair on its own, and the
 * first channel that missed a pair failed, whatever the other channels
 * reached: the grid 2.4 to 4.2 failed at its first channel ("FAIL
 * CGRIDMB(1) = 2.4 with SGRIDMB(1) = 5: the support of the blip, ... holds
 * no (Ta, Ta) neighbour distance within RCUTMB = 4.5 (2.86 to 4.5 Å), so ρ
 * gets nothing from the (Ta, Ta) pairs of atoms ..."), and so did the other
 * two grids ("FAIL CGRIDMB(1) = 3.1 with SGRIDMB(1) = 100: the Gaussian
 * ... (W, W) ...", "FAIL CGRIDMB(1) = 2.6 with SGRIDMB(1) = 10: ... (Ta,
 * Ta) ...").
 * Before the message named the channel's part of ρ, the third channel said
 * "..., so ρ is 0 for every atom (...)", although the first two reach the
 * (W, W) and (Ta, Ta) distances and ρ is not 0. */
TEST_CASE("The channels of a density are judged together on each element "
          "pair of its line",
          "[metadata][density_reach][line_pairs]") {
  const DataStats data = ta_w_ww_stats();
  const std::vector<std::string> line = {"Ta", "Ta", "W", "W"};
  auto grid = [](const char *c, const char *s, const char *rc) {
    Context g;
    g.add("CGRIDMB", c);
    g.add("SGRIDMB", s);
    g.add("RCUTMB", rc);
    return g;
  };
  CHECK(line_verdict("DM_Blip", "CGRIDMB", data,
                     grid("2.4 3.0 3.6 4.2", "5 5 5 5", "4.5"), line) ==
        std::string());
  CHECK(line_verdict("DM_EAD", "CGRIDMB", data,
                     grid("3.1 3.3", "100 100", "3.5"), line) ==
        "FAIL the channels CGRIDMB(1) and CGRIDMB(2) with their widths "
        "SGRIDMB(1) and SGRIDMB(2): each Gaussian exp(-SGRIDMB (r - "
        "CGRIDMB)^2) is below 1e-6 at every (W, W) neighbour distance within "
        "RCUTMB = 3.5 (2.5 to 2.7 Å), so ρ gets nothing from the (W, W) "
        "pairs of atoms (a smaller SGRIDMB, or a centre nearer the "
        "neighbours, reaches them).");
  CHECK(line_verdict("DM_Blip", "CGRIDMB", data,
                     grid("2.6 3.1 9", "10 10 10", "3.5"), line) ==
        "FAIL CGRIDMB(3) = 9 with SGRIDMB(3) = 10: the support of the blip, "
        "|r - CGRIDMB| < 2/SGRIDMB = 0.2 Å, holds no (Ta, Ta) neighbour "
        "distance within RCUTMB = 3.5 (2.86 to 3.5 Å) and no (W, W) one (2.5 "
        "to 2.7 Å), so its channel of ρ is 0 for every atom (a smaller "
        "SGRIDMB widens the support).");
}

/* Pins: the cutoff of every registered descriptor (each RCUT2B and RCUTMB
 * parameter of its metadata) is marked to reach each element pair of the
 * descriptor's line (ParamMeta::reach_each_pair), as its check judges each
 * pair at the cutoff (cutoff_reach_check()), and no other parameter is:
 * tadah analysis suggest_bounds (Tadah.MLIP) then suggests, for a line of
 * several pairs, a cutoff range whose every value reaches each pair.
 * Before the fix: does not compile (no ParamMeta::reach_each_pair): the
 * range of every parameter of a line of several pairs held the range of
 * each pair, so that a cutoff range began at the smallest low end of the
 * pairs' ranges, which can lie below every distance of another pair of the
 * line, where the check of the cutoff fails. */
TEST_CASE("The cutoff of every descriptor is marked to reach each element "
          "pair of its line",
          "[metadata][suggest][cutoff][line_pairs]") {
  std::size_t cutoffs = 0;
  for (const auto &label : tadah::core::Registry<D_Base>::names()) {
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
    const auto meta = d->meta();
    if (!meta.has_value()) continue;
    for (const auto &pm : meta->params) {
      const bool cutoff = pm.key == "RCUT2B" || pm.key == "RCUTMB";
      CAPTURE(label, pm.key);
      CHECK(pm.reach_each_pair == cutoff);
      if (cutoff) ++cutoffs;
    }
  }
  CHECK(cutoffs > 10u);
}
