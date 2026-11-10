#include <tadah/models/descriptors/dm/dm_embed.h>
#include <tadah/models/descriptors/density_reach.h>

namespace tadah {
namespace models {
namespace {
EmbedSpec select_spec(const core::Context &c, const std::string &id = "") {
  auto specs = embed_specs(c);
  if (id.empty() && specs.size() == 1)
    return specs.front();
  for (const auto &s : specs)
    if (core::to_upper(s.id) == core::to_upper(id))
      return s;
  throw std::runtime_error(
      "DM_Embed: exactly one owned descriptor block is required");
}
} // namespace
DM_Embed::DM_Embed() { init(); }
void DM_Embed::init() {
  nparams = 1;
  keys.clear();
  data_keys.clear();
}
DM_Embed::DM_Embed(const core::Context &c) : DM_Base(c) {
  init();
  auto spec = select_spec(c);
  evaluator_ = std::make_unique<EmbedEvaluator>(spec);
  s = spec.embedding.centres.size();
  rhoisize = 1;
  rhoipsize = s;
  set_rfidx(0);
  set_drfidx(1);
  double radius = spec.radius;
  set_fcut(core::factory<Cut_Base, double &>(
               spec.cutoff == "NONE" ? "Cut_Dummy" : spec.cutoff, radius),
           true);
  init_for_atoms(spec.pairs);
}
std::size_t DM_Embed::component_basis_size(const std::string &channel) const {
  if (core::to_upper(channel) == "DENSITY")
    return evaluator_->spec().density.centres.size();
  if (core::to_upper(channel) == "EMBEDDING")
    return s;
  return 0;
}
bool DM_Embed::component_basis_curve(const std::string &channel, std::size_t i,
                                     double q, bool cutoff, double &value,
                                     double &deriv) const {
  EmbedJet j;
  if (core::to_upper(channel) == "DENSITY")
    j = cutoff ? evaluator_->density(q, 'a', i)
               : evaluator_->density_atom(i, q);
  else if (core::to_upper(channel) == "EMBEDDING")
    j = evaluator_->embedding(i, q);
  else
    return false;
  value = j.value;
  deriv = j.deriv;
  return true;
}
bool DM_Embed::component_cutoff(const std::string &channel, double q,
                                double &fc, double &dfc) const {
  if (core::to_upper(channel) != "DENSITY")
    return false;
  const auto j = evaluator_->spatial_cutoff(q);
  fc = j.value;
  dfc = j.deriv;
  return true;
}
void DM_Embed::init_rhoi(core::rho_type &rho) {
  rho.resize(rhoisize + rhoipsize);
  rho.set_zero();
}
void DM_Embed::eam_density(int zi, int zj, double r, double &v,
                           double &d) const {
  v = d = 0;
  if (!is_init_for_atoms(zi, zj) || r >= get_rcut())
    return;
  auto j = evaluator_->density(r);
  v = weights[zi][zj] * j.value;
  d = weights[zi][zj] * j.deriv;
}
void DM_Embed::calc_rho(int zi, int zj, double r, double, const core::Vec3d &,
                        core::rho_type &rho, double scale) const {
  double v, d;
  eam_density(zi, zj, r, v, d);
  rho(rfidx) += scale * v;
}
void DM_Embed::calc_aed(core::rho_type &rho, core::aed_type &aed) const {
  for (std::size_t i = 0; i < s; ++i) {
    const auto j = evaluator_->embedding(i, rho(rfidx));
    aed(fidx + i) += j.value;
    rho(drfidx + i) = j.deriv;
  }
}
void DM_Embed::calc_dXijdri(int zi, int zj, double r, double,
                            const core::Vec3d &v, core::rho_type &rho,
                            core::fd_type &fd, double scale) const {
  if (!is_init_for_atoms(zi, zj) || r >= get_rcut())
    return;
  if (r == 0)
    throw std::domain_error(
        "DM_Embed: radial force is undefined at coincident atoms");
  double a, d;
  eam_density(zi, zj, r, a, d);
  const double pref = scale * d / r;
  for (std::size_t i = 0; i < s; ++i)
    for (std::size_t k = 0; k < 3; ++k)
      fd(fidx + i, k) += pref * rho(drfidx + i) * v[k];
}
void DM_Embed::calc_dXijdri_dXjidri(int zi, int zj, double r, double rsq,
                                    const core::Vec3d &v, core::rho_type &a,
                                    core::rho_type &b, core::fd_type &fd,
                                    double scale) const {
  calc_dXijdri(zi, zj, r, rsq, v, a, fd, scale);
  calc_dXijdri(zi, zj, r, rsq, v, b, fd, scale);
}
void DM_Embed::eam_embed(const core::t_type &w, double rho, double &v,
                         double &d) const {
  evaluator_->embedding_sum(w, fidx, rho, v, d);
}
std::optional<EmbF0Mode> DM_Embed::embf0_mode() const {
  return evaluator_ ? std::optional<EmbF0Mode>(evaluator_->spec().reference)
                    : std::nullopt;
}
std::optional<double> DM_Embed::raw_embedding_f0(const core::t_type &w) const {
  if (!evaluator_ || w.size() < fidx + s)
    return std::nullopt;
  double out = 0;
  for (std::size_t i = 0; i < s; ++i)
    out += w(fidx + i) * evaluator_->raw_zero(i);
  return out;
}
std::optional<DescriptorMeta> DM_Embed::meta() const {
  DescriptorMeta m;
  m.label = label();
  m.expected_columns = [owner = evaluator_
                                    ? core::to_upper(evaluator_->spec().id)
                                    : std::string{}](const SeedView &seed) {
    std::size_t count = 0;
    for (const auto &spec : embed_specs(seed.context()))
      if (owner.empty() || core::to_upper(spec.id) == owner)
        count += spec.embedding.centres.size();
    return count;
  };
  // Named coordinates exist only after a component is configured. The
  // registry still exposes the family and its column-count capability.
  if (!evaluator_)
    return m;
  core::Context c;
  c.append_block(embed_block(evaluator_->spec()));
  for (const auto &b : embed_bindings(c)) {
    ParamMeta p;
    p.key = b.path;
    p.arg_pos = 0;
    p.units = b.units;
    p.scale = Scale::Dimensionless;
    p.scope = PairScope::Global;
    if (b.units == "A")
      p.scale = Scale::Distance;
    else if (b.units == "A^-1")
      p.scale = Scale::InverseDistance;
    else if (b.units == "A^-2")
      p.scale = Scale::InverseDistanceSq;
    else if (b.units == "rho")
      p.scale = Scale::Density;
    else if (b.units == "rho^-1")
      p.scale = Scale::InverseDensity;
    else if (b.units == "rho^-2")
      p.scale = Scale::InverseDensitySq;
    p.role = "Component parameter in its declared coordinates";
    p.suggest = [key = b.path,
                 positive = b.positive](const core::stats::DataStats &,
                                        const SeedView &seed, int i) {
      double x =
          seed.get<double>(key, static_cast<std::size_t>(std::max(i, 0)));
      double d = std::max(0.1 * std::abs(x), 0.02);
      return std::make_pair(positive ? std::max(0.5 * x, x - d) : x - d, x + d);
    };
    if (b.positive)
      p.hard.lo = 0;
    // The density cutoff (DENSITY CUTOFF RADIUS) has the check of every
    // cutoff: the density sums over the neighbours within it, and is 0 for
    // every atom when it holds none.
    const std::string radius = ".DENSITY.CUTOFF.RADIUS";
    if (b.path.size() > radius.size() &&
        b.path.compare(b.path.size() - radius.size(), radius.size(),
                       radius) == 0) {
      p.role = "Cutoff radius of the density (DENSITY CUTOFF RADIUS)";
      p.check = [key = b.path](const core::stats::DataStats &d,
                               const SeedView &seed) {
        return cutoff_reach_check(d, seed, key);
      };
    }
    m.params.push_back(std::move(p));
  }
  return m;
}
DM_Embed::Parameter DM_Embed::parameter(const std::string &name,
                                        int pos) const {
  Parameter p;
  const std::string prefix = core::to_upper(evaluator_->spec().id) + ".";
  auto upper = core::to_upper(name);
  if (upper.rfind(prefix, 0) != 0)
    return p;
  auto field = upper.substr(prefix.size());
  if (field.rfind("DENSITY.", 0) != 0 && field.rfind("EMBEDDING.", 0) != 0)
    return p;
  p.density = field.rfind("DENSITY.", 0) == 0;
  const auto &c =
      p.density ? evaluator_->spec().density : evaluator_->spec().embedding;
  if (field == "DENSITY.CUTOFF.RADIUS" && evaluator_->spec().cutoff != "NONE")
    p.field = 'r';
  else if (field == "EMBEDDING.ENVELOPE.CUTOFF" &&
           evaluator_->spec().envelope != EmbedEnvelope::Off)
    p.field = 'e';
  else {
    auto dot = field.find('.');
    if (dot == std::string::npos)
      return p;
    auto leaf = field.substr(dot + 1);
    if (leaf == "CENTRES")
      p.field = 'c';
    else if (leaf == "AMPLITUDES" && p.density)
      p.field = 'a';
    else if (c.basis != EmbedBasis::Knot5 &&
             leaf ==
                 (c.coordinates == EmbedCoordinates::Widths ? "WIDTHS" : "ETA"))
      p.field = 's';
    else
      return p;
  }
  if (p.field != 'r' && p.field != 'e' && pos <= 0)
    return p;
  p.index = pos > 0 ? static_cast<std::size_t>(pos - 1) : 0;
  p.owned = (p.field == 'r' || p.field == 'e') ? p.index == 0
                                               : p.index < c.centres.size();
  return p;
}
bool DM_Embed::depends_on(const std::string &name, int pos) const {
  if (name == "WATOM")
    return D_Base::depends_on(name, pos);
  return parameter(name, pos).field != ' ';
}
bool DM_Embed::calc_drhoi_dtheta(const std::string &name, int pos, int zi,
                                 int zj, double r, double, const core::Vec3d &,
                                 core::rho_type &out, double scale) const {
  auto p = parameter(name, pos);
  if (!p.owned)
    return false;
  if (p.density && is_init_for_atoms(zi, zj))
    out(rfidx) += scale * weights[zi][zj] *
                  evaluator_->density(r, p.field, p.index).value;
  return true;
}
bool DM_Embed::calc_daed_dtheta(const std::string &name, int pos,
                                const core::rho_type &rho,
                                const core::rho_type &drho, core::aed_type &out,
                                double scale) const {
  auto p = parameter(name, pos);
  if (!p.owned)
    return false;
  for (std::size_t i = 0; i < s; ++i) {
    auto j = evaluator_->embedding(i, rho(rfidx), p.density ? ' ' : p.field,
                                   p.index);
    out(fidx + i) += scale * (p.density ? j.deriv * drho(rfidx) : j.value);
  }
  return true;
}
bool DM_Embed::calc_dXijdri_dtheta(const std::string &name, int pos, int zi,
                                   int zj, double r, double,
                                   const core::Vec3d &v,
                                   const core::rho_type &rho,
                                   const core::rho_type &drho,
                                   core::fd_type &out, double scale) const {
  auto p = parameter(name, pos);
  if (!p.owned)
    return false;
  if (!is_init_for_atoms(zi, zj) || r >= get_rcut())
    return true;
  if (r == 0)
    throw std::domain_error(
        "DM_Embed: radial force derivative is undefined at coincident atoms");
  auto den = evaluator_->density(r);
  auto dden = p.density ? evaluator_->density(r, p.field, p.index) : EmbedJet{};
  const double pref = scale * weights[zi][zj] / r;
  for (std::size_t i = 0; i < s; ++i) {
    auto j = evaluator_->embedding(i, rho(rfidx), p.density ? ' ' : p.field,
                                   p.index);
    double term =
        p.density ? dden.deriv * j.deriv + den.deriv * j.second * drho(rfidx)
                  : den.deriv * j.deriv;
    for (std::size_t k = 0; k < 3; ++k)
      out(fidx + i, k) += pref * term * v[k];
  }
  return true;
}
} // namespace models
} // namespace tadah
