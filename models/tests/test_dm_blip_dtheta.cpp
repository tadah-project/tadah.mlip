// test_dm_blip_dtheta.cpp
//
// Layer-1 parity tests for DM_Blip closed-form theta-derivatives.
// Mirrors test_dm_ead_dtheta.cpp; the difference between EAD and Blip
// is only the radial basis function (Gaussian vs blip), so the
// test structure is identical and only the make-context type label
// changes. Test points are chosen so the blip support |x_c| < 2
// is exercised with margin away from the C^2-breaks at |x_c| = 1.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/dm_blip.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::PeriodicTable;
using tadah::core::rho_type;
using tadah::core::Vec3d;
using tadah::models::Cut_Cos;
using tadah::models::DM_Blip;

struct TestFixture {
    TestFixture() { PeriodicTable::initialize(); }
};

Context make_blip_mb_context(const std::vector<double>& cgrid,
                             const std::vector<double>& sgrid,
                             double rcut,
                             int    Lmax = 2)
{
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INITMB", "true");
    c.add("VERBOSE", 0);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sgrid.size(); ++i) {
            if (i) ss << " ";
            ss << sgrid[i];
        }
        c.add("SGRIDMB", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cgrid.size(); ++i) {
            if (i) ss << " ";
            ss << cgrid[i];
        }
        c.add("CGRIDMB", ss.str());
    }
    /* Setting RCUTMB triggers Context::updateInternalKeys which derives
     * RCUTMBMAX (= max of RCUTMB values) for the descriptor ctor. */
    c.add("RCUTMB", rcut);
    c.add("TYPEMB", "DM_Blip");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", static_cast<int>(cgrid.size()));
    c.add("TYPEMB", static_cast<int>(sgrid.size()));
    c.add("TYPEMB", "H");
    c.add("TYPEMB", "H");
    return c;
}

aed_type eval_blip_atom0_aed(const std::vector<double>& cgrid,
                             const std::vector<double>& sgrid,
                             double rcut,
                             const std::vector<Vec3d>& positions)
{
    Context c = make_blip_mb_context(cgrid, sgrid, rcut);
    DM_Blip dm(c);
    Cut_Cos fc(rcut);
    dm.set_fcut(&fc, false);

    rho_type rhoi;
    dm.init_rhoi(rhoi);
    rhoi.set_zero();

    Vec3d delij;
    for (std::size_t j = 1; j < positions.size(); ++j) {
        delij = positions[0] - positions[j];
        const double rij_sq = delij * delij;
        if (rij_sq >= rcut * rcut) continue;
        const double rij = std::sqrt(rij_sq);
        dm.calc_rho(1, 1, rij, rij_sq, delij, rhoi, /*scale=*/1.0);
    }

    aed_type aed(dm.size());
    aed.set_zero();
    dm.calc_aed(rhoi, aed);
    return aed;
}

aed_type analytical_daed(const Context& c,
                         const std::vector<Vec3d>& positions,
                         const std::string& theta_name,
                         int                arg_pos)
{
    DM_Blip dm(c);
    Cut_Cos fc(c.get<double>("RCUTMBMAX"));
    dm.set_fcut(&fc, false);

    rho_type rhoi, drhoi;
    dm.init_rhoi(rhoi);
    dm.init_rhoi(drhoi);
    rhoi.set_zero();
    drhoi.set_zero();

    const double rcut    = c.get<double>("RCUTMBMAX");
    const double rcut_sq = rcut * rcut;

    Vec3d delij;
    for (std::size_t j = 1; j < positions.size(); ++j) {
        delij = positions[0] - positions[j];
        const double rij_sq = delij * delij;
        if (rij_sq >= rcut_sq) continue;
        const double rij = std::sqrt(rij_sq);
        dm.calc_rho(1, 1, rij, rij_sq, delij, rhoi, /*scale=*/1.0);
        dm.calc_drhoi_dtheta(theta_name, arg_pos,
                             1, 1, rij, rij_sq, delij, drhoi, /*scale=*/1.0);
    }

    aed_type aed_scratch(dm.size());
    aed_scratch.set_zero();
    dm.calc_aed(rhoi, aed_scratch);

    aed_type daed(dm.size());
    daed.set_zero();
    dm.calc_daed_dtheta(theta_name, arg_pos, rhoi, drhoi, daed, /*scale=*/1.0);
    return daed;
}

// 4-atom H cluster with neighbours placed so x_c values are
// comfortably inside the blip support |x_c| < 2 and away from
// the C^2-break at |x_c| = 1 (within margin > 0.2 either side).
// With cgrid={0.7, 1.5} and sgrid={1.5, 0.8}:
//   pair (1, r12=1.6): x_c[0]=1.5*(1.6-0.7)=+1.35; x_c[1]=0.8*(1.6-1.5)=+0.08
//   pair (2, r13=1.4): x_c[0]=1.5*(1.4-0.7)=+1.05; x_c[1]=0.8*(1.4-1.5)=-0.08
//   pair (3, r14=2.1): x_c[0]=1.5*(2.1-0.7)=+2.10  (outside support!)
// Adjust neighbour 3 so x_c[0] stays in support: r14 such that x_c[0]<2:
//   1.5*(r-0.7) < 2 -> r < 2.03; r=1.9 keeps both components in support.
std::vector<Vec3d> make_blip_cluster() {
    return {
        Vec3d(0.0, 0.0, 0.0),
        Vec3d(1.6, 0.0, 0.0),
        Vec3d(0.0, 1.4, 0.0),
        Vec3d(0.0, 0.0, 1.9),
    };
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_Blip analytical dAED/d CGRIDMB matches central difference",
          "[MODELS][dm_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_blip_cluster();

    Context c = make_blip_mb_context(cgrid, sgrid, rcut, Lmax);

    for (std::size_t k = 0; k < cgrid.size(); ++k) {
        const int arg_pos = static_cast<int>(k + 1);
        const aed_type daed = analytical_daed(c, positions, "CGRIDMB", arg_pos);

        /* Use h slightly larger to keep FD error tractable in the
         * piecewise-cubic blip — same trade-off as test_d2_blip_dtheta. */
        const double h = 1e-5;
        auto cp = cgrid; cp[k] += h;
        auto cm = cgrid; cm[k] -= h;
        aed_type aed_p = eval_blip_atom0_aed(cp, sgrid, rcut, positions);
        aed_type aed_m = eval_blip_atom0_aed(cm, sgrid, rcut, positions);

        for (std::size_t i = 0; i < daed.size(); ++i) {
            const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
            INFO("CGRIDMB[" << k + 1 << "] component=" << i
                 << " analytical=" << daed[i] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_Blip analytical dAED/d SGRIDMB matches central difference",
          "[MODELS][dm_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_blip_cluster();

    Context c = make_blip_mb_context(cgrid, sgrid, rcut, Lmax);

    for (std::size_t k = 0; k < sgrid.size(); ++k) {
        const int arg_pos = static_cast<int>(k + 1);
        const aed_type daed = analytical_daed(c, positions, "SGRIDMB", arg_pos);

        const double h = 1e-5;
        auto sp = sgrid; sp[k] += h;
        auto sm = sgrid; sm[k] -= h;
        aed_type aed_p = eval_blip_atom0_aed(cgrid, sp, rcut, positions);
        aed_type aed_m = eval_blip_atom0_aed(cgrid, sm, rcut, positions);

        for (std::size_t i = 0; i < daed.size(); ++i) {
            const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
            INFO("SGRIDMB[" << k + 1 << "] component=" << i
                 << " analytical=" << daed[i] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_Blip analytical dAED/d RCUTMB matches central difference",
          "[MODELS][dm_blip][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_blip_cluster();

    Context c = make_blip_mb_context(cgrid, sgrid, rcut, Lmax);
    const aed_type daed = analytical_daed(c, positions, "RCUTMB", -1);

    const double h = 1e-5;
    aed_type aed_p = eval_blip_atom0_aed(cgrid, sgrid, rcut + h, positions);
    aed_type aed_m = eval_blip_atom0_aed(cgrid, sgrid, rcut - h, positions);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("RCUTMB component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_Blip returns false on out-of-range or unowned theta",
          "[MODELS][dm_blip][dtheta]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {1.5, 0.8};
    const double rcut = 3.5;

    Context c = make_blip_mb_context(cgrid, sgrid, rcut, /*Lmax=*/2);
    DM_Blip dm(c);
    Cut_Cos fc(rcut);
    dm.set_fcut(&fc, false);

    rho_type drhoi;
    dm.init_rhoi(drhoi);
    drhoi.set_zero();
    Vec3d v(1.2, 0.0, 0.0);
    CHECK_FALSE(dm.calc_drhoi_dtheta("CGRIDMB", 0,  1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("CGRIDMB", 99, 1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("SGRIDMB", 0,  1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("SGRIDMB", 5,  1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("RCUT2B", -1, 1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("BOGUS",  -1, 1, 1, 1.2, 1.44, v, drhoi));

    rho_type rhoi;
    dm.init_rhoi(rhoi);
    rhoi.set_zero();
    aed_type daed_out(dm.size());
    daed_out.set_zero();
    CHECK_FALSE(dm.calc_daed_dtheta("BOGUS",  -1, rhoi, drhoi, daed_out));
    CHECK_FALSE(dm.calc_daed_dtheta("CGRIDMB", 0, rhoi, drhoi, daed_out));
    CHECK_FALSE(dm.calc_daed_dtheta("CGRIDMB", 99, rhoi, drhoi, daed_out));
}
