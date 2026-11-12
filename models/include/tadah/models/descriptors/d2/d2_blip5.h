#ifndef D2_BLIP5_H
#define D2_BLIP5_H
#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/** \brief Quintic B-spline (M6) two-body descriptor.
 *
 * \f[
 * V_i^{\eta,r_s} =\sum_{j \neq i} B_5(\eta(r_{ij}-r_s))f_c(r_{ij})
 * \f]
 *
 * where \f$ B_5 \f$ is the peak-normalised quintic B-spline bump (Schoenberg
 * M6): compact support \f$|x_c| < 3\f$, C^4 globally (only the 5th
 * derivative jumps at the knots \f$ x_c \in \{0,\pm1,\pm2,\pm3\} \f$).
 * The C^4 twin of \ref D2_Blip: same (centre, width) semantics, one order
 * better second-derivative fidelity (O(h^4) vs O(h^2) knot-scale sawtooth),
 * ~2-3x the per-call cost.
 *
 * \ref CGRID2B parameters control the centres \f$ r_s \f$; \ref SGRID2B the
 * inverse widths \f$ \eta \f$ (full support in r: \f$ 6/\eta \f$).
 *
 * Required keys:
 * \ref INIT2B \ref CGRID2B \ref SGRID2B
 */
class D2_Blip5 : public D2_Base {
private:
  std::string lab="D2_Blip5";
  tadah::core::v_type cgrid;
  tadah::core::v_type sgrid;

public:
  D2_Blip5();
  D2_Blip5(const tadah::core::Context &context);
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
   *    D_c(r) = w(Zj) * B5(x_c) * fc(r),   x_c = eta_c (r - mu_c)
   *
   *  Chain rule:
   *    dD_c/d mu_c   = w(Zj) * dB/dx_c |_{x_c} * (-eta_c) * fc(r)
   *    dD_c/d eta_c  = w(Zj) * dB/dx_c |_{x_c} * (r - mu_c) * fc(r)
   *    dD_c/d rcut   = w(Zj) * B(x_c) * df_c/d rcut(r)
   *
   *  B5 and dB5 are piecewise quintics with compact support on (-3, 3);
   *  see d_basis_functions.h. B5 is C^4 globally (only the 5th derivative
   *  jumps at the knots x_c = 0, +-1, +-2, +-3).
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
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Blip5; }
};
} // namespace models
} // namespace tadah
#endif
