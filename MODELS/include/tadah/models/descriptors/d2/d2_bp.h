#ifndef D2_BP_H
#define D2_BP_H
#include <tadah/models/descriptors/d2/d2_base.h>
#include <vector>

namespace tadah {
namespace models {

/** \brief Behler-Parrinello two-body descriptor.
 *
 * \f[
 * V_i^{\eta,r_s} = \sum_{j \neq i} \exp{\Big(-\eta(r_{ij}-r_s)^2\Big)}f_c(r_{ij})
 * \f]
 *
 * \ref CGRID2B parameters control position \f$ r_s \f$ of the gaussian basis function.
 *
 * \ref SGRID2B parameters control width \f$ \eta \f$ of the gaussian basis function.
 *
 * This is essentially a \f$ G^1_i \f$ descriptor from the below paper with
 * an exception that it can use any cutoff function defined in Ta-dah!:
 *
 * <div class="csl-entry">Behler, J., Parrinello, M. (2007).
 * Generalized neural-network representation of high-dimensional
 * potential-energy surfaces. <i>Physical Review Letters</i>,
 * <i>98</i>(14), 146401. https://doi.org/10.1103/PhysRevLett.98.146401</div>
 *
 * Required tadah::core::Context keys:
 * \ref INIT2B \ref CGRID2B \ref SGRID2B
 */
class D2_BP : public D2_Base {
private:
  std::string lab="D2_BP";
  tadah::core::v_type cgrid;
  tadah::core::v_type sgrid;

public:
  D2_BP();
  D2_BP(const tadah::core::Context &context);
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

  /** Closed-form analytical derivatives of the BP-Gaussian D2_BP
   *  descriptor with respect to its hyperparameters:
   *
   *    G(r; eta, mu, fc) = exp(-eta (r - mu)^2) * fc(r)
   *
   *    dG/d eta  = -(r - mu)^2 * G
   *    dG/d mu   = 2 eta (r - mu) * G
   *    dG/d rcut = exp(-eta (r - mu)^2) * df_c/d rcut(r)
   *
   * Supported (theta_name, arg_pos) pairs:
   *  - "CGRID2B" arg_pos = k (1-based) -> dG/d cgrid[k-1]   (one component)
   *  - "SGRID2B" arg_pos = k (1-based) -> dG/d sgrid[k-1]   (one component)
   *  - "RCUT2B"  arg_pos = -1          -> dG/d rcut         (all components)
   *
   * For other keys / out-of-range arg_pos returns false (the aggregator
   * routes via FD fallback under HYBRID, throws under ANALYTICAL).
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

  /** Closed-form analytical derivative of the BP-Gaussian D2_BP
   *  force-descriptor radial component (dG/dr scaled by w(Zj)) with
   *  respect to its hyperparameters.
   *
   *  Per-pair force-descriptor: fd(c) = w(Zj) * dG/dr where
   *    G(r; eta, mu) f_c(r) -> dG/dr (with cutoff product rule) =
   *      exp(-eta (r-mu)^2) * (-2 eta (r-mu) f_c(r) + f_c'(r))
   *
   *  Mixed derivative d/dtheta of dG/dr:
   *    d/d eta  : -(r-mu)^2 * dG/dr - 2 (r-mu) f_c exp(-eta (r-mu)^2)
   *    d/d mu   :  2 eta (r-mu) * dG/dr + 2 eta f_c exp(-eta (r-mu)^2)
   *    d/d rcut :  exp(-eta (r-mu)^2) * (-2 eta (r-mu) df_c/drcut
   *                                                  + df_c'/drcut)
   *
   *  Supported (theta_name, arg_pos) pairs match calc_dDdtheta:
   *  CGRID2B[k], SGRID2B[k], RCUT2B (arg_pos=-1, fans out across all
   *  components). Returns false otherwise. */
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
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Gaussian; }
};
} // namespace models
} // namespace tadah
#endif
