#ifndef D2_ZBL_H
#define D2_ZBL_H

#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/**
 * \brief ZBL Descriptor
 *
 * The ZBL (Ziegler-Biersack-Littmark) potential is an empirical potential used to model short-range interactions between atoms.
 *
 * The constant term \f$ \frac{e^2}{4 \pi \varepsilon_0 } \f$ is set to 1 and will be fitted as needed.
 *
 * The simplified expression for the ZBL potential is given by:
 *
 * \f[
 * V(r) = \frac{Z_1 Z_2}{r} \phi\left(\frac{r}{a}\right)
 * \f]
 *
 * where \f$ a \f$ is the screening length, expressed as:
 * \f[
 * a = \frac{s_0 a_0}{Z_1^{p_0} + Z_2^{p_1}}
 * \f]
 *
 * Here, \f$ a_0 \f$, \f$ s_0 \f$, \f$ p_0 \f$, and \f$ p_1 \f$ are adjustable hyperparameters.
 * Setting any of these to -1 uses the default values:
 *
 * - \f$ a_0 = 0.52917721067 \, \text{Å} \f$
 * - \f$ s_0 = 0.88534 \f$
 * - \f$ p_0 = 0.23 \f$
 * - \f$ p_1 = 0.23 \f$
 *
 * The screening function \f$ \phi \f$ is defined as:
 * \f[
 * \phi(x) = 0.1818 e^{-3.2x} + 0.5099 e^{-0.9423x} + 0.2802 e^{-0.4029x} + 0.02817 e^{-0.2016x}
 * \f]
 *
 * Required tadah::core::Context Key: \ref INIT2B \ref TYPE2B
 *
 * - TYPE2B D2_ZBL \f$ a_0 \f$ \f$ s_0 \f$ \f$ p_0 \f$ \f$ p_1 \f$ ELEMENT1 ELEMENT2
 *
 * Examples:
 *
 * - TYPE2B D2_ZBL 0.53 0.90 0.23 0.23 Kr Kr  # Custom parameters
 * - TYPE2B D2_ZBL -1 -1 -1 -1 Kr Kr  # Default values
 * - TYPE2B D2_ZBL 0.53 -1 -1 -1 Kr Kr  # Mix of default and custom
 *
 */
class D2_ZBL : public D2_Base {
private:
  std::string lab="D2_ZBL";
  double phi(const double x) const; // x = r/a
  // dphi/dx
  double dphi(const double x) const; // x = r/a
  // d^2 phi / dx^2 (needed by calc_dXijdri_dtheta force chain rule)
  double d2phi(const double x) const;
  double screening_length(int, int) const; // a =
  double a0=0.52917721067; // bohr radius
  double s0=0.88534; // bohr radius
  double p0=0.23; // bohr radius
  double p1=0.23; // bohr radius
  double **a = nullptr; // precomputed screening length
  int types[119]; // local index of Zi
  int ntypes=0; // number of elements for this calc
public:
  D2_ZBL();
  D2_ZBL(const tadah::core::Context &context);
  ~D2_ZBL();
  void calc_aed(
    const int Zi,
    const int Zj,
    const double rij,
    const double ,
    tadah::core::aed_type &aed,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_all(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    tadah::core::aed_type &aed,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;

  /** Closed-form analytical derivatives of D2_ZBL w.r.t. its
   *  hyperparameters. Exposed via meta(): RCUT2B (real-space cutoff)
   *  plus the four TYPE2B-positional screening params (a0, s0, p0, p1).
   *
   *  D2_ZBL contributes one component per pair:
   *    aed(fidx) = +w(Zj) (Zi Zj / r) phi(r / a) fc(r)
   *  with screening length a = s0 a0 / (Zi^p0 + Zj^p1) and Moliere phi.
   *
   *  Closed forms:
   *    dD/d rcut = w(Zj) (Zi Zj / r) phi(r/a) dfc/d rcut
   *    dD/d theta = w(Zj) (Zi Zj / r) dphi(r/a) dx/d theta fc(r)
   *      where x = r/a, dx/d theta = -(x/a) da/d theta, and:
   *        da/da0 = a / a0
   *        da/ds0 = a / s0
   *        da/dp0 = -a Zi^p0 ln(Zi) / (Zi^p0 + Zj^p1)
   *        da/dp1 = -a Zj^p1 ln(Zj) / (Zi^p0 + Zj^p1)
   *
   *  arg_pos convention (ParamMeta::arg_pos): the 1-based position of the
   *  value on the descriptor's TYPE2B line, whose first value is the name
   *  D2_ZBL:
   *    arg_pos == 2 -> a0  (config index 1)
   *    arg_pos == 3 -> s0  (config index 2)
   *    arg_pos == 4 -> p0  (config index 3)
   *    arg_pos == 5 -> p1  (config index 4)
   */
  bool calc_dDdtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      tadah::core::aed_type& dD_buf,
      const double       scale = 1) const override;

  /** Closed-form derivative of the D2_ZBL force-descriptor radial
   *  component (column 0 of fd_ij) w.r.t. its hyperparameters.
   *
   *    fd_radial = w(Zj) * [E_ZBL(r) * f_c'(r) + dE_ZBL/dr * f_c(r)]
   *
   *  with E_ZBL = Zi Zj / r * phi(r/a). Only `a(a0, s0, p0, p1)` and
   *  fc, fcp depend on theta. For theta ∈ {a0, s0, p0, p1}:
   *
   *    d/d theta (fd_radial) = -w(Zj) (Zi Zj / a²) * da/d theta
   *                            * [dphi(x) f_c'(r) + d²phi(x) f_c(r) / a]
   *
   *  (Derivation: ∂E/∂θ = -(Zi Zj / a²) dphi · da/dθ;
   *               ∂(dE/dr)/∂θ = -(Zi Zj · x · d²phi / (r · a²)) · da/dθ;
   *               x/r = 1/a → factor simplifies as shown.)
   *
   *  For theta = RCUT2B only fc and fcp depend on theta:
   *    d/d rcut = w(Zj) * [E_ZBL · dfcp/d rcut + dE_ZBL/dr · dfc/d rcut].
   *
   *  Z=1 (H) corner case: ln(1)=0 makes da/dp0 = 0 for Zi=1 (and
   *  da/dp1 = 0 for Zj=1). Closed form returns true and contributes 0,
   *  matching the FD reference. */
  bool calc_dXijdri_dtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      tadah::core::fd_type& dfd_ij,
      const double       scale = 1) const override;

  std::string label() const override;
  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  /** D2_Base::depends_on(), and the family parameters a0, s0, p0 and p1 of
   *  the TYPE2B line: TYPE2B positions 2 to 5 (the label is 1), as
   *  calc_dDdtheta takes them. */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    return (theta_name == "TYPE2B" && arg_pos >= 2 && arg_pos <= 5) ||
           D2_Base::depends_on(theta_name, arg_pos);
  }
};
} // namespace models
} // namespace tadah
#endif
