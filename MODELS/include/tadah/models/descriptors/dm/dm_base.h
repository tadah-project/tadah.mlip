#ifndef DM_BASE_H
#define DM_BASE_H

#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <cstdint>
#include <optional>
#include <string>

namespace tadah {
namespace models {

/** Compact key enum for DM-arm θ-derivative dispatch. The walker decodes
 *  the OPTIM theta_name once per outer call, then the per-pair kernels
 *  branch on a 1-byte enum compare instead of std::string ==. Saves an
 *  O(220M) byte-compare ladder per gradient on a six-function DM_Blip
 *  search. */
enum class DMThetaKey : std::uint8_t {
  None     = 0,
  CGRIDMB  = 1,
  SGRIDMB  = 2,
  RCUTMB   = 3,
  WATOM    = 4,
  CEMBFUNC = 5,
  SEMBFUNC = 6,
  RCUTENV  = 7,
};

inline DMThetaKey decode_dm_theta(const std::string& s) noexcept {
  if (s.size() < 5) return DMThetaKey::None;
  if (s == "CGRIDMB")  return DMThetaKey::CGRIDMB;
  if (s == "SGRIDMB")  return DMThetaKey::SGRIDMB;
  if (s == "RCUTMB")   return DMThetaKey::RCUTMB;
  if (s == "WATOM")    return DMThetaKey::WATOM;
  if (s == "CEMBFUNC") return DMThetaKey::CEMBFUNC;
  if (s == "SEMBFUNC") return DMThetaKey::SEMBFUNC;
  if (s == "RCUTENV")  return DMThetaKey::RCUTENV;
  return DMThetaKey::None;
}

/** Reverse mapping for the cases where a kernel needs to forward to a
 *  string-keyed downstream API (F_Base::calc_dF_dtheta /
 *  F_Base::calc_dFp_dtheta still take a std::string). Returns a stable
 *  const char*, never deallocated. */
inline const char* dm_theta_name(DMThetaKey k) noexcept {
  switch (k) {
    case DMThetaKey::CGRIDMB:  return "CGRIDMB";
    case DMThetaKey::SGRIDMB:  return "SGRIDMB";
    case DMThetaKey::RCUTMB:   return "RCUTMB";
    case DMThetaKey::WATOM:    return "WATOM";
    case DMThetaKey::CEMBFUNC: return "CEMBFUNC";
    case DMThetaKey::SEMBFUNC: return "SEMBFUNC";
    case DMThetaKey::RCUTENV:  return "RCUTENV";
    case DMThetaKey::None:     return "";
  }
  return "";
}

/** \brief Base class for all many-body type descriptors.
 *
 * All many-body descriptors **must** inherit this class.
 */
class DM_Base: public D_Base {

protected:
  size_t rfidx=0; // index of the first density component in the rhoi array
  size_t drfidx=0; // index of the first component of the derivative of the density component in the rhoi array
  size_t rhoisize=0;   // size of the density (rho) part
  size_t rhoipsize=0;  // size of the derivative (dF/drho) part
public:
  DM_Base() {};
  DM_Base(const tadah::core::Context &c): D_Base(c) {
    //if (c.get<bool>("INITMB")) {
    if (c.contains("RCUTMBMAX") && c.contains("RCTYPEMB")) {
      double rcutmb = c.get<double>("RCUTMBMAX");
      set_fcut(tadah::core::factory<Cut_Base,double&>( c.get<std::string>("RCTYPEMB"), rcutmb ), true);
    }
    else {
      set_fcut(new Cut_Dummy(0), true);
    }
  }
  virtual ~DM_Base() {};
  virtual std::vector<std::string> get_init_atoms(const tadah::core::Context &c) override;

  /** \brief D_Base::depends_on(), and RCUTMB: the density passes through
   *  the cutoff function, whose radius it is (Cut_Dummy truncates it there).
   */
  bool depends_on(const std::string &theta_name, int arg_pos) const override {
    return theta_name == "RCUTMB" || D_Base::depends_on(theta_name, arg_pos);
  }

  /** \brief Calculate \ref AED
   *
   * Calculate Atomic Energy Descriptor for the atom local environment.
   */
  virtual void calc_aed(
    tadah::core::rho_type& rho,
    tadah::core::aed_type &aed) const=0;

  /** \brief Calculate \ref FD
   *
   * Calculate Force Descriptor between to atoms.
   *
   * This method works for half NN lists
   * and linear models.
   *
   * \f[
   * \mathbf{fd}_{ij} \mathrel{+}= 
   * \frac{\partial \mathbf{X}_{ij}}{\partial \mathbf{r}_i} +
   * \frac{\partial \mathbf{X}_{ji}}{\partial \mathbf{r}_i}
   * \f]
   */
  virtual void calc_dXijdri_dXjidri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::rho_type& rhoj,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const=0;

  /** \brief Calculate \ref FD
   *
   * Calculate Force Descriptor between to atoms.
   *
   * This method works for full NN lists
   * and all models.
   *
   * mode=0 indicates only x-direction is computed and
   * the resulting descriptor must be multiplied by delx/r.
   *
   * \f[
   * \mathbf{fd}_{ij} \mathrel{+}=
   * \frac{\partial \mathbf{X}_{ij}}{\partial \mathbf{r}_i}
   * \f]
   */
  virtual void calc_dXijdri(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rhoi,
    tadah::core::fd_type &fd_ij,
    const double scale=1) const=0;

  /** \brief Resize arrays for a density and F' */
  virtual void init_rhoi(tadah::core::rho_type& rhoi)=0;

  /** \brief EAM-collapse capability (scalar-density descriptors).
   *
   *  A DM descriptor is "EAM-form" when its many-body energy is exactly
   *  \f$ V_i = F(\bar\rho_i) \f$ with \f$ \bar\rho_i = \sum_j \rho(r_{ij}) \f$
   *  and, for a linear model, \f$ F(\rho) = \sum_m w_m g_m(\rho) \f$ where the
   *  \f$ w_m \f$ are the model weights of this descriptor's block. For such
   *  descriptors the whole (linear-model) potential collapses to classic EAM
   *  scalar functions, which MD drivers can tabulate and evaluate at
   *  native-EAM cost instead of accumulating per-pair descriptor vectors.
   *
   *  eam_density(): scalar pair density and its r-derivative, including the
   *  cutoff function and the symmetric WATOM factor weights[Zi][Zj]; must
   *  return (0,0) beyond the cutoff.
   *
   *  eam_embed(): embedding value and derivative collapsed against the FULL
   *  model weight vector `w_full` (indexed with this descriptor's own block
   *  offset internally, so callers never handle offsets). It must equal the
   *  w-weighted embedding block that calc_aed() writes, at every rho_bar,
   *  including the descriptor's embedding reference (DM_REAM: EMBF0 ZERO
   *  evaluates the zero-referenced basis, so F(0) = 0 exactly).
   *
   *  Default: not EAM-form. */
  virtual bool eam_form() const { return false; }
  virtual std::size_t component_basis_size(const std::string &) const { return 0; }
  virtual bool component_basis_curve(const std::string &,std::size_t,double,
                                     bool,double &,double &) const { return false; }
  /** The spatial cutoff f_c(q) and f_c'(q) the functions of a component
   *  channel ("DENSITY") are tapered with, as component_basis_curve()
   *  applies it with its cutoff flag (0 beyond the cutoff radius). False:
   *  the channel has no spatial cutoff (an embedding, whose argument is a
   *  density) or the descriptor no component channels; fc and dfc are then
   *  untouched. `tadah analysis bfunc --type MB Y` prints it. */
  virtual bool component_cutoff(const std::string &,double,double &,
                                double &) const { return false; }
  virtual void eam_density(const int /*Zi*/, const int /*Zj*/,
                           const double /*rij*/,
                           double &rho, double &rhop) const {
    rho = 0.0; rhop = 0.0;
  }
  virtual void eam_embed(const tadah::core::t_type & /*w_full*/,
                         const double /*rho_bar*/,
                         double &F, double &Fp) const {
    F = 0.0; Fp = 0.0;
  }

  /** \brief Embedding reference (EMBF0) of an EAM-form embedding block.
   *
   *  nullopt for descriptors without an EMBF0-controlled embedding. */
  virtual std::optional<EmbF0Mode> embf0_mode() const { return std::nullopt; }

  /** \brief Raw embedding value at zero density, sum_m w_m g_m(0).
   *
   *  The isolated-atom embedding energy under EMBF0 FREE, and the constant
   *  per-atom energy that EMBF0 ZERO removes. Diagnostic only (journals):
   *  never feeds an evaluated number. @p w_full is the full model weight
   *  vector of a linear model. nullopt when not applicable. */
  virtual std::optional<double>
  raw_embedding_f0(const tadah::core::t_type & /*w_full*/) const {
    return std::nullopt;
  }

  /** \brief Calculate density */
  virtual void calc_rho(
    const int Zi,
    const int Zj,
    const double rij,
    const double rij_sq,
    const tadah::core::Vec3d &vec_ij,
    tadah::core::rho_type& rho,
    const double scale=1) const =0;

  /** \brief Closed-form per-pair contribution to drho_i/dtheta.
   *
   *  Many-body analog of D2_Base::calc_dDdtheta. The MB design-matrix
   *  builder accumulates `rho_i` in a per-pair pre-pass (calc_rho),
   *  then computes `aed = F(rho)` in a per-atom finalisation step
   *  (calc_aed). The dPhi/dtheta walker mirrors that structure with
   *  TWO methods:
   *
   *   1. calc_drhoi_dtheta  — per-pair drho_i/dtheta accumulator
   *      (analog of calc_rho).
   *   2. calc_daed_dtheta   — per-atom (rho_i, drho_i/dtheta) -> daed/dtheta
   *      reducer (analog of calc_aed).
   *
   *  Calling convention mirrors D2_Base::calc_dDdtheta:
   *   - `theta_name` is the OPTIM key, upper-case (e.g. "CGRIDMB").
   *   - `arg_pos` is 1-based for vector parameters, -1 for scalars.
   *   - `drhoi` is the per-atom accumulator, pre-zeroed by the caller.
   *   - Returns `true` iff this call wrote anything into `drhoi`.
   *   - Cross-cutting parameters (RCUTMB) flow through
   *     `fcut->calc_drcut`; each DM descriptor that uses a non-Cut_Dummy
   *     cutoff contributes additively.
   *
   *  Default returns false (no-op). DM_Dummy and DM descriptors that
   *  are numerical-only (DM_EAM, DM_mEAD) keep the default.
   */
  virtual bool calc_drhoi_dtheta(
      DMThetaKey         /*key*/,
      int                /*arg_pos*/,
      const int          /*Zi*/,
      const int          /*Zj*/,
      const double       /*rij*/,
      const double       /*rij_sq*/,
      const tadah::core::Vec3d& /*vec_ij*/,
      tadah::core::rho_type& /*drhoi*/,
      const double       /*scale=1*/) const {
    return false;
  }

  /** String-keyed form, the one the dPhi/dtheta walker calls. Decodes the
   *  legacy key once and dispatches to the enum-keyed virtual above, which
   *  the legacy descriptors override. A descriptor whose parameters are
   *  named component paths (DM_Embed) or that routes them to its children
   *  (DM_mJoin) overrides this form. */
  virtual bool calc_drhoi_dtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      const tadah::core::Vec3d& vec_ij,
      tadah::core::rho_type& drhoi,
      const double       scale=1) const {
    return calc_drhoi_dtheta(decode_dm_theta(theta_name), arg_pos,
                             Zi, Zj, rij, rij_sq, vec_ij, drhoi, scale);
  }

  /** \brief Closed-form per-atom contribution to daed/dtheta.
   *
   *  Companion to calc_drhoi_dtheta. Called by the dPhi/dtheta walker
   *  once per atom AFTER:
   *   - calc_rho        has filled `rhoi[rfidx..]   = rho_i`,
   *   - calc_aed        has filled `rhoi[drfidx..]  = F'(rho_i)`,
   *   - calc_drhoi_dtheta has filled `drhoi[rfidx..] = drho_i/dtheta`.
   *
   *  Chain rule for F(rho_i, theta):
   *
   *    daed/dtheta = (dF/drho) (drho/dtheta) + (dF/dtheta)|_{rho fixed}
   *
   *  The first term reads F'(rho) from rhoi[drfidx..] and dotproducts
   *  with drhoi[rfidx..]. The second term is non-zero only for
   *  cross-cutting parameters that enter the per-L multinomial
   *  prefactor `f[L,o] = L!/(lx! ly! lz!) / (4 rc)^L` — namely RCUTMB.
   *
   *  Returns `true` iff this call wrote anything into `daed_out`.
   *
   *  Default returns false.
   */
  virtual bool calc_daed_dtheta(
      DMThetaKey         /*key*/,
      int                /*arg_pos*/,
      const tadah::core::rho_type& /*rhoi*/,
      const tadah::core::rho_type& /*drhoi*/,
      tadah::core::aed_type& /*daed_out*/,
      const double       /*scale=1*/) const {
    return false;
  }

  /** String-keyed form (see calc_drhoi_dtheta): decodes once, calls the
   *  enum virtual. */
  virtual bool calc_daed_dtheta(
      const std::string& theta_name,
      int                arg_pos,
      const tadah::core::rho_type& rhoi,
      const tadah::core::rho_type& drhoi,
      tadah::core::aed_type& daed_out,
      const double       scale=1) const {
    return calc_daed_dtheta(decode_dm_theta(theta_name), arg_pos,
                            rhoi, drhoi, daed_out, scale);
  }

  /** \brief Closed-form per-pair contribution to d(fd_ij)/dtheta — the
   *  many-body sibling of D2_Base::calc_dXijdri_dtheta. Walker calls
   *  this once per (atom i, neighbour j) pair AFTER:
   *   - calc_rho            has filled `rhoi[rfidx..]   = rho_i`,
   *   - calc_aed            has filled `rhoi[drfidx..]  = f[L,o] F'(rho_i)`,
   *   - calc_drhoi_dtheta   has filled `drhoi_dtheta[rfidx..]
   *                                       = drho_i/dtheta`.
   *
   *  Unlike the D2 sibling (which returns a single radial column to be
   *  axis-expanded by the walker), the MB descriptor's calc_dXijdri
   *  writes all three axis components directly because the per-orbital
   *  polynomial poly = x^lx * y^ly * z^lz couples to direction. This
   *  override mirrors that signature — it writes the full axis-expanded
   *  d(fd_ij)/dtheta into `fd_ij_out(row, 0..2)`.
   *
   *  Chain rule sketch (for DM_mEAD<F>, expanding path):
   *
   *     fd_ij(p, k) = scale * w_j * f[L,o] * F'(rho_i, p) * term_k,
   *       term_k = g * (dpoly/dr_k) + dg * poly * (v_k / r)
   *
   *  with g, dg the radial Gaussian and its r-derivative (both
   *  cutoff-weighted). Differentiating w.r.t. theta sees three groups:
   *
   *    1. Radial theta (CGRIDMB[k], SGRIDMB[k], RCUTMB) — touch (g, dg)
   *       directly AND rho via calc_drhoi_dtheta. The F'(rho) chain
   *       needs F''(rho) via F_Base::calc_d2F.
   *    2. Embedding theta (CEMBFUNC[i], SEMBFUNC[i]) — touch F'
   *       directly via F_Base::calc_dFp_dtheta; rho is fixed.
   *    3. Cross-cutting RCUTENV (F_mEnv only) — same direct F' branch.
   *
   *  Calling convention mirrors calc_drhoi_dtheta / calc_daed_dtheta:
   *   - `theta_name` is the OPTIM key, upper-case.
   *   - `arg_pos` is 1-based for vector parameters, -1 for scalars.
   *   - `fd_ij_out` is the per-pair, axis-expanded buffer, pre-zeroed by
   *     the caller.
   *   - Returns `true` iff this call wrote anything into `fd_ij_out`.
   *
   *  Default returns false (descriptor doesn't own this theta on the
   *  force side; orchestrator routes to needs_fd). */
  virtual bool calc_dXijdri_dtheta(
      DMThetaKey         /*key*/,
      int                /*arg_pos*/,
      const int          /*Zi*/,
      const int          /*Zj*/,
      const double       /*rij*/,
      const double       /*rij_sq*/,
      const tadah::core::Vec3d& /*vec_ij*/,
      const tadah::core::rho_type& /*rhoi*/,
      const tadah::core::rho_type& /*drhoi_dtheta*/,
      tadah::core::fd_type& /*fd_ij_out*/,
      const double       /*scale=1*/) const {
    return false;
  }

  /** String-keyed form (see calc_drhoi_dtheta): decodes once, calls the
   *  enum virtual. */
  virtual bool calc_dXijdri_dtheta(
      const std::string& theta_name,
      int                arg_pos,
      const int          Zi,
      const int          Zj,
      const double       rij,
      const double       rij_sq,
      const tadah::core::Vec3d& vec_ij,
      const tadah::core::rho_type& rhoi,
      const tadah::core::rho_type& drhoi_dtheta,
      tadah::core::fd_type& fd_ij_out,
      const double       scale=1) const {
    return calc_dXijdri_dtheta(decode_dm_theta(theta_name), arg_pos,
                               Zi, Zj, rij, rij_sq, vec_ij,
                               rhoi, drhoi_dtheta, fd_ij_out, scale);
  }


  /** \brief This is just a convenient wrapper.
   */
  void calc_dXijdri(const int Zi, const int Zj, tadah::core::Vec3d &del, tadah::core::rho_type &rhoi, tadah::core::fd_type &fdij, double scale=1) const{
    double rsq = del*del;
    double r = sqrt(rsq);
    //(p->*func)(r,rsq,del,1.0,0.0,rhoi,fdij);
    (*this).calc_dXijdri(Zi, Zj,r,rsq,del,rhoi,fdij,scale);
  }

  /** \brief This is just a convenient wrapper.
   */
  void calc_rho_mb(const int Zi, const int Zj, tadah::core::Vec3d &del, tadah::core::rho_type &rhoi, double scale=1) const {
    double rsq = del*del;
    double r = sqrt(rsq);
    ((*this).calc_rho)(Zi, Zj,r,rsq,del,rhoi,scale);
  }
  /** \brief Return size of the density array */
  virtual size_t rhoi_size() const{
    return rhoisize;
  }
  /** \brief Return size of the derivative of the embedding energy array */
  virtual size_t rhoip_size() const{
    return rhoipsize > 0 ? rhoipsize : rhoisize;
    // return rhoipsize;
  }
  // virtual size_t size() {
  //   return s;
  // }
  virtual std::string label() const=0;

  virtual size_t get_rfidx() const;
  virtual void set_rfidx(size_t rfidx_);
  virtual size_t get_drfidx() const;
  virtual void set_drfidx(size_t drfidx_);
};
} // namespace models
} // namespace tadah
#endif
