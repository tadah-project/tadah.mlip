#ifndef DM_REAM_H
#define DM_REAM_H
#include <tadah/models/descriptors/dm/dm_base.h>
#include <tadah/models/descriptors/dm/ream_density.h>

namespace tadah {
namespace models {

/** \brief Refittable EAM many-body descriptor (flexible density + embedding).
 *
 * The native-Tadah counterpart of \ref DM_EAM, used by `tadah refit`. Where
 * DM_EAM reproduces the *tabulated* density and embedding via fixed cubic
 * splines, DM_REAM represents BOTH as flexible basis-function expansions so an
 * EAM model can be reshaped and re-optimised with the standard linear model and
 * HPO machinery.
 *
 * The EAM many-body energy is
 * \f[
 *   V_i = F\!\Big(\rho_i\Big), \qquad \rho_i = \sum_{j\neq i} \rho(r_{ij}).
 * \f]
 *
 * Because \f$F(\rho)\f$ with \f$\rho=\sum_j\rho(r)\f$ is *bilinear*, only one of
 * the two functions can carry the linear weights. DM_REAM therefore:
 *
 *  - represents the **density** as a fixed (baked) expansion
 *    \f$\rho(r)=W\sum_{n} a_n\,T_n(r)\,f_c(r)\f$ in the declared density
 *    form (below) with coefficients
 *    \f$a_n\f$ supplied in \ref AMPGRIDMB (these are NOT linear weights — they are
 *    found by a 1-D least-squares pre-fit and stored in the descriptor), and
 *  - represents the **embedding** as
 *    \f$F(\rho)=\sum_{m} c_m\,\tilde g_m(\rho)\f$ where the \f$\tilde g_m\f$
 *    are the descriptor output components (one per \ref CEMBFUNC entry) and
 *    the \f$c_m\f$ are the M_BLR **linear weights**.
 *
 * Density form (key \ref RHOFORM, a property of the potential; MANDATORY:
 * a context without it throws DensityFormError with the migration text
 * rhoform_absent_message()). With W = `weights[Zi][Zj]` (WATOM), f_c the
 * many-body cutoff, \f$\psi_n\f$ the basis function of centre n and
 * \f$S_n\f$ its integrated bump (d_basis_functions.h: 1 inside the bump's
 * inner support edge, 1/2 at its centre, 0 beyond its outer edge):
 *  - `RHOFORM SIGNED`: \f$\rho = W\sum_n a_n\,\psi_n f_c\f$ with signed
 *    \f$a_n\f$, bit for bit the density of every tadah before 1.6.0 in a
 *    MODELS compiled without FMA contraction (ReamDensity); no guarantee.
 *  - `RHOFORM NONNEG`: the same with every \f$a_n \ge 0\f$, W > 0 and f_c
 *    replaced by max(f_c, 0): \f$\rho \ge 0\f$ exactly. With the knot family,
 *    knots <= RCUTMB and a non-increasing cutoff (not Cut_SinInv) rho is
 *    also non-increasing and convex.
 *  - `RHOFORM MONOTONE`: \f$\rho = W\sum_n a_n\,S_n \max(f_c, 0)\f$ with
 *    \f$a_n \ge 0\f$, SGRIDMB > 0, W > 0, a bump family (not the knot
 *    family) and a whitelisted cutoff (rhoform_cutoff_whitelisted):
 *    \f$\rho \ge 0\f$ exactly and non-increasing to rounding, so the
 *    lattice density rhobar(a) is non-increasing on every lattice.
 *    \f$\partial\rho/\partial a_n = W S_n f_c\f$ is non-local: an
 *    amplitude moves the density everywhere inside its bump's outer edge.
 * The form is validated in the constructor (after the basis code, before
 * the embedding): amplitudes, centres and widths (every entry finite under
 * the positive forms), cutoff and the WATOM pair weights (read from the
 * context). The density itself is evaluated by one ReamDensity
 * (ream_density.h), which calc_rho, the force descriptors, eam_density and
 * pair_density all delegate to.
 *
 * Embedding reference (key \ref EMBF0, a property of the potential):
 *  - `EMBF0 ZERO` (default): \f$\tilde g_m(\rho) = g_m(\rho) - g_m(0)\f$, with
 *    \f$g_m(0)\f$ computed in the constructor from the
 *    CEMBFUNC/SEMBFUNC/KDEGEMB values actually evaluated. Hence
 *    \f$F(0) = 0\f$ exactly for any weights: an isolated atom has zero
 *    embedding energy by construction (exactly 0.0: rho == 0 short-circuits).
 *  - `EMBF0 FREE`: the raw basis \f$\tilde g_m = g_m\f$ of tadah 1.5.0 and
 *    earlier, where \f$F(0)=\sum_m c_m g_m(0)\f$.
 *  The derivatives \f$g'_m\f$ are identical in both modes, so for a model
 *  linear in the descriptor forces and stresses are identical and every
 *  atomic energy differs by the same constant.
 *
 * Keys (single element, \f$L=0\f$ scalar density):
 *  - \ref CGRIDMB / \ref SGRIDMB : density basis centres / widths (size Nρ)
 *  - \ref AMPGRIDMB                       : baked density coefficients \f$a_n\f$ (size Nρ)
 *  - \ref RHOFORM                         : density form SIGNED | NONNEG | MONOTONE
 *  - \ref CEMBFUNC / \ref SEMBFUNC      : embedding basis centres / widths (size M)
 *
 * `TYPEMB` layout (the d_base.h keys contract):
 * `DM_REAM <basis> <nC> <nS> <nAmp> <nEc> <nEs> <nKd> <nKe> <El> <El>`
 * — one family param (the basis code) followed by one SIZE token per keys
 * entry ({CGRIDMB, SGRIDMB, AMPGRIDMB, CEMBFUNC, SEMBFUNC}) and per
 * data_keys entry ({KDEGMB, KDEGEMB}), which makes DM_REAM splittable by
 * DM_mJoin and HPO-addressable via get_arg_pos. `<basis>` is `0` (Blip,
 * default), `1` (Gaussian), `2` (Knot5: one-sided quintic/cubic knot
 * functions) or `3` (Blip5: symmetric quintic B-spline bumps, C^4) and
 * selects the family used for BOTH the density and the embedding
 * expansions. Knot pots carry SIZE 0 for the width channels (SGRIDMB /
 * SEMBFUNC ABSENT — eta is a pure gauge) and use the KDEG* data lists for
 * per-atom degrees (3|5); bump pots carry SIZE 0 for the KDEG* channels.
 * The amplitudes AMPGRIDMB are excluded from the LINEAR solve by
 * bilinearity but are first-class NONLINEAR HPO parameters
 * (`OPTIM AMPGRIDMB`). For Knot5 the per-channel orientation is part of the family
 * definition: the radial density basis is LEFT-active, \f$\psi_n(r) =
 * (\mu_n - r)_+^5\f$ (vanishes C^4-smoothly at its knot, so with all knots
 * \f$\le r_{cut}\f$ the density vanishes smoothly at the cutoff), while the
 * embedding basis is RIGHT-active, \f$g_m(\rho) = (\rho - \mu_m)_+^5\f$
 * (F free at high density; F(0) = 0 by construction under EMBF0 ZERO, and
 * only knots with mu_m < 0 reach rho = 0 at all). SGRID* entries are a
 * pure gauge for this family and are conventionally 1.0.
 *
 * \note Like \ref D2_Blip / \ref DM_Blip, the per-pair density carries the
 *       symmetric WATOM weight `weights[Zi][Zj]` (1.0 for a single-element
 *       potential), so multi-element generalisation and HPO WATOM search work.
 */
class DM_REAM: public DM_Base {
private:
  std::string lab="DM_REAM";
  // Runtime-selected radial/embedding basis family (from the TYPEMB <basis>
  // wire code). BasisKind is the shared enum; the per-basis math is reached
  // through the single dispatcher in basis_dispatch.h — no scattered
  // if(basis==...) ladders live in this class any more.
  BasisKind basis = BasisKind::Blip;
  // density expansion (baked), in its RHOFORM: CGRIDMB, SGRIDMB, AMPGRIDMB,
  // KDEGMB. The one evaluator every density path delegates to.
  ReamDensity dens_;
  // embedding expansion (linear weights live in the model)
  tadah::core::v_type cemb;   // CEMBFUNC
  tadah::core::v_type semb;   // SEMBFUNC
  // optional per-atom degrees for the knot family (bcode 2): 3|5, empty = 5
  tadah::core::v_type demb_deg;  // KDEGEMB
  int emb_deg(std::size_t m) const {
    return m < demb_deg.size() ? static_cast<int>(demb_deg[m]) : 5;
  }

  /** Sum_n a_n T_n(r) f_c(r) (ReamDensity::value). */
  double rho_raw(double r, double fc) const;
  /** d/dr of rho_raw (ReamDensity::deriv). */
  double rho_raw_deriv(double r, double fc, double fcp) const;
  /** Embedding value g_m(rho) (no cutoff). */
  double gemb(std::size_t m, double rho) const;
  /** Embedding derivative g'_m(rho) w.r.t. rho. */
  double dgemb(std::size_t m, double rho) const;
  /** Embedding-grid validation (the density grids are validated by
   *  ReamDensity::validate): CEMBFUNC present, the knot family's width
   *  channel absence, CEMBFUNC/SEMBFUNC sizes, no zero SEMBFUNC, KDEGEMB
   *  size and entries in {3,5}. */
  static void validate_embedding(BasisKind b, const tadah::core::v_type &ce,
                                 const tadah::core::v_type &se,
                                 const tadah::core::v_type &de);
  /** Reads every grid of @p ctx into temporaries (basis code, RHOFORM,
   *  density, embedding, EMBF0), validates them (density form and WATOM
   *  pair weights included) and computes the embedding reference; the
   *  members change only after every check has passed (strong exception
   *  guarantee). Called by the constructor. */
  void load(const tadah::core::Context &ctx);

  // embedding reference (EMBF0): ZERO subtracts g_m(0) from every embedding
  // output; the two g0 arrays are filled only under ZERO, each with the
  // routine of the path that uses it (calc_aed: gemb; eam_embed: the fused
  // embedding_value_deriv), from the members actually evaluated.
  EmbF0Mode embf0_ = EmbF0Mode::Zero;
  tadah::core::v_type g0_aed_;   // g_m(0) via gemb()
  tadah::core::v_type g0_emb_;   // g_m(0) via embedding_value_deriv()
  /** g_m(0) of the embedding (@p b, @p ce, @p se, @p de) by each path's own
   *  routine: @p g0a as calc_aed evaluates it (gemb), @p g0e as eam_embed
   *  does (embedding_value_deriv). Used under EMBF0 ZERO; load() reads EMBF0
   *  through Context::get, which materialises the default ZERO into the
   *  model Context, so every potential written from it carries the key. */
  static void embedding_reference(BasisKind b, const tadah::core::v_type &ce,
                                  const tadah::core::v_type &se,
                                  const tadah::core::v_type &de,
                                  tadah::core::v_type &g0a,
                                  tadah::core::v_type &g0e);

  /** Last-resort guard (ctor), knot family only: a density knot
   *  beyond rcut is silently hard-truncated by Cut_Dummy — warn, once per
   *  process (log_warning_once). The
   *  honest fixes live at the command level (refit/edit/hpo derive RCUTMB
   *  from the largest knot); no auto-raise here because widening fcut
   *  without resizing the caller's neighbour list would not recover the
   *  missing pairs. */
  void warn_if_knots_beyond_rcut();

public:
  DM_REAM();
  DM_REAM(const tadah::core::Context&);
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
  void init() override;
  std::optional<DescriptorMeta> meta() const override;
  std::optional<BasisKind> basis_kind() const override { return basis; }
  std::size_t component_basis_size(const std::string &) const override;
  bool component_basis_curve(const std::string &,std::size_t,double,bool,double &,double &) const override;
  bool component_cutoff(const std::string &,double,double &,double &) const override;

  /* EAM collapse (see DM_Base): V_i = F(rho_i) exactly, so expose the scalar
   * density rho(r) (amp-baked, cutoff- and WATOM-weighted) and the
   * weight-collapsed embedding F(rho) = sum_m w_m g~_m(rho), with the same
   * EMBF0 reference as calc_aed (ZERO: g~ = g - g(0), F(0) = 0 exactly). */
  bool eam_form() const override { return true; }
  void eam_density(const int Zi, const int Zj, const double rij,
                   double &rho, double &rhop) const override;
  void eam_embed(const tadah::core::t_type &w_full, const double rho_bar,
                 double &F, double &Fp) const override;

  std::optional<EmbF0Mode> embf0_mode() const override { return embf0_; }
  std::optional<double>
  raw_embedding_f0(const tadah::core::t_type &w_full) const override;

  /** The density calc_rho adds for the pair (Zi, Zj) at distance @p rij at
   *  scale 1: exactly its increment `1.0 * W * rho_raw(rij, f_c(rij))` in a
   *  MODELS compiled without FMA contraction (ReamDensity), 0 for an
   *  uninitialised pair or rij > rcut. Not a DM_Base virtual (the plugin
   *  ABI is unchanged); for tests and holders of a DM_REAM. */
  double pair_density(const int Zi, const int Zj, const double rij) const;

  /** The density evaluator (form, grids) of this descriptor. */
  const ReamDensity &density() const { return dens_; }
};
} // namespace models
} // namespace tadah
#endif
