#ifndef D2_LJ_H
#define D2_LJ_H

#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/** \anchor D2_LJ \brief Standard Lennard - Jones descriptor
 *
 * \f[
 * V_i = \sum_{j \neq i} 4 \epsilon \Bigg(\Big(\frac{\sigma}{r_{ij}}\Big)^{12} - \Big(\frac{\sigma}{r_{ij}}\Big)^6\Bigg) f_c(r_{ij})
 * \f]
 *
 * or equivalently:
 *
 * \f[
 * V_i = \sum_{j \neq i} \frac{C_{12}}{r_{ij}^{12}} - \frac{C_6}{r_{ij}^6} f_c(r_{ij})
 * \f]
 *
 * The machine-learned coefficients \f$C_6\f$ and \f$C_{12}\f$
 * correspond to \f$\sigma\f$ and \f$\epsilon\f$ through the following relation:
 *
 * \f[ \sigma = \Big(\frac{C_{12}}{C_6}\Big)^{1/6} \f]
 * \f[ \epsilon = \frac{1}{4} \frac{C_6^2}{C_{12}} w(Z) \f]
 * where \f$w(Z)\f$ is a species depended weight factor (default is an atomic number).
 *
 * The machine learned \f$\sigma\f$ and \f$\epsilon\f$ only make sense
 * (say to compare with the literature ones) when \ref BIAS false
 * and \ref NORM false and system in monatomic.
 * It is ok thought to set them to true it's just that
 * numerical values will be different.
 *
 * Required tadah::core::Context Key: \ref INIT2B
 */
class D2_LJ : public D2_Base {
private:
  std::string lab="D2_LJ";
public:
  D2_LJ();
  D2_LJ(const tadah::core::Context &context);
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

  /** Closed-form analytical derivatives of D2_LJ w.r.t. its
   *  hyperparameters. Only RCUT2B is exposed (sigma / epsilon are
   *  recovered post-fit from the C_6 / C_12 linear regression
   *  coefficients — they are not descriptor params, see d2_lj.h docs).
   *
   *  D2_LJ contributes two components per pair:
   *    aed(fidx)   = -w(Zj) r^-6  fc(r)
   *    aed(fidx+1) = +w(Zj) r^-12 fc(r)
   *
   *  Only fc depends on rcut. So:
   *    dD/d rcut (fidx)   = -w(Zj) r^-6  * dfc/d rcut
   *    dD/d rcut (fidx+1) = +w(Zj) r^-12 * dfc/d rcut
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

  /** Closed-form ∂(fd_radial)/∂ rcut for D2_LJ.
   *
   *  Per-pair force descriptor radial component (column 0):
   *    fd(fidx,  0) = w(Zj) * ( 6 r^-7 fc - r^-6  fcp)
   *    fd(fidx+1,0) = w(Zj) * (-12 r^-13 fc + r^-12 fcp)
   *
   *  Only fc and fcp depend on rcut, so:
   *    d/d rcut (fidx,0)   = w(Zj) * ( 6 r^-7 * dfc/d rcut - r^-6  * dfcp/d rcut)
   *    d/d rcut (fidx+1,0) = w(Zj) * (-12 r^-13 * dfc/d rcut + r^-12 * dfcp/d rcut)
   *
   *  Non-RCUT2B theta returns false (sigma/epsilon are post-fit, not
   *  HPO params). */
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
};
} // namespace models
} // namespace tadah
#endif
