// test_kern_dtheta.cpp
//
// Layer-1 parity tests for the closed-form analytical derivatives of
// Kern_RBF (sigma), Kern_Polynomial (gamma, c), and Kern_Sigmoid
// (gamma, c) with respect to their continuous hyperparameters.
//
// Parallel to test_d2_bp_dtheta.cpp / test_d2_blip_dtheta.cpp on the
// kernel arm. For each kernel and each (theta_name, arg_pos) pair the
// analytical dK / d theta is compared against a central-difference
// reference obtained by rebuilding the kernel with a perturbed Context.
//
// Kern_Quadratic / Kern_LQ / Kern_Linear have no continuous theta;
// covered separately by a returns-false sanity check.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/functions/kernels/kern_lq.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/functions/kernels/kern_polynomial.h>
#include <tadah/models/functions/kernels/kern_quadratic.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/functions/kernels/kern_sigmoid.h>

#include <cmath>
#include <sstream>

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::models::Kern_Linear;
using tadah::models::Kern_LQ;
using tadah::models::Kern_Polynomial;
using tadah::models::Kern_Quadratic;
using tadah::models::Kern_RBF;
using tadah::models::Kern_Sigmoid;

namespace {

// Two synthetic feature vectors with non-trivial dot product and
// non-zero ||b - af||^2.
aed_type make_b() {
    aed_type v(3);
    v(0) = 0.30; v(1) = -0.15; v(2) = 0.05;
    return v;
}
aed_type make_af() {
    aed_type v(3);
    v(0) = 0.10; v(1) = 0.20; v(2) = -0.07;
    return v;
}

}  // namespace

// ---------------------------------------------------------------------
// Kern_RBF: K = exp(-||b - af||^2 / (2 sigma^2))
// dK/d sigma = K * ||b - af||^2 / sigma^3
// ---------------------------------------------------------------------
TEST_CASE("Kern_RBF analytical dK/d sigma matches central difference",
          "[MODELS][kernels][dtheta][parity]") {
    const double sigma = 0.30;
    Context c;
    c.add("MPARAMS", sigma);

    Kern_RBF kern(c);
    const aed_type b  = make_b();
    const aed_type af = make_af();

    double dK = 0.0;
    REQUIRE(kern.calc_dKdtheta("MPARAMS", 1, b, af, dK));

    const double h = 1e-6;
    Context cp; cp.add("MPARAMS", sigma + h);
    Context cm; cm.add("MPARAMS", sigma - h);
    Kern_RBF kp(cp);
    Kern_RBF km(cm);
    const double fd = (kp(b, af) - km(b, af)) / (2.0 * h);

    INFO("sigma=" << sigma << " analytical=" << dK << " FD=" << fd);
    CHECK(std::abs(dK - fd) < 1e-6);

    // Unsupported keys / arg_pos must decline.
    double junk = 0.0;
    CHECK_FALSE(kern.calc_dKdtheta("MPARAMS", 0, b, af, junk));
    CHECK_FALSE(kern.calc_dKdtheta("MPARAMS", 2, b, af, junk));
    CHECK_FALSE(kern.calc_dKdtheta("BOGUS",   1, b, af, junk));
}

// ---------------------------------------------------------------------
// Kern_Polynomial: K = (gamma (b . af) + c)^d
// dK/d gamma = d (b . af) (gamma (b . af) + c)^(d-1)
// dK/d c     = d         (gamma (b . af) + c)^(d-1)
// ---------------------------------------------------------------------
TEST_CASE("Kern_Polynomial analytical dK/d {gamma,c} matches central difference",
          "[MODELS][kernels][dtheta][parity]") {
    const int    d = 3;
    const double gamma = 1.123;
    const double cc    = 0.15273;
    Context c;
    c.add("MPARAMS", d);
    c.add("MPARAMS", gamma);
    c.add("MPARAMS", cc);

    Kern_Polynomial kern(c);
    const aed_type b  = make_b();
    const aed_type af = make_af();

    // dK/d gamma at MPARAMS arg_pos=2
    {
        double dK = 0.0;
        REQUIRE(kern.calc_dKdtheta("MPARAMS", 2, b, af, dK));

        const double h = 1e-6;
        Context cp; cp.add("MPARAMS", d); cp.add("MPARAMS", gamma + h); cp.add("MPARAMS", cc);
        Context cm; cm.add("MPARAMS", d); cm.add("MPARAMS", gamma - h); cm.add("MPARAMS", cc);
        Kern_Polynomial kp(cp);
        Kern_Polynomial km(cm);
        const double fd = (kp(b, af) - km(b, af)) / (2.0 * h);

        INFO("gamma analytical=" << dK << " FD=" << fd);
        CHECK(std::abs(dK - fd) < 1e-6);
    }

    // dK/d c at MPARAMS arg_pos=3
    {
        double dK = 0.0;
        REQUIRE(kern.calc_dKdtheta("MPARAMS", 3, b, af, dK));

        const double h = 1e-6;
        Context cp; cp.add("MPARAMS", d); cp.add("MPARAMS", gamma); cp.add("MPARAMS", cc + h);
        Context cm; cm.add("MPARAMS", d); cm.add("MPARAMS", gamma); cm.add("MPARAMS", cc - h);
        Kern_Polynomial kp(cp);
        Kern_Polynomial km(cm);
        const double fd = (kp(b, af) - km(b, af)) / (2.0 * h);

        INFO("c analytical=" << dK << " FD=" << fd);
        CHECK(std::abs(dK - fd) < 1e-6);
    }

    // d is integer (arg_pos=1) and not HPO-tunable: must decline.
    double junk = 0.0;
    CHECK_FALSE(kern.calc_dKdtheta("MPARAMS", 1, b, af, junk));
    CHECK_FALSE(kern.calc_dKdtheta("MPARAMS", 4, b, af, junk));
    CHECK_FALSE(kern.calc_dKdtheta("BOGUS",   2, b, af, junk));
}

// ---------------------------------------------------------------------
// Kern_Sigmoid: K = tanh(gamma (b . af) + c)
// dK/d gamma = (1 - K^2) (b . af)
// dK/d c     = (1 - K^2)
// ---------------------------------------------------------------------
TEST_CASE("Kern_Sigmoid analytical dK/d {gamma,c} matches central difference",
          "[MODELS][kernels][dtheta][parity]") {
    const double gamma = 1.5;
    const double cc    = -0.2;
    Context c;
    c.add("MPARAMS", gamma);
    c.add("MPARAMS", cc);

    Kern_Sigmoid kern(c);
    const aed_type b  = make_b();
    const aed_type af = make_af();

    // dK/d gamma at MPARAMS arg_pos=1
    {
        double dK = 0.0;
        REQUIRE(kern.calc_dKdtheta("MPARAMS", 1, b, af, dK));

        const double h = 1e-6;
        Context cp; cp.add("MPARAMS", gamma + h); cp.add("MPARAMS", cc);
        Context cm; cm.add("MPARAMS", gamma - h); cm.add("MPARAMS", cc);
        Kern_Sigmoid kp(cp);
        Kern_Sigmoid km(cm);
        const double fd = (kp(b, af) - km(b, af)) / (2.0 * h);

        INFO("gamma analytical=" << dK << " FD=" << fd);
        CHECK(std::abs(dK - fd) < 1e-6);
    }

    // dK/d c at MPARAMS arg_pos=2
    {
        double dK = 0.0;
        REQUIRE(kern.calc_dKdtheta("MPARAMS", 2, b, af, dK));

        const double h = 1e-6;
        Context cp; cp.add("MPARAMS", gamma); cp.add("MPARAMS", cc + h);
        Context cm; cm.add("MPARAMS", gamma); cm.add("MPARAMS", cc - h);
        Kern_Sigmoid kp(cp);
        Kern_Sigmoid km(cm);
        const double fd = (kp(b, af) - km(b, af)) / (2.0 * h);

        INFO("c analytical=" << dK << " FD=" << fd);
        CHECK(std::abs(dK - fd) < 1e-6);
    }

    double junk = 0.0;
    CHECK_FALSE(kern.calc_dKdtheta("MPARAMS", 3, b, af, junk));
    CHECK_FALSE(kern.calc_dKdtheta("BOGUS",   1, b, af, junk));
}

// ---------------------------------------------------------------------
// Kernels with no continuous theta: must decline every query.
// ---------------------------------------------------------------------
TEST_CASE("Kern_Linear / Kern_Quadratic / Kern_LQ have no continuous theta",
          "[MODELS][kernels][dtheta]") {
    const aed_type b  = make_b();
    const aed_type af = make_af();
    double junk = 0.0;

    Kern_Linear    klin;
    Kern_Quadratic kquad;
    Kern_LQ        klq;

    for (auto* k : std::vector<tadah::models::Kern_Base*>{&klin, &kquad, &klq}) {
        CHECK_FALSE(k->calc_dKdtheta("MPARAMS", 1, b, af, junk));
        CHECK_FALSE(k->calc_dKdtheta("MPARAMS", 2, b, af, junk));
        CHECK_FALSE(k->calc_dKdtheta("BOGUS",   1, b, af, junk));
    }
}
