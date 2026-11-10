#ifndef TADAH_MODELS_DESCRIPTORS_BASIS_DISPATCH_H
#define TADAH_MODELS_DESCRIPTORS_BASIS_DISPATCH_H

// The ONE runtime dispatch that maps a BasisKind to the hand-tuned basis
// primitives in d_basis_functions.h. Every runtime "which basis?" decision in
// the codebase funnels through here instead of a per-site if/switch on a
// descriptor name or a private enum (the anti-pattern the design removes).
//
// Cost note (goal d, "never regress speed"): each function is a single
// `static inline` switch over a 4-value enum -> a jump table. It replaces the
// former per-call if-ladders (up to 3 branch compares) and forwards to the
// exact same inlined primitive, so codegen is neutral-to-better versus the
// hand-written dispatch it subsumes. The basis is validated once at descriptor
// construction (basis_kind_from_code throws on bad codes), so the trailing
// unreachable/throw is a cold, never-taken path.
//
// Two orientations, matching the descriptor contract:
//   * radial_*   : LEFT-active knots, cutoff-weighted (density / two-body).
//   * embedding_*: RIGHT-active knots, NO cutoff (embedding F(rho)).
//   * *_bare     : no cutoff weight (basis analysis / plotting).
//   * radial_step_*: the integrated-bump step S of the bump families
//                  (DM_REAM RHOFORM MONOTONE); the knot family throws.

#include <string>

#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/d_basis_functions.h>

namespace tadah {
namespace models {
namespace basis_detail {

[[noreturn]] inline void unknown_basis(BasisKind k) {
  throw std::runtime_error(
      "basis_dispatch: unhandled BasisKind code " +
      std::to_string(static_cast<int>(k)));
}

[[noreturn]] inline void no_step_form(BasisKind k) {
  throw std::runtime_error(
      std::string("basis_dispatch: the ") + basis_name(k) +
      " family has no integrated-bump step form (RHOFORM MONOTONE is "
      "defined for the bump families Blip, Blip5 and Gaussian only).");
}

}  // namespace basis_detail

// --------------------------------------------------------------------------
//  Radial basis  psi(r) * f_c(r)   (LEFT-active knots)
//  s = width (eta / SGRID; ignored by the knot family), c = centre (mu / CGRID),
//  deg = knot degree 3|5 (ignored by non-knot families).
// --------------------------------------------------------------------------

/** \brief psi_c(r) * f_c(r) for the given basis. */
inline double radial_value(BasisKind k, double r, double fc,
                           double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return B (s * (r - c), fc);
    case BasisKind::Blip5:    return B5(s * (r - c), fc);
    case BasisKind::Knot5:    return deg == 3 ? K3L(r - c, fc) : K5L(r - c, fc);
    case BasisKind::Gaussian: return G (r, s, c, fc);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d/dr [ psi_c(r) * f_c(r) ] for the given basis (fcp = f_c'(r)). */
inline double radial_deriv(BasisKind k, double r, double fc, double fcp,
                           double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return dB (s * (r - c), s, fc, fcp);
    case BasisKind::Blip5:    return dB5(s * (r - c), s, fc, fcp);
    case BasisKind::Knot5:    return deg == 3 ? dK3L(r - c, 1.0, fc, fcp)
                                              : dK5L(r - c, 1.0, fc, fcp);
    case BasisKind::Gaussian: return dG (r, s, c, fc, fcp);
  }
  basis_detail::unknown_basis(k);
}

/** Evaluate a cutoff-weighted radial basis and its derivative together.
 * The measured Blip5 path shares support tests; the other families retain
 * their original expressions exactly. */
inline void radial_value_deriv(BasisKind k, double r, double fc, double fcp,
                               double s, double c,
                               double &value, double &deriv, int deg = 5) {
  if (k == BasisKind::Blip5) {
    double bare_value, bare_deriv;
    B5_value_deriv(s * (r - c), s, bare_value, bare_deriv);
    value = bare_value * fc;
    deriv = fc * bare_deriv + fcp * bare_value;
    return;
  }
  value = radial_value(k, r, fc, s, c, deg);
  deriv = radial_deriv(k, r, fc, fcp, s, c, deg);
}

// --------------------------------------------------------------------------
//  Embedding basis  g_m(rho)   (RIGHT-active knots, NO cutoff)
// --------------------------------------------------------------------------

/** \brief g_m(rho) for the given basis (embedding, no cutoff). */
inline double embedding_value(BasisKind k, double rho,
                              double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return B (s * (rho - c));
    case BasisKind::Blip5:    return B5(s * (rho - c));
    case BasisKind::Knot5:    return deg == 3 ? K3R(rho - c) : K5R(rho - c);
    case BasisKind::Gaussian: return G (rho, s, c);
  }
  basis_detail::unknown_basis(k);
}

/** \brief g'_m(rho) w.r.t. rho for the given basis (embedding, no cutoff). */
inline double embedding_deriv(BasisKind k, double rho,
                              double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return dB (s * (rho - c), s);
    case BasisKind::Blip5:    return dB5(s * (rho - c), s);
    case BasisKind::Knot5:    return deg == 3 ? dK3R(rho - c, 1.0)
                                              : dK5R(rho - c, 1.0);
    case BasisKind::Gaussian: return dG (rho, s, c);
  }
  basis_detail::unknown_basis(k);
}

/** Evaluate an embedding basis and its derivative together.
 * The measured Blip5 path shares support tests; the other families retain
 * their original expressions exactly. */
inline void embedding_value_deriv(BasisKind k, double rho,
                                  double s, double c,
                                  double &value, double &deriv, int deg = 5) {
  if (k == BasisKind::Blip5) {
    B5_value_deriv(s * (rho - c), s, value, deriv);
    return;
  }
  value = embedding_value(k, rho, s, c, deg);
  deriv = embedding_deriv(k, rho, s, c, deg);
}

/** \brief g''_m(rho) w.r.t. rho for the given basis (embedding, no cutoff).
 *  The bump primitives return bare d^2/dx_c^2, so the s^2 chain factor is
 *  applied here; knots carry eta = 1 (pure gauge). */
inline double embedding_deriv2(BasisKind k, double rho,
                               double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return s * s * d2B (s * (rho - c));
    case BasisKind::Blip5:    return s * s * d2B5(s * (rho - c));
    case BasisKind::Knot5:    return deg == 3 ? d2K3R(rho - c)
                                              : d2K5R(rho - c);
    case BasisKind::Gaussian: return d2G(rho, s, c);
  }
  basis_detail::unknown_basis(k);
}

// --------------------------------------------------------------------------
//  Bare radial basis  psi_c(r)  (no cutoff weight) — basis analysis / plots.
//  Matches the legacy `double(*)(double,double,double)` shape of models::B /
//  models::G etc. exactly, so it is a drop-in for the analysis engine.
// --------------------------------------------------------------------------

/** \brief psi_c(r) with no cutoff weight (LEFT-active knots). */
inline double radial_value_bare(BasisKind k, double r,
                                double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return B (s * (r - c));   // == B(r, s, c)
    case BasisKind::Blip5:    return B5(s * (r - c));
    case BasisKind::Knot5:    return deg == 3 ? K3L(r - c) : K5L(r - c);
    case BasisKind::Gaussian: return G (r, s, c);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d/dr psi_c(r) with no cutoff weight (LEFT-active knots). */
inline double radial_deriv_bare(BasisKind k, double r,
                                double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return dB (s * (r - c), s);  // == dB(r, s, c)
    case BasisKind::Blip5:    return dB5(s * (r - c), s);
    case BasisKind::Knot5:    return deg == 3 ? dK3L(r - c, 1.0)
                                              : dK5L(r - c, 1.0);
    case BasisKind::Gaussian: return dG (r, s, c);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d^2/dr^2 psi_c(r) with no cutoff weight (LEFT-active knots).
 *  The bump primitives return bare d^2/dx_c^2, so the s^2 chain factor is
 *  applied here; knots carry eta = 1 (pure gauge). */
inline double radial_deriv2_bare(BasisKind k, double r,
                                 double s, double c, int deg = 5) {
  switch (k) {
    case BasisKind::Blip:     return s * s * d2B (s * (r - c));
    case BasisKind::Blip5:    return s * s * d2B5(s * (r - c));
    case BasisKind::Knot5:    return deg == 3 ? d2K3L(r - c) : d2K5L(r - c);
    case BasisKind::Gaussian: return d2G(r, s, c);
  }
  basis_detail::unknown_basis(k);
}

// --------------------------------------------------------------------------
//  Integrated-bump step  S_c(r) * f_c(r)   (DM_REAM RHOFORM MONOTONE)
//  Bump families only (s = eta > 0); the knot family throws. The cutoff
//  weighting mirrors the radial_* functions: value = S * fc and
//  deriv = fc * dS + fcp * S, the operand order of radial_value_deriv.
// --------------------------------------------------------------------------

/** \brief S_c(r) * f_c(r) for the given bump basis. */
inline double radial_step_value(BasisKind k, double r, double fc,
                                double s, double c) {
  switch (k) {
    case BasisKind::Blip:     return SB (s * (r - c)) * fc;
    case BasisKind::Blip5:    return SB5(s * (r - c)) * fc;
    case BasisKind::Gaussian: return SG (r, s, c) * fc;
    case BasisKind::Knot5:    basis_detail::no_step_form(k);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d/dr [ S_c(r) * f_c(r) ] for the given bump basis (fcp = f_c'). */
inline double radial_step_deriv(BasisKind k, double r, double fc, double fcp,
                                double s, double c) {
  switch (k) {
    case BasisKind::Blip: {
      const double x = s * (r - c);
      return fc * dSB(x, s) + fcp * SB(x);
    }
    case BasisKind::Blip5: {
      const double x = s * (r - c);
      return fc * dSB5(x, s) + fcp * SB5(x);
    }
    case BasisKind::Gaussian:
      return fc * dSG(r, s, c) + fcp * SG(r, s, c);
    case BasisKind::Knot5:    basis_detail::no_step_form(k);
  }
  basis_detail::unknown_basis(k);
}

/** Evaluate a cutoff-weighted step and its derivative together. The Blip5
 *  path is fused (SB5_value_deriv); both results are bitwise equal to
 *  radial_step_value / radial_step_deriv for every family. */
inline void radial_step_value_deriv(BasisKind k, double r, double fc,
                                    double fcp, double s, double c,
                                    double &value, double &deriv) {
  if (k == BasisKind::Blip5) {
    double bare_value, bare_deriv;
    SB5_value_deriv(s * (r - c), s, bare_value, bare_deriv);
    value = bare_value * fc;
    deriv = fc * bare_deriv + fcp * bare_value;
    return;
  }
  value = radial_step_value(k, r, fc, s, c);
  deriv = radial_step_deriv(k, r, fc, fcp, s, c);
}

/** \brief S_c(r) with no cutoff weight (refit fitters, curve editing). */
inline double radial_step_value_bare(BasisKind k, double r, double s,
                                     double c) {
  switch (k) {
    case BasisKind::Blip:     return SB (s * (r - c));
    case BasisKind::Blip5:    return SB5(s * (r - c));
    case BasisKind::Gaussian: return SG (r, s, c);
    case BasisKind::Knot5:    basis_detail::no_step_form(k);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d/dr S_c(r) with no cutoff weight. */
inline double radial_step_deriv_bare(BasisKind k, double r, double s,
                                     double c) {
  switch (k) {
    case BasisKind::Blip:     return dSB (s * (r - c), s);
    case BasisKind::Blip5:    return dSB5(s * (r - c), s);
    case BasisKind::Gaussian: return dSG (r, s, c);
    case BasisKind::Knot5:    basis_detail::no_step_form(k);
  }
  basis_detail::unknown_basis(k);
}

/** \brief d^2/dr^2 S_c(r) with no cutoff weight. */
inline double radial_step_deriv2_bare(BasisKind k, double r, double s,
                                      double c) {
  switch (k) {
    case BasisKind::Blip:     return d2SB (s * (r - c), s);
    case BasisKind::Blip5:    return d2SB5(s * (r - c), s);
    case BasisKind::Gaussian: return d2SG (r, s, c);
    case BasisKind::Knot5:    basis_detail::no_step_form(k);
  }
  basis_detail::unknown_basis(k);
}

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_BASIS_DISPATCH_H
