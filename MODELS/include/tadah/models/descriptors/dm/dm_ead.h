#ifndef DM_EAD_H
#define DM_EAD_H
#include <tadah/models/descriptors/dm/dm_base.h>

namespace tadah {
namespace models {

/**  Embedded Atom Descriptor
 *
 * \f[
 * V_i^{L,\eta,r_s} = \sum_{l_x,l_y,l_z}^{l_x+l_y+l_z=L} \frac{L!}{l_x!l_y!l_z!}
 * \Big( \rho_i^{\eta,r_s,l_x,l_y,l_z} \Big)^2
 * \f]
 *
 * where density \f$ \rho \f$ is calculated using Gaussian Type Orbitals:
 * \f[
 * \rho_i^{\eta,r_s,l_x,l_y,l_z} = \sum_{j \neq i} x_{ij}^{l_x}y_{ij}^{l_y}z_{ij}^{l_z}
 * \exp{\Big(-\eta(r_{ij}-r_s)^2\Big)}f_c(r_{ij})
 * \f]
 *
 * \ref CGRIDMB parameters control position \f$ r_s \f$ of the gaussian basis function.
 *
 * \ref SGRIDMB parameters control width \f$ \eta \f$ of the gaussian basis function.
 *
 * e.g. \f$L_{max}=2\f$ will calculate descriptors with \f$ L=0,1,2 \f$ (s,p,d orbitals).
 *
 * More information about this descriptor:
 *
 * <div class="csl-entry">Zhang, Y., Hu, C.,Jiang, B.
 * (2019). Embedded atom neural network potentials: efficient and accurate
 * machine learning with a physically inspired representation.
 * <i>Journal of Physical Chemistry Letters</i>, <i>10</i>(17),
 * 4962–4967. https://doi.org/10.1021/acs.jpclett.9b02037</div>
 *
 * Required tadah::core::Context keys:
 * \ref INITMB \ref CGRIDMB \ref SGRIDMB
 */
class DM_EAD: public DM_Base {
private:
  int Lmax=0;
  std::string lab="DM_EAD";
  tadah::core::v_type sgrid;
  tadah::core::v_type cgrid;
  std::vector<std::vector<std::vector<int>>> orbitals;
  std::vector<std::vector<double>> fac;
  size_t gen_atomic_orbitals(int L);
  double fact(int n);
  double my_pow(double,int);
  double rc;

public:
  DM_EAD();
  DM_EAD(const tadah::core::Context&);
  void calc_aed(
    tadah::core::rho_type& rho,
    tadah::core::aed_type &aed) const override;
  void calc_dXijdri_dXjidri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type &fd_ij, 
    const double scale=1) const override;
  std::string label() const override;
  void init_rhoi(tadah::core::rho_type& rhoi) override;
  void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rho,
    const double scale=1) const override;

  /* Bring base string-keyed shims into scope. */
  using DM_Base::calc_drhoi_dtheta;
  using DM_Base::calc_daed_dtheta;
  using DM_Base::calc_dXijdri_dtheta;

  /** \brief Closed-form per-pair contribution to drho_i/dtheta for DM_EAD.
   *
   *  Math (cgrid component c, angular orbital (L, o = (lx,ly,lz))):
   *
   *    rho_i[c,L,o] = sum_j w(Zj) * px py pz * G(r_ij; eta[c], r_s[c]) * fc(r_ij)
   *
   *  with G(r; eta, r_s) = exp(-eta * (r - r_s)^2).
   *
   *  Closed forms:
   *
   *    drho/dCGRIDMB[k] = 2 eta[k] (r - r_s[k]) G fc * px py pz  (only c==k)
   *    drho/dSGRIDMB[k] = -(r - r_s[k])^2     G fc * px py pz  (only c==k)
   *    drho/dRCUTMB     = G(r; eta[c], r_s[c]) * dfc/drcut * px py pz   (all c)
   *
   *  CGRIDMB[k] / SGRIDMB[k] are component-local: they only touch
   *  drhoi entries for c == k. RCUTMB fans out across every component.
   */
  bool calc_drhoi_dtheta(
      DMThetaKey         key,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      const tadah::core::Vec3d& vec_ij,
      tadah::core::rho_type& drhoi,
      const double       scale=1) const override;

  /** \brief Closed-form per-atom contribution to daed/dtheta for DM_EAD.
   *
   *  Given rhoi already filled with both rho_i (at rfidx..) and F'(rho_i)
   *  = 2 f[L,o] rho_i (at drfidx..), plus drhoi filled with drho_i/dtheta:
   *
   *    daed[c+Lshift+fidx]/dtheta = F'(rho)[c,L,o] * drho/dtheta[c,L,o]
   *                                 + (df[L,o]/dtheta) * rho_i[c,L,o]^2
   *
   *  For RCUTMB, f[L,o] = L!/(lx!ly!lz!) / (4 rc)^L gives:
   *    df[L,o]/dRCUTMB = -L/rc * f[L,o]
   *  For CGRIDMB/SGRIDMB, f does not depend on theta so the second term
   *  vanishes.
   */
  bool calc_daed_dtheta(
      DMThetaKey         key,
      int                arg_pos,
      const tadah::core::rho_type& rhoi,
      const tadah::core::rho_type& drhoi,
      tadah::core::aed_type& daed_out,
      const double       scale=1) const override;

  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  // Radial basis is a Gaussian GTO (the F_ embedding is a separate axis).
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Gaussian; }
};
} // namespace models
} // namespace tadah
#endif
