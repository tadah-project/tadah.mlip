// test_cut_drcut.cpp
//
// Layer-1 parity tests for the closed-form analytical derivative
// d f_c(r) / d rcut, at fixed r, for every smooth cutoff that overrides
// Cut_Base::calc_drcut. The reference is a central finite difference
// constructed from two freshly-built cutoff instances at rcut +/- h.
//
// Cut_Cos and Cut_Dummy are covered separately in test_d2_bp_dtheta.cpp;
// this file covers the rest of the family.

#include "catch2/catch.hpp"

#include <tadah/models/cutoffs.h>

#include <cmath>

namespace {

// Central FD reference. Build two fresh cutoffs at rcut +/- h so that
// any rcut-dependent precomputed state (rcut_inv, rcut_inner, norm) is
// rebuilt — never poke set_rcut on a single instance.
template <class Cut>
double fd_drcut(double rcut, double r, double h) {
    Cut fcp(rcut + h);
    Cut fcm(rcut - h);
    return (fcp.calc(r) - fcm.calc(r)) / (2.0 * h);
}

// Same idea for the mixed second derivative d(df_c/dr)/d rcut: FD
// reference differentiates calc_prime in rcut at fixed r.
template <class Cut>
double fd_prime_drcut(double rcut, double r, double h) {
    Cut fcp(rcut + h);
    Cut fcm(rcut - h);
    return (fcp.calc_prime(r) - fcm.calc_prime(r)) / (2.0 * h);
}

}  // namespace

TEST_CASE("Cut_Tanh calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_Tanh;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_Tanh fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_Tanh>(rcut, r, h);
            INFO("Cut_Tanh rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_Poly calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_Poly;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_Poly fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_Poly>(rcut, r, h);
            INFO("Cut_Poly rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_PolyS calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_PolyS;
    const double h = 1e-6;
    // Smooth band is (rcut - 1, rcut). Pick probes well inside so the
    // FD perturbation does not straddle either boundary.
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            Cut_PolyS fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_PolyS>(rcut, r, h);
            INFO("Cut_PolyS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_PolyS calc_drcut returns 0 outside the smooth band",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_PolyS;
    Cut_PolyS fc(3.5);
    CHECK(fc.calc_drcut(1.0) == 0.0);   // r < rcut_inner = 2.5 → constant 1
    CHECK(fc.calc_drcut(2.0) == 0.0);
    CHECK(fc.calc_drcut(4.0) == 0.0);   // r > rcut → 0
}

TEST_CASE("Cut_CosS calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_CosS;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            Cut_CosS fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_CosS>(rcut, r, h);
            INFO("Cut_CosS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_CosS calc_drcut returns 0 outside the smooth band",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_CosS;
    Cut_CosS fc(3.5);
    CHECK(fc.calc_drcut(1.0) == 0.0);
    CHECK(fc.calc_drcut(2.0) == 0.0);
    CHECK(fc.calc_drcut(4.0) == 0.0);
}

TEST_CASE("Cut_PT calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_PT;
    // Cut_PT denominators (r - rcut)^2 blow up near rcut; stay strictly
    // inside. Avoid r = 0 too: rcut_inner = -rcut/2 so (r - rcut_inner)
    // is bounded away from zero for r > 0.
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double frac : {0.3, 0.5, 0.7, 0.85}) {
            const double r = rcut * frac;
            Cut_PT fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_PT>(rcut, r, h);
            INFO("Cut_PT rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_SinInv calc_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_SinInv;
    const double h = 1e-6;
    // For r >= rcut the function is constant (= 1) so FD is well-defined
    // and equal to 0; analytical also returns 0. Probe both sides except
    // exactly at r = rcut where the +h side flips piece (calc returns
    // sin instead of 1) and FD becomes O(1).
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_SinInv fc(rcut);
            const double an = fc.calc_drcut(r);
            const double fd = fd_drcut<Cut_SinInv>(rcut, r, h);
            INFO("Cut_SinInv rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_SinInv calc_drcut returns 0 above rcut",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_SinInv;
    Cut_SinInv fc(3.0);
    CHECK(fc.calc_drcut(3.0) == 0.0);
    CHECK(fc.calc_drcut(5.0) == 0.0);
}

TEST_CASE("Cut_Cos calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_Cos;
    const double h = 1e-6;
    // Mixed derivative d(df_c/dr)/d rcut. Probe strictly inside (0, rcut)
    // — at r = rcut the +h side enters the constant-zero piece of
    // calc_prime so FD bombs out (one-sided derivative discontinuity).
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_Cos fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_Cos>(rcut, r, h);
            INFO("Cut_Cos rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_Cos calc_prime_drcut returns 0 at or above rcut",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_Cos;
    Cut_Cos fc(3.0);
    CHECK(fc.calc_prime_drcut(3.0) == 0.0);
    CHECK(fc.calc_prime_drcut(5.0) == 0.0);
}

TEST_CASE("Cut_Base default calc_prime_drcut is zero",
          "[MODELS][cutoffs][drcut]")
{
    // Cut_Dummy inherits the default base impl (returns 0).
    using tadah::models::Cut_Dummy;
    Cut_Dummy fc(3.0);
    CHECK(fc.calc_prime_drcut(0.5) == 0.0);
    CHECK(fc.calc_prime_drcut(1.5) == 0.0);
    CHECK(fc.calc_prime_drcut(2.9) == 0.0);
}

TEST_CASE("Cut_Tanh calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_Tanh;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_Tanh fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_Tanh>(rcut, r, h);
            INFO("Cut_Tanh rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_Poly calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_Poly;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_Poly fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_Poly>(rcut, r, h);
            INFO("Cut_Poly rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_PolyS calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_PolyS;
    const double h = 1e-6;
    // Smooth band is (rcut - 1, rcut). Pick probes well inside so the
    // FD perturbation does not straddle either boundary.
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            Cut_PolyS fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_PolyS>(rcut, r, h);
            INFO("Cut_PolyS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_PolyS calc_prime_drcut returns 0 outside the smooth band",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_PolyS;
    Cut_PolyS fc(3.5);
    CHECK(fc.calc_prime_drcut(1.0) == 0.0);   // r < rcut_inner = 2.5
    CHECK(fc.calc_prime_drcut(2.0) == 0.0);
    CHECK(fc.calc_prime_drcut(4.0) == 0.0);   // r > rcut
}

TEST_CASE("Cut_CosS calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_CosS;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            Cut_CosS fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_CosS>(rcut, r, h);
            INFO("Cut_CosS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_CosS calc_prime_drcut returns 0 outside the smooth band",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_CosS;
    Cut_CosS fc(3.5);
    CHECK(fc.calc_prime_drcut(1.0) == 0.0);
    CHECK(fc.calc_prime_drcut(2.0) == 0.0);
    CHECK(fc.calc_prime_drcut(4.0) == 0.0);
}

TEST_CASE("Cut_PT calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_PT;
    // Same caveat as calc_drcut: stay strictly inside (0, rcut) so the
    // (r - rcut)^2 denominator does not blow up FD.
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double frac : {0.3, 0.5, 0.7, 0.85}) {
            const double r = rcut * frac;
            Cut_PT fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_PT>(rcut, r, h);
            INFO("Cut_PT rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_SinInv calc_prime_drcut matches central difference",
          "[MODELS][cutoffs][drcut][parity]")
{
    using tadah::models::Cut_SinInv;
    const double h = 1e-6;
    // For r >= rcut both pieces give 0, but FD straddles the f' jump
    // (smooth-side limit at rcut is 0 too — calc_prime is continuous —
    // but its rcut-derivative is not). Probe strictly below rcut.
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut) continue;
            Cut_SinInv fc(rcut);
            const double an = fc.calc_prime_drcut(r);
            const double fd = fd_prime_drcut<Cut_SinInv>(rcut, r, h);
            INFO("Cut_SinInv rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-6);
        }
    }
}

TEST_CASE("Cut_SinInv calc_prime_drcut returns 0 above rcut",
          "[MODELS][cutoffs][drcut]")
{
    using tadah::models::Cut_SinInv;
    Cut_SinInv fc(3.0);
    CHECK(fc.calc_prime_drcut(3.0) == 0.0);
    CHECK(fc.calc_prime_drcut(5.0) == 0.0);
}

/* ------------------------------------------------------------------- *
 *  Layer-1 parity tests for calc_double_prime — d^2 f_c / dr^2 at fixed
 *  rcut, central-FD reference on calc_prime. Required by F_mEnv::calc_d2F
 *  to close the radial-θ force chain for DM_mEAD<F_mEnv> configs. Same FD
 *  harness as fd_drcut: build a fresh cutoff so
 *  any rcut-dependent precomputed state is reset. The reference here
 *  perturbs r (not rcut), so a single instance is fine.
 * ------------------------------------------------------------------- */

namespace {

template <class Cut>
double fd_double_prime(double rcut, double r, double h) {
    Cut fc(rcut);
    return (fc.calc_prime(r + h) - fc.calc_prime(r - h)) / (2.0 * h);
}

}  // namespace

TEST_CASE("Cut_Cos calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_Cos;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut - h) continue;
            Cut_Cos fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_Cos>(rcut, r, h);
            INFO("Cut_Cos rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_Tanh calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_Tanh;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut - h) continue;
            Cut_Tanh fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_Tanh>(rcut, r, h);
            INFO("Cut_Tanh rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_Poly calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_Poly;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut - h) continue;
            Cut_Poly fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_Poly>(rcut, r, h);
            INFO("Cut_Poly rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_PolyS calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_PolyS;
    const double h = 1e-6;
    /* Smooth band is (rcut - 1, rcut). FD perturbs r ± h; pick probes
     * well inside so the perturbation does not straddle either boundary. */
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            if (r <= rcut - 1.0 + h || r >= rcut - h) continue;
            Cut_PolyS fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_PolyS>(rcut, r, h);
            INFO("Cut_PolyS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_CosS calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_CosS;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double offset : {0.1, 0.3, 0.5, 0.9}) {
            const double r = rcut - offset;
            if (r <= rcut - 1.0 + h || r >= rcut - h) continue;
            Cut_CosS fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_CosS>(rcut, r, h);
            INFO("Cut_CosS rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_PT calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_PT;
    const double h = 1e-6;
    /* Cut_PT has stiff tails near r=rcut where calc_prime grows fast;
     * sample the central plateau where curvature is well-behaved. */
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double frac : {0.2, 0.4, 0.6, 0.8}) {
            const double r = rcut * frac;
            if (r >= rcut - h) continue;
            Cut_PT fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_PT>(rcut, r, h);
            INFO("Cut_PT rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            /* PT is steeper than the polynomial cutoffs; relax tolerance
             * slightly. */
            CHECK(std::abs(an - fd) < std::max(1e-5, std::abs(fd) * 1e-5));
        }
    }
}

TEST_CASE("Cut_SinInv calc_double_prime matches central difference",
          "[MODELS][cutoffs][double_prime][parity]")
{
    using tadah::models::Cut_SinInv;
    const double h = 1e-6;
    for (double rcut : {2.5, 3.5, 5.0}) {
        for (double r : {0.5, 1.0, 2.0, rcut * 0.9}) {
            if (r >= rcut - h) continue;
            Cut_SinInv fc(rcut);
            const double an = fc.calc_double_prime(r);
            const double fd = fd_double_prime<Cut_SinInv>(rcut, r, h);
            INFO("Cut_SinInv rcut=" << rcut << " r=" << r
                 << " analytical=" << an << " FD=" << fd);
            CHECK(std::abs(an - fd) < 1e-5);
        }
    }
}

TEST_CASE("Cut_SinInv calc_double_prime returns 0 above rcut",
          "[MODELS][cutoffs][double_prime]")
{
    using tadah::models::Cut_SinInv;
    Cut_SinInv fc(3.0);
    CHECK(fc.calc_double_prime(3.0) == 0.0);
    CHECK(fc.calc_double_prime(5.0) == 0.0);
}

TEST_CASE("Cut_Dummy calc_double_prime is 0 by inherited default",
          "[MODELS][cutoffs][double_prime]")
{
    using tadah::models::Cut_Dummy;
    Cut_Dummy fc(3.0);
    CHECK(fc.calc_double_prime(0.5) == 0.0);
    CHECK(fc.calc_double_prime(2.0) == 0.0);
    CHECK(fc.calc_double_prime(5.0) == 0.0);
}
