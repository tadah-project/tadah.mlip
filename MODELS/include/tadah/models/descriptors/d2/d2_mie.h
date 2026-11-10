#ifndef D2_MIE_H
#define D2_MIE_H

#include <tadah/models/descriptors/d2/d2_base.h>

namespace tadah {
namespace models {

/** \brief Mie descriptor
 *
 * \f[
 * V_i = \sum_{j \neq i} C \epsilon \Bigg(\Big(\frac{\sigma}{r_{ij}}\Big)^{n} - \Big(\frac{\sigma}{r_{ij}}\Big)^m\Bigg)
 * \f]
 *
 * where
 * \f[
 * C=\frac{n}{n-m}\Big( \frac{n}{m} \Big)^{\frac{m}{n-m}}
 * \f]
 *
 *
 * Any cutoff can be used
 *
 * Required tadah::core::Context Key: \ref INIT2B \ref TYPE2B
 *
 * TYPE2B D2_MIE 12 6 ELEMENT1 ELEMENT2
 *
 * will result in Lennard-Jones type descriptor
 */
class D2_MIE : public D2_Base {
private:
  std::string lab="D2_MIE";
  double m,n;
  tadah::core::v_type etas;
public:
  D2_MIE();
  D2_MIE(const tadah::core::Context &context);
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

  /** Closed-form analytical derivatives of D2_MIE w.r.t. its
   *  hyperparameters. Exposed via meta(): RCUT2B (real-space cutoff)
   *  plus the two TYPE2B-positional Mie exponents n (config index 1)
   *  and m (config index 2).
   *
   *  D2_MIE contributes two components per pair (matching calc_aed):
   *    aed(fidx)     = -w(Zj) r^{-n} fc(r)
   *    aed(fidx+1)   = +w(Zj) r^{-m} fc(r)
   *
   *  Closed forms (only the matching column receives a non-zero entry
   *  for n / m; both receive a contribution for rcut):
   *    dD_n/dn      = +w(Zj) ln(r) r^{-n} fc(r)
   *    dD_m/dm      = -w(Zj) ln(r) r^{-m} fc(r)
   *    dD_n/d rcut  = -w(Zj) r^{-n} dfc/d rcut
   *    dD_m/d rcut  = +w(Zj) r^{-m} dfc/d rcut
   *
   *  arg_pos convention (ParamMeta::arg_pos): the 1-based position of the
   *  value on the descriptor's TYPE2B line, whose first value is the name
   *  D2_MIE:
   *    arg_pos == 2 -> n  (config index 1)
   *    arg_pos == 3 -> m  (config index 2)
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

  /** Closed-form ∂(fd_radial)/∂ theta for D2_MIE.
   *
   *  Per-pair force descriptor radial column (from calc_dXijdri):
   *    fd(fidx,  0) = w(Zj) [  n r^{-n-1} fc - fcp r^{-n}]
   *    fd(fidx+1,0) = w(Zj) [- m r^{-m-1} fc + fcp r^{-m}]
   *
   *  d/dn (only fidx contributes):
   *    = w(Zj) [r^{-n-1} fc (1 - n ln r) + fcp ln(r) r^{-n}]
   *  d/dm (only fidx+1 contributes):
   *    = -w(Zj) [r^{-m-1} fc (1 - m ln r) + fcp ln(r) r^{-m}]
   *  d/d rcut (both columns):
   *    fidx:   w(Zj) [n r^{-n-1} dfc - dfcp r^{-n}]
   *    fidx+1: w(Zj) [-m r^{-m-1} dfc + dfcp r^{-m}]
   *
   *  ln(r)=0 at r=1: d/dn and d/dm vanish identically (closed form
   *  still returns true). */
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
  /** D2_Base::depends_on(), and the exponents n and m of the TYPE2B line:
   *  TYPE2B positions 2 and 3 (the label is 1), as calc_dDdtheta takes
   *  them. */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    return (theta_name == "TYPE2B" && (arg_pos == 2 || arg_pos == 3)) ||
           D2_Base::depends_on(theta_name, arg_pos);
  }
};
} // namespace models
} // namespace tadah
#endif
