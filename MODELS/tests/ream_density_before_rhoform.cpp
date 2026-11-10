// The DM_REAM density before RHOFORM, copied from MODELS 4c4144b (see
// ream_density_before_rhoform.h). Every expression is as it was there; the
// C library's cos, sin and exp are called through Libm. Do not edit.
#include "ream_density_before_rhoform.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace ream_before_rhoform {

namespace {

// ---- descriptors/d_basis_functions.h ----------------------------------

double G(const Libm &lm, double r, double eta, double miu, double f) {
  return lm.exp(-eta*(r-miu)*(r-miu))*f;
}

double dG(const Libm &lm, double r, double eta, double miu, double f,
          double fp) {
  return lm.exp(-eta*(r-miu)*(r-miu) )*(-2.0*f*eta*(r-miu)+fp);
}

double B(double x_c, double f)
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

double dB(double x_c, double eta, double f, double fp)
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

double B5(double x_c) {
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

double B5(double x_c, double f) { return B5(x_c) * f; }

double dB5(double x_c, double eta) {
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
  return -5.0 * eta * s * acc / 66.0;
}

double dB5(double x_c, double eta, double f, double fp) {
  return f * dB5(x_c, eta) + fp * B5(x_c);
}

void B5_value_deriv(double x_c, double eta,
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

double K5L(double x_c) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  const double t2 = t * t;
  return t2 * t2 * t;
}

double K5L(double x_c, double f) { return K5L(x_c) * f; }

double dK5L(double x_c, double eta) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  const double t2 = t * t;
  return -5.0 * eta * t2 * t2;
}

double dK5L(double x_c, double eta, double f, double fp) {
  return f * dK5L(x_c, eta) + fp * K5L(x_c);
}

double K3L(double x_c) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  return t * t * t;
}

double K3L(double x_c, double f) { return K3L(x_c) * f; }

double dK3L(double x_c, double eta) {
  const double t = -x_c;
  if (t <= 0.0) return 0.0;
  return -3.0 * eta * t * t;
}

double dK3L(double x_c, double eta, double f, double fp) {
  return f * dK3L(x_c, eta) + fp * K3L(x_c);
}

// ---- descriptors/basis_dispatch.h (BasisKind by its TYPEMB code) -------

enum Basis { Blip = 0, Gaussian = 1, Knot5 = 2, Blip5 = 3 };

Basis basis_of(int bcode) {
  switch (bcode) {
    case 0: return Blip;
    case 1: return Gaussian;
    case 2: return Knot5;
    case 3: return Blip5;
  }
  throw std::invalid_argument("ream_before_rhoform: unknown basis code");
}

double radial_value(const Libm &lm, Basis k, double r, double fc,
                    double s, double c, int deg = 5) {
  switch (k) {
    case Blip:     return B (s * (r - c), fc);
    case Blip5:    return B5(s * (r - c), fc);
    case Knot5:    return deg == 3 ? K3L(r - c, fc) : K5L(r - c, fc);
    case Gaussian: return G (lm, r, s, c, fc);
  }
  throw std::invalid_argument("ream_before_rhoform: unknown basis");
}

double radial_deriv(const Libm &lm, Basis k, double r, double fc,
                    double fcp, double s, double c, int deg = 5) {
  switch (k) {
    case Blip:     return dB (s * (r - c), s, fc, fcp);
    case Blip5:    return dB5(s * (r - c), s, fc, fcp);
    case Knot5:    return deg == 3 ? dK3L(r - c, 1.0, fc, fcp)
                                   : dK5L(r - c, 1.0, fc, fcp);
    case Gaussian: return dG (lm, r, s, c, fc, fcp);
  }
  throw std::invalid_argument("ream_before_rhoform: unknown basis");
}

void radial_value_deriv(const Libm &lm, Basis k, double r, double fc,
                        double fcp, double s, double c,
                        double &value, double &deriv, int deg = 5) {
  if (k == Blip5) {
    double bare_value, bare_deriv;
    B5_value_deriv(s * (r - c), s, bare_value, bare_deriv);
    value = bare_value * fc;
    deriv = fc * bare_deriv + fcp * bare_value;
    return;
  }
  value = radial_value(lm, k, r, fc, s, c, deg);
  deriv = radial_deriv(lm, k, r, fc, fcp, s, c, deg);
}

// ---- src/cutoffs.cpp ----------------------------------------------------

enum CutKind { Dummy, Cos, Poly };

struct Cutoff {
  CutKind kind;
  double rcut;
  double rcut_inv;

  Cutoff(const std::string &name, double r) {
    if (name == "Cut_Dummy") kind = Dummy;
    else if (name == "Cut_Cos") kind = Cos;
    else if (name == "Cut_Poly") kind = Poly;
    else throw std::invalid_argument("ream_before_rhoform: cutoff " + name);
    rcut = r;
    rcut_inv = r<=0 ? 0.0 : 1.0/r;
  }

  double calc(const Libm &lm, double r) const {
    switch (kind) {
      case Dummy:
        if (r>=rcut) return 0.0;
        return 1.0;
      case Cos:
        if (r>=rcut) return 0.0;
        return 0.5 * (lm.cos(M_PI*r*rcut_inv) + 1.0);
      case Poly:
        if (r >= rcut) {
          return 0.0;
        } else {
          double x = r * rcut_inv;
          double x3 = x * x * x;
          double x4 = x3 * x;
          double x5 = x4 * x;
          return 1.0 - 10.0 * x3 + 15.0 * x4 - 6.0 * x5;
        }
    }
    throw std::invalid_argument("ream_before_rhoform: unknown cutoff");
  }

  double calc_prime(const Libm &lm, double r) const {
    switch (kind) {
      case Dummy:
        return 0.0;
      case Cos: {
        if (r>=rcut) return 0.0;
        double t = M_PI*rcut_inv;
        return -0.5 * t*lm.sin(t*r);
      }
      case Poly:
        if (r >= rcut) {
          return 0.0;
        } else {
          double x = r * rcut_inv;
          double x2 = x * x;
          double x3 = x2 * x;
          double x4 = x3 * x;
          return (-30.0 * x2 + 60.0 * x3 - 30.0 * x4) * rcut_inv;
        }
    }
    throw std::invalid_argument("ream_before_rhoform: unknown cutoff");
  }
};

// ---- src/dm_ream.cpp (DM_REAM before RHOFORM) --------------------------

struct Ream {
  const Libm &lm;
  Basis basis;
  const Density &d;

  int rho_deg(std::size_t n) const {
    return n < d.kdeg.size() ? static_cast<int>(d.kdeg[n]) : 5;
  }
  double psi(std::size_t n, double r, double fc) const {
    const double s = (n < d.s.size()) ? d.s[n] : 1.0;
    return radial_value(lm, basis, r, fc, s, d.c[n], rho_deg(n));
  }
  double dpsi(std::size_t n, double r, double fc, double fcp) const {
    const double s = (n < d.s.size()) ? d.s[n] : 1.0;
    return radial_deriv(lm, basis, r, fc, fcp, s, d.c[n], rho_deg(n));
  }
  double rho_raw(double r, double fc) const {
    double acc = 0.0;
    for (std::size_t n = 0; n < d.c.size(); ++n) acc += d.a[n] * psi(n, r, fc);
    return acc;
  }
  double rho_raw_deriv(double r, double fc, double fcp) const {
    double acc = 0.0;
    for (std::size_t n = 0; n < d.c.size(); ++n)
      acc += d.a[n] * dpsi(n, r, fc, fcp);
    return acc;
  }
};

}  // namespace

Libm platform_libm() {
  return Libm{static_cast<double (*)(double)>(std::cos),
              static_cast<double (*)(double)>(std::sin),
              static_cast<double (*)(double)>(std::exp)};
}

Values evaluate(const Density &d, double rij, const Libm &lm) {
  const Ream ream{lm, basis_of(d.bcode), d};
  const Cutoff fcut(d.cutoff, d.rcut);
  const double scale = 1.0;
  const double w = d.w;
  Values out{0.0, 0.0, 0.0, 0.0};
  if (rij > d.rcut) return out;

  // calc_rho, into a zeroed rho
  {
    const double fc = fcut.calc(lm, rij);
    double rho = 0.0;
    rho += scale * w * ream.rho_raw(rij, fc);
    out.rho = rho;
  }

  // calc_dXijdri: fd(fidx, 0) with rhoi(drfidx) = 1 and vec_ij[0] = 1
  {
    const double fc  = fcut.calc(lm, rij);
    const double fcp = fcut.calc_prime(lm, rij);
    const double rpr = ream.rho_raw_deriv(rij, fc, fcp);
    const double rinv = 1.0 / rij;
    const double pref = scale * w * rpr * rinv;
    const double coef = pref * 1.0;
    out.pref = coef * 1.0;
  }

  // eam_density
  {
    const double fc  = fcut.calc(lm, rij);
    const double fcp = fcut.calc_prime(lm, rij);
    double rho_raw_both = 0.0;
    double rhop_raw = 0.0;
    for (std::size_t n = 0; n < d.c.size(); ++n) {
      const double s = (n < d.s.size()) ? d.s[n] : 1.0;
      double psi_value, psi_deriv;
      radial_value_deriv(lm, ream.basis, rij, fc, fcp, s, d.c[n],
                         psi_value, psi_deriv, ream.rho_deg(n));
      rho_raw_both += d.a[n] * psi_value;
      rhop_raw += d.a[n] * psi_deriv;
    }
    out.eam_rho  = w * rho_raw_both;
    out.eam_rhop = w * rhop_raw;
  }
  return out;
}

}  // namespace ream_before_rhoform
