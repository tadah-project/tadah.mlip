#ifndef DM_mEAD_H
#define DM_mEAD_H
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/dm/dm_registered_names.h>

namespace tadah {
namespace models {

/** Modified Embedded Atom Descriptor
 *
 * \warning **Rotational invariance** holds only when \f$\mathcal{F}=\rho^2\f$
 * (i.e.\ F_SQ).  For any other channel function and \f$L>0\f$ the descriptor
 * is **not** rotationally invariant because \f$\mathcal{F}\f$ is applied to
 * individual orbital densities before the invariance-restoring quadratic sum.
 * A runtime warning is emitted in that case.  Use DM_mBlipL for a descriptor
 * that is invariant for arbitrary \f$\mathcal{F}_L\f$.
 *
 * REQUIRED KEYS: SGRIDMB, CGRIDMB, and KEYS OF THE EMBEDDING FUNCTION
 *
 * This descriptor has a mathematical form very similar to DM_EAD
 * but allows the usage of a custom-defined embedding function, \f$ \mathcal{F} \f$, in place of the default quadratic one.
 *
 * Available implementations:
 *   1. DM_mRLR uses F_RLR
 *   2. DM_mSQRT uses F_SQRT
 *   3. DM_mBlip uses F_Blip
 *   4. DM_mBlipEnv uses F_mEnv
 *
 * \f[
 * V_i^{L,\eta,r_s} = \sum_{l_x,l_y,l_z}^{l_x+l_y+l_z=L} \frac{L!}{l_x!l_y!l_z!}
 * \mathcal{F}\Big( \rho_i^{\eta,r_s,l_x,l_y,l_z} \Big)
 * \f]
 *
 * where the density \f$ \rho \f$ is calculated using Gaussian Type Orbitals:
 * \f[
 * \rho_i^{\eta,r_s,l_x,l_y,l_z} = \sum_{j \neq i} x_{ij}^{l_x} y_{ij}^{l_y} z_{ij}^{l_z}
 * \exp{\Big(-\eta(r_{ij}-r_s)^2\Big)} f_c(r_{ij})
 * \f]
 *
 * \ref CGRIDMB parameters control the position \f$ r_s \f$ of the Gaussian basis function.
 *
 * \ref SGRIDMB parameters control the width \f$ \eta \f$ of the Gaussian basis function.
 *
 * e.g., \f$L_{max}=2\f$ will calculate descriptors with \f$ L=0,1,2 \f$ (s, p, d orbitals).
 *
 * F_RLR and F_SQRT take one CEMBFUNC and SEMBFUNC value per radial
 * function (F_SQRT no CEMBFUNC, and every SEMBFUNC value 1 when the size
 * is 0):
 *
 * ```
 * # TYPEMB params: L, size(cgrid), size(sgrid),
 * # size(cembfunc), size(sembfunc), list of element pairs
 * TYPEMB    DM_mRLR 0 7 7 7 7 Ta Ta
 * RCTYPEMB  Cut_Tanh
 * RCUTMB    7.5
 * SGRIDMB   GEOM 7 0.1 10
 * CGRIDMB   0 0 0 0 0 0 0
 * SEMBFUNC  1.2 1.2 1.2 1.2 1.2 1.2 1.2
 * CEMBFUNC  0.5 0.5 0.5 0.5 0.5 0.5 0.5
 * ```
 *
 * ```
 * # TYPEMB params: L, size(cgrid), size(sgrid),
 * # size(cembfunc), size(sembfunc), list of element pairs
 * TYPEMB    DM_mSQRT 0 5 5 0 5 * *
 * RCTYPEMB  Cut_Cos
 * RCUTMB    3.0
 * CGRIDMB   LIN 5 0 3.0
 * SGRIDMB   GEOM 5 1.0 10.0
 * SEMBFUNC  1.5 1.5 1.5 1.5 1.5
 * ```
 *
 * For dimension-expanding embedding functions (e.g. F_Blip), CGRIDMB and
 * SGRIDMB must have size 1, while CEMBFUNC/SEMBFUNC determine the
 * descriptor output dimension:
 *
 * ```
 * TYPEMB    DM_mBlip 0 1 1 5 5 Ta Ta
 * RCTYPEMB  Cut_Cos
 * RCUTMB    5.0
 * CGRIDMB   0.0
 * SGRIDMB   -1.5
 * CEMBFUNC  0.0 0.5 1.0 1.5 2.0
 * SEMBFUNC  1.0 1.0 1.0 1.0 1.0
 * ```
 *
 * Required Config keys:
 * \ref INITMB \ref CGRIDMB \ref SGRIDMB
 */

template <typename F>
class DM_mEAD: public DM_Base {
private:
  int Lmax=0;
  std::string lab = DM_mEAD_name<F>::value;  // the registered name
  tadah::core::v_type sgrid;
  tadah::core::v_type cgrid;
  std::vector<std::vector<std::vector<int>>> orbitals;
  std::vector<std::vector<double>> fac;
  size_t gen_atomic_orbitals(int L);
  double rc;
  F emb;
  /** Output dimension per L channel.
   *  For standard functions: odim == cgrid.size().
   *  For dimension-expanding functions (F_Blip): odim == emb.output_dim(). */
  size_t odim = 0;

public:
  DM_mEAD();
  DM_mEAD(const tadah::core::Context&);
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
  /** DM_Base::depends_on(), and the scalar keys of the embedding function
   *  (RCUTENV of DM_mBlipEnv: F::scalar_keys()). */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    for (const std::string &k : F::scalar_keys())
      if (theta_name == k) return true;
    return DM_Base::depends_on(theta_name, arg_pos);
  }
  void init_rhoi(tadah::core::rho_type& rhoi) override;
  void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rho,
    const double scale=1) const override;
  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  // Radial basis is a Gaussian GTO for every embedding F (F is a separate axis).
  std::optional<BasisKind> basis_kind() const override { return BasisKind::Gaussian; }

  /* Bring base string-keyed shims into scope. */
  using DM_Base::calc_drhoi_dtheta;
  using DM_Base::calc_daed_dtheta;
  using DM_Base::calc_dXijdri_dtheta;

  /** Closed-form per-pair contribution to drho_i/dtheta — radial-side
   *  theta only (CGRIDMB[k], SGRIDMB[k], RCUTMB). Mirrors DM_EAD's
   *  override byte-for-byte (the density is a radial GTO sum that does
   *  NOT depend on the embedding function F).
   *
   *  Closed forms (per pair, fanned out across orbitals via the
   *  Cartesian-power polynomial poly = px[lx]*py[ly]*pz[lz]):
   *
   *    drho/dCGRIDMB[k] = 2 eta[k] (r - mu[k]) G(r; eta[k], mu[k]) * fc * poly
   *                                                       (only c==k)
   *    drho/dSGRIDMB[k] = -(r - mu[k])^2 G(r; eta[k], mu[k]) * fc * poly
   *                                                       (only c==k)
   *    drho/dRCUTMB     = exp(-eta[c] (r-mu[c])^2) * (dfc/drcut) * poly
   *                                                       (all c)
   *
   *  F-specific theta (CEMBFUNC / SEMBFUNC for F_Blip, RCUTENV for
   *  F_mEnv) are NOT owned here — they affect the aed via F.calc_F
   *  alone, with no drho/dtheta contribution. They are left for a
   *  follow-up F::calc_dF_dtheta hook (currently fall through to FD). */
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

  /** Closed-form per-atom contribution to daed/dtheta — handles both
   *  the expands_dimensions path (F_Blip / F_mEnv, single radial,
   *  embedding output dim = sfuncgrid.size()) and the standard path
   *  (ng outputs, one F per cgrid index).
   *
   *  Chain rule:
   *
   *    daed_p/dtheta = f[L,o] * (dF/drho)(rho, p) * drho/dtheta
   *                  + (df[L,o]/dtheta) * F(rho, p)
   *
   *  where f[L,o] = L!/(lx!ly!lz!) / (4 rc)^L gives
   *  df[L,o]/dRCUTMB = -L/rc * f[L,o]; df[L,o]/dCGRIDMB = df[L,o]/dSGRIDMB = 0.
   *  For CGRIDMB[k]/SGRIDMB[k], only c==k touches the aed
   *  (component-local in the standard path; in the expanding path
   *  ng==1 forces k==0). */
  bool calc_daed_dtheta(
      DMThetaKey         key,
      int                arg_pos,
      const tadah::core::rho_type& rhoi,
      const tadah::core::rho_type& drhoi,
      tadah::core::aed_type& daed_out,
      const double       scale=1) const override;

  /** Closed-form per-pair contribution to d(fd_ij)/dtheta — many-body
   *  sibling of D2_Base::calc_dXijdri_dtheta. Mirrors the loop structure
   *  of calc_dXijdri (full axis-expanded output) with the closed-form
   *  derivative wrt the theta parameter applied at every stage:
   *
   *  Radial theta (CGRIDMB[k], SGRIDMB[k], RCUTMB) for the
   *  F::expands_dimensions path (DM_mBlip / DM_mBlipEnv, ng==1):
   *
   *     term_k_g    = dpoly/dr_k                  (polynomial gradient)
   *     term_k_dg   = poly * (v_k / r)            (radial unit-direction)
   *     d(fd_ij_p, k)/dθ = scale * w_j * [
   *          f * (d(F'(rho, p))/dθ_radial) * (g * term_k_g + dg * term_k_dg)
   *        + f * F'(rho, p) * (dg/dθ * term_k_g + d(dg)/dθ * term_k_dg)
   *        + (df/dθ) * F'(rho, p) * (g * term_k_g + dg * term_k_dg)
   *     ]
   *
   *  where d(F'(rho))/dθ_radial = F''(rho, p) * drho/dθ (via F::calc_d2F)
   *  and df/dRCUTMB = -L/rc * f (df/dCGRIDMB = df/dSGRIDMB = 0).
   *
   *  Embedding theta (CEMBFUNC[i], SEMBFUNC[i], RCUTENV) — rho fixed:
   *
   *     d(fd_ij_p, k)/dθ_emb = scale * w_j * f *
   *                            (∂F'/∂θ at fixed rho) *
   *                            (g * term_k_g + dg * term_k_dg)
   *
   *  via F::calc_dFp_dtheta. Component-local theta only contribute when
   *  p == arg_pos - 1; RCUTENV fans out across all p.
   *
   *  Returns false on unowned theta or when the underlying F lacks the
   *  required calc_d2F / calc_dFp_dtheta closure (e.g. F_RLR has neither
   *  yet — those θ fall through to FD under HYBRID). */
  bool calc_dXijdri_dtheta(
      DMThetaKey         key,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      const tadah::core::Vec3d& vec_ij,
      const tadah::core::rho_type& rhoi,
      const tadah::core::rho_type& drhoi_dtheta,
      tadah::core::fd_type& fd_ij_out,
      const double       scale=1) const override;
};
} // namespace models
} // namespace tadah

#include "dm_mEAD.hpp"

#endif
