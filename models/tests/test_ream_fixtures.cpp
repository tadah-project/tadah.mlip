/* FEAT-02: ReamDensity on the nine campaign fixture potentials.
 *
 * ream_campaign_fixtures.h copies the density keys of the FEAT-02 fixture
 * pots (DM_REAM Blip5, 20 bumps, Cut_Dummy, WATOM 1) and their expected.tsv
 * results into the test source. scan_grid on [1.7168, RCUTMB] with 20001
 * points must reproduce rho_min, rho_max, r_at_min and r_at_max bit for bit,
 * with one exception: the campaign tool takes f_c = 1 up to and including
 * RCUTMB, tadah's Cut_Dummy gives f_c(RCUTMB) = 0, so D_M3_s301_w4_best's
 * minimum is 0.0 (not 6.8e-30) at the same r = RCUTMB.
 *
 * The lattice density on bcc (a0 3.304 A, a/a0 in [0.75, 1.10]) rises at
 * 1307 of 3501 grid steps (373 of 1001) for ALL_E_s5166_ON and nowhere for
 * the other eight.
 */
#include "catch2/catch.hpp"
#include "ream_campaign_fixtures.h"

#include <tadah/core/context.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/eam/eam_lattice.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

using namespace tadah::core;
using namespace tadah::models;

namespace {

struct FixtureInit {
  FixtureInit() { PeriodicTable::initialize(); }
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

// The fixture pot's density block, RHOFORM SIGNED (its amplitudes are signed).
Context fixture_ctx(const ream_campaign::Fixture &f) {
  Context c;
  c.add("VERBOSE", 0);
  c.add("ATOM", "Ta");
  c.add("WATOM", f.watom);
  c.add("INITMB", "true");
  c.add("RCUTMB", f.rcutmb);
  c.add("RCTYPEMB", "Cut_Dummy");
  c.add("TYPEMB", "DM_REAM 3 20 20 20 3 3 0 0 Ta Ta");
  c.add("CGRIDMB", f.cgrid);
  c.add("SGRIDMB", f.sgrid);
  c.add("AMPGRIDMB", f.amp);
  c.add("CEMBFUNC", "0.0 0.5 1.0");
  c.add("SEMBFUNC", "1.0 1.0 1.0");
  c.add("RHOFORM", "SIGNED");
  return c;
}

const double kRLo = 1.7168;

}  // namespace

TEST_CASE_METHOD(FixtureInit,
                 "ReamDensity::scan_grid reproduces the campaign fixtures' "
                 "expected.tsv bitwise", "[ream][rhoform][fixtures]") {
  REQUIRE(sizeof(ream_campaign::kFixtures) /
              sizeof(ream_campaign::kFixtures[0]) == 9);
  for (const auto &f : ream_campaign::kFixtures) {
    INFO(f.name);
    const Context ctx = fixture_ctx(f);
    const ReamDensity d = ReamDensity::from_context(ctx);
    const double rcut = ctx.get<double>("RCUTMB");
    Cut_Dummy cut(rcut);
    REQUIRE_NOTHROW(d.validate(&cut));
    REQUIRE(ctx.get<double>("WATOM") == 1.0);  // rho_raw is the tool's rho

    const DensityScan sc = d.scan_grid(kRLo, rcut, 20001, cut);
    REQUIRE(sc.finite);
    INFO("rho_min " << hex(sc.rho_min) << " expected " << hex(f.rho_min));
    INFO("rho_max " << hex(sc.rho_max) << " expected " << hex(f.rho_max));
    CHECK(bits(sc.rho_max) == bits(f.rho_max));
    CHECK(bits(sc.r_at_max) == bits(f.r_at_max));
    CHECK(bits(sc.r_at_min) == bits(f.r_at_min));
    if (std::string(f.name) == "D_M3_s301_w4_best") {
      CHECK(f.rho_min > 0.0);        // the tool's f_c(RCUTMB) = 1 value
      CHECK(bits(sc.rho_min) == bits(0.0));
      CHECK(sc.r_at_min == rcut);
      CHECK(sc.i_min == 20000);
    } else {
      CHECK(bits(sc.rho_min) == bits(f.rho_min));
    }

    // and through DM_REAM itself: calc_rho's increment at the extrema
    DM_REAM dm(ctx);
    const int Z = PeriodicTable::find_by_symbol("Ta").Z;
    CHECK(bits(dm.pair_density(Z, Z, sc.r_at_max)) == bits(sc.rho_max));
    CHECK(dm.pair_density(Z, Z, sc.r_at_min) == sc.rho_min);
  }
}

TEST_CASE_METHOD(FixtureInit,
                 "ReamDensity::rhobar_scan on bcc: ALL_E_s5166_ON rises at "
                 "1307 of 3501 (373 of 1001) steps, the other fixtures never",
                 "[ream][rhoform][fixtures]") {
  const double a0 = 3.304, f_lo = 0.75, f_hi = 1.10;
  for (const auto &f : ream_campaign::kFixtures) {
    INFO(f.name);
    const Context ctx = fixture_ctx(f);
    const ReamDensity d = ReamDensity::from_context(ctx);
    const double rcut = ctx.get<double>("RCUTMB");
    Cut_Dummy cut(rcut);
    const auto shells =
        eam_lattice::shells("bcc", rcut / (f_lo * a0) * (1.0 + 1e-12));
    const bool all_e = std::string(f.name) == "ALL_E_s5166_ON";
    for (const std::size_t npts : {std::size_t(3501), std::size_t(1001)}) {
      INFO("NPTS " << npts);
      std::vector<double> a = ReamDensity::linspace(f_lo, f_hi, npts);
      for (double &x : a) x = a0 * x;
      const RhoBarScan rb = d.rhobar_scan(shells, a, cut);
      CHECK(rb.finite);
      if (npts == 3501)
        CHECK(rb.rises == static_cast<std::size_t>(f.rhobar_rises_3501));
      if (all_e)
        CHECK(rb.rises == (npts == 3501 ? 1307u : 373u));
      else
        CHECK(rb.rises == 0);
    }
  }
}
