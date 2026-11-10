// test_intercept_ridge.cpp
//
// The ridge (LAMBDA) of M_BLR_Train and M_KRR_Train leaves the intercept
// columns of the model's function (Function_Base::intercept_columns) out of
// the penalty, and only them, also when FIXINDEX/FIXWEIGHT pin weights: the
// pinned columns leave the regression, and the free columns keep their own
// penalty, whatever their position. With BIAS true and two species (ATOM Nb
// Ta) the intercept columns of BF_Linear and Kern_Linear are 0 and 1, and
// those of BF_Polynomial2 the products of an intercept component with
// itself, (0,0) and (1,1).
//
// The reference is the closed-form solution of the penalised least-squares
// problem in long double: minimise |t - A w|^2 + LAMBDA sum_(j penalised)
// w_j^2 with the pinned weights at their values, from the normal equations
// of the free columns. The design matrices follow LinearRegressor's
// convention: the data rows, then one zero row per column for the ridge.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/models/functions/basis_functions/bf_linear.h>
#include <tadah/models/functions/basis_functions/bf_polynomial2.h>
#include <tadah/models/functions/kernels/kern_linear.h>
#include <tadah/models/m_blr_train.h>
#include <tadah/models/m_krr_train.h>

#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tadah::core;
using namespace tadah::models;

namespace {

using ld = long double;

/* n data rows of p columns. Columns 0 and 1 are the Nb and Ta counts of 12
 * structures of varied composition (the intercept columns of a linear
 * model on ATOM Nb Ta), the others smooth deterministic "descriptor sums";
 * the targets are -7.6 n_Nb - 8.9 n_Ta + a linear model of the rest plus a
 * small deterministic noise. For BF_Polynomial2 the same numbers stand for
 * its 6 product columns (the regression does not know what they are). */
struct Problem {
  std::size_t n = 12, p = 0;
  std::vector<std::vector<ld>> A;  // n x p
  std::vector<ld> t;               // n
};

Problem make_problem(std::size_t p) {
  Problem P;
  P.p = p;
  P.A.assign(P.n, std::vector<ld>(p, 0));
  P.t.assign(P.n, 0);
  for (std::size_t i = 0; i < P.n; ++i) {
    P.A[i][0] = 2 + static_cast<ld>(i % 5);
    P.A[i][1] = 3 + static_cast<ld>((7 * i) % 4);
    for (std::size_t j = 2; j < p; ++j)
      P.A[i][j] = std::sin(0.7L * (i + 1) + 1.3L * j) * (1 + 0.1L * j) +
                  0.05L * (i % 3);
    ld e = -7.6L * P.A[i][0] - 8.9L * P.A[i][1];
    for (std::size_t j = 2; j < p; ++j)
      e += (0.4L - 0.15L * j) * P.A[i][j];
    P.t[i] = e + 0.01L * std::sin(1.7L * i + 0.3L);
  }
  return P;
}

phi_type to_phi(const Problem &P) {
  phi_type Phi(P.n + P.p, P.p);
  for (std::size_t i = 0; i < P.n + P.p; ++i)
    for (std::size_t j = 0; j < P.p; ++j)
      Phi(i, j) = i < P.n ? static_cast<double>(P.A[i][j]) : 0.0;
  return Phi;
}

t_type to_t(const Problem &P) {
  t_type T(P.n + P.p);
  for (std::size_t i = 0; i < P.n + P.p; ++i)
    T[i] = i < P.n ? static_cast<double>(P.t[i]) : 0.0;
  return T;
}

/* The closed-form penalised solution: columns in `unpenalised` carry no
 * penalty, `pinned` maps a column to its fixed value. */
std::vector<ld> reference(const Problem &P, ld lambda,
                          const std::vector<std::size_t> &unpenalised,
                          const std::map<std::size_t, ld> &pinned) {
  std::vector<std::size_t> free_cols;
  for (std::size_t j = 0; j < P.p; ++j)
    if (!pinned.count(j)) free_cols.push_back(j);
  const std::size_t m = free_cols.size();
  std::vector<ld> b = P.t;
  for (const auto &kv : pinned)
    for (std::size_t i = 0; i < P.n; ++i) b[i] -= P.A[i][kv.first] * kv.second;
  std::vector<std::vector<ld>> M(m, std::vector<ld>(m + 1, 0));
  for (std::size_t a = 0; a < m; ++a) {
    for (std::size_t c = 0; c < m; ++c)
      for (std::size_t i = 0; i < P.n; ++i)
        M[a][c] += P.A[i][free_cols[a]] * P.A[i][free_cols[c]];
    bool pen = true;
    for (std::size_t u : unpenalised) pen = pen && u != free_cols[a];
    if (pen) M[a][a] += lambda;
    for (std::size_t i = 0; i < P.n; ++i) M[a][m] += P.A[i][free_cols[a]] * b[i];
  }
  for (std::size_t c = 0; c < m; ++c) {  // Gauss-Jordan, partial pivoting
    std::size_t piv = c;
    for (std::size_t r = c + 1; r < m; ++r)
      if (std::fabs(M[r][c]) > std::fabs(M[piv][c])) piv = r;
    std::swap(M[c], M[piv]);
    for (std::size_t r = 0; r < m; ++r) {
      if (r == c) continue;
      const ld f = M[r][c] / M[c][c];
      for (std::size_t k = c; k <= m; ++k) M[r][k] -= f * M[c][k];
    }
  }
  std::vector<ld> w(P.p, 0);
  for (const auto &kv : pinned) w[kv.first] = kv.second;
  for (std::size_t a = 0; a < m; ++a) w[free_cols[a]] = M[a][m] / M[a][a];
  return w;
}

ld rel_diff(const t_type &w, const std::vector<ld> &ref) {
  ld d = 0, s = 0;
  for (std::size_t j = 0; j < ref.size(); ++j) {
    d = std::max(d, std::fabs(static_cast<ld>(w[j]) - ref[j]));
    s = std::max(s, std::fabs(ref[j]));
  }
  return d / s;
}

ld rel_diff(const std::vector<ld> &a, const std::vector<ld> &ref) {
  ld d = 0, s = 0;
  for (std::size_t j = 0; j < ref.size(); ++j) {
    d = std::max(d, std::fabs(a[j] - ref[j]));
    s = std::max(s, std::fabs(ref[j]));
  }
  return d / s;
}

std::string to_text(const t_type &w) {
  std::string s;
  for (std::size_t j = 0; j < w.size(); ++j) s += " " + std::to_string(w[j]);
  return s;
}

/* ATOM Nb Ta, BIAS true, LAMBDA, OALGO; `fix` the FIXINDEX entry (1-based,
 * "" for none) with FIXWEIGHT `w`; `dsize` the DSIZE key (0: none). */
Context make_ctx(double lambda, int oalgo, const std::string &fix, double w,
                 std::size_t dsize = 0) {
  Context c;
  c.add("VERBOSE", 0);
  c.add("ATOM", "Nb");
  c.add("ATOM", "Ta");
  c.add("BIAS", "true");
  c.add("LAMBDA", lambda);
  c.add("OALGO", oalgo);
  if (!fix.empty()) {
    c.add("FIXINDEX", fix);
    c.add("FIXWEIGHT", w);
  }
  if (dsize) c.addInternal("DSIZE", dsize);
  return c;
}

template <typename Model, typename F>
t_type fit(F &f, Context &c, const Problem &P) {
  Model m(f, c);
  phi_type Phi = to_phi(P);
  t_type T = to_t(P);
  m.train(Phi, T);
  return m.get_weights();
}

}  // namespace

/* Pins: with ATOM Nb Ta and BIAS true (intercept columns 0 and 1), LAMBDA
 * 1e-2 and 1e2, OALGO 1, 2 and 4, M_BLR_Train<BF_Linear> and
 * M_KRR_Train<Kern_Linear> on a 12 x 5 design matrix give the closed-form
 * penalised solution (1e-8 relative):
 *   - FIXINDEX 1 / FIXWEIGHT -7.6 (the Nb intercept pinned): weight 1 is
 *     -7.6 exactly, the Ta intercept (column 1) is free and unpenalised,
 *     the descriptor weights (columns 2-4) are penalised; that reference
 *     differs from the one penalising the Ta intercept and leaving
 *     descriptor column 2 free by more than 1e-4, so the check tells them
 *     apart;
 *   - FIXINDEX 4 / FIXWEIGHT 0.3 (a descriptor weight pinned): both
 *     intercepts unpenalised (a pin: passes before the fix as well);
 *   - no FIXINDEX: both intercepts unpenalised (a pin).
 * Before the fix: with the Nb intercept pinned M_BLR_Train removed column 0
 * and left the first two remaining columns unpenalised, the Ta intercept
 * and descriptor column 2, and penalised the Ta intercept instead (at
 * LAMBDA 1e2 its weights are off by far more than 1e-8); M_KRR_Train zeroed
 * column 0 and kept it, unpenalised, in the regression, so OALGO 2 (QR)
 * threw "Error in DGELS: info = 1" and OALGO 4 (Cholesky) "DPOTRF: Matrix
 * is not positive definite"; with OALGO 1 its minimum-norm solution gave
 * the zero column a weight of 0 and matched. */
TEST_CASE("M_BLR_Train and M_KRR_Train with FIXINDEX pinning an intercept "
          "leave the free intercepts, and only them, out of the ridge",
          "[intercept_ridge][fixed_weights]") {
  PeriodicTable::initialize();
  const Problem P = make_problem(5);
  for (double lambda : {1e-2, 1e2})
    for (int oalgo : {1, 2, 4}) {
      DYNAMIC_SECTION("LAMBDA " << lambda << ", OALGO " << oalgo) {
        {
          INFO("FIXINDEX 1 (the Nb intercept)");
          const auto ref = reference(P, lambda, {1}, {{0, -7.6L}});
          const auto wrong = reference(P, lambda, {2}, {{0, -7.6L}});
          CHECK(rel_diff(wrong, ref) > 1e-4);
          for (int krr = 0; krr < 2; ++krr) {
            INFO((krr ? "M_KRR_Train<Kern_Linear>" : "M_BLR_Train<BF_Linear>"));
            Context c = make_ctx(lambda, oalgo, "1", -7.6);
            t_type w;
            std::string error;
            try {
              if (krr) {
                Kern_Linear k;
                w = fit<M_KRR_Train<Kern_Linear>>(k, c, P);
              } else {
                BF_Linear bf;
                w = fit<M_BLR_Train<BF_Linear>>(bf, c, P);
              }
            } catch (const std::exception &e) {
              error = e.what();
            }
            INFO("error: " << error);
            REQUIRE(error.empty());
            INFO("weights" << to_text(w));
            REQUIRE(w.size() == 5);
            CHECK(w[0] == -7.6);
            CHECK(rel_diff(w, ref) < 1e-8);
          }
        }
        {
          INFO("FIXINDEX 4 (a descriptor weight, a pin)");
          const auto ref = reference(P, lambda, {0, 1}, {{3, 0.3L}});
          Context c = make_ctx(lambda, oalgo, "4", 0.3);
          BF_Linear bf;
          const t_type w = fit<M_BLR_Train<BF_Linear>>(bf, c, P);
          INFO("weights" << to_text(w));
          CHECK(w[3] == 0.3);
          CHECK(rel_diff(w, ref) < 1e-8);
          Context ck = make_ctx(lambda, oalgo, "4", 0.3);
          Kern_Linear k;
          const t_type wk = fit<M_KRR_Train<Kern_Linear>>(k, ck, P);
          INFO("M_KRR weights" << to_text(wk));
          CHECK(wk[3] == 0.3);
          CHECK(rel_diff(wk, ref) < 1e-8);
        }
        {
          INFO("no FIXINDEX (a pin)");
          const auto ref = reference(P, lambda, {0, 1}, {});
          Context c = make_ctx(lambda, oalgo, "", 0);
          BF_Linear bf;
          const t_type w = fit<M_BLR_Train<BF_Linear>>(bf, c, P);
          INFO("weights" << to_text(w));
          CHECK(rel_diff(w, ref) < 1e-8);
        }
      }
    }
}

/* Pins: M_BLR_Train<BF_Polynomial2> with ATOM Nb Ta, BIAS true and DSIZE 3
 * (6 weights, the products (i,ii), i <= ii: (0,0) (0,1) (0,2) (1,1) (1,2)
 * (2,2)) leaves both species' constants, the products (0,0) and (1,1)
 * (columns 0 and 3), out of the ridge: at LAMBDA 1e-2 and 1e2 with OALGO 1,
 * 2 and 4 it gives the closed-form solution penalising columns 1, 2, 4 and
 * 5 (1e-8 relative), which differs from the one penalising column 3 by
 * more than 1e-4; with FIXINDEX 4 / FIXWEIGHT -8.9 (the Ta constant
 * pinned) column 0 alone is unpenalised; OALGO 3 with LAMBDA 0.01 refuses
 * it, naming its 2 intercept columns.
 * Before the fix: BF_Polynomial2's intercept columns were "1 with a bias
 * intercept" (column 0 only), so the Ta constant (column 3) was penalised:
 * the weights differed from the reference by more than 1e-8 (at LAMBDA
 * 1e2 by far more), and OALGO 3 named "1 intercept column(s)". */
TEST_CASE("BF_Polynomial2 with BIAS true on two species leaves both species' "
          "constants out of the ridge",
          "[intercept_ridge][bf_polynomial2]") {
  PeriodicTable::initialize();
  const Problem P = make_problem(6);
  for (double lambda : {1e-2, 1e2})
    for (int oalgo : {1, 2, 4}) {
      DYNAMIC_SECTION("LAMBDA " << lambda << ", OALGO " << oalgo) {
        const auto ref = reference(P, lambda, {0, 3}, {});
        const auto wrong = reference(P, lambda, {0}, {});
        CHECK(rel_diff(wrong, ref) > 1e-4);
        Context c = make_ctx(lambda, oalgo, "", 0, 3);
        BF_Polynomial2 bf;
        const t_type w = fit<M_BLR_Train<BF_Polynomial2>>(bf, c, P);
        INFO("weights" << to_text(w));
        CHECK(rel_diff(w, ref) < 1e-8);

        const auto ref_pin = reference(P, lambda, {0}, {{3, -8.9L}});
        Context cp = make_ctx(lambda, oalgo, "4", -8.9, 3);
        BF_Polynomial2 bfp;
        const t_type wp = fit<M_BLR_Train<BF_Polynomial2>>(bfp, cp, P);
        INFO("weights, the Ta constant pinned" << to_text(wp));
        CHECK(wp[3] == -8.9);
        CHECK(rel_diff(wp, ref_pin) < 1e-8);
      }
    }
  SECTION("OALGO 3 with LAMBDA != 0 names both intercept columns") {
    Context c = make_ctx(0.01, 3, "", 0, 3);
    BF_Polynomial2 bf;
    std::string msg;
    try {
      (void)fit<M_BLR_Train<BF_Polynomial2>>(bf, c, P);
    } catch (const std::exception &e) {
      msg = e.what();
    }
    INFO("message: " << msg);
    CHECK(msg.find("2 intercept column") != std::string::npos);
  }
}
