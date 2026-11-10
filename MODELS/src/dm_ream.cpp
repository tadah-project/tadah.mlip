#include <tadah/models/descriptors/dm/dm_ream.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/core/logger.h>
#include <tadah/core/stats/data_stats.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>
#include <tadah/models/descriptors/density_reach.h>

namespace tadah {
namespace models {

namespace {
// One notice per process when a DM_REAM model is built from a context that
// does not declare EMBF0 (the default ZERO applies). A trained potential
// (WEIGHTS stored) gets a WARNING: if it was written by tadah 1.5.0 or
// earlier its energies change.
std::atomic<bool> embf0_noticed{false};
void notice_missing_embf0(const tadah::core::Context &ctx) {
  if (embf0_noticed.exchange(true)) return;
  auto &log = tadah::core::Logger::instance();
  if (!ctx.has_stored("WEIGHTS")) {
    log.log(tadah::core::LogLevel::INFO,
            "DM_REAM: EMBF0 not set; using the default EMBF0 ZERO "
            "(zero-referenced embedding, F(0) = 0 by construction).");
    return;
  }
  log.log(tadah::core::LogLevel::WARNING,
          "DM_REAM: EMBF0 not set; " +
              embf0_absent_consequence(model_is_linear(ctx)));
}
}  // namespace

DM_REAM::DM_REAM() { init(); }
std::size_t DM_REAM::component_basis_size(const std::string &channel) const {
  if(core::to_upper(channel)=="DENSITY")return dens_.crho.size();
  if(core::to_upper(channel)=="EMBEDDING")return cemb.size();
  return 0;
}
bool DM_REAM::component_basis_curve(const std::string &channel,std::size_t i,double q,bool cutoff,double &v,double &d) const {
  const auto name=core::to_upper(channel);
  if(name=="DENSITY") {
    if(i>=dens_.crho.size())throw std::out_of_range("DM_REAM density basis index");
    double c=dens_.crho[i],eta=dens_.width(i);int degree=dens_.deg(i);
    if(dens_.form==RhoForm::Monotone) {
      v=radial_step_value_bare(basis,q,eta,c);d=radial_step_deriv_bare(basis,q,eta,c);
    } else {v=radial_value_bare(basis,q,eta,c,degree);d=radial_deriv_bare(basis,q,eta,c,degree);}
    if(cutoff) {if(q>get_rcut()) {v=d=0;return true;}
      d=d*fcut->calc(q)+v*fcut->calc_prime(q);v*=fcut->calc(q);}
  } else if(name=="EMBEDDING") {
    if(i>=cemb.size())throw std::out_of_range("DM_REAM embedding basis index");
    double eta=semb.empty()?1:semb[i];int degree=demb_deg.empty()?5:static_cast<int>(demb_deg[i]);
    v=embedding_value(basis,q,eta,cemb[i],degree);d=embedding_deriv(basis,q,eta,cemb[i],degree);
    if(embf0_==EmbF0Mode::Zero)v-=embedding_value(basis,0,eta,cemb[i],degree);
  } else return false;
  return true;
}
bool DM_REAM::component_cutoff(const std::string &channel,double q,double &fc,double &dfc) const {
  if(core::to_upper(channel)!="DENSITY")return false;
  // the taper of component_basis_curve: RCTYPEMB at RCUTMB, 0 beyond it
  if(q>get_rcut()) {fc=dfc=0;return true;}
  fc=fcut->calc(q);dfc=fcut->calc_prime(q);
  return true;
}


void DM_REAM::validate_embedding(BasisKind b, const tadah::core::v_type &ce,
                                 const tadah::core::v_type &se,
                                 const tadah::core::v_type &de) {
  if (ce.empty())
    throw std::runtime_error("DM_REAM: embedding basis (CEMBFUNC) is empty.");
  if (basis_traits(b).eta_is_gauge) {
    // eta is a pure gauge for this family (knots): the width channels must be
    // ABSENT (size-0 segments in TYPEMB) — no silent constant-1 lists. Asking
    // the trait (not comparing to a specific basis) keeps this future-proof.
    if (!se.empty())
      throw std::runtime_error(
          "DM_REAM (knot family): SGRIDMB/SEMBFUNC must be empty — eta is a "
          "pure gauge ((c,eta)->(c s^k, eta/s)); the knot position grids "
          "CGRIDMB/CEMBFUNC define the basis alone.");
  } else {
    if (ce.size() != se.size())
      throw std::runtime_error("DM_REAM: CEMBFUNC / SEMBFUNC sizes differ.");
    for (const auto &e : se)
      if (std::abs(e) < std::numeric_limits<double>::min())
        throw std::runtime_error("DM_REAM: zero entry in SEMBFUNC.");
  }
  if (!de.empty() && de.size() != ce.size())
    throw std::runtime_error("DM_REAM: KDEGEMB / CEMBFUNC sizes differ.");
  for (const auto &d : de)
    if (d != 3.0 && d != 5.0)
      throw std::runtime_error(
          "DM_REAM: KDEG* entries must be 3 or 5 (the knot-degree lists "
          "are structure data, not optimisable parameters).");
}

void DM_REAM::load(const tadah::core::Context &ctx) {
  // 1. the basis code (an unknown code is refused first), 2. the mandatory
  // density form, 3. the grids: density (existing checks, then the form
  // checks), embedding, 4. the density pair weights, 5. the embedding
  // reference. Everything goes into temporaries; the members are swapped in
  // only at the end.
  BasisKind b;
  try {
    b = ReamDensity::basis_from_context(ctx);
  } catch (const std::runtime_error &e) {
    throw std::runtime_error(std::string("DM_REAM: ") + e.what());
  }
  require_rhoform(ctx, "");
  ReamDensity d = ReamDensity::from_context(ctx);
  tadah::core::v_type ce, se, de;
  get_grid(ctx, "CEMBFUNC", ce);
  if (ctx.contains("SEMBFUNC") && ctx.size("SEMBFUNC") > 0)
    get_grid(ctx, "SEMBFUNC", se);    // absent for the knot family
  if (ctx.contains("KDEGEMB") && ctx.size("KDEGEMB") > 0)
    get_grid(ctx, "KDEGEMB", de);
  d.validate(fcut);
  validate_embedding(b, ce, se, de);
  if (rhoform_is_positive(d.form))
    require_positive_density_weights(ctx, rhoform_subject(d.form));

  const bool declared = ctx.has_stored("EMBF0");
  // absent -> default ZERO, materialised; throws on an invalid value
  const EmbF0Mode mode = embf0_from_context(ctx);
  if (!declared) notice_missing_embf0(ctx);
  tadah::core::v_type g0a, g0e;
  if (mode == EmbF0Mode::Zero)   // FREE: raw basis, no reference, no FP work
    embedding_reference(b, ce, se, de, g0a, g0e);

  basis = b;
  dens_ = std::move(d);
  cemb.swap(ce);
  semb.swap(se);
  demb_deg.swap(de);
  embf0_ = mode;
  g0_aed_.swap(g0a);
  g0_emb_.swap(g0e);
}

DM_REAM::DM_REAM(const tadah::core::Context& ctx) : DM_Base(ctx) {
  init();
  if (!ctx.get<bool>("INITMB")) return;

  load(ctx);
  D_Base::printGrid(dens_.crho, "CGRIDMB");
  D_Base::printGrid(cemb, "CEMBFUNC");

  s         = cemb.size();   // M embedding outputs (linear weights live in M_BLR)
  rhoisize  = 1;             // scalar density rho_i
  rhoipsize = s;             // one stored g'_m(rho_i) per output (for forces)
  set_rfidx(0);
  set_drfidx(rhoisize);

  init_for_atoms(get_init_atoms(ctx));
  warn_if_knots_beyond_rcut();
  warn_if_bump_support_beyond_rcut("DM_REAM", "CGRIDMB", "RCUTMB", dens_.crho,
                                   dens_.srho, basis);
}

void DM_REAM::warn_if_knots_beyond_rcut() {
  if (basis != BasisKind::Knot5 || dens_.crho.empty() || !fcut)
    return;
  const double mx = *std::max_element(dens_.crho.begin(), dens_.crho.end());
  const double rc = get_rcut();
  if (mx > rc + 1e-12) {
    log_warning_once(
        "DM_REAM: density knot past RCUTMB",
        "DM_REAM: the largest CGRIDMB density knot (" + std::to_string(mx) +
        ") lies beyond RCUTMB (" + std::to_string(rc) +
        ") — its contribution is silently hard-truncated at the cutoff. "
        "The effective cutoff of a knot channel is its largest knot: set "
        "RCUTMB to the largest knot (refit/edit/hpo tooling derives it "
        "automatically).");
  }
}

// ---------------------------- basis helpers ----------------------------

double DM_REAM::rho_raw(double r, double fc) const {
  return dens_.value(r, fc);
}

double DM_REAM::rho_raw_deriv(double r, double fc, double fcp) const {
  return dens_.deriv(r, fc, fcp);
}

double DM_REAM::gemb(std::size_t m, double rho) const {
  // Embedding: RIGHT-active knots, no cutoff. SEMBFUNC absent for the knot
  // family (eta gauge) -> s = 1 (ignored by the knot arms).
  const double s = (m < semb.size()) ? semb[m] : 1.0;
  return embedding_value(basis, rho, s, cemb[m], emb_deg(m));
}

double DM_REAM::dgemb(std::size_t m, double rho) const {
  const double s = (m < semb.size()) ? semb[m] : 1.0;
  return embedding_deriv(basis, rho, s, cemb[m], emb_deg(m));
}

void DM_REAM::embedding_reference(BasisKind b, const tadah::core::v_type &ce,
                                  const tadah::core::v_type &se,
                                  const tadah::core::v_type &de,
                                  tadah::core::v_type &g0a,
                                  tadah::core::v_type &g0e) {
  // The same expressions as gemb() (calc_aed's routine) and eam_embed's
  // embedding_value_deriv, on the arrays load() is about to commit.
  const std::size_t M = ce.size();
  g0a.resize(M);
  g0e.resize(M);
  for (std::size_t m = 0; m < M; ++m) {
    const double s = (m < se.size()) ? se[m] : 1.0;
    const int deg = m < de.size() ? static_cast<int>(de[m]) : 5;
    g0a[m] = embedding_value(b, 0.0, s, ce[m], deg);  // calc_aed's routine
    double v, d;
    embedding_value_deriv(b, 0.0, s, ce[m], v, d, deg);
    g0e[m] = v;                                      // eam_embed's routine
  }
}

// ------------------------------ density --------------------------------

void DM_REAM::calc_rho(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &,
  tadah::core::rho_type& rho,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const double fc = fcut->calc(rij);
  rho(rfidx) += scale * weights[Zi][Zj] * rho_raw(rij, fc);
}

// --------------------------- embedding energy --------------------------

void DM_REAM::calc_aed(
  tadah::core::rho_type& rho,
  tadah::core::aed_type &aed) const
{
  const double rho_i = rho(rfidx);
  const std::size_t M = cemb.size();
  if (embf0_ == EmbF0Mode::Free) {
    for (std::size_t m = 0; m < M; ++m) {
      aed(fidx + m) += gemb(m, rho_i);   // F(rho) = sum_m c_m g_m(rho)  (c_m in M_BLR)
      rho(drfidx + m) = dgemb(m, rho_i); // store g'_m(rho_i) for the force descriptor
    }
    return;
  }
  // EMBF0 ZERO: F(rho) = sum_m c_m (g_m(rho) - g_m(0)). At rho == 0 every
  // output is exactly zero, so nothing is added (exact whatever the FP
  // contraction); g'_m(0) is still stored for the force descriptor.
  if (rho_i == 0.0) {
    for (std::size_t m = 0; m < M; ++m) rho(drfidx + m) = dgemb(m, rho_i);
    return;
  }
  for (std::size_t m = 0; m < M; ++m) {
    aed(fidx + m) += gemb(m, rho_i) - g0_aed_[m];
    rho(drfidx + m) = dgemb(m, rho_i);
  }
}

// ------------------------------- forces --------------------------------

void DM_REAM::calc_dXijdri_dXjidri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &vec_ij,
  tadah::core::rho_type& rhoi,
  tadah::core::rho_type& rhoj,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const double fc  = fcut->calc(rij);
  const double fcp = fcut->calc_prime(rij);
  const double rpr = rho_raw_deriv(rij, fc, fcp);      // unweighted drho/dr
  const double rinv = 1.0 / rij;
  const double pref = scale * weights[Zi][Zj] * rpr * rinv;
  const std::size_t M = cemb.size();
  for (std::size_t m = 0; m < M; ++m) {
    const double coef = pref * (rhoi(drfidx + m) + rhoj(drfidx + m));
    fd_ij(fidx + m, 0) = coef * vec_ij[0];
    fd_ij(fidx + m, 1) = coef * vec_ij[1];
    fd_ij(fidx + m, 2) = coef * vec_ij[2];
  }
}

void DM_REAM::calc_dXijdri(
  const int Zi,
  const int Zj,
  const double rij,
  const double,
  const tadah::core::Vec3d &vec_ij,
  tadah::core::rho_type& rhoi,
  tadah::core::fd_type &fd_ij,
  const double scale) const
{
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const double fc  = fcut->calc(rij);
  const double fcp = fcut->calc_prime(rij);
  const double rpr = rho_raw_deriv(rij, fc, fcp);      // unweighted drho/dr
  const double rinv = 1.0 / rij;
  const double pref = scale * weights[Zi][Zj] * rpr * rinv;
  const std::size_t M = cemb.size();
  for (std::size_t m = 0; m < M; ++m) {
    const double coef = pref * rhoi(drfidx + m);       // g'_m(rho_i) from calc_aed
    fd_ij(fidx + m, 0) = coef * vec_ij[0];
    fd_ij(fidx + m, 1) = coef * vec_ij[1];
    fd_ij(fidx + m, 2) = coef * vec_ij[2];
  }
}

// --------------------------- EAM collapse ------------------------------

void DM_REAM::eam_density(const int Zi, const int Zj, const double rij,
                          double &rho, double &rhop) const {
  rho = 0.0; rhop = 0.0;
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return;
  const double fc  = fcut->calc(rij);
  const double fcp = fcut->calc_prime(rij);
  double rho_raw_both, rhop_raw;
  dens_.value_deriv(rij, fc, fcp, rho_raw_both, rhop_raw);
  const double w   = weights[Zi][Zj];
  rho  = w * rho_raw_both;
  rhop = w * rhop_raw;
}

double DM_REAM::pair_density(const int Zi, const int Zj,
                             const double rij) const {
  // calc_rho's increment at scale 1, the same expression
  if (!is_init_for_atoms(Zi,Zj) || rij > get_rcut()) return 0.0;
  const double fc = fcut->calc(rij);
  const double scale = 1.0;
  return scale * weights[Zi][Zj] * rho_raw(rij, fc);
}

void DM_REAM::eam_embed(const tadah::core::t_type &w_full, const double rho_bar,
                        double &F, double &Fp) const {
  F = 0.0; Fp = 0.0;
  const std::size_t M = cemb.size();
  if (embf0_ == EmbF0Mode::Free) {
    for (std::size_t m = 0; m < M; ++m) {
      const double w = w_full(fidx + m);
      const double s = (m < semb.size()) ? semb[m] : 1.0;
      double emb_value, emb_deriv;
      embedding_value_deriv(basis, rho_bar, s, cemb[m],
                            emb_value, emb_deriv, emb_deg(m));
      F  += w * emb_value;
      Fp += w * emb_deriv;
    }
    return;
  }
  // EMBF0 ZERO: the same zero-referenced basis as calc_aed, subtracted per
  // term so this kernel mirrors w.aed; F' is untouched.
  for (std::size_t m = 0; m < M; ++m) {
    const double w = w_full(fidx + m);
    const double s = (m < semb.size()) ? semb[m] : 1.0;
    double emb_value, emb_deriv;
    embedding_value_deriv(basis, rho_bar, s, cemb[m],
                          emb_value, emb_deriv, emb_deg(m));
    F  += w * (emb_value - g0_emb_[m]);
    Fp += w * emb_deriv;
  }
  if (rho_bar == 0.0) F = 0.0;
}

std::optional<double>
DM_REAM::raw_embedding_f0(const tadah::core::t_type &w_full) const {
  const std::size_t M = cemb.size();
  if (w_full.size() < fidx + M) return std::nullopt;
  double acc = 0.0;
  for (std::size_t m = 0; m < M; ++m) acc += w_full(fidx + m) * gemb(m, 0.0);
  return acc;
}

// ------------------------------- misc ----------------------------------

std::string DM_REAM::label() const { return lab; }

void DM_REAM::init_rhoi(tadah::core::rho_type& rhoi) {
  rhoi.resize(rhoisize + rhoipsize);
  rhoi.set_zero();
}

void DM_REAM::init() {
  // contract (see d_base.h): TYPEMB carries 1 family param (the basis code)
  // followed by one SIZE token per keys entry, then per data_keys entry:
  //   DM_REAM <bcode> <nC> <nS> <nAmp> <nEc> <nEs> <nKd> <nKe> El El
  // Knot pots carry 0 for the width channels (eta is a pure gauge) and bump
  // pots carry 0 for the KDEG channels.
  keys = {"CGRIDMB", "SGRIDMB", "AMPGRIDMB", "CEMBFUNC", "SEMBFUNC"};
  data_keys = {"KDEGMB", "KDEGEMB"};
  nparams = 1 + 5 + 2;
}

std::optional<DescriptorMeta> DM_REAM::meta() const {
  using DataStats = tadah::core::stats::DataStats;

  DescriptorMeta dm;
  dm.label = "DM_REAM";

  // RCUTMB — real-space cutoff for the density neighbour sum. On a line of
  // a knot DM_REAM alone, tadah hpo derives it from the largest knot, and
  // tadah analysis suggest_bounds gives that row no OPTIM line (Tadah.MLIP).
  ParamMeta p_rcut;
  p_rcut.key     = "RCUTMB";
  p_rcut.arg_pos = -1;
  p_rcut.scale   = Scale::Distance;
  p_rcut.units   = "Å";
  p_rcut.role    = "Real-space cutoff for the EAM density neighbour sum.";
  p_rcut.hard.lo = 0.0;
  p_rcut.crit    = Criticality::Critical;
  p_rcut.scope   = PairScope::Global;
  p_rcut.suggest = cutoff_suggestion;
  p_rcut.reach_each_pair = true;
  p_rcut.check    = [](const DataStats& d, const SeedView& s) {
    return cutoff_reach_check(d, s, "RCUTMB");
  };
  dm.params.push_back(std::move(p_rcut));

  // Density-expansion grids (r-space). Tunable, but the neighbour
  // distances give no range for them, and the knot degrees are structure
  // data that tadah hpo does not search. Each gets a row without an OPTIM
  // line, saying so.
  auto no_ream_range = [](const std::string& key) {
    if (key.rfind("KDEG", 0) == 0)
      return BoundSuggestion::no_row(
          "a knot degree (3 or 5), structure data that tadah hpo does not "
          "search (it refuses OPTIM " + key + ")");
    return BoundSuggestion::no_row(
        "the neighbour distances give no range for a value of a DM_REAM "
        "grid");
  };
  auto add_vec = [&dm, no_ream_range](const std::string& key, Scale sc, const std::string& units,
                       const std::string& role, std::optional<double> lo,
                       Criticality crit) {
    ParamMeta p;
    p.key     = key;
    p.arg_pos = 0;
    p.scale   = sc;
    p.units   = units;
    p.role    = role;
    p.hard.lo = lo;
    p.crit    = crit;
    p.scope   = PairScope::Global;
    p.suggest = [key, no_ream_range](const DataStats&, const SeedView&,
                                     int) {
      return no_ream_range(key);
    };
    dm.params.push_back(std::move(p));
  };
  add_vec("CGRIDMB", Scale::Distance, "Å",
          "Centre (knot families: knot position) of the i-th density radial "
          "basis function. Optimisable (OPTIM CGRIDMB).", 0.0,
          Criticality::Recommended);
  add_vec("SGRIDMB", basis==BasisKind::Gaussian?Scale::InverseDistanceSq:Scale::InverseDistance, basis==BasisKind::Gaussian?"Å^-2":"Å^-1",
          "Inverse width of the i-th density radial basis function — bump "
          "families only; ABSENT for knot families (eta is a pure gauge). "
          "Optimisable where present.", kHardLoPositive, Criticality::Recommended);
  add_vec("AMPGRIDMB", Scale::Dimensionless, "",
          "Density-expansion amplitude a_n: rho(r) = W sum_n a_n T_n(r) "
          "f_c(r), T_n = psi_n (RHOFORM SIGNED/NONNEG) or the integrated "
          "bump S_n (MONOTONE); >= 0 under NONNEG and MONOTONE. Excluded "
          "from the LINEAR solve by bilinearity, but a NONLINEAR "
          "search parameter of tadah hpo (OPTIM AMPGRIDMB).", std::nullopt,
          Criticality::Recommended);
  add_vec("CEMBFUNC", Scale::Density, "rho",
          "Centre (in rho-space) of the i-th embedding basis function. "
          "Optimisable (OPTIM CEMBFUNC).", std::nullopt,
          Criticality::Recommended);
  add_vec("SEMBFUNC", basis==BasisKind::Gaussian?Scale::InverseDensitySq:Scale::InverseDensity, basis==BasisKind::Gaussian?"rho^-2":"rho^-1",
          "Inverse width (in rho-space) of the i-th embedding basis "
          "function — bump families only; ABSENT for knot families. "
          "Optimisable where present.", kHardLoPositive, Criticality::Recommended);
  add_vec("KDEGMB", Scale::Dimensionless, "",
          "Knot-degree data (3 or 5) per density atom: a discrete structure "
          "choice that tadah hpo does not search; values other than 3 and "
          "5 are refused.", 3.0,
          Criticality::Optional);
  add_vec("KDEGEMB", Scale::Dimensionless, "",
          "Knot-degree data (3 or 5) per embedding atom: a discrete structure "
          "choice that tadah hpo does not search.", 3.0, Criticality::Optional);

  dm.expected_columns = [](const SeedView& s) -> std::size_t {
    if (!s.contains("CEMBFUNC")) return 0;
    return s.size("CEMBFUNC");
  };

  return dm;
}

} // namespace models
} // namespace tadah
