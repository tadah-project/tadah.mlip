// test_d2_mie_dtheta.cpp
//
// Layer-1 parity tests for D2_MIE::calc_dDdtheta.
// Surface: RCUT2B + the two TYPE2B-positional Mie exponents
//   arg_pos == 2 -> n  (config index 1, default LJ-like 12)
//   arg_pos == 3 -> m  (config index 2, default LJ-like 6)
// The +1 shift versus user-facing OPTIM (n) accounts for the leading
// "D2_MIE" string token at config index 0.
//
// Closed forms:
//   dD_n/dn      = +w(Zj) ln(r) r^{-n} fc(r)
//   dD_m/dm      = -w(Zj) ln(r) r^{-m} fc(r)
//   dD_n/d rcut  = -w(Zj) r^{-n} dfc/d rcut
//   dD_m/d rcut  = +w(Zj) r^{-m} dfc/d rcut
//
// At r==1, ln(r)==0 and the n/m derivatives vanish identically; the
// parity probes pick rij != 1 to avoid that trivial case.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_mie.h>

#include <cmath>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_MIE;

struct TestFixture { TestFixture() { PeriodicTable::initialize(); } };

Context make_mie_context(double rcut, double n_exp, double m_exp) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_MIE");
    c.add("TYPE2B", n_exp);
    c.add("TYPE2B", m_exp);
    c.add("TYPE2B", "H");
    c.add("TYPE2B", "H");
    return c;
}

aed_type eval_mie_aed(double rcut, double n_exp, double m_exp,
                      int Zi, int Zj, double rij)
{
    Context c = make_mie_context(rcut, n_exp, m_exp);
    D2_MIE d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

std::vector<double> eval_mie_fd_radial(double rcut, double n_exp, double m_exp,
                                       int Zi, int Zj, double rij)
{
    Context c = make_mie_context(rcut, n_exp, m_exp);
    D2_MIE d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd_ij(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd_ij);
    std::vector<double> out(d.size());
    for (std::size_t k = 0; k < d.size(); ++k) out[k] = fd_ij(k, 0);
    return out;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical dD/d RCUT2B matches central difference",
          "[MODELS][d2_mie][dtheta][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double n_exp = 12.0, m_exp = 6.0;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {1.0, 1.5, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size()); dD.set_zero();
            const bool ok =
                d.calc_dDdtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            const double h = 1e-6;
            aed_type aed_p = eval_mie_aed(rcut + h, n_exp, m_exp, Zi, Zj, rij);
            aed_type aed_m = eval_mie_aed(rcut - h, n_exp, m_exp, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
                INFO("D2_MIE rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                CHECK(std::abs(dD[k] - fd) < 1e-5);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical dD/d n (TYPE2B arg_pos=2) matches central difference",
          "[MODELS][d2_mie][dtheta][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double rcut = 3.5;
    const double m_exp = 6.0;

    // Probe both r > 1 (positive ln(r)) and r < 1 (negative ln(r)).
    // Avoid r exactly == 1 (ln(r) = 0 -> trivial 0=0 check).
    for (const double n_exp : {6.0, 12.0, 18.0}) {
        for (const double rij : {0.7, 1.5, 2.0}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size()); dD.set_zero();
            const bool ok =
                d.calc_dDdtheta("TYPE2B", 2, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            const double h = 1e-6;
            aed_type aed_p = eval_mie_aed(rcut, n_exp + h, m_exp, Zi, Zj, rij);
            aed_type aed_m = eval_mie_aed(rcut, n_exp - h, m_exp, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
                INFO("D2_MIE n=" << n_exp << " m=" << m_exp << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                // m-column (k=1) must be untouched (analytical ~ 0; FD ~ 0).
                CHECK(std::abs(dD[k] - fd) < 1e-5);
            }
            // The m-column should be exactly 0 in the analytical pass —
            // n-perturbation does not touch the (fidx+1) entry.
            CHECK(dD[1] == 0.0);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical dD/d m (TYPE2B arg_pos=3) matches central difference",
          "[MODELS][d2_mie][dtheta][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double rcut = 3.5;
    const double n_exp = 12.0;

    for (const double m_exp : {3.0, 6.0, 9.0}) {
        for (const double rij : {0.7, 1.5, 2.0}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size()); dD.set_zero();
            const bool ok =
                d.calc_dDdtheta("TYPE2B", 3, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            const double h = 1e-6;
            aed_type aed_p = eval_mie_aed(rcut, n_exp, m_exp + h, Zi, Zj, rij);
            aed_type aed_m = eval_mie_aed(rcut, n_exp, m_exp - h, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
                INFO("D2_MIE n=" << n_exp << " m=" << m_exp << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                CHECK(std::abs(dD[k] - fd) < 1e-5);
            }
            // The n-column should be exactly 0 in the analytical pass.
            CHECK(dD[0] == 0.0);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE TYPE2B exponent gradients vanish at r=1 (ln(1)=0)",
          "[MODELS][d2_mie][dtheta][parity]")
{
    Context c = make_mie_context(3.5, 12.0, 6.0);
    D2_MIE d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    const double rij = 1.0;
    for (int arg_pos : {2, 3}) {
        aed_type dD(d.size()); dD.set_zero();
        const bool ok =
            d.calc_dDdtheta("TYPE2B", arg_pos, 1, 1, rij, rij * rij, dD);
        REQUIRE(ok);
        for (std::size_t k = 0; k < d.size(); ++k) {
            INFO("r=1 arg_pos=" << arg_pos << " k=" << k
                 << " analytical=" << dD[k]);
            CHECK(std::abs(dD[k]) < 1e-15);
        }
    }
}

// ----------------------------------------------------------------------
// Force-descriptor θ-derivative parity.
// ----------------------------------------------------------------------

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical d(fd)/d RCUT2B matches central difference",
          "[MODELS][d2_mie][dtheta][force][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double n_exp = 12.0, m_exp = 6.0;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {1.0, 1.5, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok =
                d.calc_dXijdri_dtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            const double h = 1e-6;
            auto fdp = eval_mie_fd_radial(rcut + h, n_exp, m_exp, Zi, Zj, rij);
            auto fdm = eval_mie_fd_radial(rcut - h, n_exp, m_exp, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_MIE rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(k, 0) - fd) < 1e-4);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical d(fd)/d n (TYPE2B arg_pos=2) matches central difference",
          "[MODELS][d2_mie][dtheta][force][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double rcut = 3.5;
    const double m_exp = 6.0;

    for (const double n_exp : {6.0, 12.0, 18.0}) {
        for (const double rij : {0.7, 1.5, 2.0}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok =
                d.calc_dXijdri_dtheta("TYPE2B", 2, Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            const double h = 1e-6;
            auto fdp = eval_mie_fd_radial(rcut, n_exp + h, m_exp, Zi, Zj, rij);
            auto fdm = eval_mie_fd_radial(rcut, n_exp - h, m_exp, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_MIE force n=" << n_exp << " m=" << m_exp << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(k, 0) - fd) < 1e-4);
            }
            // m-column (k=1) untouched by n-derivative.
            CHECK(dfd(1, 0) == 0.0);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE analytical d(fd)/d m (TYPE2B arg_pos=3) matches central difference",
          "[MODELS][d2_mie][dtheta][force][parity]")
{
    const int    Zi = 1, Zj = 1;
    const double rcut = 3.5;
    const double n_exp = 12.0;

    for (const double m_exp : {3.0, 6.0, 9.0}) {
        for (const double rij : {0.7, 1.5, 2.0}) {
            if (rij >= rcut) continue;

            Context c = make_mie_context(rcut, n_exp, m_exp);
            D2_MIE d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok =
                d.calc_dXijdri_dtheta("TYPE2B", 3, Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            const double h = 1e-6;
            auto fdp = eval_mie_fd_radial(rcut, n_exp, m_exp + h, Zi, Zj, rij);
            auto fdm = eval_mie_fd_radial(rcut, n_exp, m_exp - h, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_MIE force n=" << n_exp << " m=" << m_exp << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(k, 0) - fd) < 1e-4);
            }
            // n-column (k=0) untouched by m-derivative.
            CHECK(dfd(0, 0) == 0.0);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE force d(fd)/d n,m vanish at r=1 (ln(1)=0 — only the cutoff "
          "1-n ln(r) bit remains and r^-n-1 * fc may still be non-zero at r=1)",
          "[MODELS][d2_mie][dtheta][force]")
{
    // At r=1: ln(r)=0 but the closed form for d(fd)/dn still has a
    // (1 - n·ln(r)) factor on the r^-n-1 · fc term, which reduces to
    // r^-n-1 · fc at r=1. So d(fd)/dn != 0 in general at r=1 — only
    // the energy-side dD/dn vanishes there. This test documents that
    // contract.
    Context c = make_mie_context(3.5, 12.0, 6.0);
    D2_MIE d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    const double rij = 1.0;
    for (int arg_pos : {2, 3}) {
        tadah::core::fd_type dfd(d.size());
        const bool ok =
            d.calc_dXijdri_dtheta("TYPE2B", arg_pos, 1, 1, rij, rij * rij, dfd);
        REQUIRE(ok);
        // FD reference is the ground truth; verify analytical matches.
        const double h = 1e-6;
        auto fdp = eval_mie_fd_radial(3.5, arg_pos == 2 ? 12.0 + h : 12.0,
                                              arg_pos == 3 ? 6.0  + h :  6.0,
                                              1, 1, rij);
        auto fdm = eval_mie_fd_radial(3.5, arg_pos == 2 ? 12.0 - h : 12.0,
                                              arg_pos == 3 ? 6.0  - h :  6.0,
                                              1, 1, rij);
        for (std::size_t k = 0; k < d.size(); ++k) {
            const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
            INFO("r=1 arg_pos=" << arg_pos << " k=" << k
                 << " analytical=" << dfd(k, 0) << " FD=" << fd);
            CHECK(std::abs(dfd(k, 0) - fd) < 1e-4);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE calc_dXijdri_dtheta declines unsupported keys",
          "[MODELS][d2_mie][dtheta][force]")
{
    Context c = make_mie_context(3.5, 12.0, 6.0);
    D2_MIE d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    CHECK_FALSE(d.calc_dXijdri_dtheta("TYPE2B",   1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("TYPE2B",   4, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B",  1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dfd));
}

TEST_CASE_METHOD(TestFixture,
          "D2_MIE calc_dDdtheta declines unsupported keys and arg_pos",
          "[MODELS][d2_mie][dtheta]")
{
    Context c = make_mie_context(3.5, 12.0, 6.0);
    D2_MIE d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    // arg_pos=1 is the leading "D2_MIE" string token; arg_pos=4 sits
    // at the first element symbol — both must decline so HYBRID FD-falls.
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B",   1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B",   4, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B",   0, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B",  1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("SGRID2B",  1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dD));
}
