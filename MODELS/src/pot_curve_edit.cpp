#include <tadah/models/eam/pot_curve_edit.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <memory>

#include <tadah/core/logger.h>
#include <tadah/core/registry.h>
#include <tadah/models/cut_all.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/d_mjoin.h>
#include <tadah/models/descriptors/d2/d2_blip.h>
#include <tadah/models/descriptors/d2/d2_blip5.h>
#include <tadah/models/descriptors/d2/d2_knot5.h>
#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/core/number_format.h>

namespace tadah {
namespace models {
namespace potedit {

namespace {

// (Bump support half-widths live in basis_kind.h: models::bump_halfwidth.)

std::string fmt(double v) {
  std::ostringstream os;
  os.precision(12);
  os << v;
  return os.str();
}

std::vector<double> read_grid(const core::Context &c, const std::string &key) {
  core::v_type v;
  D_Base::get_grid(c, key, v);
  return std::vector<double>(v.begin(), v.end());
}

std::vector<int> read_degs(const core::Context &c, const std::string &key,
                           std::size_t n_expected) {
  std::vector<int> d;
  if (!c.contains(key) || c.size(key) == 0) return d;
  d.resize(c.size(key));
  for (std::size_t i = 0; i < d.size(); ++i) {
    d[i] = static_cast<int>(c.get<double>(key, i));
    if (d[i] != 3 && d[i] != 5)
      throw std::runtime_error("pot edit: " + key + " entry " +
                               std::to_string(i + 1) + " is " +
                               std::to_string(d[i]) + " (allowed: 3, 5).");
  }
  if (d.size() != n_expected)
    throw std::runtime_error("pot edit: " + key + " has " +
                             std::to_string(d.size()) + " entries but the " +
                             "grid has " + std::to_string(n_expected) + ".");
  return d;
}

const char *d2_name(BasisKind k) {
  switch (k) {
    case BasisKind::Knot5: return "D2_Knot5";
    case BasisKind::Blip5: return "D2_Blip5";
    case BasisKind::Blip:  return "D2_Blip";
    default: break;
  }
  throw std::runtime_error("pot edit: unsupported two-body basis family.");
}

void add_all(core::Context &c, const std::string &key,
             const std::vector<double> &vals) {
  c.remove(key);
  for (double v : vals) c.add(key, v);
}

void add_degs(core::Context &c, const std::string &key,
              const std::vector<int> &deg) {
  c.remove(key);
  for (int d : deg) c.add(key, static_cast<double>(d));
}

// Raw basis function k of a curve at x, with CurveView::value's evaluator
// (for the bump families the bare radial form IS the embedding form; a
// RHOFORM MONOTONE density evaluates the integrated bump S_k).
double basis_value_bare(const CurveView &c, std::size_t k, double x) {
  if (c.form == RhoForm::Monotone)
    return radial_step_value_bare(c.family, x, c.width(k), c.mu[k]);
  return c.right_active
      ? embedding_value(c.family, x, c.width(k), c.mu[k], c.degree(k))
      : radial_value_bare(c.family, x, c.width(k), c.mu[k], c.degree(k));
}

double basis_value(const CurveView &c,std::size_t k,double x) {
  if(c.rcut>0 && x>c.rcut)return 0;
  return basis_value_bare(c,k,x)*(c.spatial_cutoff?c.spatial_cutoff->calc(x):1);
}
}  // namespace

// ---------------------------------------------------------------------------
// CurveView evaluation
// ---------------------------------------------------------------------------

double CurveView::support_infinity() {
  return std::numeric_limits<double>::max();
}

namespace {
double bare_curve(const CurveView &c,double x,int order) {
  double sum=0;
  for(std::size_t k=0;k<c.n();++k) {
    double v;
    if(c.form==RhoForm::Monotone) {
      v=order==0?radial_step_value_bare(c.family,x,c.width(k),c.mu[k]):
        order==1?radial_step_deriv_bare(c.family,x,c.width(k),c.mu[k]):
                 radial_step_deriv2_bare(c.family,x,c.width(k),c.mu[k]);
    } else if(c.right_active) {
      v=order==0?embedding_value(c.family,x,c.width(k),c.mu[k],c.degree(k)):
        order==1?embedding_deriv(c.family,x,c.width(k),c.mu[k],c.degree(k)):
                 embedding_deriv2(c.family,x,c.width(k),c.mu[k],c.degree(k));
    } else {
      v=order==0?radial_value_bare(c.family,x,c.width(k),c.mu[k],c.degree(k)):
        order==1?radial_deriv_bare(c.family,x,c.width(k),c.mu[k],c.degree(k)):
                 radial_deriv2_bare(c.family,x,c.width(k),c.mu[k],c.degree(k));
    }
    if(order==0 && c.zero_ref)v-=basis_value(c,k,0);
    sum+=c.coeff[k]*v;
  }
  return sum;
}
}
double CurveView::value(double x) const {
  if((rcut>0 && x>rcut) || (zero_ref && x==0))return 0;
  return bare_curve(*this,x,0)*(spatial_cutoff?spatial_cutoff->calc(x):1);
}
double CurveView::deriv(double x) const {
  if(rcut>0 && x>rcut)return 0;
  double d=bare_curve(*this,x,1);
  return spatial_cutoff?d*spatial_cutoff->calc(x)+bare_curve(*this,x,0)*spatial_cutoff->calc_prime(x):d;
}
double CurveView::deriv2(double x) const {
  if(rcut>0 && x>rcut)return 0;
  double dd=bare_curve(*this,x,2);
  return spatial_cutoff?dd*spatial_cutoff->calc(x)+2*bare_curve(*this,x,1)*spatial_cutoff->calc_prime(x)+bare_curve(*this,x,0)*spatial_cutoff->calc_double_prime(x):dd;
}

std::pair<double, double> CurveView::support(std::size_t k) const {
  if (family == BasisKind::Knot5) {
    return right_active
        ? std::make_pair(mu[k], support_infinity())
        : std::make_pair(-support_infinity(), mu[k]);
  }
  const double w = bump_halfwidth(family) / width(k);
  if (form == RhoForm::Monotone)  // S_k = 1 below the bump
    return {-support_infinity(), mu[k] + w};
  return {mu[k] - w, mu[k] + w};
}

// ---------------------------------------------------------------------------
// Parse / rebuild
// ---------------------------------------------------------------------------

PotEAMView parse_pot_view(const core::Context &ctx) {
  PotEAMView v;
  v.ctx = ctx;

  if (!ctx.contains("TYPE2B") || ctx.size("TYPE2B") == 0)
    throw std::runtime_error(
        "pot edit: the potential must carry a TYPE2B key (a knot/blip pair "
        "descriptor is the editable part).");

  // ---- editing safety: WEIGHTS must be [pair block | many-body block] ----
  // BIAS prepends intercept columns and NORM stores column-aligned
  // NMEAN/NSTDEV — neither survives inserting/removing a column.
  if (ctx.contains("BIAS") && ctx.size("BIAS") > 0 && ctx.get<bool>("BIAS"))
    throw std::runtime_error(
        "pot edit: BIAS true is not supported — the intercept columns "
        "precede the pair block in WEIGHTS, so a basis edit cannot keep the "
        "layout consistent.");
  if (ctx.contains("NORM") && ctx.size("NORM") > 0 && ctx.get<bool>("NORM"))
    throw std::runtime_error(
        "pot edit: NORM true is not supported — the stored normalisation "
        "vectors are column-aligned with WEIGHTS and cannot survive a basis "
        "edit; a basis edit takes a potential fitted with NORM false.");

  // ---- two-body family ----
  const std::string t2 = ctx.get<std::string>("TYPE2B", 0);
  if (t2 == "D2_Knot5")      v.phi.family = BasisKind::Knot5;
  else if (t2 == "D2_Blip")  v.phi.family = BasisKind::Blip;
  else if (t2 == "D2_Blip5") v.phi.family = BasisKind::Blip5;
  else
    throw std::runtime_error(
        "pot edit: unsupported two-body descriptor '" + t2 +
        "' — only refit families (D2_Knot5, D2_Blip, D2_Blip5) can be "
        "edited (Gaussian/D2_BP and D2_mJoin are not supported).");

  // ---- many-body situation: absent/inactive, DM_REAM, or foreign ----
  // The many-body descriptor only contributes WEIGHTS columns when INITMB is
  // true (DC_Selector builds DM_Dummy otherwise), so an absent/false INITMB
  // means a pure two-body potential regardless of a stray TYPEMB key.
  const bool mb_active = ctx.contains("INITMB") && ctx.size("INITMB") > 0 &&
                         ctx.get<bool>("INITMB");
  BasisKind mbk = BasisKind::Blip;
  if (mb_active) {
    if (!ctx.contains("TYPEMB") || ctx.size("TYPEMB") == 0)
      throw std::runtime_error(
          "pot edit: INITMB is true but the potential has no TYPEMB key.");
    v.mb_name = ctx.get<std::string>("TYPEMB", 0);
    if (v.mb_name == "DM_REAM") {
      v.mb = MBKind::Ream;
      mbk = basis_kind_from_code(ctx.get<int>("TYPEMB", 1));
      if (mbk == BasisKind::Gaussian)
        throw std::runtime_error(
            "pot edit: the Gaussian DM_REAM basis is not supported.");
    } else {
      v.mb = MBKind::Foreign;  // pair channel editable; tail kept verbatim
    }
  } else {
    v.mb = MBKind::None;
  }
  // RHOFORM is mandatory wherever a DM_REAM block is active (also inside a
  // foreign DM_mJoin, whose keys the rebuild writes back): a key-less
  // potential gets the migration text here, not a rebuild self-check error.
  if (context_has_ream(ctx)) {
    const RhoForm form = require_rhoform(ctx, "pot edit");
    if (v.mb == MBKind::Ream) v.rho.form = form;
  }

  if (!ctx.contains("ATOM") || ctx.size("ATOM") == 0)
    throw std::runtime_error("pot edit: the potential has no ATOM key.");
  v.element = ctx.get<std::string>("ATOM", 0);

  // ---- phi ----
  v.phi.right_active = false;
  v.phi.mu = read_grid(ctx, "CGRID2B");
  if (v.phi.mu.empty())
    throw std::runtime_error("pot edit: CGRID2B is empty.");
  if (v.phi.family == BasisKind::Knot5) {
    if (ctx.contains("SGRID2B") && ctx.size("SGRID2B") > 0)
      throw std::runtime_error(
          "pot edit: SGRID2B must be absent for the knot family "
          "(eta is a pure gauge).");
    v.phi.deg = read_degs(ctx, "KDEG2B", v.phi.mu.size());
  } else {
    v.phi.eta = read_grid(ctx, "SGRID2B");
    if (v.phi.eta.size() != v.phi.mu.size())
      throw std::runtime_error("pot edit: SGRID2B/CGRID2B size mismatch.");
  }
  v.phi.rcut = ctx.get<double>("RCUT2B");
  v.phi.spatial_cutoff.reset(core::factory<Cut_Base,double&>(ctx.get<std::string>("RCTYPE2B"),v.phi.rcut));

  if (v.mb == MBKind::Ream) {
    // ---- rho ----
    v.rho.family = mbk;
    v.rho.right_active = false;
    v.rho.mu = read_grid(ctx, "CGRIDMB");
    v.rho.coeff = read_grid(ctx, "AMPGRIDMB");
    if (v.rho.mu.empty() || v.rho.coeff.size() != v.rho.mu.size())
      throw std::runtime_error(
          "pot edit: CGRIDMB/AMPGRIDMB must be non-empty and equal-sized.");
    if (mbk == BasisKind::Knot5) {
      v.rho.deg = read_degs(ctx, "KDEGMB", v.rho.mu.size());
    } else {
      v.rho.eta = read_grid(ctx, "SGRIDMB");
      if (v.rho.eta.size() != v.rho.mu.size())
        throw std::runtime_error("pot edit: SGRIDMB/CGRIDMB size mismatch.");
    }
    v.rho.rcut = ctx.get<double>("RCUTMB");
  v.rho.spatial_cutoff.reset(core::factory<Cut_Base,double&>(ctx.get<std::string>("RCTYPEMB"),v.rho.rcut));
    // The density must be valid in its form before it is edited: the checks
    // DM_REAM applies on load (amplitude signs, widths, the MONOTONE basis
    // and cutoff, the density pair weights), with their messages.
    if (rhoform_is_positive(v.rho.form)) {
      try {
        ReamDensity::validate_context(ctx);
      } catch (const DensityFormError &e) {
        throw DensityFormError(std::string("pot edit: ") + e.what());
      }
    }

    // ---- F (embedding) ----
    v.F.family = mbk;
    v.F.right_active = (mbk == BasisKind::Knot5);
    // EMBF0 is a property of the potential (absent -> the default ZERO):
    // the F view evaluates the embedding reference DM_REAM evaluates.
    try {
      v.embf0 = stored_embf0(ctx).value_or(EmbF0Mode::Zero);
    } catch (const std::exception &e) {
      throw std::runtime_error(std::string("pot edit: ") + e.what());
    }
    v.F.zero_ref = (v.embf0 == EmbF0Mode::Zero);
    v.F.mu = read_grid(ctx, "CEMBFUNC");
    if (v.F.mu.empty())
      throw std::runtime_error("pot edit: CEMBFUNC is empty.");
    if (mbk == BasisKind::Knot5) {
      v.F.deg = read_degs(ctx, "KDEGEMB", v.F.mu.size());
    } else {
      v.F.eta = read_grid(ctx, "SEMBFUNC");
      if (v.F.eta.size() != v.F.mu.size())
        throw std::runtime_error("pot edit: SEMBFUNC/CEMBFUNC size mismatch.");
    }
    v.F.rcut = 0.0;  // no r cutoff on the embedding
  }

  // ---- weights: [phi | many-body block] ----
  const std::size_t K = ctx.contains("WEIGHTS") ? ctx.size("WEIGHTS") : 0;
  const std::size_t n2b = v.phi.mu.size();
  if (v.mb == MBKind::Ream) {
    const std::size_t nEc = v.F.mu.size();
    if (K != n2b + nEc)
      throw std::runtime_error(
          "pot edit: WEIGHTS has " + std::to_string(K) + " entries, expected " +
          std::to_string(n2b) + " (CGRID2B) + " + std::to_string(nEc) +
          " (CEMBFUNC) = " + std::to_string(n2b + nEc) + ".");
  } else if (v.mb == MBKind::None) {
    if (K != n2b)
      throw std::runtime_error(
          "pot edit: WEIGHTS has " + std::to_string(K) + " entries but the "
          "pure two-body potential has " + std::to_string(n2b) +
          " CGRID2B basis functions (one weight each).");
  } else {  // Foreign: pair block first, opaque many-body tail after it
    if (K < n2b)
      throw std::runtime_error(
          "pot edit: WEIGHTS has " + std::to_string(K) + " entries — fewer "
          "than the " + std::to_string(n2b) + " CGRID2B basis functions.");
  }
  std::vector<double> w(K);
  for (std::size_t i = 0; i < K; ++i) w[i] = ctx.get<double>("WEIGHTS", i);
  v.phi.coeff.assign(w.begin(), w.begin() + n2b);
  if (v.mb == MBKind::Ream)
    v.F.coeff.assign(w.begin() + n2b, w.end());
  else if (v.mb == MBKind::Foreign)
    v.tail.assign(w.begin() + n2b, w.end());

  return v;
}

core::Context rebuild_pot_context(const PotEAMView &v) {
  core::Context c = v.ctx;
  const std::string el = v.element;
  const bool knot2b = (v.phi.family == BasisKind::Knot5);
  const bool ream = (v.mb == MBKind::Ream);
  const bool knotmb = ream && (v.rho.family == BasisKind::Knot5);

  // ---- two-body: <name> <nC> <nKdeg|nS> El El ----
  c.remove("TYPE2B");
  c.add("TYPE2B", d2_name(v.phi.family));
  c.add("TYPE2B", static_cast<int>(v.phi.mu.size()));
  c.add("TYPE2B", static_cast<int>(knot2b ? v.phi.deg.size()
                                          : v.phi.eta.size()));
  c.add("TYPE2B", el);
  c.add("TYPE2B", el);
  add_all(c, "CGRID2B", v.phi.mu);
  c.remove("SGRID2B");
  if (!knot2b) add_all(c, "SGRID2B", v.phi.eta);
  add_degs(c, "KDEG2B", v.phi.deg);

  // ---- many-body ----
  // DM_REAM is regenerated from the (possibly edited) rho/F views; a pure
  // two-body potential has nothing to write; a FOREIGN many-body descriptor
  // is left untouched (its keys survive in the cloned Context and its
  // WEIGHTS block rides along in v.tail) — only EMBF0 follows the writers'
  // rule below.
  if (ream) {
    // DM_REAM <bcode> <nC> <nS> <nAmp> <nEc> <nEs> <nKd> <nKe> El El
    c.remove("TYPEMB");
    c.add("TYPEMB", "DM_REAM");
    c.add("TYPEMB", basis_code(v.rho.family));
    c.add("TYPEMB", static_cast<int>(v.rho.mu.size()));
    c.add("TYPEMB", static_cast<int>(knotmb ? 0 : v.rho.eta.size()));
    c.add("TYPEMB", static_cast<int>(v.rho.coeff.size()));
    c.add("TYPEMB", static_cast<int>(v.F.mu.size()));
    c.add("TYPEMB", static_cast<int>(knotmb ? 0 : v.F.eta.size()));
    c.add("TYPEMB", static_cast<int>(v.rho.deg.size()));
    c.add("TYPEMB", static_cast<int>(v.F.deg.size()));
    c.add("TYPEMB", el);
    c.add("TYPEMB", el);
    add_all(c, "CGRIDMB", v.rho.mu);
    c.remove("SGRIDMB");
    if (!knotmb) add_all(c, "SGRIDMB", v.rho.eta);
    add_all(c, "AMPGRIDMB", v.rho.coeff);
    add_degs(c, "KDEGMB", v.rho.deg);
    add_all(c, "CEMBFUNC", v.F.mu);
    c.remove("SEMBFUNC");
    if (!knotmb) add_all(c, "SEMBFUNC", v.F.eta);
    add_degs(c, "KDEGEMB", v.F.deg);
    // every DM_REAM potential tadah writes declares its embedding reference
    // and its density form
    c.remove("EMBF0");
    c.add("EMBF0", std::string(embf0_name(v.embf0)));
    c.remove("RHOFORM");
    c.add("RHOFORM", std::string(rhoform_name(v.rho.form)));
  } else {
    // pure two-body / foreign: the writers' rule (a stray key is dropped;
    // a foreign DM_mJoin that carries DM_REAM gets the explicit EMBF0 and
    // keeps its RHOFORM, canonicalised)
    normalise_embf0_key(c);
    normalise_rhoform_key(c);
  }

  // ---- weights + HPO coefficient-search seed (mirrors build_pot_context) ----
  std::vector<double> w(v.phi.coeff);
  if (ream) w.insert(w.end(), v.F.coeff.begin(), v.F.coeff.end());
  else      w.insert(w.end(), v.tail.begin(), v.tail.end());
  add_all(c, "WEIGHTS", w);
  c.remove("FIXINDEX");
  c.remove("FIXWEIGHT");
  if (!w.empty()) {
    c.add("FIXINDEX", std::string("1-") + std::to_string(w.size()));
    for (double x : w) c.add("FIXWEIGHT", x);
  }

  // ---- RCUT: derived for knot channels, preserved for bump channels ----
  if (knot2b) {
    c.remove("RCUT2B");
    c.add("RCUT2B", *std::max_element(v.phi.mu.begin(), v.phi.mu.end()));
  } else {
    c.remove("RCUT2B");
    c.add("RCUT2B", v.phi.rcut);
  }
  if (knotmb) {
    c.remove("RCUTMB");
    c.add("RCUTMB", *std::max_element(v.rho.mu.begin(), v.rho.mu.end()));
  } else if (ream) {
    c.remove("RCUTMB");
    c.add("RCUTMB", v.rho.rcut);
  }
  // None/Foreign: RCUTMB (if any) stays exactly as it was.

  // ---- self-check: the rebuilt Context must construct cleanly ----
  try {
    core::Context probe = c;
    if (!probe.contains("VERBOSE")) probe.add("VERBOSE", 0);
    if (knot2b) { D2_Knot5 d(probe); }
    else if (v.phi.family == BasisKind::Blip5) { D2_Blip5 d(probe); }
    else { D2_Blip d(probe); }
    if (ream) { DM_REAM m(probe); }
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("pot edit: rebuilt potential failed descriptor "
                    "validation (internal error): ") + e.what());
  }
  return c;
}

// ---------------------------------------------------------------------------
// RCUT derivation (F2)
// ---------------------------------------------------------------------------

std::optional<double> derived_knot_rcut(const core::Context &ctx,
                                        const std::string &suffix) {
  try {
    if (suffix == "2B") {
      if (!ctx.contains("TYPE2B") || !ctx.contains("CGRID2B") ||
          ctx.size("CGRID2B") == 0)
        return std::nullopt;
      if (ctx.get<std::string>("TYPE2B", 0) != "D2_Knot5") return std::nullopt;
      const auto g = read_grid(ctx, "CGRID2B");
      return *std::max_element(g.begin(), g.end());
    }
    if (suffix == "MB") {
      if (!ctx.contains("TYPEMB") || !ctx.contains("CGRIDMB") ||
          ctx.size("CGRIDMB") == 0)
        return std::nullopt;
      if (ctx.get<std::string>("TYPEMB", 0) != "DM_REAM") return std::nullopt;
      if (basis_kind_from_code(ctx.get<int>("TYPEMB", 1)) != BasisKind::Knot5)
        return std::nullopt;
      const auto g = read_grid(ctx, "CGRIDMB");
      return *std::max_element(g.begin(), g.end());
    }
  } catch (const std::exception &) {
    return std::nullopt;  // malformed channel: not ours to police here
  }
  throw std::invalid_argument("derived_knot_rcut: suffix must be 2B or MB");
}

bool normalize_knot_rcut(core::Context &ctx, const std::string &who) {
  std::vector<KnotRcutNote> notes;
  const bool changed = normalize_knot_rcut(ctx, who, notes);
  for (const auto &n : notes) core::Logger::instance().log(n.level, n.text);
  return changed;
}

namespace {
/* The note of normalize_knot_rcut for a knot channel whose cutoff is
 * @p derived and whose stated cutoff is @p stated (@p have: one is stated)
 * under the cutoff @p rctype (its context and suffix @p c, @p sfx), the
 * channel named by @p where ("" for a line of one descriptor, else "TYPE2B
 * descriptor 1 (D2_Knot5): ") and, for a descriptor of a joined line, its
 * entry of RCUT2B or RCUTMB (@p entry, 1-based as an OPTIM line numbers the
 * whole key; 0 for a line of one descriptor). */
KnotRcutNote knot_rcut_note(const core::Context &c, const char *sfx,
                            const std::string &who, const std::string &where,
                            std::size_t entry, bool have, double stated,
                            double derived) {
  const std::string key = std::string("RCUT") + sfx;
  // The key as the note names it: "RCUT2B (1)" for an entry of a joined
  // line, which holds the cutoff of each of its descriptors.
  const std::string key_entry =
      entry ? key + " (" + std::to_string(entry) + ")" : key;
  // Only a real mismatch is worth a WARNING; a gap within 1e-9 relative
  // (e.g. the ~3e-13 left by the <= 1.5.0 writers, which rounded CGRID to
  // 12 decimals but not RCUT) is fixed at INFO.
  const bool mismatch =
      !have || std::fabs(stated - derived) >
                   1e-9 * std::max(1.0, std::fabs(derived));
  // Shortest exact text: the values may differ in the 13th digit.
  const std::string d_txt = core::format_roundtrip(derived);
  // The channel whose cutoff this is, as the configuration names it: a
  // D2_Knot5 of a joined line is one descriptor of its line.
  const std::string channel = std::string(sfx) == "2B"
                                  ? (entry ? "a D2_Knot5 descriptor"
                                           : "a D2_Knot5 line")
                                  : "a knot DM_REAM density";
  const std::string set_by = ", which the CGRID" + std::string(sfx) +
                             " knot positions set.";
  std::ostringstream msg;
  msg << who << ": " << where << key_entry << " ";
  if (!have) {
    msg << "is missing: the cutoff of " << channel
        << " is its largest knot, " << d_txt << set_by;
  } else if (stated < derived) {
    msg << core::format_roundtrip(stated) << " is below the largest knot ("
        << d_txt << "): the cutoff of " << channel
        << " is its largest knot, " << d_txt << set_by;
  } else {
    msg << core::format_roundtrip(stated) << " is above the largest knot ("
        << d_txt << "): the knot basis vanishes at its largest knot, so "
        << "the cutoff of " << channel << " is " << d_txt << set_by;
  }
  // pair_style tadah (Tadah.LAMMPS, never modified here) reads the stated
  // RCUT from a potential file and does not re-derive it. Under a smooth
  // cutoff the RCUT value enters f_c(r) of every pair, so LAMMPS then
  // differs from train/predict/hpo on that file, even for a sub-1e-9 gap:
  // say so at default visibility. A hard cutoff (Cut_Dummy) past the
  // largest knot is prediction-exact: no note, INFO stays INFO. A
  // descriptor of a joined line (@p where names it) has no such note:
  // tadah predict and tadah refit --edit, as LAMMPS, take the cutoff a
  // potential file states for it (normalize_potential_knot_rcut).
  const bool smooth_note =
      have && where.empty() && !channel_cutoff_is_discontinuous(c, sfx);
  if (smooth_note) {
    std::ostringstream gap;
    gap << std::setprecision(2)
        << std::fabs(stated - derived) / std::max(1.0, std::fabs(derived));
    msg << " Under the smooth RCTYPE" << sfx << " the cutoff value enters "
        << "every pair term, and pair_style tadah (LAMMPS) reads the stated "
        << key << " without re-deriving it: on a potential file stating "
        << core::format_roundtrip(stated) << ", LAMMPS differs from tadah "
        << "train/predict/hpo (relative " << key << " gap " << gap.str()
        << ") until the potential is re-written — a potential written by "
        << "tadah train, tadah refit --edit or tadah hpo (best_pot.tadah) "
        << "carries the derived value.";
  }
  return {(mismatch || smooth_note) ? core::LogLevel::WARNING
                                    : core::LogLevel::INFO,
          msg.str()};
}

/* Whether the TYPE`sfx` line of @p ctx joins descriptors (D2_mJoin,
 * DM_mJoin; a TYPEMB line configured by DESCRIPTOR blocks does not). */
bool joins_descriptors(const core::Context &ctx, const char *sfx) {
  const std::string type_key = std::string("TYPE") + sfx;
  if (!ctx.contains(type_key) || ctx.size(type_key) == 0) return false;
  if (std::string(sfx) == "MB" && !ctx.blocks("DESCRIPTOR").empty())
    return false;
  return ctx.get<std::string>(type_key, 0).find("mJoin") != std::string::npos;
}

/* normalize_knot_rcut, and with @p joined false normalize_potential_knot_rcut:
 * a joined line is left as stated. */
bool normalize_knot_rcut_lines(core::Context &ctx, const std::string &who,
                               std::vector<KnotRcutNote> &notes,
                               bool joined) {
  bool changed = false;
  for (const char *sfx : {"2B", "MB"}) {
    const std::string key = std::string("RCUT") + sfx;
    // Exact: every consumer (train, predict, refit --edit, analysis
    // check_bounds, hpo startup and hpo's per-evaluation derivation) must
    // use the same cutoff, so ANY difference is rewritten (a joined line of
    // a potential is left as stated, below).
    if (joins_descriptors(ctx, sfx)) {
      // A potential keeps the cutoffs of its joined line: its coefficients
      // were fitted with them (every fit before Tadah! 1.6.0 kept them).
      if (!joined) continue;
      // A joined line: each knot descriptor's own RCUT value, derived from
      // its own knots (D_mJoin::parse_context gives each descriptor its
      // RCUT value and its share of CGRID, in the order of the line).
      std::vector<core::Context> parts;
      try {
        parts = D_mJoin::parse_context(ctx, std::string("TYPE") + sfx);
      } catch (const std::exception &) {
        continue;  // a line D_mJoin refuses: its own error, when it is built
      }
      std::vector<double> values;
      std::size_t at = 0;  // the entry of the part's RCUT value
      for (std::size_t k = 0; k < parts.size(); ++k) {
        const core::Context &part = parts[k];
        const std::size_t n = part.contains(key) ? part.size(key) : 0;
        const auto derived = derived_knot_rcut(part, sfx);
        if (derived && n == 1 && part.get<double>(key) != *derived) {
          if (values.empty()) {
            values.resize(ctx.size(key));
            for (std::size_t i = 0; i < values.size(); ++i)
              values[i] = ctx.get<double>(key, i);
          }
          if (at < values.size()) {
            const std::string where =
                std::string("TYPE") + sfx + " descriptor " +
                std::to_string(k + 1) + " (" +
                part.get<std::string>(std::string("TYPE") + sfx, 0) + "): ";
            notes.push_back(knot_rcut_note(part, sfx, who, where, at + 1,
                                           true, part.get<double>(key),
                                           *derived));
            values[at] = *derived;
          }
        }
        at += n;
      }
      if (!values.empty()) {
        ctx.remove(key);
        ctx.add(key, values);
        changed = true;
      }
      continue;
    }
    const auto derived = derived_knot_rcut(ctx, sfx);
    if (!derived) continue;
    const bool have = ctx.contains(key) && ctx.size(key) > 0;
    const double stated = have ? ctx.get<double>(key) : -1.0;
    if (have && stated == *derived) continue;
    notes.push_back(
        knot_rcut_note(ctx, sfx, who, "", 0, have, stated, *derived));
    ctx.remove(key);
    ctx.add(key, *derived);
    changed = true;
  }
  return changed;
}
}  // namespace

bool normalize_knot_rcut(core::Context &ctx, const std::string &who,
                         std::vector<KnotRcutNote> &notes) {
  return normalize_knot_rcut_lines(ctx, who, notes, true);
}

bool normalize_potential_knot_rcut(core::Context &ctx,
                                   const std::string &who) {
  std::vector<KnotRcutNote> notes;
  const bool changed = normalize_knot_rcut_lines(ctx, who, notes, false);
  for (const auto &n : notes) core::Logger::instance().log(n.level, n.text);
  return changed;
}

std::optional<double> bump_channel_halfwidth(const core::Context &ctx,
                                             const std::string &suffix) {
  try {
    if (suffix == "2B") {
      if (!ctx.contains("TYPE2B") || ctx.size("TYPE2B") == 0)
        return std::nullopt;
      const std::string n = ctx.get<std::string>("TYPE2B", 0);
      if (n == "D2_Blip") return bump_halfwidth(BasisKind::Blip);
      if (n == "D2_Blip5") return bump_halfwidth(BasisKind::Blip5);
      return std::nullopt;
    }
    if (suffix == "MB") {
      if (!ctx.contains("TYPEMB") || ctx.size("TYPEMB") == 0)
        return std::nullopt;
      if (ctx.get<std::string>("TYPEMB", 0) != "DM_REAM") return std::nullopt;
      const double w =
          bump_halfwidth(basis_kind_from_code(ctx.get<int>("TYPEMB", 1)));
      if (w > 0.0) return w;
      return std::nullopt;
    }
  } catch (const std::exception &) {
    return std::nullopt;  // malformed channel: not ours to police here
  }
  throw std::invalid_argument(
      "bump_channel_halfwidth: suffix must be 2B or MB");
}

bool channel_cutoff_is_discontinuous(const core::Context &ctx,
                                     const std::string &suffix) {
  const std::string key = "RCTYPE" + suffix;
  try {
    if (!ctx.contains(key) || ctx.size(key) == 0)
      return true;  // D2_Base/DM_Base ctor default: Cut_Dummy
    double probe_rcut = 1.0;  // kind() is rcut-independent
    std::unique_ptr<Cut_Base> fc(core::factory<Cut_Base, double &>(
        ctx.get<std::string>(key, 0), probe_rcut));
    return !fc || fc->kind() == Cut_Base::Kind::Discontinuous;
  } catch (const std::exception &) {
    return true;  // unknown cutoff name: conservatively a hard step
  }
}

// ---------------------------------------------------------------------------
// ADD
// ---------------------------------------------------------------------------

AddReport add_basis(CurveView &c, const AddSpec &s, bool radial,
                    bool hard_cutoff) {
  AddReport rep;
  const bool knot = (c.family == BasisKind::Knot5);
  if (c.n() == 0)
    throw std::runtime_error("pot edit: cannot edit an empty curve.");

  if (knot && s.eta)
    throw std::runtime_error(
        "pot edit: --eta is rejected for the knot family — the width is a "
        "pure gauge (pinned 1); the knot position is the only shape "
        "parameter.");
  if (!knot && s.deg)
    throw std::runtime_error(
        "pot edit: --deg only applies to the knot family (Knot5/Knot3); "
        "bump families have no per-function degree.");
  if (s.eta && *s.eta <= 0.0)
    throw std::runtime_error("pot edit: --eta must be positive.");
  rep.deg_used = s.deg.value_or(5);
  if (rep.deg_used != 3 && rep.deg_used != 5)
    throw std::runtime_error("pot edit: --deg must be 3 or 5.");
  if (radial && s.centre <= 0.0)
    throw std::runtime_error(
        "pot edit: a radial basis-function centre must be positive (got " +
        fmt(s.centre) + ").");

  if (!knot) {
    // width default: mean of the (up to) two nearest neighbours' widths.
    if (s.eta) {
      rep.eta_used = *s.eta;
    } else {
      std::vector<std::size_t> idx(c.n());
      for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
      std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return std::fabs(c.mu[a] - s.centre) < std::fabs(c.mu[b] - s.centre);
      });
      double acc = 0.0;
      std::size_t m = std::min<std::size_t>(2, idx.size());
      for (std::size_t i = 0; i < m; ++i) acc += c.eta[idx[i]];
      rep.eta_used = acc / static_cast<double>(m);
    }
    const double w = bump_halfwidth(c.family) / rep.eta_used;
    // A MONOTONE step is 1 below its bump and 0 beyond its outer edge
    // centre + w: one lying beyond rcut is 1 on the whole range, so only its
    // outer edge is judged, in the words of a step.
    const bool step = (c.form == RhoForm::Monotone);
    if (radial && c.rcut > 0.0) {
      if (!step && s.centre - w >= c.rcut)
        throw std::runtime_error(
            "pot edit: the new basis function's support [" +
            fmt(s.centre - w) + ", " + fmt(s.centre + w) +
            "] lies entirely beyond RCUT = " + fmt(c.rcut) +
            " — it could never contribute (RCUT is fixed for blip "
            "potentials: the hard cutoff is part of the fitted model).");
      if (s.centre + w > c.rcut + bump_rcut_tol(c.rcut)) {
        if (hard_cutoff) {
          std::string hint =
              s.centre < c.rcut
                  ? "raise --eta to at least " +
                        fmt(bump_halfwidth(c.family) / (c.rcut - s.centre)) +
                        " or move the centre inward"
                  : "move the centre below RCUT";
          if (step)
            throw std::runtime_error(
                "pot edit: the new step's outer edge centre + w/eta = " +
                fmt(s.centre + w) + " lies beyond RCUT = " + fmt(c.rcut) +
                " — under the hard Cut_Dummy cutoff the step would be "
                "truncated into an energy jump at the cutoff (RCUT is never "
                "changed for blip potentials); " + hint + ".");
          throw std::runtime_error(
              "pot edit: the new basis function's support [" +
              fmt(s.centre - w) + ", " + fmt(s.centre + w) +
              "] crosses RCUT = " + fmt(c.rcut) +
              " — under the hard Cut_Dummy cutoff the overhang would be "
              "truncated into an energy step at the cutoff (RCUT is never "
              "changed for blip potentials); " + hint + ".");
        }
        if (step && s.centre - w >= c.rcut)
          rep.warnings.push_back(
              "the new step equals 1 on the whole range [0, RCUT = " +
              fmt(c.rcut) + "]: its amplitude a adds a f_c(r) to the "
              "density.");
        else if (step)
          rep.warnings.push_back(
              "the new step's outer edge centre + w/eta = " +
              fmt(s.centre + w) + " lies beyond RCUT = " + fmt(c.rcut) +
              "; the smooth cutoff damps the step to zero there.");
        else
          rep.warnings.push_back(
              "the new basis function's support crosses RCUT = " +
              fmt(c.rcut) +
              "; the smooth cutoff damps the overhang to zero there.");
      }
    }
  } else if (radial) {
    const double mx = *std::max_element(c.mu.begin(), c.mu.end());
    if (s.centre > mx)
      rep.warnings.push_back(
          "the new knot at " + fmt(s.centre) +
          " is beyond the current largest knot (" + fmt(mx) +
          ") — the effective cutoff (RCUT) grows to " + fmt(s.centre) +
          "; downstream HPO/MD neighbour lists must cover it.");
  }

  // Append; grids are positional and index-aligned with the coefficients,
  // sortedness is not required by any consumer.
  rep.index = c.n();
  c.mu.push_back(s.centre);
  c.coeff.push_back(0.0);
  if (!knot) {
    c.eta.push_back(rep.eta_used);
  } else if (rep.deg_used == 3 && c.deg.empty()) {
    c.deg.assign(rep.index, 5);  // materialise: prior entries were quintic
    c.deg.push_back(3);
  } else if (!c.deg.empty()) {
    c.deg.push_back(rep.deg_used);
  }
  // (deg stays empty when everything, including the new knot, is quintic)
  return rep;
}

// ---------------------------------------------------------------------------
// REMOVE
// ---------------------------------------------------------------------------

RemoveReport remove_basis(CurveView &c, double centre_request, bool radial) {
  RemoveReport rep;
  if (c.n() == 0)
    throw std::runtime_error("pot edit: cannot edit an empty curve.");
  if (c.n() == 1)
    throw std::runtime_error(
        "pot edit: cannot remove the last basis function of a curve.");

  // nearest centre; ties -> lower index
  std::size_t r = 0;
  for (std::size_t k = 1; k < c.n(); ++k)
    if (std::fabs(c.mu[k] - centre_request) < std::fabs(c.mu[r] - centre_request))
      r = k;
  rep.removed_idx = r;
  rep.removed_mu = c.mu[r];
  rep.removed_coeff = c.coeff[r];

  const bool knot = (c.family == BasisKind::Knot5);
  if (knot && radial) {
    const double mx = *std::max_element(c.mu.begin(), c.mu.end());
    if (c.mu[r] >= mx) {
      double next = -CurveView::support_infinity();
      for (std::size_t k = 0; k < c.n(); ++k)
        if (k != r && c.mu[k] > next) next = c.mu[k];
      rep.warnings.push_back(
          "removing the OUTERMOST knot: the curve on (" + fmt(next) + ", " +
          fmt(c.mu[r]) + "] cannot be represented by the remaining basis, "
          "and the effective cutoff (RCUT) shrinks to " + fmt(next) + ".");
    }
  }

  if (rep.removed_coeff != 0.0) {
    // ---- zero-referenced F (EMBF0 ZERO): g_k(0) of every function ----
    // Filled only under zero_ref; the raw path does no extra work.
    std::vector<double> g0;
    if (c.zero_ref) {
      g0.resize(c.n());
      for (std::size_t k = 0; k < c.n(); ++k) g0[k] = basis_value(c, k, 0.0);
    }

    // ---- compensators: up to two remaining BFs nearest in centre ----
    std::vector<std::size_t> J;
    {
      std::vector<std::size_t> idx;
      for (std::size_t k = 0; k < c.n(); ++k)
        if (k != r) idx.push_back(k);
      std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return std::fabs(c.mu[a] - c.mu[r]) < std::fabs(c.mu[b] - c.mu[r]);
      });
      for (std::size_t i = 0; i < std::min<std::size_t>(2, idx.size()); ++i)
        J.push_back(idx[i]);
    }

    // ---- affected region ----
    const double r_lo = radial ? kCompensationRLoFrac * c.rcut : 0.0;
    double lo, hi;
    if (!knot) {
      const double w = bump_halfwidth(c.family) / c.width(r);
      // RHOFORM MONOTONE: the removed step contributed its full height
      // everywhere below the bump, down to the inner sampling bound
      lo = c.form == RhoForm::Monotone ? r_lo : c.mu[r] - w;
      hi = c.mu[r] + w;
      if (radial) {
        lo = std::max(lo, r_lo);
        hi = std::min(hi, c.rcut);
      }
    } else if (!c.right_active) {
      lo = r_lo;
      hi = c.mu[r];
    } else {
      double x_lo = *std::min_element(c.mu.begin(), c.mu.end());
      double x_hi = *std::max_element(c.mu.begin(), c.mu.end());
      lo = c.mu[r];
      hi = x_hi + 0.25 * std::max(x_hi - x_lo, 1e-12);
    }
    // A function "covers rho = 0" when |g(0)| exceeds a rounding-level
    // threshold (the bumps are peak-normalised; an argument landing on a
    // support edge by one ulp gives g(0) ~ 1e-48, which must not count).
    auto covers0 = [&](std::size_t k) {
      return c.zero_ref && std::fabs(g0[k]) > 1e-12;
    };
    bool comp_covers0 = false;
    for (std::size_t j : J) comp_covers0 = comp_covers0 || covers0(j);
    if (covers0(r) || comp_covers0) {
      // Removing c_r (g_r - g_r(0)) with g_r(0) != 0 changes F across the
      // WHOLE F domain: a bump leaves the constant +c_r g_r(0) beyond its
      // support, a right-active knot (mu_r < 0) is active over all of it.
      // Adjusting a compensator j with g_j(0) != 0 does the same with
      // -dc_j g_j(0). Sample [0, largest upper support of all functions];
      // right-active knots are open above: the convention of the knot branch
      // (largest knot + a quarter of the span, 0 included).
      double top = 0.0;
      bool open = false;
      for (std::size_t k = 0; k < c.n(); ++k) {
        const double up = c.support(k).second;
        if (up == CurveView::support_infinity()) open = true;
        else top = std::max(top, up);
      }
      if (open) {
        const double x_lo =
            std::min(0.0, *std::min_element(c.mu.begin(), c.mu.end()));
        const double x_hi =
            std::max(0.0, *std::max_element(c.mu.begin(), c.mu.end()));
        top = std::max(top, x_hi + 0.25 * std::max(x_hi - x_lo, 1e-12));
      }
      lo = 0.0;
      hi = top;
      rep.region_widened = true;
      rep.notes.push_back(
          std::string("EMBF0 ZERO: ") +
          (covers0(r)
               ? "the removed function has g(0) = " + fmt(g0[r]) +
                     " (c g(0) = " + fmt(c.coeff[r] * g0[r]) + ")"
               : std::string("a compensating function covers rho = 0")) +
          ", so the edit changes F across the whole F domain (a function "
          "with g(0) != 0 leaves the constant c g(0) beyond its support); "
          "the compensation region was widened to the F domain [0, " +
          fmt(top) + "].");
    }

    if (hi > lo) {
      // ---- stacked value/1st/2nd rows on a dense sample of [lo, hi] ----
      constexpr std::size_t N = 512;
      // Basis function k of the curve as evaluated (CurveView::value and
      // its derivatives): under a spatial cutoff f_c (a radial curve), the
      // tapered f_c g_k and its product-rule derivatives, so the
      // compensation minimises the residual of the tapered curve.
      auto bare = [&](std::size_t k, int order, double x) {
        if (c.form == RhoForm::Monotone) {  // the integrated bumps S_k
          if (order == 0) return radial_step_value_bare (c.family, x, c.width(k), c.mu[k]);
          if (order == 1) return radial_step_deriv_bare (c.family, x, c.width(k), c.mu[k]);
          return radial_step_deriv2_bare(c.family, x, c.width(k), c.mu[k]);
        }
        if (c.right_active) {
          if (order == 0) return embedding_value (c.family, x, c.width(k), c.mu[k], c.degree(k));
          if (order == 1) return embedding_deriv (c.family, x, c.width(k), c.mu[k], c.degree(k));
          return embedding_deriv2(c.family, x, c.width(k), c.mu[k], c.degree(k));
        }
        if (order == 0) return radial_value_bare (c.family, x, c.width(k), c.mu[k], c.degree(k));
        if (order == 1) return radial_deriv_bare (c.family, x, c.width(k), c.mu[k], c.degree(k));
        return radial_deriv2_bare(c.family, x, c.width(k), c.mu[k], c.degree(k));
      };
      auto bf = [&](std::size_t k, int order, double x) {
        // zero_ref: the value order is g_k(x) - g_k(0) (exactly 0 at x == 0);
        // the derivatives are the same in both embedding references. F has
        // no spatial cutoff, so this is the whole zero-referenced basis.
        if (order == 0 && c.zero_ref) return basis_value(c, k, x) - g0[k];
        if (!c.spatial_cutoff) return bare(k, order, x);
        Cut_Base &fc = *c.spatial_cutoff;  // Cut_Base evaluates non-const
        if (order == 0) return fc.calc(x) * bare(k, 0, x);
        if (order == 1)
          return fc.calc(x) * bare(k, 1, x) + fc.calc_prime(x) * bare(k, 0, x);
        return fc.calc(x) * bare(k, 2, x) +
               2.0 * fc.calc_prime(x) * bare(k, 1, x) +
               fc.calc_double_prime(x) * bare(k, 0, x);
      };

      const std::size_t nj = J.size();
      // per-order data: t[m][i] (lost contribution), A[m][i][j]
      std::array<std::vector<double>, 3> t;
      std::array<std::vector<std::array<double, 2>>, 3> A;
      std::array<double, 3> wm{0.0, 0.0, 0.0};
      for (int m = 0; m < 3; ++m) {
        t[m].resize(N);
        A[m].resize(N);
        double ss = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
          const double x =
              lo + (hi - lo) * static_cast<double>(i) / (N - 1);
          t[m][i] = c.coeff[r] * bf(r, m, x);
          for (std::size_t j = 0; j < nj; ++j) A[m][i][j] = bf(J[j], m, x);
          ss += t[m][i] * t[m][i];
        }
        wm[m] = std::sqrt(ss / N);
      }
      const double wmax = std::max({wm[0], wm[1], wm[2]});
      for (int m = 0; m < 3; ++m)
        wm[m] = (wm[m] > 1e-14 * wmax && wm[m] > 0.0) ? 1.0 / wm[m] : 0.0;

      // ---- normal equations over the compensators ----
      auto solve = [&](std::size_t nuse, std::array<double, 2> &dc) -> bool {
        double G[2][2] = {{0, 0}, {0, 0}}, h[2] = {0, 0};
        for (int m = 0; m < 3; ++m) {
          if (wm[m] == 0.0) continue;
          const double w2 = wm[m] * wm[m];
          for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t a = 0; a < nuse; ++a) {
              h[a] += w2 * A[m][i][a] * t[m][i];
              for (std::size_t b = 0; b < nuse; ++b)
                G[a][b] += w2 * A[m][i][a] * A[m][i][b];
            }
          }
        }
        if (nuse == 2) {
          const double det = G[0][0] * G[1][1] - G[0][1] * G[1][0];
          if (std::fabs(det) <= 1e-12 * std::fabs(G[0][0] * G[1][1]))
            return false;  // ill-conditioned: caller retries with one
          dc[0] = (h[0] * G[1][1] - h[1] * G[0][1]) / det;
          dc[1] = (h[1] * G[0][0] - h[0] * G[1][0]) / det;
          return true;
        }
        if (G[0][0] <= 0.0) return false;
        dc[0] = h[0] / G[0][0];
        dc[1] = 0.0;
        return true;
      };

      // ---- RHOFORM NONNEG / MONOTONE: bounded least squares ----
      // The same objective with every amplitude kept >= 0, c_j + dc_j >= 0:
      // a convex quadratic in <= 2 variables. Each compensator is either
      // free or at its bound (dc_j = -c_j); every such active set is solved
      // by its reduced normal equations, and the feasible candidate with
      // the smallest objective dc^T G dc - 2 h^T dc is the optimum. "Both at
      // the bound" is always feasible, so a solution always exists.
      // A compensator whose weighted column is negligible on the region (a
      // bump whose support misses it; rounding level relative to the
      // removed function's column) does not enter the objective: every dc_j
      // is optimal there, and holding it at the bound would wipe its
      // amplitude outside the region unseen. It stays fixed (dc_j = 0,
      // `inert`) and out of the enumeration; with no other compensator
      // there is no usable system (false: plain removal, as under SIGNED).
      auto solve_bounded = [&](std::array<double, 2> &dc,
                               std::vector<std::size_t> &held,
                               std::array<bool, 2> &inert) -> bool {
        double G[2][2] = {{0, 0}, {0, 0}}, h[2] = {0, 0}, gr = 0.0;
        for (int m = 0; m < 3; ++m) {
          if (wm[m] == 0.0) continue;
          const double w2 = wm[m] * wm[m];
          for (std::size_t i = 0; i < N; ++i) {
            gr += w2 * t[m][i] * t[m][i];
            for (std::size_t a = 0; a < nj; ++a) {
              h[a] += w2 * A[m][i][a] * t[m][i];
              for (std::size_t b = 0; b < nj; ++b)
                G[a][b] += w2 * A[m][i][a] * A[m][i][b];
            }
          }
        }
        // the removed function's own column: t / c_r (c_r != 0 here)
        gr /= c.coeff[r] * c.coeff[r];
        bool any_live = false;
        for (std::size_t a = 0; a < nj; ++a) {
          inert[a] = !(G[a][a] > 1e-24 * gr);
          any_live = any_live || !inert[a];
        }
        if (!any_live) return false;
        bool found = false;
        double best = 0.0;
        for (unsigned mask = 0; mask < (1u << nj); ++mask) {
          bool skip = false;
          for (std::size_t a = 0; a < nj; ++a)
            skip = skip || ((mask & (1u << a)) && inert[a]);
          if (skip) continue;  // an inert compensator is never at its bound
          std::array<double, 2> x{0.0, 0.0};
          std::vector<std::size_t> fr;
          for (std::size_t a = 0; a < nj; ++a) {
            if (mask & (1u << a)) x[a] = -c.coeff[J[a]];
            else if (!inert[a]) fr.push_back(a);
          }
          if (fr.size() == 2) {
            const double det = G[0][0] * G[1][1] - G[0][1] * G[1][0];
            if (std::fabs(det) <= 1e-12 * std::fabs(G[0][0] * G[1][1]))
              continue;  // singular face: its optimum lies on an edge
            x[0] = (h[0] * G[1][1] - h[1] * G[0][1]) / det;
            x[1] = (h[1] * G[0][0] - h[0] * G[1][0]) / det;
          } else if (fr.size() == 1) {
            const std::size_t a = fr[0], b = 1 - a;
            if (G[a][a] <= 0.0) continue;
            x[a] = (h[a] - (nj == 2 ? G[a][b] * x[b] : 0.0)) / G[a][a];
          }
          bool feasible = true;
          for (std::size_t a : fr)
            feasible = feasible && c.coeff[J[a]] + x[a] >= 0.0;
          if (!feasible) continue;
          double f = 0.0;
          for (std::size_t a = 0; a < nj; ++a) {
            f -= 2.0 * h[a] * x[a];
            for (std::size_t b = 0; b < nj; ++b) f += x[a] * G[a][b] * x[b];
          }
          if (found && !(f < best)) continue;
          found = true;
          best = f;
          dc = x;
          held.clear();
          for (std::size_t a = 0; a < nj; ++a)
            if (mask & (1u << a)) held.push_back(J[a]);
        }
        return found;
      };

      std::array<double, 2> dc{0.0, 0.0};
      std::array<bool, 2> inert{false, false};
      std::size_t nuse = nj;
      bool ok = false;
      if (rhoform_is_positive(c.form)) {
        std::vector<std::size_t> held;
        ok = nuse > 0 && solve_bounded(dc, held, inert);
        for (std::size_t j : held)
          rep.notes.push_back(
              std::string("RHOFORM ") + rhoform_name(c.form) +
              ": the compensating amplitude of the basis function at " +
              fmt(c.mu[j]) + " is held at its bound 0 (every amplitude "
              "stays >= 0).");
      } else {
        ok = nuse > 0 && solve(nuse, dc);
        if (!ok && nuse == 2) {
          nuse = 1;
          ok = solve(nuse, dc);
          if (ok)
            rep.warnings.push_back(
                "compensation system ill-conditioned with two neighbours; "
                "adjusted only the nearest one.");
        }
      }

      // ---- residuals (stacked weighted + value-only RMS) ----
      auto resid = [&](bool with_dc) {
        double stacked = 0.0, val = 0.0;
        for (int m = 0; m < 3; ++m) {
          for (std::size_t i = 0; i < N; ++i) {
            double d = -t[m][i];
            if (with_dc)
              for (std::size_t j = 0; j < nuse; ++j) d += dc[j] * A[m][i][j];
            stacked += wm[m] * wm[m] * d * d;
            if (m == 0) val += d * d;
          }
        }
        return std::make_pair(std::sqrt(stacked / (3.0 * N)),
                              std::sqrt(val / N));
      };
      std::tie(rep.resid_plain, rep.vresid_plain) = resid(false);
      if (ok) {
        std::tie(rep.resid_comp, rep.vresid_comp) = resid(true);
        for (std::size_t j = 0; j < nuse; ++j) {
          if (inert[j]) continue;  // positive forms: fixed, dc_j == 0
          c.coeff[J[j]] += dc[j];
          rep.adjusted.push_back(J[j]);
          rep.delta_c.push_back(dc[j]);
        }
        rep.compensated = true;
      } else {
        rep.resid_comp = rep.resid_plain;
        rep.vresid_comp = rep.vresid_plain;
        rep.warnings.push_back(
            "no usable compensation system — plain removal applied.");
      }
    } else {
      rep.warnings.push_back(
          "the removed basis function only acts below the sampled window "
          "(r < " + fmt(r_lo) + ") — plain removal applied.");
    }
  }

  // ---- erase index r from every parallel array ----
  c.mu.erase(c.mu.begin() + r);
  c.coeff.erase(c.coeff.begin() + r);
  if (!c.eta.empty()) c.eta.erase(c.eta.begin() + r);
  if (!c.deg.empty()) {
    c.deg.erase(c.deg.begin() + r);
    // canonical form: an all-quintic degree array collapses to "absent"
    if (std::all_of(c.deg.begin(), c.deg.end(),
                    [](int d) { return d == 5; }))
      c.deg.clear();
  }
  return rep;
}

}  // namespace potedit
}  // namespace models
}  // namespace tadah
