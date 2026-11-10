#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/core/number_format.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

// Every method of ReamDensity lives in this translation unit, compiled with
// MODELS' own flags, as DM_REAM's density loops before RHOFORM were: the
// SIGNED sums are those loops. In a MODELS compiled without FMA contraction
// (the presets' flags on x86-64: no -march=native, no -ffast-math; LTO and
// a -march=native tadah included) they are the density before 1.6.0 bit
// for bit, and the bitwise relations of ream_density.h hold
// (test_ream_goldens.cpp, test_ream_density.cpp,
// tadah.models_ream_goldens_native_link). Do not move any of it into a
// header: a -march=native unit that includes it contracts a*b+c into an FMA
// (measured to change the density bits of 7 of the 9 campaign fixtures),
// and an LTO link may then keep that unit's copy for every caller. A MODELS
// compiled with FMA contraction (-march=native on an FMA machine; AArch64,
// whose compilers fuse at default flags) gives these sums and relations to
// rounding only, and nothing pins this file's flags, deliberately: with
// -ffp-contract=off here ReamDensity's own relations hold, but its
// agreement with the density of EAMRefit and potedit, compiled with FMA,
// breaks, and the goldens fail either way (GCC 15.2, 2026-10-12). Without
// LTO such a build gives the bits of the earlier version built the same
// way; with LTO 205 of the 1770 golden points differed (by up to 97 ulp).

namespace tadah {
namespace models {

namespace {

std::string num(double v) { return tadah::core::format_roundtrip(v); }

// Contiguous grid-index range [lo, hi) of scan_grid's grid outside which a
// term is an exact zero, from the term's support edges in r (lo_edge may be
// -inf, hi_edge +inf). Widened by one index on each side, so a point below
// lo lies at least one grid step beyond the support edge: there psi_n == 0
// and S_n == 1 exactly. A non-finite edge (s_n == 0 or not finite) gives
// the full range, i.e. every point is evaluated.
struct IndexRange {
  std::size_t lo;
  std::size_t hi;
};

IndexRange support_range(double lo_edge, double hi_edge, double r_lo,
                         double step, std::size_t n_end) {
  IndexRange out{0, n_end};
  const double flo = std::floor((lo_edge - r_lo) / step) - 1.0;
  if (flo > 0.0)  // NaN and -inf keep 0
    out.lo = flo >= static_cast<double>(n_end) ? n_end
                                                : static_cast<std::size_t>(flo);
  const double fhi = std::ceil((hi_edge - r_lo) / step) + 1.0;
  if (fhi < 0.0)
    out.hi = 0;
  else if (fhi < static_cast<double>(n_end))  // NaN and +inf keep n_end
    out.hi = static_cast<std::size_t>(fhi) + 1;
  if (out.hi < out.lo) out.hi = out.lo;
  return out;
}

}  // namespace

int ReamDensity::deg(std::size_t n) const {
  return n < rho_deg.size() ? static_cast<int>(rho_deg[n]) : 5;
}

double ReamDensity::width(std::size_t n) const {
  // Knot family: SGRIDMB is absent (eta is a pure gauge) -> s = 1 (ignored
  // by the knot arms of the dispatcher).
  return n < srho.size() ? srho[n] : 1.0;
}

BasisKind ReamDensity::basis_from_context(const tadah::core::Context &ctx) {
  static const char *const kKnown =
      " (known: 0=Blip, 1=Gaussian, 2=Knot5, 3=Blip5).";
  const auto tok = ctx.stored_value("TYPEMB", 1);
  if (!tok)
    throw std::runtime_error(
        std::string("TYPEMB has no basis code (TYPEMB DM_REAM <basis code> "
                    "...") + ")" + kKnown);
  if (!tadah::core::is_integer_literal(*tok))
    throw std::runtime_error("TYPEMB basis code '" + *tok +
                             "' is not an integer" + kKnown);
  int code = 0;
  try {
    code = std::stoi(*tok);
  } catch (const std::out_of_range &) {
    throw std::runtime_error("TYPEMB basis code '" + *tok +
                             "' is not a basis code" + kKnown);
  }
  return basis_kind_from_code(code);
}

ReamDensity ReamDensity::from_context(const tadah::core::Context &ctx) {
  ReamDensity d;
  d.form = require_rhoform(ctx, "");

  const auto label = ctx.stored_value("TYPEMB", 0);
  if (!label || *label != "DM_REAM")
    throw DensityFormError(
        "DM_REAM density: the context is not a bare DM_REAM block (TYPEMB " +
        (label ? *label : std::string("absent")) + ").");
  // The basis code as DM_REAM reads it (basis_from_context).
  try {
    d.basis = basis_from_context(ctx);
  } catch (const std::runtime_error &e) {
    throw DensityFormError(std::string("DM_REAM density: ") + e.what());
  }

  for (const char *key : {"CGRIDMB", "AMPGRIDMB"})
    if (!ctx.has_stored(key))
      throw DensityFormError(std::string("DM_REAM density: ") + key +
                             " is missing.");
  D_Base::get_grid(ctx, "CGRIDMB", d.crho);
  if (ctx.has_stored("SGRIDMB"))
    D_Base::get_grid(ctx, "SGRIDMB", d.srho);  // absent for the knot family
  D_Base::get_grid(ctx, "AMPGRIDMB", d.arho);
  if (ctx.has_stored("KDEGMB"))
    D_Base::get_grid(ctx, "KDEGMB", d.rho_deg);  // optional knot degrees
  return d;
}

void ReamDensity::validate(Cut_Base *fc) const {
  validate(fc ? fc->label() : std::string());
}

void ReamDensity::validate(const std::string &rctype) const {
  // The pre-existing density checks, texts unchanged.
  if (crho.size() != arho.size())
    throw DensityFormError("DM_REAM: CGRIDMB / AMPGRIDMB sizes differ.");
  if (basis_traits(basis).eta_is_gauge) {
    if (!srho.empty())
      throw DensityFormError(
          "DM_REAM (knot family): SGRIDMB/SEMBFUNC must be empty — eta is a "
          "pure gauge ((c,eta)->(c s^k, eta/s)); the knot position grids "
          "CGRIDMB/CEMBFUNC define the basis alone.");
  } else {
    if (crho.size() != srho.size())
      throw DensityFormError("DM_REAM: CGRIDMB / SGRIDMB sizes differ.");
    for (const auto &e : srho)
      if (std::abs(e) < std::numeric_limits<double>::min())
        throw DensityFormError("DM_REAM: zero entry in SGRIDMB.");
  }
  if (!rho_deg.empty() && rho_deg.size() != crho.size())
    throw DensityFormError("DM_REAM: KDEGMB / CGRIDMB sizes differ.");
  for (const auto &dg : rho_deg)
    if (dg != 3.0 && dg != 5.0)
      throw DensityFormError(
          "DM_REAM: KDEG* entries must be 3 or 5 (the knot-degree lists "
          "are structure data, not optimisable parameters).");

  if (form == RhoForm::Signed) return;
  const std::string who = rhoform_subject(form);
  if (form == RhoForm::Monotone && basis_traits(basis).eta_is_gauge)
    throw DensityFormError(
        who + " is not defined for the knot family (basis code 2, any "
        "KDEGMB): use RHOFORM NONNEG: " + monotone_knot_refusal_text() + ".");
  for (std::size_t n = 0; n < arho.size(); ++n)
    if (!std::isfinite(arho[n]) || arho[n] < 0.0)
      throw DensityFormError(
          who + " requires every AMPGRIDMB entry to be finite and >= 0; entry " +
          std::to_string(n + 1) + " is " + num(arho[n]) + ".");
  // A non-finite centre or width makes the term NaN (x = s (r - c) is NaN,
  // or inf * 0 at r = c), and NaN breaks rho >= 0.
  for (std::size_t n = 0; n < crho.size(); ++n)
    if (!std::isfinite(crho[n]))
      throw DensityFormError(
          who + " requires every CGRIDMB entry to be finite; entry " +
          std::to_string(n + 1) + " is " + num(crho[n]) + ".");
  if (form != RhoForm::Monotone) {
    // The Blip bumps are even in s, but the Gaussian exp(-s (r - c)^2)
    // grows without bound for s < 0 (inf * 0 = NaN at the cutoff), which
    // breaks rho >= 0: its widths must be > 0 as well.
    const bool gauss = basis == BasisKind::Gaussian;
    for (std::size_t n = 0; n < srho.size(); ++n)  // empty for the knot family
      if (!std::isfinite(srho[n]) || (gauss && !(srho[n] > 0.0)))
        throw DensityFormError(
            who + " requires every SGRIDMB entry to be finite" +
            (gauss ? std::string(" and > 0 for the Gaussian basis") : "") +
            "; entry " + std::to_string(n + 1) + " is " + num(srho[n]) + ".");
    return;
  }
  for (std::size_t n = 0; n < srho.size(); ++n)
    if (!std::isfinite(srho[n]) || !(srho[n] > 0.0))
      throw DensityFormError(
          who + " requires every SGRIDMB entry to be finite and > 0 (S_n "
          "steps down across the bump s_n (r - c_n)); entry " +
          std::to_string(n + 1) + " is " + num(srho[n]) + ".");
  if (rctype.empty())
    throw DensityFormError(
        who + " needs the many-body cutoff (RCTYPEMB) to validate the form.");
  if (!rhoform_cutoff_whitelisted(rctype))
    throw DensityFormError(
        who + " requires a non-negative, non-increasing many-body cutoff: "
        "RCTYPEMB must be one of " + rhoform_cutoff_whitelist_text() +
        " (got " + rctype + ").");
}

void ReamDensity::validate_context(const tadah::core::Context &ctx) {
  const ReamDensity d = from_context(ctx);
  // The DM_Base default without RCTYPEMB (dm_base.h) is Cut_Dummy.
  d.validate(ctx.stored_value("RCTYPEMB", 0).value_or("Cut_Dummy"));
  if (rhoform_is_positive(d.form))
    D_Base::require_positive_density_weights(ctx, rhoform_subject(d.form));
}

// ------------------------------ point values ------------------------------
// SIGNED and NONNEG are verbatim copies of the DM_REAM loops before RHOFORM
// (rho_raw, rho_raw_deriv, the eam_density sum): acc += a_n * term, n
// ascending, W outside. NONNEG and MONOTONE clamp the cutoff value at 0
// first; the form is branched on outside the n-loop.

double ReamDensity::value(double r, double fc) const {
  const std::size_t N = crho.size();
  double acc = 0.0;
  if (form == RhoForm::Signed) {
    for (std::size_t n = 0; n < N; ++n)
      acc += arho[n] * radial_value(basis, r, fc, width(n), crho[n], deg(n));
    return acc;
  }
  const double f = fc > 0.0 ? fc : 0.0;
  if (form == RhoForm::NonNeg) {
    for (std::size_t n = 0; n < N; ++n)
      acc += arho[n] * radial_value(basis, r, f, width(n), crho[n], deg(n));
    return acc;
  }
  for (std::size_t n = 0; n < N; ++n)
    acc += arho[n] * radial_step_value(basis, r, f, width(n), crho[n]);
  return acc;
}

double ReamDensity::deriv(double r, double fc, double fcp) const {
  const std::size_t N = crho.size();
  double acc = 0.0;
  if (form == RhoForm::Signed) {
    for (std::size_t n = 0; n < N; ++n)
      acc += arho[n] *
             radial_deriv(basis, r, fc, fcp, width(n), crho[n], deg(n));
    return acc;
  }
  const double f = fc > 0.0 ? fc : 0.0;
  if (form == RhoForm::NonNeg) {
    for (std::size_t n = 0; n < N; ++n)
      acc += arho[n] *
             radial_deriv(basis, r, f, fcp, width(n), crho[n], deg(n));
    return acc;
  }
  for (std::size_t n = 0; n < N; ++n)
    acc += arho[n] * radial_step_deriv(basis, r, f, fcp, width(n), crho[n]);
  return acc;
}

void ReamDensity::value_deriv(double r, double fc, double fcp,
                              double &v, double &d) const {
  const std::size_t N = crho.size();
  double rho_raw_both = 0.0;
  double rhop_raw = 0.0;
  if (form == RhoForm::Monotone) {
    const double f = fc > 0.0 ? fc : 0.0;
    for (std::size_t n = 0; n < N; ++n) {
      double step_value, step_deriv;
      radial_step_value_deriv(basis, r, f, fcp, width(n), crho[n],
                              step_value, step_deriv);
      rho_raw_both += arho[n] * step_value;
      rhop_raw += arho[n] * step_deriv;
    }
    v = rho_raw_both;
    d = rhop_raw;
    return;
  }
  const double f = form == RhoForm::NonNeg ? (fc > 0.0 ? fc : 0.0) : fc;
  for (std::size_t n = 0; n < N; ++n) {
    double psi_value, psi_deriv;
    radial_value_deriv(basis, r, f, fcp, width(n), crho[n],
                       psi_value, psi_deriv, deg(n));
    rho_raw_both += arho[n] * psi_value;
    rhop_raw += arho[n] * psi_deriv;
  }
  v = rho_raw_both;
  d = rhop_raw;
}

// --------------------------------- scans ----------------------------------

std::vector<double> ReamDensity::linspace(double lo, double hi,
                                          std::size_t n) {
  std::vector<double> x(n);
  if (n == 0) return x;
  if (n == 1) {
    x[0] = lo;
    return x;
  }
  const double step = (hi - lo) / static_cast<double>(n - 1);
  for (std::size_t i = 0; i + 1 < n; ++i)
    x[i] = static_cast<double>(i) * step + lo;
  x[n - 1] = hi;
  return x;
}

DensityScan ReamDensity::scan_grid(double r_lo, double r_hi, std::size_t n,
                                   Cut_Base &fc, std::vector<double> *rho,
                                   std::vector<double> *term_abs) const {
  if (n < 2 || !std::isfinite(r_lo) || !std::isfinite(r_hi) || !(r_lo < r_hi))
    throw std::invalid_argument(
        "ReamDensity::scan_grid: needs n >= 2 and finite r_lo < r_hi (got n " +
        std::to_string(n) + ", window [" + num(r_lo) + ", " + num(r_hi) + "]).");
  const std::vector<double> r = linspace(r_lo, r_hi, n);
  const double step = (r_hi - r_lo) / static_cast<double>(n - 1);

  // calc_rho's rule: a pair beyond rcut adds nothing. The grid ascends, so
  // the points r <= rcut are the prefix [0, n_end).
  const double rc = fc.get_rcut();
  std::size_t n_end = n;
  while (n_end > 0 && r[n_end - 1] > rc) --n_end;
  const bool positive = form != RhoForm::Signed;
  std::vector<double> f(n_end);
  for (std::size_t i = 0; i < n_end; ++i) {
    const double v = fc.calc(r[i]);
    f[i] = positive ? (v > 0.0 ? v : 0.0) : v;
  }

  std::vector<double> buf;
  std::vector<double> &acc = rho ? *rho : buf;
  acc.assign(n, 0.0);
  if (term_abs) term_abs->assign(n, 0.0);

  const double w = bump_halfwidth(basis);
  const double inf = std::numeric_limits<double>::infinity();
  for (std::size_t k = 0; k < crho.size(); ++k) {
    const double a = arho[k];
    const double s = width(k);
    const double c = crho[k];
    // support edges in r: bumps |s (r - c)| < w; left-active knots r < c;
    // the Gaussian is not compact
    double lo_edge = -inf, hi_edge = inf;
    if (!std::isfinite(a)) {
      // (SIGNED only; the positive forms refuse it) value() gives
      // a * 0 = NaN outside the support too: every point is evaluated
    } else if (form == RhoForm::Monotone && !(s > 0.0)) {
      // not a valid MONOTONE width (validate() refuses it): no S == 1
      // shortcut, every point is evaluated
    } else if (w > 0.0) {
      const double h = w / std::fabs(s);
      lo_edge = c - h;
      hi_edge = c + h;
    } else if (basis_traits(basis).eta_is_gauge) {
      hi_edge = c;
    }
    const IndexRange ir = support_range(lo_edge, hi_edge, r_lo, step, n_end);
    if (form == RhoForm::Monotone) {
      for (std::size_t i = 0; i < ir.lo; ++i) {  // S_k == 1 exactly
        const double t = a * (1.0 * f[i]);
        acc[i] += t;
        if (term_abs) (*term_abs)[i] += std::fabs(t);
      }
      for (std::size_t i = ir.lo; i < ir.hi; ++i) {
        const double t = a * radial_step_value(basis, r[i], f[i], s, c);
        acc[i] += t;
        if (term_abs) (*term_abs)[i] += std::fabs(t);
      }
    } else {
      const int dg = deg(k);
      for (std::size_t i = ir.lo; i < ir.hi; ++i) {
        const double t = a * radial_value(basis, r[i], f[i], s, c, dg);
        acc[i] += t;
        if (term_abs) (*term_abs)[i] += std::fabs(t);
      }
    }
  }

  DensityScan out;
  out.rho_min = out.rho_max = acc[0];
  for (std::size_t i = 0; i < n; ++i) {
    const double v = acc[i];
    if (!std::isfinite(v)) out.finite = false;
    if (v < out.rho_min) { out.rho_min = v; out.i_min = i; }
    if (v > out.rho_max) { out.rho_max = v; out.i_max = i; }
  }
  out.r_at_min = r[out.i_min];
  out.r_at_max = r[out.i_max];
  return out;
}

RhoBarScan ReamDensity::rhobar_scan(
    const std::vector<eam_lattice::Shell> &shells,
    const std::vector<double> &a, Cut_Base &fc) const {
  RhoBarScan out;
  out.rhobar.assign(a.size(), 0.0);
  const double rc = fc.get_rcut();
  for (std::size_t k = 0; k < a.size(); ++k) {
    double rb = 0.0;
    for (const auto &sh : shells) {
      const double r = sh.d * a[k];
      if (r > rc) break;  // calc_rho's rule; the shells ascend in d
      rb += static_cast<double>(sh.m) * value(r, fc.calc(r));
    }
    out.rhobar[k] = rb;
    if (!std::isfinite(rb)) out.finite = false;
  }
  for (std::size_t k = 0; k + 1 < a.size(); ++k)
    if (out.rhobar[k + 1] >= out.rhobar[k]) {
      if (out.rises == 0) out.first_rise = k;
      ++out.rises;
    }
  return out;
}

}  // namespace models
}  // namespace tadah
