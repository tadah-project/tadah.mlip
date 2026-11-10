// test_dm_mblip_dtheta.cpp
//
// Layer-1 parity tests for DM_mEAD<F_Blip> (registered as DM_mBlip)
// closed-form theta-derivatives. Mirrors test_dm_blip_dtheta.cpp; the
// difference is that DM_mBlip uses an F_Blip embedding function so
// CGRIDMB/SGRIDMB are enforced to size 1 (single radial GTO per
// orbital) and CEMBFUNC/SEMBFUNC control the descriptor output
// dimension per L.
//
// Theta coverage:
//   - CGRIDMB[1] (only k=1 valid; ng forced to 1 by F_Blip)
//   - SGRIDMB[1]
//   - RCUTMB
//
// CEMBFUNC / SEMBFUNC (the embedding-function radial-blip params) are
// NOT covered here; they would require an F_Blip::calc_dF_dtheta hook
// (deferred — they fall through to FD under HYBRID, where calc_daed_dtheta
// returns false).
//
// Test points: a single (Gaussian) radial means the derivative chain
// is smooth everywhere in r < rcut. We probe a small H cluster (one
// central atom + 3 neighbours) at fixed positions and FD-check each
// theta partial.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/maths.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/dm_all.h>
#include <tadah/models/descriptors/dm/dm_func.h>
#include <tadah/models/descriptors/dm/dm_mEAD.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::Context;
using tadah::core::fd_type;
using tadah::core::PeriodicTable;
using tadah::core::rho_type;
using tadah::core::Vec3d;
using tadah::models::Cut_Cos;
using tadah::models::DM_mEAD;
using tadah::models::F_Blip;

struct TestFixture {
    TestFixture() { PeriodicTable::initialize(); }
};

/* Context builder for DM_mEAD<F_Blip> (= DM_mBlip).
 *
 * F_Blip enforces CGRIDMB and SGRIDMB to be size 1; the
 * embedding output dimension comes from CEMBFUNC / SEMBFUNC sizes. */
Context make_mblip_mb_context(double               cgrid_val,
                              double               sgrid_val,
                              const std::vector<double>& cembfunc,
                              const std::vector<double>& sembfunc,
                              double               rcut,
                              int                  Lmax = 2)
{
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", 1);
    c.add("INITMB", "true");
    c.add("VERBOSE", 0);

    /* SGRIDMB / CGRIDMB — single values for F_Blip. */
    c.add("SGRIDMB", sgrid_val);
    c.add("CGRIDMB", cgrid_val);

    /* CEMBFUNC / SEMBFUNC — vectors of equal length. */
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cembfunc.size(); ++i) {
            if (i) ss << " ";
            ss << cembfunc[i];
        }
        c.add("CEMBFUNC", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sembfunc.size(); ++i) {
            if (i) ss << " ";
            ss << sembfunc[i];
        }
        c.add("SEMBFUNC", ss.str());
    }

    /* Setting RCUTMB triggers Context::updateInternalKeys which derives
     * RCUTMBMAX (= max of RCUTMB values) for the descriptor ctor. */
    c.add("RCUTMB", rcut);
    /* TYPEMB layout for DM_mBlip (DM_mEAD with nparams=5: label + Lmax +
     * 4 key sizes):
     *   "DM_mBlip" Lmax size(cgrid)=1 size(sgrid)=1
     *   size(cembfunc) size(sembfunc) Z1 Z2 */
    c.add("TYPEMB", "DM_mBlip");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", static_cast<int>(cembfunc.size()));
    c.add("TYPEMB", static_cast<int>(sembfunc.size()));
    c.add("TYPEMB", "H");
    c.add("TYPEMB", "H");
    return c;
}

aed_type eval_mblip_atom0_aed(double cgrid_val,
                              double sgrid_val,
                              const std::vector<double>& cembfunc,
                              const std::vector<double>& sembfunc,
                              double rcut,
                              const std::vector<Vec3d>& positions,
                              int    Lmax = 2)
{
    Context c = make_mblip_mb_context(cgrid_val, sgrid_val,
                                      cembfunc, sembfunc, rcut, Lmax);
    DM_mEAD<F_Blip> dm(c);
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
    DM_mEAD<F_Blip> dm(c);
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

    /* calc_aed populates rhoi[drfidx..] = f * dF/drho — required by
     * calc_daed_dtheta's chain rule (it reads emb.calc_F / emb.calc_dF
     * directly rather than the stored F'; the call is kept here for
     * parity with DM_Blip's helper and to ensure rhoi is in the same
     * state the production walker delivers). */
    aed_type aed_scratch(dm.size());
    aed_scratch.set_zero();
    dm.calc_aed(rhoi, aed_scratch);

    aed_type daed(dm.size());
    daed.set_zero();
    dm.calc_daed_dtheta(theta_name, arg_pos, rhoi, drhoi, daed, /*scale=*/1.0);
    return daed;
}

/* Small H cluster — central atom + 3 neighbours, all at r < rcut.
 * Neighbours are off-axis (non-zero in all 3 cartesian components)
 * so the per-orbital ρ for L>0 mixed polynomials does NOT collapse
 * to exactly zero. A zero ρ pins x_c = -mu_0*eta_0 which can land
 * on b''(x_c)'s C^1 break and inflate FD truncation error in the
 * force-side parity tests. */
std::vector<Vec3d> make_mblip_cluster() {
    return {
        Vec3d(0.0, 0.0, 0.0),
        Vec3d(1.2, 0.7, 0.4),
        Vec3d(0.3, 1.1, 0.6),
        Vec3d(0.5, 0.6, 1.3),
    };
}

/* Test config — small grids keep the descriptor cheap. */
constexpr double TEST_CGRID = 0.5;
constexpr double TEST_SGRID = 3.0;
constexpr double TEST_RCUT  = 3.5;
constexpr int    TEST_LMAX  = 2;

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dCGRIDMB matches central difference",
          "[MODELS][dm_mblip][dtheta][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const aed_type daed = analytical_daed(c, positions, "CGRIDMB", /*arg_pos=*/1);

    const double h = 1e-5;
    aed_type aed_p = eval_mblip_atom0_aed(TEST_CGRID + h, TEST_SGRID,
                                           cembfunc, sembfunc,
                                           TEST_RCUT, positions, TEST_LMAX);
    aed_type aed_m = eval_mblip_atom0_aed(TEST_CGRID - h, TEST_SGRID,
                                           cembfunc, sembfunc,
                                           TEST_RCUT, positions, TEST_LMAX);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("CGRIDMB[1] component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dSGRIDMB matches central difference",
          "[MODELS][dm_mblip][dtheta][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const aed_type daed = analytical_daed(c, positions, "SGRIDMB", /*arg_pos=*/1);

    const double h = 1e-5;
    aed_type aed_p = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID + h,
                                           cembfunc, sembfunc,
                                           TEST_RCUT, positions, TEST_LMAX);
    aed_type aed_m = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID - h,
                                           cembfunc, sembfunc,
                                           TEST_RCUT, positions, TEST_LMAX);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("SGRIDMB[1] component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dRCUTMB matches central difference",
          "[MODELS][dm_mblip][dtheta][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const aed_type daed = analytical_daed(c, positions, "RCUTMB", /*arg_pos=*/-1);

    /* RCUTMB FD: re-evaluate at rcut ± h. The cutoff value enters BOTH
     * the per-pair fc factor AND the multinomial prefactor fac[L,o] =
     * ... / (4 rc)^L — the analytical chain rule covers both. */
    const double h = 1e-5;
    aed_type aed_p = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                           cembfunc, sembfunc,
                                           TEST_RCUT + h, positions, TEST_LMAX);
    aed_type aed_m = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                           cembfunc, sembfunc,
                                           TEST_RCUT - h, positions, TEST_LMAX);

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
          "DM_mBlip calc_drhoi_dtheta / calc_daed_dtheta reject unknown theta",
          "[MODELS][dm_mblip][dtheta][routing]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    DM_mEAD<F_Blip> dm(c);
    Cut_Cos fc(c.get<double>("RCUTMBMAX"));
    dm.set_fcut(&fc, false);

    rho_type rhoi, drhoi;
    dm.init_rhoi(rhoi);
    dm.init_rhoi(drhoi);
    rhoi.set_zero();
    drhoi.set_zero();

    const Vec3d  delij = positions[0] - positions[1];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);

    /* Theta keys this descriptor does NOT own at all (RCUTENV is
     * F_mEnv-only; CGRID2B / RCUT2B belong to the D2 arm; UNKNOWN is
     * not a registered key). Both hooks must return false so the
     * orchestrator routes them to needs_fd under HYBRID. */
    for (const char* bad : {"RCUTENV", "CGRID2B", "RCUT2B", "UNKNOWN"}) {
        CHECK_FALSE(dm.calc_drhoi_dtheta(bad, /*arg_pos=*/1,
                                          1, 1, rij, rij_sq, delij,
                                          drhoi, /*scale=*/1.0));
        aed_type daed_out(dm.size());
        daed_out.set_zero();
        CHECK_FALSE(dm.calc_daed_dtheta(bad, /*arg_pos=*/1,
                                         rhoi, drhoi, daed_out,
                                         /*scale=*/1.0));
    }

    /* CEMBFUNC / SEMBFUNC ARE owned by F_Blip — calc_drhoi_dtheta
     * returns false (rho is independent of these), but calc_daed_dtheta
     * returns true with the F-side derivative contribution filled. */
    for (const char* fkey : {"CEMBFUNC", "SEMBFUNC"}) {
        rho_type drhoi_scratch;
        dm.init_rhoi(drhoi_scratch);
        drhoi_scratch.set_zero();
        CHECK_FALSE(dm.calc_drhoi_dtheta(fkey, /*arg_pos=*/1,
                                          1, 1, rij, rij_sq, delij,
                                          drhoi_scratch, /*scale=*/1.0));

        rho_type rhoi_full;
        dm.init_rhoi(rhoi_full);
        rhoi_full.set_zero();
        /* Populate rhoi by accumulating one pair (so rho is non-zero
         * and the F-side derivative is non-trivial). */
        dm.calc_rho(1, 1, rij, rij_sq, delij, rhoi_full, /*scale=*/1.0);
        aed_type aed_scratch(dm.size());
        aed_scratch.set_zero();
        dm.calc_aed(rhoi_full, aed_scratch);

        rho_type drhoi_zero;
        dm.init_rhoi(drhoi_zero);
        drhoi_zero.set_zero();
        aed_type daed_out(dm.size());
        daed_out.set_zero();
        CHECK(dm.calc_daed_dtheta(fkey, /*arg_pos=*/1,
                                  rhoi_full, drhoi_zero, daed_out,
                                  /*scale=*/1.0));
    }

    /* Out-of-range arg_pos for CGRIDMB / SGRIDMB (ng==1) must also
     * return false rather than silently writing garbage. */
    CHECK_FALSE(dm.calc_drhoi_dtheta("CGRIDMB", /*arg_pos=*/2,
                                      1, 1, rij, rij_sq, delij,
                                      drhoi, /*scale=*/1.0));
    CHECK_FALSE(dm.calc_drhoi_dtheta("SGRIDMB", /*arg_pos=*/0,
                                      1, 1, rij, rij_sq, delij,
                                      drhoi, /*scale=*/1.0));
}

/* ------------------------------------------------------------------------
 * Embedding-side theta — CEMBFUNC[i] and SEMBFUNC[i] for DM_mBlip
 *
 * These don't enter rho (radial side doesn't see them), so calc_drhoi_dtheta
 * is irrelevant; the gradient lives entirely in F_Blip::calc_dF_dtheta and
 * propagates through DM_mEAD's daed loop as f[L,o] * dF/dtheta. Component-
 * local: CEMBFUNC[i] / SEMBFUNC[i] only touch the i-1 embedding output p.
 * ------------------------------------------------------------------------ */
TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dCEMBFUNC matches central difference",
          "[MODELS][dm_mblip][dtheta][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    for (std::size_t i = 0; i < cembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_LMAX);
        const aed_type daed = analytical_daed(c, positions, "CEMBFUNC", arg_pos);

        const double h = 1e-5;
        auto cembp = cembfunc; cembp[i] += h;
        auto cembm = cembfunc; cembm[i] -= h;
        aed_type aed_p = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                               cembp, sembfunc,
                                               TEST_RCUT, positions, TEST_LMAX);
        aed_type aed_m = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                               cembm, sembfunc,
                                               TEST_RCUT, positions, TEST_LMAX);

        for (std::size_t k = 0; k < daed.size(); ++k) {
            const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
            INFO("CEMBFUNC[" << i + 1 << "] component=" << k
                 << " analytical=" << daed[k] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[k]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dSEMBFUNC matches central difference",
          "[MODELS][dm_mblip][dtheta][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    for (std::size_t i = 0; i < sembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_LMAX);
        const aed_type daed = analytical_daed(c, positions, "SEMBFUNC", arg_pos);

        const double h = 1e-3;  /* slightly larger for the rho-dependent x_c chain */
        auto sembp = sembfunc; sembp[i] += h;
        auto sembm = sembfunc; sembm[i] -= h;
        aed_type aed_p = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                               cembfunc, sembp,
                                               TEST_RCUT, positions, TEST_LMAX);
        aed_type aed_m = eval_mblip_atom0_aed(TEST_CGRID, TEST_SGRID,
                                               cembfunc, sembm,
                                               TEST_RCUT, positions, TEST_LMAX);

        for (std::size_t k = 0; k < daed.size(); ++k) {
            const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
            INFO("SEMBFUNC[" << i + 1 << "] component=" << k
                 << " analytical=" << daed[k] << " FD=" << fd);
            const double tol = 1e-4 * std::max({std::abs(daed[k]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
        }
    }
}

/* ------------------------------------------------------------------------
 * DM_mBlipEnv (= DM_mEAD<F_mEnv<Cut_SinInv>>) — embedding F has an extra
 * RCUTENV envelope on rho. F_mEnv overrides calc_dF_dtheta to chain:
 *   CEMBFUNC[i] : fcut(rho) * dB/dmu_i
 *   SEMBFUNC[i] : fcut(rho) * dB/deta_i
 *   RCUTENV     : (dfcut/drcut)(rho) * B(rho, eta_p, mu_p) — fans out
 *                 across every p (single fcut shared by all output channels).
 * ------------------------------------------------------------------------ */

namespace {

using tadah::models::Cut_SinInv;
using tadah::models::F_mEnv;

using DM_mBlipEnv = tadah::models::DM_mEAD<F_mEnv<Cut_SinInv>>;

Context make_mblipenv_mb_context(double cgrid_val,
                                 double sgrid_val,
                                 const std::vector<double>& cembfunc,
                                 const std::vector<double>& sembfunc,
                                 double rcut,
                                 double rcutenv,
                                 int    Lmax = 2)
{
    Context c = make_mblip_mb_context(cgrid_val, sgrid_val,
                                      cembfunc, sembfunc, rcut, Lmax);
    /* Swap the TYPEMB label: DM_mBlip -> DM_mBlipEnv. The TYPEMB layout
     * matches DM_mEAD's get_init_atoms expectation (nparams+1=6 entries
     * before atom pairs). */
    c.remove("TYPEMB");
    c.add("TYPEMB", "DM_mBlipEnv");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", static_cast<int>(cembfunc.size()));
    c.add("TYPEMB", static_cast<int>(sembfunc.size()));
    c.add("TYPEMB", "H");
    c.add("TYPEMB", "H");
    /* RCUTENV — width of the F_mEnv envelope (in rho units). The
     * envelope ramps from 0 (rho=0) to 1 (rho=RCUTENV+); rho values
     * in the test cluster need to be well within (0, RCUTENV) so the
     * envelope is non-saturated and the derivative is non-zero. */
    c.add("RCUTENV", rcutenv);
    return c;
}

aed_type eval_mblipenv_atom0_aed(double cgrid_val,
                                 double sgrid_val,
                                 const std::vector<double>& cembfunc,
                                 const std::vector<double>& sembfunc,
                                 double rcut,
                                 double rcutenv,
                                 const std::vector<Vec3d>& positions,
                                 int    Lmax = 2)
{
    Context c = make_mblipenv_mb_context(cgrid_val, sgrid_val,
                                         cembfunc, sembfunc,
                                         rcut, rcutenv, Lmax);
    DM_mBlipEnv dm(c);
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

aed_type analytical_daed_env(const Context& c,
                             const std::vector<Vec3d>& positions,
                             const std::string& theta_name,
                             int                arg_pos)
{
    DM_mBlipEnv dm(c);
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

/* Rho values for the test cluster end up small (order 1e-2..1e-1).
 * Pick CEMBFUNC values inside that range and RCUTENV comfortably
 * above so the envelope sits on its non-trivial slope. */
constexpr double TEST_RCUTENV = 1.0;

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical dAED/dRCUTMB matches central difference",
          "[MODELS][dm_mblipenv][dtheta][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                         cembfunc, sembfunc,
                                         TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
    const aed_type daed = analytical_daed_env(c, positions, "RCUTMB", /*arg_pos=*/-1);

    const double h = 1e-5;
    aed_type aed_p = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                              cembfunc, sembfunc,
                                              TEST_RCUT + h, TEST_RCUTENV,
                                              positions, TEST_LMAX);
    aed_type aed_m = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                              cembfunc, sembfunc,
                                              TEST_RCUT - h, TEST_RCUTENV,
                                              positions, TEST_LMAX);

    for (std::size_t k = 0; k < daed.size(); ++k) {
        const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
        INFO("DM_mBlipEnv RCUTMB component=" << k
             << " analytical=" << daed[k] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[k]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical dAED/dCEMBFUNC matches central difference",
          "[MODELS][dm_mblipenv][dtheta][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    for (std::size_t i = 0; i < cembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                             cembfunc, sembfunc,
                                             TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
        const aed_type daed = analytical_daed_env(c, positions, "CEMBFUNC", arg_pos);

        const double h = 1e-5;
        auto cembp = cembfunc; cembp[i] += h;
        auto cembm = cembfunc; cembm[i] -= h;
        aed_type aed_p = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                                  cembp, sembfunc,
                                                  TEST_RCUT, TEST_RCUTENV,
                                                  positions, TEST_LMAX);
        aed_type aed_m = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                                  cembm, sembfunc,
                                                  TEST_RCUT, TEST_RCUTENV,
                                                  positions, TEST_LMAX);

        for (std::size_t k = 0; k < daed.size(); ++k) {
            const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
            INFO("DM_mBlipEnv CEMBFUNC[" << i + 1 << "] component=" << k
                 << " analytical=" << daed[k] << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(daed[k]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical dAED/dSEMBFUNC matches central difference",
          "[MODELS][dm_mblipenv][dtheta][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    for (std::size_t i = 0; i < sembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                             cembfunc, sembfunc,
                                             TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
        const aed_type daed = analytical_daed_env(c, positions, "SEMBFUNC", arg_pos);

        const double h = 1e-3;
        auto sembp = sembfunc; sembp[i] += h;
        auto sembm = sembfunc; sembm[i] -= h;
        aed_type aed_p = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                                  cembfunc, sembp,
                                                  TEST_RCUT, TEST_RCUTENV,
                                                  positions, TEST_LMAX);
        aed_type aed_m = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                                  cembfunc, sembm,
                                                  TEST_RCUT, TEST_RCUTENV,
                                                  positions, TEST_LMAX);

        for (std::size_t k = 0; k < daed.size(); ++k) {
            const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
            INFO("DM_mBlipEnv SEMBFUNC[" << i + 1 << "] component=" << k
                 << " analytical=" << daed[k] << " FD=" << fd);
            const double tol = 1e-4 * std::max({std::abs(daed[k]),
                                                std::abs(fd), 1e-9});
            CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical dAED/dRCUTENV matches central difference",
          "[MODELS][dm_mblipenv][dtheta][parity][embedding][rcutenv]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                         cembfunc, sembfunc,
                                         TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
    const aed_type daed = analytical_daed_env(c, positions, "RCUTENV", /*arg_pos=*/-1);

    const double h = 1e-5;
    aed_type aed_p = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                              cembfunc, sembfunc,
                                              TEST_RCUT, TEST_RCUTENV + h,
                                              positions, TEST_LMAX);
    aed_type aed_m = eval_mblipenv_atom0_aed(TEST_CGRID, TEST_SGRID,
                                              cembfunc, sembfunc,
                                              TEST_RCUT, TEST_RCUTENV - h,
                                              positions, TEST_LMAX);

    for (std::size_t k = 0; k < daed.size(); ++k) {
        const double fd = (aed_p[k] - aed_m[k]) / (2.0 * h);
        INFO("DM_mBlipEnv RCUTENV component=" << k
             << " analytical=" << daed[k] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[k]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[k] - fd) <= tol + 1e-6);
    }
}

/* ------------------------------------------------------------------------
 * FORCE-side closed forms for DM_mBlip<F_Blip> (CGRIDMB / SGRIDMB /
 * RCUTMB / CEMBFUNC[i] / SEMBFUNC[i]). Mirrors the energy-side parity
 * harness above: build rho_i via calc_rho over the 3-neighbour cluster
 * and calc_aed to populate F'(rho), then call calc_dXijdri_dtheta for
 * one specific (atom 0, neighbour j) pair. FD reference repeats the
 * same pipeline at θ ± h using the existing calc_dXijdri.
 * ------------------------------------------------------------------------ */

namespace {

fd_type eval_mblip_pair_fd(double cgrid_val,
                           double sgrid_val,
                           const std::vector<double>& cembfunc,
                           const std::vector<double>& sembfunc,
                           double rcut,
                           const std::vector<Vec3d>& positions,
                           std::size_t j_idx,
                           int Lmax = 2)
{
    Context c = make_mblip_mb_context(cgrid_val, sgrid_val,
                                      cembfunc, sembfunc, rcut, Lmax);
    DM_mEAD<F_Blip> dm(c);
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

    /* Populate rhoi[drfidx..] = f * F'(rho), the input calc_dXijdri
     * reads. The aed side-output is discarded. */
    aed_type aed_scratch(dm.size());
    aed_scratch.set_zero();
    dm.calc_aed(rhoi, aed_scratch);

    fd_type fd_ij(dm.size());
    fd_ij.set_zero();
    delij = positions[0] - positions[j_idx];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);
    dm.calc_dXijdri(1, 1, rij, rij_sq, delij, rhoi, fd_ij, /*scale=*/1.0);
    return fd_ij;
}

fd_type analytical_pair_dfd(const Context& c,
                            const std::vector<Vec3d>& positions,
                            const std::string& theta_name,
                            int                arg_pos,
                            std::size_t        j_idx)
{
    DM_mEAD<F_Blip> dm(c);
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

    fd_type dfd_ij(dm.size());
    dfd_ij.set_zero();
    delij = positions[0] - positions[j_idx];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);
    const bool ok = dm.calc_dXijdri_dtheta(theta_name, arg_pos,
                                           1, 1, rij, rij_sq, delij,
                                           rhoi, drhoi, dfd_ij, /*scale=*/1.0);
    (void)ok;
    return dfd_ij;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dCGRIDMB matches central difference",
          "[MODELS][dm_mblip][dtheta][force][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd(c, positions, "CGRIDMB",
                                             /*arg_pos=*/1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblip_pair_fd(TEST_CGRID + h, TEST_SGRID,
                                       cembfunc, sembfunc,
                                       TEST_RCUT, positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblip_pair_fd(TEST_CGRID - h, TEST_SGRID,
                                       cembfunc, sembfunc,
                                       TEST_RCUT, positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("CGRIDMB[1] row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dSGRIDMB matches central difference",
          "[MODELS][dm_mblip][dtheta][force][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd(c, positions, "SGRIDMB",
                                             /*arg_pos=*/1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID + h,
                                       cembfunc, sembfunc,
                                       TEST_RCUT, positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID - h,
                                       cembfunc, sembfunc,
                                       TEST_RCUT, positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("SGRIDMB[1] row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dRCUTMB matches central difference",
          "[MODELS][dm_mblip][dtheta][force][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd(c, positions, "RCUTMB",
                                             /*arg_pos=*/-1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                       cembfunc, sembfunc,
                                       TEST_RCUT + h, positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                       cembfunc, sembfunc,
                                       TEST_RCUT - h, positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("RCUTMB row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dCEMBFUNC matches central difference",
          "[MODELS][dm_mblip][dtheta][force][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    for (std::size_t i = 0; i < cembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_LMAX);
        const fd_type dfd = analytical_pair_dfd(c, positions, "CEMBFUNC",
                                                 arg_pos, j_idx);

        const double h = 1e-5;
        auto cembp = cembfunc; cembp[i] += h;
        auto cembm = cembfunc; cembm[i] -= h;
        fd_type fd_p = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                           cembp, sembfunc,
                                           TEST_RCUT, positions, j_idx, TEST_LMAX);
        fd_type fd_m = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                           cembm, sembfunc,
                                           TEST_RCUT, positions, j_idx, TEST_LMAX);

        for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
            for (std::size_t k = 0; k < 3; ++k) {
                const double an = dfd(row, k);
                const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
                INFO("CEMBFUNC[" << i + 1 << "] row=" << row << " axis=" << k
                     << " analytical=" << an << " FD=" << fd);
                const double tol = 1e-5 * std::max({std::abs(an), std::abs(fd), 1e-9});
                CHECK(std::abs(an - fd) <= tol + 1e-6);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dSEMBFUNC matches central difference",
          "[MODELS][dm_mblip][dtheta][force][parity][embedding]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    for (std::size_t i = 0; i < sembfunc.size(); ++i) {
        const int arg_pos = static_cast<int>(i + 1);
        Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_LMAX);
        const fd_type dfd = analytical_pair_dfd(c, positions, "SEMBFUNC",
                                                 arg_pos, j_idx);

        const double h = 1e-3;  /* widen for rho-dependent x_c chain */
        auto sembp = sembfunc; sembp[i] += h;
        auto sembm = sembfunc; sembm[i] -= h;
        fd_type fd_p = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                           cembfunc, sembp,
                                           TEST_RCUT, positions, j_idx, TEST_LMAX);
        fd_type fd_m = eval_mblip_pair_fd(TEST_CGRID, TEST_SGRID,
                                           cembfunc, sembm,
                                           TEST_RCUT, positions, j_idx, TEST_LMAX);

        for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
            for (std::size_t k = 0; k < 3; ++k) {
                const double an = dfd(row, k);
                const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
                INFO("SEMBFUNC[" << i + 1 << "] row=" << row << " axis=" << k
                     << " analytical=" << an << " FD=" << fd);
                const double tol = 1e-4 * std::max({std::abs(an), std::abs(fd), 1e-9});
                CHECK(std::abs(an - fd) <= tol + 1e-6);
            }
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip routes RCUTENV / unknown F-theta to non-owned",
          "[MODELS][dm_mblip][dtheta][routing][embedding]")
{
    /* DM_mBlip's F_Blip does NOT own RCUTENV (that's F_mEnv only).
     * The orchestrator must see calc_daed_dtheta return false so the
     * theta is routed to needs_fd under HYBRID. */
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context(TEST_CGRID, TEST_SGRID,
                                      cembfunc, sembfunc,
                                      TEST_RCUT, TEST_LMAX);
    const aed_type daed_rcutenv =
        analytical_daed(c, positions, "RCUTENV", /*arg_pos=*/-1);
    for (std::size_t k = 0; k < daed_rcutenv.size(); ++k)
        CHECK(daed_rcutenv[k] == 0.0);

    /* Out-of-range CEMBFUNC arg_pos must also return zero (and
     * calc_daed_dtheta returns false — checked via the daed contents
     * staying zero across the whole vector). */
    const aed_type daed_cemb_oob =
        analytical_daed(c, positions, "CEMBFUNC", /*arg_pos=*/99);
    for (std::size_t k = 0; k < daed_cemb_oob.size(); ++k)
        CHECK(daed_cemb_oob[k] == 0.0);
}

/* ------------------------------------------------------------------------
 * WATOM[1] parity for DM_mBlip<F_Blip>.
 * d(rho)/d WATOM[Z_target] is the unweighted per-pair calc_rho contribution
 * fired only on pairs with Zj == Z_target; calc_daed_dtheta chains via
 * F'(rho) * drho/dWATOM. For the force side, the additional direct-factor
 * term d(scale*wj)/dW[Z] = scale*δ(Zj,Z) propagates through the radial g
 * exactly like the existing radial-θ test pattern.
 * ------------------------------------------------------------------------ */

namespace {

Context make_mblip_mb_context_with_watom(double               cgrid_val,
                                          double               sgrid_val,
                                          const std::vector<double>& cembfunc,
                                          const std::vector<double>& sembfunc,
                                          double               rcut,
                                          double               watom,
                                          int                  Lmax = 2)
{
    Context c;
    c.add("ATOM", "H");
    c.add("WATOM", watom);
    c.add("INITMB", "true");
    c.add("VERBOSE", 0);
    c.add("SGRIDMB", sgrid_val);
    c.add("CGRIDMB", cgrid_val);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < cembfunc.size(); ++i) { if (i) ss << " "; ss << cembfunc[i]; }
        c.add("CEMBFUNC", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < sembfunc.size(); ++i) { if (i) ss << " "; ss << sembfunc[i]; }
        c.add("SEMBFUNC", ss.str());
    }
    c.add("RCUTMB", rcut);
    c.add("TYPEMB", "DM_mBlip");
    c.add("TYPEMB", Lmax);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", 1);
    c.add("TYPEMB", static_cast<int>(cembfunc.size()));
    c.add("TYPEMB", static_cast<int>(sembfunc.size()));
    c.add("TYPEMB", "H"); c.add("TYPEMB", "H");
    return c;
}

aed_type eval_mblip_atom0_aed_with_watom(double cgrid_val, double sgrid_val,
                                          const std::vector<double>& cembfunc,
                                          const std::vector<double>& sembfunc,
                                          double rcut, double watom,
                                          const std::vector<Vec3d>& positions,
                                          int    Lmax = 2)
{
    Context c = make_mblip_mb_context_with_watom(cgrid_val, sgrid_val,
                                                  cembfunc, sembfunc,
                                                  rcut, watom, Lmax);
    DM_mEAD<F_Blip> dm(c);
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

fd_type eval_mblip_pair_fd_with_watom(double cgrid_val, double sgrid_val,
                                       const std::vector<double>& cembfunc,
                                       const std::vector<double>& sembfunc,
                                       double rcut, double watom,
                                       const std::vector<Vec3d>& positions,
                                       std::size_t j_idx,
                                       int Lmax = 2)
{
    Context c = make_mblip_mb_context_with_watom(cgrid_val, sgrid_val,
                                                  cembfunc, sembfunc,
                                                  rcut, watom, Lmax);
    DM_mEAD<F_Blip> dm(c);
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
    aed_type aed_scratch(dm.size());
    aed_scratch.set_zero();
    dm.calc_aed(rhoi, aed_scratch);

    fd_type fd_ij(dm.size());
    fd_ij.set_zero();
    delij = positions[0] - positions[j_idx];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);
    dm.calc_dXijdri(1, 1, rij, rij_sq, delij, rhoi, fd_ij, /*scale=*/1.0);
    return fd_ij;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical dAED/dWATOM matches central difference",
          "[MODELS][dm_mblip][dtheta][watom][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();

    Context c = make_mblip_mb_context_with_watom(TEST_CGRID, TEST_SGRID,
                                                  cembfunc, sembfunc,
                                                  TEST_RCUT, 1.0, TEST_LMAX);
    const aed_type daed = analytical_daed(c, positions, "WATOM", /*arg_pos=*/1);

    const double h = 1e-5;
    aed_type aed_p = eval_mblip_atom0_aed_with_watom(
        TEST_CGRID, TEST_SGRID, cembfunc, sembfunc, TEST_RCUT, 1.0 + h,
        positions, TEST_LMAX);
    aed_type aed_m = eval_mblip_atom0_aed_with_watom(
        TEST_CGRID, TEST_SGRID, cembfunc, sembfunc, TEST_RCUT, 1.0 - h,
        positions, TEST_LMAX);

    for (std::size_t i = 0; i < daed.size(); ++i) {
        const double fd = (aed_p[i] - aed_m[i]) / (2.0 * h);
        INFO("WATOM[1] component=" << i
             << " analytical=" << daed[i] << " FD=" << fd);
        const double tol = 1e-5 * std::max({std::abs(daed[i]),
                                            std::abs(fd), 1e-9});
        CHECK(std::abs(daed[i] - fd) <= tol + 1e-6);
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlip analytical d(fd_ij)/dWATOM matches central difference",
          "[MODELS][dm_mblip][dtheta][force][watom][parity]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblip_mb_context_with_watom(TEST_CGRID, TEST_SGRID,
                                                  cembfunc, sembfunc,
                                                  TEST_RCUT, 1.0, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd(c, positions, "WATOM",
                                             /*arg_pos=*/1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblip_pair_fd_with_watom(
        TEST_CGRID, TEST_SGRID, cembfunc, sembfunc, TEST_RCUT, 1.0 + h,
        positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblip_pair_fd_with_watom(
        TEST_CGRID, TEST_SGRID, cembfunc, sembfunc, TEST_RCUT, 1.0 - h,
        positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("WATOM[1] row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-5 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

/* ------------------------------------------------------------------------
 * DM_mBlipEnv FORCE-side closed forms — exercises the radial-θ chain
 * through F_mEnv::calc_d2F. Without this, the
 * radial-θ branch in DM_mEAD::calc_dXijdri_dtheta returned false for
 * any F_mEnv config because calc_d2F was a stub. With Cut_*::calc_double_prime
 * + the product-rule wiring, the chain is fully analytical.
 * ------------------------------------------------------------------------ */

namespace {

fd_type eval_mblipenv_pair_fd(double cgrid_val,
                              double sgrid_val,
                              const std::vector<double>& cembfunc,
                              const std::vector<double>& sembfunc,
                              double rcut,
                              double rcutenv,
                              const std::vector<Vec3d>& positions,
                              std::size_t j_idx,
                              int Lmax = 2)
{
    Context c = make_mblipenv_mb_context(cgrid_val, sgrid_val,
                                         cembfunc, sembfunc, rcut, rcutenv, Lmax);
    DM_mBlipEnv dm(c);
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
    aed_type aed_scratch(dm.size());
    aed_scratch.set_zero();
    dm.calc_aed(rhoi, aed_scratch);

    fd_type fd_ij(dm.size());
    fd_ij.set_zero();
    delij = positions[0] - positions[j_idx];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);
    dm.calc_dXijdri(1, 1, rij, rij_sq, delij, rhoi, fd_ij, /*scale=*/1.0);
    return fd_ij;
}

fd_type analytical_pair_dfd_env(const Context& c,
                                const std::vector<Vec3d>& positions,
                                const std::string& theta_name,
                                int                arg_pos,
                                std::size_t        j_idx)
{
    DM_mBlipEnv dm(c);
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

    fd_type dfd_ij(dm.size());
    dfd_ij.set_zero();
    delij = positions[0] - positions[j_idx];
    const double rij_sq = delij * delij;
    const double rij    = std::sqrt(rij_sq);
    const bool ok = dm.calc_dXijdri_dtheta(theta_name, arg_pos,
                                           1, 1, rij, rij_sq, delij,
                                           rhoi, drhoi, dfd_ij, /*scale=*/1.0);
    REQUIRE(ok);
    return dfd_ij;
}

}  // namespace

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical d(fd_ij)/dCGRIDMB matches central difference",
          "[MODELS][dm_mblipenv][dtheta][force][parity][radial]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                         cembfunc, sembfunc,
                                         TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd_env(c, positions, "CGRIDMB",
                                                 /*arg_pos=*/1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblipenv_pair_fd(TEST_CGRID + h, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblipenv_pair_fd(TEST_CGRID - h, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("DM_mBlipEnv CGRIDMB[1] row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-4 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical d(fd_ij)/dSGRIDMB matches central difference",
          "[MODELS][dm_mblipenv][dtheta][force][parity][radial]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                         cembfunc, sembfunc,
                                         TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd_env(c, positions, "SGRIDMB",
                                                 /*arg_pos=*/1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblipenv_pair_fd(TEST_CGRID, TEST_SGRID + h,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblipenv_pair_fd(TEST_CGRID, TEST_SGRID - h,
                                          cembfunc, sembfunc,
                                          TEST_RCUT, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("DM_mBlipEnv SGRIDMB[1] row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-4 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}

TEST_CASE_METHOD(TestFixture,
          "DM_mBlipEnv analytical d(fd_ij)/dRCUTMB matches central difference",
          "[MODELS][dm_mblipenv][dtheta][force][parity][radial]")
{
    const std::vector<double> cembfunc = {0.05, 0.15};
    const std::vector<double> sembfunc = {20.0, 20.0};
    const auto positions = make_mblip_cluster();
    const std::size_t j_idx = 1;

    Context c = make_mblipenv_mb_context(TEST_CGRID, TEST_SGRID,
                                         cembfunc, sembfunc,
                                         TEST_RCUT, TEST_RCUTENV, TEST_LMAX);
    const fd_type dfd = analytical_pair_dfd_env(c, positions, "RCUTMB",
                                                 /*arg_pos=*/-1, j_idx);

    const double h = 1e-5;
    fd_type fd_p = eval_mblipenv_pair_fd(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT + h, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);
    fd_type fd_m = eval_mblipenv_pair_fd(TEST_CGRID, TEST_SGRID,
                                          cembfunc, sembfunc,
                                          TEST_RCUT - h, TEST_RCUTENV,
                                          positions, j_idx, TEST_LMAX);

    for (std::size_t row = 0; row < static_cast<std::size_t>(dfd.rows()); ++row) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double an = dfd(row, k);
            const double fd = (fd_p(row, k) - fd_m(row, k)) / (2.0 * h);
            INFO("DM_mBlipEnv RCUTMB row=" << row << " axis=" << k
                 << " analytical=" << an << " FD=" << fd);
            const double tol = 1e-4 * std::max({std::abs(an), std::abs(fd), 1e-9});
            CHECK(std::abs(an - fd) <= tol + 1e-6);
        }
    }
}
