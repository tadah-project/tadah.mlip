#ifndef TADAH_MODELS_DESCRIPTORS_PARAM_META_H
#define TADAH_MODELS_DESCRIPTORS_PARAM_META_H

// Per-descriptor parameter metadata schema.
//
// Each registered descriptor (D2_*, DM_*) optionally returns a
// DescriptorMeta from its overridden `D_Base::meta() const`. The
// HPO bound-validation pipeline reads this metadata to:
//
//   * Suggest OPTIM bounds from the data and the seed (BoundSuggester).
//   * Diagnose degenerate parameter combinations (HealthCheck).
//   * Flag missing OPTIM blocks on Critical hyperparameters.
//   * Predict design-matrix column counts before construction.
//
// Default-`nullopt` semantics keep every existing descriptor working
// unchanged.

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <tadah/core/context.h>
#include <tadah/models/descriptors/dm/embed_spec.h>
#include <tadah/models/descriptors/line_pairs.h>
#include <tadah/core/stats/data_stats.h>

namespace tadah {
namespace models {

/** \brief Physical scale category of a parameter — drives which dataset
 *  statistic the suggester anchors to.
 */
enum class Scale {
  Distance,           // Å (e.g. RCUT, CGRIDxB)
  InverseDistanceSq,  // Å^-2 (e.g. SGRIDxB — Gaussian inverse width)
  InverseDistance,    // Å^-1 (compact bump eta)
  Density,            // ρ (rho-units; e.g. CEMBFUNC, RCUTENV)
  InverseDensity,     // ρ^-1 (e.g. SEMBFUNC)
  InverseDensitySq,   // ρ^-2 (Gaussian embedding eta)
  Dimensionless,      // unitless (e.g. exponents)
  Energy,             // eV
  Force,              // eV/Å
  Count               // dimensionless integer count
};

/** \brief The lower limit (HardLimit::lo) a descriptor declares for a
 *  value that must be > 0 (a width, RCUTENV): the OPTIM box of such a
 *  value has a low bound > 0, and an AUTO box that reaches below it is
 *  clamped at this limit (models::optim_box_low_bound_rule). */
inline constexpr double kHardLoPositive = 1.0e-6;

/** \brief Hard physical limits of a value — NEVER tuning thresholds.
 *  lo: kHardLoPositive for a value that must be > 0, else the smallest
 *  value (0 for a cutoff or a centre); unset: no limit. The lower limit
 *  of the values of a key (arg_pos <= 0) is the OPTIM box rule of the key
 *  (models::declared_low_bound_rule): tadah hpo refuses an explicit box
 *  below it and clamps an AUTO box at it.
 */
struct HardLimit {
  std::optional<double> lo{};
  std::optional<double> hi{};
};

/** \brief Whether a parameter must be optimised.
 *  - Critical: a fixed value that misses the data can make the
 *    descriptor vanish (e.g. a CGRIDMB centre outside the neighbour
 *    distances collapses ρ); used by the pre-flight audit to warn when
 *    no OPTIM block is present.
 *  - Recommended: optimisation usually helps; not strictly necessary.
 *  - Optional: routine to fix.
 */
enum class Criticality { Critical, Recommended, Optional };

/** \brief Whether a parameter is shared across all element pairs or
 *  declared per (element-i, element-j) pair.
 */
enum class PairScope { Global, PerElementPair };

// ---------------------------------------------------------------------
//  HealthVerdict — Ok / Warn / Fail
// ---------------------------------------------------------------------

struct HealthOk {};

struct HealthWarn {
  std::string message;
};

struct HealthFail {
  std::string message;
};

using HealthVerdict = std::variant<HealthOk, HealthWarn, HealthFail>;

inline bool is_ok  (const HealthVerdict& v) { return std::holds_alternative<HealthOk>(v); }
inline bool is_warn(const HealthVerdict& v) { return std::holds_alternative<HealthWarn>(v); }
inline bool is_fail(const HealthVerdict& v) { return std::holds_alternative<HealthFail>(v); }

inline std::string verdict_message(const HealthVerdict& v) {
  if (auto* w = std::get_if<HealthWarn>(&v)) return w->message;
  if (auto* f = std::get_if<HealthFail>(&v)) return f->message;
  return {};
}

// ---------------------------------------------------------------------
//  SeedView — read-only view over the seed Context
// ---------------------------------------------------------------------

/** \brief Thin read-only wrapper over `tadah::core::Context` giving
 *  metadata closures access to the current parameter values without
 *  allowing mutation.
 *
 *  Constructed from a `const Context&`; the underlying Context must
 *  outlive the SeedView.
 */
class SeedView {
 public:
  explicit SeedView(const tadah::core::Context& c) : ctx_(&c) {}
  /** \brief A view of @p c, the context of one descriptor of a joined
   *  TYPE2B or TYPEMB line (D_mJoin::parse_context()): its values of a key
   *  @p k come after @p first_entries.at(k) values of that key in the
   *  configuration, those of the descriptors before it on the line, so
   *  that a message numbers an entry as an OPTIM line of the
   *  configuration does (entry_number()). */
  SeedView(const tadah::core::Context& c,
           std::map<std::string, std::size_t> first_entries)
      : ctx_(&c), first_(std::move(first_entries)) {}
  /** \brief A view of @p c for the descriptor whose TYPE2B or TYPEMB line
   *  names the element pairs @p pairs (D_Base::get_init_atoms()), its
   *  entries numbered after @p first_entries as above. The statistics its
   *  checks and suggestions are given are those of these pairs
   *  (LinePairs::stats()), and their messages name them (line_pairs()). */
  SeedView(const tadah::core::Context& c,
           std::map<std::string, std::size_t> first_entries, LinePairs pairs)
      : ctx_(&c), first_(std::move(first_entries)), pairs_(std::move(pairs)) {}

  /** \brief The element pairs of the descriptor's line when the view has
   *  them; nullptr for a view without them (the statistics it is given are
   *  then those of every element pair of the data, as the caller gives
   *  them, and a message names no pair). */
  const LinePairs* line_pairs() const {
    return pairs_ ? &*pairs_ : nullptr;
  }

  /** \brief The 1-based number of value @p i of @p key in the
   *  configuration, as an OPTIM line names it: @p i + 1, after the values
   *  of the key that come before the view's own (none but for the view of
   *  a joined line's descriptor). */
  std::size_t entry_number(const std::string& key, std::size_t i) const {
    const auto it = first_.find(key);
    return (it == first_.end() ? 0 : it->second) + i + 1;
  }

  template <typename T>
  T get(const std::string& key) const {
    if(key.find('.')!=std::string::npos) return get<T>(key,0);
    return ctx_->get<T>(key);
  }

  template <typename T>
  T get(const std::string& key, std::size_t arg_pos) const {
    if(key.find('.')!=std::string::npos) {
      double v=embed_binding(*ctx_,key).values.at(arg_pos);
      if constexpr(std::is_arithmetic_v<T>) return static_cast<T>(v);
      else if constexpr(std::is_same_v<T,std::string>) return core::format_roundtrip(v);
    }
    return ctx_->get<T>(key, arg_pos);
  }

  std::size_t size(const std::string& key) const {
    if(key.find('.')!=std::string::npos) return embed_binding(*ctx_,key).values.size();
    return ctx_->size(key);
  }

  bool contains(const std::string& key) const {
    if(key.find('.')!=std::string::npos) {
      for(const auto &b:embed_bindings(*ctx_)) if(b.path==core::to_upper(key)) return true;
      return false;
    }
    return ctx_->contains(key);
  }

  /** Read a v_type / vector<double> in one shot. */
  std::vector<double> vec(const std::string& key) const {
    std::vector<double> out;
    if (!contains(key)) return out;
    const std::size_t n = size(key);
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      out.push_back(get<double>(key, i));
    }
    return out;
  }

  const tadah::core::Context& context() const { return *ctx_; }

 private:
  const tadah::core::Context* ctx_;
  std::map<std::string, std::size_t> first_;
  std::optional<LinePairs> pairs_;
};

// ---------------------------------------------------------------------
//  Closures
// ---------------------------------------------------------------------

/** \brief A suggested OPTIM range of one parameter entry and how it was
 *  obtained: `first` is the low end and `second` the high end, as in the
 *  `(lo, hi)` pair it extends (a suggester may return the pair alone).
 *
 *  - `rationale`: the form the range assumes and the statistics it comes
 *    from; empty when the suggester gives none (the framework then states
 *    the scale and the role of the parameter);
 *  - `fixed`: the range is a fixed one, not derived from the data (the
 *    statistic it needs is absent), and `rationale` says why;
 *  - `none`: no OPTIM line is suggested for the entry (a radial centre at
 *    the origin, whose width alone sets its reach; data without a pair of
 *    atoms to take a range from; a value the data give no range for), and
 *    `rationale` says why; the range is then empty.
 */
struct BoundSuggestion : std::pair<double, double> {
  std::string rationale{};
  bool fixed = false;
  bool none = false;

  BoundSuggestion() : std::pair<double, double>(0.0, 0.0) {}
  BoundSuggestion(double lo, double hi, std::string why = {})
      : std::pair<double, double>(lo, hi), rationale(std::move(why)) {}
  /** The range alone, without a rationale (implicit: a suggester that
   *  returns a pair gives this). */
  BoundSuggestion(const std::pair<double, double>& range)
      : std::pair<double, double>(range) {}

  /** A fixed range, not derived from the data: @p why says so. */
  static BoundSuggestion fixed_range(double lo, double hi, std::string why) {
    BoundSuggestion s(lo, hi, std::move(why));
    s.fixed = true;
    return s;
  }
  /** No OPTIM line for the entry: @p why says why. */
  static BoundSuggestion no_row(std::string why) {
    BoundSuggestion s;
    s.rationale = std::move(why);
    s.none = true;
    return s;
  }
};

/** \brief Suggest a `(lo, hi)` bound from data + seed.
 *
 *  arg_pos: -1 for scalar params; 0..n-1 for vector entries
 *  (the framework calls the suggester per vector entry). A suggester never
 *  returns a range of zero width: without a range from the data it returns
 *  BoundSuggestion::no_row with the reason. tadah analysis suggest_bounds
 *  calls it with the statistics of one element pair of the descriptor's
 *  line at a time (LinePairs::stats(), SeedView::line_pairs()) and takes
 *  the range that holds the range of each, or, for a parameter marked
 *  ParamMeta::reach_each_pair (a cutoff), the range from the largest low
 *  end of their ranges to the largest high end.
 *
 *  Closures are written to read DataStats — never to embed magic
 *  numbers. The only constants permitted in a suggester body are
 *  small structural ones (e.g. "double the FWHM") that are documented
 *  at the declaration site. A range that does not come from the data
 *  is returned as BoundSuggestion::fixed_range, with the reason.
 */
using BoundSuggester = std::function<BoundSuggestion(
    const tadah::core::stats::DataStats&,
    const SeedView&,
    int arg_pos)>;

/** \brief Validate (or warn about) a parameter or parameter combination
 *  given the seed and the data: the pre-flight audit gives it the
 *  statistics of the element pairs of the descriptor's line
 *  (LinePairs::stats(), SeedView::line_pairs()), and a check of the reach
 *  of a cutoff or of the channels of a density judges each element pair
 *  the line stands for on its own (cutoff_reach_check(),
 *  radial_reach_check()).
 */
using HealthCheck = std::function<HealthVerdict(
    const tadah::core::stats::DataStats&,
    const SeedView&)>;

/** \brief Predict the number of design-matrix columns this descriptor
 *  occupies, given a seed. Used by ColumnLayout to populate
 *  the per-block layout without instantiating descriptors.
 */
using ColumnCounter = std::function<std::size_t(const SeedView&)>;

// ---------------------------------------------------------------------
//  ParamMeta / DescriptorMeta
// ---------------------------------------------------------------------

struct ParamMeta {
  std::string  key{};
  /** Which values of the key the parameter is, in one convention:
   *  - -1: the key's one value (a scalar key: RCUT2B);
   *  - 0: each value of a list key (CGRID2B), the framework iterating over
   *    the seed's values;
   *  - n >= 1: the value at 1-based position n among the key's values in
   *    the descriptor's own context, its own line for a value of its
   *    TYPE2B or TYPEMB line, where position 1 is the descriptor's name
   *    (the screening length a0 of "D2_ZBL a0 s0 p0 p1 Ta Ta" is 2): the
   *    arg_pos the descriptor's calc_dDdtheta takes for that value.
   *  tadah analysis suggest_bounds names a value as an OPTIM line of the
   *  configuration numbers it, among the numbers of the whole key. */
  int          arg_pos{-1};

  Scale        scale{Scale::Dimensionless};
  std::string  units{};
  std::string  role{};

  HardLimit    hard{};
  Criticality  crit{Criticality::Optional};
  PairScope    scope{PairScope::Global};

  BoundSuggester suggest{};
  HealthCheck    check{};
  /** Whether each value of the parameter must reach each element pair of
   *  the descriptor's line, as the check of a cutoff judges it
   *  (cutoff_reach_check()): every RCUT2B and RCUTMB. For a line of
   *  several pairs, tadah analysis suggest_bounds then suggests the range
   *  from the largest low end of the ranges of its pairs to the largest
   *  high end, so that each value in it reaches each pair; for any other
   *  parameter, the range that holds the range of each pair. */
  bool           reach_each_pair{false};

  /** Per-parameter aggregate thresholds used by the per-iter health
   *  monitor. All have rationale-bearing defaults; descriptors
   *  may override.
   *
   *  drift_to_bound_threshold: a parameter that sits within
   *      (1 - this) * (bound width) of either edge for >= window evals
   *      is flagged. Default 0.20 — i.e. "stuck within 20% of a bound".
   *      Rationale: 1/5 of the bound is the smallest "near-edge"
   *      window where local-search noise alone is unlikely to keep a
   *      parameter pinned across hundreds of evaluations.
   *
   *  weight_collinearity_ratio: max(|w|)/median(|w|) > this in this
   *      block triggers a warning. Default 1e3 — "1000x stretch
   *      between the largest and the typical weight is the canonical
   *      signature of a near-rank-deficient block."
   *
   *  weight_extrapolation_max: max(|w|) > this triggers a warning.
   *      Default 1e4 — extrapolation outside the training pool's
   *      energy scale is unreliable beyond this magnitude.
   *
   *  All three are *defaults* — the framework never compares against a
   *  literal in the audit code.
   */
  double drift_to_bound_threshold{0.20};
  double weight_collinearity_ratio{1.0e3};
  double weight_extrapolation_max{1.0e4};
};

struct DescriptorMeta {
  std::string                    label{};
  std::vector<ParamMeta>         params{};
  /** Whole-descriptor checks (e.g. "ρ collapse" — uses several
   *  parameters in concert). */
  std::vector<HealthCheck>       cross_param_checks{};
  /** Predict column count in the design matrix from the seed values. */
  ColumnCounter                  expected_columns{};
  /** Block-dead column-norm-ratio threshold, in [0, 1). A column block
   *  whose median column norm is < `block_dead_norm_ratio * max_block_norm`
   *  is flagged "dead" by the pre-flight audit. The default: a column
   *  block 6 orders of magnitude smaller than another block in the same
   *  matrix has no meaningful signal. */
  double block_dead_norm_ratio{1.0e-6};
};

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_PARAM_META_H
