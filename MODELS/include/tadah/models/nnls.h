#ifndef TADAH_MODELS_NNLS_H
#define TADAH_MODELS_NNLS_H

#include <cstddef>
#include <vector>

namespace tadah {
namespace models {

/** \brief Why nnls() stopped. */
enum class NnlsStop {
  /** Every zero-set dual is at most the dual tolerance: a Kuhn-Tucker
   *  point to the accuracy of those duals. */
  KuhnTucker,
  /** The iteration cap (3 n + 10) was reached. */
  IterationCap,
  /** Columns with a dual above the tolerance remained, but each was refused
   *  (numerically dependent on the passive columns, or a coefficient <= 0
   *  on entry), so no column could enter. */
  Refused
};

/** "a Kuhn-Tucker point", "the iteration cap", "every column with a dual
 *  above the tolerance was refused": the reason, for messages. */
const char *nnls_stop_text(NnlsStop stop);

/** \brief Result of nnls(). */
struct NnlsResult {
  /** The passive set at termination: the columns with x_j > 0, ascending.
   *  Every other x_j is exactly 0. A variable-projection Jacobian needs it
   *  (the solution is the unconstrained least-squares solution on these
   *  columns). On an ill-conditioned problem it is determined only above
   *  the forward error of x, about cond(A) eps |x|: a column whose exact
   *  coefficient is 0 can hold a positive coefficient of that size. */
  std::vector<std::size_t> passive;
  /** Outer iterations (columns moved into the passive set). */
  std::size_t iterations = 0;
  /** True: every zero-set dual, computed from the final passive
   *  factorisation, is at most the dual tolerance, i.e. x is a Kuhn-Tucker
   *  point to the accuracy of those duals (stop == KuhnTucker). False when
   *  the iteration cap (3 n + 10) stopped the solver, or when every column
   *  whose dual was above the tolerance was refused: x is then feasible
   *  (every x_j >= 0) but not proven optimal. */
  bool converged = true;
  /** Why the solver stopped. */
  NnlsStop stop = NnlsStop::KuhnTucker;
  /** At a Refused stop, the number of columns refused in the last attempt
   *  whose dual was above the tolerance; 0 otherwise. */
  std::size_t refused = 0;
};

/**
 * @brief Non-negative least squares (Lawson and Hanson 1974, algorithm NNLS).
 *
 * Solves
 * \f[
 *   \min_{x \ge 0}\; \sum_i w_i \big((A x)_i - b_i\big)^2
 *                   + \lambda \sum_{j\ \mathrm{free}} x_j^2
 * \f]
 * over the free columns (@p col_mask nonzero, or every column when null),
 * with x_j = 0 for masked-out columns and for columns that are identically
 * zero after the row weighting. The regularisation is applied as appended
 * \f$ \sqrt{\lambda} I \f$ rows (the LinearRegressor LAMBDA convention).
 *
 * The passive-set subproblems are solved by Householder QR of the passive
 * columns, never by the normal equations: the step-function columns of a
 * RHOFORM MONOTONE density have condition numbers of 5e7 to 4e9, whose
 * square exhausts double precision. A column whose QR diagonal shows it
 * numerically dependent on the passive columns (|R_pp| <= 100 eps |a_p|),
 * or whose coefficient comes out <= 0 on entry, is not admitted in that
 * iteration (Lawson and Hanson's safeguard against cycling). When every
 * column with a dual above the tolerance is refused, the solver stops
 * without convergence (NnlsStop::Refused): x is feasible, not a proven
 * Kuhn-Tucker point. As in Lawson
 * and Hanson, the dual w = A^T r is formed from the residual of the passive
 * factorisation, r = Q [0; (Q^T b)[p, m)], never from b - A x: the forward
 * error of x (cond(A_P) eps |x| and more) flips the sign of the small duals
 * of columns nearly in the span of the passive ones, which on an exactly
 * representable step-column fit stops the solver at |r|/|b| ~ 1e-9 instead
 * of ~1e-16. Dual feasibility is tested against 10 eps |A|_F |r|_2 (scaled
 * by the norms of the weighted system and of the current residual).
 *
 * The factorisation follows the passive set instead of being redone for
 * every subproblem: an entering column gets the stored reflectors applied
 * (O(m p) for p passive columns, against O(m p^2) for a fresh QR), and
 * dropping columns refactors from the first dropped one on. The factors are
 * those of a fresh QR of the passive columns in insertion order. A solve
 * that never drops a column costs O(m n^2) for n columns; each drop costs
 * up to O(m p^2) more. The step columns of MONOTONE drop often (measured:
 * 66 drops re-appending 21k columns for n = 1024, m = 8192, 60 s).
 *
 * The solution is deterministic: no BLAS, no threads, a fixed operation
 * order.
 *
 * @param A           m x n matrix, column-major (leading dimension m)
 * @param m           rows
 * @param n           columns
 * @param b           right-hand side, length m
 * @param x           solution, length n (output; every entry >= 0)
 * @param row_weights w_i >= 0 per row (the squared residual of row i is
 *                    multiplied by w_i, i.e. the row by sqrt(w_i)); null =
 *                    all 1
 * @param col_mask    per column, nonzero = free, 0 = fixed at 0; null = all
 *                    free
 * @param lambda      Tikhonov weight >= 0 on the free columns
 * @return the passive set and the iteration record
 * @throws std::invalid_argument for a negative or non-finite weight or
 *         lambda, or a non-finite entry of A or b
 */
NnlsResult nnls(const double *A, std::size_t m, std::size_t n,
                const double *b, double *x,
                const double *row_weights = nullptr,
                const unsigned char *col_mask = nullptr,
                double lambda = 0.0);

}  // namespace models
}  // namespace tadah

#endif
