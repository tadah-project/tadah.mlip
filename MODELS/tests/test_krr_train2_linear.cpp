// test_krr_train2_linear.cpp
//
// M_KRR_Train::train2, the solve of MODEL M_KRR <kernel> 2, finds the
// coefficients w of its basis vectors b_j (the columns of B) from
// (K + LAMBDA sigma^2 I) w = T, K_ij = K(b_i, b_j), sigma the error scale
// of the targets (1 by default). A non-linear kernel predicts
// with them: E(x) = sum_j w_j K(b_j, x). Kern_Linear predicts weights . x
// (Kern_Linear::epredict, as everything that evaluates a Kern_Linear
// model), so its weights are the descriptor weights the coefficients make,
// sum_j w_j b_j = B w, one per descriptor component.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/m_krr_train.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

// Four basis vectors of 8 components, the columns of a deterministic,
// well-conditioned 8 x 4 matrix (as test_model_sigma and test_fixed_weights).
Matrix make_basis() {
  Matrix B(8, 4);
  double v = 0.17;
  for (size_t j = 0; j < B.cols(); ++j)
    for (size_t i = 0; i < B.rows(); ++i) {
      v = std::fmod(v * 1.618 + 0.31 * static_cast<double>(j + 1), 1.0) - 0.5;
      B(i, j) = v + (i == j ? 2.0 : 0.0);
    }
  return B;
}

// The solution of A x = b, Gaussian elimination with partial pivoting.
std::vector<double> solve(std::vector<std::vector<double>> A,
                          std::vector<double> b) {
  const size_t n = b.size();
  for (size_t k = 0; k < n; ++k) {
    size_t p = k;
    for (size_t i = k + 1; i < n; ++i)
      if (std::fabs(A[i][k]) > std::fabs(A[p][k])) p = i;
    std::swap(A[k], A[p]);
    std::swap(b[k], b[p]);
    for (size_t i = k + 1; i < n; ++i) {
      const double f = A[i][k] / A[k][k];
      for (size_t j = k; j < n; ++j) A[i][j] -= f * A[k][j];
      b[i] -= f * b[k];
    }
  }
  std::vector<double> x(n);
  for (size_t k = n; k-- > 0;) {
    double s = b[k];
    for (size_t j = k + 1; j < n; ++j) s -= A[k][j] * x[j];
    x[k] = s / A[k][k];
  }
  return x;
}

// The coefficients of the basis vectors, (K + lambda I) w = T with
// K_ij = kernel(b_i, b_j).
template <class Kern>
std::vector<double> coefficients(const Kern &kernel, const Matrix &B,
                                 const t_type &T, double lambda) {
  std::vector<std::vector<double>> K(B.cols(), std::vector<double>(B.cols()));
  std::vector<double> t(B.cols());
  for (size_t i = 0; i < B.cols(); ++i) {
    t[i] = T[i];
    for (size_t j = 0; j < B.cols(); ++j)
      K[i][j] = kernel(B.col(i), B.col(j)) + (i == j ? lambda : 0.0);
  }
  return solve(K, t);
}

Context make_ctx() {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", "1e-3");
  c.add("OALGO", "1");
  c.add("MPARAMS", "1.0");
  return c;
}

t_type make_targets(size_t n) {
  t_type T(n);
  for (size_t j = 0; j < n; ++j) T[j] = 0.3 * static_cast<double>(j + 1);
  return T;
}

}  // namespace

/* Pins: M_KRR_Train<Kern_Linear>::train2 on four basis vectors of 8
 * components (the columns of the 8 x 4 matrix B, targets 0.3, 0.6, 0.9,
 * 1.2, LAMBDA 1e-3) has 8 weights, B w, w the solution of
 * (B^T B + 1e-3 I) w = T solved here by Gaussian elimination (1e-12
 * relative); its prediction at basis vector b_j is (K w)_j =
 * T_j - 1e-3 w_j (1e-12 relative), the value the solve fits.
 * Before the fix: the model's weights were the 4 coefficients w, so the
 * REQUIRE on 8 weights failed (Kern_Linear's prediction took them for the
 * weights of the first 4 descriptor components, and `tadah predict`
 * refused the potential, or with SBASIS = DSIZE predicted wrong
 * energies). */
TEST_CASE("M_KRR implementation 2 (train2) with Kern_Linear has the "
          "descriptor weights of its basis vectors",
          "[krr][train2]") {
  Context c = make_ctx();
  const Matrix B = make_basis();
  const t_type T = make_targets(B.cols());
  Kern_Linear k;
  M_KRR_Train<Kern_Linear> m(k, c);
  Matrix Bt = B;
  t_type Tt = T;
  REQUIRE_NOTHROW(m.train2(Bt, Tt));
  const t_type w = m.get_weights();
  const std::vector<double> coef = coefficients(k, B, T, 1e-3);
  REQUIRE(w.size() == B.rows());
  for (size_t i = 0; i < B.rows(); ++i) {
    double Bw = 0;
    for (size_t j = 0; j < B.cols(); ++j) Bw += B(i, j) * coef[j];
    INFO("weight " << i);
    CHECK(w[i] == Approx(Bw).epsilon(1e-12).margin(1e-300));
  }
  for (size_t j = 0; j < B.cols(); ++j) {
    INFO("basis vector " << j);
    CHECK(m.predict(B.col(j)) ==
          Approx(T[j] - 1e-3 * coef[j]).epsilon(1e-12));
  }
}

/* Pins: M_KRR_Train<Kern_RBF>::train2 (MPARAMS 1.0) on the same basis
 * keeps one weight per basis vector, the 4 coefficients w of
 * (K + 1e-3 I) w = T with K_ij = exp(-|b_i - b_j|^2 / 2), solved here by
 * Gaussian elimination (1e-12 relative): a non-linear kernel predicts with
 * its basis vectors.
 * Before the fix: passes as well (a pin: only Kern_Linear's weights are
 * mapped to the descriptor). */
TEST_CASE("M_KRR implementation 2 (train2) with Kern_RBF keeps the weights "
          "of its basis vectors",
          "[krr][train2]") {
  Context c = make_ctx();
  const Matrix B = make_basis();
  const t_type T = make_targets(B.cols());
  Kern_RBF k(c);
  M_KRR_Train<Kern_RBF> m(k, c);
  Matrix Bt = B;
  t_type Tt = T;
  REQUIRE_NOTHROW(m.train2(Bt, Tt));
  const t_type w = m.get_weights();
  const std::vector<double> coef = coefficients(k, B, T, 1e-3);
  REQUIRE(w.size() == B.cols());
  for (size_t j = 0; j < B.cols(); ++j) {
    INFO("weight " << j);
    CHECK(w[j] == Approx(coef[j]).epsilon(1e-12).margin(1e-300));
  }
}

/* Pins: train2 with an error scale sigma of the targets solves (K + LAMBDA
 * sigma^2 I) w = T, the ridge of the residuals over sigma: Kern_Linear and
 * Kern_RBF on the basis above with LAMBDA 1e-3 and sigma 0.05 have the
 * weights of the solve with lambda 1e-3 x 0.05^2 here (1e-12 relative); an
 * error scale of 0, -1 or NaN is a logic_error (Tadah.MLIP refuses such an
 * ESIGMA before a fit).
 * Before the fix: train2 took no error scale (the test does not compile),
 * so Tadah.MLIP's ESIGMA did not reach the solve. */
TEST_CASE("M_KRR implementation 2 (train2) weighs LAMBDA against the "
          "residuals over the error scale",
          "[krr][train2][error_scale]") {
  Context c = make_ctx();
  const Matrix B = make_basis();
  const t_type T = make_targets(B.cols());
  const double sigma = 0.05;
  SECTION("Kern_Linear") {
    Kern_Linear k;
    M_KRR_Train<Kern_Linear> m(k, c);
    Matrix Bt = B;
    t_type Tt = T;
    REQUIRE_NOTHROW(m.train2(Bt, Tt, sigma));
    const t_type w = m.get_weights();
    const std::vector<double> coef =
        coefficients(k, B, T, 1e-3 * sigma * sigma);
    REQUIRE(w.size() == B.rows());
    for (size_t i = 0; i < B.rows(); ++i) {
      double Bw = 0;
      for (size_t j = 0; j < B.cols(); ++j) Bw += B(i, j) * coef[j];
      INFO("weight " << i);
      CHECK(w[i] == Approx(Bw).epsilon(1e-12).margin(1e-300));
    }
  }
  SECTION("Kern_RBF") {
    Kern_RBF k(c);
    M_KRR_Train<Kern_RBF> m(k, c);
    Matrix Bt = B;
    t_type Tt = T;
    REQUIRE_NOTHROW(m.train2(Bt, Tt, sigma));
    const t_type w = m.get_weights();
    const std::vector<double> coef =
        coefficients(k, B, T, 1e-3 * sigma * sigma);
    REQUIRE(w.size() == B.cols());
    for (size_t j = 0; j < B.cols(); ++j) {
      INFO("weight " << j);
      CHECK(w[j] == Approx(coef[j]).epsilon(1e-12).margin(1e-300));
    }
  }
  SECTION("an error scale that is not a finite number > 0") {
    for (double bad : {0.0, -1.0, std::nan("")}) {
      INFO("error scale " << bad);
      Kern_Linear k;
      M_KRR_Train<Kern_Linear> m(k, c);
      Matrix Bt = B;
      t_type Tt = T;
      CHECK_THROWS_AS(m.train2(Bt, Tt, bad), std::logic_error);
    }
  }
}
