#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/cutoffs.h>
#include <cmath>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {
struct ReamFixture { ReamFixture() { PeriodicTable::initialize(); } };

Context make_ream_context() {
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", 0);          // basis = Blip
  // size tokens per keys {C,S,AMP,Ec,Es} then data_keys {Kd,Ke}
  c.add("TYPEMB", 4); c.add("TYPEMB", 4); c.add("TYPEMB", 4);
  c.add("TYPEMB", 5); c.add("TYPEMB", 5);
  c.add("TYPEMB", 0); c.add("TYPEMB", 0);
  c.add("TYPEMB", "H");
  c.add("TYPEMB", "H");
  // density expansion (4 baked terms)
  c.add("CGRIDMB", "1.0 2.0 3.0 4.0");
  c.add("SGRIDMB", "1.0 1.0 1.0 1.0");
  c.add("AMPGRIDMB",    "0.40 0.25 0.12 0.05");
  c.add("RHOFORM", "SIGNED");
  // embedding expansion (5 terms; M output components)
  c.add("CEMBFUNC", "0.0 0.4 0.8 1.2 1.6");
  c.add("SEMBFUNC", "1.5 1.5 1.5 1.5 1.5");
  return c;
}

// Total cluster energy E = sum_i sum_m c_m g_m(rho_i), recomputed from scratch
// so it can be finite-differenced w.r.t. an atom position.
double cluster_energy(DM_REAM &dm, const std::vector<Vec3d> &pos,
                      const t_type &cw) {
  const std::size_t N = pos.size();
  double E = 0.0;
  for (std::size_t i = 0; i < N; ++i) {
    rho_type rhoi; dm.init_rhoi(rhoi);
    for (std::size_t j = 0; j < N; ++j) {
      if (j == i) continue;
      Vec3d del = pos[i] - pos[j];
      dm.calc_rho_mb(1, 1, del, rhoi);
    }
    aed_type aed(dm.size()); aed.set_zero();
    dm.calc_aed(rhoi, aed);
    for (std::size_t m = 0; m < dm.size(); ++m) E += cw[m] * aed(m);
  }
  return E;
}
} // namespace

TEST_CASE_METHOD(ReamFixture, "DM_REAM constructs and sizes", "[ream]") {
  Context c = make_ream_context();
  DM_REAM dm(c);
  REQUIRE(dm.label() == "DM_REAM");
  REQUIRE(dm.size() == 5);          // M = CEMBFUNC.size()
  REQUIRE(dm.rhoi_size() == 1);     // scalar density
  REQUIRE(dm.rhoip_size() == 5);    // one g'_m per output
}

namespace {
// Embedding outputs vs the public blip basis. EMBF0 ZERO (default): the
// zero-referenced basis B(s(rho-c)) - B(s(0-c)); EMBF0 FREE: the raw basis.
void check_ream_basis_sum(bool free) {
  Context c = make_ream_context();
  if (free) c.add("EMBF0", "FREE");
  DM_REAM dm(c);
  Cut_Cos fc(c.get<double>("RCUTMB"));
  dm.set_fcut(&fc, false);

  const std::vector<double> crho{1.0, 2.0, 3.0, 4.0};
  const std::vector<double> srho{1.0, 1.0, 1.0, 1.0};
  const std::vector<double> arho{0.40, 0.25, 0.12, 0.05};
  const std::vector<double> cemb{0.0, 0.4, 0.8, 1.2, 1.6};
  const std::vector<double> semb{1.5, 1.5, 1.5, 1.5, 1.5};

  std::vector<Vec3d> pos = {
      Vec3d(0.0, 0.0, 0.0), Vec3d(2.1, 0.3, 0.4),
      Vec3d(2.7, 1.9, 0.6), Vec3d(1.2, 2.4, 1.1)};

  // Accumulate atom-0 density via the descriptor.
  rho_type rho0; dm.init_rhoi(rho0);
  for (std::size_t j = 1; j < pos.size(); ++j) {
    Vec3d del = pos[0] - pos[j];
    dm.calc_rho_mb(1, 1, del, rho0);
  }

  // Expected rho_0 from the public blip basis.
  double rho0_expected = 0.0;
  for (std::size_t j = 1; j < pos.size(); ++j) {
    Vec3d del = pos[0] - pos[j];
    const double r = std::sqrt(del * del);
    const double fcv = fc.calc(r);
    for (std::size_t n = 0; n < crho.size(); ++n)
      rho0_expected += arho[n] * B(srho[n] * (r - crho[n]), fcv);
  }
  REQUIRE(rho0(0) == Approx(rho0_expected));
  REQUIRE(rho0_expected > 0.0);   // geometry chosen so the embedding is excited

  aed_type aed(dm.size()); aed.set_zero();
  dm.calc_aed(rho0, aed);
  double excited = 0.0;
  for (std::size_t m = 0; m < cemb.size(); ++m) {
    const double raw = B(semb[m] * (rho0_expected - cemb[m]));
    const double expected = free ? raw : raw - B(semb[m] * (0.0 - cemb[m]));
    REQUIRE(aed(m) == Approx(expected).margin(1e-12));
    excited += std::fabs(aed(m));
  }
  REQUIRE(excited > 0.0);
}
} // namespace

TEST_CASE_METHOD(ReamFixture, "DM_REAM energy matches the intended basis sum",
                 "[ream][embf0]") {
  SECTION("EMBF0 ZERO (default): zero-referenced basis") {
    check_ream_basis_sum(false);
  }
  SECTION("EMBF0 FREE: raw basis") { check_ream_basis_sum(true); }
}

TEST_CASE_METHOD(ReamFixture, "DM_REAM force matches finite-difference of energy", "[ream]") {
  Context c = make_ream_context();
  DM_REAM dm(c);
  Cut_Cos fc(c.get<double>("RCUTMB"));
  dm.set_fcut(&fc, false);

  t_type cw(dm.size());
  const double cvals[5] = {0.5, -0.3, 0.2, 0.1, -0.05};
  for (std::size_t m = 0; m < dm.size(); ++m) cw[m] = cvals[m];

  std::vector<Vec3d> pos = {
      Vec3d(0.0, 0.0, 0.0), Vec3d(2.1, 0.3, 0.4),
      Vec3d(2.7, 1.9, 0.6), Vec3d(1.2, 2.4, 1.1)};
  const std::size_t N = pos.size();

  // Per-atom density arrays with derivative slots populated by calc_aed.
  std::vector<rho_type> rho(N);
  for (std::size_t i = 0; i < N; ++i) {
    dm.init_rhoi(rho[i]);
    for (std::size_t j = 0; j < N; ++j) {
      if (j == i) continue;
      Vec3d del = pos[i] - pos[j];
      dm.calc_rho_mb(1, 1, del, rho[i]);
    }
    aed_type aed(dm.size()); aed.set_zero();
    dm.calc_aed(rho[i], aed);
  }

  // Analytic dE/dx_0 = sum_{j} c . FD_0j  (symmetric pair descriptor).
  Vec3d grad_analytic(0.0, 0.0, 0.0);
  for (std::size_t j = 1; j < N; ++j) {
    Vec3d del = pos[0] - pos[j];
    double r = std::sqrt(del * del);
    fd_type FD(dm.size()); FD.set_zero();
    dm.calc_dXijdri_dXjidri(1, 1, r, r * r, del, rho[0], rho[j], FD, 1.0);
    for (std::size_t m = 0; m < dm.size(); ++m)
      for (int k = 0; k < 3; ++k) grad_analytic[k] += cw[m] * FD(m, k);
  }

  // Numeric dE/dx_0 via central difference of the full cluster energy.
  const double h = 1e-6;
  Vec3d grad_numeric(0.0, 0.0, 0.0);
  for (int k = 0; k < 3; ++k) {
    std::vector<Vec3d> pp = pos, pm = pos;
    pp[0][k] += h; pm[0][k] -= h;
    grad_numeric[k] = (cluster_energy(dm, pp, cw) - cluster_energy(dm, pm, cw)) / (2.0 * h);
  }

  for (int k = 0; k < 3; ++k)
    REQUIRE(grad_analytic[k] == Approx(grad_numeric[k]).margin(1e-6));
}

TEST_CASE_METHOD(ReamFixture, "DM_REAM rejects unknown basis code", "[ream]") {
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", 7);          // unknown basis code: must ERROR, not -> Blip
  // (no RHOFORM: the basis code is refused before the density form is read)
  c.add("TYPEMB", 2); c.add("TYPEMB", 2); c.add("TYPEMB", 2);
  c.add("TYPEMB", 2); c.add("TYPEMB", 2);
  c.add("TYPEMB", 0); c.add("TYPEMB", 0);
  c.add("TYPEMB", "H");
  c.add("TYPEMB", "H");
  c.add("CGRIDMB", "1.0 2.0");
  c.add("SGRIDMB", "1.0 1.0");
  c.add("AMPGRIDMB",    "0.4 0.2");
  c.add("CEMBFUNC", "0.0 0.4");
  c.add("SEMBFUNC", "1.5 1.5");
  REQUIRE_THROWS_WITH(DM_REAM(c),
                      Catch::Matchers::Contains("unknown basis code"));
}


namespace {
Context make_ream_context_bcode(int bcode, bool mixed_deg) {
  // knot family (bcode 2): width channels ABSENT (eta is a pure gauge);
  // bump families carry matching SGRID/SEMBFUNC lists
  const bool knot = (bcode == 2);
  Context c;
  c.add("VERBOSE", 0);
  c.add("INITMB", "true");
  c.add("RCUTMB", 6.0);
  c.add("RCTYPEMB", "Cut_Cos");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", bcode);
  c.add("TYPEMB", 3); c.add("TYPEMB", knot ? 0 : 3); c.add("TYPEMB", 3);
  c.add("TYPEMB", 3); c.add("TYPEMB", knot ? 0 : 3);
  c.add("TYPEMB", mixed_deg ? 3 : 0); c.add("TYPEMB", mixed_deg ? 3 : 0);
  c.add("TYPEMB", "H");
  c.add("TYPEMB", "H");
  c.add("CGRIDMB", "2.0 3.5 5.0");
  if (!knot) c.add("SGRIDMB", "1.0 1.0 1.0");
  c.add("AMPGRIDMB", "0.40 0.25 0.12");
  c.add("RHOFORM", "NONNEG");  // non-negative amplitudes: bitwise SIGNED
  c.add("CEMBFUNC", "0.0 0.4 0.8");
  if (!knot) c.add("SEMBFUNC", "1.0 1.0 1.0");
  if (mixed_deg) {
    c.add("KDEGMB", "5 3 5");
    c.add("KDEGEMB", "3 5 5");
  }
  return c;
}
}  // namespace

TEST_CASE_METHOD(ReamFixture,
                 "DM_REAM forces match FD for knot/Blip5 bases (bcode 2/3)",
                 "[ream][knot5][blip5]") {
  // The analytic force chain (dpsi/dgemb) for the NEW families: a sign or
  // degree-dispatch error here silently corrupts every LAMMPS/training force
  // for the new default basis. Mirrors the Blip (bcode 0) FD test.
  struct Case { int bcode; bool mixed; };
  for (const Case cs : {Case{2, false}, Case{2, true}, Case{3, false}}) {
    INFO("bcode=" << cs.bcode << " mixed=" << cs.mixed);
    Context c = make_ream_context_bcode(cs.bcode, cs.mixed);
    DM_REAM dm(c);
    Cut_Cos fc(6.0);
    dm.set_fcut(&fc, false);

    std::vector<Vec3d> pos = {{0.0, 0.0, 0.0},
                              {2.6, 0.3, -0.2},
                              {-0.4, 2.8, 0.5},
                              {1.9, -2.2, 1.4}};
    t_type cw(dm.size());
    for (std::size_t m = 0; m < dm.size(); ++m)
      cw[m] = 0.3 - 0.1 * static_cast<double>(m);

    // analytic dE/dx_0 via the stored g' chain
    const std::size_t N = pos.size();
    std::vector<rho_type> rho(N);
    for (std::size_t i = 0; i < N; ++i) {
      dm.init_rhoi(rho[i]);
      for (std::size_t j = 0; j < N; ++j) {
        if (j == i) continue;
        Vec3d del = pos[i] - pos[j];
        dm.calc_rho_mb(1, 1, del, rho[i]);
      }
      aed_type aed(dm.size());
      aed.set_zero();
      dm.calc_aed(rho[i], aed);  // stores g'_m(rho_i)
    }
    Vec3d grad_analytic(0.0, 0.0, 0.0);
    for (std::size_t j = 1; j < N; ++j) {
      Vec3d del = pos[0] - pos[j];
      double r = std::sqrt(del * del);
      fd_type FD(dm.size());
      FD.set_zero();
      dm.calc_dXijdri_dXjidri(1, 1, r, r * r, del, rho[0], rho[j], FD, 1.0);
      for (std::size_t m = 0; m < dm.size(); ++m)
        for (int k = 0; k < 3; ++k) grad_analytic[k] += cw[m] * FD(m, k);
    }
    const double h = 1e-6;
    for (int k = 0; k < 3; ++k) {
      std::vector<Vec3d> pp = pos, pm = pos;
      pp[0][k] += h;
      pm[0][k] -= h;
      const double num =
          (cluster_energy(dm, pp, cw) - cluster_energy(dm, pm, cw)) / (2 * h);
      REQUIRE(grad_analytic[k] == Approx(num).margin(1e-6));
    }
  }
}


TEST_CASE_METHOD(ReamFixture, "DM_REAM keys contract: arg positions",
                 "[ream]") {
  // contract (d_base.h): get_arg_pos = index-in-keys + fparam, with
  // fparam = nparams - keys.size() - data_keys.size() = 8 - 5 - 2 = 1
  // (the leading basis-code token). The amplitudes are HPO-addressable;
  // the KDEG* data lists are NOT.
  DM_REAM dm;
  REQUIRE(dm.get_arg_pos("CGRIDMB") == 1);
  REQUIRE(dm.get_arg_pos("SGRIDMB") == 2);
  REQUIRE(dm.get_arg_pos("AMPGRIDMB") == 3);
  REQUIRE(dm.get_arg_pos("CEMBFUNC") == 4);
  REQUIRE(dm.get_arg_pos("SEMBFUNC") == 5);
  REQUIRE(dm.get_arg_pos("KDEGMB") == -1);
  REQUIRE(dm.get_arg_pos("KDEGEMB") == -1);
}
