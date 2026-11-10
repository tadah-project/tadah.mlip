// test_dm_ead_dtheta.cpp
//
// Layer-1 parity tests for the closed-form analytical derivatives of
// DM_EAD with respect to its hyperparameters. The DM (many-body)
// pipeline is structurally different from D2: AED is computed in two
// passes (calc_rho per-pair, then calc_aed per-atom on accumulated
// rho_i). The dAED/dtheta chain therefore mirrors that structure:
//
//   1. calc_drhoi_dtheta accumulates drho_i/dtheta per pair (same
//      neighbour iteration as calc_rho).
//   2. calc_daed_dtheta folds (rho_i, drho_i/dtheta) into daed/dtheta
//      per atom (same per-atom reduction as calc_aed, with an extra
//      df[L,o]/dtheta * rho^2 term for cross-cutting RCUTMB).
//
// FD reference re-runs the full (calc_rho + calc_aed) pipeline at
// perturbed theta.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/dm_ead.h>

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
using tadah::models::DM_EAD;

struct TestFixture {
    TestFixture() { PeriodicTable::initialize(); }
};

// Build a Context for DM_EAD with one H species and the given grids
// + rcut. TYPEMB indices: ["DM_EAD", Lmax, |cgrid|, |sgrid|, H, H].
Context make_ead_context(const std::vector<double>& cgrid,
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
     * RCUTMBMAX (= max of RCUTMB values) for the descriptor ctor. The
     * internal RCUTMBMAX key cannot be set via add() directly. */
    c.add("RCUTMB", rcut);
    c.add("TYPEMB", "DM_EAD");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", static_cast<int>(cgrid.size()));
    c.add("TYPEMB", static_cast<int>(sgrid.size()));
    c.add("TYPEMB", "H");
    c.add("TYPEMB", "H");
    return c;
}

// Build aed for atom 0 given the neighbour positions, by running the
// full pipeline (calc_rho over neighbours + calc_aed). Caller supplies
// the descriptor + cutoff fresh per call so perturbed-theta evaluations
// are clean. Vectors are full positions including atom 0 at index 0;
// neighbours start at index 1.
aed_type eval_ead_atom0_aed(const std::vector<double>& cgrid,
                            const std::vector<double>& sgrid,
                            double rcut,
                            const std::vector<Vec3d>& positions)
{
    Context c = make_ead_context(cgrid, sgrid, rcut);
    DM_EAD dm(c);
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

// Run the analytical pipeline: per-pair calc_drhoi_dtheta over
// neighbours, then per-atom calc_daed_dtheta on accumulated (rho, drho).
aed_type analytical_daed(const Context& c,
                         const std::vector<Vec3d>& positions,
                         const std::string& theta_name,
                         int                arg_pos)
{
    DM_EAD dm(c);
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
    dm.calc_aed(rhoi, aed_scratch);   // populates rhoi[drfidx..] = 2 f rho

    aed_type daed(dm.size());
    daed.set_zero();
    dm.calc_daed_dtheta(theta_name, arg_pos, rhoi, drhoi, daed, /*scale=*/1.0);
    return daed;
}

// A 4-atom cluster placed so atom 0 sees three neighbours within the
// cutoff; spacing chosen to keep rij comfortably below rcut and away
// from any cutoff edge. Mirrors test_dm.cpp's 4-atom layout but smaller.
std::vector<Vec3d> make_cluster() {
    return {
        Vec3d(0.0, 0.0, 0.0),
        Vec3d(1.6, 0.0, 0.0),
        Vec3d(0.0, 1.4, 0.0),
        Vec3d(0.0, 0.0, 2.1),
    };
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_EAD analytical dAED/d CGRIDMB matches central difference",
          "[MODELS][dm_ead][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {2.0, 3.0};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_cluster();

    Context c = make_ead_context(cgrid, sgrid, rcut, Lmax);

    for (std::size_t k = 0; k < cgrid.size(); ++k) {
        const int arg_pos = static_cast<int>(k + 1);
        const aed_type daed = analytical_daed(c, positions, "CGRIDMB", arg_pos);

        const double h = 1e-5;
        auto cp = cgrid; cp[k] += h;
        auto cm = cgrid; cm[k] -= h;
        aed_type aed_p = eval_ead_atom0_aed(cp, sgrid, rcut, positions);
        aed_type aed_m = eval_ead_atom0_aed(cm, sgrid, rcut, positions);

        for (std::size_t i = 0; i < daed.size(); ++i) {
            const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
            INFO("CGRIDMB[" << k + 1 << "] component=" << i
                 << " analytical=" << daed[i] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[i] - fd) <= tol + 1e-7);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_EAD analytical dAED/d SGRIDMB matches central difference",
          "[MODELS][dm_ead][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {2.0, 3.0};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_cluster();

    Context c = make_ead_context(cgrid, sgrid, rcut, Lmax);

    for (std::size_t k = 0; k < sgrid.size(); ++k) {
        const int arg_pos = static_cast<int>(k + 1);
        const aed_type daed = analytical_daed(c, positions, "SGRIDMB", arg_pos);

        const double h = 1e-5;
        auto sp = sgrid; sp[k] += h;
        auto sm = sgrid; sm[k] -= h;
        aed_type aed_p = eval_ead_atom0_aed(cgrid, sp, rcut, positions);
        aed_type aed_m = eval_ead_atom0_aed(cgrid, sm, rcut, positions);

        for (std::size_t i = 0; i < daed.size(); ++i) {
            const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
            INFO("SGRIDMB[" << k + 1 << "] component=" << i
                 << " analytical=" << daed[i] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[i] - fd) <= tol + 1e-7);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_EAD analytical dAED/d RCUTMB matches central difference",
          "[MODELS][dm_ead][dtheta][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {2.0, 3.0};
    const double rcut = 3.5;
    const int    Lmax = 2;
    const auto   positions = make_cluster();

    Context c = make_ead_context(cgrid, sgrid, rcut, Lmax);
    const aed_type daed = analytical_daed(c, positions, "RCUTMB", -1);

    const double h = 1e-5;
    aed_type aed_p = eval_ead_atom0_aed(cgrid, sgrid, rcut + h, positions);
    aed_type aed_m = eval_ead_atom0_aed(cgrid, sgrid, rcut - h, positions);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("RCUTMB component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        /* RCUTMB chain hits both fc and the L-prefactor 1/(4 rc)^L; the
         * latter amplifies FD truncation slightly so we widen the floor. */
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_EAD returns false on out-of-range or unowned theta",
          "[MODELS][dm_ead][dtheta]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {2.0, 3.0};
    const double rcut = 3.5;

    Context c = make_ead_context(cgrid, sgrid, rcut, /*Lmax=*/2);
    DM_EAD dm(c);
    Cut_Cos fc(rcut);
    dm.set_fcut(&fc, false);

    /* drhoi pre-pass: out-of-range arg_pos and unowned theta_name. */
    rho_type drhoi;
    dm.init_rhoi(drhoi);
    drhoi.set_zero();
    Vec3d v(1.2, 0.0, 0.0);
    CHECK_FALSE(dm.calc_drhoi_dtheta("CGRIDMB", 0,  1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("CGRIDMB", 99, 1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("SGRIDMB", 0,  1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("SGRIDMB", 5,  1, 1, 1.2, 1.44, v, drhoi));
    /* RCUT2B is the D2 cutoff key; DM_EAD owns RCUTMB only. */
    CHECK_FALSE(dm.calc_drhoi_dtheta("RCUT2B", -1, 1, 1, 1.2, 1.44, v, drhoi));
    CHECK_FALSE(dm.calc_drhoi_dtheta("BOGUS",  -1, 1, 1, 1.2, 1.44, v, drhoi));

    /* daed reducer: same theta_name checks. */
    rho_type rhoi;
    dm.init_rhoi(rhoi);
    rhoi.set_zero();
    aed_type daed_out(dm.size());
    daed_out.set_zero();
    CHECK_FALSE(dm.calc_daed_dtheta("BOGUS",  -1, rhoi, drhoi, daed_out));
    CHECK_FALSE(dm.calc_daed_dtheta("CGRIDMB", 0, rhoi, drhoi, daed_out));
    CHECK_FALSE(dm.calc_daed_dtheta("CGRIDMB", 99, rhoi, drhoi, daed_out));
}

/* ------------------------------------------------------------------------
 * WATOM[1] parity for DM_EAD. Same chain-rule structure
 * as DM_mBlip's WATOM test: drho/dW[Z] is the unweighted per-pair
 * contribution fired only on pairs with Zj == Z_target; calc_daed_dtheta
 * chains via F'(rho) * drho/dW[Z]. f doesn't depend on WATOM.
 * ------------------------------------------------------------------------ */

namespace {
Context make_ead_context_with_watom(const std::vector<double>& cgrid,
                                     const std::vector<double>& sgrid,
                                     double rcut, double watom,
                                     int Lmax = 2) {
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", watom);
    c.add("INITMB", "true");
    c.add("VERBOSE", 0);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sgrid.size(); ++i) { if (i) ss << " "; ss << sgrid[i]; }
        c.add("SGRIDMB", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cgrid.size(); ++i) { if (i) ss << " "; ss << cgrid[i]; }
        c.add("CGRIDMB", ss.str());
    }
    c.add("RCUTMB", rcut);
    c.add("TYPEMB", "DM_EAD");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", static_cast<int>(cgrid.size()));
    c.add("TYPEMB", static_cast<int>(sgrid.size()));
    c.add("TYPEMB", "H"); c.add("TYPEMB", "H");
    return c;
}
aed_type eval_ead_atom0_aed_with_watom(const std::vector<double>& cgrid,
                                        const std::vector<double>& sgrid,
                                        double rcut, double watom,
                                        const std::vector<Vec3d>& positions) {
    Context c = make_ead_context_with_watom(cgrid, sgrid, rcut, watom);
    DM_EAD dm(c);
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
        dm.calc_rho(1, 1, rij, rij_sq, delij, rhoi, 1.0);
    }
    aed_type aed(dm.size()); aed.set_zero();
    dm.calc_aed(rhoi, aed);
    return aed;
}
}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_EAD analytical dAED/dWATOM matches central difference",
          "[MODELS][dm_ead][dtheta][watom][parity]")
{
    const std::vector<double> cgrid = {0.7, 1.5};
    const std::vector<double> sgrid = {2.0, 3.0};
    const double rcut = 3.5;
    const auto positions = make_cluster();

    Context c = make_ead_context_with_watom(cgrid, sgrid, rcut, 1.0);
    const aed_type daed = analytical_daed(c, positions, "WATOM", /*arg_pos=*/1);

    const double h = 1e-5;
    aed_type aed_p = eval_ead_atom0_aed_with_watom(cgrid, sgrid, rcut, 1.0 + h, positions);
    aed_type aed_m = eval_ead_atom0_aed_with_watom(cgrid, sgrid, rcut, 1.0 - h, positions);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("WATOM[1] component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}
