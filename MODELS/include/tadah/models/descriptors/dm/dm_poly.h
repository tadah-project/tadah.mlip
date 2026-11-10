#ifndef DM_POLY_H
#define DM_POLY_H
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/dm/radial_policies.h>
#include <tadah/models/descriptors/dm/dm_registered_names.h>

namespace tadah {
namespace models {

/* =========================================================================
 * Aggregation mode tags — select how the channel function F is applied.
 * ========================================================================= */

/**
 * \brief PowerSpectrum aggregation: \f$V_{L,c} = \mathcal{F}(\varrho_{L,c})\f$
 *
 * The orbital densities are first squared and summed to give the invariant
 * power-spectrum scalar
 * \f$ \varrho_{L,c} = \sum_o \frac{L!}{l_x!\,l_y!\,l_z!}\,\rho_{L,o,c}^2 \f$,
 * then \f$\mathcal{F}\f$ is applied.  **Rotationally invariant for any F.**
 */
struct PowerSpectrum {};

/**
 * \brief OrbitalWise aggregation: \f$V_{L,c} = \sum_o \frac{L!}{l_x!\,l_y!\,l_z!}\,\mathcal{F}(\rho_{L,o,c})\f$
 *
 * \f$\mathcal{F}\f$ is applied to each individual orbital density before
 * the weighted sum.  Equivalent to DM_mEAD for BlipRadial.
 *
 * \warning Rotationally invariant **only** when \f$\mathcal{F}=\rho^2\f$ (F_SQ)
 * and \f$L>0\f$.  For any other F and L>0 the descriptor is NOT invariant.
 * See \ref DM_mEAD for the proof.  Prefer PowerSpectrum for invariance.
 */
struct OrbitalWise {};

/* =========================================================================
 * DM_Poly
 * ========================================================================= */

/** \brief Generalised many-body descriptor with swappable radial basis,
 *  channel function, and aggregation mode.
 *
 * \f[
 *   \rho_{i}^{L,c,l_x,l_y,l_z}
 *   = \sum_{j \neq i} x_{ij}^{l_x}\,y_{ij}^{l_y}\,z_{ij}^{l_z}\,
 *     \phi_c(r_{ij})\,f_c(r_{ij})
 * \f]
 *
 * **PowerSpectrum mode** (default, always invariant):
 * \f[
 *   V_{i}^{L,c} = \mathcal{F}\!\left(
 *     \sum_{\substack{l_x+l_y+l_z=L}} \frac{L!}{l_x!\,l_y!\,l_z!}\,
 *     (\rho_{i}^{L,c,l_x,l_y,l_z})^2
 *   \right)
 * \f]
 *
 * **OrbitalWise mode** (DM_mEAD-equivalent, invariant only for F_SQ and L=0):
 * \f[
 *   V_{i}^{L,c} = \sum_{\substack{l_x+l_y+l_z=L}} \frac{L!}{l_x!\,l_y!\,l_z!}\,
 *   \mathcal{F}(\rho_{i}^{L,c,l_x,l_y,l_z})
 * \f]
 *
 * The multinomial prefactor is normalised by \f$(4r_c)^L\f$.
 *
 * \tparam ChannelFn   Channel function (F_SQ, F_SQRT, F_RLR, F_Blip, …).
 * \tparam RadialBasis Radial basis policy (BlipRadial, GaussianRadial, …).
 *                     Default: BlipRadial.
 * \tparam AggMode     Aggregation mode (PowerSpectrum, OrbitalWise).
 *                     Default: PowerSpectrum.
 *
 * Available instantiations registered in dm_all.cpp:
 * | Config name      | ChannelFn | RadialBasis     | AggMode       |
 * |------------------|-----------|-----------------|---------------|
 * | `DM_PS_SQ`       | F_SQ      | BlipRadial      | PowerSpectrum |
 * | `DM_PS_SQRT`     | F_SQRT    | BlipRadial      | PowerSpectrum |
 * | `DM_PS_RLR`      | F_RLR     | BlipRadial      | PowerSpectrum |
 * | `DM_PS_Blip`     | F_Blip    | BlipRadial      | PowerSpectrum |
 * | `DM_GPS_SQ`      | F_SQ      | GaussianRadial  | PowerSpectrum |
 * | `DM_GPS_SQRT`    | F_SQRT    | GaussianRadial  | PowerSpectrum |
 * | `DM_GPS_RLR`     | F_RLR     | GaussianRadial  | PowerSpectrum |
 * | `DM_GPS_Blip`    | F_Blip    | GaussianRadial  | PowerSpectrum |
 * | `DM_OW_SQ`       | F_SQ      | BlipRadial      | OrbitalWise   |
 * | `DM_OW_SQRT`     | F_SQRT    | BlipRadial      | OrbitalWise   |
 * | `DM_OW_RLR`      | F_RLR     | BlipRadial      | OrbitalWise   |
 * | `DM_OW_Blip`     | F_Blip    | BlipRadial      | OrbitalWise   |
 * | `DM_GOW_SQ`      | F_SQ      | GaussianRadial  | OrbitalWise   |
 * | `DM_GOW_SQRT`    | F_SQRT    | GaussianRadial  | OrbitalWise   |
 * | `DM_GOW_RLR`     | F_RLR     | GaussianRadial  | OrbitalWise   |
 * | `DM_GOW_Blip`    | F_Blip    | GaussianRadial  | OrbitalWise   |
 *
 * Required Config keys:
 * \ref INITMB \ref CGRIDMB \ref SGRIDMB (plus channel-function keys)
 */
template <typename ChannelFn,
          typename RadialBasis = BlipRadial,
          typename AggMode     = PowerSpectrum>
class DM_Poly : public DM_Base {
private:
  int Lmax = 0;
  // the registered name
  std::string lab = DM_Poly_name<ChannelFn, RadialBasis, AggMode>::value;
  tadah::core::v_type sgrid;
  tadah::core::v_type cgrid;
  std::vector<std::vector<std::vector<int>>> orbitals;
  std::vector<std::vector<double>> fac;
  size_t gen_atomic_orbitals(int L);
  double rc;
  ChannelFn emb;
  /** Output dimension per L channel.
   *  For standard functions: odim == cgrid.size().
   *  For dimension-expanding functions (F_Blip): odim == emb.output_dim(). */
  size_t odim = 0;

public:
  DM_Poly();
  DM_Poly(const tadah::core::Context&);
  void calc_aed(
    tadah::core::rho_type& rho,
    tadah::core::aed_type& aed) const override;
  void calc_dXijdri_dXjidri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d& vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type& fd_ij,
    const double scale = 1) const override;
  void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d& vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type& fd_ij,
    const double scale = 1) const override;
  std::string label() const override;
  /** DM_Base::depends_on(), and the scalar keys of the embedding function
   *  (ChannelFn::scalar_keys()). */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    for (const std::string &k : ChannelFn::scalar_keys())
      if (theta_name == k) return true;
    return DM_Base::depends_on(theta_name, arg_pos);
  }
  void init_rhoi(tadah::core::rho_type& rhoi) override;
  void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d& vec_ij,
    tadah::core::rho_type& rho,
    const double scale = 1) const override;
  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  // Radial family comes from the compile-time policy (BlipRadial/GaussianRadial);
  // the ChannelFn/AggMode axes are orthogonal to the radial basis.
  std::optional<BasisKind> basis_kind() const override { return RadialBasis::kind; }
};

} // namespace models
} // namespace tadah

#include "dm_poly.hpp"

#endif
