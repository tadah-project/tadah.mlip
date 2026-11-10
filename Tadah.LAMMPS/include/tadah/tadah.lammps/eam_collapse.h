#pragma once
#ifndef TADAH_LAMMPS_EAM_COLLAPSE_H
#define TADAH_LAMMPS_EAM_COLLAPSE_H

/* Table collapse of a linear Tadah model.
 *
 * For a linear model the total energy is E_i = w . aed_i, so the 2b block
 * collapses to a scalar pair function phi(r) = sum_k w_k B_k(r) and an
 * EAM-form MB block to F(rho_bar) with rho_bar_i = sum_j rho(r_ij). The MD
 * hot path then needs only phi, phi', rho, rho' per pair (tabulated here as
 * cubic Hermite splines with analytic node derivatives, so energy and force
 * come from the same interpolant) plus the exact analytic F, F' per atom
 * (DM_Base::eam_embed, including the DM_REAM EMBF0 reference: under the
 * default EMBF0 ZERO, F(0) = 0 exactly).
 *
 * Two collapses are supported, selected by the model's descriptor set:
 *  - build_eam_collapse:    radial D2 + EAM-form DM (e.g. the `tadah refit`
 *    family D2_Knot5 + DM_REAM) -> phi + rho tables, analytic F.
 *  - build_pair2b_collapse: pure two-body (INITMB false) -> phi table only.
 *    This is the degenerate F==0, rho==0 case; no capability opt-in is
 *    needed because the phi sampling below is descriptor-agnostic.
 *
 * The pair tables are built by sampling the descriptors' OWN public calc
 * routines (zeroed-scratch trick for the 2b block), so they are node-exact
 * against the generic compute path for ANY radial D2 descriptor without
 * knowing its basis internals.
 *
 * Certified range. A table only serves distances where it is known to be
 * accurate. At build time certify_table() compares every segment with the
 * analytic function, top-down from the cutoff, and records the smallest
 * node r_lo above which every segment meets a relative tolerance
 * (kCollapseRelTol) for the value AND the derivative, bounding each
 * segment's error from 31 interior check points (check_segment()).
 * collapse_phi() / collapse_rho() use the table for r >= r_lo and the
 * analytic function below it, so the fast kernels agree with the generic
 * ones everywhere: to the tolerance inside the certified range and exactly
 * below it. The range is one interval ending at the cutoff: the first
 * segment that fails, scanning down, ends it, even when segments further
 * down would pass. The scheme is unit-free: distances are in the
 * potential's own units and the tolerance is relative to a floor built
 * from the function itself. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <tadah/core/core_types.h>
#include <tadah/models/dc_selector.h>

namespace tadah {

/* Nodes per collapse table (pair_tadah builds every table with this). */
constexpr int kCollapseNR = 8192;

/* Relative tolerance every certified table segment meets, for the value and
 * for the derivative (energy and force). */
constexpr double kCollapseRelTol = 1e-9;

struct EamRadialTable {
  int nr = 0;
  double rmax = 0.0;       // table upper edge = descriptor cutoff
  double dr = 0.0;
  double inv_dr = 0.0;
  std::vector<double> v;   // node values   v_k   = f(k*dr)
  std::vector<double> d;   // node derivs   d_k   = f'(k*dr)

  /* Value and derivative from the same cubic Hermite segment, so that the
   * interpolated force is exactly minus the gradient of the interpolated
   * energy (matters for minimizer line searches). */
  inline void eval(const double r, double &val, double &der) const {
    double t = r * inv_dr;
    int k = static_cast<int>(t);
    if (k >= nr - 1) k = nr - 2;
    t -= k;
    const double p0 = v[k],   p1 = v[k + 1];
    const double m0 = d[k] * dr, m1 = d[k + 1] * dr;
    const double t2 = t * t;
    const double t3 = t2 * t;
    val = ( 2.0 * t3 - 3.0 * t2 + 1.0) * p0
        + (       t3 - 2.0 * t2 + t  ) * m0
        + (-2.0 * t3 + 3.0 * t2      ) * p1
        + (       t3 -       t2      ) * m1;
    der = (( 6.0 * t2 - 6.0 * t      ) * p0
        +  ( 3.0 * t2 - 4.0 * t + 1.0) * m0
        +  (-6.0 * t2 + 6.0 * t      ) * p1
        +  ( 3.0 * t2 - 2.0 * t      ) * m1) * inv_dr;
  }
  inline double eval_val(const double r) const {
    double val, der;
    eval(r, val, der);
    return val;
  }
};

/* Where a table may stand in for its analytic function.
 *
 * Every segment of [r_lo, r_hi] meets the relative tolerance `tol` for the
 * value and for the derivative (see certify_table()); below r_lo the
 * callers evaluate the analytic function. r_lo is a table node and is at
 * least dr, because the head segment [0, dr) is extrapolated. An empty
 * certificate (r_lo == r_hi) means the top segment, just below the cutoff,
 * already fails. max_rel_val / max_rel_der are the largest error bounds
 * over the certified segments, relative to the floor (certify_table()). */
struct TableCertificate {
  double r_lo = 0.0;         // lowest certified distance (a table node)
  double r_hi = 0.0;         // table upper edge (the cutoff)
  double tol = 0.0;          // relative tolerance met on [r_lo, r_hi]
  double max_rel_val = 0.0;  // bound on the floored relative value error
  double max_rel_der = 0.0;  // bound on the floored relative derivative error
  int n = 0;                 // certified segments

  bool empty() const { return !(r_lo < r_hi); }
};

/* Descriptor scratch for phi_analytic(): one aed/fd pair sized to the full
 * descriptor. Callers own it (pair_tadah keeps one per LammpsTadah, sized in
 * setup_eam_fastpath; the fast kernels are serial). */
struct CollapseScratch {
  tadah::core::aed_type aed;
  tadah::core::fd_type fd;

  void resize(const std::size_t ndesc) {
    if (aed.size() != ndesc) aed = tadah::core::aed_type(ndesc);
    if (fd.rows() != ndesc) fd = tadah::core::fd_type(ndesc);
  }
};

struct EamCollapseTables {
  bool valid = false;
  bool has_mb = true;      // false for the pure-2b (phi-only) collapse
  EamRadialTable phi;      // pair function (per pair, total; halves tallied)
  EamRadialTable rho;      // scalar pair density (empty when !has_mb)
  TableCertificate phi_cert;  // phi table vs phi_analytic()
  TableCertificate rho_cert;  // rho table vs eam_density() (empty when !has_mb)
  tadah::core::t_type w;   // full model weight vector (for eam_embed)
  int Z = 0;               // single species atomic number
};

namespace collapse_detail {

/* Finite test on the bit pattern: under -ffast-math the compiler may fold
 * std::isfinite (and NaN comparisons) to "always finite". */
inline bool finite_bits(const double x) {
  std::uint64_t b;
  std::memcpy(&b, &x, sizeof b);
  return (b & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
}

/* "%.6g": enough digits to name a node unambiguously (dr / r > 1e-4). */
inline std::string fmt_r(const double x) {
  char b[32];
  std::snprintf(b, sizeof b, "%.6g", x);
  return b;
}

/* One significant digit, exponent without padding: 4e-10, 1e-9, 0. */
inline std::string fmt_e1(const double x) {
  if (x == 0.0) return "0";
  char b[32];
  std::snprintf(b, sizeof b, "%.0e", x);   // e.g. "4e-10", "1e-09"
  const std::string s(b);
  const std::size_t e = s.find('e');
  if (e == std::string::npos) return s;
  std::string digits = s.substr(e + 2);
  digits.erase(0, digits.find_first_not_of('0'));
  if (digits.empty()) return s.substr(0, e);
  return s.substr(0, e) + "e" + (s[e + 1] == '-' ? "-" : "") + digits;
}

inline std::string cert_clause(const std::string &what,
                               const TableCertificate &c,
                               const char *val, const char *der) {
  return what + " certified for r in [" + fmt_r(c.r_lo) + ", " +
         fmt_r(c.r_hi) + "] (rel tol " + fmt_e1(c.tol) + ": " + val + " " +
         fmt_e1(c.max_rel_val) + ", " + der + " " + fmt_e1(c.max_rel_der) +
         ")";
}

/* Why a table has no certified range: the range is one interval ending at
 * the cutoff, so it is empty exactly when the top segment fails (segments
 * further down may pass). */
inline std::string uncertified(const std::string &what,
                               const TableCertificate &c) {
  return what + " table misses rel tol " + fmt_e1(c.tol) +
         " already in its top segment, just below the cutoff r=" +
         fmt_r(c.r_hi);
}

} // namespace collapse_detail

/* Fill `tab` on the uniform grid [0, rmax], nr nodes, from f(r, v, d)
 * (value and derivative). Nodes 1..nr-2 sit at k*dr. The top node holds
 * the limit from below at rmax: the table stands in for r < rmax, and a
 * cutoff function may drop to 0 AT rmax (Cut_Dummy does), which would pull
 * the whole top segment towards 0; sampling at nextafter(rmax, 0) also
 * avoids (nr-1)*dr rounding above rmax. Node 0 is a linear extrapolation
 * of node 1, so the head segment [0, dr) is defined but never certified. */
template <class F>
inline void sample_table(EamRadialTable &tab, const double rmax,
                         const int nr, F &&f) {
  tab.nr = nr;
  tab.rmax = rmax;
  tab.dr = rmax / (nr - 1);
  tab.inv_dr = 1.0 / tab.dr;
  tab.v.assign(nr, 0.0);
  tab.d.assign(nr, 0.0);
  for (int k = 1; k < nr - 1; ++k) f(k * tab.dr, tab.v[k], tab.d[k]);
  f(std::nextafter(rmax, 0.0), tab.v[nr - 1], tab.d[nr - 1]);
  tab.d[0] = tab.d[1];
  tab.v[0] = tab.v[1] - tab.d[1] * tab.dr;
}

/* Check intervals per table segment (see check_segment()). */
constexpr int kCollapseCheckIntervals = 32;

/* What check_segment() found on one table segment. */
struct SegmentCheck {
  bool finite = true;   // every node, analytic value and error is finite
  double err_v = 0.0;   // bound on |table - f| over the segment
  double err_d = 0.0;   // bound on |table' - f'| over the segment
  double max_v = 0.0;   // max |f| over the segment's nodes and check points
  double max_d = 0.0;   // max |f'| over the same points
};

namespace collapse_detail {

/* Bound on max |e(t)| over t in [0, 1] from the signed samples
 * e[i] = e(i/n), i = 0..n, of an interpolation error (e[0] = e[n] = 0:
 * the table interpolates its nodes).
 *
 * Between two samples the error may peak where f loses smoothness (a knot
 * of the basis, the junction of a switching cutoff), and the peak can be
 * narrow. On either side of such a point the error is smooth, so its value
 * there is bounded by the linear extrapolation from the two neighbouring
 * samples on that side plus the magnitude of their second difference (a
 * curvature allowance). The bound is the largest sample and the largest
 * such extrapolation, taken from both sides of every sample. It holds when
 * at most one point of lost smoothness lies between adjacent samples; for
 * a smooth error it exceeds the maximum by a few per cent (n = 32). */
inline double interval_error_bound(const double *e, const int n) {
  double b = 0.0;
  for (int i = 1; i < n; ++i) {
    double fwd = std::fabs(2.0 * e[i] - e[i - 1]);   // towards i + 1
    if (i >= 2) fwd += std::fabs(e[i] - 2.0 * e[i - 1] + e[i - 2]);
    double bwd = std::fabs(2.0 * e[i] - e[i + 1]);   // towards i - 1
    if (i <= n - 2) bwd += std::fabs(e[i] - 2.0 * e[i + 1] + e[i + 2]);
    b = std::max({b, std::fabs(e[i]), fwd, bwd});
  }
  return b;
}

} // namespace collapse_detail

/* Compare table segment s ([s dr, (s+1) dr]) with the analytic f(r, v, d)
 * it samples, at the kCollapseCheckIntervals - 1 interior points
 * t = i / kCollapseCheckIntervals, and bound the value and derivative
 * errors over the whole segment (collapse_detail::interval_error_bound).
 * Dense sampling matters: a knot or cutoff junction next to a node puts a
 * narrow error peak between sparse check points (three points per segment
 * underestimated the derivative error by up to 3.5x). */
template <class F>
inline SegmentCheck check_segment(const EamRadialTable &tab, const int s,
                                  F &&f) {
  using collapse_detail::finite_bits;
  constexpr int n = kCollapseCheckIntervals;
  SegmentCheck c;
  for (int k = s; k <= s + 1; ++k) {
    if (!finite_bits(tab.v[k]) || !finite_bits(tab.d[k])) {
      c.finite = false;
      return c;
    }
    c.max_v = std::max(c.max_v, std::fabs(tab.v[k]));
    c.max_d = std::max(c.max_d, std::fabs(tab.d[k]));
  }
  double ev[n + 1], ed[n + 1];
  ev[0] = ev[n] = ed[0] = ed[n] = 0.0;
  for (int i = 1; i < n; ++i) {
    const double r = (s + static_cast<double>(i) / n) * tab.dr;
    double av = 0.0, ad = 0.0, tv = 0.0, td = 0.0;
    f(r, av, ad);
    tab.eval(r, tv, td);
    ev[i] = tv - av;
    ed[i] = td - ad;
    if (!finite_bits(av) || !finite_bits(ad) || !finite_bits(ev[i]) ||
        !finite_bits(ed[i])) {
      c.finite = false;
      return c;
    }
    c.max_v = std::max(c.max_v, std::fabs(av));
    c.max_d = std::max(c.max_d, std::fabs(ad));
  }
  c.err_v = collapse_detail::interval_error_bound(ev, n);
  c.err_d = collapse_detail::interval_error_bound(ed, n);
  return c;
}

/* Certify `tab` against the analytic f(r, v, d) it samples.
 *
 * Segments are scanned top-down from the cutoff. Segment s passes when its
 * error bounds (check_segment()) are at most tol * M_v(s) for the value
 * and tol * M_d(s) for the derivative, with the unit-free floor
 *   M(s) = max( max|f| on [rc/2, rc], max |f| over segments s' >= s )
 * (separately for f and f'). The floor keeps zero crossings and tapered
 * cutoffs, where |f| -> 0, from failing on errors that are small on the
 * function's own scale, while the running maximum keeps the criterion
 * relative where |f| grows (r^-12 walls). max_rel_val / max_rel_der are
 * the largest bound / floor ratios over the certified segments.
 *
 * r_lo is the upper node of the highest failing segment, and at least dr:
 * the extrapolated head [0, dr) is never certified. A non-finite value or
 * error fails its segment. The certified range is one interval ending at
 * the cutoff, so a single failing segment ends it. When the top segment
 * fails, the certificate is empty (r_lo == r_hi). */
template <class F>
inline TableCertificate certify_table(const EamRadialTable &tab, F &&f,
                                      const double tol = kCollapseRelTol) {
  using collapse_detail::finite_bits;
  TableCertificate c;
  c.tol = tol;
  c.r_hi = tab.rmax;
  c.r_lo = tab.rmax;
  const int nseg = tab.nr - 1;
  if (nseg < 2) return c;

  double ref_v = 0.0, ref_d = 0.0;   // max |f|, |f'| on [rc/2, rc]
  for (int k = 1; k < tab.nr; ++k) {
    if (k * tab.dr < 0.5 * tab.rmax) continue;
    if (finite_bits(tab.v[k])) ref_v = std::max(ref_v, std::fabs(tab.v[k]));
    if (finite_bits(tab.d[k])) ref_d = std::max(ref_d, std::fabs(tab.d[k]));
  }

  double run_v = ref_v, run_d = ref_d;
  int s = nseg - 1;
  for (; s >= 1; --s) {             // segment 0 is extrapolated
    const SegmentCheck k = check_segment(tab, s, f);
    if (!k.finite) break;
    run_v = std::max(run_v, k.max_v);
    run_d = std::max(run_d, k.max_d);
    if (k.err_v > tol * run_v || k.err_d > tol * run_d) break;
    if (run_v > 0.0) c.max_rel_val = std::max(c.max_rel_val, k.err_v / run_v);
    if (run_d > 0.0) c.max_rel_der = std::max(c.max_rel_der, k.err_d / run_d);
  }
  c.r_lo = (s == nseg - 1) ? tab.rmax : (s + 1) * tab.dr;
  c.n = nseg - 1 - s;
  if (c.n == 0) c.max_rel_val = c.max_rel_der = 0.0;
  return c;
}

/* The analytic pair term w . aed(r) and its r-derivative w . fd(r), from
 * the 2b descriptor's own calc_all (zeroed scratch). Builds the phi table
 * nodes and is the fast kernels' evaluation below the certified range, so
 * both agree with the generic compute path node for node. `scratch` must be
 * sized to the descriptor (w.size()). */
inline void phi_analytic(const models::DC_Selector &S,
                         const tadah::core::t_type &w,
                         const int Zi, const int Zj, const double r,
                         CollapseScratch &scratch, double &v, double &d) {
  scratch.aed.set_zero();
  scratch.fd.set_zero();
  S.d2b->calc_all(Zi, Zj, r, r * r, scratch.aed, scratch.fd, 1.0);
  double pv = 0.0, pd = 0.0;
  const std::size_t ndesc = w.size();
  for (std::size_t c = 0; c < ndesc; ++c) {
    pv += w(c) * scratch.aed(c);
    pd += w(c) * scratch.fd(c, 0);
  }
  v = pv;
  d = pd;
}

/* phi(r) table from phi_analytic(). (Zi,Zj) is passed explicitly so a
 * future multi-species extension can key one table per pair; today callers
 * pass the single species twice. */
inline void sample_phi_table(const models::DC_Selector &S,
                             const tadah::core::t_type &w,
                             const int Zi, const int Zj, const int nr,
                             EamRadialTable &tab, CollapseScratch &scratch) {
  sample_table(tab, S.d2b->get_rcut(), nr,
               [&](const double r, double &v, double &d) {
                 phi_analytic(S, w, Zi, Zj, r, scratch, v, d);
               });
}

/* phi(r), phi'(r) for a pair at r < T.phi.rmax: the table inside its
 * certified range, phi_analytic() below it. */
inline void collapse_phi(const EamCollapseTables &T,
                         const models::DC_Selector &S, const double r,
                         CollapseScratch &scratch, double &v, double &d) {
  if (r >= T.phi_cert.r_lo) T.phi.eval(r, v, d);
  else phi_analytic(S, T.w, T.Z, T.Z, r, scratch, v, d);
}

/* rho(r), rho'(r) for a pair at r < T.rho.rmax: the table inside its
 * certified range, the descriptor's analytic eam_density() below it. */
inline void collapse_rho(const EamCollapseTables &T,
                         const models::DC_Selector &S, const double r,
                         double &v, double &d) {
  if (r >= T.rho_cert.r_lo) T.rho.eval(r, v, d);
  else S.dmb->eam_density(T.Z, T.Z, r, v, d);
}

/* The load-time log line (without the "Tadah!" prefix): the certified
 * range(s), the tolerance with the errors achieved, and where the fast
 * kernels evaluate analytically. Distances are in the potential's units. */
inline std::string format_collapse_report(const EamCollapseTables &T) {
  using collapse_detail::cert_clause;
  using collapse_detail::fmt_r;
  const std::string nr = std::to_string(T.phi.nr);
  if (!T.has_mb)
    return "pair-collapse fast path ON (2b-only, nr=" + nr + "): " +
           cert_clause("table", T.phi_cert, "energy", "force") +
           "; analytic below r=" + fmt_r(T.phi_cert.r_lo);
  return "EAM-collapse fast path ON (nr=" + nr + "): " +
         cert_clause("phi table", T.phi_cert, "energy", "force") + ", " +
         cert_clause("rho table", T.rho_cert, "density", "derivative") +
         "; analytic below r=" + fmt_r(T.phi_cert.r_lo) + " (phi) and r=" +
         fmt_r(T.rho_cert.r_lo) + " (rho)";
}

/* Build the pure two-body collapse (phi table only; INITMB false).
 * Returns false (with `why` set) when this S/model combination does not
 * collapse, or when the phi table has no certified range (its top segment
 * fails); the caller then keeps the generic compute path. `w` must be the full linear
 * weight vector (no bias). */
inline bool build_pair2b_collapse(const models::DC_Selector &S,
                                  const tadah::core::t_type &w,
                                  const int Z, const int nr,
                                  EamCollapseTables &T, std::string &why) {
  T.valid = false;
  if (!S.d2b) { why = "missing d2b"; return false; }
  if (nr < 3) { why = "table needs at least 3 nodes"; return false; }
  const std::size_t ndesc = S.descriptor_size();
  if (w.size() != ndesc) {
    why = "weight count " + std::to_string(w.size()) +
          " != descriptor size " + std::to_string(ndesc);
    return false;
  }
  const double rc2b = S.d2b->get_rcut();
  if (!(rc2b > 0.0)) { why = "non-positive cutoff"; return false; }

  T.w = w;
  T.Z = Z;
  T.has_mb = false;
  T.rho = EamRadialTable{};      // no density in the pure-2b collapse
  T.rho_cert = TableCertificate{};

  CollapseScratch scratch;
  scratch.resize(ndesc);
  const auto phi_fn = [&](const double r, double &v, double &d) {
    phi_analytic(S, w, Z, Z, r, scratch, v, d);
  };
  sample_table(T.phi, rc2b, nr, phi_fn);
  T.phi_cert = certify_table(T.phi, phi_fn);
  if (T.phi_cert.empty()) {
    why = collapse_detail::uncertified("phi", T.phi_cert);
    return false;
  }

  T.valid = true;
  return true;
}

/* Build the EAM collapse tables (radial D2 + EAM-form DM). Returns false
 * (with `why` set) when this S/model combination does not collapse, or when
 * the phi or the rho table has no certified range (its top segment fails);
 * the caller then keeps the generic compute path. `w` must be the full linear weight vector
 * (no bias). */
inline bool build_eam_collapse(const models::DC_Selector &S,
                               const tadah::core::t_type &w,
                               const int Z, const int nr,
                               EamCollapseTables &T, std::string &why) {
  T.valid = false;
  if (!S.d2b || !S.dmb) { why = "missing d2b/dmb"; return false; }
  if (!S.dmb->eam_form()) {
    why = "DM descriptor " + S.dmb->label() + " is not EAM-form";
    return false;
  }
  if (nr < 3) { why = "table needs at least 3 nodes"; return false; }
  const std::size_t ndesc = S.descriptor_size();
  if (w.size() != ndesc) {
    why = "weight count " + std::to_string(w.size()) +
          " != descriptor size " + std::to_string(ndesc);
    return false;
  }

  const double rc2b = S.d2b->get_rcut();
  const double rcmb = S.dmb->get_rcut();
  if (!(rc2b > 0.0) || !(rcmb > 0.0)) { why = "non-positive cutoff"; return false; }

  T.w = w;
  T.Z = Z;
  T.has_mb = true;

  CollapseScratch scratch;
  scratch.resize(ndesc);
  const auto phi_fn = [&](const double r, double &v, double &d) {
    phi_analytic(S, w, Z, Z, r, scratch, v, d);
  };
  sample_table(T.phi, rc2b, nr, phi_fn);
  T.phi_cert = certify_table(T.phi, phi_fn);

  /* rho(r): exact scalar density from the DM descriptor */
  const auto rho_fn = [&](const double r, double &v, double &d) {
    S.dmb->eam_density(Z, Z, r, v, d);
  };
  sample_table(T.rho, rcmb, nr, rho_fn);
  T.rho_cert = certify_table(T.rho, rho_fn);

  if (T.phi_cert.empty()) {
    why = collapse_detail::uncertified("phi", T.phi_cert);
    return false;
  }
  if (T.rho_cert.empty()) {
    why = collapse_detail::uncertified("rho", T.rho_cert);
    return false;
  }

  T.valid = true;
  return true;
}

} // namespace tadah
#endif
