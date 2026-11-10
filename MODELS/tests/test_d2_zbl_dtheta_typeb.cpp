// test_d2_zbl_dtheta_typeb.cpp
//
// Layer-1 parity tests for the D2_ZBL TYPE2B HPO surface
// (a0 / s0 / p0 / p1). The RCUT2B parity test
// already lives in test_d2_zbl_dtheta.cpp; this file covers the four
// screening-length parameters baked into TYPE2B positions 1..4.
//
// arg_pos convention recap (HPOTargets::ui_offset + i+1):
//   arg_pos == 2 -> a0
//   arg_pos == 3 -> s0
//   arg_pos == 4 -> p0
//   arg_pos == 5 -> p1
// The +1 shift relative to the user-facing OPTIM (n) accounts for the
// leading "D2_ZBL" string token that sits at TYPE2B config index 0.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_zbl.h>

#include <cmath>
#include <string>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_ZBL;

struct TestFixture { TestFixture() { PeriodicTable::initialize(); } };

// All four screening params supplied as concrete numerics — sentinel
// -1 would select the hardcoded defaults (and we'd silently FD against
// them). Caller is responsible for choosing values inside HPO
// bound conventions (>0).
Context make_zbl_context_explicit(const std::string& elem,
                                  double rcut,
                                  double a0,
                                  double s0,
                                  double p0,
                                  double p1) {
    Context c;
    c.add("ATOM", elem);
    c.add("WATOM", 1);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", a0);
    c.add("TYPE2B", s0);
    c.add("TYPE2B", p0);
    c.add("TYPE2B", p1);
    c.add("TYPE2B", elem);
    c.add("TYPE2B", elem);
    return c;
}

aed_type eval_aed(const std::string& elem,
                  double rcut,
                  double a0, double s0, double p0, double p1,
                  int Zi, int Zj, double rij)
{
    Context c = make_zbl_context_explicit(elem, rcut, a0, s0, p0, p1);
    D2_ZBL d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

std::vector<double> eval_fd_radial(const std::string& elem,
                                   double rcut,
                                   double a0, double s0, double p0, double p1,
                                   int Zi, int Zj, double rij)
{
    Context c = make_zbl_context_explicit(elem, rcut, a0, s0, p0, p1);
    D2_ZBL d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    tadah::core::fd_type fd_ij(d.size());
    d.calc_dXijdri(Zi, Zj, rij, rij * rij, fd_ij);
    std::vector<double> out(d.size());
    for (std::size_t k = 0; k < d.size(); ++k) out[k] = fd_ij(k, 0);
    return out;
}

// Probe with a moderately-heavy single-element pair (Si Z=14) so:
//  - Zi=Zj > 1 -> ln(Zi) != 0, so da/dp0 and da/dp1 are non-trivial.
//  - Screening length a is short enough that x = r/a sits in the
//    sensitive region (~3..5) where dphi/dx is well-conditioned.
// rij values are picked to stay safely inside the cosine cutoff and
// inside the screening "knee" where phi varies smoothly.
constexpr int    kZSi        = 14;
constexpr double kA0Default  = 0.52917721067;
constexpr double kS0Default  = 0.88534;
constexpr double kP0Default  = 0.23;
constexpr double kP1Default  = 0.23;

void check_one_theta(int    arg_pos,
                     double theta_perturb_h,
                     double abs_tol,
                     int    Zi, int Zj,
                     const std::string& elem)
{
    const double a0 = kA0Default;
    const double s0 = kS0Default;
    const double p0 = kP0Default;
    const double p1 = kP1Default;

    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {0.6, 1.0, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_zbl_context_explicit(elem, rcut, a0, s0, p0, p1);
            D2_ZBL d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size()); dD.set_zero();
            const bool ok =
                d.calc_dDdtheta("TYPE2B", arg_pos, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            double a0_p = a0, s0_p = s0, p0_p = p0, p1_p = p1;
            double a0_m = a0, s0_m = s0, p0_m = p0, p1_m = p1;
            switch (arg_pos) {
                case 2: a0_p += theta_perturb_h; a0_m -= theta_perturb_h; break;
                case 3: s0_p += theta_perturb_h; s0_m -= theta_perturb_h; break;
                case 4: p0_p += theta_perturb_h; p0_m -= theta_perturb_h; break;
                case 5: p1_p += theta_perturb_h; p1_m -= theta_perturb_h; break;
                default: FAIL("unexpected arg_pos");
            }

            aed_type aed_p =
                eval_aed(elem, rcut, a0_p, s0_p, p0_p, p1_p, Zi, Zj, rij);
            aed_type aed_m =
                eval_aed(elem, rcut, a0_m, s0_m, p0_m, p1_m, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * theta_perturb_h);
                INFO("D2_ZBL elem=" << elem << " arg_pos=" << arg_pos
                     << " rcut=" << rcut << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                CHECK(std::abs(dD[k] - fd) < abs_tol);
            }
        }
    }
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical dD/d a0 (TYPE2B arg_pos=2) matches central difference",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    // a0 enters multiplicatively in `a`, so dphi/da0 has the same
    // sensitivity as dphi/d rcut. Same loose tol (1e-4).
    check_one_theta(/*arg_pos=*/2, /*h=*/1e-6, /*abs_tol=*/1e-4,
                    kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical dD/d s0 (TYPE2B arg_pos=3) matches central difference",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    check_one_theta(/*arg_pos=*/3, /*h=*/1e-6, /*abs_tol=*/1e-4,
                    kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical dD/d p0 (TYPE2B arg_pos=4) matches central difference",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    // p0 enters via Zi^p0 * ln(Zi); for Si (Z=14) ln(Z) = 2.64.
    // Slightly larger FD truncation than a0/s0 -> 1e-3.
    check_one_theta(/*arg_pos=*/4, /*h=*/1e-6, /*abs_tol=*/1e-3,
                    kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical dD/d p1 (TYPE2B arg_pos=5) matches central difference",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    check_one_theta(/*arg_pos=*/5, /*h=*/1e-6, /*abs_tol=*/1e-3,
                    kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL TYPE2B p0/p1 gradients vanish identically for Z=1 (ln(1)=0)",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    // For H-H pairs, da/dp0 = -a * 1^p0 * ln(1) / D = 0 and likewise
    // da/dp1 = 0. Closed form must return true (claims ownership) but
    // contribute nothing. FD must be ~0 too.
    Context c = make_zbl_context_explicit(
        "H", /*rcut=*/3.5, kA0Default, kS0Default, kP0Default, kP1Default);
    D2_ZBL d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    const int    Zi = 1, Zj = 1;
    const double rij = 1.0;
    const double h   = 1e-6;

    for (int arg_pos : {4, 5}) {
        aed_type dD(d.size()); dD.set_zero();
        const bool ok =
            d.calc_dDdtheta("TYPE2B", arg_pos, Zi, Zj, rij, rij * rij, dD);
        REQUIRE(ok);

        double p0_p = kP0Default, p0_m = kP0Default;
        double p1_p = kP1Default, p1_m = kP1Default;
        if (arg_pos == 4) { p0_p += h; p0_m -= h; }
        else              { p1_p += h; p1_m -= h; }

        aed_type aed_p = eval_aed("H", 3.5, kA0Default, kS0Default,
                                  p0_p, p1_p, Zi, Zj, rij);
        aed_type aed_m = eval_aed("H", 3.5, kA0Default, kS0Default,
                                  p0_m, p1_m, Zi, Zj, rij);

        for (std::size_t k = 0; k < d.size(); ++k) {
            const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
            INFO("H-H arg_pos=" << arg_pos << " k=" << k
                 << " analytical=" << dD[k] << " FD=" << fd);
            CHECK(std::abs(dD[k]) < 1e-12);
            CHECK(std::abs(fd)    < 1e-9);
        }
    }
}

// ----------------------------------------------------------------------
// Force-descriptor θ-derivative parity.
// Reference: central FD on the radial column of calc_dXijdri.
// ----------------------------------------------------------------------

void check_one_theta_force(int    arg_pos,
                           double h,
                           double abs_tol,
                           int    Zi, int Zj,
                           const std::string& elem)
{
    const double a0 = kA0Default;
    const double s0 = kS0Default;
    const double p0 = kP0Default;
    const double p1 = kP1Default;

    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {0.6, 1.0, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_zbl_context_explicit(elem, rcut, a0, s0, p0, p1);
            D2_ZBL d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok =
                d.calc_dXijdri_dtheta("TYPE2B", arg_pos,
                                      Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            double a0_p = a0, s0_p = s0, p0_p = p0, p1_p = p1;
            double a0_m = a0, s0_m = s0, p0_m = p0, p1_m = p1;
            switch (arg_pos) {
                case 2: a0_p += h; a0_m -= h; break;
                case 3: s0_p += h; s0_m -= h; break;
                case 4: p0_p += h; p0_m -= h; break;
                case 5: p1_p += h; p1_m -= h; break;
                default: FAIL("unexpected arg_pos");
            }

            auto fdp = eval_fd_radial(elem, rcut, a0_p, s0_p, p0_p, p1_p, Zi, Zj, rij);
            auto fdm = eval_fd_radial(elem, rcut, a0_m, s0_m, p0_m, p1_m, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_ZBL force elem=" << elem << " arg_pos=" << arg_pos
                     << " rcut=" << rcut << " r=" << rij
                     << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(k, 0) - fd) < abs_tol);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical d(fd)/d a0 (TYPE2B arg_pos=2) matches central difference",
          "[MODELS][d2_zbl][dtheta][force][parity]")
{
    // a0 enters multiplicatively in a; FD on the radial-force chain
    // amplifies the dphi-singularity sensitivity. Tol 1e-3 — looser
    // than the energy side because d²phi appears in the closed form
    // and r^-2 enters in the fd-radial chain.
    check_one_theta_force(/*arg_pos=*/2, /*h=*/1e-6, /*abs_tol=*/1e-3,
                          kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical d(fd)/d s0 (TYPE2B arg_pos=3) matches central difference",
          "[MODELS][d2_zbl][dtheta][force][parity]")
{
    check_one_theta_force(/*arg_pos=*/3, /*h=*/1e-6, /*abs_tol=*/1e-3,
                          kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical d(fd)/d p0 (TYPE2B arg_pos=4) matches central difference",
          "[MODELS][d2_zbl][dtheta][force][parity]")
{
    // ln(Z) chain factor — looser tol.
    check_one_theta_force(/*arg_pos=*/4, /*h=*/1e-6, /*abs_tol=*/1e-2,
                          kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical d(fd)/d p1 (TYPE2B arg_pos=5) matches central difference",
          "[MODELS][d2_zbl][dtheta][force][parity]")
{
    check_one_theta_force(/*arg_pos=*/5, /*h=*/1e-6, /*abs_tol=*/1e-2,
                          kZSi, kZSi, "Si");
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL force d(fd)/d theta vanishes for Z=1 (ln(1)=0)",
          "[MODELS][d2_zbl][dtheta][force]")
{
    Context c = make_zbl_context_explicit(
        "H", 3.5, kA0Default, kS0Default, kP0Default, kP1Default);
    D2_ZBL d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    const int    Zi = 1, Zj = 1;
    const double rij = 1.0;

    for (int arg_pos : {4, 5}) {
        tadah::core::fd_type dfd(d.size());
        const bool ok =
            d.calc_dXijdri_dtheta("TYPE2B", arg_pos,
                                  Zi, Zj, rij, rij * rij, dfd);
        REQUIRE(ok);
        for (std::size_t k = 0; k < d.size(); ++k) {
            INFO("H-H arg_pos=" << arg_pos << " k=" << k
                 << " analytical=" << dfd(k, 0));
            CHECK(std::abs(dfd(k, 0)) < 1e-12);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL force d(fd)/d RCUT2B matches central difference",
          "[MODELS][d2_zbl][dtheta][force][parity]")
{
    const std::string elem = "Si";
    const int Zi = kZSi, Zj = kZSi;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {0.6, 1.0, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_zbl_context_explicit(elem, rcut, kA0Default,
                                                  kS0Default, kP0Default, kP1Default);
            D2_ZBL d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok =
                d.calc_dXijdri_dtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            const double h = 1e-6;
            auto fdp = eval_fd_radial(elem, rcut + h, kA0Default, kS0Default,
                                      kP0Default, kP1Default, Zi, Zj, rij);
            auto fdm = eval_fd_radial(elem, rcut - h, kA0Default, kS0Default,
                                      kP0Default, kP1Default, Zi, Zj, rij);
            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_ZBL rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                CHECK(std::abs(dfd(k, 0) - fd) < 1e-3);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL TYPE2B arg_pos out-of-range declines gracefully",
          "[MODELS][d2_zbl][dtheta]")
{
    Context c = make_zbl_context_explicit(
        "H", 3.5, kA0Default, kS0Default, kP0Default, kP1Default);
    D2_ZBL d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    // arg_pos = 1 (would be the "D2_ZBL" string token in OPTIM context)
    // and arg_pos = 6 (past p1) must both decline so HYBRID can FD-fall.
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B", 1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B", 6, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B", 0, 1, 1, 1.2, 1.44, dD));
}
