#ifndef D2_BLIP_H
#define D2_BLIP_H
#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/** \brief Blip two-body descriptor.
 *
 * \f[
 * V_i^{\eta,r_s} =\sum_{j \neq i} \mathcal{B}(\eta(r_{ij}-r_s))f_c(r_{ij})
 * \f]
 *
 * where \f$ f_c \f$ is a cutoff function and \f$ \mathcal{B} \f$ is a blip
 * basis function centered at \f$r_s\f$ of width \f$4/\eta\f$.
 *
 * \ref CGRID2B parameters control position \f$ r_s \f$ of blip centres.
 *
 * \ref SGRID2B parameters control width \f$ \eta \f$ of blips.
 *
 * Blip basis function is built out of 3rd degree polynomials
 * in the four intervals [-2,-1], [-1,0], [0,1], [1,2] and is defined as:
 *
 * \f[
	\begin{equation}
	  \mathcal{B}(r) =
		\begin{cases}
		  1-\frac{3}{2}r^2+\frac{3}{4}|r|^3 & \text{if} \qquad 0<|r|<1\\
		  \frac{1}{4}(2-|r|)^3 & \text{if} \qquad 1<|r|<2\\
		  0 & \text{if} \qquad |r|>2
		\end{cases}
	\end{equation}
 * \f]
 *
 * More details about the blip basis functions can be found in the following paper:
 *
 * <div class="csl-entry">Hernández, E., Gillan, M., Goringe, C.
 * (1997). Basis functions for linear-scaling first-principles calculations.
 * <i>Physical Review B - Condensed Matter and Materials Physics</i>,
 * <i>55</i>(20), 13485–13493. https://doi.org/10.1103/PhysRevB.55.13485</div>
 *
 * Required keys:
 * \ref INIT2B \ref CGRID2B \ref SGRID2B
 */
class D2_Blip : public D2_Base {
private:
  std::string lab="D2_Blip";
  tadah::core::v_type cgrid;
  tadah::core::v_type sgrid;

public:
  D2_Blip();
  D2_Blip(const tadah::core::Context &context);
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

  /** Closed-form analytical derivatives of the Blip D2_Blip descriptor
   *  with respect to its hyperparameters.
   *
   *    D_c(r) = w(Zj) * B(x_c) * fc(r),   x_c = eta_c (r - mu_c)
   *
   *  Chain rule:
   *    dD_c/d mu_c   = w(Zj) * dB/dx_c |_{x_c} * (-eta_c) * fc(r)
   *    dD_c/d eta_c  = w(Zj) * dB/dx_c |_{x_c} * (r - mu_c) * fc(r)
   *    dD_c/d rcut   = w(Zj) * B(x_c) * df_c/d rcut(r)
   *
   *  The blip basis function B and its derivative dB/dx are piecewise
   *  cubics with compact support on (-2, 2); see d_basis_functions.h.
   *  B is C^2 globally (it is the cubic B-spline: B'' is continuous,
   *  piecewise linear; only B''' jumps at the knots x_c = 0, +-1, +-2).
   *
   *  Supported (theta_name, arg_pos) pairs:
   *   - "CGRID2B" arg_pos = k (1-based) -> dD/d cgrid[k-1] (one component)
   *   - "SGRID2B" arg_pos = k (1-based) -> dD/d sgrid[k-1] (one component)
   *   - "RCUT2B"  arg_pos = -1          -> dD/d rcut       (all components)
   *
   *  Anything else returns false (HYBRID FD-fallback, ANALYTICAL throw).
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

  /** Closed-form analytical derivative of the D2_Blip force-descriptor
   *  radial component (dD/dr) with respect to its hyperparameters.
   *
   *    fd(c, 0) = w(Zj) * dD_c/dr
   *             = w(Zj) * [fc(r) * eta_c * b'(x_c) + fc'(r) * B(x_c)]
   *
   *  where x_c = eta_c (r - mu_c) and b'(x_c) is the bare blip
   *  derivative (dB(x, 1.0) in d_basis_functions.h). The mixed
   *  derivatives use b''(x_c) too — derived inline.
   *
   *  Branches:
   *   - "CGRID2B" arg_pos = k -> d/d mu_c: only c == k-1 contributes
   *   - "SGRID2B" arg_pos = k -> d/d eta_c: only c == k-1 contributes
   *   - "RCUT2B"  arg_pos = -1 -> d/d rcut: fans out across components
   *  Returns false on unowned theta / out-of-range arg_pos. */
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
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Blip; }
};
} // namespace models
} // namespace tadah
#endif
