#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/descriptors/d_mjoin.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/eam/pot_curve_edit.h>
#include <tadah/core/number_format.h>
#include <tadah/core/registry.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/param_meta.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <cmath>
#include <map>
#include <memory>
#include <set>

namespace tadah {
namespace models {

namespace {
std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char ch) { return std::toupper(ch); });
  return s;
}
// The cutoffs measured non-negative and non-increasing on [0, rcut]
// (rhoform_cutoff_whitelisted, rhoform_cutoff_whitelist_text).
const char *const kMonotoneCutoffs[] = {"Cut_Dummy", "Cut_Cos",  "Cut_CosS",
                                        "Cut_Tanh",  "Cut_PT",   "Cut_Poly",
                                        "Cut_PolyS"};
}  // namespace

const char *rhoform_name(RhoForm f) {
  switch (f) {
    case RhoForm::Signed:   return "SIGNED";
    case RhoForm::NonNeg:   return "NONNEG";
    case RhoForm::Monotone: return "MONOTONE";
  }
  return "SIGNED";  // unreachable (enum is exhaustive above)
}

bool rhoform_is_positive(RhoForm f) { return f != RhoForm::Signed; }

RhoForm parse_rhoform(const std::string &value) {
  const std::string v = upper(value);
  if (v == "SIGNED") return RhoForm::Signed;
  if (v == "NONNEG") return RhoForm::NonNeg;
  if (v == "MONOTONE") return RhoForm::Monotone;
  throw DensityFormError("RHOFORM must be SIGNED, NONNEG or MONOTONE (got '" +
                         value + "').");
}

namespace {
// The three forms, shared by the absent-key and the invalid-value messages.
const char *const kFormList =
    "SIGNED (rho = W f_c sum a_n psi_n with signed a_n: the density before "
    "tadah 1.6.0, which a RHOFORM SIGNED line declares for an existing "
    "potential, with an EMBF0 FREE line for its embedding reference), "
    "NONNEG (the same with every a_n >= 0: rho >= 0 by construction) or "
    "MONOTONE (rho = W f_c sum a_n S_n(r), S_n the integrated bump, "
    "a_n >= 0: rho >= 0 and non-increasing by construction).";
}  // namespace

std::string rhoform_absent_message() {
  return std::string("DM_REAM: RHOFORM is required: ") + kFormList;
}

std::optional<RhoForm> stored_rhoform(const tadah::core::Context &ctx) {
  const auto v = ctx.stored_value("RHOFORM");
  if (!v) return std::nullopt;
  return parse_rhoform(*v);
}

RhoForm require_rhoform(const tadah::core::Context &ctx,
                        const std::string &who) {
  const std::string prefix = who.empty() ? std::string() : who + ": ";
  const auto v = ctx.stored_value("RHOFORM");
  if (!v) throw DensityFormError(prefix + rhoform_absent_message());
  try {
    return parse_rhoform(*v);
  } catch (const DensityFormError &) {
    throw DensityFormError(prefix + "DM_REAM: RHOFORM '" + *v +
                           "' is not a density form; the density forms are " +
                           kFormList);
  }
}

void normalise_rhoform_key(tadah::core::Context &c) {
  if (context_has_ream(c)) {
    const RhoForm f = require_rhoform(c, "");
    c.remove("RHOFORM");
    c.add("RHOFORM", std::string(rhoform_name(f)));
    return;
  }
  if (c.has_stored("RHOFORM")) {
    report_ream_only_key("RHOFORM");
    c.remove("RHOFORM");
  }
}

void check_rhoform_precedence(const tadah::core::Context &pot,
                              const tadah::core::Context &config,
                              const std::string &pot_name) {
  if (!context_has_ream(pot)) {
    // a key on either side has no effect (validated when it was read)
    if (pot.has_stored("RHOFORM") || config.has_stored("RHOFORM"))
      report_ream_only_key("RHOFORM");
    return;
  }
  // The potential must declare its own form: a configuration value never
  // supplies it.
  if (!pot.has_stored("RHOFORM") && config.has_stored("RHOFORM"))
    throw DensityFormError(
        pot_name + ": the RHOFORM of the configuration file does not apply "
        "(the form belongs to the potential, whose density amplitudes were "
        "fitted in it); the RHOFORM line belongs in " + pot_name + ". " +
        rhoform_absent_message());
  const RhoForm pv = require_rhoform(pot, pot_name);
  const auto cv = stored_rhoform(config);
  if (cv && *cv != pv)
    throw DensityFormError(
        "RHOFORM conflict: " + pot_name + " declares RHOFORM " +
        rhoform_name(pv) + " and the configuration file sets RHOFORM " +
        rhoform_name(*cv) +
        ". RHOFORM is a property of the potential (its density amplitudes "
        "were fitted in that form): the potential file sets it, not the "
        "configuration file.");
}

std::string rhoform_subject(RhoForm f) {
  return std::string("DM_REAM: RHOFORM ") + rhoform_name(f);
}

bool rhoform_cutoff_whitelisted(const std::string &label) {
  for (const char *l : kMonotoneCutoffs)
    if (label == l) return true;
  return false;
}

std::string rhoform_cutoff_whitelist_text() {
  const std::size_t n = std::size(kMonotoneCutoffs);
  std::string s;
  for (std::size_t i = 0; i < n; ++i)
    s += std::string(i == 0 ? "" : i + 1 == n ? " or " : ", ") +
         kMonotoneCutoffs[i];
  return s;
}

const char *monotone_knot_refusal_text() {
  return "a non-negative knot density is already non-increasing with a "
         "non-increasing cutoff";
}

OptimLowerBound optim_key_lower_bound(const std::string &key,
                                      std::optional<RhoForm> form) {
  if (form && rhoform_is_positive(*form) && upper(key) == "AMPGRIDMB")
    return OptimLowerBound::NonNegative;
  return OptimLowerBound::Free;
}

std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, std::optional<RhoForm> form,
    std::optional<BasisKind> basis) {
  if (!form) return std::nullopt;
  const std::string by = std::string("RHOFORM ") + rhoform_name(*form);
  if (optim_key_lower_bound(key, form) == OptimLowerBound::NonNegative)
    return OptimLowBoundRule{false, "every AMPGRIDMB amplitude >= 0", by};
  if (!rhoform_is_positive(*form)) return std::nullopt;
  const std::string k = upper(key);
  if (k == "WATOM")
    return OptimLowBoundRule{true, "every density pair weight W > 0", by};
  if (k == "SGRIDMB" && *form == RhoForm::Monotone)
    return OptimLowBoundRule{true,
                             "every SGRIDMB width > 0 (S_n steps down across "
                             "the bump s_n (r - c_n))",
                             by};
  // ReamDensity::validate: NONNEG needs s > 0 for the Gaussian basis only
  if (k == "SGRIDMB" && basis == BasisKind::Gaussian)
    return OptimLowBoundRule{true,
                             "every SGRIDMB width > 0 for the Gaussian basis "
                             "(exp(-s (r - c)^2) grows without bound for "
                             "s < 0)",
                             by};
  return std::nullopt;
}

namespace {
/* optim_low_bound_rule(key, ctx) and, with @p entry, of that entry
 * (0-based) of the key. */
std::optional<OptimLowBoundRule> low_bound_rule_of(
    const std::string &key, const tadah::core::Context &ctx,
    std::optional<std::size_t> entry) {
  std::optional<RhoForm> form;
  std::optional<BasisKind> basis;
  if (context_has_ream(ctx)) {
    form = require_rhoform(ctx, "the OPTIM box rules");
    basis = optim_rule_basis(ctx);
  }
  if (auto r = optim_low_bound_rule(key, form, basis)) return r;
  // A radial bump channel: every width > 0 in every form.
  const std::string k = upper(key);
  if (k == "SGRIDMB" && basis && bump_halfwidth(*basis) > 0.0)
    return OptimLowBoundRule{
        true,
        "every SGRIDMB width > 0 (a radial bump spans [c - w/s, c + w/s]; a "
        "width <= 0 has no support edge)",
        std::string("the DM_REAM ") + basis_name(*basis) + " density basis"};
  if (k == "SGRID2B") {
    if (potedit::bump_channel_halfwidth(ctx, "2B")) {
      const auto label = ctx.stored_value("TYPE2B", 0);
      return OptimLowBoundRule{
          true,
          "every SGRID2B width > 0 (a radial bump spans [c - w/s, c + w/s]; "
          "a width <= 0 has no support edge)",
          "the " + label.value_or("TYPE2B") + " pair basis"};
    }
    // A D2_mJoin: a D2_Blip or D2_Blip5 descriptor holds its own SGRID2B
    // entries (D_mJoin::parse_context gives each descriptor its share, in
    // the order of the line), the entries of another descriptor keep that
    // descriptor's rule (a D2_BP's, none). Without an entry, the rule of
    // the first bump descriptor's entries.
    const auto line = ctx.stored_value("TYPE2B", 0);
    if (line && *line == "D2_mJoin") {
      std::size_t at = 0;  // the first entry of the descriptor
      for (const auto &child : D_mJoin::parse_context(ctx, "TYPE2B")) {
        const std::size_t n =
            child.contains("SGRID2B") ? child.size("SGRID2B") : 0;
        const bool bump =
            potedit::bump_channel_halfwidth(child, "2B").has_value();
        const bool owns = entry && *entry >= at && *entry < at + n;
        at += n;
        if (entry && !owns) continue;
        if (!bump) {
          if (entry) return std::nullopt;
          continue;
        }
        return OptimLowBoundRule{
            true,
            "each of its SGRID2B widths > 0 (a radial bump spans [c - w/s, "
            "c + w/s]; a width <= 0 has no support edge)",
            "the " + child.stored_value("TYPE2B", 0).value_or("TYPE2B") +
                " pair basis of the D2_mJoin line"};
      }
    }
  }
  return std::nullopt;
}
}  // namespace

std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx) {
  return low_bound_rule_of(key, ctx, std::nullopt);
}

std::optional<OptimLowBoundRule> optim_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx,
    std::size_t entry) {
  return low_bound_rule_of(key, ctx, entry);
}

std::optional<BasisKind> optim_rule_basis(const tadah::core::Context &ctx) {
  if (!context_has_ream(ctx)) return std::nullopt;
  auto basis_of = [](const tadah::core::Context &c) {
    try {
      return ReamDensity::basis_from_context(c);
    } catch (const std::runtime_error &e) {
      throw std::runtime_error(std::string("DM_REAM: ") + e.what());
    }
  };
  const auto label = ctx.stored_value("TYPEMB", 0);
  if (label && *label == "DM_REAM") return basis_of(ctx);
  // a DM_mJoin: its DM_REAM children
  std::optional<BasisKind> first;
  for (const auto &child : D_mJoin::parse_context(ctx, "TYPEMB")) {
    const auto l = child.stored_value("TYPEMB", 0);
    if (!l || *l != "DM_REAM") continue;
    const BasisKind b = basis_of(child);
    if (b == BasisKind::Gaussian) return b;
    if (!first) first = b;
  }
  return first;
}

bool OptimLowBoundRule::admits(double lo) const {
  return strict ? lo > bound : lo >= bound;
}

double positive_box_low(double seed, double bound) {
  const double distance = seed - bound;
  if (distance > 0.0) return bound + 0.5 * distance;
  return bound + std::max(kHardLoPositive, 0.5 * std::fabs(distance));
}

double OptimLowBoundRule::clamp_low(double seed) const {
  if (!strict) return bound;
  return positive_box_low(seed, bound);
}

namespace {
/* The lower limit each registered descriptor declares for the values of a
 * key (ParamMeta::hard.lo of a parameter with arg_pos <= 0): registered
 * name -> upper-case key -> limit. Read once. */
const std::map<std::string, std::map<std::string, double>> &
declared_limits() {
  static const auto limits = [] {
    std::map<std::string, std::map<std::string, double>> out;
    for (const auto &label : tadah::core::Registry<D_Base>::names()) {
      std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
      const auto meta = d->meta();
      if (!meta) continue;
      for (const auto &pm : meta->params)
        if (pm.arg_pos <= 0 && pm.hard.lo) out[label][upper(pm.key)] = *pm.hard.lo;
    }
    return out;
  }();
  return limits;
}

/* The limit the descriptor @p label declares for @p key (upper case). */
std::optional<double> declared_limit(const std::string &label,
                                     const std::string &key) {
  const auto &limits = declared_limits();
  const auto d = limits.find(label);
  if (d == limits.end()) return std::nullopt;
  const auto k = d->second.find(key);
  if (k == d->second.end()) return std::nullopt;
  return k->second;
}

/* The rule of the limit @p lo that the descriptor @p label declares for
 * @p key (upper case). */
OptimLowBoundRule rule_of_limit(const std::string &key, double lo,
                                const std::string &label) {
  const std::string by = "the " + label + " descriptor";
  if (lo == kHardLoPositive)
    return OptimLowBoundRule{true, "every " + key + " value > 0", by, 0.0};
  return OptimLowBoundRule{
      false, "every " + key + " value >= " + tadah::core::format_roundtrip(lo),
      by, lo};
}

/* The descriptors of @p ctx that read the values of @p key (upper case),
 * in the order of the TYPE2B and TYPEMB lines, each with the 0-based range
 * [first, first + count) of the key's entries that it reads: a descriptor
 * of a D2_mJoin or DM_mJoin line its share of a key the line splits (its
 * RCUT and RCTYPE values, one per descriptor, and the list keys of its
 * descriptors: D_mJoin::parse_context), every entry of any other key (the
 * line gives each descriptor such a key whole: RCUTENV of a DM_mJoin line,
 * the keys of the other TYPE line), as any other descriptor reads every
 * entry. */
struct KeyReader {
  std::string label;
  std::size_t first = 0;
  std::size_t count = 0;
  bool every = false;
};
std::vector<KeyReader> key_readers(const std::string &key,
                                   const tadah::core::Context &ctx) {
  std::vector<KeyReader> out;
  for (const char *type : {"TYPE2B", "TYPEMB"}) {
    const auto label = ctx.stored_value(type, 0);
    if (!label) continue;
    if (*label != "D2_mJoin" && *label != "DM_mJoin") {
      out.push_back(KeyReader{*label, 0, 0, true});
      continue;
    }
    const std::vector<tadah::core::Context> children =
        D_mJoin::parse_context(ctx, type);
    const std::string suffix = std::string(type).substr(4);
    std::set<std::string> split = {"RCUT" + suffix, "RCTYPE" + suffix};
    for (const auto &child : children) {
      const auto child_label = child.stored_value(type, 0);
      if (!child_label) continue;
      std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(*child_label));
      for (const auto &k : d->keys) split.insert(upper(k));
      for (const auto &k : d->data_keys) split.insert(upper(k));
    }
    const bool shared = split.count(key) == 0;
    std::size_t at = 0;
    for (const auto &child : children) {
      const auto child_label = child.stored_value(type, 0);
      if (!child_label) continue;
      if (shared) {
        out.push_back(KeyReader{*child_label, 0, 0, true});
        continue;
      }
      const std::size_t n = child.has_stored(key) ? child.size(key) : 0;
      out.push_back(KeyReader{*child_label, at, n, false});
      at += n;
    }
  }
  return out;
}

std::optional<OptimLowBoundRule> declared_rule_of(
    const std::string &key, const tadah::core::Context &ctx,
    std::optional<std::size_t> entry) {
  const std::string k = upper(key);
  for (const KeyReader &r : key_readers(k, ctx)) {
    if (entry && !r.every && !(*entry >= r.first && *entry < r.first + r.count))
      continue;
    const auto lo = declared_limit(r.label, k);
    if (lo) return rule_of_limit(k, *lo, r.label);
    // the descriptor of a joined line whose entry it is declares none
    if (entry && !r.every) return std::nullopt;
  }
  return std::nullopt;
}
}  // namespace

std::optional<OptimLowBoundRule> declared_low_bound_rule(
    const std::string &key) {
  const std::string k = upper(key);
  for (const auto &[label, limits] : declared_limits()) {
    const auto it = limits.find(k);
    if (it != limits.end()) return rule_of_limit(k, it->second, label);
  }
  return std::nullopt;
}

std::optional<OptimLowBoundRule> optim_box_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx) {
  if (auto r = optim_low_bound_rule(key, ctx)) return r;
  return declared_rule_of(key, ctx, std::nullopt);
}

std::optional<OptimLowBoundRule> optim_box_low_bound_rule(
    const std::string &key, const tadah::core::Context &ctx,
    std::size_t entry) {
  if (auto r = optim_low_bound_rule(key, ctx, entry)) return r;
  return declared_rule_of(key, ctx, entry);
}

}  // namespace models
}  // namespace tadah
