#ifndef TADAH_MODELS_EAM_REFIT_H
#define TADAH_MODELS_EAM_REFIT_H

#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/models/eam/eam_setfl_io.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/descriptors/dm/embed_kernel.h>

#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief Configuration for a `tadah refit` run. */
struct EAMRefitConfig {
  std::optional<EmbedSpec> component;
  bool fit_density_amplitudes = true;
  bool unified_output = false;
  std::string basis  = "Blip";       ///< "Blip", "Blip5", "Knot5" or "Gaussian"
                                     ///<  (Knot5 = one-sided quintic knot
                                     ///<   functions; compact mode only)
  std::string rctype = "Cut_Dummy";  ///< cutoff family (hard cut reproduces EAM)
  // The embedding F(rho) is evaluated at a single rho_i per atom, so its local
  // fit error feeds straight into the energy with no neighbour averaging — it
  // needs markedly more basis functions than phi(r)/rho(r) for a given energy
  // accuracy. Hence nF >> n2b, nrho by default.
  int n2b  = 44;     ///< number of two-body basis functions
  int nrho = 36;     ///< number of density basis functions
  int nF   = 100;    ///< number of embedding basis functions
  double r_lo_frac = 0.20;  ///< inner radial bound, as a fraction of rcut
  double lambda    = 0.0;   ///< optional Tikhonov damping for the fits
  int oversample   = 8;     ///< least-squares sample points per basis function

  // ---- basis placement (Cycle 1) ----
  // "window": uniform blip grids per region — the bulk of the basis on the
  //   physically REACHABLE window (pair distances / embedding densities a
  //   crystal can visit between strong compression and mild expansion), a
  //   small uniform allocation on the unreachable tail. Fixes the capacity
  //   dilution of eam/fs files whose F is tabulated far beyond reachable rho
  //   (measured: Ti compressed-error 267 meV -> few-meV class).
  // "uniform": legacy single uniform grid over the full range.
  // (A curvature-equidistributed placement was tried and ABANDONED: stretched
  //  nonuniform blips lose the approximation power of uniform translates —
  //  measured 49 meV ambient error vs 0.35 meV uniform. See cycle1 report.)
  std::string placement = "window";
  double reach_smin  = 0.75;   ///< reachability probe: min linear compression a/a0
                               ///<  (0.75 ~ V/V0 = 0.42, multi-hundred-GPa regime)
  double reach_margin = 1.25;  ///< safety margin on the reachable rho window
  bool constrain_F0  = true;   ///< pin F(0) to the table's value (isolated-atom ref);
                               ///<  FREE only (exact by construction under ZERO)
  /// EMBF0 ZERO: fit and evaluate the zero-referenced embedding
  /// g~_m(rho) = g_m(rho) - g_m(0) against F_ref = F_tab - F_tab(0), so
  /// F(0) = 0 exactly (the DM_REAM default). false = EMBF0 FREE: the raw basis
  /// against the raw table, bit-identical to tadah 1.5.0.
  bool embf0_zero = true;
  /// RHOFORM: the density form the density amplitudes are fitted in and the
  /// written potential declares (dense fit, rho_fit, rho_setfl). SIGNED: the
  /// signed expansion sum_n a_n psi_n by GELSD, bit-identical to the fit
  /// before RHOFORM existed.
  /// NONNEG: the same columns by non-negative least squares (every a_n >= 0).
  /// MONOTONE: the integrated-bump steps S_n by non-negative least squares
  /// (bump families only; the cutoff rctype must be whitelisted). The
  /// positive forms refuse a table the form cannot follow (see fit()).
  RhoForm rho_form = RhoForm::Signed;
};

/** \brief Goodness-of-fit statistics for one 1-D fit. */
struct FitStats {
  double rmse   = 0.0;
  double mae    = 0.0;
  double maxabs = 0.0;
  std::size_t n = 0;
};

/** \brief Fit a tabulated EAM potential into a native Tadah M_BLR model.
 *
 * Performs three independent 1-D least-squares expansions:
 *   1. pair  \f$\phi(r)=z2r/r \to \sum_k c^{2b}_k B_k(r)\f$  (linear weights),
 *   2. density \f$\rho(r) \to \sum_n a_n T_n(r)\f$       (baked coefficients;
 *      T_n = \f$\psi_n\f$ under RHOFORM SIGNED/NONNEG, the integrated bump S_n
 *      under MONOTONE; every a_n >= 0 under NONNEG/MONOTONE),
 *   3. embedding \f$F(\rho) \to \sum_m c^{mb}_m g_m(\rho)\f$ (linear weights),
 * then assembles a standard `pot.tadah` Context (D2_Blip + DM_REAM) whose
 * WEIGHTS are `[c^{2b} | c^{mb}]`.
 *
 * Under EMBF0 ZERO (EAMRefitConfig::embf0_zero) the embedding is fitted and
 * evaluated in the zero-referenced basis \f$g_m(\rho)-g_m(0)\f$ against the
 * zero-referenced table F_ref, exactly as DM_REAM evaluates it.
 *
 * The density is fitted, evaluated (rho_fit, rho_setfl, the stats) and
 * written (RHOFORM) in the form EAMRefitConfig::rho_form, so rho_fit(r)
 * equals the pair density of the DM_REAM built from build_pot_context() for
 * r < rcut (Cut_Dummy).
 */
class EAMRefit {
public:
  /** `eam` must have its splines built (EAMSetflIO::build_splines()).
   *  RHOFORM MONOTONE with the knot family, or with a cutoff rctype that
   *  rhoform_cutoff_whitelisted() refuses, throws DensityFormError. */
  EAMRefit(const EAMSetflIO &eam, const EAMRefitConfig &cfg);

  /** EMBF0 ZERO (true) or FREE (false), from EAMRefitConfig::embf0_zero. */
  bool embf0_zero() const { return cfg_.embf0_zero; }

  /** The density form (EAMRefitConfig::rho_form). */
  RhoForm rho_form() const { return cfg_.rho_form; }

  /** The table embedding in the reference the native F represents: FREE ->
   *  eam.F_tab(rho) verbatim; ZERO -> eam.F_tab(rho) - eam.F_tab(0), exactly 0
   *  at rho = 0. The table itself is never modified (DM_EAM reads it). */
  double F_ref(double rho) const;

  /** Basis family, parsed from EAMRefitConfig::basis into the shared BasisKind
   *  enum. For Knot5 the orientation is per channel: phi/rho LEFT-active
   *  (mu - x)_+^5 (vanish C^4-smoothly at the cutoff), embedding F RIGHT-active
   *  (x - mu)_+^5 — exactly the LEFT/RIGHT split the shared dispatcher applies. */
  BasisKind family() const { return family_; }

  /** Run the three least-squares fits. Under RHOFORM NONNEG and MONOTONE
   *  the tabulated density is checked on the fit window first (the table
   *  nodes r_i in [fit_window_r_lo(), rcut], the window of the two-stage
   *  solve, never below r_lo, under which no fit samples the table): a node
   *  value below -tol (NONNEG and MONOTONE) or a rise rho_tab(r_{i+1}) -
   *  rho_tab(r_i) > tol (MONOTONE) throws std::runtime_error naming r,
   *  tol = 8 eps max|rho_tab| on the window, since no fit in the form can
   *  follow such a table there. The samples and the grid start at r_lo:
   *  with a detected wall the window starts above r_lo, and the samples in
   *  [r_lo, fit_window_r_lo()) are fitted in the tail block (in the form,
   *  but outside the window's stats) and not checked. */
  void fit();

  /** Reachability analysis only (fit() also performs it): computes r_reach /
   *  rho_reach from the header lattice. Compact mode needs the windows BEFORE
   *  fitting. */
  void analyze_reachability();

  /** The RHOFORM NONNEG/MONOTONE table pre-check fit() runs on its fit
   *  window [win_lo, rcut] (see fit(); std::runtime_error naming r); SIGNED:
   *  no-op. Public for fitters that inject their own expansion (compact
   *  mode), which must refuse the same tables. */
  void check_rho_table(double win_lo) const;

  // ---- compact-mode injection (parameters optimised externally, e.g. the
  //      HPO ladder + Ceres fitter) ----
  enum class Fn { PHI, RHO, F };
  /** Inject an externally optimised blip expansion for one function.
   *  `deg` (knot family only): per-atom degree 5|3; empty = all quintic.
   *  Fn::RHO under RHOFORM NONNEG/MONOTONE: `c` holds the amplitudes of the
   *  form's own terms (psi_n or S_n) and must be finite and >= 0
   *  (DensityFormError otherwise). */
  void set_blip_fit(Fn which, const std::vector<double> &mu,
                    const std::vector<double> &eta,
                    const std::vector<double> &c,
                    const std::vector<int> &deg = {});
  /** Rebuild WEIGHTS and the per-function stats after injections. Stats are
   *  evaluated on [rwin_lo, rcut] (phi, rho) and [0, rho_win_hi] (F, against
   *  F_ref), the windows of the injected fit, which fit_window_r_lo() and
   *  fit_window_rho_hi() then return. */
  void finalize_injected(double rwin_lo, double rho_win_hi);

  // Native fitted functions (cutoff-weighted; ready for the setfl writer).
  double phi_fit(double r) const;     ///< pair function phi(r)
  /// density rho(r) in the declared form: sum_n a_n T_n(r) times the hard
  /// cutoff, summed in ascending n (DM_REAM's order, so for r < rcut it is
  /// the pair density of the build_pot_context() potential)
  double rho_fit(double r) const;
  double F_fit(double rho) const;     ///< embedding F(rho) (ZERO: F_fit(0) == 0.0)

  // Tabulation variants for the round-trip setfl: identical to *_fit from
  // the inner fit bound r_lo up; below it the ORIGINAL table, shifted by the
  // constant that makes it continuous with the fit at r_lo (C0), replaces
  // the blip tails, which decay below the fit window and would leave an
  // attractive hole at extreme compression. The native pot.tadah carries
  // only the blips — its validity domain starts at r_lo.
  /** phi_fit from r_lo up; below it phi_tab(r) + (phi_fit - phi_tab)(r_lo)
   *  (z2_setfl(r) / r; 0 for r <= 1e-12). */
  double phi_setfl(double r) const;
  /** rho_fit above r_lo; below it the original table offset-matched at r_lo,
   *  clamped at 0 under RHOFORM NONNEG/MONOTONE (the tabulated continuation
   *  keeps rho >= 0; the form's guarantees are those of rho_fit, r >= r_lo). */
  double rho_setfl(double r) const;
  /** z2r(r) = phi_setfl(r)*r for the setfl writer: phi_fit(r) r from r_lo
   *  up; below it z2_tab(r) + (phi_fit - phi_tab)(r_lo) r, finite at r = 0
   *  (the offset works on z2r, since phi diverges as z2r(0)/r). */
  double z2_setfl(double r) const;
  /** F for the setfl writer: F_fit inside [0, rho_join], the ORIGINAL table
   *  (offset-matched at the join) beyond it. Dense mode: rho_join = rhomax,
   *  identical to F_fit; compact mode: rho_join = the reachable-window edge
   *  (the blips only cover the window — beyond it they decay to garbage).
   *  EMBF0 ZERO: F_setfl(0) == F_fit(0) == 0.0, and F_tab(0) cancels in the
   *  join offset, so the continuation follows F_ref. */
  double F_setfl(double rho) const;
  void set_rho_join(double rj) { rho_join_ = rj; }
  double rho_join() const { return rho_join_; }
  double r_lo() const { return r_lo_; }

  /** Assemble the M_BLR pot.tadah Context. */
  tadah::core::Context build_pot_context() const;

  const FitStats &phi_stats() const { return phi_stats_; }
  const FitStats &rho_stats() const { return rho_stats_; }
  const FitStats &F_stats()   const { return F_stats_; }

  /** Concatenated linear weights `[c2b | cmb]` in pot.tadah column order. */
  const tadah::core::v_type &weights() const { return weights_; }

  double rcut() const { return rcut_; }

  // Reachability diagnostics (valid after fit(); 0 when placement=="uniform").
  double r_reach()   const { return r_reach_; }    ///< min relevant pair distance
  double rho_reach() const { return rho_reach_; }  ///< max relevant density
  bool lattice_known() const { return lattice_known_; }

  /** The windows the fit used: phi and rho were fitted on
   *  [fit_window_r_lo(), rcut()], F on [0, fit_window_rho_hi()]. fit():
   *  the radial window of its two-stage solve, min(max(r_lo, r_K),
   *  0.8 r_reach) under placement "window" (r_K the largest r of a wall
   *  above 30x the reachable scale), r_lo under "uniform", and never below
   *  r_lo, where its samples and its grid start; the embedding window
   *  rho_reach (window) or rhomax (uniform). finalize_injected(): its two
   *  arguments, the windows of the injected fit. 0 before either. */
  double fit_window_r_lo() const { return fit_r_lo_; }
  double fit_window_rho_hi() const { return fit_rho_hi_; }  ///< see fit_window_r_lo()

private:
  void fit_components(double rwin_lo);
  std::unique_ptr<EmbedEvaluator> component_evaluator_;
  std::unique_ptr<Cut_Base> spatial_cutoff_;
  /** Evaluate one basis function. `emb` selects the embedding-channel
   *  orientation (right-active) for the Knot5 family; ignored for the
   *  symmetric Blip/Gaussian families. */
  double basisval(double x, double center, double width,
                  bool emb = false, int deg = 5) const;
  /** `radial`: the grid feeds a radial channel whose upper edge `hi` is the
   *  hard rcut — for the compact bump families the spacing switches to
   *  dx = (hi-lo)/(n-1+w) so the OUTERMOST bump's support ends exactly at
   *  `hi` (uniform-translate structure preserved; no truncated tail at the
   *  cutoff). false for the embedding channel (no r cutoff). */
  void make_grid(double lo, double hi, int n,
                 tadah::core::v_type &centers,
                 tadah::core::v_type &widths, bool radial) const;
  /** Uniform sub-grid on the reachable window [win_lo,win_hi] (>= 2/3 of the
   *  basis) + small uniform allocations on the unreachable tail(s). */
  void make_grid_windowed(double lo, double hi, int n,
                          double win_lo, double win_hi,
                          tadah::core::v_type &centers,
                          tadah::core::v_type &widths, bool radial) const;
  /** Two-stage block solve: stage A fits the samples inside [win_lo,win_hi]
   *  with the blips supported there (unweighted — full quality); stage B fits
   *  the remaining samples with the remaining blips, stage-A contribution
   *  subtracted (crude but sane tail, no scale competition with the window).
   *  Optional pin: one heavily-weighted row y(pin_x)=pin_y in stage A.
   *  `zero_ref` (EMBF0 ZERO, embedding channel): every value use (pin row
   *  included) takes the zero-referenced column b_k(x) - b_k(0); fit() then
   *  passes no pin (the F(0) row would be identically zero).
   *  `form` (density channel; Signed for phi and F): Signed solves every
   *  block by GELSD (as before RHOFORM, bit for bit); NonNeg solves it by
   *  non-negative least squares on the same columns; Monotone by
   *  non-negative least squares on the integrated-bump step columns S_k
   *  (whose support (-inf, c_k + half] joins the window block whenever it
   *  reaches win_lo). LAMBDA applies in every form.
   *  Returned stats cover the window samples only. */
  FitStats lsq_fit_windowed(const std::vector<double> &xs,
                            const std::vector<double> &ys,
                            double win_lo, double win_hi,
                            const tadah::core::v_type &centers,
                            const tadah::core::v_type &widths,
                            bool apply_cutoff,
                            tadah::core::v_type &coeffs,
                            bool pin, double pin_x, double pin_y,
                            bool zero_ref = false,
                            RhoForm form = RhoForm::Signed) const;
  /** Density term n of the fitted expansion without the cutoff: psi_n
   *  (SIGNED, NONNEG) or the integrated bump S_n (MONOTONE). */
  double rho_term(double r, std::size_t n) const;
  /** EMBF0 ZERO: recompute g0F_ = g_m(0) with F_fit's evaluator. Called
   *  whenever the F basis changes (fit(), set_blip_fit(Fn::F)); clears it
   *  under FREE. */
  void refresh_embedding_reference();
  double cutoff(double r) const;

  const EAMSetflIO &eam_;
  EAMRefitConfig cfg_;
  BasisKind family_ = BasisKind::Blip;
  double rcut_, r_lo_, rhomax_;
  double rho_join_ = 0.0;  ///< F tabulation join (ctor sets rhomax)
  double r_reach_ = 0.0, rho_reach_ = 0.0;
  double fit_r_lo_ = 0.0, fit_rho_hi_ = 0.0;  ///< the windows the fit used
  bool lattice_known_ = false;
  double F0tab_ = 0.0;     ///< EMBF0 ZERO: eam.F_tab(0) (ctor; unused under FREE)

  tadah::core::v_type cgrid2b_, sgrid2b_, c2b_;   // two-body
  tadah::core::v_type crho_, srho_, arho_;        // density (baked)
  tadah::core::v_type cF_, sF_, cmb_;             // embedding (linear)
  tadah::core::v_type g0F_;                       // ZERO: g_m(0); empty (FREE)
  std::vector<int> deg2b_, degrho_, degF_;        // knot degrees (empty = all 5)
  tadah::core::v_type weights_;                   // [c2b | cmb]

  FitStats phi_stats_, rho_stats_, F_stats_;
};

} // namespace models
} // namespace tadah
#endif // TADAH_MODELS_EAM_REFIT_H
