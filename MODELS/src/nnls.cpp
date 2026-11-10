#include <tadah/models/nnls.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace tadah {
namespace models {

namespace {

// Householder QR of the passive columns of the mr x nf column-major M, kept
// across the iterations of nnls() (no pivoting: the column order is the
// passive-set insertion order, so the newest column is last). Column k of
// `q` holds R above the diagonal (rows < k) and the reflector v_k from the
// diagonal down (H_k = I - beta_k v_k v_k^T); `qty` is H_{p-1} ... H_0 y.
//
// append() applies the stored reflectors to the new column in order, which
// are the operations a factorisation of the whole passive set from scratch
// performs on that column: the factors depend on the column order only, not
// on how the passive set was reached. Appending costs O(mr p) where a fresh
// factorisation costs O(mr p^2); dropping columns (reset()) keeps the
// reflectors of the unchanged prefix and re-appends the rest.
class PassiveQR {
 public:
  PassiveQR(const std::vector<double> &M, std::size_t mr,
            const std::vector<double> &cn, const std::vector<double> &y)
      : M_(M), mr_(mr), cn_(cn), y_(y), qty_(y) {}

  std::size_t size() const { return cols_.size(); }

  // Appends column c. Returns false, leaving the factorisation unchanged,
  // when c is numerically dependent on the passive columns
  // (|R_pp| <= 100 eps |a_c|, cn holding the column norms of M) or when
  // there would be more columns than rows.
  bool append(std::size_t c) {
    const std::size_t k = cols_.size();
    if (k + 1 > mr_) return false;
    const double eps = std::numeric_limits<double>::epsilon();
    q_.resize(mr_ * (k + 1));
    double *ak = &q_[k * mr_];
    for (std::size_t i = 0; i < mr_; ++i) ak[i] = M_[c * mr_ + i];
    for (std::size_t j = 0; j < k; ++j) {
      const double *vj = &q_[j * mr_];
      double d = 0.0;
      for (std::size_t i = j; i < mr_; ++i) d += vj[i] * ak[i];
      d *= beta_[j];
      for (std::size_t i = j; i < mr_; ++i) ak[i] -= d * vj[i];
    }
    double scale = 0.0;
    for (std::size_t i = k; i < mr_; ++i) scale = std::fmax(scale, std::fabs(ak[i]));
    double nrm = 0.0;
    if (scale > 0.0) {
      double s2 = 0.0;
      for (std::size_t i = k; i < mr_; ++i) {
        const double t = ak[i] / scale;
        s2 += t * t;
      }
      nrm = scale * std::sqrt(s2);
    }
    if (!(nrm > 100.0 * eps * cn_[c])) {
      q_.resize(mr_ * k);
      return false;
    }
    // H = I - beta v v^T with v = a - alpha e_k, alpha = -sign(a_kk) |a|:
    // v_k = a_kk + sign(a_kk) |a| never cancels, v^T v = 2 |a| (|a| + |a_kk|)
    const double akk = ak[k];
    const double alpha = akk > 0.0 ? -nrm : nrm;
    beta_.push_back(1.0 / (nrm * (nrm + std::fabs(akk))));
    ak[k] = akk - alpha;
    rdiag_.push_back(alpha);
    cols_.push_back(c);
    qty_saved_ = qty_;
    double d = 0.0;
    for (std::size_t i = k; i < mr_; ++i) d += ak[i] * qty_[i];
    d *= beta_[k];
    for (std::size_t i = k; i < mr_; ++i) qty_[i] -= d * ak[i];
    return true;
  }

  // Undoes the append() that immediately preceded it.
  void pop() {
    cols_.pop_back();
    beta_.pop_back();
    rdiag_.pop_back();
    q_.resize(mr_ * cols_.size());
    qty_.swap(qty_saved_);
  }

  // Refactors for P, a subsequence of the passive columns in their order:
  // the reflectors of the common prefix stay, the rest are re-appended.
  // False when a re-appended column fails the dependency test (not
  // expected: a column's orthogonal part only grows when earlier columns
  // are dropped).
  bool reset(const std::vector<std::size_t> &P) {
    std::size_t k0 = 0;
    while (k0 < P.size() && k0 < cols_.size() && P[k0] == cols_[k0]) ++k0;
    cols_.resize(k0);
    beta_.resize(k0);
    rdiag_.resize(k0);
    q_.resize(mr_ * k0);
    qty_ = y_;
    for (std::size_t k = 0; k < k0; ++k) {
      const double *vk = &q_[k * mr_];
      double d = 0.0;
      for (std::size_t i = k; i < mr_; ++i) d += vk[i] * qty_[i];
      d *= beta_[k];
      for (std::size_t i = k; i < mr_; ++i) qty_[i] -= d * vk[i];
    }
    for (std::size_t k = k0; k < P.size(); ++k)
      if (!append(P[k])) return false;
    return true;
  }

  // Least-squares solution z of min |M_P z - y|_2 (back substitution,
  // R z = (Q^T y)[0, p)).
  void solve(std::vector<double> &z) const {
    const std::size_t p = cols_.size();
    z.assign(p, 0.0);
    for (std::size_t kk = p; kk-- > 0;) {
      double sum = qty_[kk];
      for (std::size_t j = kk + 1; j < p; ++j) sum -= q_[j * mr_ + kk] * z[j];
      z[kk] = sum / rdiag_[kk];
    }
  }

  // The residual formed from the factorisation, r = H_0 ... H_{p-1}
  // [0; (Q^T y)[p, mr)], NOT y - M_P z. This is Lawson and Hanson's dual: r
  // is orthogonal to the passive columns to within eps |r|, whatever the
  // forward error of z (cond(M_P) eps |z| + cond(M_P)^2 eps |r|, 1e-9 and
  // more for the step-function columns). A dual M^T r formed from y - M_P z
  // inherits that error, which flips the sign of the small duals of columns
  // nearly in span(M_P) and stops nnls() early (|r|/|b| 1e-9 where 1e-16 is
  // reachable).
  void residual(std::vector<double> &r) const {
    const std::size_t p = cols_.size();
    r.assign(mr_, 0.0);
    for (std::size_t i = p; i < mr_; ++i) r[i] = qty_[i];
    for (std::size_t k = p; k-- > 0;) {
      const double *vk = &q_[k * mr_];
      double d = 0.0;
      for (std::size_t i = k; i < mr_; ++i) d += vk[i] * r[i];
      d *= beta_[k];
      for (std::size_t i = k; i < mr_; ++i) r[i] -= d * vk[i];
    }
  }

 private:
  const std::vector<double> &M_;
  std::size_t mr_;
  const std::vector<double> &cn_;
  const std::vector<double> &y_;
  std::vector<std::size_t> cols_;
  std::vector<double> q_, beta_, rdiag_, qty_, qty_saved_;
};

}  // namespace

const char *nnls_stop_text(NnlsStop stop) {
  switch (stop) {
    case NnlsStop::KuhnTucker:   return "a Kuhn-Tucker point";
    case NnlsStop::IterationCap: return "the iteration cap";
    case NnlsStop::Refused:
      return "every column with a dual above the tolerance was refused";
  }
  return "a Kuhn-Tucker point";  // unreachable (enum is exhaustive above)
}

NnlsResult nnls(const double *A, std::size_t m, std::size_t n,
                const double *b, double *x, const double *row_weights,
                const unsigned char *col_mask, double lambda) {
  NnlsResult res;
  if (!std::isfinite(lambda) || lambda < 0.0)
    throw std::invalid_argument("nnls: lambda must be finite and >= 0.");
  for (std::size_t i = 0; i < m; ++i) {
    if (!std::isfinite(b[i]))
      throw std::invalid_argument("nnls: non-finite right-hand side (row " +
                                  std::to_string(i) + ").");
    if (row_weights &&
        (!std::isfinite(row_weights[i]) || row_weights[i] < 0.0))
      throw std::invalid_argument("nnls: row weights must be finite and >= 0 "
                                  "(row " + std::to_string(i) + ").");
  }
  for (std::size_t k = 0; k < m * n; ++k)
    if (!std::isfinite(A[k]))
      throw std::invalid_argument("nnls: non-finite matrix entry.");
  for (std::size_t j = 0; j < n; ++j) x[j] = 0.0;

  // The weighted system: rows scaled by sqrt(w_i), sqrt(lambda) I rows
  // appended for the free columns. A masked-out column, or one that is zero
  // after the weighting (e.g. a basis function whose support holds no
  // sample), stays at 0.
  std::vector<double> sw(m, 1.0);
  if (row_weights)
    for (std::size_t i = 0; i < m; ++i) sw[i] = std::sqrt(row_weights[i]);
  std::vector<std::size_t> free_cols;
  for (std::size_t j = 0; j < n; ++j) {
    if (col_mask && !col_mask[j]) continue;
    bool nonzero = false;
    for (std::size_t i = 0; i < m && !nonzero; ++i)
      nonzero = sw[i] * A[j * m + i] != 0.0;
    if (nonzero) free_cols.push_back(j);
  }
  const std::size_t nf = free_cols.size();
  if (nf == 0) return res;
  const std::size_t mr = m + (lambda > 0.0 ? nf : 0);
  std::vector<double> M(mr * nf, 0.0), y(mr, 0.0);
  for (std::size_t q = 0; q < nf; ++q) {
    for (std::size_t i = 0; i < m; ++i)
      M[q * mr + i] = sw[i] * A[free_cols[q] * m + i];
    if (lambda > 0.0) M[q * mr + m + q] = std::sqrt(lambda);
  }
  for (std::size_t i = 0; i < m; ++i) y[i] = sw[i] * b[i];

  std::vector<double> cn(nf);
  double normM2 = 0.0;
  for (std::size_t q = 0; q < nf; ++q) {
    double s = 0.0;
    for (std::size_t i = 0; i < mr; ++i) s += M[q * mr + i] * M[q * mr + i];
    cn[q] = std::sqrt(s);
    normM2 += s;
  }
  const double normM = std::sqrt(normM2);
  const double eps = std::numeric_limits<double>::epsilon();
  const std::size_t max_iter = 3 * nf + 10;

  // r: the residual of the current passive solution, from its factorisation
  // (PassiveQR::residual); y while the passive set is empty
  std::vector<double> z(nf, 0.0), r, w(nf), zz;
  std::vector<char> in_p(nf, 0);
  std::vector<std::size_t> P;  // passive set, in insertion order
  PassiveQR f(M, mr, cn, y);   // its factorisation, same column order
  bool done = false;
  while (!done) {
    // the dual w = M^T r on the zero set
    f.residual(r);
    double rr = 0.0;
    for (std::size_t i = 0; i < mr; ++i) rr += r[i] * r[i];
    const double tol = 10.0 * eps * normM * std::sqrt(rr);
    for (std::size_t q = 0; q < nf; ++q) {
      w[q] = 0.0;
      if (in_p[q]) continue;
      for (std::size_t i = 0; i < mr; ++i) w[q] += M[q * mr + i] * r[i];
    }

    // Admit the zero-set column of largest dual above the tolerance. A
    // column that is numerically dependent on the passive set, or whose
    // coefficient comes out <= 0, is skipped in this iteration.
    std::vector<char> rejected(nf, 0);
    std::size_t n_rejected = 0;
    bool entered = false;
    while (!entered) {
      std::size_t t = nf;
      double wt = tol;
      for (std::size_t q = 0; q < nf; ++q)
        if (!in_p[q] && !rejected[q] && w[q] > wt) { wt = w[q]; t = q; }
      if (t == nf) {
        // No column left to try: a Kuhn-Tucker point only when none was
        // refused. A refused column's dual is above the tolerance, so x is
        // then feasible but not proven optimal.
        if (n_rejected > 0) {
          res.converged = false;
          res.stop = NnlsStop::Refused;
          res.refused = n_rejected;
        }
        done = true;
        break;
      }
      if (res.iterations >= max_iter) {
        res.converged = false;
        res.stop = NnlsStop::IterationCap;
        done = true;
        break;
      }
      if (!f.append(t)) {
        rejected[t] = 1;
        ++n_rejected;
        continue;
      }
      f.solve(zz);
      if (!(zz.back() > 0.0)) {
        f.pop();
        rejected[t] = 1;
        ++n_rejected;
        continue;
      }
      P.push_back(t);
      in_p[t] = 1;
      ++res.iterations;
      entered = true;
    }
    if (done) break;

    // Inner loop: step from z towards the passive-set solution zz, stopping
    // where the first coefficient reaches 0; drop the zeroed columns and
    // re-solve until every passive coefficient is positive.
    for (;;) {
      bool all_positive = true;
      for (std::size_t k = 0; k < P.size(); ++k)
        if (!(zz[k] > 0.0)) all_positive = false;
      if (all_positive) {
        for (std::size_t k = 0; k < P.size(); ++k) z[P[k]] = zz[k];
        break;
      }
      double alpha = std::numeric_limits<double>::infinity();
      std::size_t kmin = 0;
      for (std::size_t k = 0; k < P.size(); ++k)
        if (!(zz[k] > 0.0)) {
          const double zk = z[P[k]];
          const double a = zk / (zk - zz[k]);
          if (a < alpha) { alpha = a; kmin = k; }
        }
      for (std::size_t k = 0; k < P.size(); ++k)
        z[P[k]] += alpha * (zz[k] - z[P[k]]);
      z[P[kmin]] = 0.0;
      std::vector<std::size_t> keep;
      for (std::size_t q : P) {
        if (z[q] > 0.0) {
          keep.push_back(q);
        } else {
          z[q] = 0.0;
          in_p[q] = 0;
        }
      }
      P.swap(keep);
      if (!f.reset(P)) {        // cannot happen for a subset of admitted
        res.converged = false;  // columns; stay feasible
        done = true;
        break;
      }
      if (P.empty()) break;
      f.solve(zz);
    }
  }

  for (std::size_t q = 0; q < nf; ++q) {
    x[free_cols[q]] = z[q];
    if (z[q] > 0.0) res.passive.push_back(free_cols[q]);
  }
  return res;
}

}  // namespace models
}  // namespace tadah
