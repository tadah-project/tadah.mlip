#ifndef DM_RADIAL_POLICIES_H
#define DM_RADIAL_POLICIES_H

/**
 * \file radial_policies.h
 * \brief Static-method radial-basis policies for DM_Poly.
 *
 * Each policy exposes two static methods:
 * \code
 *   static void eval(r, fc, sg, cg, g, n);          // g[i]  = B(r, sg[i], cg[i]) * fc
 *   static void eval(r, fc, fcp, sg, cg, g, dg, n); // + dg[i] = dB/dr
 * \endcode
 *
 * Policies are selected at compile time as a template parameter to DM_Poly,
 * so all calls inline with zero virtual overhead.
 *
 * Available policies:
 * - \ref tadah::models::BlipRadial    — cubic B-spline (blip) basis
 * - \ref tadah::models::GaussianRadial — Gaussian \f$ e^{-\eta(r-r_s)^2} \f$ basis
 */

#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <cstddef>
#include <vector>

namespace tadah {
namespace models {

/* =========================================================================
 *  BlipRadial
 *
 *  Radial basis: B(η·(r−r_s)) · f_c(r)
 *  where B is the cubic B-spline (blip) function with compact support |x|<2.
 * ========================================================================= */

/**
 * \brief Blip (cubic B-spline) radial basis policy.
 *
 * Evaluates \f$ g_c = \mathcal{B}\!\bigl(\eta_c(r-r_{s,c})\bigr)\,f_c(r) \f$
 * and its derivative w.r.t.\ \f$ r \f$.
 *
 * Each grid point \f$ c \f$ has independent \f$ \eta_c \f$ (sgrid) and
 * \f$ r_{s,c} \f$ (cgrid).  The full support in \f$ r \f$ for grid point
 * \f$ c \f$ is \f$ 4/\eta_c \f$.
 */
struct BlipRadial {
  /** Shared basis-family tag (lets DM_Poly report its radial family). */
  static constexpr BasisKind kind = BasisKind::Blip;
  /** \brief Evaluate \f$ g_c = \mathcal{B}(\eta_c(r-r_{s,c}))\,f_c \f$. */
  static void eval(
      double r, double fc,
      const std::vector<double>& sg,
      const std::vector<double>& cg,
      double* g, std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i) {
      const double x = sg[i]*(r - cg[i]);
      g[i] = B(x, fc);
    }
  }

  /**
   * \brief Evaluate \f$ g_c \f$ and \f$ \partial g_c/\partial r \f$.
   *
   * \param fcp cutoff derivative \f$ f_c'(r) \f$
   */
  static void eval(
      double r, double fc, double fcp,
      const std::vector<double>& sg,
      const std::vector<double>& cg,
      double* g, double* dg, std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i) {
      const double s = sg[i];
      const double x = s*(r - cg[i]);
      g [i] = B (x, fc);
      dg[i] = dB(x, s, fc, fcp);
    }
  }
};

/* =========================================================================
 *  GaussianRadial
 *
 *  Radial basis: exp(-η·(r−r_s)²) · f_c(r)
 *  Parameters: sgrid = η (width), cgrid = r_s (centre).
 * ========================================================================= */

/**
 * \brief Gaussian radial basis policy.
 *
 * Evaluates \f$ g_c = e^{-\eta_c(r-r_{s,c})^2}\,f_c(r) \f$ and its
 * derivative w.r.t.\ \f$ r \f$.
 *
 * This is the same radial basis used by DM_EAD and DM_mEAD.
 */
struct GaussianRadial {
  /** Shared basis-family tag (lets DM_Poly report its radial family). */
  static constexpr BasisKind kind = BasisKind::Gaussian;
  /** \brief Evaluate \f$ g_c = e^{-\eta_c(r-r_{s,c})^2}\,f_c \f$. */
  static void eval(
      double r, double fc,
      const std::vector<double>& sg,
      const std::vector<double>& cg,
      double* g, std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i)
      g[i] = G(r, sg[i], cg[i], fc);
  }

  /**
   * \brief Evaluate \f$ g_c \f$ and \f$ \partial g_c/\partial r \f$.
   *
   * \param fcp cutoff derivative \f$ f_c'(r) \f$
   */
  static void eval(
      double r, double fc, double fcp,
      const std::vector<double>& sg,
      const std::vector<double>& cg,
      double* g, double* dg, std::size_t n)
  {
    for (std::size_t i = 0; i < n; ++i) {
      g [i] = G (r, sg[i], cg[i], fc);
      dg[i] = dG(r, sg[i], cg[i], fc, fcp);
    }
  }
};

} // namespace models
} // namespace tadah

#endif // DM_RADIAL_POLICIES_H
