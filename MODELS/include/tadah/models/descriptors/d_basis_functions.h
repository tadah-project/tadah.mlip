#ifndef D_BASIS_F_H
#define D_BASIS_F_H

#include <math.h>

namespace tadah {
namespace models {

/** \defgroup BasisFunctions Radial Basis Functions
 * \brief Inline Gaussian and Blip radial basis functions and their derivatives.
 *
 * This header provides two families of radial basis functions used to expand
 * atomic densities and two-body descriptors:
 *
 * - **Gaussian** functions \f$ G(r;\eta,\mu) \f$
 * - **Blip** (B-spline) functions \f$ \mathcal{B}(x_c) \f$
 *
 * Both families are available in plain and cutoff-weighted overloads.
 * @{
 */

// =========================================================================
// GAUSSIANS
// =========================================================================

/** \brief Gaussian basis function.
 *
 * \f[
 *   G(r;\,\eta,\mu) = e^{-\eta(r-\mu)^2}
 * \f]
 *
 * \param r   interatomic distance \f$ r_{ij} \f$
 * \param eta width parameter \f$ \eta > 0 \f$
 * \param miu centre \f$ \mu \f$ (often written \f$ r_s \f$)
 */
inline double G(double r, double eta, double miu) {
  return exp(-eta*(r-miu)*(r-miu));
}

/** \brief Gaussian basis function weighted by a cutoff.
 *
 * \f[
 *   G(r;\,\eta,\mu)\,f_c(r) = e^{-\eta(r-\mu)^2}\,f_c(r)
 * \f]
 *
 * \param r   interatomic distance
 * \param eta width parameter \f$ \eta \f$
 * \param miu centre \f$ \mu \f$
 * \param f   cutoff value \f$ f_c(r) \f$
 */
inline double G(double r, double eta, double miu, double f) {
  return exp(-eta*(r-miu)*(r-miu))*f;
}

/** \brief Derivative of the Gaussian basis function w.r.t.\ \f$ r \f$.
 *
 * \f[
 *   \frac{d G}{d r} = -2\eta(r-\mu)\,e^{-\eta(r-\mu)^2}
 * \f]
 *
 * \param r   interatomic distance
 * \param eta width parameter \f$ \eta \f$
 * \param miu centre \f$ \mu \f$
 */
inline double dG(double r, double eta, double miu) {
  return -2.0*eta*(r-miu)*exp(-eta*(r-miu)*(r-miu));
}

/** \brief Derivative of the cutoff-weighted Gaussian w.r.t.\ \f$ r \f$.
 *
 * Product rule applied to \f$ G(r)\,f_c(r) \f$:
 * \f[
 *   \frac{d}{dr}\bigl[G(r;\eta,\mu)\,f_c(r)\bigr]
 *   = e^{-\eta(r-\mu)^2}\!\left[-2\eta(r-\mu)\,f_c(r)+f_c'(r)\right]
 * \f]
 *
 * \param r   interatomic distance
 * \param eta width parameter \f$ \eta \f$
 * \param miu centre \f$ \mu \f$
 * \param f   cutoff value \f$ f_c(r) \f$
 * \param fp  cutoff derivative \f$ f_c'(r) \f$
 */
inline double dG(double r, double eta, double miu, double f, double fp) {
  return exp(-eta*(r-miu)*(r-miu) )*(-2.0*f*eta*(r-miu)+fp);
}

/** \brief Second derivative of the Gaussian basis function w.r.t.\ \f$ r \f$.
 *
 * \f[
 *   \frac{d^2 G}{d r^2} = \left(4\eta^2(r-\mu)^2 - 2\eta\right)
 *                         e^{-\eta(r-\mu)^2}
 * \f]
 *
 * Unlike d2B/d2B5/d2K5L (bare \f$ d^2/dx_c^2 \f$, chain factor applied by
 * the caller), the Gaussian primitives take \f$ (r,\eta,\mu) \f$ directly,
 * so the derivative here is already w.r.t.\ \f$ r \f$ — mirroring dG.
 *
 * \param r   interatomic distance
 * \param eta width parameter \f$ \eta \f$
 * \param miu centre \f$ \mu \f$
 */
inline double d2G(double r, double eta, double miu) {
  const double d = r - miu;
  return (4.0*eta*eta*d*d - 2.0*eta) * exp(-eta*d*d);
}

// =========================================================================
// BLIPS
// =========================================================================

/** \brief Blip (cubic B-spline) basis function.
 *
 * The blip function is a piecewise cubic polynomial with compact support on
 * \f$(-2,\,2)\f$ in the normalised coordinate \f$ x_c = \eta(r_{ij}-r_s) \f$:
 *
 * \f[
 *   \mathcal{B}(x_c) =
 *   \begin{cases}
 *     \dfrac{1}{4}(2+x_c)^3       & -2 < x_c \le -1 \\[6pt]
 *     1 - \dfrac{3}{2}x_c^2 + \dfrac{3}{4}|x_c|^3 & -1 < x_c \le 1 \\[6pt]
 *     \dfrac{1}{4}(2-x_c)^3       & 1 < x_c < 2 \\[4pt]
 *     0                           & \text{otherwise}
 *   \end{cases}
 * \f]
 *
 * The function is \f$ C^2 \f$ everywhere: it is the classic cubic B-spline
 * (rescaled to unit peak), so \f$ \mathcal{B} \f$, \f$ \mathcal{B}' \f$ and
 * \f$ \mathcal{B}'' \f$ are all continuous at every knot
 * \f$ x_c \in \{0,\pm1,\pm2\} \f$; only \f$ \mathcal{B}''' \f$ jumps there
 * (by \f$ \{+9,-6,+1.5\} \f$ at \f$ \{0,\pm1,\pm2\} \f$ respectively).
 *
 * \param x_c normalised coordinate \f$ x_c = \eta(r_{ij}-r_s) \f$
 *
 * \sa Hernández, E., Gillan, M., Goringe, C. (1997).
 *     Basis functions for linear-scaling first-principles calculations.
 *     *Phys. Rev. B*, **55**(20), 13485–13493.
 *     https://doi.org/10.1103/PhysRevB.55.13485
 */
inline double B(double x_c)
{
  if (-2.0 < x_c && x_c <= -1.0) {
    double b = 2.0 + x_c;
    return 0.25 * b*b*b;
  }
  else if (-1.0 < x_c && x_c <= 1.0) {
    double x2 = x_c*x_c;
    double x3 = x2*x_c;
    return 1.0 - 1.5*x2 + 0.75*std::fabs(x3);
  }
  else if (1.0 < x_c && x_c < 2.0) {
    double b = 2.0 - x_c;
    return 0.25 * b*b*b;
  }
  else {
    return 0.0;
  }
}

/** \brief Blip basis function evaluated at \f$ x_c = \eta(r-r_s) \f$.
 *
 * Convenience wrapper: computes \f$ x_c \f$ from physical coordinates and
 * delegates to B(double).
 *
 * \param r   interatomic distance
 * \param eta inverse width \f$ \eta \f$; full support in \f$ r \f$ is \f$ 4/\eta \f$
 * \param miu centre \f$ r_s \f$
 */
inline double B(double r, double eta, double miu) {
  return B(eta*(r-miu));
}

/** \brief Cutoff-weighted blip basis function.
 *
 * \f[
 *   \mathcal{B}(x_c)\,f_c(r)
 * \f]
 *
 * \param x_c normalised coordinate \f$ x_c = \eta(r_{ij}-r_s) \f$
 * \param f   cutoff value \f$ f_c(r) \f$
 */
inline double B(double x_c, double f)
{
  if (-2.0 < x_c && x_c <= -1.0) {
    double b = 2.0 + x_c;
    return 0.25 * b*b*b *f;
  }
  else if (-1.0 < x_c && x_c <= 1.0) {
    double x2 = x_c*x_c;
    double x3 = x2*x_c;
    return f*(1.0 - 1.5*x2 + 0.75*std::fabs(x3));
  }
  else if (1.0 < x_c && x_c < 2.0) {
    double b = 2.0 - x_c;
    return f*(0.25 * b*b*b);
  }
  else {
    return 0.0;
  }
}

/** \brief Derivative of the blip basis function w.r.t.\ \f$ r \f$.
 *
 * Given \f$ x_c = \eta(r-r_s) \f$, by the chain rule:
 * \f[
 *   \frac{d\mathcal{B}}{dr} = \eta\,\frac{d\mathcal{B}}{dx_c}
 * \f]
 * where
 * \f[
 *   \frac{d\mathcal{B}}{dx_c} =
 *   \begin{cases}
 *     \dfrac{3}{4}(2+x_c)^2             & -2 < x_c \le -1 \\[6pt]
 *     -3x_c + \dfrac{9}{4}\,x_c|x_c|   & -1 < x_c \le 1  \\[6pt]
 *     -\dfrac{3}{4}(2-x_c)^2            & 1 < x_c < 2     \\[4pt]
 *     0                                  & \text{otherwise}
 *   \end{cases}
 * \f]
 *
 * \param x_c normalised coordinate \f$ x_c = \eta(r-r_s) \f$
 * \param eta inverse width \f$ \eta \f$
 */
inline double dB(double x_c, double eta)
{
  if (-2.0 < x_c && x_c <= -1.0) {
    double d = 2.0+x_c;
    return 0.75*eta*d*d;
  }
  else if (-1.0 < x_c && x_c <= 1.0) {
    return -3.0*eta*x_c + 2.25*eta*x_c*std::fabs(x_c);
  }
  else if (1.0 < x_c && x_c < 2.0) {
    double d = 2.0-x_c;
    return -0.75*eta*d*d;
  }
  else {
    return 0.0;
  }
}

/** \brief Derivative of the cutoff-weighted blip function w.r.t.\ \f$ r \f$.
 *
 * Product rule applied to \f$ f_c(r)\,\mathcal{B}(x_c) \f$:
 * \f[
 *   \frac{d}{dr}\bigl[f_c(r)\,\mathcal{B}(x_c)\bigr]
 *   = f_c'(r)\,\mathcal{B}(x_c) + f_c(r)\,\frac{d\mathcal{B}}{dr}
 * \f]
 *
 * \param x_c normalised coordinate \f$ x_c = \eta(r-r_s) \f$
 * \param eta inverse width \f$ \eta \f$
 * \param f   cutoff value \f$ f_c(r) \f$
 * \param fp  cutoff derivative \f$ f_c'(r) \f$
 */
inline double dB(double x_c, double eta, double f, double fp)
{
  if (-2.0 < x_c && x_c <= -1.0) {
    double d = 2.0+x_c;
    return f*(0.75*eta*d*d)+fp*(0.25 * d*d*d);
  }
  else if (-1.0 < x_c && x_c <= 1.0) {
    return f*(-3.0*eta*x_c + 2.25*eta*x_c*std::fabs(x_c))
      +fp*(1.0 - 1.5*x_c*x_c + 0.75*std::fabs(x_c*x_c*x_c));
  }
  else if (1.0 < x_c && x_c < 2.0) {
    double d = 2.0-x_c;
    return f*(-0.75*eta*d*d)+fp*(0.25 * d*d*d);
  }
  else {
    return 0.0;
  }
}

/** \brief Derivative of the blip function evaluated at physical coordinates.
 *
 * Convenience wrapper: computes \f$ x_c = \eta(r-r_s) \f$ and calls
 * dB(double, double).
 *
 * \param r   interatomic distance
 * \param eta inverse width \f$ \eta \f$
 * \param miu centre \f$ r_s \f$
 */
inline double dB(double r, double eta, double miu) {
  return dB(eta*(r-miu), eta);
}

/** \brief Second derivative of the blip basis function w.r.t.\ \f$ x_c \f$.
 *
 * Piecewise LINEAR and CONTINUOUS everywhere (the blip is the cubic
 * B-spline, C^2 globally: both pieces give 1.5 at |x_c|=1 and 0 at
 * |x_c|=2; only B''' jumps at the knots). Used by HPO analytical-gradient
 * code paths that need the F''(rho) chain through Blip embeddings.
 *
 * \param x_c normalised coordinate \f$ x_c = \eta(r-r_s) \f$
 */
inline double d2B(double x_c) {
  if (-2.0 < x_c && x_c <= -1.0) return 1.5 * (2.0 + x_c);
  if (-1.0 < x_c && x_c <= 1.0)  return -3.0 + 4.5 * std::fabs(x_c);
  if (1.0 < x_c && x_c < 2.0)    return 1.5 * (2.0 - x_c);
  return 0.0;
}

// =========================================================================
// QUINTIC B-SPLINE BUMP (M6)
// =========================================================================
//
// The quintic B-spline bump (Schoenberg M6, rescaled to unit peak): support
// [-3, 3] in x_c = eta*(r - r_s), C^4 globally (only the 5th derivative
// jumps, at the knots x_c in {0, +-1, +-2, +-3}). The C^4 twin of the cubic
// blip B: same symmetric-bump semantics (free centre mu, free width eta),
// one order better second-derivative fidelity (O(h^4) vs O(h^2) sawtooth).

/** \brief Quintic B-spline bump \f$ B_5(x_c) \f$ (peak-normalised M6). */
inline double B5(double x_c) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) return 0.0;
  const double t3 = 3.0 - a;
  double acc = t3 * t3 * t3 * t3 * t3;
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    acc -= 6.0 * t2 * t2 * t2 * t2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    acc += 15.0 * t1 * t1 * t1 * t1 * t1;
  }
  return acc / 66.0;
}

/** \brief Cutoff-weighted quintic B-spline bump. */
inline double B5(double x_c, double f) { return B5(x_c) * f; }

/** \brief Derivative of B5 w.r.t.\ \f$ r \f$ (returns \f$ \eta\,B_5'(x_c) \f$). */
inline double dB5(double x_c, double eta) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) return 0.0;
  const double s = (x_c >= 0.0) ? 1.0 : -1.0;
  const double t3 = 3.0 - a;
  double acc = t3 * t3 * t3 * t3;
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    acc -= 6.0 * t2 * t2 * t2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    acc += 15.0 * t1 * t1 * t1 * t1;
  }
  return -5.0 * eta * s * acc / 66.0;  // == 0 at x_c = 0 (bracket vanishes)
}

/** \brief Cutoff-weighted derivative of B5 w.r.t.\ \f$ r \f$ (product rule). */
inline double dB5(double x_c, double eta, double f, double fp) {
  return f * dB5(x_c, eta) + fp * B5(x_c);
}

/** Evaluate B5 and its radial derivative together.
 *
 * Algebraically identical to separate B5(x_c) and dB5(x_c, eta) calls,
 * while sharing the support tests and polynomial powers. */
inline void B5_value_deriv(double x_c, double eta,
                           double& value, double& deriv) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) {
    value = 0.0;
    deriv = 0.0;
    return;
  }
  const double s = (x_c >= 0.0) ? 1.0 : -1.0;

  const double t3 = 3.0 - a;
  double acc_v = t3 * t3 * t3 * t3 * t3;
  double acc_d = t3 * t3 * t3 * t3;
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    acc_v -= 6.0 * t2 * t2 * t2 * t2 * t2;
    acc_d -= 6.0 * t2 * t2 * t2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    acc_v += 15.0 * t1 * t1 * t1 * t1 * t1;
    acc_d += 15.0 * t1 * t1 * t1 * t1;
  }
  value = acc_v / 66.0;
  deriv = -5.0 * eta * s * acc_d / 66.0;
}

/** \brief Bare second derivative \f$ d^2 B_5 / dx_c^2 \f$ (continuous,
 *  piecewise cubic — the C^4 family). */
inline double d2B5(double x_c) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) return 0.0;
  const double t3 = 3.0 - a;
  double acc = t3 * t3 * t3;
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    acc -= 6.0 * t2 * t2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    acc += 15.0 * t1 * t1 * t1;
  }
  return 20.0 * acc / 66.0;
}

// =========================================================================
// INTEGRATED-BUMP STEP FUNCTIONS (DM_REAM RHOFORM MONOTONE)
// =========================================================================
//
// S(r) = (integral of the bump from r outwards) / (its integral over the
// whole support), with x_c = eta*(r - mu), eta > 0. So S = 1 inside the inner
// support edge, S(mu) = 1/2 exactly, S = 0 beyond the outer edge (the same
// edge as the bump's) and dS/dr = -eta*bump(x_c)/A <= 0, A = the bump area in
// x_c: 1.5 for B, 20/11 for B5, sqrt(pi/eta) for the Gaussian in r.
// Evaluation rules (verified against 60-digit quadrature: tail error <= 1.4
// ulp relative, plateau error <= 5.4e-17 absolute):
//   - truncated-power products, never monomial expansions, with the
//     left-to-right product order of B/B5;
//   - one fixed dS/dr expression per family, shared by every derivative path;
//   - SB5_value_deriv reuses B5_value_deriv's products, so its S is bitwise
//     equal to SB5 and its derivative bitwise equal to dSB5.
// The one-sided knot family has no step form (a non-negative knot density is
// already non-increasing with a non-increasing cutoff).

/** \brief Integrated cubic blip step \f$ S_B(x_c) \f$.
 *
 *  With \f$ a = |x_c| \f$ and \f$ Q(a) = (2-a)^4 - 4(1-a)_+^4 \f$:
 *  \f$ S_B = Q/24 \f$ for \f$ x_c \ge 0 \f$ and \f$ 1 - Q/24 \f$ for
 *  \f$ x_c < 0 \f$; exactly 1 for \f$ x_c \le -2 \f$, 0 for \f$ x_c \ge 2 \f$.
 */
inline double SB(double x_c) {
  const double a = std::fabs(x_c);
  if (a >= 2.0) return x_c > 0.0 ? 0.0 : 1.0;
  const double t2 = 2.0 - a;
  double p = t2 * t2 * t2 * t2;
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    p -= 4.0 * t1 * t1 * t1 * t1;
  }
  const double q = p / 24.0;
  return x_c >= 0.0 ? q : 1.0 - q;
}

/** \brief \f$ dS_B/dr = -\eta\,B(x_c)/1.5 \f$ (the one expression every
 *  derivative path uses). */
inline double dSB(double x_c, double eta) {
  return -eta * B(x_c) / 1.5;
}

/** \brief \f$ d^2S_B/dr^2 = -\eta\,dB/dr / 1.5 = -\eta^2 B'(x_c)/1.5 \f$. */
inline double d2SB(double x_c, double eta) {
  return -eta * dB(x_c, eta) / 1.5;
}

/** \brief Integrated quintic bump step \f$ S_{B5}(x_c) \f$.
 *
 *  With \f$ a = |x_c| \f$ and
 *  \f$ P(a) = (3-a)^6 - 6(2-a)_+^6 + 15(1-a)_+^6 \f$:
 *  \f$ S_{B5} = P/720 \f$ for \f$ x_c \ge 0 \f$ and \f$ 1 - P/720 \f$ for
 *  \f$ x_c < 0 \f$; exactly 1 for \f$ x_c \le -3 \f$, 0 for \f$ x_c \ge 3 \f$.
 */
inline double SB5(double x_c) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) return x_c > 0.0 ? 0.0 : 1.0;
  const double t3 = 3.0 - a;
  double p = t3 * t3 * t3 * t3 * t3 * t3;
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    p -= 6.0 * t2 * t2 * t2 * t2 * t2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    p += 15.0 * t1 * t1 * t1 * t1 * t1 * t1;
  }
  const double q = p / 720.0;
  return x_c >= 0.0 ? q : 1.0 - q;
}

/** \brief \f$ dS_{B5}/dr = -\eta\,B_5(x_c)\cdot 11/20 \f$, given the bump
 *  value \p b5 = B5(x_c) (the one expression every derivative path uses). */
inline double dSB5_from_B5(double b5, double eta) {
  return -eta * b5 * 11.0 / 20.0;
}

/** \brief \f$ dS_{B5}/dr = -\eta\,B_5(x_c)\cdot 11/20 \f$. */
inline double dSB5(double x_c, double eta) {
  return dSB5_from_B5(B5(x_c), eta);
}

/** \brief \f$ d^2S_{B5}/dr^2 = -\eta\,dB_5/dr \cdot 11/20 \f$. */
inline double d2SB5(double x_c, double eta) {
  return -eta * dB5(x_c, eta) * 11.0 / 20.0;
}

/** Evaluate SB5 and its radial derivative together.
 *
 *  Starts from B5_value_deriv's products t3^5, 6 t2^5 and 15 t1^5 and
 *  multiplies each by its t, so @p value is bitwise equal to SB5(x_c) and
 *  @p deriv bitwise equal to dSB5(x_c, eta). */
inline void SB5_value_deriv(double x_c, double eta,
                            double &value, double &deriv) {
  const double a = std::fabs(x_c);
  if (a >= 3.0) {
    value = x_c > 0.0 ? 0.0 : 1.0;
    deriv = dSB5_from_B5(0.0, eta);
    return;
  }
  const double t3 = 3.0 - a;
  const double p3 = t3 * t3 * t3 * t3 * t3;
  double acc_b = p3;         // B5's accumulator
  double acc_s = p3 * t3;    // SB5's accumulator
  if (a < 2.0) {
    const double t2 = 2.0 - a;
    const double p2 = 6.0 * t2 * t2 * t2 * t2 * t2;
    acc_b -= p2;
    acc_s -= p2 * t2;
  }
  if (a < 1.0) {
    const double t1 = 1.0 - a;
    const double p1 = 15.0 * t1 * t1 * t1 * t1 * t1;
    acc_b += p1;
    acc_s += p1 * t1;
  }
  const double q = acc_s / 720.0;
  value = x_c >= 0.0 ? q : 1.0 - q;
  deriv = dSB5_from_B5(acc_b / 66.0, eta);
}

/** \brief Integrated Gaussian step
 *  \f$ S_G(r) = \tfrac12\,\mathrm{erfc}(\sqrt{\eta}\,(r-\mu)) \f$.
 *
 *  Written with erfc, never as 1 - erf (which loses every digit in the
 *  tail). The Gaussian has no compact support, so neither has its step. */
inline double SG(double r, double eta, double miu) {
  return 0.5 * erfc(std::sqrt(eta) * (r - miu));
}

/** \brief \f$ dS_G/dr = -\sqrt{\eta/\pi}\,e^{-\eta(r-\mu)^2} \f$. */
inline double dSG(double r, double eta, double miu) {
  const double pi = 3.14159265358979323846264338327950288;
  return -std::sqrt(eta / pi) * exp(-eta * (r - miu) * (r - miu));
}

/** \brief \f$ d^2S_G/dr^2 = 2\eta(r-\mu)\sqrt{\eta/\pi}\,e^{-\eta(r-\mu)^2} \f$. */
inline double d2SG(double r, double eta, double miu) {
  return -2.0 * eta * (r - miu) * dSG(r, eta, miu);
}

// =========================================================================
// ONE-SIDED QUINTIC KNOT FUNCTIONS (truncated powers)
// =========================================================================
//
// K5L(x_c) = max(0, -x_c)^5  with x_c = eta*(r - mu) is the LEFT-active
// quintic knot function: c*K5L = c*eta^5*(mu - r)_+^5, i.e. the classic
// Mendelev/Ackland knotted-polynomial EAM form (active for r < mu, vanishes
// C^4-smoothly at the knot r = mu). With all knots mu <= rcut the pair /
// density functions vanish C^4-smoothly at the cutoff BY CONSTRUCTION.
// K5R is the mirrored RIGHT-active twin (active for x > mu), used for the
// embedding F(rho) where the function must be free at high density instead.
// Both are C^4 globally; only the 5th derivative jumps at the knot.
// NOTE eta is a pure gauge for this family ((c,eta)->(c s^5, eta/s) leaves
// the function invariant); fitters pin eta = 1 and the knot position mu is
// the only shape parameter.

/** \brief Left-active quintic knot function \f$ \max(0,-x_c)^5 \f$. */
inline double K5L(double x_c) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  const double t2 = t * t;
  return t2 * t2 * t;
}

/** \brief Cutoff-weighted left-active quintic knot function. */
inline double K5L(double x_c, double f) { return K5L(x_c) * f; }

/** \brief Derivative of K5L w.r.t.\ \f$ r \f$ (returns \f$ \eta\,K5L'(x_c) \f$). */
inline double dK5L(double x_c, double eta) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  const double t2 = t * t;
  return -5.0 * eta * t2 * t2;
}

/** \brief Cutoff-weighted derivative of K5L w.r.t.\ \f$ r \f$ (product rule). */
inline double dK5L(double x_c, double eta, double f, double fp) {
  return f * dK5L(x_c, eta) + fp * K5L(x_c);
}

/** \brief Bare second derivative \f$ d^2 K5L / dx_c^2 \f$ (continuous: C^4 family). */
inline double d2K5L(double x_c) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  return 20.0 * t * t * t;
}

// Cubic knot functions (t - r)_+^3 — the EXACT atoms of the published
// Mendelev/Ackland knotted-cubic EAM class. C^2 globally (only the 3rd
// derivative jumps at the knot): a cubic atom IS an f''' corner, which the
// C^4 quintic family cannot represent. Used by the KnotMix auto-mode (and
// REFIT_BASIS Knot3) via the per-atom degree arrays KDEG2B / KDEGMB /
// KDEGEMB; same eta-gauge convention as K5*.

/** \brief Left-active cubic knot function \f$ \max(0,-x_c)^3 \f$. */
inline double K3L(double x_c) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  return t * t * t;
}

/** \brief Cutoff-weighted left-active cubic knot function. */
inline double K3L(double x_c, double f) { return K3L(x_c) * f; }

/** \brief Derivative of K3L w.r.t.\ \f$ r \f$ (returns \f$ \eta\,K3L'(x_c) \f$). */
inline double dK3L(double x_c, double eta) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  return -3.0 * eta * t * t;
}

/** \brief Cutoff-weighted derivative of K3L w.r.t.\ \f$ r \f$ (product rule). */
inline double dK3L(double x_c, double eta, double f, double fp) {
  return f * dK3L(x_c, eta) + fp * K3L(x_c);
}

/** \brief Bare second derivative \f$ d^2 K3L / dx_c^2 \f$ (continuous: C^2 family). */
inline double d2K3L(double x_c) {
  const double t = -x_c;
  return (t > 0.0) ? 6.0 * t : 0.0;
}

/** \brief Right-active cubic knot function \f$ \max(0,x_c)^3 \f$. */
inline double K3R(double x_c) { return K3L(-x_c); }

/** \brief Cutoff-weighted right-active cubic knot function. */
inline double K3R(double x_c, double f) { return K3L(-x_c) * f; }

/** \brief Derivative of K3R w.r.t.\ \f$ r \f$ (returns \f$ \eta\,K3R'(x_c) \f$). */
inline double dK3R(double x_c, double eta) { return -dK3L(-x_c, eta); }

/** \brief Cutoff-weighted derivative of K3R w.r.t.\ \f$ r \f$ (product rule). */
inline double dK3R(double x_c, double eta, double f, double fp) {
  return f * dK3R(x_c, eta) + fp * K3R(x_c);
}

/** \brief Bare second derivative \f$ d^2 K3R / dx_c^2 \f$ (continuous: C^2 family). */
inline double d2K3R(double x_c) { return d2K3L(-x_c); }

/** \brief Right-active quintic knot function \f$ \max(0,x_c)^5 \f$. */
inline double K5R(double x_c) { return K5L(-x_c); }

/** \brief Cutoff-weighted right-active quintic knot function. */
inline double K5R(double x_c, double f) { return K5L(-x_c) * f; }

/** \brief Derivative of K5R w.r.t.\ \f$ r \f$ (returns \f$ \eta\,K5R'(x_c) \f$). */
inline double dK5R(double x_c, double eta) { return -dK5L(-x_c, eta); }

/** \brief Cutoff-weighted derivative of K5R w.r.t.\ \f$ r \f$ (product rule). */
inline double dK5R(double x_c, double eta, double f, double fp) {
  return f * dK5R(x_c, eta) + fp * K5R(x_c);
}

/** \brief Bare second derivative \f$ d^2 K5R / dx_c^2 \f$ (continuous: C^4 family). */
inline double d2K5R(double x_c) { return d2K5L(-x_c); }

/** @} */ // end of BasisFunctions group

} // namespace models
} // namespace tadah
#endif
