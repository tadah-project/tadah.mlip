// test_d2_bp_dtheta.cpp
//
// Layer-1 parity tests for the closed-form analytical derivatives of
// D2_BP with respect to its hyperparameters. For each (theta_name,
// arg_pos) the analytical dD/d theta is compared against a central-
// difference reference computed by building the descriptor from a
// perturbed Context.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_bp.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_BP;

// Test fixture: periodic table must be initialised before any
// constructor that resolves an atom symbol like "H".
struct TestFixture {
    TestFixture() { PeriodicTable::initialize(); }
};

// Build a Context populated with a fresh CGRID2B/SGRID2B/RCUT2B and
// the boilerplate D2_BP requires. Used by both the analytical-eval and
// the FD-perturbation paths.
Context make_bp_context(const std::vector<double>& cgrid,
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
    // RCUT2BMAX is an internal key not user-addable; leave it absent
    // so D2_Base falls back to Cut_Dummy(0) at construction time. The
    // test harness then attaches a Cut_Cos via set_fcut explicitly,
    // mirroring the existing test_d2 pattern.
    c.add("TYPE2B", "D2_BP");
    c.add("TYPE2B", 2);    // grid sizes
    c.add("TYPE2B", 2);
    c.add("TYPE2B", "H");  // atom-pair symbols (matches test_d2 setup)
    c.add("TYPE2B", "H");
    return c;
}

// Compute calc_aed at the given pair / distance, with cgrid/sgrid/rcut
// taken from the supplied Context. Caller owns the cutoff lifetime.
aed_type eval_bp_aed(const std::vector<double>& cgrid,
                     const std::vector<double>& sgrid,
                     double rcut,
                     int Zi, int Zj, double rij) {
    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

// Compute calc_dXijdri (the radial fd_ij column) at the given pair /
// distance with the supplied (cgrid, sgrid, rcut). Returns a vector
// containing fd(c, 0) for c in [0, size). Used as the FD-perturbation
// reference for the calc_dXijdri_dtheta parity tests.
std::vector<double>
eval_bp_fd_radial(const std::vector<double>& cgrid,
                  const std::vector<double>& sgrid,
                  double rcut,
                  int Zi, int Zj, double rij) {
    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd_ij(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd_ij);
    std::vector<double> out(d.size());
    for (std::size_t c_ = 0; c_ < d.size(); ++c_)
        out[c_] = fd_ij(c_, 0);
    return out;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical dD/d CGRID2B matches central difference",
          "[MODELS][d2_bp][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    for (std::size_t k = 0; k < cgrid.size(); ++k) {
        // Analytical.
        aed_type dD(d.size());
        dD.set_zero();
        const bool ok = d.calc_dDdtheta("CGRID2B", static_cast<int>(k + 1),
                                        Zi, Zj, rij, rij * rij, dD);
        REQUIRE(ok);

        // FD on cgrid[k].
        const double h = 1e-5;
        auto cp = cgrid;  cp[k] += h;
        auto cm = cgrid;  cm[k] -= h;
        aed_type aed_p = eval_bp_aed(cp, sgrid, rcut, Zi, Zj, rij);
        aed_type aed_m = eval_bp_aed(cm, sgrid, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
            INFO("CGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dD[c_] << " FD=" << fd);
            // 1e-5 absolute tolerance: FD error scales like O(h^2 * f''')
            // and f''' for a Gaussian can be O(1).
            CHECK(std::abs(dD[c_] - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical dD/d SGRID2B matches central difference",
          "[MODELS][d2_bp][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
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
        aed_type aed_p = eval_bp_aed(cgrid, sp, rcut, Zi, Zj, rij);
        aed_type aed_m = eval_bp_aed(cgrid, sm, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
            INFO("SGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dD[c_] << " FD=" << fd);
            CHECK(std::abs(dD[c_] - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical dD/d RCUT2B matches central difference",
          "[MODELS][d2_bp][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    aed_type dD(d.size());
    dD.set_zero();
    const bool ok = d.calc_dDdtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dD);
    REQUIRE(ok);

    const double h = 1e-5;
    aed_type aed_p = eval_bp_aed(cgrid, sgrid, rcut + h, Zi, Zj, rij);
    aed_type aed_m = eval_bp_aed(cgrid, sgrid, rcut - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
        INFO("RCUT2B component=" << c_
             << " analytical=" << dD[c_] << " FD=" << fd);
        CHECK(std::abs(dD[c_] - fd) < 1e-5);
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP returns false on out-of-range CGRID2B arg_pos",
          "[MODELS][d2_bp][dtheta]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    Context c = make_bp_context(cgrid, sgrid, 3.5);
    D2_BP d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B", 0,  1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B", 99, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dD));
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical d(fd)/d CGRID2B matches central difference",
          "[MODELS][d2_bp][dtheta][force][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
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
        auto fdp = eval_bp_fd_radial(cp, sgrid, rcut, Zi, Zj, rij);
        auto fdm = eval_bp_fd_radial(cm, sgrid, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
            INFO("CGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dfd(c_, 0) << " FD=" << fd);
            CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical d(fd)/d SGRID2B matches central difference",
          "[MODELS][d2_bp][dtheta][force][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
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
        auto fdp = eval_bp_fd_radial(cgrid, sp, rcut, Zi, Zj, rij);
        auto fdm = eval_bp_fd_radial(cgrid, sm, rcut, Zi, Zj, rij);

        for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
            const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
            INFO("SGRID2B[" << k+1 << "] component=" << c_
                 << " analytical=" << dfd(c_, 0) << " FD=" << fd);
            CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical d(fd)/d RCUT2B matches central difference",
          "[MODELS][d2_bp][dtheta][force][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5;
    const double rij  = 1.2;
    const int    Zi   = 1, Zj = 1;

    Context c = make_bp_context(cgrid, sgrid, rcut);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    const bool ok = d.calc_dXijdri_dtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dfd);
    REQUIRE(ok);

    const double h = 1e-5;
    auto fdp = eval_bp_fd_radial(cgrid, sgrid, rcut + h, Zi, Zj, rij);
    auto fdm = eval_bp_fd_radial(cgrid, sgrid, rcut - h, Zi, Zj, rij);

    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd = (fdp[c_] - fdm[c_]) / (2.0 * h);
        INFO("RCUT2B component=" << c_
             << " analytical=" << dfd(c_, 0) << " FD=" << fd);
        CHECK(std::abs(dfd(c_, 0) - fd) < 1e-5);
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_BP returns false on unowned theta_name for force derivative",
          "[MODELS][d2_bp][dtheta][force]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    Context c = make_bp_context(cgrid, sgrid, 3.5);
    D2_BP d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B", 0,  1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B", 99, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dfd));
}

// ----------------------------------------------------------------------
// WATOM parity. The per-pair contribution is
// weights[Zj] * G; its WATOM derivative drops the weights[Zj] factor
// and only fires when Zj matches the species at arg_pos in the ATOM list.
// ----------------------------------------------------------------------

namespace {
Context make_bp_context_with_watom(const std::vector<double>& cgrid,
                                   const std::vector<double>& sgrid,
                                   double rcut, double watom) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", watom);
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
    c.add("TYPE2B", "D2_BP");
    c.add("TYPE2B", 2);
    c.add("TYPE2B", 2);
    c.add("TYPE2B", "H"); c.add("TYPE2B", "H");
    return c;
}
aed_type eval_bp_aed_with_watom(const std::vector<double>& cgrid,
                                const std::vector<double>& sgrid,
                                double rcut, double watom,
                                int Zi, int Zj, double rij) {
    Context c = make_bp_context_with_watom(cgrid, sgrid, rcut, watom);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}
std::vector<double>
eval_bp_fd_with_watom(const std::vector<double>& cgrid,
                      const std::vector<double>& sgrid,
                      double rcut, double watom,
                      int Zi, int Zj, double rij) {
    Context c = make_bp_context_with_watom(cgrid, sgrid, rcut, watom);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd);
    std::vector<double> out(d.size());
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) out[c_] = fd(c_, 0);
    return out;
}
}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_BP analytical d/d WATOM matches FD (energy and force)",
          "[MODELS][d2_bp][dtheta][watom][parity]")
{
    const std::vector<double> cgrid = {0.5, 1.4};
    const std::vector<double> sgrid = {2.0, 1.0};
    const double rcut = 3.5, rij = 1.2;
    const int Zi = 1, Zj = 1;

    Context c = make_bp_context_with_watom(cgrid, sgrid, rcut, 1.0);
    D2_BP d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    REQUIRE(d.calc_dDdtheta("WATOM", 1, Zi, Zj, rij, rij * rij, dD));
    tadah::core::fd_type dfd(d.size());
    REQUIRE(d.calc_dXijdri_dtheta("WATOM", 1, Zi, Zj, rij, rij * rij, dfd));

    const double h = 1e-5;
    aed_type aed_p = eval_bp_aed_with_watom(cgrid, sgrid, rcut, 1.0 + h, Zi, Zj, rij);
    aed_type aed_m = eval_bp_aed_with_watom(cgrid, sgrid, rcut, 1.0 - h, Zi, Zj, rij);
    auto fdp = eval_bp_fd_with_watom(cgrid, sgrid, rcut, 1.0 + h, Zi, Zj, rij);
    auto fdm = eval_bp_fd_with_watom(cgrid, sgrid, rcut, 1.0 - h, Zi, Zj, rij);
    for (std::size_t c_ = 0; c_ < d.size(); ++c_) {
        const double fd_e = (aed_p[c_] - aed_m[c_]) / (2.0 * h);
        const double fd_f = (fdp[c_]   - fdm[c_])   / (2.0 * h);
        CHECK(std::abs(dD[c_]      - fd_e) < 1e-5);
        CHECK(std::abs(dfd(c_, 0)  - fd_f) < 1e-5);
    }
}

TEST_CASE("Cut_Cos calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.95}) {
            if (r >= rcut) continue;
            Cut_Cos fc0(rcut);
            const double analytical = fc0.calc_drcut(r);

            const double h = 1e-6;
            Cut_Cos fcp(rcut + h);
            Cut_Cos fcm(rcut - h);
            const double fd = (fcp.calc(r) - fcm.calc(r)) / (2.0 * h);

            INFO("rcut=" << rcut << " r=" << r
                 << " analytical=" << analytical << " FD=" << fd);
            CHECK(std::abs(analytical - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_Dummy calc_drcut returns 0 (discontinuous)",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_Dummy;
    Cut_Dummy fc(3.0);
    CHECK(fc.calc_drcut(1.0) == 0.0);
    CHECK(fc.calc_drcut(2.5) == 0.0);
    CHECK(fc.calc_drcut(5.0) == 0.0);
}
