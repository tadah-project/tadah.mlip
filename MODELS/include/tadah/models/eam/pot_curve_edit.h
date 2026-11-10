#ifndef TADAH_MODELS_EAM_POT_CURVE_EDIT_H
#define TADAH_MODELS_EAM_POT_CURVE_EDIT_H

// Editing primitives for EXISTING `tadah refit` potentials (pot.tadah with a
// D2_Knot5 / D2_Blip / D2_Blip5 two-body descriptor and a DM_REAM many-body
// descriptor): parse the three EAM curves into per-channel views, add or
// remove ONE basis function, and rebuild a consistent potential Context
// (grids, TYPE2B/TYPEMB count tokens, WEIGHTS/FIXWEIGHT/FIXINDEX, RCUT).
//
// Deliberately self-contained over the shared basis primitives
// (d_basis_functions.h via basis_dispatch.h) so it works in plain MLIP
// builds — it does NOT reuse HPO's Ceres-gated BlipFit (which also carries an
// incompatible BasisKind enum).
//
// RCUT contract (family-aware; the load-bearing asymmetry):
// - Knot channels: the basis vanishes C^4-smoothly at its own largest knot,
//   so the TRUE effective cutoff IS max(knot centre) and RCUT2B/RCUTMB are
//   DERIVED quantities — `derived_knot_rcut` computes them and
//   `normalize_knot_rcut` rewrites any stated value that differs (warning
//   when the gap is real, or under a smooth cutoff, where pair_style tadah
//   still reads the stated value). Moving RCUT anywhere >= max knot is
//   prediction-exact under a hard cutoff only: a smooth cutoff's f_c(r)
//   depends on RCUT for every pair.
// - Blip channels: RCUT is AUTHORITATIVE and is never auto-changed. Under a
//   discontinuous cutoff (Cut_Dummy) a bump whose support crosses RCUT is
//   hard-truncated mid-tail -> phi(RCUT) != 0 -> an energy/force step at the
//   cutoff, so AUTHORING surfaces (hpo preflight, refit's optimizer stages,
//   the hpotarget emitter, --edit add) enforce `c + w/eta <= RCUT` as a hard
//   error (`bump_channel_halfwidth` + bump_rcut_tol). READING an existing
//   fitted pot stays warn-only: legacy spilled pots exist and their
//   truncation is part of the fitted model. Smooth cutoffs (Cut_Poly, ...)
//   damp any overhang to zero smoothly, so overhang there is legitimate and
//   is not gated (`channel_cutoff_is_discontinuous`).

#include <cstddef>
#include <optional>
#include <memory>
#include <tadah/models/cutoffs.h>
#include <string>
#include <utility>
#include <vector>

#include <tadah/core/context.h>
#include <tadah/core/logger.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/rhoform.h>

namespace tadah {
namespace models {
namespace potedit {

enum class Channel { Phi, Rho, F };

/** \brief Inner sampling bound of the removal-compensation window as a
 *  fraction of rcut. The pot file does not carry the fit's own r_lo, so this
 *  mirrors the inspection domain used by the refit curve dumps. Heuristic —
 *  documented in refit.rst. */
constexpr double kCompensationRLoFrac = 0.15;

/** \brief One parsed EAM curve: basis family + parallel per-BF arrays.
 *
 * Invariants: mu/coeff always sized n(); eta sized n() for bump families and
 * EMPTY for knots (eta = 1 gauge); deg EMPTY (= all quintic) or sized n().
 * coeff maps to the WEIGHTS 2b block (Phi), AMPGRIDMB (Rho) or the WEIGHTS
 * embedding block (F). rcut > 0 only for the radial channels.
 */
struct CurveView {
  BasisKind family = BasisKind::Blip;
  bool right_active = false;  ///< true only for F with the knot family
  /// true only for F of an EMBF0 ZERO potential: the zero-referenced
  /// embedding basis g_k(x) - g_k(0) that DM_REAM evaluates (see value())
  bool zero_ref = false;
  /// Rho of a DM_REAM potential: its RHOFORM (Signed for Phi and F). Under
  /// Monotone basis function k is the integrated bump S_k (value, deriv,
  /// deriv2, support); under NonNeg and Monotone every coeff is >= 0 and
  /// the edits keep it so.
  RhoForm form = RhoForm::Signed;
  std::vector<double> mu;
  std::vector<double> eta;    ///< empty for the knot family
  std::vector<double> coeff;
  std::vector<int> deg;       ///< empty = all quintic
  double rcut = 0.0;          ///< RCUT2B/RCUTMB; 0 for F (no r cutoff)
  std::shared_ptr<Cut_Base> spatial_cutoff; ///< immutable parsed radial taper

  std::size_t n() const { return mu.size(); }
  int degree(std::size_t k) const {
    return deg.empty() ? 5 : deg[k];
  }
  double width(std::size_t k) const {  ///< eta of BF k (1 for knots)
    return eta.empty() ? 1.0 : eta[k];
  }

  /// Curve value/derivatives at x: sum_k coeff_k g_k(x). Radial channels
  /// apply the configured spatial taper and its product derivatives.
  /// With zero_ref the value is sum_k coeff_k (g_k(x) - g_k(0)), term by
  /// term as DM_REAM::calc_aed forms it, and exactly 0 at x == 0; the
  /// derivatives are the same in both embedding references.
  double value(double x) const;
  double deriv(double x) const;
  double deriv2(double x) const;

  /// Active x-interval of BF k. One-sided knots return a half-line encoded
  /// with +-support_infinity() on the open side. With zero_ref, BF k is the
  /// constant -g_k(0) outside this interval (its derivatives vanish). Under
  /// RHOFORM MONOTONE the step S_k is 1 below the bump: (-inf, mu + w/eta).
  std::pair<double, double> support(std::size_t k) const;
  static double support_infinity();
};

/** \brief The many-body situation of a parsed potential. */
enum class MBKind {
  None,     ///< pure two-body (no active many-body term): WEIGHTS = pair block
  Ream,     ///< DM_REAM: density (baked amps) + embedding (WEIGHTS tail)
  Foreign,  ///< some other active many-body descriptor: its WEIGHTS tail is
            ///< preserved OPAQUELY; only the pair channel can be edited
};

/** \brief A knot/blip potential parsed into per-channel views + retained
 *  Context. The pair channel is always present; rho/F are populated only
 *  for MBKind::Ream. For MBKind::Foreign the non-pair WEIGHTS block is kept
 *  verbatim in `tail` (the design matrix is [bias | pair | many-body], so
 *  with BIAS false the pair block is the WEIGHTS prefix for ANY many-body
 *  descriptor — see DC_Selector::init). */
struct PotEAMView {
  core::Context ctx;  ///< the parsed original; untouched keys preserved
  CurveView phi, rho, F;
  MBKind mb = MBKind::None;
  std::string mb_name;        ///< TYPEMB descriptor name (Foreign diagnostics)
  std::vector<double> tail;   ///< opaque non-pair WEIGHTS (Foreign only)
  std::string element;
  /// MBKind::Ream only: the embedding reference (stored EMBF0; absent ->
  /// the default ZERO). F.zero_ref is (embf0 == EmbF0Mode::Zero).
  EmbF0Mode embf0 = EmbF0Mode::Zero;
  // The density form (RHOFORM, mandatory for DM_REAM) is rho.form.
};

/** \brief Parse a knot/blip potential Context into per-channel views.
 *  Requirements: TYPE2B is a refit family (D2_Knot5 / D2_Blip / D2_Blip5),
 *  BIAS and NORM are false (intercept / normalization columns are aligned
 *  with WEIGHTS and cannot survive an edit). The many-body term is optional:
 *  absent/inactive (pure two-body), DM_REAM (fully editable), or any other
 *  descriptor (pair channel only). For DM_REAM the stored EMBF0 (absent ->
 *  ZERO) sets `embf0` and F.zero_ref, and the stored RHOFORM sets rho.form,
 *  so the F and Rho views evaluate what DM_REAM evaluates. A potential with
 *  a DM_REAM block (also inside a foreign DM_mJoin) must declare RHOFORM:
 *  absent or invalid -> DensityFormError with the migration text
 *  (require_rhoform). Throws std::runtime_error with an actionable message
 *  otherwise (D2_mJoin, Gaussian/D2_BP, inconsistent arrays, invalid
 *  EMBF0). */
PotEAMView parse_pot_view(const core::Context &ctx);

/** \brief Rebuild a full potential Context from an (edited) view: grids,
 *  KDEG / SGRID / AMPGRIDMB arrays, TYPE2B/TYPEMB count tokens, WEIGHTS =
 *  [phi|F], FIXINDEX "1-K" + FIXWEIGHT, RCUT2B/RCUTMB (derived for knot
 *  channels, preserved for bump channels) and, for DM_REAM, explicit EMBF0
 *  (`embf0`) and RHOFORM (rho.form, canonical upper case) lines; any other
 *  potential goes through normalise_embf0_key / normalise_rhoform_key (a
 *  stray key is dropped). Self-checks by constructing the D2/DM descriptors
 *  from the result before returning. */
core::Context rebuild_pot_context(const PotEAMView &v);

/** \brief Effective cutoff of a knot channel, from the pot Context alone.
 *  suffix is "2B" (TYPE2B family + CGRID2B) or "MB" (DM_REAM bcode +
 *  CGRIDMB). Returns max knot centre for knot channels; std::nullopt for
 *  bump families or when the channel keys are absent/non-refit. */
std::optional<double> derived_knot_rcut(const core::Context &ctx,
                                        const std::string &suffix);

/** \brief F2 normalisation of a configuration to fit (train, hpo startup,
 *  analysis check_bounds; a potential evaluated with its coefficients takes
 *  normalize_potential_knot_rcut): for each KNOT channel compare the stated
 *  RCUT2B/RCUTMB against `derived_knot_rcut` and, whenever they differ at
 *  all (exact comparison), rewrite the key in ctx (RCUT*MAX follows via
 *  Context internal-key derivation), so every consumer uses the same
 *  cutoff. A difference above 1e-9 relative is logged as a WARNING, a
 *  smaller one (e.g. the ~3e-13 gap of <= 1.5.0 knot pots) at INFO; both
 *  prefixed with `who`. Under a smooth cutoff (not
 *  `channel_cutoff_is_discontinuous`) any difference is a WARNING that
 *  also says pair_style tadah reads the stated value (LAMMPS then differs
 *  from train/predict/hpo on that file until it is re-written). On a
 *  D2_mJoin or DM_mJoin line each knot descriptor's own RCUT value is
 *  derived from its own knots (D_mJoin::parse_context) and the message
 *  names it ("TYPE2B descriptor 1 (D2_Knot5): "), without the pair_style
 *  note (tadah predict, tadah refit --edit and LAMMPS take the cutoffs a
 *  potential file states for a joined line). Bump channels are never
 *  modified. Returns true if anything changed. */
bool normalize_knot_rcut(core::Context &ctx, const std::string &who);

/** \brief One message of normalize_knot_rcut: its level (WARNING or INFO)
 *  and its text, which the logging form prints. */
struct KnotRcutNote {
  core::LogLevel level = core::LogLevel::INFO;
  std::string text;
};

/** \brief normalize_knot_rcut that appends its messages to @p notes instead
 *  of logging them, for a caller that reports them itself (tadah hpo writes
 *  them to its journal). */
bool normalize_knot_rcut(core::Context &ctx, const std::string &who,
                         std::vector<KnotRcutNote> &notes);

/** \brief normalize_knot_rcut for a potential that is evaluated with its
 *  coefficients (tadah predict, tadah refit --edit): the cutoff of a line
 *  of one knot descriptor is rewritten as normalize_knot_rcut rewrites it
 *  (every fit of such a line derives it, so its coefficients were fitted
 *  with the derived value), and the stated cutoffs of a D2_mJoin or
 *  DM_mJoin line are kept: its coefficients were fitted with them (every
 *  fit before Tadah! 1.6.0 kept them; a fit now writes the derived ones),
 *  and pair_style tadah reads them. Logs as the two-argument form; returns
 *  true if anything changed. */
bool normalize_potential_knot_rcut(core::Context &ctx, const std::string &who);

/** \brief Support half-width w of a BUMP channel, from the pot Context alone:
 *  bump k spans [c_k - w/eta_k, c_k + w/eta_k]. suffix is "2B" (TYPE2B
 *  D2_Blip -> 2, D2_Blip5 -> 3) or "MB" (DM_REAM density, bcode 0 -> 2,
 *  bcode 3 -> 3). std::nullopt for knot/Gaussian channels or when the channel
 *  keys are absent/non-refit (malformed channels are not policed here). The
 *  Context-level twin of `derived_knot_rcut` for the bump families. */
std::optional<double> bump_channel_halfwidth(const core::Context &ctx,
                                             const std::string &suffix);

/** \brief True when the channel's cutoff function is a hard step
 *  (Cut_Base::Kind::Discontinuous, i.e. Cut_Dummy). RCTYPE{suffix} absent
 *  matches the D2_Base/DM_Base constructor default (Cut_Dummy) -> true; an
 *  unknown cutoff name is conservatively treated as discontinuous. Gates the
 *  bump-support-vs-RCUT hard errors: smooth cutoffs damp overhang to zero at
 *  RCUT, so overhang there is legitimate. */
bool channel_cutoff_is_discontinuous(const core::Context &ctx,
                                     const std::string &suffix);

struct AddSpec {
  double centre = 0.0;
  std::optional<double> eta;  ///< bump families only
  std::optional<int> deg;     ///< knot family only: 3|5 (default 5)
};

struct AddReport {
  std::size_t index = 0;   ///< position of the new BF in the arrays (appended)
  double eta_used = 1.0;
  int deg_used = 5;
  std::vector<std::string> warnings;
};

/** \brief Insert one basis function with coefficient 0 (curve, derivatives
 *  and extrapolation bit-identical until the coefficient is varied; with
 *  zero_ref too, as 0 * (g(x) - g(0)) is +-0; 0 is a valid amplitude under
 *  RHOFORM NONNEG and MONOTONE).
 *  `radial` selects the validation rules (centre > 0, rcut interplay).
 *  Throws on invalid spec (eta on knots, deg on bumps, bad centre, bump
 *  support entirely beyond rcut). With `hard_cutoff` (the Cut_Dummy default;
 *  callers with a pot Context pass `channel_cutoff_is_discontinuous`), a bump
 *  support merely CROSSING rcut is also an error — the overhang would be
 *  hard-truncated into an energy step at the cutoff; under a smooth cutoff it
 *  is only warned about. Under RHOFORM MONOTONE the step's outer edge
 *  mu + w/eta is checked the same way, with messages that describe a step:
 *  a step whose bump lies beyond rcut is not refused, as it is 1 on the
 *  whole range (under a smooth cutoff it adds a f_c(r), warned). */
AddReport add_basis(CurveView &c, const AddSpec &s, bool radial,
                    bool hard_cutoff = true);

struct RemoveReport {
  std::size_t removed_idx = 0;
  double removed_mu = 0.0;
  double removed_coeff = 0.0;
  std::vector<std::size_t> adjusted;  ///< compensator indices (post-removal order NOT applied; indices refer to the pre-removal arrays)
  std::vector<double> delta_c;
  double resid_plain = 0.0;  ///< stacked value/d1/d2 residual, no compensation
  double resid_comp = 0.0;   ///< same after compensation (<= resid_plain)
  double vresid_plain = 0.0; ///< value-only RMS over the affected region
  double vresid_comp = 0.0;
  bool compensated = false;
  bool region_widened = false;  ///< zero_ref: region widened to the F domain
  std::vector<std::string> warnings;
  std::vector<std::string> notes;  ///< informational (e.g. region_widened)
};

/** \brief Remove the basis function whose centre is nearest centre_request
 *  (ties -> lower index). Unless its coefficient is exactly 0, the (up to)
 *  two remaining nearest-in-centre coefficients are least-squares adjusted
 *  against the removed contribution's value + 1st + 2nd derivative on a
 *  dense sample of the affected region (per-order rows normalised by the
 *  removed term's RMS so the orders are balanced). With zero_ref (F only)
 *  the value rows use g(x) - g(0), and when the removed function has
 *  g(0) != 0 its removal also shifts F by a constant beyond its support, so
 *  the region is widened to [0, largest upper support of all functions]
 *  (region_widened + a note). Under RHOFORM NONNEG / MONOTONE (Rho) the
 *  adjustment is the bounded least-squares solution with every
 *  c_j + dc_j >= 0 (the free/at-bound active sets of the <= 2 compensators
 *  are enumerated; a note names a compensator held at 0; a compensator with
 *  no support in the affected region stays unchanged, and with none left
 *  the removal is plain, with the SIGNED warning); under MONOTONE the
 *  functions are the steps S_k and the affected region runs from the inner
 *  sampling bound to the removed step's outer edge. Throws when the channel
 *  has a single basis function. */
RemoveReport remove_basis(CurveView &c, double centre_request, bool radial);

}  // namespace potedit
}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_EAM_POT_CURVE_EDIT_H
