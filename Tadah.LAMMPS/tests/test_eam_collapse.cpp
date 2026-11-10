#include "catch2/catch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <unistd.h>

#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/eam/embed_edit.h>
#include <tadah/tadah.lammps/eam_collapse.h>
#include <tadah/tadah.lammps/lammps_tadah.h>

using tadah::EamCollapseTables;
using tadah::EamRadialTable;
using tadah::LammpsTadah;
using tadah::TableCertificate;

TEST_CASE("DM_Embed scalar interface preserves EAM density and embedding collapse",
          "[unified-dm][eam_collapse]") {
    tadah::core::PeriodicTable::initialize();
    auto c=tadah::core::CoreUserConfigParser().parse("tests_data/pots/ta_ream_knot5.tadah");
    tadah::models::DC_Selector old(c);
    auto converted=c;tadah::models::replace_ream_with_embed(converted);
    tadah::models::DC_Selector current(converted);
    REQUIRE(current.dmb->eam_form());
    tadah::core::t_type weights(c.size("WEIGHTS"));c.get("WEIGHTS",weights);
    for(int i=0;i<200;++i) {
      double r=0.2+6.0*i/199,rho0,d0,rho1,d1;
      old.dmb->eam_density(73,73,r,rho0,d0);current.dmb->eam_density(73,73,r,rho1,d1);
      REQUIRE(rho1==Approx(rho0).margin(1e-12));REQUIRE(d1==Approx(d0).margin(1e-12));
      old.dmb->eam_embed(weights,r,rho0,d0);current.dmb->eam_embed(weights,r,rho1,d1);
      REQUIRE(rho1==Approx(rho0).margin(1e-10));REQUIRE(d1==Approx(d0).margin(1e-10));
    }
    EamCollapseTables tables;std::string reason;
    REQUIRE(tadah::build_eam_collapse(current,weights,73,tadah::kCollapseNR,tables,reason));
    REQUIRE(tables.valid);REQUIRE(tables.has_mb);
}

namespace {

/* Build a fixture potential exactly the way pair_coeff does.
 * (unique_ptr: LammpsTadah is neither copyable nor movable.) */
std::unique_ptr<LammpsTadah> load_pot(const std::string& path,
                                      const std::string& symbol) {
    tadah::core::PeriodicTable::initialize();
    std::vector<std::string> args = {"*", "*", path, symbol};
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    auto lt = std::make_unique<LammpsTadah>(4, argv.data());
    lt->initialize(4, argv.data());
    return lt;
}

std::unique_ptr<LammpsTadah> make_kr_pure2b() {
    return load_pot("tests_data/pots/kr_pure2b.tadah", "Kr");
}

tadah::core::t_type weights_of(const LammpsTadah& lt) {
    tadah::core::t_type w;
    w.resize(lt.c.size("WEIGHTS"));
    lt.c.get<tadah::core::t_type>("WEIGHTS", w);
    return w;
}

using Analytic = std::function<void(double, double&, double&)>;

/* phi(r) and phi'(r) from the descriptor's SEPARATE value and derivative
 * routines (calc_aed, calc_dXijdri), independent of the calc_all path the
 * collapse samples. */
Analytic independent_phi(const LammpsTadah& lt,
                         const tadah::core::t_type& w) {
    const std::size_t ndesc = lt.S.descriptor_size();
    auto aed = std::make_shared<tadah::core::aed_type>(ndesc);
    auto fd = std::make_shared<tadah::core::fd_type>(ndesc);
    return [&lt, &w, aed, fd, ndesc](double r, double& v, double& d) {
        const int Z = lt.Z[0];
        aed->set_zero();
        fd->set_zero();
        lt.S.d2b->calc_aed(Z, Z, r, r * r, *aed, 1.0);
        lt.S.d2b->calc_dXijdri(Z, Z, r, r * r, *fd, 1.0);
        v = 0.0;
        d = 0.0;
        for (std::size_t c = 0; c < ndesc; ++c) {
            v += w(c) * (*aed)(c);
            d += w(c) * (*fd)(c, 0);
        }
    };
}

Analytic density_of(const LammpsTadah& lt) {
    return [&lt](double r, double& v, double& d) {
        lt.S.dmb->eam_density(lt.Z[0], lt.Z[0], r, v, d);
    };
}

struct DenseCheck {
    double max_rel_val = 0.0;
    double max_rel_der = 0.0;
    double r_val = 0.0;                    // where the worst errors are
    double r_der = 0.0;
};

/* Samples per segment of dense_check(): a multiple of the certificate's 32
 * check intervals, so the dense points include the certificate's own. */
constexpr int kDensePerSegment = 128;
/* Samples per segment holding a `hot` distance (a knot, a junction). */
constexpr int kHotPerSegment = 4096;

/* Independent re-check of a certificate. Every segment s of [r_lo, r_hi]
 * is sampled at t = i / kDensePerSegment (and a segment holding one of the
 * `hot` distances at t = i / kHotPerSegment), and the worst error is
 * scaled by the certificate's unit-free floor
 *   M(s) = max( max|f| on [rc/2, rc], max |f| over segments s' >= s )
 * separately for the value and the derivative. The floor is taken over the
 * table nodes and this check's own samples, a superset of the points the
 * certificate uses, so it is never below the certificate's floor: a dense
 * error above the certificate's max_rel_* is an error the certificate
 * missed. */
DenseCheck dense_check(const EamRadialTable& tab,
                       const TableCertificate& cert, const Analytic& f,
                       const std::vector<double>& hot = {}) {
    const int s_lo = static_cast<int>(std::lround(cert.r_lo / tab.dr));
    const double r_half = 0.5 * tab.rmax;

    double run_v = 0.0, run_d = 0.0;       // max |f|, |f'| on [rc/2, rc]
    for (int k = 1; k < tab.nr; ++k) {
        if (k * tab.dr < r_half) continue;
        run_v = std::max(run_v, std::fabs(tab.v[k]));
        run_d = std::max(run_d, std::fabs(tab.d[k]));
    }

    DenseCheck out;
    std::vector<double> av, ad, tv, td;
    for (int s = tab.nr - 2; s >= s_lo; --s) {
        bool is_hot = false;
        for (double r : hot)
            is_hot = is_hot || static_cast<int>(r / tab.dr) == s;
        const int n = is_hot ? kHotPerSegment : kDensePerSegment;
        av.assign(n, 0.0); ad.assign(n, 0.0);
        tv.assign(n, 0.0); td.assign(n, 0.0);
        run_v = std::max({run_v, std::fabs(tab.v[s]), std::fabs(tab.v[s + 1])});
        run_d = std::max({run_d, std::fabs(tab.d[s]), std::fabs(tab.d[s + 1])});
        for (int i = 1; i < n; ++i) {
            const double r = (s + static_cast<double>(i) / n) * tab.dr;
            f(r, av[i], ad[i]);
            tab.eval(r, tv[i], td[i]);
            run_v = std::max(run_v, std::fabs(av[i]));
            run_d = std::max(run_d, std::fabs(ad[i]));
        }
        for (int i = 1; i < n; ++i) {
            const double r = (s + static_cast<double>(i) / n) * tab.dr;
            const double ev = std::fabs(tv[i] - av[i]) / run_v;
            const double ed = std::fabs(td[i] - ad[i]) / run_d;
            if (ev > out.max_rel_val) { out.max_rel_val = ev; out.r_val = r; }
            if (ed > out.max_rel_der) { out.max_rel_der = ed; out.r_der = r; }
        }
    }
    return out;
}

/* The certificate holds under dense_check(): the errors it states are
 * bounds (and within the tolerance, see require_well_formed()). */
void require_dense_within(const TableCertificate& cert,
                          const DenseCheck& dense) {
    INFO("certificate: value " << cert.max_rel_val << ", derivative "
         << cert.max_rel_der << "; dense: value " << dense.max_rel_val
         << " at r=" << dense.r_val << ", derivative " << dense.max_rel_der
         << " at r=" << dense.r_der);
    CHECK(dense.max_rel_val <= cert.max_rel_val);
    CHECK(dense.max_rel_der <= cert.max_rel_der);
}

/* Worst error of segment s alone, sampled at t = i / kHotPerSegment and
 * scaled by the floor over the certified range above it (from the table
 * nodes and these samples). */
DenseCheck segment_error(const EamRadialTable& tab, int s,
                         const Analytic& f) {
    double m_v = 0.0, m_d = 0.0;
    for (int k = 1; k < tab.nr; ++k) {
        if (k * tab.dr < 0.5 * tab.rmax && k < s) continue;
        m_v = std::max(m_v, std::fabs(tab.v[k]));
        m_d = std::max(m_d, std::fabs(tab.d[k]));
    }
    std::vector<double> ev(kHotPerSegment), ed(kHotPerSegment);
    for (int i = 1; i < kHotPerSegment; ++i) {
        const double r =
            (s + static_cast<double>(i) / kHotPerSegment) * tab.dr;
        double v, d, tv, td;
        f(r, v, d);
        tab.eval(r, tv, td);
        m_v = std::max(m_v, std::fabs(v));
        m_d = std::max(m_d, std::fabs(d));
        ev[i] = std::fabs(tv - v);
        ed[i] = std::fabs(td - d);
    }
    DenseCheck out;
    for (int i = 1; i < kHotPerSegment; ++i) {
        out.max_rel_val = std::max(out.max_rel_val, ev[i] / m_v);
        out.max_rel_der = std::max(out.max_rel_der, ed[i] / m_d);
    }
    return out;
}

bool is_node(double r, double dr) {
    const double k = r / dr;
    return std::fabs(k - std::round(k)) < 1e-9 * std::max(1.0, k);
}

/* Structural checks every certificate must satisfy. */
void require_well_formed(const TableCertificate& cert,
                         const EamRadialTable& tab) {
    INFO("r_lo=" << cert.r_lo << " r_hi=" << cert.r_hi << " n=" << cert.n
         << " max_rel_val=" << cert.max_rel_val
         << " max_rel_der=" << cert.max_rel_der);
    REQUIRE(cert.tol == tadah::kCollapseRelTol);
    REQUIRE(cert.r_hi == tab.rmax);
    REQUIRE(cert.r_lo >= tab.dr * (1.0 - 1e-12));  // [0,dr) is extrapolated
    REQUIRE(cert.r_lo < cert.r_hi);
    REQUIRE(is_node(cert.r_lo, tab.dr));
    REQUIRE(cert.n == static_cast<int>(
        std::lround((cert.r_hi - cert.r_lo) / tab.dr)));
    REQUIRE(cert.max_rel_val <= cert.tol);
    REQUIRE(cert.max_rel_der <= cert.tol);
}

/* The bcc nearest-neighbour distance at a = 3.5 A, the lattice at which the
 * Tadah! 1.6.0 quickstart in.ml ran the D2_LJ potential of
 * ta_lj_cutdummy.tadah (its own bcc lattice is near 2.9 A). */
double lj_fixture_bcc_nn() { return 3.5 * std::sqrt(3.0) / 2.0; }

} // namespace

TEST_CASE("build_pair2b_collapse: node-exact phi for pure-2B D2_Knot5",
          "[eam_collapse][pair2b]") {
    auto lt_ptr = make_kr_pure2b();
    LammpsTadah& lt = *lt_ptr;
    REQUIRE(lt.init2b);
    REQUIRE_FALSE(lt.initmb);

    const auto w = weights_of(lt);
    const int Z = lt.Z[0];
    constexpr int NR = 2048;

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_pair2b_collapse(lt.S, w, Z, NR, T, why));
    REQUIRE(T.valid);
    REQUIRE_FALSE(T.has_mb);
    REQUIRE(T.phi.nr == NR);
    REQUIRE(T.phi.rmax == Approx(lt.S.d2b->get_rcut()));
    REQUIRE(T.rho.nr == 0);           // no density table in the 2b collapse

    /* Node exactness: table nodes must equal w . calc_aed / w . calc_dXijdri
     * bit-for-bit up to summation order (same loop as the builder). */
    const std::size_t ndesc = lt.S.descriptor_size();
    REQUIRE(w.size() == ndesc);
    for (int k = 1; k < NR; k += NR / 37) {
        const double r = k * T.phi.dr;
        tadah::core::aed_type aed(ndesc);
        tadah::core::fd_type  fd(ndesc);
        aed.set_zero();
        lt.S.d2b->calc_aed(Z, Z, r, r * r, aed, 1.0);
        fd.set_zero();
        lt.S.d2b->calc_dXijdri(Z, Z, r, r * r, fd, 1.0);
        double pv = 0.0, pd = 0.0;
        for (std::size_t c = 0; c < ndesc; ++c) {
            pv += w(c) * aed(c);
            pd += w(c) * fd(c, 0);
        }
        REQUIRE(T.phi.v[k] == Approx(pv).margin(1e-14));
        REQUIRE(T.phi.d[k] == Approx(pd).margin(1e-14));
    }

    /* The builder's certificate: a table range in which every segment meets
     * the relative tolerance for value and derivative (was: a max-abs
     * mid-node error sampled on ~64 segments, with no range). */
    require_well_formed(T.phi_cert, T.phi);
    REQUIRE(T.rho_cert.n == 0);       // no density table, no certificate

    /* ... and it holds under an independent dense check. */
    require_dense_within(T.phi_cert, dense_check(T.phi, T.phi_cert,
                                                 independent_phi(lt, w)));

    /* Hermite self-consistency: derivative returned by eval() must match a
     * central difference of the interpolated value (same segment). */
    const double r0 = 0.43 * T.phi.rmax;
    const double h = 1e-6;
    double v_lo, v_hi, v_mid, d_mid, dummy;
    T.phi.eval(r0 - h, v_lo, dummy);
    T.phi.eval(r0 + h, v_hi, dummy);
    T.phi.eval(r0, v_mid, d_mid);
    REQUIRE(d_mid == Approx((v_hi - v_lo) / (2.0 * h)).epsilon(1e-6).margin(1e-8));
}

TEST_CASE("build_eam_collapse refuses a pure-2B model (DM_Dummy is not EAM-form)",
          "[eam_collapse][pair2b]") {
    auto lt_ptr = make_kr_pure2b();
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE_FALSE(tadah::build_eam_collapse(lt.S, w, lt.Z[0], 512, T, why));
    REQUIRE_FALSE(T.valid);
    REQUIRE(why.find("not EAM-form") != std::string::npos);
}

TEST_CASE("build_pair2b_collapse refuses on weight-size mismatch",
          "[eam_collapse][pair2b]") {
    auto lt_ptr = make_kr_pure2b();
    LammpsTadah& lt = *lt_ptr;

    tadah::core::t_type w_bad;
    w_bad.resize(lt.S.descriptor_size() + 3);

    EamCollapseTables T;
    std::string why;
    REQUIRE_FALSE(tadah::build_pair2b_collapse(lt.S, w_bad, lt.Z[0], 512, T, why));
    REQUIRE_FALSE(T.valid);
    REQUIRE(why.find("weight count") != std::string::npos);
}

TEST_CASE("pair collapse: the Ta D2_LJ table is certified to rel tol 1e-9 "
          "above r_lo, and r_lo is the smallest such node",
          "[eam_collapse][pair2b][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_cutdummy.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                         tadah::kCollapseNR, T, why));
    REQUIRE(tadah::kCollapseNR == 8192);
    REQUIRE(tadah::kCollapseRelTol == 1e-9);
    REQUIRE(T.phi.nr == tadah::kCollapseNR);
    REQUIRE(T.phi.rmax == 6.5);

    const TableCertificate& c = T.phi_cert;
    require_well_formed(c, T.phi);

    /* The table serves the lattice the 1.6.0 quickstart ran this potential
     * at: its bcc nearest neighbours (3.03) lie well inside the certified
     * range. */
    REQUIRE(c.r_lo < 0.95 * lj_fixture_bcc_nn());
    REQUIRE(c.max_rel_val > 0.0);
    REQUIRE(c.max_rel_der > 0.0);

    /* Independent dense check above r_lo. */
    const Analytic f = independent_phi(lt, w);
    require_dense_within(c, dense_check(T.phi, c, f));

    /* r_lo is the upper node of the first failing segment: the check of the
     * segment just below it exceeds the tolerance, and its error really is
     * at the tolerance (for a smooth function the check's bound exceeds the
     * error by less than 10 per cent, so r_lo is not set needlessly high). */
    const int s_below = static_cast<int>(std::lround(c.r_lo / T.phi.dr)) - 1;
    REQUIRE(s_below >= 1);
    const auto below = segment_error(T.phi, s_below, f);
    INFO("segment below r_lo: value " << below.max_rel_val
         << ", derivative " << below.max_rel_der);
    REQUIRE(std::max(below.max_rel_val, below.max_rel_der) > 0.9 * c.tol);
    tadah::CollapseScratch scratch;
    scratch.resize(lt.S.descriptor_size());
    const auto phi_fn = [&](double r, double& v, double& d) {
        tadah::phi_analytic(lt.S, T.w, lt.Z[0], lt.Z[0], r, scratch, v, d);
    };
    double floor_v = 0.0, floor_d = 0.0;   // the certificate's M(s_below)
    for (int k = 1; k < T.phi.nr; ++k) {
        if (k * T.phi.dr < 0.5 * T.phi.rmax && k < s_below) continue;
        floor_v = std::max(floor_v, std::fabs(T.phi.v[k]));
        floor_d = std::max(floor_d, std::fabs(T.phi.d[k]));
    }
    for (int s = s_below; s < T.phi.nr - 1; ++s) {
        const tadah::SegmentCheck k = tadah::check_segment(T.phi, s, phi_fn);
        REQUIRE(k.finite);
        floor_v = std::max(floor_v, k.max_v);
        floor_d = std::max(floor_d, k.max_d);
    }
    const tadah::SegmentCheck k_below =
        tadah::check_segment(T.phi, s_below, phi_fn);
    REQUIRE((k_below.err_v > c.tol * floor_v || k_below.err_d > c.tol * floor_d));

    /* The table itself is useless at the r -> 0 head (the old diagnostic's
     * 3e42 came from here): r^-12 changes 4096x across [dr, 2dr]. */
    double tv, td, av, ad;
    const double r_head = 1.5 * T.phi.dr;
    T.phi.eval(r_head, tv, td);
    f(r_head, av, ad);
    REQUIRE(std::fabs(tv - av) > std::fabs(av));
}

TEST_CASE("pair collapse: the load-time report states the certified range, "
          "the tolerance and the analytic clause",
          "[eam_collapse][pair2b][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_cutdummy.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                         tadah::kCollapseNR, T, why));
    const std::string s = tadah::format_collapse_report(T);
    INFO(s);

    const std::regex re(
        R"(^pair-collapse fast path ON \(2b-only, nr=8192\): )"
        R"(table certified for r in \[([^,\]]+), ([^\]]+)\] )"
        R"(\(rel tol 1e-9: energy ([^,]+), force ([^)]+)\); )"
        R"(analytic below r=(\S+)$)");
    std::smatch m;
    REQUIRE(std::regex_match(s, m, re));

    const TableCertificate& c = T.phi_cert;
    const double r_lo_txt = std::stod(m[1].str());
    const double r_hi_txt = std::stod(m[2].str());
    REQUIRE(r_lo_txt == Approx(c.r_lo).epsilon(1e-5));
    REQUIRE(r_hi_txt == Approx(c.r_hi).epsilon(1e-5));
    REQUIRE(std::stod(m[5].str()) == r_lo_txt);        // same boundary twice
    /* achieved errors, one significant digit, never above the tolerance */
    REQUIRE(std::stod(m[3].str()) == Approx(c.max_rel_val).epsilon(0.5));
    REQUIRE(std::stod(m[4].str()) == Approx(c.max_rel_der).epsilon(0.5));
    REQUIRE(std::stod(m[3].str()) <= 1e-9);
    REQUIRE(std::stod(m[4].str()) <= 1e-9);
    /* the old absolute diagnostic without a range is gone */
    REQUIRE(s.find("parity") == std::string::npos);
}

TEST_CASE("collapse_phi: analytic below r_lo, the table from r_lo up",
          "[eam_collapse][pair2b][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_cutdummy.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                         tadah::kCollapseNR, T, why));
    const double r_lo = T.phi_cert.r_lo;
    const double dr = T.phi.dr;
    const Analytic f = independent_phi(lt, w);

    tadah::CollapseScratch scratch;
    scratch.resize(lt.S.descriptor_size());

    for (double r : {0.5 * dr, 1.5 * dr, 0.1, 0.5 * r_lo, r_lo - 0.25 * dr,
                     std::nextafter(r_lo, 0.0)}) {
        CAPTURE(r);
        REQUIRE(r < r_lo);
        double v, d, pv, pd, iv, id;
        tadah::collapse_phi(T, lt.S, r, scratch, v, d);
        tadah::phi_analytic(lt.S, T.w, lt.Z[0], lt.Z[0], r, scratch, pv, pd);
        REQUIRE(v == pv);
        REQUIRE(d == pd);
        f(r, iv, id);
        REQUIRE(v == Approx(iv).epsilon(1e-13));
        REQUIRE(d == Approx(id).epsilon(1e-13));
    }
    for (double r : {r_lo, r_lo + dr / 3.0, 0.5 * (r_lo + T.phi.rmax),
                     T.phi.rmax}) {
        CAPTURE(r);
        double v, d, tv, td;
        tadah::collapse_phi(T, lt.S, r, scratch, v, d);
        T.phi.eval(r, tv, td);
        REQUIRE(v == tv);
        REQUIRE(d == td);
    }

    /* The table nodes come from the same helper as the fallback; the top
     * node holds the limit from below at the cutoff. */
    for (int k : {1, 17, 4096, tadah::kCollapseNR - 1}) {
        double pv, pd;
        const double r = k == tadah::kCollapseNR - 1
            ? std::nextafter(T.phi.rmax, 0.0) : k * dr;
        tadah::phi_analytic(lt.S, T.w, lt.Z[0], lt.Z[0], r, scratch, pv, pd);
        REQUIRE(T.phi.v[k] == pv);
        REQUIRE(T.phi.d[k] == pd);
    }
}

TEST_CASE("pair collapse: a Cut_Cos taper is certified up to the cutoff "
          "through the floor",
          "[eam_collapse][pair2b][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_cutcos.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                         tadah::kCollapseNR, T, why));
    const TableCertificate& c = T.phi_cert;
    require_well_formed(c, T.phi);
    REQUIRE(c.r_lo < 0.95 * lj_fixture_bcc_nn());

    const Analytic f = independent_phi(lt, w);
    require_dense_within(c, dense_check(T.phi, c, f));

    /* phi and phi' vanish at the taper, so a pure pointwise relative error
     * fails in the top segment; the floor keeps it certified. */
    const int s_top = T.phi.nr - 2;
    const double r_mid = (s_top + 0.5) * T.phi.dr;
    double tv, td, av, ad;
    T.phi.eval(r_mid, tv, td);
    f(r_mid, av, ad);
    REQUIRE(std::fabs(av) < 1e-6);
    REQUIRE(std::fabs(tv - av) > c.tol * std::fabs(av));
}

TEST_CASE("EAM collapse: D2_LJ + DM_REAM certifies phi and rho, with "
          "analytic density below the rho range",
          "[eam_collapse][eam][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_ream.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    REQUIRE(lt.initmb);
    REQUIRE(lt.S.dmb->eam_form());
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE(tadah::build_eam_collapse(lt.S, w, lt.Z[0],
                                      tadah::kCollapseNR, T, why));
    REQUIRE(T.valid);
    REQUIRE(T.has_mb);
    REQUIRE(T.rho.rmax == 6.0);

    require_well_formed(T.phi_cert, T.phi);
    require_well_formed(T.rho_cert, T.rho);

    require_dense_within(T.phi_cert, dense_check(T.phi, T.phi_cert,
                                                 independent_phi(lt, w)));
    /* The density Blips (centres 1..4, eta 1) are only C^2 at their knots,
     * the integers 1..5: sample those segments finely. */
    const Analytic rho = density_of(lt);
    require_dense_within(T.rho_cert,
                         dense_check(T.rho, T.rho_cert, rho,
                                     {1.0, 2.0, 3.0, 4.0, 5.0}));

    /* collapse_rho: eam_density below the rho range, the table above. */
    const double r_lo = T.rho_cert.r_lo;
    for (double r : {0.5 * T.rho.dr, 0.5 * r_lo, std::nextafter(r_lo, 0.0)}) {
        CAPTURE(r);
        REQUIRE(r < r_lo);
        double v, d, av, ad;
        tadah::collapse_rho(T, lt.S, r, v, d);
        rho(r, av, ad);
        REQUIRE(v == av);
        REQUIRE(d == ad);
    }
    for (double r : {r_lo, r_lo + 0.3 * T.rho.dr, 0.5 * (r_lo + T.rho.rmax),
                     T.rho.rmax}) {
        CAPTURE(r);
        double v, d, tv, td;
        tadah::collapse_rho(T, lt.S, r, v, d);
        T.rho.eval(r, tv, td);
        REQUIRE(v == tv);
        REQUIRE(d == td);
    }

    const std::string s = tadah::format_collapse_report(T);
    INFO(s);
    REQUIRE(s.rfind("EAM-collapse fast path ON (nr=8192): ", 0) == 0);
    REQUIRE(s.find("phi table certified for r in [") != std::string::npos);
    REQUIRE(s.find("rho table certified for r in [") != std::string::npos);
    REQUIRE(s.find("rel tol 1e-9") != std::string::npos);
    REQUIRE(s.find("analytic below r=") != std::string::npos);
    REQUIRE(s.find("parity") == std::string::npos);
}

TEST_CASE("certify_table: r_lo is the upper node of the highest failing "
          "segment",
          "[eam_collapse][certificate]") {
    constexpr int NR = 1025;
    const double rmax = 4.0;
    const double tol = tadah::kCollapseRelTol;

    SECTION("a cubic is reproduced exactly: certified down to dr") {
        const Analytic f = [](double r, double& v, double& d) {
            v = 1.0 + r - 0.5 * r * r + 0.25 * r * r * r;
            d = 1.0 - r + 0.75 * r * r;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        const TableCertificate c = tadah::certify_table(tab, f, tol);
        REQUIRE(c.r_lo == tab.dr);
        REQUIRE(c.r_hi == rmax);
        REQUIRE(c.n == NR - 2);
        REQUIRE(c.max_rel_val <= tol);
        REQUIRE(c.max_rel_der <= tol);
    }

    SECTION("a kink stops the scan at its segment") {
        /* |r - r_k|^3: third derivative jumps at r_k (placed at t = 0.4 of
         * segment 700); cubic elsewhere, so every other segment is exact. */
        EamRadialTable probe;
        tadah::sample_table(probe, rmax, NR, [](double, double& v, double& d) {
            v = 0.0; d = 0.0; });
        const double r_k = (700 + 0.4) * probe.dr;
        const Analytic f = [r_k](double r, double& v, double& d) {
            const double x = r - r_k;
            v = 1.0 + std::fabs(x) * x * x;
            d = 3.0 * std::fabs(x) * x;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        const TableCertificate c = tadah::certify_table(tab, f, 1e-10);
        REQUIRE(c.r_lo == 701 * tab.dr);
        REQUIRE(c.n == NR - 1 - 701);
    }

    SECTION("a C^2 knot next to a node is bounded, not missed") {
        /* f = r + (r - r_k)_+^3: the third derivative jumps at r_k, placed
         * at t = 0.95 of segment 700, next to its upper node. f' grows with
         * r, so the floor is f'(rmax) everywhere. The derivative error
         * peaks in a narrow band next to the knot; the check must see it. */
        EamRadialTable probe;
        tadah::sample_table(probe, rmax, NR, [](double, double& v, double& d) {
            v = 0.0; d = 0.0; });
        const int s_k = 700;
        const double r_k = (s_k + 0.95) * probe.dr;
        const Analytic f = [r_k](double r, double& v, double& d) {
            const double x = std::max(r - r_k, 0.0);
            v = r + x * x * x;
            d = 1.0 + 3.0 * x * x;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        double f_top, floor_d;
        f(std::nextafter(rmax, 0.0), f_top, floor_d);
        double true_der = 0.0;                     // dense, floored
        for (int i = 1; i < 20000; ++i) {
            const double r = (s_k + i / 20000.0) * tab.dr;
            double v, d, tv, td;
            f(r, v, d);
            tab.eval(r, tv, td);
            true_der = std::max(true_der, std::fabs(td - d) / floor_d);
        }
        CAPTURE(true_der);

        /* Below the knot segment's true error: the scan stops there. */
        const TableCertificate strict =
            tadah::certify_table(tab, f, true_der / 1.5);
        REQUIRE(strict.r_lo == (s_k + 1) * tab.dr);

        /* Above it: certified down to dr, and the stated error bounds the
         * true one. */
        const TableCertificate loose =
            tadah::certify_table(tab, f, 2.5 * true_der);
        REQUIRE(loose.r_lo == tab.dr);
        REQUIRE(loose.max_rel_der >= true_der);
    }

    SECTION("a non-finite analytic value fails its segment") {
        const double r_nan = 2.0 + 0.3 * rmax / (NR - 1);
        const Analytic f = [r_nan](double r, double& v, double& d) {
            if (r < r_nan) {
                v = std::numeric_limits<double>::quiet_NaN();
                d = v;
                return;
            }
            v = 2.0 - r;
            d = -1.0;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        const TableCertificate c = tadah::certify_table(tab, f, tol);
        const int s_nan = static_cast<int>(r_nan / tab.dr);
        REQUIRE(c.r_lo == (s_nan + 1) * tab.dr);
        REQUIRE(c.r_lo < c.r_hi);
    }

    SECTION("a failing top segment leaves an empty certificate") {
        const double r_k = rmax - 0.5 * rmax / (NR - 1);
        const Analytic f = [r_k](double r, double& v, double& d) {
            const double x = r - r_k;
            v = std::fabs(x) * x * x;
            d = 3.0 * std::fabs(x) * x;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        const TableCertificate c = tadah::certify_table(tab, f, tol);
        REQUIRE(c.r_lo == c.r_hi);
        REQUIRE(c.n == 0);
        REQUIRE(c.empty());
    }
}

TEST_CASE("build_pair2b_collapse refuses a table whose top segment misses "
          "the tolerance",
          "[eam_collapse][pair2b][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_blip_uncertifiable.tadah", "Ta");
    LammpsTadah& lt = *lt_ptr;
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE_FALSE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                               tadah::kCollapseNR, T, why));
    REQUIRE_FALSE(T.valid);
    INFO(why);
    /* The certified range is one interval ending at the cutoff, so the
     * top segment decides; the segments below it may well pass. */
    REQUIRE(why == "phi table misses rel tol 1e-9 already in its top "
                   "segment, just below the cutoff r=6.5");
    REQUIRE(T.phi_cert.empty());
    const Analytic f = independent_phi(lt, w);
    /* the bump covers the top two segments; the one below them is exact */
    const auto below = segment_error(T.phi, T.phi.nr - 4, f);
    REQUIRE(std::max(below.max_rel_val, below.max_rel_der) <= T.phi_cert.tol);
}

TEST_CASE("build_eam_collapse refuses a model whose rho table misses the "
          "tolerance in its top segment",
          "[eam_collapse][eam][certificate]") {
    auto lt_ptr = load_pot("tests_data/pots/ta_lj_ream_rho_uncertifiable.tadah",
                           "Ta");
    LammpsTadah& lt = *lt_ptr;
    REQUIRE(lt.initmb);
    REQUIRE(lt.S.dmb->eam_form());
    const auto w = weights_of(lt);

    EamCollapseTables T;
    std::string why;
    REQUIRE_FALSE(tadah::build_eam_collapse(lt.S, w, lt.Z[0],
                                            tadah::kCollapseNR, T, why));
    REQUIRE_FALSE(T.valid);
    INFO(why);
    REQUIRE(why == "rho table misses rel tol 1e-9 already in its top "
                   "segment, just below the cutoff r=6");
    REQUIRE(T.rho_cert.empty());
    require_well_formed(T.phi_cert, T.phi);   // the phi table was fine
}

TEST_CASE("sample_table: the top node holds the limit from below at the "
          "cutoff",
          "[eam_collapse][pair2b][certificate]") {
    /* Cut_Dummy is 0 AT rc, so a D2 evaluated at r = rc returns 0 while the
     * pair term just below rc is phi(rc-) != 0 (ta_lj_cutdummy: -0.0137).
     * A top node sampled at rc made the top segment interpolate towards 0
     * and gave pairs in [rc - dr, rc) forces of order |phi(rc-)| / dr. The
     * round-up fixture adds the case where k*dr at k = nr-1 overshoots rc. */
    for (const char* pot : {"tests_data/pots/ta_lj_cutdummy.tadah",
                            "tests_data/pots/ta_lj_rc_roundup.tadah"}) {
        CAPTURE(pot);
        auto lt_ptr = load_pot(pot, "Ta");
        LammpsTadah& lt = *lt_ptr;
        const auto w = weights_of(lt);
        const double rc = lt.S.d2b->get_rcut();
        const int nr = tadah::kCollapseNR;

        EamCollapseTables T;
        std::string why;
        REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0], nr, T, why));
        REQUIRE(T.phi.rmax == rc);

        const Analytic f = independent_phi(lt, w);
        double at_rc, at_rc_d, below, below_d;
        f(rc, at_rc, at_rc_d);
        f(std::nextafter(rc, 0.0), below, below_d);
        REQUIRE(at_rc == 0.0);            // Cut_Dummy: the jump at rc
        REQUIRE(below < -1e-3);
        REQUIRE(T.phi.v[nr - 1] == Approx(below).epsilon(1e-14));
        REQUIRE(T.phi.d[nr - 1] == Approx(below_d).epsilon(1e-14));

        /* ... so the top segment is certified and a pair in it matches the
         * pair term. */
        require_well_formed(T.phi_cert, T.phi);
        const double r = rc - 0.3 * T.phi.dr;
        double tv, td, av, ad;
        T.phi.eval(r, tv, td);
        f(r, av, ad);
        REQUIRE(tv == Approx(av).epsilon(1e-12));
        REQUIRE(td == Approx(ad).epsilon(1e-9));
    }
    /* the round-up fixture's premise */
    const double rc_up = 7.999196555374454;
    REQUIRE((tadah::kCollapseNR - 1) * (rc_up / (tadah::kCollapseNR - 1)) > rc_up);
}

TEST_CASE("pair collapse: a C^2 knot or cutoff junction next to a node is "
          "bounded, not missed",
          "[eam_collapse][pair2b][certificate]") {
    /* The error of a cubic Hermite segment that holds a C^2 point peaks in
     * a narrow band next to it; with the point next to a node, three check
     * points per segment understated the force error up to 3.5 times and
     * certified segments 1.8-2.8 times over the tolerance. */
    struct Case {
        const char* pot;
        std::vector<double> hot;        // the C^2 points
    };
    const double eta_inv = 1.0 / 1.6408253205128203;
    const double c = 5.0001672567452085;
    for (const Case& k :
         {Case{"tests_data/pots/ta_lj_polys_junction.tadah",
               {9.00060436239767 - 1.0}},
          Case{"tests_data/pots/ta_blip_knot_near_node.tadah",
               {c - 2 * eta_inv, c - eta_inv, c, c + eta_inv,
                c + 2 * eta_inv}}}) {
        CAPTURE(k.pot);
        auto lt_ptr = load_pot(k.pot, "Ta");
        LammpsTadah& lt = *lt_ptr;
        const auto w = weights_of(lt);

        EamCollapseTables T;
        std::string why;
        REQUIRE(tadah::build_pair2b_collapse(lt.S, w, lt.Z[0],
                                             tadah::kCollapseNR, T, why));
        INFO(tadah::format_collapse_report(T));
        require_well_formed(T.phi_cert, T.phi);
        /* the fixtures' premise: each C^2 point sits next to a node */
        for (double r : k.hot) {
            const double t = r / T.phi.dr - std::floor(r / T.phi.dr);
            REQUIRE(t > 0.94);
        }
        require_dense_within(T.phi_cert,
                             dense_check(T.phi, T.phi_cert,
                                         independent_phi(lt, w), k.hot));
    }
}

TEST_CASE("check_segment bounds the segment error wherever a C^2 knot sits, "
          "and closely for a smooth function",
          "[eam_collapse][certificate]") {
    constexpr int NR = 257;
    const double rmax = 2.0;
    const int s = 100;
    EamRadialTable probe;
    tadah::sample_table(probe, rmax, NR, [](double, double& v, double& d) {
        v = 0.0; d = 0.0; });
    const double dr = probe.dr;

    /* Worst |table - f| and |table' - f'| over segment s, 20000 points. */
    const auto true_error = [&](const EamRadialTable& tab, const Analytic& f,
                                double& ev, double& ed) {
        ev = ed = 0.0;
        for (int i = 1; i < 20000; ++i) {
            const double r = (s + i / 20000.0) * dr;
            double v, d, tv, td;
            f(r, v, d);
            tab.eval(r, tv, td);
            ev = std::max(ev, std::fabs(tv - v));
            ed = std::max(ed, std::fabs(td - d));
        }
    };

    SECTION("a knot anywhere in the segment, alone or with a smooth part") {
        for (double lam : {0.0, 1e-3, -1e-3, 1.0, -1.0}) {
            for (int j = 0; j <= 200; ++j) {
                /* knot positions across the segment, both ends included */
                const double tk = 0.0005 + 0.999 * j / 200.0;
                const double r_k = (s + tk) * dr;
                const double r_0 = s * dr;
                const Analytic f = [r_k, r_0, lam](double r, double& v,
                                                   double& d) {
                    const double x = std::max(r - r_k, 0.0);
                    const double y = (r - r_0) / 0.0078125;   // 1 / dr
                    v = x * x * x + lam * 1e-6 * y * y * y * y;
                    d = 3.0 * x * x + lam * 4e-6 * y * y * y / 0.0078125;
                };
                EamRadialTable tab;
                tadah::sample_table(tab, rmax, NR, f);
                const tadah::SegmentCheck k = tadah::check_segment(tab, s, f);
                double ev, ed;
                true_error(tab, f, ev, ed);
                CAPTURE(lam, tk, ev, ed, k.err_v, k.err_d);
                REQUIRE(k.finite);
                REQUIRE(ev <= k.err_v);
                REQUIRE(ed <= k.err_d);
                REQUIRE(k.err_d <= 2.1 * ed);   // conservative, not blind
            }
        }
    }

    SECTION("a knot next to a part that is steep on the scale of a segment") {
        /* f = x^n + lam dr^n exp(-k (r - s dr) / dr), n = 2 (C^1) or 3
         * (C^2): the exponential falls by e^-k across the segment. Its
         * error is strongly curved between check points, and the linear
         * extrapolation alone falls short of the peak next to the knot
         * (by up to 1.6x for k = 20); the curvature allowance covers it.
         * (The bound assumes f smooth on the scale of the check spacing,
         * dr / 32, so k stays well below 32.) */
        const double r_0 = s * dr;
        for (int n : {2, 3}) {
            for (double k : {10.0, 20.0}) {
                for (double lam : {1e-4, 1e-3, 1e-2}) {
                    for (int j = 0; j <= 200; ++j) {
                        const double tk = 0.0005 + 0.999 * j / 200.0;
                        const double r_k = (s + tk) * dr;
                        const double amp = lam * std::pow(dr, n);
                        const Analytic f = [=](double r, double& v,
                                               double& d) {
                            const double x = std::max(r - r_k, 0.0);
                            const double y = std::max((r - r_0) / dr, -1.0);
                            const double e = amp * std::exp(-k * y);
                            v = std::pow(x, n) + e;
                            d = n * std::pow(x, n - 1) - k / dr * e;
                        };
                        EamRadialTable tab;
                        tadah::sample_table(tab, rmax, NR, f);
                        const tadah::SegmentCheck c =
                            tadah::check_segment(tab, s, f);
                        double ev, ed;
                        true_error(tab, f, ev, ed);
                        CAPTURE(n, k, lam, tk, ev, ed, c.err_v, c.err_d);
                        REQUIRE(c.finite);
                        REQUIRE(ev <= c.err_v);
                        REQUIRE(ed <= c.err_d);
                    }
                }
            }
        }
    }

    SECTION("a smooth function: the bound is within 10 per cent") {
        const Analytic f = [](double r, double& v, double& d) {
            v = std::exp(-3.0 * r);
            d = -3.0 * v;
        };
        EamRadialTable tab;
        tadah::sample_table(tab, rmax, NR, f);
        const tadah::SegmentCheck k = tadah::check_segment(tab, s, f);
        double ev, ed;
        true_error(tab, f, ev, ed);
        CAPTURE(ev, ed, k.err_v, k.err_d);
        REQUIRE(ev <= k.err_v);
        REQUIRE(ed <= k.err_d);
        REQUIRE(k.err_v <= 1.1 * ev);
        REQUIRE(k.err_d <= 1.1 * ed);
    }
}

/* ---------------------------------------------------------------------- */
/* DM_REAM (EAM-form many-body block) and the EMBF0 embedding reference.   */

namespace {

/* Load a pot the way pair_coeff does; with `free` the same file is loaded
 * with an extra `EMBF0 FREE` line (written to a temporary copy). */
std::unique_ptr<LammpsTadah> load_ream_pot(const std::string& file, bool free) {
    tadah::core::PeriodicTable::initialize();
    std::string path = file;
    if (free) {
        path = file + ".free.tmp";
        std::ifstream in(file);
        std::ofstream out(path);
        out << in.rdbuf() << "\nEMBF0 FREE\n";
    }
    const char* argv[] = {"*", "*", path.c_str(), "Ta"};
    auto lt = std::make_unique<LammpsTadah>(4, const_cast<char**>(argv));
    lt->initialize(4, const_cast<char**>(argv));
    if (free) std::remove(path.c_str());
    return lt;
}

/* w . aed over the DM block, with aed from DM_Base::calc_aed at rho_bar;
 * `abs_sum` (optional) receives sum |w . aed|, the scale of its rounding
 * (this test file is compiled with the preset's fast-math flags, so its
 * reassociated sum need not round like the library's loop). */
double embedding_via_aed(LammpsTadah& lt, const tadah::core::t_type& w,
                         double rho_bar, double* abs_sum = nullptr) {
    const std::size_t ndesc = lt.S.descriptor_size();
    tadah::core::rho_type rho;
    lt.S.dmb->init_rhoi(rho);
    rho(0) = rho_bar;
    tadah::core::aed_type aed(ndesc);
    aed.set_zero();
    lt.S.dmb->calc_aed(rho, aed);
    double F = 0.0, A = 0.0;
    const std::size_t off = ndesc - lt.S.dmb->size();
    for (std::size_t c = off; c < ndesc; ++c) {
        F += w(c) * aed(c);
        A += std::fabs(w(c) * aed(c));
    }
    if (abs_sum) *abs_sum = A;
    return F;
}

} // namespace

TEST_CASE("DM_REAM collapse: eam_embed equals w.calc_aed, EMBF0 ZERO gives F(0)=0",
          "[eam_collapse][ream][embf0]") {
    for (const std::string file : {"tests_data/pots/ta_ream_blip5.tadah",
                                   "tests_data/pots/ta_ream_knot5.tadah"}) {
        INFO(file);
        auto z_ptr = load_ream_pot(file, false);
        auto f_ptr = load_ream_pot(file, true);
        LammpsTadah& lz = *z_ptr;
        LammpsTadah& lf = *f_ptr;
        REQUIRE(lz.initmb);
        REQUIRE(lz.S.dmb->eam_form());
        REQUIRE(lz.S.dmb->embf0_mode() == tadah::models::EmbF0Mode::Zero);
        REQUIRE(lf.S.dmb->embf0_mode() == tadah::models::EmbF0Mode::Free);

        const auto w = weights_of(lz);
        REQUIRE(w.size() == lz.S.descriptor_size());

        EamCollapseTables T;
        std::string why;
        REQUIRE(tadah::build_eam_collapse(lz.S, w, lz.Z[0], 1024, T, why));
        REQUIRE(T.valid);
        REQUIRE(T.has_mb);

        const double raw_f0 = *lf.S.dmb->raw_embedding_f0(w);
        REQUIRE(raw_f0 != 0.0);   // the fixture is sensitive to the reference
        // Rounding floor of sums cancelling down from S0 = sum |w g(0)|
        // (every embedding family has g >= 0, so S0 = raw F(0) of |w|).
        tadah::core::t_type wabs = w;
        for (std::size_t i = 0; i < wabs.size(); ++i) wabs(i) = std::fabs(w(i));
        const double s0 = *lf.S.dmb->raw_embedding_f0(wabs);
        const double eps = std::numeric_limits<double>::epsilon();

        for (const double rb : {0.0, 1e-9, 0.05, 0.3, 0.9, 1.7, 3.1, 7.5}) {
            INFO("rho_bar = " << rb);
            double Fz, Fpz, Ff, Fpf;
            lz.S.dmb->eam_embed(w, rb, Fz, Fpz);
            lf.S.dmb->eam_embed(w, rb, Ff, Fpf);
            // F' is identical in both references (forces unchanged).
            REQUIRE(Fpz == Fpf);
            // The EAMFAST embedding equals the generic w . aed (both modes).
            double az = 0.0, af = 0.0;
            const double gz = embedding_via_aed(lz, w, rb, &az);
            const double gf = embedding_via_aed(lf, w, rb, &af);
            const double tol = 128.0 * eps * (s0 + az + af) + 1e-12;
            REQUIRE(std::fabs(Fz - gz) <= tol);
            REQUIRE(std::fabs(Ff - gf) <= tol);
            // ZERO is FREE shifted by the raw F(0).
            REQUIRE(std::fabs(Fz - (Ff - raw_f0)) <= tol);
        }
        // Exactly zero at rho = 0 under ZERO, on both evaluation paths.
        double F0, Fp0;
        lz.S.dmb->eam_embed(w, 0.0, F0, Fp0);
        REQUIRE(F0 == 0.0);
        REQUIRE(embedding_via_aed(lz, w, 0.0) == 0.0);
        // FREE: the isolated-atom embedding energy is the raw F(0).
        lf.S.dmb->eam_embed(w, 0.0, F0, Fp0);
        REQUIRE(std::fabs(F0 - raw_f0) <= 128.0 * eps * s0);
    }
}

/* ---------------------------------------------------------------------- */
/* DM_REAM density forms (RHOFORM) in the EAM collapse.                    */

namespace {

/* A scratch potential file, removed with the object however the test case
 * ends (a failed REQUIRE throws past a trailing std::remove). */
struct TempPot {
    std::string path;
    explicit TempPot(std::string p) : path(std::move(p)) {}
    TempPot(const TempPot&) = delete;
    TempPot& operator=(const TempPot&) = delete;
    ~TempPot() { std::remove(path.c_str()); }
};

/* Copy of @p file with its RHOFORM line replaced by "RHOFORM <form>" (form
 * empty: the line is dropped), written next to it under @p tag and the
 * process id (ctest runs the cases as concurrent processes). */
std::unique_ptr<TempPot> pot_with_rhoform(const std::string& file,
                                          const std::string& form,
                                          const std::string& tag) {
    std::ifstream in(file);
    REQUIRE(in.good());
    auto pot = std::make_unique<TempPot>(file + "." + tag + "." +
                                         std::to_string(::getpid()) + ".tmp");
    const std::string& path = pot->path;
    std::ofstream out(path);
    std::string line;
    int replaced = 0;
    while (std::getline(in, line)) {
        if (line.rfind("RHOFORM", 0) == 0) {
            ++replaced;
            if (!form.empty()) out << "RHOFORM           " << form << "\n";
            continue;
        }
        out << line << "\n";
    }
    REQUIRE(replaced == 1);
    return pot;
}

std::unique_ptr<LammpsTadah> load_path(const std::string& path) {
    tadah::core::PeriodicTable::initialize();
    const char* argv[] = {"*", "*", path.c_str(), "Ta"};
    auto lt = std::make_unique<LammpsTadah>(4, const_cast<char**>(argv));
    lt->initialize(4, const_cast<char**>(argv));
    return lt;
}

std::unique_ptr<LammpsTadah> load_with_rhoform(const std::string& file,
                                               const std::string& form) {
    const auto pot = pot_with_rhoform(file, form, form);
    return load_path(pot->path);
}

tadah::models::RhoForm density_form(const LammpsTadah& lt) {
    const auto* ream =
        dynamic_cast<const tadah::models::DM_REAM*>(lt.S.dmb);
    REQUIRE(ream != nullptr);
    return ream->density().form;
}

} // namespace

TEST_CASE("DM_REAM collapse under RHOFORM MONOTONE: the rho table follows "
          "eam_density (relative tolerance)",
          "[eam_collapse][ream][rhoform]") {
    /* The Blip5 fixture has every density amplitude > 0, SGRIDMB > 0,
     * WATOM 1 and Cut_Dummy, so it is also a valid MONOTONE potential:
     * rho = sum_n a_n S_n(r), S_n = 1 inside the bump's inner support edge
     * (c_n - 3/s_n), 0 beyond its outer edge (c_n + 3/s_n). This file is
     * compiled with the preset's fast-math flags, so every comparison with
     * the library (built without them) is relative, never bitwise. */
    const std::string file = "tests_data/pots/ta_ream_blip5.tadah";
    auto m_ptr = load_with_rhoform(file, "MONOTONE");
    auto s_ptr = load_path(file);   // RHOFORM SIGNED, as written
    LammpsTadah& lm = *m_ptr;
    LammpsTadah& ls = *s_ptr;
    REQUIRE(tadah::models::stored_rhoform(lm.c) ==
            tadah::models::RhoForm::Monotone);
    REQUIRE(density_form(lm) == tadah::models::RhoForm::Monotone);
    REQUIRE(density_form(ls) == tadah::models::RhoForm::Signed);

    const auto w = weights_of(lm);
    constexpr int NR = tadah::kCollapseNR;   // the table size pair_tadah uses
    EamCollapseTables T, TS;
    std::string why;
    REQUIRE(tadah::build_eam_collapse(lm.S, w, lm.Z[0], NR, T, why));
    REQUIRE(tadah::build_eam_collapse(ls.S, w, ls.Z[0], NR, TS, why));
    REQUIRE(T.valid);
    REQUIRE(T.has_mb);
    const int Z = lm.Z[0];

    double rho_max = 0.0, rhop_max = 0.0;
    for (int k = 1; k < NR; ++k) {
        rho_max = std::max(rho_max, std::fabs(T.rho.v[k]));
        rhop_max = std::max(rhop_max, std::fabs(T.rho.d[k]));
    }
    REQUIRE(rho_max > 0.0);
    REQUIRE(rhop_max > 0.0);
    const double eps = std::numeric_limits<double>::epsilon();

    SECTION("nodes are eam_density; rho >= 0 and non-increasing") {
        for (int k = 1; k < NR; ++k) {
            // the top node holds the limit from below at the cutoff
            const double r = k == NR - 1 ? std::nextafter(T.rho.rmax, 0.0)
                                         : k * T.rho.dr;
            double v, d;
            lm.S.dmb->eam_density(Z, Z, r, v, d);
            REQUIRE(std::fabs(T.rho.v[k] - v) <= 4.0 * eps * rho_max);
            REQUIRE(std::fabs(T.rho.d[k] - d) <= 4.0 * eps * rhop_max);
            // library values: the form's guarantees, to rounding
            REQUIRE(T.rho.v[k] >= 0.0);
            REQUIRE(T.rho.d[k] <= 16.0 * eps * rhop_max);
            if (k + 1 < NR)
                REQUIRE(T.rho.v[k + 1] - T.rho.v[k] <= 16.0 * eps * rho_max);
        }
    }

    SECTION("off-node interpolation: value and derivative, relative") {
        /* The table is certified over its whole range [dr, rc] (the step
         * sum is smooth), so the fast kernels never fall back to the
         * analytic density here, and the certificate's bounds hold under
         * the independent dense re-check. Blip5 is C^4 at c_n + j/s_n,
         * j = -3..3, and S_n one order smoother: sample those segments
         * finely. */
        require_well_formed(T.rho_cert, T.rho);
        REQUIRE(T.rho_cert.r_lo == Approx(T.rho.dr).epsilon(1e-12));
        std::vector<double> knots;
        for (const double c : {2.8, 3.6, 4.4})
            for (int j = -3; j <= 3; ++j) knots.push_back(c + j / 3.0);
        require_dense_within(T.rho_cert, dense_check(T.rho, T.rho_cert,
                                                     density_of(lm), knots));
        // mid-node parity on every nr/64-th segment, as the pre-certificate
        // parity_err_rho self-check measured it
        double parity = 0.0;
        for (int k = 1; k < NR - 1; k += NR / 64) {
            const double r = (k + 0.5) * T.rho.dr;
            double av, ad;
            lm.S.dmb->eam_density(Z, Z, r, av, ad);
            parity = std::max(parity, std::fabs(T.rho.eval_val(r) - av));
        }
        REQUIRE(parity <= 1e-10 * rho_max);
        // 4001 points strictly inside (0, rcut), none on a node
        const double rmax = T.rho.rmax;
        double worst_v = 0.0, worst_d = 0.0;
        for (int i = 1; i <= 4001; ++i) {
            const double r = rmax * (i - 0.37) / 4002.0;
            double tv, td, av, ad;
            T.rho.eval(r, tv, td);
            lm.S.dmb->eam_density(Z, Z, r, av, ad);
            worst_v = std::max(worst_v, std::fabs(tv - av));
            worst_d = std::max(worst_d, std::fabs(td - ad));
        }
        INFO("worst |table - eam_density| = " << worst_v << " (rho_max "
             << rho_max << "), worst |d| = " << worst_d << " (rhop_max "
             << rhop_max << ")");
        REQUIRE(worst_v <= 1e-10 * rho_max);
        REQUIRE(worst_d <= 1e-6 * rhop_max);
    }

    SECTION("the step form, not the bump form, is tabulated") {
        // Inside every inner support edge (1.8 A): S_n = 1, rho = sum a_n.
        // Beyond every outer edge (5.4 A) and below RCUTMB: rho = 0; the
        // Hermite segments there interpolate zeros (undershoot ~1e-22).
        const double sum_a = 0.5 + 0.3 + 0.1;
        for (const double r : {0.3, 0.9, 1.5, 1.75}) {
            INFO("r = " << r);
            REQUIRE(T.rho.eval_val(r) == Approx(sum_a).epsilon(1e-12));
            // SIGNED: every bump is 0 there
            REQUIRE(std::fabs(TS.rho.eval_val(r)) <= 1e-12 * rho_max);
        }
        for (const double r : {5.41, 5.45, 5.49}) {
            INFO("r = " << r);
            REQUIRE(std::fabs(T.rho.eval_val(r)) <= 1e-12 * rho_max);
        }
        // The pair function and the embedding do not depend on the form.
        for (int k = 1; k < NR; k += 97) {
            REQUIRE(T.phi.v[k] == Approx(TS.phi.v[k]).epsilon(4.0 * eps)
                                      .margin(4.0 * eps));
        }
        for (const double rb : {0.0, 0.4, 0.9, 2.5}) {
            double Fm, Fpm, Fs, Fps;
            lm.S.dmb->eam_embed(w, rb, Fm, Fpm);
            ls.S.dmb->eam_embed(w, rb, Fs, Fps);
            REQUIRE(Fm == Approx(Fs).epsilon(4.0 * eps).margin(4.0 * eps));
            REQUIRE(Fpm == Approx(Fps).epsilon(4.0 * eps).margin(4.0 * eps));
        }
    }

    SECTION("NONNEG tabulates the SIGNED density of the same amplitudes") {
        // Every amplitude >= 0 and Cut_Dummy >= 0: the NONNEG density is the
        // SIGNED one (bit for bit in the library; relative here).
        auto n_ptr = load_with_rhoform(file, "NONNEG");
        REQUIRE(density_form(*n_ptr) == tadah::models::RhoForm::NonNeg);
        EamCollapseTables TN;
        REQUIRE(tadah::build_eam_collapse(n_ptr->S, w, Z, NR, TN, why));
        double smax = 0.0;
        for (int k = 1; k < NR; ++k)
            smax = std::max(smax, std::fabs(TS.rho.v[k]));
        for (int k = 1; k < NR; ++k)
            REQUIRE(std::fabs(TN.rho.v[k] - TS.rho.v[k]) <= 4.0 * eps * smax);
    }
}

TEST_CASE("DM_REAM potential without RHOFORM: the load fails with the "
          "migration text",
          "[ream][rhoform]") {
    /* PairTadah::load keeps e.what() of exactly these exceptions and
     * pair_coeff puts it in its error->all message (checked through LAMMPS
     * in the HPO test_ream_fastpath). */
    tadah::core::PeriodicTable::initialize();
    const std::string file = "tests_data/pots/ta_ream_knot5.tadah";

    SECTION("pair_coeff * * <file> (the CLI constructor)") {
        const auto pot = pot_with_rhoform(file, "", "absent");
        const std::string& path = pot->path;
        const char* argv[] = {"*", "*", path.c_str(), "Ta"};
        REQUIRE_THROWS_AS(LammpsTadah(4, const_cast<char**>(argv)),
                          tadah::models::DensityFormError);
        REQUIRE_THROWS_WITH(LammpsTadah(4, const_cast<char**>(argv)),
                            Catch::Contains(
                                tadah::models::rhoform_absent_message()) &&
                            Catch::Contains("which a RHOFORM SIGNED line "
                                            "declares for an existing "
                                            "potential"));
    }
    SECTION("a driver-built potential (the Context constructor)") {
        const auto pot = pot_with_rhoform(file, "", "absent_ctx");
        tadah::core::Context ctx =
            tadah::core::CoreUserConfigParser().parse(pot->path);
        REQUIRE_FALSE(ctx.has_stored("RHOFORM"));
        REQUIRE_THROWS_WITH(LammpsTadah(ctx),
                            Catch::Contains("DM_REAM: RHOFORM is required"));
        REQUIRE_FALSE(ctx.has_stored("RHOFORM"));   // nothing materialised
    }
    SECTION("an invalid form in the file: an error at its line") {
        const auto pot = pot_with_rhoform(file, "SQUARE", "square");
        const std::string& path = pot->path;
        const char* argv[] = {"*", "*", path.c_str(), "Ta"};
        REQUIRE_THROWS_WITH(LammpsTadah(4, const_cast<char**>(argv)),
                            Catch::Contains("Invalid value for key RHOFORM "
                                            "on line ") &&
                            Catch::Contains(" of " + path + ": 'SQUARE' "
                                            "(allowed: SIGNED, NONNEG, "
                                            "MONOTONE)"));
    }
    SECTION("an invalid form in a driver-built potential") {
        const auto pot = pot_with_rhoform(file, "SIGNED", "square_ctx");
        tadah::core::Context ctx =
            tadah::core::CoreUserConfigParser().parse(pot->path);
        ctx.remove("RHOFORM");
        ctx.add("RHOFORM", std::string("SQUARE"));
        REQUIRE_THROWS_WITH(LammpsTadah(ctx),
                            Catch::Contains("RHOFORM 'SQUARE' is not a "
                                            "density form"));
    }
    SECTION("the fixture as written (RHOFORM SIGNED) loads") {
        auto lt = load_path(file);
        REQUIRE(density_form(*lt) == tadah::models::RhoForm::Signed);
    }
}

TEST_CASE("LammpsTadah::initialize: an element the potential does not have "
          "throws with its symbol",
          "[pair_coeff]") {
    /* pair_coeff * * <file> W on a Ta potential. pair_tadah's coeff() turns
     * this exception into a LAMMPS error->all carrying the text, so it
     * reaches log.lammps (checked through LAMMPS in the HPO test
     * test_ream_fastpath: this library has no LAMMPS instance). */
    tadah::core::PeriodicTable::initialize();
    const std::string path = "tests_data/pots/ta_ream_knot5.tadah";
    const char* argv[] = {"*", "*", path.c_str(), "W"};
    LammpsTadah lt(4, const_cast<char**>(argv));
    REQUIRE_THROWS_AS(lt.initialize(4, const_cast<char**>(argv)),
                      std::runtime_error);
    REQUIRE_THROWS_WITH(lt.initialize(4, const_cast<char**>(argv)),
                        Catch::Contains("Symbol not in potential file: W"));
    // NULL skips a LAMMPS type, the potential's own element maps
    const char* ok[] = {"*", "*", path.c_str(), "Ta", "NULL"};
    REQUIRE_NOTHROW(lt.initialize(5, const_cast<char**>(ok)));
}
