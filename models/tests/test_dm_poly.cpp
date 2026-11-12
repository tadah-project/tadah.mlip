#include "catch2/catch.hpp"
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/descriptors/dm/dm_all.h>
#include <tadah/models/cutoffs.h>

#include <cmath>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

// -----------------------------------------------------------------------
//  central_difference_mb: FD of AED of atom 0 w.r.t. position of atom j.
//  Identical to test_dm.cpp — redeclared here to keep files independent.
// -----------------------------------------------------------------------
template <typename DM>
static void central_difference_mb(DM *dm, std::vector<Vec3d> &positions,
                                   size_t j, fd_type &fdij, double h = 1e-8)
{
    std::vector<size_t> indices = {0, 1, 2, 3};
    indices.erase(indices.begin() + j);
    indices.erase(indices.begin());

    Vec3d &x1   = positions[0];
    Vec3d del13 = x1 - positions[indices.back()]; indices.pop_back();
    Vec3d del14 = x1 - positions[indices.back()]; indices.pop_back();

    for (int i = 0; i < 3; ++i) {
        Vec3d xjn = positions[j];
        Vec3d xjp = positions[j];
        xjn[i] -= h;
        xjp[i] += h;

        Vec3d del12n = x1 - xjn;
        Vec3d del12p = x1 - xjp;

        aed_type rhoip, rhoin;
        dm->init_rhoi(rhoip);
        dm->init_rhoi(rhoin);

        dm->calc_rho_mb(1, 1, del12p, rhoip);
        dm->calc_rho_mb(1, 1, del13,  rhoip);
        dm->calc_rho_mb(1, 1, del14,  rhoip);

        dm->calc_rho_mb(1, 1, del12n, rhoin);
        dm->calc_rho_mb(1, 1, del13,  rhoin);
        dm->calc_rho_mb(1, 1, del14,  rhoin);

        aed_type aedip(fdij.rows()), aedin(fdij.rows());
        aedip.set_zero();
        aedin.set_zero();
        dm->calc_aed(rhoip, aedip);
        dm->calc_aed(rhoin, aedin);

        fdij[i] = -(0.5 / h) * (aedip - aedin);
    }
}

// -----------------------------------------------------------------------
//  test_dm_poly_fd: FD gradient test for any DM_Poly variant and cutoff.
//  Mirrors test_case() in test_dm.cpp.
// -----------------------------------------------------------------------
template <typename DM, typename CUT>
static void test_dm_poly_fd(Context &ctx,
                             const std::string &expected_label,
                             size_t             expected_size)
{
    ctx.add("ATOM",   "H");
    ctx.add("WATOM",  1);
    ctx.add("TYPEMB", "H");
    ctx.add("TYPEMB", "H");

    Vec3d x1( 0.0,   0.0,   0.0);
    Vec3d x2( 2.1,   3.3,   4.4);
    Vec3d x3( 5.1,   4.3,   3.4);
    Vec3d x4(-4.11,  5.33, -3.44);
    std::vector<Vec3d> positions = {x1, x2, x3, x4};

    Vec3d del12=x1-x2, del21=x2-x1, del13=x1-x3, del23=x2-x3;
    Vec3d del14=x1-x4, del24=x2-x4, del31=x3-x1, del32=x3-x2;
    Vec3d del34=x3-x4, del41=x4-x1, del42=x4-x2, del43=x4-x3;

    double rsq12=del12*del12, rsq21=del21*del21, rsq13=del13*del13;
    double rsq23=del23*del23, rsq14=del14*del14, rsq24=del24*del24;
    double rsq31=del31*del31, rsq32=del32*del32, rsq34=del34*del34;
    double rsq41=del41*del41, rsq42=del42*del42, rsq43=del43*del43;
    double r12=sqrt(rsq12), r21=sqrt(rsq21), r13=sqrt(rsq13), r23=sqrt(rsq23);
    double r14=sqrt(rsq14), r24=sqrt(rsq24), r31=sqrt(rsq31), r32=sqrt(rsq32);
    double r34=sqrt(rsq34), r41=sqrt(rsq41), r42=sqrt(rsq42), r43=sqrt(rsq43);

    CUT fc(ctx.get<double>("RCUTMB"));
    DM dm(ctx);
    dm.set_fcut(&fc, false);

    REQUIRE(dm.label() == expected_label);
    REQUIRE(dm.size()  == expected_size);

    // ---------- rho accumulation ----------
    aed_type aed1(dm.size()), aed2(dm.size()), aed3(dm.size()), aed4(dm.size());
    aed1.set_zero(); aed2.set_zero(); aed3.set_zero(); aed4.set_zero();
    rho_type rho1, rho2, rho3, rho4;
    dm.init_rhoi(rho1); dm.init_rhoi(rho2); dm.init_rhoi(rho3); dm.init_rhoi(rho4);

    dm.calc_rho_mb(1,1,del12,rho1); dm.calc_rho_mb(1,1,del13,rho1); dm.calc_rho_mb(1,1,del14,rho1);
    dm.calc_rho_mb(1,1,del21,rho2); dm.calc_rho_mb(1,1,del23,rho2); dm.calc_rho_mb(1,1,del24,rho2);
    dm.calc_rho_mb(1,1,del31,rho3); dm.calc_rho_mb(1,1,del32,rho3); dm.calc_rho_mb(1,1,del34,rho3);
    dm.calc_rho_mb(1,1,del41,rho4); dm.calc_rho_mb(1,1,del42,rho4); dm.calc_rho_mb(1,1,del43,rho4);
    dm.calc_aed(rho1,aed1); dm.calc_aed(rho2,aed2); dm.calc_aed(rho3,aed3); dm.calc_aed(rho4,aed4);

    // ---------- full-list derivatives ----------
    fd_type fd12(dm.size()), fd13(dm.size()), fd14(dm.size());
    fd_type fd21(dm.size()), fd23(dm.size()), fd24(dm.size());
    fd_type fd31(dm.size()), fd32(dm.size()), fd34(dm.size());
    fd_type fd41(dm.size()), fd42(dm.size()), fd43(dm.size());
    fd12.set_zero(); fd13.set_zero(); fd14.set_zero();
    fd21.set_zero(); fd23.set_zero(); fd24.set_zero();
    fd31.set_zero(); fd32.set_zero(); fd34.set_zero();
    fd41.set_zero(); fd42.set_zero(); fd43.set_zero();

    dm.calc_dXijdri(1,1,r12,rsq12,del12,rho1,fd12); dm.calc_dXijdri(1,1,r13,rsq13,del13,rho1,fd13);
    dm.calc_dXijdri(1,1,r14,rsq14,del14,rho1,fd14); dm.calc_dXijdri(1,1,r21,rsq21,del21,rho2,fd21);
    dm.calc_dXijdri(1,1,r23,rsq23,del23,rho2,fd23); dm.calc_dXijdri(1,1,r24,rsq24,del24,rho2,fd24);
    dm.calc_dXijdri(1,1,r31,rsq31,del31,rho3,fd31); dm.calc_dXijdri(1,1,r32,rsq32,del32,rho3,fd32);
    dm.calc_dXijdri(1,1,r34,rsq34,del34,rho3,fd34); dm.calc_dXijdri(1,1,r41,rsq41,del41,rho4,fd41);
    dm.calc_dXijdri(1,1,r42,rsq42,del42,rho4,fd42); dm.calc_dXijdri(1,1,r43,rsq43,del43,rho4,fd43);

    // ---------- half-list derivatives ----------
    fd_type FD12(dm.size()), FD13(dm.size()), FD14(dm.size());
    fd_type FD21(dm.size()), FD23(dm.size()), FD24(dm.size());
    fd_type FD31(dm.size()), FD32(dm.size()), FD34(dm.size());
    fd_type FD41(dm.size()), FD42(dm.size()), FD43(dm.size());
    FD12.set_zero(); FD13.set_zero(); FD14.set_zero();
    FD21.set_zero(); FD23.set_zero(); FD24.set_zero();
    FD31.set_zero(); FD32.set_zero(); FD34.set_zero();
    FD41.set_zero(); FD42.set_zero(); FD43.set_zero();

    dm.calc_dXijdri_dXjidri(1,1,r12,rsq12,del12,rho1,rho2,FD12,1);
    dm.calc_dXijdri_dXjidri(1,1,r13,rsq13,del13,rho1,rho3,FD13,1);
    dm.calc_dXijdri_dXjidri(1,1,r14,rsq14,del14,rho1,rho4,FD14,1);
    dm.calc_dXijdri_dXjidri(1,1,r21,rsq21,del21,rho2,rho1,FD21,1);
    dm.calc_dXijdri_dXjidri(1,1,r23,rsq23,del23,rho2,rho3,FD23,1);
    dm.calc_dXijdri_dXjidri(1,1,r24,rsq24,del24,rho2,rho4,FD24,1);
    dm.calc_dXijdri_dXjidri(1,1,r31,rsq31,del31,rho3,rho1,FD31,1);
    dm.calc_dXijdri_dXjidri(1,1,r32,rsq32,del32,rho3,rho2,FD32,1);
    dm.calc_dXijdri_dXjidri(1,1,r34,rsq34,del34,rho3,rho4,FD34,1);
    dm.calc_dXijdri_dXjidri(1,1,r41,rsq41,del41,rho4,rho1,FD41,1);
    dm.calc_dXijdri_dXjidri(1,1,r42,rsq42,del42,rho4,rho2,FD42,1);
    dm.calc_dXijdri_dXjidri(1,1,r43,rsq43,del43,rho4,rho3,FD43,1);

    // Half-list = full-list antisymmetric combination
    REQUIRE(FD12.isApprox(fd12-fd21, 1e-6)); REQUIRE(FD13.isApprox(fd13-fd31, 1e-6));
    REQUIRE(FD14.isApprox(fd14-fd41, 1e-6)); REQUIRE(FD21.isApprox(fd21-fd12, 1e-6));
    REQUIRE(FD23.isApprox(fd23-fd32, 1e-6)); REQUIRE(FD24.isApprox(fd24-fd42, 1e-6));
    REQUIRE(FD31.isApprox(fd31-fd13, 1e-6)); REQUIRE(FD32.isApprox(fd32-fd23, 1e-6));
    REQUIRE(FD34.isApprox(fd34-fd43, 1e-6)); REQUIRE(FD41.isApprox(fd41-fd14, 1e-6));
    REQUIRE(FD42.isApprox(fd42-fd24, 1e-6)); REQUIRE(FD43.isApprox(fd43-fd34, 1e-6));

    // ---------- FD numerical vs analytical ----------
    fd_type fd12a(dm.size()), fd13a(dm.size()), fd14a(dm.size());
    fd12a.set_zero(); fd13a.set_zero(); fd14a.set_zero();
    central_difference_mb(&dm, positions, 1, fd12a, 1e-8);
    central_difference_mb(&dm, positions, 2, fd13a, 1e-8);
    central_difference_mb(&dm, positions, 3, fd14a, 1e-8);

    const double margin = 1e-5;
    auto check = [&](fd_type &ana, fd_type &num) {
        for (int dir = 0; dir < 3; ++dir) {
            std::vector<double> fa(ana[dir].data(), ana[dir].data() + ana[dir].size());
            std::vector<double> fn(num[dir].data(), num[dir].data() + num[dir].size());
            REQUIRE_THAT(fn, Catch::Matchers::Approx(fa).margin(margin));
        }
    };
    check(fd12, fd12a);
    check(fd13, fd13a);
    check(fd14, fd14a);
}

// -----------------------------------------------------------------------
//  Context builders — one per channel-function family
// -----------------------------------------------------------------------

static Context make_ctx_sq(double rcut, int Lmax, const std::string &label)
{
    Context ctx;
    ctx.add("RCUTMB", rcut);
    ctx.remove("INITMB");
    ctx.add("INITMB", "true");
    ctx.add("TYPEMB", label);
    ctx.add("TYPEMB", Lmax);
    ctx.add("TYPEMB", Lmax);
    ctx.add("TYPEMB", Lmax);
    ctx.add("CGRIDMB", "1 2");
    ctx.add("SGRIDMB", "0.1 0.2");
    return ctx;
}

static Context make_ctx_sqrt(double rcut, int Lmax, const std::string &label)
{
    Context ctx = make_ctx_sq(rcut, Lmax, label);
    ctx.add("SEMBFUNC", "1 1");
    return ctx;
}

static Context make_ctx_rlr(double rcut, int Lmax, const std::string &label)
{
    Context ctx = make_ctx_sq(rcut, Lmax, label);
    ctx.add("CEMBFUNC", "1 2");
    ctx.add("SEMBFUNC", "0.1 0.2");
    return ctx;
}

static Context make_ctx_blipf(double rcut, int Lmax, const std::string &label)
{
    Context ctx;
    ctx.add("RCUTMB", rcut);
    ctx.remove("INITMB");
    ctx.add("INITMB", "true");
    ctx.add("TYPEMB", label);
    ctx.add("TYPEMB", Lmax);
    ctx.add("TYPEMB", Lmax);
    ctx.add("TYPEMB", Lmax);
    ctx.add("CGRIDMB", "0.0");
    ctx.add("SGRIDMB", "-1.5");
    ctx.add("CEMBFUNC", "0.5 1.5");
    ctx.add("SEMBFUNC", "0.5 0.8");
    return ctx;
}

// -----------------------------------------------------------------------
//  Fixture
// -----------------------------------------------------------------------
struct TestFixturePoly {
    TestFixturePoly() { PeriodicTable::initialize(); }
};

// -----------------------------------------------------------------------
//  PowerSpectrum / BlipRadial  (DM_PS_*)
//  Tested: Lmax=0 and Lmax=1, Cut_Dummy and Cut_Cos.
// -----------------------------------------------------------------------

/* Each case expects the label of its descriptor to be its registered name
 * (DM_PS_SQ, ..., DM_GOW_Blip), the TYPEMB value that names it. (Before
 * the fix every DM_Poly instance was labelled DM_Poly, and these cases
 * expected it: a correction, so each FD case fails on its label.) */
TEST_CASE_METHOD(TestFixturePoly, "DM_PS_SQ: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_sq(10.0, 0, "DM_PS_SQ");
      test_dm_poly_fd<DM_Poly<F_SQ>, Cut_Dummy>(ctx, "DM_PS_SQ", 1*2);
      test_dm_poly_fd<DM_Poly<F_SQ>, Cut_Cos>  (ctx, "DM_PS_SQ", 1*2); }
    { auto ctx = make_ctx_sq(10.0, 1, "DM_PS_SQ");
      test_dm_poly_fd<DM_Poly<F_SQ>, Cut_Dummy>(ctx, "DM_PS_SQ", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_PS_SQRT: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_sqrt(10.0, 0, "DM_PS_SQRT");
      test_dm_poly_fd<DM_Poly<F_SQRT>, Cut_Dummy>(ctx, "DM_PS_SQRT", 1*2);
      test_dm_poly_fd<DM_Poly<F_SQRT>, Cut_Cos>  (ctx, "DM_PS_SQRT", 1*2); }
    { auto ctx = make_ctx_sqrt(10.0, 1, "DM_PS_SQRT");
      test_dm_poly_fd<DM_Poly<F_SQRT>, Cut_Dummy>(ctx, "DM_PS_SQRT", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_PS_RLR: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_rlr(10.0, 0, "DM_PS_RLR");
      test_dm_poly_fd<DM_Poly<F_RLR>, Cut_Dummy>(ctx, "DM_PS_RLR", 1*2);
      test_dm_poly_fd<DM_Poly<F_RLR>, Cut_Cos>  (ctx, "DM_PS_RLR", 1*2); }
    { auto ctx = make_ctx_rlr(10.0, 1, "DM_PS_RLR");
      test_dm_poly_fd<DM_Poly<F_RLR>, Cut_Dummy>(ctx, "DM_PS_RLR", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_PS_Blip: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_blipf(10.0, 0, "DM_PS_Blip");
      test_dm_poly_fd<DM_Poly<F_Blip>, Cut_Dummy>(ctx, "DM_PS_Blip", 1*2);
      test_dm_poly_fd<DM_Poly<F_Blip>, Cut_Cos>  (ctx, "DM_PS_Blip", 1*2); }
    { auto ctx = make_ctx_blipf(10.0, 1, "DM_PS_Blip");
      test_dm_poly_fd<DM_Poly<F_Blip>, Cut_Dummy>(ctx, "DM_PS_Blip", 2*2); }
}

// -----------------------------------------------------------------------
//  PowerSpectrum / GaussianRadial  (DM_GPS_*)
//  Lmax=0 only (covers the GaussianRadial code path).
// -----------------------------------------------------------------------

TEST_CASE_METHOD(TestFixturePoly, "DM_GPS_SQ: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_sq(10.0, 0, "DM_GPS_SQ");
    test_dm_poly_fd<DM_Poly<F_SQ, GaussianRadial>, Cut_Dummy>(ctx, "DM_GPS_SQ", 1*2);
    test_dm_poly_fd<DM_Poly<F_SQ, GaussianRadial>, Cut_Cos>  (ctx, "DM_GPS_SQ", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GPS_SQRT: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_sqrt(10.0, 0, "DM_GPS_SQRT");
    test_dm_poly_fd<DM_Poly<F_SQRT, GaussianRadial>, Cut_Dummy>(ctx, "DM_GPS_SQRT", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GPS_RLR: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_rlr(10.0, 0, "DM_GPS_RLR");
    test_dm_poly_fd<DM_Poly<F_RLR, GaussianRadial>, Cut_Dummy>(ctx, "DM_GPS_RLR", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GPS_Blip: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_blipf(10.0, 0, "DM_GPS_Blip");
    test_dm_poly_fd<DM_Poly<F_Blip, GaussianRadial>, Cut_Dummy>(ctx, "DM_GPS_Blip", 1*2);
}

// -----------------------------------------------------------------------
//  OrbitalWise / BlipRadial  (DM_OW_*)
//  Tested: Lmax=0 and Lmax=1.
// -----------------------------------------------------------------------

TEST_CASE_METHOD(TestFixturePoly, "DM_OW_SQ: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_sq(10.0, 0, "DM_OW_SQ");
      test_dm_poly_fd<DM_Poly<F_SQ, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_SQ", 1*2);
      test_dm_poly_fd<DM_Poly<F_SQ, BlipRadial, OrbitalWise>, Cut_Cos>  (ctx, "DM_OW_SQ", 1*2); }
    { auto ctx = make_ctx_sq(10.0, 1, "DM_OW_SQ");
      test_dm_poly_fd<DM_Poly<F_SQ, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_SQ", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_OW_SQRT: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_sqrt(10.0, 0, "DM_OW_SQRT");
      test_dm_poly_fd<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_SQRT", 1*2);
      test_dm_poly_fd<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>, Cut_Cos>  (ctx, "DM_OW_SQRT", 1*2); }
    { auto ctx = make_ctx_sqrt(10.0, 1, "DM_OW_SQRT");
      test_dm_poly_fd<DM_Poly<F_SQRT, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_SQRT", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_OW_RLR: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_rlr(10.0, 0, "DM_OW_RLR");
      test_dm_poly_fd<DM_Poly<F_RLR, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_RLR", 1*2);
      test_dm_poly_fd<DM_Poly<F_RLR, BlipRadial, OrbitalWise>, Cut_Cos>  (ctx, "DM_OW_RLR", 1*2); }
    { auto ctx = make_ctx_rlr(10.0, 1, "DM_OW_RLR");
      test_dm_poly_fd<DM_Poly<F_RLR, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_RLR", 2*2); }
}

TEST_CASE_METHOD(TestFixturePoly, "DM_OW_Blip: FD gradient test", "[dm_poly]")
{
    { auto ctx = make_ctx_blipf(10.0, 0, "DM_OW_Blip");
      test_dm_poly_fd<DM_Poly<F_Blip, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_Blip", 1*2);
      test_dm_poly_fd<DM_Poly<F_Blip, BlipRadial, OrbitalWise>, Cut_Cos>  (ctx, "DM_OW_Blip", 1*2); }
    { auto ctx = make_ctx_blipf(10.0, 1, "DM_OW_Blip");
      test_dm_poly_fd<DM_Poly<F_Blip, BlipRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_OW_Blip", 2*2); }
}

// -----------------------------------------------------------------------
//  OrbitalWise / GaussianRadial  (DM_GOW_*)
//  Lmax=0 only.
// -----------------------------------------------------------------------

TEST_CASE_METHOD(TestFixturePoly, "DM_GOW_SQ: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_sq(10.0, 0, "DM_GOW_SQ");
    test_dm_poly_fd<DM_Poly<F_SQ, GaussianRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_GOW_SQ", 1*2);
    test_dm_poly_fd<DM_Poly<F_SQ, GaussianRadial, OrbitalWise>, Cut_Cos>  (ctx, "DM_GOW_SQ", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GOW_SQRT: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_sqrt(10.0, 0, "DM_GOW_SQRT");
    test_dm_poly_fd<DM_Poly<F_SQRT, GaussianRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_GOW_SQRT", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GOW_RLR: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_rlr(10.0, 0, "DM_GOW_RLR");
    test_dm_poly_fd<DM_Poly<F_RLR, GaussianRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_GOW_RLR", 1*2);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GOW_Blip: FD gradient test", "[dm_poly]")
{
    auto ctx = make_ctx_blipf(10.0, 0, "DM_GOW_Blip");
    test_dm_poly_fd<DM_Poly<F_Blip, GaussianRadial, OrbitalWise>, Cut_Dummy>(ctx, "DM_GOW_Blip", 1*2);
}

// -----------------------------------------------------------------------
//  Rotational invariance: PowerSpectrum with Lmax=2.
//  Apply a 30° rotation around the z-axis to all atomic positions and
//  verify that the AED is unchanged within 1e-10.
// -----------------------------------------------------------------------

template <typename DM>
static void test_rotational_invariance(Context &ctx)
{
    ctx.add("ATOM",   "H");
    ctx.add("WATOM",  1);
    ctx.add("TYPEMB", "H");
    ctx.add("TYPEMB", "H");

    Cut_Dummy fc(ctx.get<double>("RCUTMB"));
    DM dm(ctx);
    dm.set_fcut(&fc, false);

    Vec3d x1( 0.0,   0.0,   0.0);
    Vec3d x2( 2.1,   3.3,   4.4);
    Vec3d x3( 5.1,   4.3,   3.4);
    Vec3d x4(-4.11,  5.33, -3.44);

    auto accumulate = [&](Vec3d a1, Vec3d a2, Vec3d a3, Vec3d a4) {
        Vec3d d12=a1-a2, d13=a1-a3, d14=a1-a4;
        rho_type rho1; dm.init_rhoi(rho1);
        dm.calc_rho_mb(1,1,d12,rho1);
        dm.calc_rho_mb(1,1,d13,rho1);
        dm.calc_rho_mb(1,1,d14,rho1);
        aed_type aed(dm.size()); aed.set_zero();
        dm.calc_aed(rho1, aed);
        return aed;
    };

    aed_type aed_orig = accumulate(x1, x2, x3, x4);

    // 30° rotation around z-axis
    const double c = std::cos(M_PI / 6.0);
    const double s = std::sin(M_PI / 6.0);
    auto rot = [c, s](Vec3d v) {
        Vec3d rv = v;
        rv[0] = c * v[0] - s * v[1];
        rv[1] = s * v[0] + c * v[1];
        // rv[2] unchanged
        return rv;
    };

    aed_type aed_rot = accumulate(rot(x1), rot(x2), rot(x3), rot(x4));

    REQUIRE(aed_orig.isApprox(aed_rot, 1e-10));
}

TEST_CASE_METHOD(TestFixturePoly, "DM_PS_SQ: rotational invariance (Lmax=2)", "[dm_poly]")
{
    auto ctx = make_ctx_sq(10.0, 2, "DM_PS_SQ");
    test_rotational_invariance<DM_Poly<F_SQ>>(ctx);
}

TEST_CASE_METHOD(TestFixturePoly, "DM_GPS_SQRT: rotational invariance (Lmax=2)", "[dm_poly]")
{
    auto ctx = make_ctx_sqrt(10.0, 2, "DM_GPS_SQRT");
    test_rotational_invariance<DM_Poly<F_SQRT, GaussianRadial>>(ctx);
}

// -----------------------------------------------------------------------
//  Equivalence cross-validation
// -----------------------------------------------------------------------

// DM_OW_SQ ≡ DM_Blip  (same F=ρ², same BlipRadial basis, same OrbitalWise mode)
TEST_CASE_METHOD(TestFixturePoly, "DM_OW_SQ equivalence with DM_Blip", "[dm_poly]")
{
    const double rcut = 10.0;
    const int Lmax = 0;

    // Build both descriptors with identical numerical parameters
    Context ctx_blip = make_ctx_sq(rcut, Lmax, "DM_Blip");
    ctx_blip.add("ATOM", "H"); ctx_blip.add("WATOM", 1);
    ctx_blip.add("TYPEMB", "H"); ctx_blip.add("TYPEMB", "H");

    Context ctx_poly = make_ctx_sq(rcut, Lmax, "DM_OW_SQ");
    ctx_poly.add("ATOM", "H"); ctx_poly.add("WATOM", 1);
    ctx_poly.add("TYPEMB", "H"); ctx_poly.add("TYPEMB", "H");

    Cut_Dummy fc(rcut);
    DM_Blip db(ctx_blip);
    db.set_fcut(&fc, false);
    DM_Poly<F_SQ, BlipRadial, OrbitalWise> dp(ctx_poly);
    dp.set_fcut(&fc, false);

    REQUIRE(db.size() == dp.size());

    Vec3d x1(0,0,0), x2(2.1,3.3,4.4), x3(5.1,4.3,3.4), x4(-4.11,5.33,-3.44);
    Vec3d d12=x1-x2, d13=x1-x3, d14=x1-x4;

    // AED comparison
    rho_type rho_b, rho_p;
    db.init_rhoi(rho_b); dp.init_rhoi(rho_p);
    db.calc_rho_mb(1,1,d12,rho_b); db.calc_rho_mb(1,1,d13,rho_b); db.calc_rho_mb(1,1,d14,rho_b);
    dp.calc_rho_mb(1,1,d12,rho_p); dp.calc_rho_mb(1,1,d13,rho_p); dp.calc_rho_mb(1,1,d14,rho_p);

    aed_type aed_b(db.size()), aed_p(dp.size());
    aed_b.set_zero(); aed_p.set_zero();
    db.calc_aed(rho_b, aed_b);
    dp.calc_aed(rho_p, aed_p);
    REQUIRE(aed_b.isApprox(aed_p, 1e-12));

    // Force descriptor comparison
    double r12 = sqrt(d12*d12), rsq12 = d12*d12;
    fd_type fd_b(db.size()), fd_p(dp.size());
    fd_b.set_zero(); fd_p.set_zero();
    db.calc_dXijdri(1,1,r12,rsq12,d12,rho_b,fd_b);
    dp.calc_dXijdri(1,1,r12,rsq12,d12,rho_p,fd_p);
    REQUIRE(fd_b.isApprox(fd_p, 1e-12));
}

// DM_GOW_SQ ≡ DM_mEAD<F_SQ>  (GaussianRadial + OrbitalWise + F_SQ)
TEST_CASE_METHOD(TestFixturePoly, "DM_GOW_SQ equivalence with DM_mEAD<F_SQ>", "[dm_poly]")
{
    const double rcut = 10.0;
    const int Lmax = 0;

    // DM_mEAD<F_SQ> context (nparams=5, no CEMBFUNC/SEMBFUNC values)
    Context ctx_mead;
    ctx_mead.add("RCUTMB", rcut);
    ctx_mead.remove("INITMB"); ctx_mead.add("INITMB", "true");
    ctx_mead.add("TYPEMB", "DM_mSQ");
    ctx_mead.add("TYPEMB", Lmax); ctx_mead.add("TYPEMB", Lmax);
    ctx_mead.add("TYPEMB", Lmax); ctx_mead.add("TYPEMB", 0);
    ctx_mead.add("TYPEMB", 0);
    ctx_mead.add("CGRIDMB", "1 2"); ctx_mead.add("SGRIDMB", "0.1 0.2");
    ctx_mead.add("ATOM", "H"); ctx_mead.add("WATOM", 1);
    ctx_mead.add("TYPEMB", "H"); ctx_mead.add("TYPEMB", "H");

    Context ctx_poly = make_ctx_sq(rcut, Lmax, "DM_GOW_SQ");
    ctx_poly.add("ATOM", "H"); ctx_poly.add("WATOM", 1);
    ctx_poly.add("TYPEMB", "H"); ctx_poly.add("TYPEMB", "H");

    Cut_Dummy fc(rcut);
    DM_mEAD<F_SQ> dm(ctx_mead); dm.set_fcut(&fc, false);
    DM_Poly<F_SQ, GaussianRadial, OrbitalWise> dp(ctx_poly); dp.set_fcut(&fc, false);

    REQUIRE(dm.size() == dp.size());

    Vec3d x1(0,0,0), x2(2.1,3.3,4.4), x3(5.1,4.3,3.4), x4(-4.11,5.33,-3.44);
    Vec3d d12=x1-x2, d13=x1-x3, d14=x1-x4;

    // AED comparison
    rho_type rho_m, rho_p;
    dm.init_rhoi(rho_m); dp.init_rhoi(rho_p);
    dm.calc_rho_mb(1,1,d12,rho_m); dm.calc_rho_mb(1,1,d13,rho_m); dm.calc_rho_mb(1,1,d14,rho_m);
    dp.calc_rho_mb(1,1,d12,rho_p); dp.calc_rho_mb(1,1,d13,rho_p); dp.calc_rho_mb(1,1,d14,rho_p);

    aed_type aed_m(dm.size()), aed_p(dp.size());
    aed_m.set_zero(); aed_p.set_zero();
    dm.calc_aed(rho_m, aed_m);
    dp.calc_aed(rho_p, aed_p);
    REQUIRE(aed_m.isApprox(aed_p, 1e-12));

    // Force descriptor comparison
    double r12 = sqrt(d12*d12), rsq12 = d12*d12;
    fd_type fd_m(dm.size()), fd_p(dp.size());
    fd_m.set_zero(); fd_p.set_zero();
    dm.calc_dXijdri(1,1,r12,rsq12,d12,rho_m,fd_m);
    dp.calc_dXijdri(1,1,r12,rsq12,d12,rho_p,fd_p);
    REQUIRE(fd_m.isApprox(fd_p, 1e-12));
}
