#include "catch2/catch.hpp"
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/eam/eam_setfl_io.h>
#include <tadah/models/eam/eam_refit.h>
#include <tadah/models/eam/eam_lattice.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/embed_spec.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/dc_selector.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace tadah::models;

TEST_CASE("Refit evaluator agrees with saved smooth cutoff density", "[unified-dm][refit][cutoff]") {
  tadah::core::PeriodicTable::initialize();
  EAMSetflIO eam;eam.read_setfl("tests_data/eam.alloy");eam.build_splines();
  EAMRefitConfig cfg;cfg.rctype="Cut_Cos";cfg.n2b=8;cfg.nrho=8;cfg.nF=12;
  EAMRefit fit(eam,cfg);fit.fit();auto c=fit.build_pot_context();DC_Selector selector(c);
  for(double r:{1.4,2.7,4.2}) {
    double rho,d;selector.dmb->eam_density(73,73,r,rho,d);
    REQUIRE(rho==Approx(fit.rho_fit(r)).margin(1e-13));
  }
}

namespace {
struct RefitFixture { RefitFixture() { tadah::core::PeriodicTable::initialize(); } };

/* A scratch file name unique per process (ctest runs the cases as
 * concurrent processes), removed however the test case ends (a failed
 * REQUIRE throws past a trailing std::remove). */
struct ScratchFile {
  std::string path;
  explicit ScratchFile(const std::string &stem)
      : path(stem + "_" + std::to_string(::getpid()) + ".eam.alloy") {}
  ScratchFile(const ScratchFile &) = delete;
  ScratchFile &operator=(const ScratchFile &) = delete;
  ~ScratchFile() { std::remove(path.c_str()); }
};

// Minimal single-element setfl (header + tiny tabulated block) with a chosen
// lattice token and lattice constant: 3-point tables, the smallest
// read_setfl() accepts (the spline reads a table's third point). It wrote
// 2-point tables (nr = nrho = 2), which read_setfl() now refuses.
void write_min_setfl(const std::string &path, const std::string &lat,
                     double a0) {
  std::ofstream os(path);
  os << "# minimal setfl for tests\n# comment 2\n# comment 3\n"
     << "1 Cu\n"
     << "3 0.5 3 0.5 1.0\n"
     << "29 63.55 " << a0 << " " << lat << "\n"
     << "0.0 0.1 0.15\n0.2 0.3 0.35\n0.4 0.5 0.55\n";
}

// Relative RMS error of f_fit vs f_tab sampled over [a,b], normalised by the
// peak-to-peak amplitude of f_tab over the same range.
template <class FFit, class FTab>
double rel_rmse(FFit fit, FTab tab, double a, double b, int N) {
  double se = 0.0, lo = 1e300, hi = -1e300;
  for (int i = 0; i < N; ++i) {
    const double x = a + (b - a) * i / (N - 1);
    const double t = tab(x);
    const double d = fit(x) - t;
    se += d * d;
    lo = std::min(lo, t); hi = std::max(hi, t);
  }
  const double amp = (hi - lo) > 0 ? (hi - lo) : 1.0;
  return std::sqrt(se / N) / amp;
}
} // namespace

TEST_CASE_METHOD(RefitFixture, "EAMRefit reproduces the Ta2 EAM functions", "[refit]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  // EMBF0 ZERO (default) fits F_ref = F_tab - F_tab(0); FREE fits the raw
  // table (F_ref == F_tab verbatim) — the tadah 1.5.0 dense fit.
  for (const bool zero : {true, false}) {
    INFO("EMBF0 " << (zero ? "ZERO" : "FREE"));
    EAMRefitConfig cfg;          // defaults
    cfg.embf0_zero = zero;
    EAMRefit refit(eam, cfg);
    refit.fit();

    const double rcut = eam.rcut();
    const double r_lo = 0.20 * rcut;
    const double rhomax = eam.rhomax();

    const double phi_rel = rel_rmse([&](double r){ return refit.phi_fit(r); },
                                    [&](double r){ return eam.phi_tab(r); },
                                    r_lo, rcut, 400);
    const double rho_rel = rel_rmse([&](double r){ return refit.rho_fit(r); },
                                    [&](double r){ return eam.rho_tab(r); },
                                    r_lo, rcut, 400);
    // F fidelity contract (default windowed placement): TIGHT inside the
    // physically reachable window [0, rho_reach], crude-but-bounded on the
    // unreachable tail (it keeps only a small uniform basis by design).
    const double F_rel_win = rel_rmse([&](double x){ return refit.F_fit(x); },
                                      [&](double x){ return refit.F_ref(x); },
                                      0.0, refit.rho_reach(), 400);
    const double F_rel_full = rel_rmse([&](double x){ return refit.F_fit(x); },
                                       [&](double x){ return refit.F_ref(x); },
                                       0.0, rhomax, 400);

    std::cout << "[refit] EMBF0 " << (zero ? "ZERO" : "FREE")
              << " phi rel_rmse=" << phi_rel
              << " rho rel_rmse=" << rho_rel
              << " F rel_rmse win/full=" << F_rel_win << "/" << F_rel_full
              << std::endl;
    std::cout << "[refit] phi maxabs=" << refit.phi_stats().maxabs
              << " rho maxabs=" << refit.rho_stats().maxabs
              << " F maxabs=" << refit.F_stats().maxabs << std::endl;

    REQUIRE(phi_rel < 0.02);
    REQUIRE(rho_rel < 0.015);
    REQUIRE(F_rel_win < 0.002);
    REQUIRE(F_rel_full < 0.10);
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit windowed placement: reachability + F(0) = 0 "
                 "(EMBF0 ZERO)",
                 "[refit][embf0]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  EAMRefitConfig cfg;  // defaults: placement = "window", EMBF0 ZERO
  REQUIRE(cfg.embf0_zero);
  EAMRefit refit(eam, cfg);
  refit.fit();

  // reachability diagnostics populated and sane
  REQUIRE(refit.lattice_known());
  REQUIRE(refit.r_reach() > 0.0);
  REQUIRE(refit.r_reach() < eam.rcut());
  REQUIRE(refit.rho_reach() > 0.0);
  REQUIRE(refit.rho_reach() <= eam.rhomax());

  // F window fidelity against the zero-referenced table; the unreachable tail
  // is crude BY DESIGN
  REQUIRE(refit.F_ref(0.0) == 0.0);
  const double F_win = rel_rmse([&](double x){ return refit.F_fit(x); },
                                [&](double x){ return refit.F_ref(x); },
                                0.0, refit.rho_reach(), 400);
  REQUIRE(F_win < 2e-3);

  // isolated-atom reference: F(0) = 0 EXACTLY by construction (no pin), and
  // the round-trip setfl table starts at exactly 0.0
  REQUIRE(refit.F_fit(0.0) == 0.0);
  REQUIRE(refit.F_setfl(0.0) == 0.0);
  {
    const std::string p = "tmp_refit_zero.eam.alloy";
    eam.write_setfl(p, eam.input_grid(),
                    [&](double x) { return refit.F_setfl(x); },
                    [&](double r) { return refit.rho_setfl(r); },
                    [&](double r) { return refit.z2_setfl(r); }, "test");
    EAMSetflIO back;
    back.read_setfl(p);
    std::remove(p.c_str());
    REQUIRE(back.frho().at(0) == 0.0);
  }

  // legacy uniform placement still works
  EAMRefitConfig ucfg;
  ucfg.placement = "uniform";
  EAMRefit urefit(eam, ucfg);
  urefit.fit();
  const double F_uni = rel_rmse([&](double x){ return urefit.F_fit(x); },
                                [&](double x){ return urefit.F_ref(x); },
                                0.0, eam.rhomax(), 400);
  REQUIRE(F_uni < 0.015);
  REQUIRE(urefit.F_fit(0.0) == 0.0);
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit windowed placement: reachability + F(0) pin "
                 "(EMBF0 FREE)",
                 "[refit][embf0]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  EAMRefitConfig cfg;  // defaults: placement = "window", constrain_F0 = true
  cfg.embf0_zero = false;
  EAMRefit refit(eam, cfg);
  refit.fit();

  // reachability diagnostics populated and sane
  REQUIRE(refit.lattice_known());
  REQUIRE(refit.r_reach() > 0.0);
  REQUIRE(refit.r_reach() < eam.rcut());
  REQUIRE(refit.rho_reach() > 0.0);
  REQUIRE(refit.rho_reach() <= eam.rhomax());

  // F window fidelity is tight; the unreachable tail is crude BY DESIGN
  const double F_win = rel_rmse([&](double x){ return refit.F_fit(x); },
                                [&](double x){ return eam.F_tab(x); },
                                0.0, refit.rho_reach(), 400);
  REQUIRE(F_win < 2e-3);

  // isolated-atom reference: F(0) pinned to the table's value
  REQUIRE(std::fabs(refit.F_fit(0.0) - eam.F_tab(0.0)) < 1e-4);
  // FREE: F_ref is the raw table verbatim
  REQUIRE(refit.F_ref(0.7) == eam.F_tab(0.7));

  // legacy uniform placement still works
  EAMRefitConfig ucfg;
  ucfg.placement = "uniform";
  ucfg.embf0_zero = false;
  EAMRefit urefit(eam, ucfg);
  urefit.fit();
  const double F_uni = rel_rmse([&](double x){ return urefit.F_fit(x); },
                                [&](double x){ return eam.F_tab(x); },
                                0.0, eam.rhomax(), 400);
  REQUIRE(F_uni < 0.015);
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit compact-mode injection round-trips",
                 "[refit][embf0]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();
  // EMBF0 ZERO (default) and the FREE sibling: the injected F blips at 0.0
  // and 0.6 cover rho = 0, so the two references differ.
  for (const bool zero : {true, false}) {
    INFO("EMBF0 " << (zero ? "ZERO" : "FREE"));
    EAMRefitConfig cfg;
    cfg.n2b = 5; cfg.nrho = 4; cfg.nF = 6;  // injected sizes
    cfg.embf0_zero = zero;
    EAMRefit refit(eam, cfg);
    refit.analyze_reachability();
    REQUIRE(refit.r_reach() > 0.0);

    std::vector<double> mu2{2.0, 2.8, 3.6, 4.4, 5.2}, et2(5, 1.2),
        c2{1.0, -0.5, 0.2, -0.1, 0.05};
    std::vector<double> mur{2.2, 3.1, 4.0, 4.9}, etr(4, 1.0),
        cr{0.4, 0.3, 0.2, 0.1};
    std::vector<double> muF{0.0, 0.6, 1.2, 1.8, 2.4, 3.0}, etF(6, 1.5),
        cF{0.0, -1.0, -1.5, -1.2, -0.6, 0.2};
    refit.set_blip_fit(EAMRefit::Fn::PHI, mu2, et2, c2);
    refit.set_blip_fit(EAMRefit::Fn::RHO, mur, etr, cr);
    refit.set_blip_fit(EAMRefit::Fn::F, muF, etF, cF);
    refit.finalize_injected(2.0, 3.0);
    refit.set_rho_join(3.0);

    // pot context carries the injected grids and WEIGHTS = [c2b | cmb]
    tadah::core::Context c = refit.build_pot_context();
    REQUIRE(c.size("CGRID2B") == 5);
    REQUIRE(c.size("AMPGRIDMB") == 4);
    REQUIRE(c.size("CEMBFUNC") == 6);
    REQUIRE(c.size("WEIGHTS") == 11);
    REQUIRE(c.get<double>("WEIGHTS", 0) == Approx(1.0));
    REQUIRE(c.get<double>("WEIGHTS", 5) == Approx(0.0));
    // the embedding reference is written explicitly
    const auto embf0 = c.stored_value("EMBF0");
    REQUIRE(embf0);
    REQUIRE(*embf0 == (zero ? "ZERO" : "FREE"));

    // F(0): exactly 0 under ZERO; the raw sum c_m g_m(0) under FREE
    double raw0 = 0.0;
    for (std::size_t m = 0; m < muF.size(); ++m)
      raw0 += cF[m] * B(etF[m] * (0.0 - muF[m]));
    REQUIRE(raw0 != 0.0);
    if (zero) {
      REQUIRE(refit.F_fit(0.0) == 0.0);
      REQUIRE(refit.F_setfl(0.0) == 0.0);
    } else {
      REQUIRE(refit.F_fit(0.0) == Approx(raw0).margin(1e-14));
    }

    // F_setfl: identical to F_fit inside the join, original+CONSTANT offset
    // beyond it, C0 at the join
    REQUIRE(refit.F_setfl(2.0) == Approx(refit.F_fit(2.0)));
    REQUIRE(refit.F_setfl(3.0) == Approx(refit.F_fit(3.0)).margin(1e-12));
    const double off1 = refit.F_setfl(4.0) - eam.F_tab(4.0);
    const double off2 = refit.F_setfl(6.0) - eam.F_tab(6.0);
    REQUIRE(off1 == Approx(off2).margin(1e-10));

    // stats were computed on the requested windows, F against F_ref
    REQUIRE(refit.phi_stats().n == 1000);
    REQUIRE(refit.F_stats().n == 1000);
    double mx = 0.0;
    for (int i = 0; i < 1000; ++i) {
      const double x = 3.0 * i / 999;
      mx = std::max(mx, std::fabs(refit.F_fit(x) - refit.F_ref(x)));
    }
    REQUIRE(refit.F_stats().maxabs == Approx(mx).margin(1e-12));
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit setfl continuation below r_lo follows the original",
                 "[refit]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();
  EAMRefitConfig cfg;
  EAMRefit refit(eam, cfg);
  refit.fit();

  const double rl = refit.r_lo();
  // C0 at the join
  REQUIRE(refit.phi_setfl(rl) == Approx(refit.phi_fit(rl)).margin(1e-10));
  REQUIRE(refit.rho_setfl(rl) == Approx(refit.rho_fit(rl)).margin(1e-10));
  // below the join: original shape with a CONSTANT phi offset
  const double off1 = refit.phi_setfl(0.5 * rl) - eam.phi_tab(0.5 * rl);
  const double off2 = refit.phi_setfl(0.8 * rl) - eam.phi_tab(0.8 * rl);
  REQUIRE(off1 == Approx(off2).margin(1e-8));
  // z2r finite at the origin (the writer tabulates z2_setfl from r=0)
  REQUIRE(std::isfinite(refit.z2_setfl(0.0)));
  // identity above the join
  REQUIRE(refit.phi_setfl(1.5 * rl) == Approx(refit.phi_fit(1.5 * rl)));
}

TEST_CASE_METHOD(RefitFixture, "EAMRefit assembles a structurally valid pot context", "[refit]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  EAMRefitConfig cfg;
  EAMRefit refit(eam, cfg);
  refit.fit();

  tadah::core::Context c = refit.build_pot_context();
  // WATOM must be pinned to 1.0: when absent, the HPO pipeline fills it from
  // the blueprint default (atomic number), scaling DM_REAM's density by Z.
  REQUIRE(c.contains("WATOM"));
  REQUIRE(c.get<double>("WATOM", 0) == Approx(1.0));
  REQUIRE(c.get<std::string>("MODEL", 0) == "M_BLR");
  REQUIRE(c.get<std::string>("MODEL", 1) == "BF_Linear");
  REQUIRE(c.get<std::string>("TYPE2B", 0) == "D2_Blip");
  REQUIRE(c.get<std::string>("TYPEMB", 0) == "DM_REAM");
  REQUIRE(c.size("CGRID2B") == static_cast<std::size_t>(cfg.n2b));
  REQUIRE(c.size("AMPGRIDMB") == static_cast<std::size_t>(cfg.nrho));
  REQUIRE(c.size("CEMBFUNC") == static_cast<std::size_t>(cfg.nF));
  // WEIGHTS = [c2b | cmb] = n2b + nF
  REQUIRE(c.size("WEIGHTS") ==
          static_cast<std::size_t>(cfg.n2b + cfg.nF));
}


TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit honours injected knot degrees and emits DGRID keys",
                 "[refit][knotmix]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();
  EAMRefitConfig cfg;
  cfg.basis = "KnotMix";
  EAMRefit refit(eam, cfg);
  refit.analyze_reachability();

  const std::vector<double> mu = {3.0, 4.0, 5.0};
  const std::vector<double> eta = {1.0, 1.0, 1.0};
  const std::vector<double> c = {0.2, -0.1, 0.05};
  const std::vector<int> deg = {5, 3, 5};
  refit.set_blip_fit(EAMRefit::Fn::PHI, mu, eta, c, deg);
  refit.set_blip_fit(EAMRefit::Fn::RHO, mu, eta, c);       // all 5
  refit.set_blip_fit(EAMRefit::Fn::F, {0.1, 0.5}, {1.0, 1.0},
                     {0.3, -0.2}, {3, 5});
  refit.finalize_injected(2.0, 1.0);

  // (a) degree honoured in evaluation: middle phi atom must be CUBIC
  const double r = 3.5;
  const double expect = c[0] * K5L(r - mu[0]) +
                        c[1] * K3L(r - mu[1]) +
                        c[2] * K5L(r - mu[2]);
  REQUIRE(refit.phi_fit(r) == Approx(expect).margin(1e-12));

  // (b) F right-active with cubic first atom
  const double rho = 0.7;
  const double expectF = 0.3 * K3R(rho - 0.1) +
                         -0.2 * K5R(rho - 0.5);
  REQUIRE(refit.F_fit(rho) == Approx(expectF).margin(1e-12));

  // (c) writer: TYPE2B family name, bcode, DGRID emission (and absence)
  tadah::core::Context pot = refit.build_pot_context();
  REQUIRE(pot.get<std::string>("TYPE2B") == "D2_Knot5");
  REQUIRE(pot.get<int>("TYPEMB", 1) == 2);
  REQUIRE(pot.size("KDEG2B") == 3);
  tadah::core::v_type d2b(pot.size("KDEG2B"));
  pot.get("KDEG2B", d2b);
  REQUIRE(d2b[0] == 5.0);
  REQUIRE(d2b[1] == 3.0);
  REQUIRE(d2b[2] == 5.0);
  REQUIRE(!pot.contains("KDEGMB"));    // all-quintic: no key (back-compat)
  REQUIRE(pot.size("KDEGEMB") == 2);
  // knot family: NO width keys anywhere (eta is a pure gauge)
  REQUIRE(!pot.contains("SGRID2B"));
  REQUIRE(!pot.contains("SGRIDMB"));
  REQUIRE(!pot.contains("SEMBFUNC"));
  // TYPE2B contract: D2_Knot5 <nC> <nKdeg> El El
  REQUIRE(pot.get<int>("TYPE2B", 1) == 3);
  REQUIRE(pot.get<int>("TYPE2B", 2) == 3);
}

TEST_CASE_METHOD(RefitFixture, "EAMRefit emits per-family TYPE2B names",
                 "[refit]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();
  for (const auto &[basis, t2b, bcode] :
       {std::tuple<std::string, std::string, int>{"Blip", "D2_Blip", 0},
        {"Blip5", "D2_Blip5", 3},
        {"Knot5", "D2_Knot5", 2},
        {"Gaussian", "D2_BP", 1}}) {
    EAMRefitConfig cfg;
    cfg.basis = basis;
    EAMRefit refit(eam, cfg);
    // inject rather than fit(): the dense LSQ path is Blip/Gaussian-only —
    // this test exercises the WRITER's family dispatch
    refit.set_blip_fit(EAMRefit::Fn::PHI, {3.0, 4.0}, {1.0, 1.0},
                       {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::RHO, {3.0, 4.0}, {1.0, 1.0},
                       {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::F, {0.2, 0.6}, {1.0, 1.0},
                       {0.1, 0.2});
    refit.finalize_injected(2.0, 1.0);
    tadah::core::Context pot = refit.build_pot_context();
    INFO("basis=" << basis);
    REQUIRE(pot.get<std::string>("TYPE2B") == t2b);
    REQUIRE(pot.get<int>("TYPEMB", 1) == bcode);
    if (basis == "Knot5") {
      REQUIRE(!pot.contains("SGRID2B"));
      REQUIRE(!pot.contains("SGRIDMB"));
    } else {
      REQUIRE(pot.size("SGRID2B") == 2);
      REQUIRE(pot.size("SGRIDMB") == 2);
    }
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit derives knot-family RCUT from the largest knot",
                 "[refit]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  // Knot family: the basis vanishes at its largest knot, so the emitted
  // RCUT2B/RCUTMB must be the largest knot position, NOT the table cutoff.
  {
    EAMRefitConfig cfg;
    cfg.basis = "Knot5";
    EAMRefit refit(eam, cfg);
    refit.set_blip_fit(EAMRefit::Fn::PHI, {3.0, 4.7}, {1.0, 1.0}, {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::RHO, {2.8, 4.1}, {1.0, 1.0}, {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::F, {0.2, 0.6}, {1.0, 1.0}, {0.1, 0.2});
    refit.finalize_injected(2.0, 1.0);
    tadah::core::Context pot = refit.build_pot_context();
    REQUIRE(pot.get<double>("RCUT2B") == Approx(4.7));
    REQUIRE(pot.get<double>("RCUTMB") == Approx(4.1));
  }
  // Bump families keep the table cutoff: the hard truncation at rcut is
  // part of the fitted model.
  {
    EAMRefitConfig cfg;
    cfg.basis = "Blip";
    EAMRefit refit(eam, cfg);
    refit.set_blip_fit(EAMRefit::Fn::PHI, {3.0, 4.7}, {1.0, 1.0}, {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::RHO, {2.8, 4.1}, {1.0, 1.0}, {0.1, 0.2});
    refit.set_blip_fit(EAMRefit::Fn::F, {0.2, 0.6}, {1.0, 1.0}, {0.1, 0.2});
    refit.finalize_injected(2.0, 1.0);
    tadah::core::Context pot = refit.build_pot_context();
    REQUIRE(pot.get<double>("RCUT2B") == Approx(eam.rcut()));
    REQUIRE(pot.get<double>("RCUTMB") == Approx(eam.rcut()));
  }
}

TEST_CASE("EAMSetflIO normalises the lattice label to lower case",
          "[refit][setfl]") {
  const std::string p = "tmp_min_upper.eam.alloy";
  write_min_setfl(p, "FCC", 3.615);
  EAMSetflIO eam;
  eam.read_setfl(p);
  std::remove(p.c_str());
  REQUIRE(eam.lattice() == "fcc");        // canonical, used by the crystal code
  REQUIRE(eam.lattice_raw() == "FCC");    // original case preserved
  REQUIRE(eam_lattice::supported(eam.lattice()));
  REQUIRE(eam.lattice_param() == Approx(3.615));
  REQUIRE_NOTHROW(
      eam_lattice::require_crystal(eam.lattice_raw(), eam.lattice_param()));
}

TEST_CASE("EAMSetflIO: an unspecified lattice (NONE / a0=0) fails the crystal "
          "gate", "[refit][setfl]") {
  const std::string p = "tmp_min_none.eam.fs";
  write_min_setfl(p, "NONE", 0.0);
  EAMSetflIO eam;
  eam.read_setfl(p);                       // parsing the file itself is fine
  std::remove(p.c_str());
  REQUIRE(eam.lattice() == "none");
  REQUIRE(eam.lattice_raw() == "NONE");
  REQUIRE_FALSE(eam_lattice::supported(eam.lattice()));
  REQUIRE(eam.lattice_param() == Approx(0.0));
  // the gate the refit engine applies must reject it
  REQUIRE_THROWS(
      eam_lattice::require_crystal(eam.lattice_raw(), eam.lattice_param()));
}

TEST_CASE_METHOD(RefitFixture,
                 "crystal_epa: fitted EAM reproduces the cohesive energy",
                 "[refit]") {
  // The self-test's cohesive point: crystal_epa with the original tabulated
  // functions vs the fitted native functions, on the header lattice at a0.
  // EMBF0 ZERO compares against the zero-referenced table F_ref (energies
  // relative to the isolated atom); FREE against the raw table.
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");  // Ta, bcc, a0=3.304
  eam.build_splines();
  for (const bool zero : {true, false}) {
    INFO("EMBF0 " << (zero ? "ZERO" : "FREE"));
    EAMRefitConfig cfg;                      // dense Blip defaults
    cfg.embf0_zero = zero;
    EAMRefit refit(eam, cfg);
    refit.fit();

    const std::string lat = eam.lattice();
    const double a0 = eam.lattice_param(), rc = eam.rcut();
    REQUIRE(eam_lattice::supported(lat));

    const double e_orig = eam_lattice::crystal_epa(
        lat, a0, rc, [&](double r) { return eam.phi_tab(r); },
        [&](double r) { return eam.rho_tab(r); },
        [&](double x) { return refit.F_ref(x); });  // FREE: eam.F_tab
    const double e_fit = eam_lattice::crystal_epa(
        lat, a0, rc, [&](double r) { return refit.phi_fit(r); },
        [&](double r) { return refit.rho_fit(r); },
        [&](double x) { return refit.F_fit(x); });

    REQUIRE(std::isfinite(e_orig));
    REQUIRE(std::isfinite(e_fit));
    const double delta_mev = 1000.0 * std::fabs(e_fit - e_orig);
    std::cout << "[refit] EMBF0 " << (zero ? "ZERO" : "FREE")
              << " cohesive original=" << e_orig << " fit=" << e_fit
              << " delta=" << delta_mev << " meV/atom" << std::endl;
    REQUIRE(delta_mev < 1.0);    // measured ~0.01 meV/atom (dense Blip on Ta);
                                 // a broken self-test/fit lands orders larger
  }
}

namespace {
// The embedding energy the native potential evaluates, sum_m w_m aed_m(rho),
// from a DM_REAM built out of the refit's own pot context, against
// EAMRefit::F_fit at each rho: the offline fit and the runtime descriptor
// must evaluate the same (zero-referenced or raw) embedding.
void check_ream_matches_F_fit(const EAMRefit &refit,
                              const std::vector<double> &rhos) {
  tadah::core::Context pot = refit.build_pot_context();
  const auto embf0 = pot.stored_value("EMBF0");
  REQUIRE(embf0);
  REQUIRE(*embf0 == (refit.embf0_zero() ? "ZERO" : "FREE"));
  DM_REAM dm(pot);
  REQUIRE(dm.embf0_mode().has_value());
  REQUIRE((*dm.embf0_mode() == EmbF0Mode::Zero) == refit.embf0_zero());
  const std::size_t n2b = pot.size("CGRID2B");
  REQUIRE(dm.size() == pot.size("CEMBFUNC"));
  REQUIRE(pot.size("WEIGHTS") == n2b + dm.size());
  for (double rho : rhos) {
    INFO("EMBF0 " << *embf0 << "  rho = " << rho);
    tadah::core::rho_type rhoi;
    dm.init_rhoi(rhoi);
    rhoi(0) = rho;
    tadah::core::aed_type aed(dm.size());
    aed.set_zero();
    dm.calc_aed(rhoi, aed);
    double E = 0.0;
    for (std::size_t m = 0; m < dm.size(); ++m)
      E += pot.get<double>("WEIGHTS", n2b + m) * aed(m);
    const double F = refit.F_fit(rho);
    REQUIRE(std::fabs(E - F) <= 1e-12 * (1.0 + std::fabs(F)));
    if (refit.embf0_zero() && rho == 0.0) {  // isolated atom: exactly zero
      REQUIRE(F == 0.0);
      REQUIRE(E == 0.0);
    }
  }
}

// Copy of a single-element setfl whose embedding table is shifted by a
// constant: header and the rho/z2r blocks verbatim, F + shift written
// lossless (17 significant digits).
void write_F_shifted_setfl(const std::string &src, const std::string &dst,
                           double shift) {
  std::ifstream in(src);
  REQUIRE(in.good());
  std::ofstream os(dst);
  std::string line;
  int nrho = 0;
  for (int k = 0; k < 6; ++k) {  // 3 comments, element, grid, atom lines
    std::getline(in, line);
    os << line << "\n";
    if (k == 4) std::istringstream(line) >> nrho;
  }
  REQUIRE(nrho > 0);
  os << std::scientific << std::setprecision(16);
  std::string tok;
  for (int i = 0; in >> tok; ++i) {
    if (i < nrho) os << " " << (std::stod(tok) + shift);
    else          os << " " << tok;
    if (i % 5 == 4) os << "\n";
  }
  os << "\n";
}
}  // namespace

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit F_fit equals the DM_REAM embedding of its own pot "
                 "(EMBF0 ZERO and FREE)",
                 "[refit][embf0]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();

  SECTION("dense Blip fit") {
    for (const bool zero : {true, false}) {
      EAMRefitConfig cfg;
      cfg.embf0_zero = zero;
      EAMRefit refit(eam, cfg);
      refit.fit();
      const double rr = refit.rho_reach();
      check_ream_matches_F_fit(refit, {0.0, 1e-9, 0.05, 0.3, 1.0, 0.5 * rr,
                                       rr, 0.9 * eam.rhomax()});
    }
  }

  SECTION("injected Knot5: negative centres, KDEGEMB 3") {
    // RIGHT-active knots (rho - mu)_+^deg: g(0) != 0 only for mu < 0, so the
    // two negative knots (one cubic) carry the whole reference.
    const std::vector<double> muF{-0.8, -0.3, 0.4, 1.1};
    const std::vector<double> cF{0.15, -0.4, 0.3, -0.05};
    const std::vector<int> degF{3, 5, 3, 5};
    for (const bool zero : {true, false}) {
      INFO("EMBF0 " << (zero ? "ZERO" : "FREE"));
      EAMRefitConfig cfg;
      cfg.basis = "Knot5";
      cfg.embf0_zero = zero;
      EAMRefit refit(eam, cfg);
      refit.set_blip_fit(EAMRefit::Fn::PHI, {3.0, 4.0, 5.0}, {1.0, 1.0, 1.0},
                         {0.2, -0.1, 0.05}, {5, 3, 5});
      refit.set_blip_fit(EAMRefit::Fn::RHO, {2.8, 3.9, 5.0}, {1.0, 1.0, 1.0},
                         {0.3, 0.2, 0.1});
      refit.set_blip_fit(EAMRefit::Fn::F, muF, std::vector<double>(4, 1.0), cF,
                         degF);
      refit.finalize_injected(2.0, 2.0);
      REQUIRE(refit.build_pot_context().size("KDEGEMB") == 4);
      check_ream_matches_F_fit(refit, {0.0, 1e-9, 0.1, 0.35, 0.8, 1.5, 2.5});

      // independent formula (catches a wrong orientation/degree in g(0))
      const double rho = 0.7;
      double expect = 0.0;
      for (std::size_t m = 0; m < muF.size(); ++m) {
        const double g  = degF[m] == 3 ? K3R(rho - muF[m]) : K5R(rho - muF[m]);
        const double g0 = degF[m] == 3 ? K3R(0.0 - muF[m]) : K5R(0.0 - muF[m]);
        expect += cF[m] * (zero ? g - g0 : g);
      }
      REQUIRE(refit.F_fit(rho) == Approx(expect).margin(1e-12));
      if (zero) REQUIRE(refit.F_fit(0.0) == 0.0);
    }
  }

  SECTION("injected Blip5 bumps covering rho = 0") {
    // support |rho - mu| < 3/eta = 1.5: the first four bumps cover rho = 0
    const std::vector<double> muF{0.0, 0.3, 0.7, 1.2, 1.8};
    const std::vector<double> etF(5, 2.0);
    const std::vector<double> cF{-0.2, -0.6, -0.9, -0.7, -0.3};
    for (const bool zero : {true, false}) {
      INFO("EMBF0 " << (zero ? "ZERO" : "FREE"));
      EAMRefitConfig cfg;
      cfg.basis = "Blip5";
      cfg.embf0_zero = zero;
      EAMRefit refit(eam, cfg);
      refit.set_blip_fit(EAMRefit::Fn::PHI, {2.5, 3.3, 4.1}, {1.5, 1.5, 1.5},
                         {0.5, -0.2, 0.05});
      refit.set_blip_fit(EAMRefit::Fn::RHO, {2.4, 3.2}, {1.5, 1.5},
                         {0.4, 0.2});
      refit.set_blip_fit(EAMRefit::Fn::F, muF, etF, cF);
      refit.finalize_injected(2.0, 2.5);
      check_ream_matches_F_fit(refit,
                               {0.0, 1e-9, 0.1, 0.45, 0.9, 1.6, 2.4, 3.5});

      const double rho = 0.5;
      double expect = 0.0;
      for (std::size_t m = 0; m < muF.size(); ++m) {
        const double g = B5(etF[m] * (rho - muF[m]));
        const double g0 = B5(etF[m] * (0.0 - muF[m]));
        expect += cF[m] * (zero ? g - g0 : g);
      }
      REQUIRE(refit.F_fit(rho) == Approx(expect).margin(1e-12));
      if (zero) {
        REQUIRE(refit.F_fit(0.0) == 0.0);
        REQUIRE(refit.F_setfl(0.0) == 0.0);
      }
    }
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit EMBF0 ZERO is invariant to a constant shift of the "
                 "table F, FREE pins the shifted F(0)",
                 "[refit][embf0]") {
  // The Ta table has F(0) = 0; a shifted copy (F + 0.37 eV) exercises every
  // F_ref use: a raw F_tab left anywhere in the ZERO fit shows up as a 0.37 eV
  // difference, far above the rounding the shift itself introduces.
  const double shift = 0.37;
  const std::string p = "tmp_refit_Fshift.eam.alloy";
  write_F_shifted_setfl("tests_data/eam.alloy", p, shift);
  EAMSetflIO eam_u, eam_s;
  eam_u.read_setfl("tests_data/eam.alloy");
  eam_s.read_setfl(p);
  std::remove(p.c_str());
  eam_u.build_splines();
  eam_s.build_splines();
  REQUIRE(eam_u.F_tab(0.0) == 0.0);
  REQUIRE(eam_s.F_tab(0.0) == shift);

  SECTION("EMBF0 ZERO: same weights, same F_fit, F(0) = 0") {
    EAMRefitConfig cfg;  // EMBF0 ZERO
    EAMRefit ru(eam_u, cfg), rs(eam_s, cfg);
    ru.fit();
    rs.fit();
    REQUIRE(rs.F_ref(0.0) == 0.0);
    REQUIRE(rs.F_ref(0.9) == eam_s.F_tab(0.9) - shift);

    const auto &wu = ru.weights();
    const auto &ws = rs.weights();
    REQUIRE(wu.size() == ws.size());
    double wmax = 0.0, dmax = 0.0;
    for (std::size_t k = 0; k < wu.size(); ++k) {
      wmax = std::max(wmax, std::fabs(wu[k]));
      dmax = std::max(dmax, std::fabs(ws[k] - wu[k]));
    }
    std::cout << "[refit] F+" << shift << " table, EMBF0 ZERO: max|dw| = "
              << dmax << " (max|w| = " << wmax << ")" << std::endl;
    REQUIRE(wmax > 0.0);
    REQUIRE(dmax <= 1e-12 * wmax);   // rounding of the shifted table only

    REQUIRE(ru.F_fit(0.0) == 0.0);
    REQUIRE(rs.F_fit(0.0) == 0.0);
    for (int i = 0; i < 400; ++i) {
      const double x = ru.rho_reach() * i / 399;
      REQUIRE(std::fabs(rs.F_fit(x) - ru.F_fit(x)) <= 1e-12 * wmax);
    }
    // the fit statistics are measured against F_ref: the same quality
    REQUIRE(rs.F_stats().maxabs == Approx(ru.F_stats().maxabs).margin(1e-10));
  }

  SECTION("EMBF0 FREE: the F(0) pin tracks the table's F(0)") {
    EAMRefitConfig cfg;
    cfg.embf0_zero = false;
    EAMRefit ru(eam_u, cfg), rs(eam_s, cfg);
    ru.fit();
    rs.fit();
    REQUIRE(std::fabs(ru.F_fit(0.0) - eam_u.F_tab(0.0)) < 1e-4);
    REQUIRE(std::fabs(rs.F_fit(0.0) - eam_s.F_tab(0.0)) < 1e-4);
    REQUIRE(rs.F_ref(0.9) == eam_s.F_tab(0.9));   // raw table verbatim
    REQUIRE(rs.build_pot_context().stored_value("EMBF0") ==
            std::optional<std::string>("FREE"));
  }
}

// ---------------------------------------------------------------------------
//  RHOFORM (FEAT-02): the density is fitted, evaluated and written in the
//  declared form
// ---------------------------------------------------------------------------
namespace {
// Distance in units in the last place of two finite doubles (0: same bits).
std::uint64_t ulp_distance(double a, double b) {
  if (a == b) return 0;
  auto key = [](double v) {
    std::int64_t i;
    std::memcpy(&i, &v, sizeof i);
    return i < 0 ? std::numeric_limits<std::int64_t>::min() - i : i;
  };
  const std::int64_t ka = key(a), kb = key(b);
  return ka > kb ? static_cast<std::uint64_t>(ka - kb)
                 : static_cast<std::uint64_t>(kb - ka);
}

// A smooth single-element fcc setfl (rcut 5, a0 3.6) with the density table
// rho(r): phi(r) = 3 (1 - r/rcut)^3, F(rho) = -2 sqrt(rho) + 0.05 rho; with
// `wall`, phi also has the steep wall of the window tests,
// 100 exp(-(r - 1.6) / 0.05) (write_window_setfl).
void write_rho_setfl(const std::string &path,
                     const std::function<double(double)> &rho,
                     bool wall = false) {
  const int nr = 400, nrho = 400;
  const double rcut = 5.0, dr = rcut / (nr - 1);
  const double rhomax = 60.0, drho = rhomax / (nrho - 1);
  std::ofstream os(path);
  os << "# synthetic single-element EAM (RHOFORM tests)\n# 2\n# 3\n"
     << "1 Cu\n"
     << nrho << " " << drho << " " << nr << " " << dr << " " << rcut << "\n"
     << "29 63.55 3.6 fcc\n";
  os << std::setprecision(17);
  for (int i = 0; i < nrho; ++i)
    os << (-2.0 * std::sqrt(i * drho) + 0.05 * i * drho)
       << ((i % 5 == 4) ? "\n" : " ");
  os << "\n";
  for (int i = 0; i < nr; ++i) os << rho(i * dr) << ((i % 5 == 4) ? "\n" : " ");
  os << "\n";
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr, x = 1.0 - r / rcut;
    const double phi = (x > 0 ? 3.0 * x * x * x : 0.0) +
                       (wall ? 100.0 * std::exp(-(r - 1.6) / 0.05) : 0.0);
    os << phi * r << ((i % 5 == 4) ? "\n" : " ");
  }
  os << "\n";
}

EAMRefitConfig coarse_config(const std::string &basis, RhoForm form) {
  EAMRefitConfig cfg;
  cfg.basis = basis;
  cfg.rho_form = form;
  cfg.n2b = 24;
  cfg.nrho = 20;
  cfg.nF = 60;
  return cfg;
}
}  // namespace

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit RHOFORM: rho_fit is the pair density of its own "
                 "potential in every form",
                 "[refit][rhoform]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");  // Ravelo Ta2: rho >= 0, decreasing
  eam.build_splines();
  const double rc = eam.rcut();
  struct Case {
    const char *basis;
    RhoForm form;
  };
  for (const Case &cs :
       {Case{"Blip", RhoForm::Signed}, Case{"Blip", RhoForm::NonNeg},
        Case{"Blip", RhoForm::Monotone}, Case{"Blip5", RhoForm::Signed},
        Case{"Blip5", RhoForm::NonNeg}, Case{"Blip5", RhoForm::Monotone},
        Case{"Gaussian", RhoForm::Monotone}}) {
    INFO(cs.basis << " RHOFORM " << rhoform_name(cs.form));
    EAMRefit refit(eam, coarse_config(cs.basis, cs.form));
    REQUIRE(refit.rho_form() == cs.form);
    refit.fit();
    tadah::core::Context pot = refit.build_pot_context();
    REQUIRE(pot.stored_value("RHOFORM") ==
            std::optional<std::string>(rhoform_name(cs.form)));
    if (rhoform_is_positive(cs.form))
      for (std::size_t n = 0; n < pot.size("AMPGRIDMB"); ++n)
        REQUIRE(pot.get<double>("AMPGRIDMB", n) >= 0.0);
    pot.add("VERBOSE", 0);
    DM_REAM dm(pot);  // validates the form (amplitudes, widths, cutoff)
    REQUIRE(dm.density().form == cs.form);
    const int Z = 73;
    std::uint64_t worst = 0;
    double rho_max = 0.0;
    for (int i = 0; i < 4000; ++i) {
      const double r = 0.1 * rc + (0.9 * rc) * i / 4000.0;  // r < rcut
      const double a = refit.rho_fit(r), b = dm.pair_density(Z, Z, r);
      REQUIRE(std::isfinite(a));
      worst = std::max(worst, ulp_distance(a, b));
      rho_max = std::max(rho_max, std::fabs(a));
    }
    std::cout << "[refit] " << cs.basis << " RHOFORM "
              << rhoform_name(cs.form)
              << ": max ulp(rho_fit, pair_density) = " << worst << std::endl;
    CHECK(worst <= 1);
    // the tabulation keeps the form's sign below r_lo as well
    if (rhoform_is_positive(cs.form))
      for (int i = 0; i <= 400; ++i)
        REQUIRE(refit.rho_setfl(rc * i / 400.0) >= 0.0);
    // MONOTONE: non-increasing to rounding (Blip steps have ulp rises)
    if (cs.form == RhoForm::Monotone) {
      double prev = refit.rho_fit(0.05 * rc), rise = 0.0;
      for (int i = 1; i <= 20000; ++i) {
        const double r = 0.05 * rc + 0.95 * rc * i / 20000.0;
        const double v = refit.rho_fit(r);
        rise = std::max(rise, v - prev);
        prev = v;
      }
      CHECK(rise <= 64.0 * std::numeric_limits<double>::epsilon() * rho_max);
    }
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit RHOFORM: NONNEG equals SIGNED on a table whose "
                 "unconstrained fit is non-negative, MONOTONE fits the "
                 "decreasing Ta density",
                 "[refit][rhoform]") {
  EAMSetflIO eam;
  eam.read_setfl("tests_data/eam.alloy");
  eam.build_splines();
  EAMRefit rs(eam, coarse_config("Blip5", RhoForm::Signed));
  EAMRefit rn(eam, coarse_config("Blip5", RhoForm::NonNeg));
  EAMRefit rm(eam, coarse_config("Blip5", RhoForm::Monotone));
  rs.fit();
  rn.fit();
  rm.fit();
  const tadah::core::Context ps = rs.build_pot_context();
  const tadah::core::Context pn = rn.build_pot_context();
  REQUIRE(ps.size("AMPGRIDMB") == pn.size("AMPGRIDMB"));
  double amax = 0.0, dmax = 0.0, smin = 1e300;
  for (std::size_t n = 0; n < ps.size("AMPGRIDMB"); ++n) {
    const double a = ps.get<double>("AMPGRIDMB", n);
    smin = std::min(smin, a);
    amax = std::max(amax, std::fabs(a));
    dmax = std::max(dmax, std::fabs(pn.get<double>("AMPGRIDMB", n) - a));
  }
  REQUIRE(smin > 0.0);            // the GELSD optimum is already feasible
  CHECK(dmax <= 1e-12 * amax);    // NNLS = its passive-set LS solution
  // phi and F never see the form
  REQUIRE(rs.weights().size() == rn.weights().size());
  for (std::size_t k = 0; k < rs.weights().size(); ++k)
    REQUIRE(rs.weights()[k] == rn.weights()[k]);
  // the step expansion fits a smooth decreasing density closely
  std::cout << "[refit] Ta rho fit max|err|: SIGNED " << rs.rho_stats().maxabs
            << "  NONNEG " << rn.rho_stats().maxabs << "  MONOTONE "
            << rm.rho_stats().maxabs << std::endl;
  CHECK(rm.rho_stats().maxabs <= 1e-4 * eam.rho_tab(rm.r_lo()));
  CHECK(rm.rho_stats().maxabs < 1e-2 * rs.rho_stats().maxabs);
}

/* Pins (the two table pre-checks): a refusal of the table names the
 * command, "tadah refit: RHOFORM NONNEG cannot fit this table: ...", and
 * ends with the form that fits it, "RHOFORM SIGNED fits it." ("RHOFORM
 * NONNEG or SIGNED fits it." for MONOTONE).
 * Before the fix: "EAMRefit: RHOFORM ... Use RHOFORM SIGNED." and "... Use
 * RHOFORM NONNEG or SIGNED.", a class name and an instruction. */
TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit RHOFORM: table pre-checks and form guards",
                 "[refit][rhoform]") {
  SECTION("NONNEG / MONOTONE refuse a negative table, naming r") {
    const ScratchFile f("tmp_rhoform_neg");
    const std::string &p = f.path;
    // negative for r > rcut (1 - sqrt(0.02)) = 4.29 A
    write_rho_setfl(p, [](double r) {
      const double x = 1.0 - r / 5.0;
      return x * x - 0.02;
    });
    EAMSetflIO eam;
    eam.read_setfl(p);
    eam.build_splines();
    for (RhoForm f : {RhoForm::NonNeg, RhoForm::Monotone}) {
      INFO(rhoform_name(f));
      EAMRefit refit(eam, coarse_config("Blip", f));
      REQUIRE_THROWS_WITH(
          refit.fit(),
          Catch::Contains(std::string("RHOFORM ") + rhoform_name(f) +
                          " cannot fit this table") &&
              Catch::Contains("< 0 at r = 4.29") &&
              Catch::Contains("RHOFORM SIGNED fits it.") &&
              Catch::StartsWith("tadah refit: RHOFORM "));
    }
    EAMRefit signed_fit(eam, coarse_config("Blip", RhoForm::Signed));
    REQUIRE_NOTHROW(signed_fit.fit());
  }
  SECTION("MONOTONE refuses a rising table, naming r; NONNEG fits it") {
    const ScratchFile f("tmp_rhoform_rise");
    const std::string &p = f.path;
    // a bump on a decreasing background: rises just below r = 3.5
    write_rho_setfl(p, [](double r) {
      const double x = 1.0 - r / 5.0, g = (r - 3.5) / 0.2;
      return x * x + 0.05 * std::exp(-g * g);
    });
    EAMSetflIO eam;
    eam.read_setfl(p);
    eam.build_splines();
    EAMRefit mono(eam, coarse_config("Blip", RhoForm::Monotone));
    REQUIRE_THROWS_WITH(mono.fit(),
                        Catch::Contains("RHOFORM MONOTONE cannot fit this "
                                        "table: the tabulated density rises") &&
                            Catch::Contains("at r = 3.") &&
                            Catch::Contains("RHOFORM NONNEG or SIGNED fits "
                                            "it.") &&
                            Catch::StartsWith("tadah refit: RHOFORM "
                                              "MONOTONE"));
    EAMRefit nonneg(eam, coarse_config("Blip", RhoForm::NonNeg));
    REQUIRE_NOTHROW(nonneg.fit());
    const tadah::core::Context pot = nonneg.build_pot_context();
    for (std::size_t n = 0; n < pot.size("AMPGRIDMB"); ++n)
      REQUIRE(pot.get<double>("AMPGRIDMB", n) >= 0.0);
  }
  SECTION("MONOTONE: the knot family and an increasing cutoff are refused") {
    EAMSetflIO eam;
    eam.read_setfl("tests_data/eam.alloy");
    eam.build_splines();
    REQUIRE_THROWS_AS(EAMRefit(eam, coarse_config("Knot5", RhoForm::Monotone)),
                      DensityFormError);
    // The refusal says what fits the knot family, without an instruction
    // (before the fix: "...: use RHOFORM NONNEG: ...").
    REQUIRE_THROWS_WITH(
        EAMRefit(eam, coarse_config("Knot3", RhoForm::Monotone)),
        "EAMRefit: RHOFORM MONOTONE is not defined for the knot family "
        "(basis Knot3); RHOFORM NONNEG fits it: a non-negative knot density "
        "is already non-increasing with a non-increasing cutoff.");
    EAMRefitConfig sininv = coarse_config("Blip5", RhoForm::Monotone);
    sininv.rctype = "Cut_SinInv";
    REQUIRE_THROWS_WITH(EAMRefit(eam, sininv),
                        Catch::Contains("got Cut_SinInv"));
    REQUIRE_NOTHROW(EAMRefit(eam, coarse_config("Knot5", RhoForm::NonNeg)));
    sininv.rho_form = RhoForm::NonNeg;  // only MONOTONE needs the whitelist
    REQUIRE_NOTHROW(EAMRefit(eam, sininv));
  }
  SECTION("positive forms: injected density amplitudes must be >= 0") {
    EAMSetflIO eam;
    eam.read_setfl("tests_data/eam.alloy");
    eam.build_splines();
    EAMRefit nn(eam, coarse_config("Blip5", RhoForm::NonNeg));
    REQUIRE_THROWS_WITH(
        nn.set_blip_fit(EAMRefit::Fn::RHO, {2.4, 3.2}, {1.5, 1.5},
                        {0.4, -0.2}),
        Catch::Contains("RHOFORM NONNEG density amplitudes must be finite "
                        "and >= 0; injected amplitude 2 is -0.2"));
    REQUIRE_NOTHROW(nn.set_blip_fit(EAMRefit::Fn::RHO, {2.4, 3.2}, {1.5, 1.5},
                                    {0.4, 0.0}));
    EAMRefit sg(eam, coarse_config("Blip5", RhoForm::Signed));
    REQUIRE_NOTHROW(sg.set_blip_fit(EAMRefit::Fn::RHO, {2.4, 3.2}, {1.5, 1.5},
                                    {0.4, -0.2}));
  }
  SECTION("LAMBDA is honoured by the non-negative solve") {
    EAMSetflIO eam;
    eam.read_setfl("tests_data/eam.alloy");
    eam.build_splines();
    auto amp_norm = [&](double lambda) {
      EAMRefitConfig cfg = coarse_config("Blip5", RhoForm::Monotone);
      cfg.lambda = lambda;
      EAMRefit refit(eam, cfg);
      refit.fit();
      const tadah::core::Context pot = refit.build_pot_context();
      double s = 0.0;
      for (std::size_t n = 0; n < pot.size("AMPGRIDMB"); ++n) {
        const double a = pot.get<double>("AMPGRIDMB", n);
        REQUIRE(a >= 0.0);
        s += a * a;
      }
      return s;
    };
    CHECK(amp_norm(1e-2) < amp_norm(0.0));
  }
}

namespace {
// The synthetic fcc table of the MLIP refit tests (a0 3.6 A, rcut 5 A, 400
// points): rho(r) = (1 - r/rcut)^2, phi(r) = 3 (1 - r/rcut)^3 plus, with
// `wall`, a steep pair wall 100 exp(-(r - 1.6)/0.05), F(rho) = -2 sqrt(rho)
// + 0.05 rho on [0, 60].
void write_window_setfl(const std::string &path, bool wall) {
  const int nr = 400, nrho = 400;
  const double rcut = 5.0, dr = rcut / (nr - 1);
  const double rhomax = 60.0, drho = rhomax / (nrho - 1);
  std::ofstream os(path);
  os << "# synthetic fcc setfl (fit windows)\n# 2\n# 3\n1 Cu\n"
     << nrho << " " << drho << " " << nr << " " << dr << " " << rcut << "\n"
     << "29 63.55 3.6 fcc\n" << std::setprecision(12);
  for (int i = 0; i < nrho; ++i)
    os << (-2.0 * std::sqrt(i * drho) + 0.05 * i * drho) << "\n";
  for (int i = 0; i < nr; ++i) {
    const double x = 1.0 - i * dr / rcut;
    os << (x > 0 ? x * x : 0.0) << "\n";
  }
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr, x = 1.0 - r / rcut;
    const double phi = (x > 0 ? 3.0 * x * x * x : 0.0) +
                       (wall ? 100.0 * std::exp(-(r - 1.6) / 0.05) : 0.0);
    os << phi * r << "\n";
  }
}
}  // namespace

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit records the fit windows it used",
                 "[refit]") {
  /* Pins: fit_window_r_lo() and fit_window_rho_hi() are 0 before a fit;
   * after fit() they are the windows it fitted on, phi and rho on
   * [fit_window_r_lo(), rcut], F on [0, fit_window_rho_hi()]: under
   * REFIT_PLACEMENT window (the default) on the synthetic fcc table
   * r_lo() (1 A, no wall) and rho_reach(); with a steep pair wall above
   * 30x its reachable scale up to r = 1.67 A, 0.8 r_reach() (1.527 A);
   * with r_lo_frac 0.35, r_lo() = 1.75 A, above 0.8 r_reach() (the samples
   * and the grid start at r_lo, so the window does); under "uniform" r_lo()
   * and the table's rhomax. After finalize_injected(lo, hi), the compact
   * fitter's handoff, they are lo and hi.
   * Before the fix: the accessors do not exist (the test does not
   * compile); the window lived in fit()'s local rwin_lo only, and
   * refit_range_report.dat wrote 0.9 r_reach for it. */
  ScratchFile f("synth_window");
  for (const bool wall : {false, true}) {
    INFO((wall ? "wall" : "no wall"));
    write_window_setfl(f.path, wall);
    EAMSetflIO eam;
    eam.read_setfl(f.path);
    eam.build_splines();
    EAMRefitConfig cfg;
    cfg.n2b = 24; cfg.nrho = 20; cfg.nF = 60;
    EAMRefit refit(eam, cfg);
    CHECK(refit.fit_window_r_lo() == 0.0);
    CHECK(refit.fit_window_rho_hi() == 0.0);
    refit.fit();
    REQUIRE(refit.r_reach() == Approx(0.75 * 3.6 / std::sqrt(2.0)));
    CHECK(refit.fit_window_r_lo() ==
          Approx(wall ? 0.8 * refit.r_reach() : refit.r_lo()));
    CHECK(refit.r_lo() == Approx(1.0));
    CHECK(refit.fit_window_rho_hi() == refit.rho_reach());
    CHECK(refit.rho_reach() < eam.rhomax());
    refit.finalize_injected(1.234, 7.5);
    CHECK(refit.fit_window_r_lo() == 1.234);
    CHECK(refit.fit_window_rho_hi() == 7.5);
  }
  write_window_setfl(f.path, false);
  EAMSetflIO eam;
  eam.read_setfl(f.path);
  eam.build_splines();
  {
    EAMRefitConfig cfg;
    cfg.n2b = 24; cfg.nrho = 20; cfg.nF = 60;
    cfg.r_lo_frac = 0.35;
    EAMRefit refit(eam, cfg);
    refit.fit();
    CHECK(refit.r_lo() == Approx(1.75));
    CHECK(0.8 * refit.r_reach() < refit.r_lo());
    CHECK(refit.fit_window_r_lo() == refit.r_lo());
  }
  {
    EAMRefitConfig cfg;
    cfg.n2b = 24; cfg.nrho = 20; cfg.nF = 60;
    cfg.placement = "uniform";
    EAMRefit refit(eam, cfg);
    refit.fit();
    CHECK(refit.fit_window_r_lo() == refit.r_lo());
    CHECK(refit.fit_window_rho_hi() == eam.rhomax());
  }
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit RHOFORM: the table pre-check reads the fit window "
                 "only, not the table below r_lo",
                 "[refit][rhoform]") {
  /* Pins: fit()'s NONNEG/MONOTONE table pre-check reads the table nodes of
   * the fit window, [fit_window_r_lo(), rcut], never below r_lo (with a
   * wall the window starts above r_lo: the next test case). On the synthetic
   * fcc table (a0 3.6 A, rcut 5 A) with r_lo_frac 0.35, r_lo() = 1.75 A
   * lies above 0.8 r_reach() = 1.527 A (window placement), and the samples
   * and the grid start at r_lo: a density with a narrow bump at 1.64 A,
   * which rises only below r_lo, fits under MONOTONE, and one with a
   * negative dip at 1.64 A fits under NONNEG and MONOTONE, each with
   * fit_window_r_lo() == r_lo() and every amplitude >= 0; a bump at 2.2 A,
   * inside the window, is still refused, the message naming the window
   * "[1.750000, 5.000000] A".
   * Before the fix: the pre-check read the nodes from 0.8 r_reach, so the
   * bump below r_lo was refused ("RHOFORM MONOTONE cannot fit this table:
   * the tabulated density rises from rho = 0.468... at r = 1.591475 A to
   * 0.473... at r = 1.604006 A, inside the fit window [1.527351, 5.000000]
   * A"), and so was the dip under NONNEG and MONOTONE ("... rho =
   * -0.0709... < 0 at r = 1.629069 A, inside the fit window [1.527351,
   * 5.000000] A"); the message for the bump at 2.2 A named the window
   * [1.527351, 5.000000] A. */
  const auto config = [](RhoForm form) {
    EAMRefitConfig cfg = coarse_config("Blip", form);
    cfg.r_lo_frac = 0.35;
    return cfg;
  };
  const auto bump = [](double centre, double height) {
    return [=](double r) {
      const double x = 1.0 - r / 5.0, g = (r - centre) / 0.03;
      return x * x + height * std::exp(-g * g);
    };
  };
  struct Case {
    const char *what;
    std::function<double(double)> rho;
    std::vector<RhoForm> forms;
  };
  const std::vector<Case> fits = {
      {"a rise below r_lo", bump(1.64, 0.05), {RhoForm::Monotone}},
      {"a negative dip below r_lo", bump(1.64, -0.6),
       {RhoForm::NonNeg, RhoForm::Monotone}}};
  for (const Case &cs : fits) {
    const ScratchFile f("tmp_rhoform_below_rlo");
    write_rho_setfl(f.path, cs.rho);
    EAMSetflIO eam;
    eam.read_setfl(f.path);
    eam.build_splines();
    for (RhoForm form : cs.forms) {
      INFO(cs.what << ", RHOFORM " << rhoform_name(form));
      EAMRefit refit(eam, config(form));
      bool fitted = false;
      CHECK_NOTHROW((refit.fit(), fitted = true));
      if (!fitted) continue;
      REQUIRE(refit.r_lo() == Approx(1.75));
      REQUIRE(0.8 * refit.r_reach() < refit.r_lo());
      CHECK(refit.fit_window_r_lo() == refit.r_lo());
      const tadah::core::Context pot = refit.build_pot_context();
      for (std::size_t n = 0; n < pot.size("AMPGRIDMB"); ++n)
        CHECK(pot.get<double>("AMPGRIDMB", n) >= 0.0);
    }
  }
  const ScratchFile f("tmp_rhoform_in_window");
  write_rho_setfl(f.path, bump(2.2, 0.05));
  EAMSetflIO eam;
  eam.read_setfl(f.path);
  eam.build_splines();
  EAMRefit mono(eam, config(RhoForm::Monotone));
  CHECK_THROWS_WITH(
      mono.fit(),
      Catch::Contains("RHOFORM MONOTONE cannot fit this table: the tabulated "
                      "density rises") &&
          Catch::Contains("inside the fit window [1.750000, 5.000000] A"));
}

TEST_CASE_METHOD(RefitFixture,
                 "EAMRefit RHOFORM: with a pair wall the table pre-check reads "
                 "the fit window, which starts above r_lo",
                 "[refit][rhoform]") {
  /* Pins: with the steep pair wall of the window tests (100 exp(-(r -
   * 1.6 A) / 0.05 A) added to phi) on the synthetic fcc table (a0 3.6 A,
   * rcut 5 A, r_lo() = 1 A), fit() fits on the window from
   * fit_window_r_lo() = 0.8 r_reach() = 1.527 A while its samples and grid
   * start at r_lo: the samples in [r_lo, fit_window_r_lo()) are fitted in
   * the tail block of the two-stage solve, and the NONNEG/MONOTONE table
   * pre-check reads the fit window only. A density with a negative dip at
   * 1.25 A, below the window, fits under NONNEG, every amplitude >= 0 and
   * rho_fit(1.25 A) >= 0 (the tail block is solved in the form too); a
   * negative dip at 2.2 A, inside the window, is refused, the message
   * naming the window "[1.527351, 5.000000] A".
   * Before the fix: passes as well (a pin of the window the corrected
   * comments of fit() describe: with a wall the pre-check started at the
   * window's edge, above r_lo, before this branch too). */
  const auto dip = [](double centre) {
    return [=](double r) {
      const double x = 1.0 - r / 5.0, g = (r - centre) / 0.03;
      return x * x - 0.6 * std::exp(-g * g);
    };
  };
  {
    const ScratchFile f("tmp_rhoform_wall_tail");
    write_rho_setfl(f.path, dip(1.25), true);
    EAMSetflIO eam;
    eam.read_setfl(f.path);
    eam.build_splines();
    REQUIRE(eam.rho_tab(1.25) < 0.0);
    EAMRefit refit(eam, coarse_config("Blip", RhoForm::NonNeg));
    bool fitted = false;
    CHECK_NOTHROW((refit.fit(), fitted = true));
    if (fitted) {
      CHECK(refit.r_lo() == Approx(1.0));
      CHECK(refit.fit_window_r_lo() == Approx(0.8 * refit.r_reach()));
      CHECK(refit.fit_window_r_lo() > 1.25);
      const tadah::core::Context pot = refit.build_pot_context();
      for (std::size_t n = 0; n < pot.size("AMPGRIDMB"); ++n)
        CHECK(pot.get<double>("AMPGRIDMB", n) >= 0.0);
      CHECK(refit.rho_fit(1.25) >= 0.0);
    }
  }
  const ScratchFile f("tmp_rhoform_wall_window");
  write_rho_setfl(f.path, dip(2.2), true);
  EAMSetflIO eam;
  eam.read_setfl(f.path);
  eam.build_splines();
  EAMRefit nonneg(eam, coarse_config("Blip", RhoForm::NonNeg));
  CHECK_THROWS_WITH(
      nonneg.fit(),
      Catch::Contains("RHOFORM NONNEG cannot fit this table") &&
          Catch::Contains("inside the fit window [1.527351, 5.000000] A"));
}

namespace {
// A smooth single-element setfl on a compressed bcc lattice (a0 = 1.5 A, so
// 0.8 r_reach = 0.78 A lies below r_lo = 0.2 rcut = 1.0 A), whose density
// table is slightly negative on r in [0.80, 0.90] A: below r_lo, where no
// fit samples the table.
void write_compressed_setfl(const std::string &path) {
  const int nr = 501, nrho = 501;
  const double dr = 0.01, drho = 0.008, rc = 5.0;
  std::ofstream os(path);
  os << std::setprecision(17)
     << "# compressed bcc test table\n# negative density below r_lo\n#\n"
     << "1 Cu\n" << nrho << " " << drho << " " << nr << " " << dr << " "
     << rc << "\n29 63.546 1.5 bcc\n";
  for (int i = 0; i < nrho; ++i) {
    const double q = i * drho;
    os << -std::sqrt(q + 0.25) + 0.5 + 0.05 * q * q << "\n";
  }
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr, x = 1.0 - r / rc;
    const bool dip = r > 0.795 && r < 0.905;
    os << (dip ? -1e-4 : std::exp(-(r - 1.0)) * x * x * x) << "\n";
  }
  for (int i = 0; i < nr; ++i) {
    const double r = i * dr, x = 1.0 - r / rc;
    os << r * (2.0 * std::exp(-2.0 * (r - 1.0)) - 0.5) * x * x * x << "\n";
  }
}
}  // namespace

/* Pins: a component refit (DM_Embed density FORM NONNEG with amplitudes to
 * fit) of a table whose lattice puts 0.8 r_reach below r_lo fits on
 * [r_lo, rcut], as the legacy fit does: fit_window_r_lo() is r_lo, and a
 * negative density node below r_lo, which no fit samples, is not refused.
 * Before the fix the component fit used the unclamped wall-detection edge
 * min(max(r_lo, r_K), 0.8 r_reach) = 0.78 A, sampled the table from there,
 * refused it ("rho = -0.0001 < 0 at r = 0.800000 A, inside the fit window
 * [0.779423, 5.000000] A") and reported that window. */
TEST_CASE_METHOD(RefitFixture,
                 "Component refit fit window is never below r_lo",
                 "[unified-dm][refit][window]") {
  ScratchFile file("compressed_component");
  write_compressed_setfl(file.path);
  EAMSetflIO eam;
  eam.read_setfl(file.path);
  eam.build_splines();
  std::istringstream in(
      "DESCRIPTOR cu DM_Embed\nPAIRS Cu Cu\nDENSITY\nBASIS Blip5\n"
      "FORM NONNEG\nCENTRES 1.2 1.9 2.6 3.3 4.0\nETA 1.4 1.4 1.4 1.4 1.4\n"
      "AMPLITUDES 0 0 0 0 0\nCUTOFF\nTYPE Cut_Poly\nRADIUS 5\nENDCUTOFF\n"
      "ENDDENSITY\nEMBEDDING\nBASIS Gaussian\nCENTRES 0 0.5 1 1.5 2 2.5 3 "
      "3.5 4\nETA 4 4 4 4 4 4 4 4 4\nREFERENCE ZERO\nENDEMBEDDING\n"
      "ENDDESCRIPTOR\n");
  const auto ctx = tadah::core::CoreUserConfigParser().parse(in);
  EAMRefitConfig cfg;
  cfg.component = embed_specs(ctx).at(0);
  cfg.fit_density_amplitudes = true;
  cfg.unified_output = true;
  cfg.basis = "Blip5";
  cfg.rho_form = RhoForm::NonNeg;
  cfg.n2b = 12;
  EAMRefit fit(eam, cfg);
  REQUIRE_NOTHROW(fit.fit());
  REQUIRE(fit.r_lo() == Approx(1.0));
  REQUIRE(0.8 * fit.r_reach() < fit.r_lo());
  REQUIRE(fit.fit_window_r_lo() == fit.r_lo());
}

/* Pins: a component refit (DM_Embed density FORM NONNEG with amplitudes to
 * fit) of a table whose density is negative inside the fit window is
 * refused naming the component's FORM and the remedy in its block, FORM
 * SIGNED: a configuration with DESCRIPTOR blocks refuses the RHOFORM key.
 * Before the fix the refusal named the DM_REAM key ("RHOFORM NONNEG cannot
 * fit this table ... Use RHOFORM SIGNED"), whose advice a component
 * configuration cannot follow.
 * Before the fix that states the remedy: "EAMRefit: the density component
 * CU's FORM NONNEG cannot fit this table: ... Use FORM SIGNED in its
 * DENSITY block.", a class name and an instruction. */
TEST_CASE_METHOD(RefitFixture,
                 "Component refit form refusal names the component FORM",
                 "[unified-dm][refit][rhoform]") {
  const ScratchFile f("tmp_component_form_refusal");
  write_rho_setfl(f.path, [](double r) {
    const double x = 1.0 - r / 5.0, g = (r - 3.0) / 0.03;
    return x * x - 0.6 * std::exp(-g * g);
  });
  EAMSetflIO eam;
  eam.read_setfl(f.path);
  eam.build_splines();
  std::istringstream in(
      "DESCRIPTOR cu DM_Embed\nPAIRS Cu Cu\nDENSITY\nBASIS Blip5\n"
      "FORM NONNEG\nCENTRES 1.2 1.9 2.6 3.3 4.0\nETA 1.4 1.4 1.4 1.4 1.4\n"
      "AMPLITUDES 0 0 0 0 0\nCUTOFF\nTYPE Cut_Poly\nRADIUS 5\nENDCUTOFF\n"
      "ENDDENSITY\nEMBEDDING\nBASIS Gaussian\nCENTRES 0 0.5 1 1.5 2 2.5 3 "
      "3.5 4\nETA 4 4 4 4 4 4 4 4 4\nREFERENCE ZERO\nENDEMBEDDING\n"
      "ENDDESCRIPTOR\n");
  const auto ctx = tadah::core::CoreUserConfigParser().parse(in);
  EAMRefitConfig cfg;
  cfg.component = embed_specs(ctx).at(0);
  cfg.fit_density_amplitudes = true;
  cfg.unified_output = true;
  cfg.basis = "Blip5";
  cfg.rho_form = RhoForm::NonNeg;
  cfg.n2b = 12;
  EAMRefit fit(eam, cfg);
  REQUIRE_THROWS_WITH(
      fit.fit(),
      Catch::Contains("the density component CU's FORM NONNEG cannot fit "
                      "this table") &&
          Catch::Contains("FORM SIGNED in its DENSITY block fits it.") &&
          Catch::StartsWith("tadah refit: the density component CU's FORM") &&
          !Catch::Contains("RHOFORM"));
}
