// test_oalgo3_solve.cpp
//
// The weights an OALGO 3 fit (LinearRegressor::train, the SVD solve)
// returns: the ridge solution
//
//     w = sum_{kept i} v_i s_i (u_i . T) / (s_i^2 + lambda),
//
// where A = U S V^T is the (weighted) design matrix and the kept
// directions are those with s_i > rcond * s_max (with every direction kept
// w = (A^T A + lambda I)^-1 A^T T); the evidence approximation of LAMBDA -1,
// which counts the observations (the rows of A without the
// ridge-augmentation rows the design matrix appends, one per column, all
// zero for OALGO 3); and the WARNING of the directions the solve leaves
// out.
//
// The references are computed here in long double from the real rows of A
// alone (Gauss-Jordan inverses, Gram-Schmidt bases), never through the
// library's SVD.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/logger.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/functions/kernels/kern_rbf.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>
#include <tadah/models/memory/models_workspace_manager.h>
#include <tadah/models/ols.h>
#include <tadah/models/svd.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

using LD = long double;
using LMat = std::vector<std::vector<LD>>;  // row-major, [row][col]

/* A deterministic, well-conditioned m x n matrix (row-major, long
 * double): entries in [-0.5, 0.5) plus 2 on the diagonal. */
LMat make_A(size_t m, size_t n, double seed) {
  LMat A(m, std::vector<LD>(n));
  double v = seed;
  for (size_t j = 0; j < n; ++j)
    for (size_t i = 0; i < m; ++i) {
      v = std::fmod(v * 1.618 + 0.31 * static_cast<double>(j + 1), 1.0) - 0.5;
      A[i][j] = v + (i == j ? 2.0 : 0.0);
    }
  return A;
}

/* Targets of A: A w_true plus a perturbation, so the residuals (and s^2)
 * are not zero. */
std::vector<LD> make_T(const LMat &A) {
  const size_t m = A.size(), n = A[0].size();
  std::vector<LD> T(m, 0.0L);
  for (size_t i = 0; i < m; ++i) {
    for (size_t j = 0; j < n; ++j)
      T[i] += A[i][j] * 0.5L * static_cast<LD>(j + 1) * (j % 2 ? -1 : 1);
    T[i] += 0.05L * std::sin(1.7L * static_cast<LD>(i) + 0.3L);
  }
  return T;
}

/* The design matrix as the DesignMatrix gives it: the m real rows of A,
 * then n ridge-augmentation rows, zero (OALGO 3 writes none). */
phi_type with_augmentation(const LMat &A) {
  const size_t m = A.size(), n = A[0].size();
  phi_type Phi(m + n, n);
  Phi.set_zero();
  for (size_t i = 0; i < m; ++i)
    for (size_t j = 0; j < n; ++j) Phi(i, j) = static_cast<double>(A[i][j]);
  return Phi;
}

t_type target_with_augmentation(const std::vector<LD> &T, size_t n) {
  t_type t(T.size() + n);
  for (size_t i = 0; i < t.size(); ++i)
    t[i] = i < T.size() ? static_cast<double>(T[i]) : 0.0;
  return t;
}

LMat transpose_times(const LMat &A, const LMat &B) {  // A^T B
  const size_t m = A.size(), n = A[0].size(), p = B[0].size();
  LMat C(n, std::vector<LD>(p, 0.0L));
  for (size_t i = 0; i < m; ++i)
    for (size_t a = 0; a < n; ++a)
      for (size_t b = 0; b < p; ++b) C[a][b] += A[i][a] * B[i][b];
  return C;
}

/* Gauss-Jordan inverse with partial pivoting (long double). */
LMat inverse(LMat M) {
  const size_t n = M.size();
  LMat I(n, std::vector<LD>(n, 0.0L));
  for (size_t i = 0; i < n; ++i) I[i][i] = 1.0L;
  for (size_t c = 0; c < n; ++c) {
    size_t p = c;
    for (size_t r = c + 1; r < n; ++r)
      if (std::fabs(M[r][c]) > std::fabs(M[p][c])) p = r;
    std::swap(M[c], M[p]);
    std::swap(I[c], I[p]);
    const LD d = M[c][c];
    for (size_t k = 0; k < n; ++k) {
      M[c][k] /= d;
      I[c][k] /= d;
    }
    for (size_t r = 0; r < n; ++r)
      if (r != c) {
        const LD f = M[r][c];
        for (size_t k = 0; k < n; ++k) {
          M[r][k] -= f * M[c][k];
          I[r][k] -= f * I[c][k];
        }
      }
  }
  return I;
}

/* The ridge fit of the real rows: w = (A^T A + lambda I)^-1 A^T T, its RSS
 * and gamma = tr(A^T A (A^T A + lambda I)^-1). */
struct RidgeRef {
  std::vector<LD> w;
  LD rss = 0, gamma = 0;
};

RidgeRef ridge_reference(const LMat &A, const std::vector<LD> &T, LD lambda) {
  const size_t m = A.size(), n = A[0].size();
  LMat AtA = transpose_times(A, A);
  LMat M = AtA;
  for (size_t i = 0; i < n; ++i) M[i][i] += lambda;
  const LMat Minv = inverse(M);
  std::vector<LD> AtT(n, 0.0L);
  for (size_t i = 0; i < m; ++i)
    for (size_t j = 0; j < n; ++j) AtT[j] += A[i][j] * T[i];
  RidgeRef r;
  r.w.assign(n, 0.0L);
  for (size_t a = 0; a < n; ++a)
    for (size_t b = 0; b < n; ++b) r.w[a] += Minv[a][b] * AtT[b];
  for (size_t i = 0; i < m; ++i) {
    LD e = T[i];
    for (size_t j = 0; j < n; ++j) e -= A[i][j] * r.w[j];
    r.rss += e * e;
  }
  for (size_t a = 0; a < n; ++a)
    for (size_t b = 0; b < n; ++b) r.gamma += AtA[a][b] * Minv[b][a];
  return r;
}

Context oalgo3_context(const std::string &lambda) {
  Context c;
  c.add("VERBOSE", "0");
  c.add("LAMBDA", lambda);
  c.add("OALGO", "3");
  return c;
}

/* Captures std::cout (where the Logger prints) at WARNING level and
 * restores the stream and the level afterwards. */
class LogCapture {
public:
  LogCapture()
      : old_level_(Logger::instance().getLogLevel()),
        old_out_(std::cout.rdbuf(out_.rdbuf())) {
    Logger::instance().setLogLevel(LogLevel::WARNING);
  }
  ~LogCapture() {
    std::cout.rdbuf(old_out_);
    Logger::instance().setLogLevel(old_level_);
  }
  LogCapture(const LogCapture &) = delete;
  LogCapture &operator=(const LogCapture &) = delete;
  std::string text() const { return out_.str(); }

private:
  std::ostringstream out_;
  LogLevel old_level_;
  std::streambuf *old_out_;
};

/* Orthonormal columns from the columns of @p B (Gram-Schmidt twice). */
LMat orthonormal_columns(LMat B) {
  const size_t m = B.size(), n = B[0].size();
  for (size_t j = 0; j < n; ++j)
    for (int pass = 0; pass < 2; ++pass) {
      for (size_t k = 0; k < j; ++k) {
        LD d = 0;
        for (size_t i = 0; i < m; ++i) d += B[i][k] * B[i][j];
        for (size_t i = 0; i < m; ++i) B[i][j] -= d * B[i][k];
      }
      LD nrm = 0;
      for (size_t i = 0; i < m; ++i) nrm += B[i][j] * B[i][j];
      nrm = std::sqrt(nrm);
      for (size_t i = 0; i < m; ++i) B[i][j] /= nrm;
    }
  return B;
}

}  // namespace

/* Pins: with OALGO 3 the weights of LinearRegressor::train,
 * M_BLR_Train<BF_Linear>, M_KRR_Train<Kern_Linear> and
 * M_KRR_Train<Kern_RBF> (whose design-matrix columns are its basis
 * vectors, kept as they are) on a random 9 x 4 A with its 4
 * ridge-augmentation rows are the ridge solution (A^T A + lambda I)^-1 A^T
 * T of the 9 real rows, for LAMBDA 1e-3 and LAMBDA 0, to 1e-10 relative.
 * Before the fix: passes as well (the weights were the solve's; the case
 * pinned the covariance of the fit with them). */
TEST_CASE("OALGO 3: the weights are the ridge solution",
          "[oalgo3]") {
  const LMat A = make_A(9, 4, 0.17);
  const std::vector<LD> T = make_T(A);
  for (const char *lambda : {"1e-3", "0"}) {
    const RidgeRef ref = ridge_reference(A, T, std::stold(lambda));
    for (const char *who : {"LinearRegressor", "M_BLR_Train<BF_Linear>",
                            "M_KRR_Train<Kern_Linear>",
                            "M_KRR_Train<Kern_RBF>"}) {
      DYNAMIC_SECTION(who << ", LAMBDA " << lambda) {
        Context c = oalgo3_context(lambda);
        c.add("MPARAMS", "1.0");  // Kern_RBF's sigma
        phi_type Phi = with_augmentation(A);
        t_type t = target_with_augmentation(T, 4);
        t_type w;
        const std::string name = who;
        if (name == "LinearRegressor") {
          LinearRegressor::train(c, Phi, t, w);
        } else if (name == "M_BLR_Train<BF_Linear>") {
          BF_Linear bf;
          M_BLR_Train<BF_Linear> m(bf, c);
          m.train(Phi, t);
          w = m.get_weights();
        } else if (name == "M_KRR_Train<Kern_Linear>") {
          Kern_Linear k;
          M_KRR_Train<Kern_Linear> m(k, c);
          m.train(Phi, t);
          w = m.get_weights();
        } else {
          Kern_RBF k(c);
          M_KRR_Train<Kern_RBF> m(k, c);
          m.train(Phi, t);
          w = m.get_weights();
        }
        REQUIRE(w.size() == 4);
        for (size_t j = 0; j < 4; ++j)
          CHECK(std::fabs(w[j] - static_cast<double>(ref.w[j])) <
                1e-10 * std::fabs(static_cast<double>(ref.w[j])));
      }
    }
  }
}

/* Pins: with OALGO 3, LAMBDA 0 and two equal columns (A = [a0 a1 a2 a2],
 * 9 real rows) LinearRegressor::train logs a WARNING that 1 of the 4
 * singular directions is left out of the weights.
 * Before the fix: the WARNING named the weight covariance WCOV too (the
 * case pinned that covariance as well). */
TEST_CASE("OALGO 3, LAMBDA 0, rank deficient: a WARNING names the dropped "
          "direction",
          "[oalgo3]") {
  LMat B = make_A(9, 3, 0.29);
  LMat A(9, std::vector<LD>(4));
  for (size_t i = 0; i < 9; ++i) {
    for (size_t j = 0; j < 3; ++j) A[i][j] = B[i][j];
    A[i][3] = B[i][2];
  }
  const std::vector<LD> T = make_T(A);
  Context c = oalgo3_context("0");
  phi_type Phi = with_augmentation(A);
  t_type t = target_with_augmentation(T, 4);
  t_type w;
  std::string log;
  {
    LogCapture cap;
    LinearRegressor::train(c, Phi, t, w);
    log = cap.text();
  }
  INFO("log: " << log);
  REQUIRE(w.size() == 4);
  CHECK(log.find("1 of 4 singular directions") != std::string::npos);
}

/* Pins: with OALGO 3 and LAMBDA 1e-6 > 0, a direction the solve drops
 * (singular values 10, 3, 1 and 1e-10 of a 9 x 4 A = U S V^T, so s_4 is
 * below rcond 1e-8 x s_max; one truncation rule for every lambda) is left
 * out of the weights: they are the ridge solution of the three kept
 * directions, sum_{i<3} v_i s_i (u_i . T) / (s_i^2 + lambda) (1e-9
 * relative), with no component along v_4 (1e-12 of their norm); a WARNING
 * names 1 of the 4 directions.
 * Before the fix: passes as well for the weights and the WARNING (the case
 * pinned the covariance of the kept directions too). */
TEST_CASE("OALGO 3 with LAMBDA > 0: the weights leave out a direction the "
          "solve drops",
          "[oalgo3]") {
  const LMat U = orthonormal_columns(make_A(9, 4, 0.41));
  const LMat V = orthonormal_columns(make_A(4, 4, 0.53));
  const LD s[4] = {10.0L, 3.0L, 1.0L, 1e-10L};
  const LD lambda = 1e-6L;
  LMat A(9, std::vector<LD>(4, 0.0L));
  for (size_t i = 0; i < 9; ++i)
    for (size_t j = 0; j < 4; ++j)
      for (size_t k = 0; k < 4; ++k) A[i][j] += U[i][k] * s[k] * V[j][k];
  const std::vector<LD> T = make_T(A);
  // The solve's weights: the kept directions only.
  std::vector<LD> w_ref(4, 0.0L);
  for (size_t k = 0; k < 3; ++k) {
    LD utb = 0;
    for (size_t i = 0; i < 9; ++i) utb += U[i][k] * T[i];
    for (size_t j = 0; j < 4; ++j)
      w_ref[j] += V[j][k] * s[k] / (s[k] * s[k] + lambda) * utb;
  }

  Context c = oalgo3_context("1e-6");
  phi_type Phi = with_augmentation(A);
  t_type t = target_with_augmentation(T, 4);
  t_type w;
  std::string log;
  {
    LogCapture cap;
    LinearRegressor::train(c, Phi, t, w);
    log = cap.text();
  }
  INFO("log: " << log);
  REQUIRE(w.size() == 4);
  LD wn = 0, w4 = 0;
  for (size_t j = 0; j < 4; ++j) {
    wn += static_cast<LD>(w[j]) * w[j];
    w4 += static_cast<LD>(w[j]) * V[j][3];
  }
  CHECK(static_cast<double>(std::fabs(w4)) <
        1e-12 * static_cast<double>(std::sqrt(wn)));
  LD dmax = 0, rmax = 0;
  for (size_t j = 0; j < 4; ++j) {
    dmax = std::max(dmax, std::fabs(static_cast<LD>(w[j]) - w_ref[j]));
    rmax = std::max(rmax, std::fabs(w_ref[j]));
  }
  CHECK(static_cast<double>(dmax / rmax) < 1e-9);
  CHECK(log.find("1 of 4 singular directions") != std::string::npos);
}

/* Pins: LAMBDA -1 (the evidence approximation, OALGO 3) counts the
 * observations only: on the 9 real rows of a 9 x 4 A (4
 * ridge-augmentation rows appended) it converges to the lambda = alpha /
 * beta of the iteration alpha = gamma / |w|^2, beta = (N - gamma) / RSS
 * with N = 9 (gamma over the kept directions; long double, iterated to
 * 1e-15) to 1e-8 relative, and to its weights (1e-8).
 * Before the fix: N was max(rows, cols) = 13, which counted the
 * augmentation rows: beta was too large by about (13 - gamma)/(9 - gamma),
 * so lambda was 8.13e-3 against 1.469e-2, and the weights followed. */
TEST_CASE("LAMBDA -1: the evidence approximation counts the observations",
          "[oalgo3][ea]") {
  const LMat A = make_A(9, 4, 0.71);
  std::vector<LD> T = make_T(A);
  for (size_t i = 0; i < T.size(); ++i)
    T[i] += 0.2L * std::cos(2.3L * static_cast<LD>(i));
  // Reference EA: alpha, beta from 1, 1.
  LD alpha = 1.0L, beta = 1.0L;
  RidgeRef r;
  for (int it = 0; it < 10000; ++it) {
    r = ridge_reference(A, T, alpha / beta);
    LD ww = 0;
    for (LD x : r.w) ww += x * x;
    const LD a_new = r.gamma / ww;
    const LD b_new = (9.0L - r.gamma) / r.rss;
    const bool done = std::fabs(a_new - alpha) < 1e-15L * a_new &&
                      std::fabs(b_new - beta) < 1e-15L * b_new;
    alpha = a_new;
    beta = b_new;
    if (done) break;
  }
  const LD lambda_ref = alpha / beta;
  r = ridge_reference(A, T, lambda_ref);

  Context c = oalgo3_context("-1");
  c.add("ALPHA", "1");
  c.add("BETA", "1");
  phi_type Phi = with_augmentation(A);
  t_type t = target_with_augmentation(T, 4);
  t_type w;
  LinearRegressor::train(c, Phi, t, w);
  const double lambda = c.get<double>("ALPHA") / c.get<double>("BETA");
  INFO("lambda " << lambda << " against " << static_cast<double>(lambda_ref));
  CHECK(std::fabs(lambda - static_cast<double>(lambda_ref)) <
        1e-8 * static_cast<double>(lambda_ref));
  REQUIRE(w.size() == 4);
  for (size_t j = 0; j < 4; ++j)
    CHECK(std::fabs(w[j] - static_cast<double>(r.w[j])) <
          1e-8 * std::fabs(static_cast<double>(r.w[j])));
}

/* Pins: with FIXINDEX 2 / FIXWEIGHT 0.7 and OALGO 3, M_BLR_Train<BF_Linear>
 * and M_KRR_Train<Kern_Linear> pin weight 2 at 0.7, and the free weights
 * (1, 3, 4) are the ridge solution of the free columns A_f against T -
 * 0.7 a_2 over the 9 real rows, for LAMBDA 1e-3 and 0 (1e-10 relative); at
 * LAMBDA 0 no WARNING about dropped directions (the pinned column is
 * removed, not zeroed and kept).
 * Before the fix: passes as well for the weights (the case pinned the
 * covariance of every weight too). */
TEST_CASE("FIXINDEX with OALGO 3: the pinned weight and the fit of the free "
          "columns",
          "[oalgo3][fixed_weights]") {
  const LMat A = make_A(9, 4, 0.23);
  const std::vector<LD> T = make_T(A);
  const size_t free_cols[3] = {0, 2, 3};
  LMat Af(9, std::vector<LD>(3));
  std::vector<LD> Tr = T;
  for (size_t i = 0; i < 9; ++i) {
    for (size_t j = 0; j < 3; ++j) Af[i][j] = A[i][free_cols[j]];
    Tr[i] -= 0.7L * A[i][1];
  }
  for (const char *lambda : {"1e-3", "0"}) {
    const RidgeRef ref = ridge_reference(Af, Tr, std::stold(lambda));
    for (int krr = 0; krr < 2; ++krr) {
      DYNAMIC_SECTION((krr ? "M_KRR_Train<Kern_Linear>" : "M_BLR_Train<BF_Linear>")
                      << ", LAMBDA " << lambda) {
        Context c = oalgo3_context(lambda);
        c.add("FIXINDEX", "2");
        c.add("FIXWEIGHT", "0.7");
        phi_type Phi = with_augmentation(A);
        t_type t = target_with_augmentation(T, 4);
        t_type w;
        std::string log;
        {
          LogCapture cap;
          if (krr) {
            Kern_Linear k;
            M_KRR_Train<Kern_Linear> m(k, c);
            m.train(Phi, t);
            w = m.get_weights();
          } else {
            BF_Linear bf;
            M_BLR_Train<BF_Linear> m(bf, c);
            m.train(Phi, t);
            w = m.get_weights();
          }
          log = cap.text();
        }
        INFO("log: " << log);
        REQUIRE(w.size() == 4);
        CHECK(w[1] == 0.7);
        for (size_t a = 0; a < 3; ++a)
          CHECK(std::fabs(w[free_cols[a]] - static_cast<double>(ref.w[a])) <
                1e-10 * std::fabs(static_cast<double>(ref.w[a])));
        CHECK(log.find("singular directions") == std::string::npos);
      }
    }
  }
}

/* Pins: LAMBDA -1 (the evidence approximation, OALGO 3) refuses a fit
 * with no more observations than effective weights, before it estimates
 * the noise: a design matrix of 3 rows of weight 0 (zero rows) and its 2
 * augmentation rows, n_obs 0, throws std::runtime_error naming LAMBDA -1,
 * the 0 observations and the remedy (LAMBDA >= 0), with the Context's
 * ALPHA and BETA as they were (1 and 1); through
 * M_BLR_Train<BF_Linear>::train(Phi, T, 0) the model is not trained; and
 * through train(Phi, T) on the augmentation rows alone (rows - cols = 0
 * observations) the same.
 * Before the fix: passes as well (a pin of the refusal MODELS 9b2629f
 * added with n_obs; before that commit EA took N = max(rows, cols), which
 * counted these rows, and divided by RSS = 0: beta was infinite and there
 * was no refusal). */
TEST_CASE("LAMBDA -1 refuses a fit without observations",
          "[oalgo3][ea]") {
  auto zero_phi = [](size_t rows) {
    phi_type Phi(rows, 2);
    Phi.set_zero();
    return Phi;
  };
  auto zero_t = [](size_t rows) {
    t_type t(rows);
    for (size_t i = 0; i < rows; ++i) t[i] = 0.0;
    return t;
  };
  {
    Context c = oalgo3_context("-1");
    c.add("ALPHA", "1");
    c.add("BETA", "1");
    phi_type Phi = zero_phi(5);
    t_type t = zero_t(5);
    t_type w;
    tadah::models::memory::ModelsWorkspaceManager wsm;
    std::string msg = "<no exception>";
    try {
      LinearRegressor::train(c, Phi, t, w, wsm, {}, 0,
                             LinearRegressor::Layout::of(Phi));
    } catch (const std::runtime_error &e) {
      msg = e.what();
    }
    INFO(msg);
    CHECK(msg.find("LAMBDA -1") != std::string::npos);
    CHECK(msg.find("0 observations") != std::string::npos);
    CHECK(msg.find("LAMBDA >= 0") != std::string::npos);
    CHECK(c.get<double>("ALPHA") == 1.0);
    CHECK(c.get<double>("BETA") == 1.0);
  }
  for (const bool explicit_n_obs : {true, false}) {
    DYNAMIC_SECTION("M_BLR_Train<BF_Linear>, "
                    << (explicit_n_obs ? "n_obs 0" : "rows - cols = 0")) {
      Context c = oalgo3_context("-1");
      c.add("ALPHA", "1");
      c.add("BETA", "1");
      BF_Linear bf;
      M_BLR_Train<BF_Linear> m(bf, c);
      phi_type Phi = zero_phi(explicit_n_obs ? 5 : 2);
      t_type t = zero_t(explicit_n_obs ? 5 : 2);
      std::string msg = "<no exception>";
      try {
        if (explicit_n_obs)
          m.train(Phi, t, 0);
        else
          m.train(Phi, t);
      } catch (const std::runtime_error &e) {
        msg = e.what();
      }
      INFO(msg);
      CHECK(msg.find("LAMBDA -1") != std::string::npos);
      CHECK(msg.find("0 observations") != std::string::npos);
      CHECK_FALSE(m.trained);
    }
  }
}

/* Pins: OLS::solveWithPseudoinverse of a wide 3 x 5 matrix (m < n, so
 * V^T has min(m,n) = 3 rows) returns the minimum-norm solution A^T (A
 * A^T)^-1 b at lambda 0 and the ridge solution A^T (A A^T + lambda I)^-1 b
 * at lambda 0.1 (1e-10 relative).
 * Before the fix: it multiplied by V^T with a leading dimension of n = 5
 * instead of min(m,n) = 3, which reads V^T's 15 values with the wrong
 * stride and past their end, so the weights were not the solution (lambda
 * 0: 0.103, -0.235, 3e-38, -2e-38 for weights 2 to 5 against 0.337, 0.578,
 * 0.114, -0.055; a latent defect: the design matrix always has at least as
 * many rows as columns). */
TEST_CASE("solveWithPseudoinverse of a wide matrix: the minimum-norm "
          "solution",
          "[oalgo3][ols]") {
  const LMat A = make_A(3, 5, 0.83);
  std::vector<LD> b(3);
  for (size_t i = 0; i < 3; ++i) b[i] = 0.4L + 0.3L * static_cast<LD>(i);
  for (const LD lambda : {0.0L, 0.1L}) {
    DYNAMIC_SECTION("lambda " << static_cast<double>(lambda)) {
      LMat AAt(3, std::vector<LD>(3, 0.0L));
      for (size_t a = 0; a < 3; ++a)
        for (size_t c2 = 0; c2 < 3; ++c2)
          for (size_t k = 0; k < 5; ++k) AAt[a][c2] += A[a][k] * A[c2][k];
      for (size_t a = 0; a < 3; ++a) AAt[a][a] += lambda;
      const LMat inv = inverse(AAt);
      std::vector<LD> y(3, 0.0L), x(5, 0.0L);
      for (size_t a = 0; a < 3; ++a)
        for (size_t c2 = 0; c2 < 3; ++c2) y[a] += inv[a][c2] * b[c2];
      for (size_t k = 0; k < 5; ++k)
        for (size_t a = 0; a < 3; ++a) x[k] += A[a][k] * y[a];

      phi_type M(3, 5);
      for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 5; ++j) M(i, j) = static_cast<double>(A[i][j]);
      t_type B(3);
      for (size_t i = 0; i < 3; ++i) B[i] = static_cast<double>(b[i]);
      SVD svd;
      svd.compute(M);
      t_type w(5);
      OLS ols;
      ols.solveWithPseudoinverse(3, 5, B, w, svd.getSVDWorkspace(), 1e-8,
                                 static_cast<double>(lambda));
      for (size_t k = 0; k < 5; ++k) {
        INFO("k " << k << ": " << w[k] << " against "
                  << static_cast<double>(x[k]));
        CHECK(std::fabs(w[k] - static_cast<double>(x[k])) <
              1e-10 * std::fabs(static_cast<double>(x[k])));
      }
    }
  }
}
