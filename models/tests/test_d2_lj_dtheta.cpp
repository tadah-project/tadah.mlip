// test_d2_lj_dtheta.cpp
//
// Layer-1 parity test for D2_LJ::calc_dDdtheta. Only RCUT2B is on the
// HPO surface (sigma/epsilon are post-fit reconstructions from the
// linear C_6/C_12 weights, not descriptor params). Analytical
// derivative is compared against central FD on the cutoff.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_lj.h>

#include <cmath>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_LJ;

struct TestFixture { TestFixture() { PeriodicTable::initialize(); } };

Context make_lj_context(double rcut) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_LJ");
    c.add("TYPE2B", "H");
    c.add("TYPE2B", "H");
    return c;
}

aed_type eval_lj_aed(double rcut, int Zi, int Zj, double rij) {
    Context c = make_lj_context(rcut);
    D2_LJ d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

std::vector<double> eval_lj_fd_radial(double rcut, int Zi, int Zj, double rij) {
    Context c = make_lj_context(rcut);
    D2_LJ d(c);
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
          "D2_LJ analytical dD/d RCUT2B matches central difference",
          "[MODELS][d2_lj][dtheta][parity]")
{
    const int    Zi = 1, Zj = 1;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {1.0, 1.5, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_lj_context(rcut);
            D2_LJ d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size());
            dD.set_zero();
            const bool ok = d.calc_dDdtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            const double h = 1e-6;
            aed_type aed_p = eval_lj_aed(rcut + h, Zi, Zj, rij);
            aed_type aed_m = eval_lj_aed(rcut - h, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
                INFO("D2_LJ rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                CHECK(std::abs(dD[k] - fd) < 1e-5);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_LJ analytical d(fd)/d RCUT2B matches central difference",
          "[MODELS][d2_lj][dtheta][force][parity]")
{
    const int    Zi = 1, Zj = 1;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {1.0, 1.5, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_lj_context(rcut);
            D2_LJ d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            tadah::core::fd_type dfd(d.size());
            const bool ok = d.calc_dXijdri_dtheta("RCUT2B", -1,
                                                  Zi, Zj, rij, rij * rij, dfd);
            REQUIRE(ok);

            const double h = 1e-6;
            auto fdp = eval_lj_fd_radial(rcut + h, Zi, Zj, rij);
            auto fdm = eval_lj_fd_radial(rcut - h, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (fdp[k] - fdm[k]) / (2.0 * h);
                INFO("D2_LJ rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dfd(k, 0) << " FD=" << fd);
                // r^-13 amplifies FD truncation at small r; tolerance is
                // looser than the energy-side test.
                CHECK(std::abs(dfd(k, 0) - fd) < 1e-4);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_LJ calc_dXijdri_dtheta declines unsupported keys",
          "[MODELS][d2_lj][dtheta][force]")
{
    Context c = make_lj_context(3.5);
    D2_LJ d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    tadah::core::fd_type dfd(d.size());
    CHECK_FALSE(d.calc_dXijdri_dtheta("CGRID2B",  1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("SGRID2B",  1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dfd));
    CHECK_FALSE(d.calc_dXijdri_dtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dfd));
}

TEST_CASE_METHOD(TestFixture,
          "D2_LJ calc_dDdtheta declines unsupported keys",
          "[MODELS][d2_lj][dtheta]")
{
    Context c = make_lj_context(3.5);
    D2_LJ d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    CHECK_FALSE(d.calc_dDdtheta("CGRID2B",  1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("SGRID2B",  1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dD));
}
