// test_d2_blip_dtheta.cpp
//
// Layer-1 parity tests for the closed-form analytical derivatives of
// D2_Blip with respect to its hyperparameters. Mirrors
// test_d2_bp_dtheta.cpp: for each (theta_name, arg_pos) the analytical
// dD/d theta is compared against a central-difference reference computed
// by rebuilding the descriptor with a perturbed Context.
//
// Test points are chosen to exercise both blip pieces:
//   - the interior cubic region |x_c| < 1
//   - the outer cubic region 1 < |x_c| < 2
// staying clear of |x_c| in { 1, 2 } where B is only C^1 / where the FD
// step could straddle the support boundary.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_blip.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_Blip;

struct TestFixture {
    TestFixture() { PeriodicTable::initialize(); }
};

// Build a Context populated with a fresh CGRID2B/SGRID2B/RCUT2B and the
// boilerplate D2_Blip requires. Mirrors make_bp_context in
// test_d2_bp_dtheta.cpp but with TYPE2B label "D2_Blip".
Context make_blip_context(const std::vector<double>& cgrid,
                          const std::vector<double>& sgrid,
                          double rcut) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sgrid.size(); ++i) {
            if (i) ss << " ";
            ss << sgrid[i];
        }
        c.add("SGRID2B", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cgrid.size(); ++i) {
            if (i) ss << " ";
            ss << cgrid[i];
        }
        c.add("CGRID2B", ss.str());
    }
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_Blip");
    c.add("TYPE2B", 2);
    c.add("TYPE2B", 2);
    c.add("TYPE2B", "H");
    c.add("TYPE2B", "H");
    return c;
}

aed_type eval_blip_aed(const std::vector<double>& cgrid,
                       const std::vector<double>& sgrid,
                       double rcut,
                       int Zi, int Zj, double rij) {
    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

// Force-descriptor radial column (column 0 of fd_ij). FD-perturbation
// reference for calc_dXijdri_dtheta parity.
std::vector<double>
eval_blip_fd_radial(const std::vector<double>& cgrid,
                    const std::vector<double>& sgrid,
                    double rcut,
                    int Zi, int Zj, double rij) {
    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd_ij(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd_ij);
    std::vector<double> out(d.size());
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) out[c_] = fd_ij(c_, 0);
    return out;
}

}  // namespace

// Component 0 sits in the outer cubic region; component 1 sits in the
// interior. Probing rij = 1.2 with cgrid={0.5,1.4} sgrid={2.0,1.0}:
//   x_c[0] = 2.0 * (1.2 - 0.5) = +1.4   (outer cubic, |x_c| in (1,2))
//   x_c[1] = 1.0 * (1.2 - 1.4) = -0.2   (interior cubic, |x_c| < 1)

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical dD/d CGRID2B matches central difference",
          "[MODELS][d2_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    for (std::size_t k = 0; k < cgrid.size(); ++k) {
        aed_type dD(d.size());
        dD.set_zero();
        const bool ok = d.calc_dDdtheta("CGRID2B", static_cast<int>(k + 1),
                                        Zi, Zj, rij, rij * rij, dD);
        REQUIRE(ok);

        const double h = 1e-5;
        auto cp = cgrid;  cp[k] += h;
        auto cm = cgrid;  cm[k] -= h;
        aed_type aed_p = eval_blip_aed(cp, sgrid, rcut, Zi, Zj, rij);
        aed_type aed_m = eval_blip_aed(cm, sgrid, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
            INFO("CGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dD[c_] << " FD=" << fd);
            CHECK(std::abs(dD[c_] - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical dD/d SGRID2B matches central difference",
          "[MODELS][d2_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    for (std::size_t k = 0; k < sgrid.size(); ++k) {
        aed_type dD(d.size());
        dD.set_zero();
        const bool ok = d.calc_dDdtheta("SGRID2B", static_cast<int>(k + 1),
                                        Zi, Zj, rij, rij * rij, dD);
        REQUIRE(ok);

        const double h = 1e-5;
        auto sp = sgrid;  sp[k] += h;
        auto sm = sgrid;  sm[k] -= h;
        aed_type aed_p = eval_blip_aed(cgrid, sp, rcut, Zi, Zj, rij);
        aed_type aed_m = eval_blip_aed(cgrid, sm, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
            INFO("SGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dD[c_] << " FD=" << fd);
            CHECK(std::abs(dD[c_] - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical dD/d RCUT2B matches central difference",
          "[MODELS][d2_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    aed_type dD(d.size());
    dD.set_zero();
    const bool ok = d.calc_dDdtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dD);
    REQUIRE(ok);

    const double h = 1e-5;
    aed_type aed_p = eval_blip_aed(cgrid, sgrid, rcut + h, Zi, Zj, rij);
    aed_type aed_m = eval_blip_aed(cgrid, sgrid, rcut - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
        INFO("RCUT2B component=" << c_
             << " analytical=" << dD[c_] << " FD=" << fd);
        CHECK(std::abs(dD[c_] - fd) < 1e-5);
    }
}

// Probe a second (eta, mu, r) configuration so that both grid components
// land in the interior cubic region |x_c| < 1. Catches sign / chain-rule
// errors that the (outer, interior) split in the cases above would mask.
TEST_CASE_METHOD(TestFixture,
          "D2_Blip dtheta parity on interior-only configuration",
          "[MODELS][d2_blip][dtheta][parity]")
{
    // cgrid={1.0, 1.5}, sgrid={1.5, 0.8}, rij=1.2:
    //   x_c[0] = 1.5 * (1.2 - 1.0) = +0.30   (interior)
    //   x_c[1] = 0.8 * (1.2 - 1.5) = -0.24   (interior)
    const std::vector<double> cgrid = {1.0, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    const double h = 1e-5;
    for (const std::string key : {"CGRID2B", "SGRID2B"}) {
        for (std::size_t k = 0; k < cgrid.size(); ++k) {
            aed_type dD(d.size());
            dD.set_zero();
            const bool ok = d.calc_dDdtheta(key, static_cast<int>(k + 1),
                                            Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            auto cp = cgrid, cm = cgrid, sp = sgrid, sm = sgrid;
            if (key == "CGRID2B") { cp[k] += h; cm[k] -= h; }
            else                  { sp[k] += h; sm[k] -= h; }
            aed_type aed_p = eval_blip_aed(cp, sp, rcut, Zi, Zj, rij);
            aed_type aed_m = eval_blip_aed(cm, sm, rcut, Zi, Zj, rij);

            for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
                const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
                INFO(key << "[" << k+1 << "] component=" << c_
                     << " analytical=" << dD[c_] << " FD=" << fd);
                CHECK(std::abs(dD[c_] - fd) < 1e-5);
            }
        }
    }
}

// ----------------------------------------------------------------------
// Force-descriptor θ-derivative parity.
// fd_radial(c) = w(Zj) * (fc * eta_c * b'(x_c) + fcp * B(x_c))
// FD reference: central difference of calc_dXijdri at perturbed θ.
// Probes stay strictly inside |x_c| in (0,1) ∪ (1,2) and at least 0.2
// away from |x_c| = 1 — the C^2 break makes b'' jump there and FD
// truncation explodes within margin h * |b'''|.
// ----------------------------------------------------------------------

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical d(fd)/d CGRID2B matches central difference",
          "[MODELS][d2_blip][dtheta][force][parity]")
{
    // x_c[0]=+1.4 (outer cubic), x_c[1]=-0.2 (interior, 0.8 from |x|=1).
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    for (std::size_t k = 0; k < cgrid.size(); ++k) {
        tadah::core::fd_type dfd(d.size());
        const bool ok = d.calc_dXijdri_dtheta("CGRID2B", static_cast<int>(k + 1),
                                              Zi, Zj, rij, rij * rij, dfd);
        REQUIRE(ok);

        const double h = 1e-5;
        auto cp = cgrid;  cp[k] += h;
        auto cm = cgrid;  cm[k] -= h;
        auto fdp = eval_blip_fd_radial(cp, sgrid, rcut, Zi, Zj, rij);
        auto fdm = eval_blip_fd_radial(cm, sgrid, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
            INFO("CGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dfd(c_, 0) << " FD=" << fd);
            CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical d(fd)/d SGRID2B matches central difference",
          "[MODELS][d2_blip][dtheta][force][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    for (std::size_t k = 0; k < sgrid.size(); ++k) {
        tadah::core::fd_type dfd(d.size());
        const bool ok = d.calc_dXijdri_dtheta("SGRID2B", static_cast<int>(k + 1),
                                              Zi, Zj, rij, rij * rij, dfd);
        REQUIRE(ok);

        const double h = 1e-5;
        auto sp = sgrid;  sp[k] += h;
        auto sm = sgrid;  sm[k] -= h;
        auto fdp = eval_blip_fd_radial(cgrid, sp, rcut, Zi, Zj, rij);
        auto fdm = eval_blip_fd_radial(cgrid, sm, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
            INFO("SGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dfd(c_, 0) << " FD=" << fd);
            CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical d(fd)/d RCUT2B matches central difference",
          "[MODELS][d2_blip][dtheta][force][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    const bool ok = d.calc_dXijdri_dtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dfd);
    REQUIRE(ok);

    const double h = 1e-5;
    auto fdp = eval_blip_fd_radial(cgrid, sgrid, rcut + h, Zi, Zj, rij);
    auto fdm = eval_blip_fd_radial(cgrid, sgrid, rcut - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
        INFO("RCUT2B component=" << c_
             << " analytical=" << dfd(c_, 0) << " FD=" << fd);
        CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip d(fd)/d theta parity on interior-only configuration",
          "[MODELS][d2_blip][dtheta][force][parity]")
{
    // x_c[0]=+0.30, x_c[1]=-0.24 — both interior with margin >= 0.7
    // from |x|=1, so b'' is smooth and FD is accurate.
    const std::vector<double> cgrid = {1.0, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context(cgrid, sgrid, rcut);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    const double h = 1e-5;
    for (const std::string key : {"CGRID2B", "SGRID2B"}) {
        for (std::size_t k = 0; k < cgrid.size(); ++k) {
            tadah::core::fd_type dfd(d.size());
            const bool ok = d.calc_dXijdri_dtheta(key, static_cast<int>(k + 1),
                                                  Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            auto cp = cgrid, cm = cgrid, sp = sgrid, sm = sgrid;
            if (key == "CGRID2B") { cp[k] += h; cm[k] -= h; }
            else                  { sp[k] += h; sm[k] -= h; }
            auto fdp = eval_blip_fd_radial(cp, sp, rcut, Zi, Zj, rij);
            auto fdm = eval_blip_fd_radial(cm, sm, rcut, Zi, Zj, rij);

            for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
                const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
                INFO(key << "[" << k+1 << "] component=" << c_
                     << " analytical=" << dfd(c_, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip returns false on unowned theta_name for force derivative",
          "[MODELS][d2_blip][dtheta][force]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    Context c = make_blip_context(cgrid, sgrid, 3.5);
    D2_Blip d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B", 0,  1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B", 99, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("SGRID2B", 0,  1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dfd));
}

// ----------------------------------------------------------------------
// WATOM parity.
// D2_Blip's per-pair contribution is weights[Zj] * t_pair; the WATOM
// derivative drops the weights[Zj] factor and only fires when Zj matches
// the species the optimised W[arg_pos] indexes into.
// ----------------------------------------------------------------------

namespace {
Context make_blip_context_with_watom(const std::vector<double>& cgrid,
                                     const std::vector<double>& sgrid,
                                     double rcut,
                                     double watom_value) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", watom_value);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sgrid.size(); ++i) { if (i) ss << " "; ss << sgrid[i]; }
        c.add("SGRID2B", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cgrid.size(); ++i) { if (i) ss << " "; ss << cgrid[i]; }
        c.add("CGRID2B", ss.str());
    }
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_Blip");
    c.add("TYPE2B", 2);
    c.add("TYPE2B", 2);
    c.add("TYPE2B", "H");
    c.add("TYPE2B", "H");
    return c;
}

aed_type eval_blip_aed_with_watom(const std::vector<double>& cgrid,
                                  const std::vector<double>& sgrid,
                                  double rcut, double watom,
                                  int Zi, int Zj, double rij) {
    Context c = make_blip_context_with_watom(cgrid, sgrid, rcut, watom);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

std::vector<double>
eval_blip_fd_radial_with_watom(const std::vector<double>& cgrid,
                               const std::vector<double>& sgrid,
                               double rcut, double watom,
                               int Zi, int Zj, double rij) {
    Context c = make_blip_context_with_watom(cgrid, sgrid, rcut, watom);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd_ij(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd_ij);
    std::vector<double> out(d.size());
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) out[c_] = fd_ij(c_, 0);
    return out;
}
}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical dD/d WATOM matches central difference",
          "[MODELS][d2_blip][dtheta][watom][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context_with_watom(cgrid, sgrid, rcut, 1.0);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    aed_type dD(d.size());
    dD.set_zero();
    const bool ok = d.calc_dDdtheta("WATOM", 1, Zi, Zj, rij, rij * rij, dD);
    REQUIRE(ok);

    const double h = 1e-5;
    aed_type aed_p = eval_blip_aed_with_watom(cgrid, sgrid, rcut, 1.0 + h, Zi, Zj, rij);
    aed_type aed_m = eval_blip_aed_with_watom(cgrid, sgrid, rcut, 1.0 - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
        INFO("WATOM[1] component=" << c_
             << " analytical=" << dD[c_] << " FD=" << fd);
        CHECK(std::abs(dD[c_] - fd) < 1e-5);
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip analytical d(fd)/d WATOM matches central difference",
          "[MODELS][d2_blip][dtheta][force][watom][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_blip_context_with_watom(cgrid, sgrid, rcut, 1.0);
    D2_Blip d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    const bool ok = d.calc_dXijdri_dtheta("WATOM", 1, Zi, Zj, rij, rij * rij, dfd);
    REQUIRE(ok);

    const double h = 1e-5;
    auto fdp = eval_blip_fd_radial_with_watom(cgrid, sgrid, rcut, 1.0 + h, Zi, Zj, rij);
    auto fdm = eval_blip_fd_radial_with_watom(cgrid, sgrid, rcut, 1.0 - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
        INFO("WATOM[1] component=" << c_
             << " analytical=" << dfd(c_, 0) << " FD=" << fd);
        CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip WATOM claims ownership with chain-rule contributions",
          "[MODELS][d2_blip][dtheta][watom]")
{
    // Build a multi-species context (H, Ta) with per-species WATOM 1.0 1.0.
    // Under the geometric-mean pair-weight rule, the WATOM chain factor is
    //   chain(Zi, Zj, n) = d weights[Zi][Zj] / d w_n
    // which equals 1 on the diagonal (Zi==Zj==Z_target), 0.5*sqrt(w_other/w_n)
    // off-diagonal when exactly one of Zi, Zj equals Z_target, and 0 otherwise.
    // Concretely with w_H=w_Ta=1.0:
    //   arg_pos=1 (Z_target=H): chain(H,H)=1, chain(Ta,H)=chain(H,Ta)=0.5,
    //                            chain(Ta,Ta)=0.
    //   arg_pos=2 (Z_target=Ta): chain(Ta,Ta)=1, chain(Ta,H)=chain(H,Ta)=0.5,
    //                            chain(H,H)=0.
    // Ownership (return value) is true whenever arg_pos is in range, regardless
    // of whether the chain factor is zero.
    Context c;
    c.add("ATOM", "H");
    c.add("ATOM", "Ta");
    c.add("WATOM", 1.0);
    c.add("WATOM", 1.0);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    c.add("SGRID2B", "2.0 1.0");
    c.add("CGRID2B", "0.5 1.4");
    c.add("RCUT2B", 3.5);
    c.add("TYPE2B", "D2_Blip");
    c.add("TYPE2B", 2);
    c.add("TYPE2B", 2);
    c.add("TYPE2B", "H"); c.add("TYPE2B", "H");
    c.add("TYPE2B", "H"); c.add("TYPE2B", "Ta");
    c.add("TYPE2B", "Ta"); c.add("TYPE2B", "H");
    c.add("TYPE2B", "Ta"); c.add("TYPE2B", "Ta");

    D2_Blip d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    const double rij = 1.2;
    const int Z_H = 1, Z_Ta = 73;

    // arg_pos=2 (Z_target=Ta), pair (Zi=H, Zj=H) → neither matches, chain=0.
    aed_type dD2(d.size()); dD2.set_zero();
    const bool ok2 = d.calc_dDdtheta("WATOM", 2, Z_H, Z_H, rij, rij * rij, dD2);
    REQUIRE(ok2);
    for (std::size_t c_ = 0; c_ < d.size(); ++c_)
        CHECK(dD2[c_] == Approx(0.0).margin(1e-15));

    // arg_pos=2 (Z_target=Ta), pair (Zi=Ta, Zj=H) → off-diagonal, chain=0.5.
    aed_type dD2_off(d.size()); dD2_off.set_zero();
    const bool ok2_off = d.calc_dDdtheta("WATOM", 2, Z_Ta, Z_H, rij, rij * rij, dD2_off);
    REQUIRE(ok2_off);
    double sumsq_off = 0;
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) sumsq_off += dD2_off[c_] * dD2_off[c_];
    CHECK(sumsq_off > 1e-12);

    // arg_pos=1 (Z_target=H), pair (Zi=H, Zj=H) → diagonal, chain=1.
    aed_type dD1(d.size()); dD1.set_zero();
    const bool ok1 = d.calc_dDdtheta("WATOM", 1, Z_H, Z_H, rij, rij * rij, dD1);
    REQUIRE(ok1);
    double sumsq = 0;
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) sumsq += dD1[c_] * dD1[c_];
    CHECK(sumsq > 1e-12);

    // Off-diagonal contribution should be exactly 0.5 * diagonal (since
    // w_H = w_Ta = 1, chain_off / chain_diag = 0.5/1.0). Compare per-component.
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        INFO("component=" << c_ << " dD1=" << dD1[c_] << " dD2_off=" << dD2_off[c_]);
        CHECK(dD2_off[c_] == Approx(0.5 * dD1[c_]).margin(1e-12));
    }

    // Force-side mirror: arg_pos=2, (Zi=H, Zj=H) → zero contribution.
    tadah::core::fd_type dfd2(d.size());
    const bool okf2 = d.calc_dXijdri_dtheta("WATOM", 2, Z_H, Z_H, rij, rij * rij, dfd2);
    REQUIRE(okf2);
    for (std::size_t c_ = 0; c_ < d.size(); ++c_)
        CHECK(dfd2(c_, 0) == Approx(0.0).margin(1e-15));

    // arg_pos out of range returns false (configuration error, route to FD).
    aed_type dD3(d.size()); dD3.set_zero();
    CHECK_FALSE(d.calc_dDdtheta("WATOM", 99, Z_Ta, Z_H, rij, rij * rij, dD3));
    CHECK_FALSE(d.calc_dDdtheta("WATOM", 0,  Z_Ta, Z_H, rij, rij * rij, dD3));
}

TEST_CASE_METHOD(TestFixture,
          "D2_Blip returns false on out-of-range or unowned theta",
          "[MODELS][d2_blip][dtheta]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    Context c = make_blip_context(cgrid, sgrid, 3.5);
    D2_Blip d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B", 0,  1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B", 99, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("SGRID2B", 0,  1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("SGRID2B", 5,  1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dD));

    // Beyond support of B: x_c = eta (r - mu); pick mu and r so x_c > 2.
    // With cgrid[0]=0.5, sgrid[0]=2.0: x_c at r=3.0 is 2.0*(3.0-0.5)=5.0.
    // calc_dDdtheta still returns true (B is zero but the analytical
    // expression evaluates to zero in that branch) — match calc_aed's
    // contract (it would += 0).
    aed_type dD2(d.size()); dD2.set_zero();
    const bool ok = d.calc_dDdtheta("CGRID2B", 1, 1, 1, 3.0, 9.0, dD2);
    REQUIRE(ok);
    // All entries should still be zero — outside support.
    for (std::size_t i = 0; i < d.size(); ++i) {
        CHECK(dD2[i] == Approx(0.0).margin(1e-15));
    }
}
