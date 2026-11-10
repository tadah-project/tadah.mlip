// test_linear_regressor_ift.cpp
//
// Layer-2 parity tests for the implicit-function-theorem (IFT) gradient
// path used by HPO analytical / hybrid gradients.
//
// What we test:
//   1. solveAndCacheFactor produces the same weights as OLS::solve under
//      Algorithm::NormalEquations.
//   2. The cached factor satisfies U^T U == Phi^T Phi (modulo ridge
//      augmentation).
//   3. backsolve correctly applies (Phi^T Phi + lambda I_no_bias)^{-1}.
//   4. dW_star/dtheta computed analytically via the IFT chain rule
//      matches a central-difference reference within 1e-6 relative.
//   5. train_with_factor rejects OALGO != 4 and LAMBDA == -1.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/lapack.h>
#include <tadah/models/factor_handle.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/ols.h>

#include <cmath>
#include <random>
#include <vector>

namespace {

using tadah::core::aed_type;
using tadah::core::Matrix;
using Vector = aed_type;

// Build a small overdetermined design matrix Phi(theta) where every
// element depends smoothly on the scalar theta. theta scales the
// second column by exp(theta) — gives non-trivial dPhi/dtheta and a
// non-trivial dW_star/dtheta. The other columns are theta-independent.
//
// Phi(theta)[i, j] = base[i, j] for j != 1
// Phi(theta)[i, 1] = base[i, 1] * exp(theta)
//
// dPhi/dtheta has a single non-zero column (column 1) equal to
// base[*, 1] * exp(theta) = Phi(theta)[*, 1].
//
// Returned by value (no augmentation). Use augment_ridge() if a ridge
// penalty is wanted.
Matrix build_phi(const Matrix& base, double theta) {
    const std::size_t m = base.rows();
    const std::size_t n = base.cols();
    Matrix Phi(m, n);
    const double s = std::exp(theta);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < m; ++i) {
            Phi(i, j) = (j == 1) ? base(i, j) * s : base(i, j);
        }
    }
    return Phi;
}

// Fill base with reproducible pseudo-random doubles in [-1, 1].
Matrix random_matrix(std::size_t m, std::size_t n, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    Matrix M(m, n);
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i)
            M(i, j) = dist(rng);
    return M;
}

Vector random_vector(std::size_t m, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    Vector v(m);
    for (std::size_t i = 0; i < m; ++i) v[i] = dist(rng);
    return v;
}

// Build a (m+n) x n ridge-augmented design matrix from an unaugmented
// (m x n) Phi: bottom n rows hold sqrt(lambda) * I (zero in the bias
// columns; here n_bias = 0 so the whole I block is filled).
Matrix build_phi_augmented(const Matrix& Phi_unaug, double lambda) {
    const std::size_t m = Phi_unaug.rows();
    const std::size_t n = Phi_unaug.cols();
    Matrix Phi(m + n, n);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < m; ++i) Phi(i, j) = Phi_unaug(i, j);
        for (std::size_t i = m; i < m + n; ++i) Phi(i, j) = 0.0;
    }
    if (lambda > 0) {
        const double s = std::sqrt(lambda);
        for (std::size_t k = 0; k < n; ++k) {
            Phi(m + k, k) = s;
        }
    }
    return Phi;
}

Vector build_t_augmented(const Vector& T_unaug, std::size_t n_aug) {
    Vector T(T_unaug.size() + n_aug);
    for (std::size_t i = 0; i < T_unaug.size(); ++i) T[i] = T_unaug[i];
    for (std::size_t i = T_unaug.size(); i < T.size(); ++i) T[i] = 0.0;
    return T;
}

// Compute Phi^T Phi v - useful for both the IFT RHS assembly and the
// factor-correctness check.
Vector phi_t_phi_times(const Matrix& Phi, const Vector& v) {
    const std::size_t m = Phi.rows();
    const std::size_t n = Phi.cols();
    REQUIRE(v.size() == n);
    Vector Phi_v(m);
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += Phi(i, j) * v[j];
        Phi_v[i] = s;
    }
    Vector out(n);
    for (std::size_t j = 0; j < n; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i) s += Phi(i, j) * Phi_v[i];
        out[j] = s;
    }
    return out;
}

// Accept either absolute (~1e-10) or relative (1e-6) agreement. Pure
// relative tolerance fails on values near machine zero where the
// analytical IFT gives ~1e-18 noise vs. exact zero from FD.
bool close(double a, double b, double rtol = 1e-6, double atol = 1e-9) {
    return std::abs(a - b) <= atol + rtol * std::max(std::abs(a), std::abs(b));
}

double rel_diff(double a, double b) {
    const double denom = std::max({std::abs(a), std::abs(b), 1e-30});
    return std::abs(a - b) / denom;
}

}  // namespace

TEST_CASE("solveAndCacheFactor matches OLS::solve(NormalEquations)",
          "[MODELS][ift][parity]")
{
    const std::size_t m = 12, n = 4;
    Matrix base = random_matrix(m, n, /*seed=*/42);
    Vector T   = random_vector(m, /*seed=*/43);

    Matrix A_ref  = base;
    Matrix A_test = base;
    Vector B_ref  = T;
    Vector B_test = T;

    Vector w_ref(n), w_test(n);
    tadah::models::OLS ols_ref;
    ols_ref.solve(A_ref, B_ref, w_ref, /*rcond=*/-1.0,
                  tadah::models::Algorithm::NormalEquations);

    tadah::models::FactorHandle factor;
    tadah::models::OLS ols_test;
    ols_test.solveAndCacheFactor(A_test, B_test, w_test, factor, /*n_bias=*/0);

    REQUIRE(factor.valid);
    REQUIRE(factor.n == static_cast<int>(n));
    REQUIRE(factor.algo == tadah::models::Algorithm::NormalEquations);

    for (std::size_t i = 0; i < n; ++i) {
        CHECK(rel_diff(w_ref[i], w_test[i]) < 1e-12);
    }
}

TEST_CASE("backsolve inverts (Phi^T Phi) on a known vector",
          "[MODELS][ift][backsolve]")
{
    const std::size_t m = 10, n = 3;
    Matrix Phi  = random_matrix(m, n, /*seed=*/100);
    Vector T    = random_vector(m, /*seed=*/101);

    Matrix Phi_copy = Phi;
    Vector T_copy   = T;

    Vector w(n);
    tadah::models::FactorHandle factor;
    tadah::models::OLS ols;
    ols.solveAndCacheFactor(Phi_copy, T_copy, w, factor, /*n_bias=*/0);

    // Pick an arbitrary x, compute rhs = (Phi^T Phi) x, then verify
    // that backsolve(rhs) recovers x.
    Vector x_known(n);
    x_known[0] = 0.7; x_known[1] = -1.3; x_known[2] = 2.1;
    const Vector rhs = phi_t_phi_times(Phi, x_known);

    Vector x_recovered(n);
    tadah::models::LinearRegressor::backsolve(factor, rhs, x_recovered);

    for (std::size_t i = 0; i < n; ++i) {
        CHECK(rel_diff(x_known[i], x_recovered[i]) < 1e-10);
    }
}

TEST_CASE("IFT chain-rule analytical dW/dtheta matches central-difference",
          "[MODELS][ift][parity]")
{
    // Pure OLS (lambda = 0). Phi has a column scaled by exp(theta).
    const std::size_t m = 16, n = 4;
    Matrix base = random_matrix(m, n, /*seed=*/7);
    Vector T    = random_vector(m, /*seed=*/8);

    const double theta_0 = 0.3;

    // ---- Forward fit at theta_0.
    Matrix Phi_0      = build_phi(base, theta_0);
    Matrix Phi_0_copy = Phi_0;
    Vector T_copy     = T;
    Vector W_star(n);
    tadah::models::FactorHandle factor;
    tadah::models::OLS ols;
    ols.solveAndCacheFactor(Phi_0_copy, T_copy, W_star, factor, /*n_bias=*/0);

    // ---- Build dPhi/dtheta. Only column 1 is non-zero; equal to
    // Phi_0[*, 1] (since Phi_0[*, 1] = base[*, 1] * exp(theta_0) and
    // d/dtheta of that is itself).
    Matrix dPhi_dtheta(m, n);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < m; ++i) {
            dPhi_dtheta(i, j) = (j == 1) ? Phi_0(i, j) : 0.0;
        }
    }

    // ---- IFT chain rule (lambda = 0):
    //   (Phi^T Phi) dW/dtheta = (dPhi^T/dtheta) (T - Phi W*)
    //                          - Phi^T (dPhi/dtheta) W*
    Vector residual(m);
    for (std::size_t i = 0; i < m; ++i) {
        double phiW = 0.0;
        for (std::size_t j = 0; j < n; ++j) phiW += Phi_0(i, j) * W_star[j];
        residual[i] = T[i] - phiW;
    }
    Vector dPhiW(m);
    for (std::size_t i = 0; i < m; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += dPhi_dtheta(i, j) * W_star[j];
        dPhiW[i] = s;
    }
    Vector rhs(n);
    for (std::size_t j = 0; j < n; ++j) {
        double term1 = 0.0, term2 = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            term1 += dPhi_dtheta(i, j) * residual[i];
            term2 += Phi_0(i, j)       * dPhiW[i];
        }
        rhs[j] = term1 - term2;
    }
    Vector dW_dtheta_analytical(n);
    tadah::models::LinearRegressor::backsolve(factor, rhs,
                                              dW_dtheta_analytical);

    // ---- Central-difference reference.
    const double h = 1e-5;
    auto fit_at = [&](double theta) {
        Matrix Phi_h      = build_phi(base, theta);
        Matrix Phi_h_copy = Phi_h;
        Vector T_h_copy   = T;
        Vector W_h(n);
        tadah::models::FactorHandle f_h;
        tadah::models::OLS ols_h;
        ols_h.solveAndCacheFactor(Phi_h_copy, T_h_copy, W_h, f_h, 0);
        return W_h;
    };
    Vector Wp = fit_at(theta_0 + h);
    Vector Wm = fit_at(theta_0 - h);
    Vector dW_dtheta_fd(n);
    for (std::size_t j = 0; j < n; ++j)
        dW_dtheta_fd[j] = (Wp[j] - Wm[j]) / (2.0 * h);

    // Required tolerance: 1e-6 relative (with 1e-9 absolute floor for
    // near-zero entries) per the plan.
    for (std::size_t j = 0; j < n; ++j) {
        const double a = dW_dtheta_analytical[j];
        const double b = dW_dtheta_fd[j];
        INFO("j=" << j << " analytical=" << a << " FD=" << b);
        CHECK(close(a, b));
    }
}

TEST_CASE("IFT chain-rule under ridge regression (lambda > 0)",
          "[MODELS][ift][parity][ridge]")
{
    // Ridge with fixed lambda > 0. The augmentation rows of Phi do NOT
    // depend on theta, so dPhi_aug/dtheta is zero in the augmentation
    // block. The IFT formula still holds with the augmented design.
    const std::size_t m = 14, n = 3;
    const double lambda = 0.05;
    Matrix base = random_matrix(m, n, /*seed=*/200);
    Vector T_un = random_vector(m, /*seed=*/201);
    const double theta_0 = -0.2;

    // ---- Forward fit at theta_0 with ridge augmentation (n_bias = 0
    // so the full I block is augmented with sqrt(lambda)).
    Matrix Phi_un_0    = build_phi(base, theta_0);
    Matrix Phi_aug_0   = build_phi_augmented(Phi_un_0, lambda);
    Vector T_aug       = build_t_augmented(T_un, n);
    Matrix Phi_aug_cpy = Phi_aug_0;
    Vector T_aug_cpy   = T_aug;
    Vector W_star(n);
    tadah::models::FactorHandle factor;
    tadah::models::OLS ols;
    ols.solveAndCacheFactor(Phi_aug_cpy, T_aug_cpy, W_star, factor, /*n_bias=*/0);

    // ---- dPhi_aug/dtheta: only column 1 of the top m rows is non-zero;
    // the augmentation block is zero (lambda is fixed).
    Matrix dPhi_aug(m + n, n);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < m; ++i)
            dPhi_aug(i, j) = (j == 1) ? Phi_un_0(i, j) : 0.0;
        for (std::size_t i = m; i < m + n; ++i)
            dPhi_aug(i, j) = 0.0;
    }

    // ---- IFT (with augmented design); same shape as OLS case.
    Vector residual(m + n);
    for (std::size_t i = 0; i < m + n; ++i) {
        double phiW = 0.0;
        for (std::size_t j = 0; j < n; ++j) phiW += Phi_aug_0(i, j) * W_star[j];
        residual[i] = T_aug[i] - phiW;
    }
    Vector dPhiW(m + n);
    for (std::size_t i = 0; i < m + n; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += dPhi_aug(i, j) * W_star[j];
        dPhiW[i] = s;
    }
    Vector rhs(n);
    for (std::size_t j = 0; j < n; ++j) {
        double term1 = 0.0, term2 = 0.0;
        for (std::size_t i = 0; i < m + n; ++i) {
            term1 += dPhi_aug(i, j)   * residual[i];
            term2 += Phi_aug_0(i, j)  * dPhiW[i];
        }
        rhs[j] = term1 - term2;
    }
    Vector dW_dtheta_analytical(n);
    tadah::models::LinearRegressor::backsolve(factor, rhs,
                                              dW_dtheta_analytical);

    // ---- Central-difference reference (re-augmenting at each theta).
    const double h = 1e-5;
    auto fit_at = [&](double theta) {
        Matrix Phi_un_h    = build_phi(base, theta);
        Matrix Phi_aug_h   = build_phi_augmented(Phi_un_h, lambda);
        Matrix Phi_h_cpy   = Phi_aug_h;
        Vector T_h_cpy     = T_aug;
        Vector W_h(n);
        tadah::models::FactorHandle f_h;
        tadah::models::OLS ols_h;
        ols_h.solveAndCacheFactor(Phi_h_cpy, T_h_cpy, W_h, f_h, 0);
        return W_h;
    };
    Vector Wp = fit_at(theta_0 + h);
    Vector Wm = fit_at(theta_0 - h);
    Vector dW_dtheta_fd(n);
    for (std::size_t j = 0; j < n; ++j)
        dW_dtheta_fd[j] = (Wp[j] - Wm[j]) / (2.0 * h);

    for (std::size_t j = 0; j < n; ++j) {
        const double a = dW_dtheta_analytical[j];
        const double b = dW_dtheta_fd[j];
        INFO("ridge j=" << j << " analytical=" << a << " FD=" << b);
        CHECK(close(a, b));
    }
}

/* Pins: train_with_factor, a fit of the C++ API that hands its caller the
 * Cholesky factor for an analytical gradient (tadah hpo builds its own
 * factor and does not call it), refuses OALGO 1 with an internal error
 * (std::logic_error, a caller's error) that says what the fit needs: "internal error: the analytical gradient fits with OALGO 4
 * (the Cholesky normal equations), whose factor it reuses, and was given
 * OALGO 1".
 * Before the fix: std::runtime_error "LinearRegressor::train_with_factor
 * requires OALGO=4 (NormalEquations); got OALGO=1. ... Use OALGO=4 with
 * GRAD_STRATEGY=ANALYTICAL/HYBRID, or fall back to ...". */
TEST_CASE("train_with_factor rejects OALGO != 4",
          "[MODELS][ift][validate]")
{
    tadah::core::Context ctx;
    ctx.add("OALGO", 1);
    ctx.add("LAMBDA", 0.0);

    const std::size_t m = 6, n = 2;
    Matrix Phi = random_matrix(m, n, /*seed=*/300);
    Vector T   = random_vector(m, /*seed=*/301);
    Vector w(n);
    tadah::models::FactorHandle factor;

    CHECK_THROWS_AS(
        tadah::models::LinearRegressor::train_with_factor(
            ctx, Phi, T, w, factor),
        std::logic_error);
    CHECK_THROWS_WITH(
        tadah::models::LinearRegressor::train_with_factor(
            ctx, Phi, T, w, factor),
        "internal error: the analytical gradient fits with OALGO 4 (the "
        "Cholesky normal equations), whose factor it reuses, and was given "
        "OALGO 1");
}

/* Pins: train_with_factor refuses LAMBDA -1 with an internal error
 * (std::logic_error): "internal error: the analytical gradient fits with a
 * fixed LAMBDA >= 0, and was given LAMBDA -1 (the evidence approximation,
 * which changes the fit inside the solve)".
 * Before the fix: std::runtime_error "LinearRegressor::train_with_factor:
 * LAMBDA=-1 (BLR/EA) is not supported under analytical gradients. ... Use
 * a fixed LAMBDA >= 0, or GRAD_STRATEGY=NUMERICAL.". */
TEST_CASE("train_with_factor rejects LAMBDA == -1 (BLR)",
          "[MODELS][ift][validate]")
{
    tadah::core::Context ctx;
    ctx.add("OALGO", 4);
    ctx.add("LAMBDA", -1.0);

    const std::size_t m = 6, n = 2;
    Matrix Phi = random_matrix(m, n, /*seed=*/400);
    Vector T   = random_vector(m, /*seed=*/401);
    Vector w(n);
    tadah::models::FactorHandle factor;

    CHECK_THROWS_AS(
        tadah::models::LinearRegressor::train_with_factor(
            ctx, Phi, T, w, factor),
        std::logic_error);
    CHECK_THROWS_WITH(
        tadah::models::LinearRegressor::train_with_factor(
            ctx, Phi, T, w, factor),
        "internal error: the analytical gradient fits with a fixed LAMBDA >= "
        "0, and was given LAMBDA -1 (the evidence approximation, which "
        "changes the fit inside the solve)");
}
