#include "catch2/catch.hpp"
#include <tadah/models/descriptors/dm/radial_policies.h>
#include <tadah/models/descriptors/d_basis_functions.h>

#include <cmath>
#include <vector>

using namespace tadah::models;

// -----------------------------------------------------------------------
//  BlipRadial
// -----------------------------------------------------------------------

TEST_CASE("BlipRadial::eval — values match B(x_c, fc)", "[radial_policies]")
{
    const std::vector<double> sg = {1.0, 2.0, 0.5};
    const std::vector<double> cg = {1.0, 1.5, 2.0};
    const double r = 1.2, fc = 0.9;
    const std::size_t n = sg.size();

    double g[3];
    BlipRadial::eval(r, fc, sg, cg, g, n);

    for (std::size_t i = 0; i < n; ++i) {
        const double x = sg[i] * (r - cg[i]);
        REQUIRE(g[i] == Approx(B(x, fc)));
    }
}

TEST_CASE("BlipRadial::eval — derivative: g consistent, dg matches dB", "[radial_policies]")
{
    const std::vector<double> sg = {1.0, 2.0, 0.5};
    const std::vector<double> cg = {1.0, 1.5, 2.0};
    const double r = 1.2, fc = 0.9, fcp = -0.3;
    const std::size_t n = sg.size();

    double g[3], dg[3], g0[3];
    BlipRadial::eval(r, fc, fcp, sg, cg, g, dg, n);
    BlipRadial::eval(r, fc,      sg, cg, g0,    n);

    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(g[i] == Approx(g0[i]));                         // g consistent
        const double x = sg[i] * (r - cg[i]);
        REQUIRE(dg[i] == Approx(dB(x, sg[i], fc, fcp)));       // dg matches dB
    }
}

TEST_CASE("BlipRadial: dg matches central-difference FD of g", "[radial_policies]")
{
    // Use fc=1, fcp=0 so the derivative is purely from the blip function.
    // The dg formula is: dB(x, s, fc, fcp) = B'(x)*s*fc + B(x)*fcp.
    // With fc=1, fcp=0: dg = B'(x)*s, which matches FD of B(x)*1 w.r.t. r.
    const std::vector<double> sg = {1.0, 2.0, 0.5};
    const std::vector<double> cg = {1.0, 1.5, 2.0};
    const double fc = 1.0, fcp = 0.0;
    const double r = 1.2, h = 1e-8;
    const std::size_t n = sg.size();

    double gp[3], gm[3], g[3], dg[3];
    BlipRadial::eval(r + h, fc, sg, cg, gp, n);
    BlipRadial::eval(r - h, fc, sg, cg, gm, n);
    BlipRadial::eval(r, fc, fcp, sg, cg, g, dg, n);

    for (std::size_t i = 0; i < n; ++i)
        REQUIRE(dg[i] == Approx((gp[i] - gm[i]) / (2.0 * h)).margin(1e-5));
}

// -----------------------------------------------------------------------
//  GaussianRadial
// -----------------------------------------------------------------------

TEST_CASE("GaussianRadial::eval — values match G(r, eta, miu, fc)", "[radial_policies]")
{
    const std::vector<double> sg = {0.1, 0.5, 1.0};
    const std::vector<double> cg = {1.0, 2.0, 3.0};
    const double r = 2.5, fc = 0.8;
    const std::size_t n = sg.size();

    double g[3];
    GaussianRadial::eval(r, fc, sg, cg, g, n);

    for (std::size_t i = 0; i < n; ++i)
        REQUIRE(g[i] == Approx(G(r, sg[i], cg[i], fc)));
}

TEST_CASE("GaussianRadial::eval — derivative: g consistent, dg matches dG", "[radial_policies]")
{
    const std::vector<double> sg = {0.1, 0.5, 1.0};
    const std::vector<double> cg = {1.0, 2.0, 3.0};
    const double r = 2.5, fc = 0.8, fcp = -0.4;
    const std::size_t n = sg.size();

    double g[3], dg[3], g0[3];
    GaussianRadial::eval(r, fc, fcp, sg, cg, g, dg, n);
    GaussianRadial::eval(r, fc,      sg, cg, g0,    n);

    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(g[i] == Approx(g0[i]));
        REQUIRE(dg[i] == Approx(dG(r, sg[i], cg[i], fc, fcp)));
    }
}

TEST_CASE("GaussianRadial: dg matches central-difference FD of g", "[radial_policies]")
{
    // Use fc=1, fcp=0 so the derivative is purely from the Gaussian function.
    // The dg formula is: exp(-eta*(r-miu)^2)*(-2*eta*(r-miu)*fc + fcp).
    // With fc=1, fcp=0: dg = dG/dr (no cutoff cross-term), matching FD of G*1.
    const std::vector<double> sg = {0.1, 0.5, 1.0};
    const std::vector<double> cg = {1.0, 2.0, 3.0};
    const double fc = 1.0, fcp = 0.0;
    const double r = 2.5, h = 1e-8;
    const std::size_t n = sg.size();

    double gp[3], gm[3], g[3], dg[3];
    GaussianRadial::eval(r + h, fc, sg, cg, gp, n);
    GaussianRadial::eval(r - h, fc, sg, cg, gm, n);
    GaussianRadial::eval(r, fc, fcp, sg, cg, g, dg, n);

    for (std::size_t i = 0; i < n; ++i)
        REQUIRE(dg[i] == Approx((gp[i] - gm[i]) / (2.0 * h)).margin(1e-5));
}
