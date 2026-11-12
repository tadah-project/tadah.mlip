// test_d2_zbl_dtheta.cpp
//
// Layer-1 parity test for D2_ZBL::calc_dDdtheta. Only RCUT2B is on the
// HPO surface today (a0/s0/p0/p1 are baked into TYPE2B and not in the
// meta()). Analytical derivative is compared against central FD on
// the cutoff.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/d2/d2_zbl.h>

#include <cmath>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::models::Cut_Cos;
using tadah::models::D2_ZBL;

struct TestFixture { TestFixture() { PeriodicTable::initialize(); } };

Context make_zbl_context(double rcut) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INIT2B", "true");
    c.add("VERBOSE", 0);
    c.add("RCUT2B", rcut);
    c.add("TYPE2B", "D2_ZBL");
    c.add("TYPE2B", -1);   // a0 default
    c.add("TYPE2B", -1);   // s0 default
    c.add("TYPE2B", -1);   // p0 default
    c.add("TYPE2B", -1);   // p1 default
    c.add("TYPE2B", "H");
    c.add("TYPE2B", "H");
    return c;
}

aed_type eval_zbl_aed(double rcut, int Zi, int Zj, double rij) {
    Context c = make_zbl_context(rcut);
    D2_ZBL d(c);
    Cut_Cos fc(rcut);
    d.set_fcut(&fc, false);
    aed_type aed(d.size());
    d.calc_aed(Zi, Zj, rij, rij * rij, aed);
    return aed;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL analytical dD/d RCUT2B matches central difference",
          "[MODELS][d2_zbl][dtheta][parity]")
{
    const int    Zi = 1, Zj = 1;
    for (const double rcut : {3.0, 3.5, 5.0}) {
        for (const double rij : {0.5, 1.0, rcut * 0.9}) {
            if (rij >= rcut) continue;

            Context c = make_zbl_context(rcut);
            D2_ZBL d(c);
            Cut_Cos fc(rcut);
            d.set_fcut(&fc, false);

            aed_type dD(d.size());
            dD.set_zero();
            const bool ok = d.calc_dDdtheta("RCUT2B", -1, Zi, Zj, rij, rij * rij, dD);
            REQUIRE(ok);

            const double h = 1e-6;
            aed_type aed_p = eval_zbl_aed(rcut + h, Zi, Zj, rij);
            aed_type aed_m = eval_zbl_aed(rcut - h, Zi, Zj, rij);

            for (std::size_t k = 0; k < d.size(); ++k) {
                const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
                INFO("D2_ZBL rcut=" << rcut << " r=" << rij << " k=" << k
                     << " analytical=" << dD[k] << " FD=" << fd);
                // Looser tol than D2_LJ: the ZBL pair energy has very
                // steep features near r=0 so FD truncation is larger.
                CHECK(std::abs(dD[k] - fd) < 1e-4);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "D2_ZBL calc_dDdtheta declines unsupported keys",
          "[MODELS][d2_zbl][dtheta]")
{
    Context c = make_zbl_context(3.5);
    D2_ZBL d(c);
    Cut_Cos fc(3.5);
    d.set_fcut(&fc, false);

    aed_type dD(d.size()); dD.set_zero();
    CHECK_FALSE(d.calc_dDdtheta("TYPE2B",   1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("RCUT_MB", -1, 1, 1, 1.2, 1.44, dD));
    CHECK_FALSE(d.calc_dDdtheta("BOGUS",   -1, 1, 1, 1.2, 1.44, dD));
}
