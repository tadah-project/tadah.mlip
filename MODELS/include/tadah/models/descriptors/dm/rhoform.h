#ifndef TADAH_MODELS_RHOFORM_H
#define TADAH_MODELS_RHOFORM_H

#include <tadah/core/context.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace tadah {
namespace models {

/** \brief DM_REAM density form, selected by the mandatory RHOFORM key.
 *
 *  With W the WATOM pair weight, f_c the many-body cutoff and psi_n the
 *  density basis function:
 *  - Signed: rho = W f_c sum_n a_n psi_n with amplitudes of any sign, the
 *    density of every tadah before 2.0.0 (bit for bit in a MODELS compiled
 *    without FMA contraction: ReamDensity).
 *  - NonNeg: the same expression with every a_n >= 0, W > 0 and the cutoff
 *    clamped at max(f_c, 0): rho >= 0 exactly.
 *  - Monotone: rho = W max(f_c, 0) sum_n a_n S_n, S_n the integrated bump
 *    (1 inside the inner support edge, 0 beyond the outer edge), a_n >= 0,
 *    s_n > 0, W > 0: rho >= 0 exactly and non-increasing to rounding. Bump
 *    families only, with a whitelisted cutoff (rhoform_cutoff_whitelisted).
 *
 *  Key-handling contract: RHOFORM has no blueprint default and is mandatory
 *  on every DM_REAM context. Every reader uses stored_rhoform() /
 *  require_rhoform(), which read through Context::stored_value only, so an
 *  absent key never materialises anything and never reaches Context::get
 *  (which would log a spurious ERROR before throwing). */
enum class RhoForm { Signed, NonNeg, Monotone };

/** \brief Thrown by every density-form and density validation (the RHOFORM
 *  key, the DM_REAM density grids, the density pair weights).
 *
 *  A distinct type so that code which probes a model by constructing it and
 *  swallows the construction errors (HPO startup probes) can rethrow it. */
class DensityFormError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/** "SIGNED" | "NONNEG" | "MONOTONE". */
const char *rhoform_name(RhoForm f);

/** True for NonNeg and Monotone, the forms that guarantee rho >= 0. */
bool rhoform_is_positive(RhoForm f);

/** Case-insensitive parse; throws DensityFormError for anything other than
 *  SIGNED, NONNEG or MONOTONE. */
RhoForm parse_rhoform(const std::string &value);

/** The migration text for a DM_REAM context without RHOFORM ("DM_REAM:
 *  RHOFORM is required: SIGNED (...), NONNEG (...) or MONOTONE (...)."). One
 *  text shared by DM_REAM, HPO and the LAMMPS pair style. */
std::string rhoform_absent_message();

/** Form stored in @p ctx, or nullopt when the key is absent. Never inserts;
 *  throws DensityFormError for an invalid stored value. */
std::optional<RhoForm> stored_rhoform(const tadah::core::Context &ctx);

/** Form stored in @p ctx. Throws DensityFormError with the migration text
 *  (rhoform_absent_message) when the key is absent or invalid. @p who, when
 *  not empty, prefixes the message ("<who>: DM_REAM: RHOFORM is required:
 *  ..."); DM_REAM itself passes an empty @p who. Never inserts. */
RhoForm require_rhoform(const tadah::core::Context &ctx,
                        const std::string &who);

/** Potential writers: a context with a DM_REAM block must declare RHOFORM
 *  (absent -> DensityFormError with the migration text; nothing is
 *  inserted) and gets it canonicalised to upper case; a context without one
 *  has a stray RHOFORM removed (report_ream_only_key: a stderr notice once
 *  per process), so potentials without a DM_REAM block never carry the
 *  key. */
void normalise_rhoform_key(tadah::core::Context &c);

/** Engines that merge a run's configuration over a loaded potential (tadah
 *  refit --edit with -c, analysis bfunc of a program's context): call with
 *  the potential as read and the configuration BEFORE merging. RHOFORM is a
 *  property of the potential, so a potential with a DM_REAM block but no
 *  RHOFORM throws (DensityFormError, migration text) whatever the
 *  configuration says: the configuration cannot supply the key ("<pot>: the
 *  RHOFORM of the configuration file does not apply ..."). Different
 *  explicit values throw as well ("RHOFORM conflict: ..."). A RHOFORM on
 *  either side of a potential without a DM_REAM block is ignored
 *  (report_ream_only_key: a stderr notice once per process). An engine
 *  whose model is the potential's alone (tadah predict) passes an empty
 *  configuration. */
void check_rhoform_precedence(const tadah::core::Context &pot,
                              const tadah::core::Context &config,
                              const std::string &pot_name);

/** "DM_REAM: RHOFORM <form>", the subject of every form-rule message
 *  (ReamDensity::validate, the density pair weights). */
std::string rhoform_subject(RhoForm f);

/** True when the cutoff class @p label (Cut_Base::label(), RCTYPEMB) is
 *  measured non-negative and non-increasing on [0, rcut], as RHOFORM
 *  MONOTONE requires: Cut_Dummy, Cut_Cos, Cut_CosS, Cut_Tanh, Cut_PT,
 *  Cut_Poly and Cut_PolyS (the last two dip to -2.7e-15 near rcut, which the
 *  positive forms clamp at 0). Cut_SinInv (increasing) and every other label
 *  are refused. Exact, case-sensitive match. */
bool rhoform_cutoff_whitelisted(const std::string &label);

/** The cutoffs rhoform_cutoff_whitelisted() accepts, for messages:
 *  "Cut_Dummy, Cut_Cos, Cut_CosS, Cut_Tanh, Cut_PT, Cut_Poly or Cut_PolyS". */
std::string rhoform_cutoff_whitelist_text();

/** Why RHOFORM MONOTONE refuses the knot family (basis code 2), for every
 *  refusal message: "a non-negative knot density is already non-increasing
 *  with a non-increasing cutoff" (use RHOFORM NONNEG). */
const char *monotone_knot_refusal_text();

/** \brief Lower-bound class of an OPTIM key's box under a density form. */
enum class OptimLowerBound {
  Free,        ///< the form imposes nothing (the key's own rules apply)
  NonNegative  ///< lo = max(0, v - d): AMPGRIDMB under NONNEG and MONOTONE
};

/** Lower-bound class of the OPTIM key @p key (case-insensitive) under the
 *  density form @p form (nullopt: no DM_REAM block). Shared by the MLIP
 *  hpotarget writer / retarget and the HPO AUTO boxes. */
OptimLowerBound optim_key_lower_bound(const std::string &key,
                                      std::optional<RhoForm> form);

/** \brief What a density form, a radial bump channel or the limit a
 *  descriptor declares requires of the low bound of an OPTIM box. */
struct OptimLowBoundRule {
  bool strict;              ///< lo > bound (else lo >= bound)
  std::string requirement;  ///< "every AMPGRIDMB amplitude >= 0", ...
  /** What sets the rule, the subject of "<by> requires <requirement>":
   *  "RHOFORM NONNEG", "the D2_Blip5 pair basis", "the DM_REAM Blip density
   *  basis", "the DM_mBlipEnv descriptor". */
  std::string by;
  /** The bound: 0, but the lower limit a descriptor declares for a key
   *  other than 0 and kHardLoPositive (3 for the knot degrees), whose rule
   *  is >=; a > rule has the bound 0. */
  double bound = 0.0;
  /** True when @p lo satisfies the rule (NaN never does). */
  bool admits(double lo) const;
  /** The low end of an AUTO box around @p seed (a value the rule admits)
   *  that reaches below the rule: the bound of a >= rule; for a > rule,
   *  positive_box_low(seed, bound), half-way from the bound to the seed
   *  (half the seed for the bound 0), as the refit writer clamps the box
   *  of a width, so the box holds the seed and spans a factor of 2 or more
   *  below it. */
  double clamp_low(double seed) const;
};

/** The low end of a box around @p seed of a value that must be
 *  > @p bound, for a box whose symmetric low end is at or below the bound:
 *  half-way from the bound to the seed for a seed above the bound (the box
 *  holds the seed), else bound + max(kHardLoPositive, half the seed's
 *  distance from the bound). The AUTO FRAC boxes of tadah hpo
 *  (OptimLowBoundRule::clamp_low), the target file tadah refit writes and
 *  tadah refit --retarget clamp the box of such a value at it. */
double positive_box_low(double seed, double bound = 0.0);

/** The rule the lower limit that the registered descriptors declare for
 *  the values of @p key (case-insensitive; ParamMeta::hard.lo of a
 *  parameter with arg_pos <= 0) sets on the low bound of an OPTIM box,
 *  nullopt when no descriptor declares one: every value > 0 for the limit
 *  kHardLoPositive (SGRID2B, SGRIDMB, SEMBFUNC, RCUTENV), else every value
 *  >= the limit (RCUT2B, RCUTMB, CGRID2B, CGRIDMB: >= 0). The descriptors
 *  that declare a limit for a key declare the same one; the subject names
 *  the first of them by its registered name ("the DM_mBlipEnv
 *  descriptor"). */
std::optional<OptimLowBoundRule> declared_low_bound_rule(
    const std::string &key);

/** The rule RHOFORM @p form sets on the low bound of an OPTIM box on @p key
 *  (case-insensitive) of a DM_REAM density in the TYPEMB basis @p basis,
 *  nullopt when it sets none. Under NONNEG and MONOTONE: AMPGRIDMB lo >= 0
 *  (optim_key_lower_bound), WATOM lo > 0 (every density pair weight W > 0)
 *  and SGRIDMB lo > 0 (every width > 0) under MONOTONE for every basis and
 *  under NONNEG for the Gaussian basis, whose exp(-s (r - c)^2) grows
 *  without bound for s < 0 (the Blip bumps are even in s). These are the
 *  rules ReamDensity::validate applies to the density: a box whose low
 *  bound breaks one reaches densities DM_REAM refuses, so tadah hpo refuses
 *  it at startup and tadah analysis check_bounds reports it. Both pass the
 *  seed's basis (optim_rule_basis). @p basis nullopt: the basis is not
 *  known (tadah refit --retarget, whose hpotarget file names no potential),
 *  so the rules that depend on it are not applied there. */
std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, std::optional<RhoForm> form,
    std::optional<BasisKind> basis);

/** The basis optim_low_bound_rule() takes for the OPTIM boxes of the
 *  configuration or potential @p ctx: nullopt without a DM_REAM block
 *  (context_has_ream); the TYPEMB basis of a bare DM_REAM block
 *  (ReamDensity::basis_from_context); for a DM_mJoin, the Gaussian basis
 *  when one of its DM_REAM children uses it (an OPTIM box on a joined key
 *  does not say whose entries it holds, so each entry is held to the
 *  strictest child rule, as MONOTONE's SGRIDMB rule holds them all), else
 *  the basis of its first DM_REAM child. Throws when a DM_REAM basis code
 *  does not convert ("DM_REAM: TYPEMB basis code 'x' is not an integer
 *  ..."). */
std::optional<BasisKind> optim_rule_basis(const tadah::core::Context &ctx);

/** The rule on the low bound of an OPTIM box on @p key (case-insensitive)
 *  of the configuration or potential @p ctx, nullopt when there is none:
 *  the density-form rule optim_low_bound_rule(key, form, basis) with the
 *  RHOFORM of @p ctx and its density basis (optim_rule_basis), else, on a
 *  radial bump channel, every width > 0 in every form -- SGRIDMB of a
 *  DM_REAM Blip or Blip5 density (optim_rule_basis, so a DM_mJoin's
 *  DM_REAM children count) and SGRID2B of a D2_Blip or D2_Blip5 pair term
 *  (potedit::bump_channel_halfwidth), or of such a descriptor of a
 *  D2_mJoin line (the rule of its entries, "the D2_Blip pair basis of the
 *  D2_mJoin line requires each of its SGRID2B widths > 0": the entry form
 *  below says whose entry an entry is): a bump spans [c - w/s, c + w/s],
 *  and a width s <= 0 has no support edge, so no bump-support rule can
 *  hold it inside the cutoff. tadah hpo refuses such a box and such a seed
 *  width at startup; tadah analysis check_bounds reports it. Throws the
 *  migration text (require_rhoform) for a DM_REAM configuration without
 *  RHOFORM. */
std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx);

/** The rule of entry @p entry (0-based, as the values of the key are
 *  numbered, a grid generator counting as the values it makes) of @p key:
 *  on a D2_mJoin line, the rule of the descriptor whose SGRID2B entry it
 *  is (none for a D2_BP's, as for a line of one D2_BP; none past the
 *  line's values); else optim_low_bound_rule(key, ctx), which holds every
 *  entry (a DM_mJoin's bump density holds every SGRIDMB entry). The rule
 *  tadah hpo, at an OPTIM box and at startup, and tadah analysis
 *  check_bounds apply to a value. */
std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx,
    std::size_t entry);

/** The rule of an OPTIM box on @p key (case-insensitive) of the
 *  configuration or potential @p ctx, nullopt when there is none: the
 *  density-form or bump rule optim_low_bound_rule(key, ctx) when there is
 *  one, else the rule of the lower limit that a descriptor of @p ctx that
 *  reads the key declares (declared_low_bound_rule, its subject that
 *  descriptor: "the D2_BP descriptor requires every SGRID2B value > 0"); a
 *  key that no descriptor of @p ctx reads has none. tadah hpo refuses an
 *  explicit box whose low bound the rule does not admit, at its OPTIM
 *  line, and clamps an AUTO box at OptimLowBoundRule::clamp_low; tadah
 *  analysis check_bounds reports a corner below it. */
std::optional<OptimLowBoundRule> optim_box_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx);

/** The rule of entry @p entry (0-based, numbered as in
 *  optim_low_bound_rule(key, ctx, entry)) of @p key: on a D2_mJoin or
 *  DM_mJoin line, that of the descriptor whose entry it is. */
std::optional<OptimLowBoundRule> optim_box_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx,
    std::size_t entry);

}  // namespace models
}  // namespace tadah

#endif
