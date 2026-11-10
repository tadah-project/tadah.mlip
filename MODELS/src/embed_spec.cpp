#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tadah/core/number_format.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/d_mjoin.h>
#include <tadah/models/descriptors/dm/embed_spec.h>

namespace tadah {
namespace models {
namespace {
using Fields = std::map<std::string, std::vector<std::string>>;
[[noreturn]] void bad(const std::string &what) {
  throw std::runtime_error("DM_Embed: " + what);
}
std::vector<std::string> words(const std::string &line) {
  std::istringstream in(line.substr(0, line.find('#')));
  std::vector<std::string> out;
  std::string s;
  while (in >> s)
    out.push_back(s);
  return out;
}
std::string scalar(Fields &f, const std::string &key,
                   const std::string &fallback = "") {
  auto it = f.find(key);
  if (it == f.end()) {
    if (fallback.empty())
      bad("missing " + key);
    return fallback;
  }
  if (it->second.size() != 1)
    bad(key + " requires exactly one value");
  auto out = it->second[0];
  f.erase(it);
  return out;
}
std::vector<double> grid(Fields &f, const std::string &key,
                         bool required = true) {
  auto it = f.find(key);
  if (it == f.end()) {
    if (required)
      bad("missing " + key);
    return {};
  }
  // Validate generator arity before using the established grid evaluator.
  const auto tokens = it->second;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    auto t = core::to_upper(tokens[i]);
    if (t == "LIN" || t == "GEOM") {
      if (i + 3 >= tokens.size())
        bad(key + ": incomplete " + t + " grid");
      double n = 0;
      if (!core::parse_decimal_finite(tokens[i + 1], n) || n < 1 ||
          n > 1000000 || std::floor(n) != n)
        bad(key + ": grid count must be in [1,1000000]");
      for (int k = 2; k <= 3; ++k) {
        double v;
        if (!core::parse_decimal_finite(tokens[i + k], v))
          bad(key + ": invalid grid endpoint");
      }
      i += 3;
    } else {
      double v;
      if (!core::parse_decimal_finite(tokens[i], v))
        bad(key + ": nonfinite or invalid number " + tokens[i]);
    }
  }
  auto out = D_Base::get_grid(tokens);
  f.erase(it);
  if (out.empty())
    bad(key + " must not be empty");
  for (double x : out)
    if (!std::isfinite(x))
      bad(key + ": nonfinite grid value");
  return out;
}
EmbedComponent component(Fields &f, const std::string &prefix, bool density) {
  EmbedComponent out;
  out.basis = embed_basis_from_name(scalar(f, prefix + "BASIS"));
  out.centres = grid(f, prefix + "CENTRES");
  const auto n = out.centres.size();
  if (out.basis == EmbedBasis::Knot5) {
    if (f.count(prefix + "ETA") || f.count(prefix + "WIDTHS"))
      bad(prefix + "knots accept neither ETA nor WIDTHS");
    auto deg = grid(f, prefix + "DEGREES", false);
    if (!deg.empty() && deg.size() != n)
      bad(prefix + "DEGREES size differs from CENTRES");
    out.degrees.assign(n, 5);
    for (std::size_t i = 0; i < deg.size(); ++i) {
      if (deg[i] != 3 && deg[i] != 5)
        bad(prefix + "DEGREES entries must be 3 or 5");
      out.degrees[i] = static_cast<int>(deg[i]);
    }
  } else {
    bool width = f.count(prefix + "WIDTHS");
    if (width && f.count(prefix + "ETA"))
      bad(prefix + "ETA and WIDTHS are mutually exclusive");
    out.coordinates = width ? EmbedCoordinates::Widths : EmbedCoordinates::Eta;
    out.scales = grid(f, prefix + (width ? "WIDTHS" : "ETA"));
    if (out.scales.size() != n)
      bad(prefix + "scales size differs from CENTRES");
    for (std::size_t i = 0; i < n; ++i) {
      if (!(out.scales[i] > 0))
        bad(prefix + "scales must be finite and positive");
      double eta = embed_eta(out, i);
      if (!(eta > 0) || !std::isfinite(eta) || !std::isfinite(eta * eta))
        bad(prefix + "scale is outside the evaluable range");
    }
  }
  if (density) {
    out.amplitudes = grid(f, prefix + "AMPLITUDES");
    if (out.amplitudes.size() != n)
      bad(prefix + "AMPLITUDES size differs from CENTRES");
  }
  return out;
}
std::string values(const std::vector<double> &v) {
  std::string out;
  for (double x : v)
    out += " " + core::format_roundtrip(x);
  return out;
}
} // namespace

const char *embed_basis_name(EmbedBasis k) {
  switch (k) {
  case EmbedBasis::Blip:
    return "Blip";
  case EmbedBasis::Blip5:
    return "Blip5";
  case EmbedBasis::Gaussian:
    return "Gaussian";
  case EmbedBasis::Knot5:
    return "Knot5";
  case EmbedBasis::WendlandC2:
    return "WendlandC2";
  case EmbedBasis::WendlandC4:
    return "WendlandC4";
  }
  bad("unknown basis code");
}
std::vector<double> expand_embed_grid(const std::vector<std::string> &tokens,
                                      const std::string &label) {
  Fields fields{{label, tokens}};
  return grid(fields, label);
}
EmbedBasis embed_basis_from_name(const std::string &s) {
  for (auto k :
       {EmbedBasis::Blip, EmbedBasis::Blip5, EmbedBasis::Gaussian,
        EmbedBasis::Knot5, EmbedBasis::WendlandC2, EmbedBasis::WendlandC4})
    if (core::to_upper(s) == core::to_upper(embed_basis_name(k)))
      return k;
  bad("unknown BASIS '" + s + "'");
}
double embed_support(EmbedBasis k) {
  switch (k) {
  case EmbedBasis::Blip:
    return 2;
  case EmbedBasis::Blip5:
    return 3;
  case EmbedBasis::Gaussian:
    return std::numeric_limits<double>::infinity();
  case EmbedBasis::Knot5:
    return 0;
  case EmbedBasis::WendlandC2:
  case EmbedBasis::WendlandC4:
    return 1;
  }
  bad("unknown support");
}
double embed_eta(const EmbedComponent &c, std::size_t i) {
  if (c.basis == EmbedBasis::Knot5)
    return 1;
  double x = c.scales.at(i);
  if (c.coordinates == EmbedCoordinates::Eta)
    return x;
  return c.basis == EmbedBasis::Gaussian ? (1 / x) / x : 1 / x;
}

std::vector<EmbedSpec> embed_specs(const core::Context &ctx) {
  std::vector<EmbedSpec> out;
  std::set<std::string> ids;
  for (const auto &block : ctx.blocks("DESCRIPTOR")) {
    try {
      auto header = words(block.lines.at(0));
      if (header.size() != 3 || header[2] != "DM_Embed")
        bad("header must be DESCRIPTOR <id> DM_Embed");
      EmbedSpec s;
      s.id = header[1];
      if (s.id.empty() ||
          s.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRS"
                                 "TUVWXYZ0123456789_") != std::string::npos)
        bad("invalid descriptor ID '" + s.id + "'");
      if (!ids.insert(core::to_upper(s.id)).second)
        bad("duplicate descriptor ID '" + s.id + "'");
      Fields f;
      std::vector<std::string> path;
      std::set<std::string> sections;
      for (std::size_t i = 1; i + 1 < block.lines.size(); ++i) {
        auto w = words(block.lines[i]);
        if (w.empty())
          continue;
        std::string k = core::to_upper(w[0]);
        if (k.rfind("END", 0) == 0) {
          if (path.empty() || "END" + path.back() != k)
            bad("mismatched boundary at line " +
                std::to_string(block.first_line + i));
          path.pop_back();
          continue;
        }
        std::string name;
        for (const auto &p : path)
          name += p + ".";
        name += k;
        if (w.size() == 1) {
          if (name != "DENSITY" && name != "EMBEDDING" &&
              name != "DENSITY.CUTOFF" && name != "EMBEDDING.ENVELOPE")
            bad("unknown section " + name);
          if (!sections.insert(name).second)
            bad("duplicate section " + name);
          path.push_back(k);
          continue;
        }
        w.erase(w.begin());
        if (!f.emplace(name, w).second)
          bad("duplicate field " + name + " at line " +
              std::to_string(block.first_line + i));
      }
      if (!path.empty())
        bad("unclosed component section");
      auto p = f.find("PAIRS");
      if (p == f.end() || p->second.empty() || p->second.size() % 2)
        bad("PAIRS requires element pairs");
      s.pairs = p->second;
      f.erase(p);
      s.density = component(f, "DENSITY.", true);
      s.embedding = component(f, "EMBEDDING.", false);
      auto form = core::to_upper(scalar(f, "DENSITY.FORM"));
      if (form == "SIGNED")
        s.form = RhoForm::Signed;
      else if (form == "NONNEG")
        s.form = RhoForm::NonNeg;
      else if (form == "MONOTONE")
        s.form = RhoForm::Monotone;
      else
        bad("FORM must be SIGNED, NONNEG or MONOTONE");
      if (s.form != RhoForm::Signed) {
        for (double a : s.density.amplitudes)
          if (a < 0)
            bad("positive density forms require nonnegative AMPLITUDES");
        D_Base::require_positive_density_weights(ctx, "DM_Embed");
      }
      if (s.form == RhoForm::Monotone && s.density.basis == EmbedBasis::Knot5)
        bad("MONOTONE requires a bump family (knots already decrease under "
            "NONNEG)");
      auto ref = core::to_upper(scalar(f, "EMBEDDING.REFERENCE", "ZERO"));
      if (ref == "ZERO")
        s.reference = EmbF0Mode::Zero;
      else if (ref == "FREE")
        s.reference = EmbF0Mode::Free;
      else
        bad("REFERENCE must be ZERO or FREE");
      s.cutoff = scalar(f, "DENSITY.CUTOFF.TYPE");
      if (core::to_upper(s.cutoff) == "NONE")
        s.cutoff = "NONE";
      const std::set<std::string> cuts{"NONE",     "Cut_Cos",   "Cut_CosS",
                                       "Cut_Poly", "Cut_PolyS", "Cut_Tanh",
                                       "Cut_PT"};
      if (!cuts.count(s.cutoff))
        bad("unsuitable spatial cutoff '" + s.cutoff + "'");
      auto radius = grid(f, "DENSITY.CUTOFF.RADIUS");
      if (radius.size() != 1 || !(radius[0] > 0))
        bad("RADIUS requires one finite positive value");
      s.radius = radius[0];
      if (s.cutoff == "NONE") {
        if (s.density.basis == EmbedBasis::Gaussian)
          bad("Gaussian density requires a real smooth cutoff");
        for (std::size_t i = 0; i < s.density.centres.size(); ++i) {
          double edge = s.density.centres[i] + embed_support(s.density.basis) /
                                                   embed_eta(s.density, i);
          if (edge > s.radius + 16 * std::numeric_limits<double>::epsilon() *
                                    std::max(1.0, s.radius))
            bad("NONE requires every density component to terminate within "
                "RADIUS");
        }
      }
      auto env = core::to_upper(scalar(f, "EMBEDDING.ENVELOPE.TYPE", "OFF"));
      if (env == "OFF")
        s.envelope = EmbedEnvelope::Off;
      else if (env == "SININV")
        s.envelope = EmbedEnvelope::SinInv;
      else if (env == "SMOOTHSTEP2")
        s.envelope = EmbedEnvelope::Smoothstep2;
      else if (env == "SMOOTHSTEP4")
        s.envelope = EmbedEnvelope::Smoothstep4;
      else
        bad("unknown ENVELOPE '" + env + "'");
      if (s.envelope != EmbedEnvelope::Off) {
        auto ec = grid(f, "EMBEDDING.ENVELOPE.CUTOFF");
        if (ec.size() != 1 || !(ec[0] > 0))
          bad("envelope CUTOFF requires one finite positive value");
        s.envelope_cutoff = ec[0];
      }
      if (!f.empty())
        bad("unknown or inactive field " + f.begin()->first);
      out.push_back(std::move(s));
    } catch (const std::exception &e) {
      throw std::runtime_error(std::string(e.what()) + " (" + block.source +
                               ":" + std::to_string(block.first_line) + ")");
    }
  }
  return out;
}

core::ConfigBlock embed_block(const EmbedSpec &s) {
  core::ConfigBlock b{
      "DESCRIPTOR", "", 1, {"DESCRIPTOR " + s.id + " DM_Embed"}};
  std::string pairs = "  PAIRS";
  for (const auto &p : s.pairs)
    pairs += " " + p;
  b.lines.push_back(pairs);
  auto comp = [&](const EmbedComponent &c, bool density) {
    std::string tag = density ? "DENSITY" : "EMBEDDING";
    b.lines.push_back("  " + tag);
    b.lines.push_back(std::string("    BASIS ") + embed_basis_name(c.basis));
    b.lines.push_back("    CENTRES" + values(c.centres));
    if (c.basis == EmbedBasis::Knot5) {
      std::string deg = "    DEGREES";
      for (int d : c.degrees)
        deg += " " + std::to_string(d);
      b.lines.push_back(deg);
    } else
      b.lines.push_back(
          std::string("    ") +
          (c.coordinates == EmbedCoordinates::Eta ? "ETA" : "WIDTHS") +
          values(c.scales));
    if (density) {
      b.lines.push_back(std::string("    FORM ") + rhoform_name(s.form));
      b.lines.push_back("    AMPLITUDES" + values(c.amplitudes));
      b.lines.push_back("    CUTOFF");
      b.lines.push_back("      TYPE " + s.cutoff);
      b.lines.push_back("      RADIUS " + core::format_roundtrip(s.radius));
      b.lines.push_back("    ENDCUTOFF");
    } else {
      b.lines.push_back(std::string("    REFERENCE ") +
                        (s.reference == EmbF0Mode::Zero ? "ZERO" : "FREE"));
      const char *env = s.envelope == EmbedEnvelope::Off      ? "OFF"
                        : s.envelope == EmbedEnvelope::SinInv ? "SinInv"
                        : s.envelope == EmbedEnvelope::Smoothstep2
                            ? "Smoothstep2"
                            : "Smoothstep4";
      b.lines.push_back("    ENVELOPE");
      b.lines.push_back(std::string("      TYPE ") + env);
      if (s.envelope != EmbedEnvelope::Off)
        b.lines.push_back("      CUTOFF " +
                          core::format_roundtrip(s.envelope_cutoff));
      b.lines.push_back("    ENDENVELOPE");
    }
    b.lines.push_back("  END" + tag);
  };
  comp(s.density, true);
  comp(s.embedding, false);
  b.lines.push_back("ENDDESCRIPTOR");
  return b;
}

void resolve_embed_blocks(core::Context &c) {
  auto specs = embed_specs(c);
  if (specs.empty()) {
    c.set_component_cutoffs({});
    return;
  }
  if (c.has_stored("INITMB") && !c.get<bool>("INITMB"))
    bad("DESCRIPTOR blocks are inactive under INITMB false");
  c.add("INITMB", true);
  if (!c.has_stored("TYPEMB"))
    for (const auto &s : specs)
      c.add("TYPEMB", "@" + s.id);
  std::vector<core::ConfigBlock> blocks;
  for (const auto &s : specs)
    blocks.push_back(embed_block(s));
  // Preserve parser source locations during resolution.
  auto raw = c.blocks();
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    blocks[i].source = raw[i].source;
    blocks[i].first_line = raw[i].first_line;
  }
  c.replace_blocks(blocks);
  std::vector<double> radii;
  for (const auto &s : specs)
    radii.push_back(s.radius);
  c.set_component_cutoffs(radii);
}

std::vector<core::Context> embed_contexts(const core::Context &c) {
  auto specs = embed_specs(c);
  std::map<std::string, const EmbedSpec *> byid;
  for (const auto &s : specs)
    byid.emplace(core::to_upper(s.id), &s);
  std::vector<std::string> types;
  if (c.has_stored("TYPEMB"))
    types = c("TYPEMB");
  else
    for (const auto &s : specs)
      types.push_back("@" + s.id);
  std::vector<std::string> legacy;
  std::vector<std::string> order;
  for (const auto &t : types) {
    if (t == "DM_mJoin")
      continue;
    if (!t.empty() && t[0] == '@')
      order.push_back(core::to_upper(t.substr(1)));
    else {
      if (t.rfind("DM_", 0) == 0)
        order.push_back("");
      legacy.push_back(t);
    }
  }
  std::vector<core::Context> old;
  if (!legacy.empty()) {
    auto copy = c;
    copy.replace_blocks({});
    copy.set_component_cutoffs({});
    copy.storage_update("TYPEMB", legacy);
    old = D_mJoin::parse_context(copy, "TYPEMB");
  }
  std::set<std::string> seen;
  std::vector<core::Context> out;
  std::size_t k = 0;
  for (const auto &id : order) {
    if (id.empty()) {
      out.push_back(std::move(old.at(k++)));
      continue;
    }
    auto it = byid.find(id);
    if (it == byid.end())
      bad("TYPEMB references undeclared block @" + id);
    if (!seen.insert(id).second)
      bad("TYPEMB references block @" + id + " more than once");
    const auto &s = *it->second;
    auto child = c;
    child.replace_blocks({embed_block(s)});
    for (const char *key : {"CGRIDMB", "SGRIDMB", "AMPGRIDMB", "CEMBFUNC",
                            "SEMBFUNC", "KDEGMB", "KDEGEMB"})
      child.remove(key);
    child.remove("TYPEMB");
    child.add("TYPEMB", "DM_Embed @" + s.id);
    for (const auto &p : s.pairs)
      child.add("TYPEMB", p);
    child.remove("RCUTMB");
    child.add("RCUTMB", s.radius);
    child.remove("RCTYPEMB");
    child.add("RCTYPEMB", s.cutoff == "NONE" ? "Cut_Dummy" : s.cutoff);
    out.push_back(std::move(child));
  }
  if (seen.size() != specs.size())
    bad("each declared block must be referenced exactly once by TYPEMB");
  return out;
}

std::vector<EmbedParameterBinding> embed_bindings(const core::Context &c) {
  std::vector<EmbedParameterBinding> out;
  for (const auto &s : embed_specs(c)) {
    auto add = [&](std::string path, std::vector<double> v, std::string units,
                   bool positive = false) {
      out.push_back({core::to_upper(s.id + "." + path), s.id, units, positive,
                     std::move(v)});
    };
    auto comp = [&](const EmbedComponent &v, bool density) {
      std::string prefix = density ? "density." : "embedding.";
      std::string unit = density ? "A" : "rho";
      add(prefix + "centres", v.centres, unit);
      if (v.basis != EmbedBasis::Knot5) {
        bool width = v.coordinates == EmbedCoordinates::Widths;
        add(prefix + (width ? "widths" : "eta"), v.scales,
            width ? unit
                  : unit + (v.basis == EmbedBasis::Gaussian ? "^-2" : "^-1"),
            true);
      }
      if (density)
        add(prefix + "amplitudes", v.amplitudes, "", s.form != RhoForm::Signed);
    };
    comp(s.density, true);
    comp(s.embedding, false);
    if (s.cutoff != "NONE")
      add("density.cutoff.radius", {s.radius}, "A", true);
    if (s.envelope != EmbedEnvelope::Off)
      add("embedding.envelope.cutoff", {s.envelope_cutoff}, "rho", true);
  }
  return out;
}
EmbedParameterBinding embed_binding(const core::Context &c,
                                    const std::string &path) {
  for (auto &p : embed_bindings(c))
    if (p.path == core::to_upper(path))
      return p;
  bad("unknown, inactive or discrete parameter '" + path + "'");
}
double EmbedParameterBinding::eta_jacobian(const EmbedComponent &c,
                                           std::size_t i) const {
  if (c.coordinates == EmbedCoordinates::Eta)
    return 1;
  double w = c.scales.at(i);
  return c.basis == EmbedBasis::Gaussian ? -2 * embed_eta(c, i) / w
                                         : -embed_eta(c, i) / w;
}
void EmbedParameterBinding::write(core::Context &c, std::size_t index,
                                  double value) const {
  // Rebind against this candidate, rejecting stale aliases and dimensions.
  auto current = embed_binding(c, path);
  if (index >= current.values.size() || !std::isfinite(value))
    bad("invalid coordinate update for " + path);
  current.values[index] = value;
  write_embed_coordinates(c, {{path, current.values}});
}
void write_embed_coordinates(
    core::Context &c,
    const std::map<std::string, std::vector<double>> &updates) {
  auto specs = embed_specs(c);
  for (const auto &update : updates) {
    auto binding = embed_binding(c, update.first);
    if (update.second.size() != binding.values.size())
      bad("coordinate count changed for " + binding.path);
    for (double value : update.second)
      if (!std::isfinite(value))
        bad("nonfinite coordinate for " + binding.path);
    for (auto &s : specs)
      if (core::to_upper(s.id) == core::to_upper(binding.owner)) {
        std::string field = binding.path.substr(binding.owner.size() + 1);
        if (field == "DENSITY.CUTOFF.RADIUS")
          s.radius = update.second[0];
        else if (field == "EMBEDDING.ENVELOPE.CUTOFF")
          s.envelope_cutoff = update.second[0];
        else {
          bool density = field.rfind("DENSITY.", 0) == 0;
          auto &v = density ? s.density : s.embedding;
          field = field.substr(field.find('.') + 1);
          if (field == "CENTRES")
            v.centres = update.second;
          else if (field == "AMPLITUDES")
            v.amplitudes = update.second;
          else
            v.scales = update.second;
        }
      }
  }
  auto candidate = c;
  std::vector<core::ConfigBlock> blocks;
  for (const auto &s : specs)
    blocks.push_back(embed_block(s));
  candidate.replace_blocks(blocks);
  resolve_embed_blocks(candidate);
  c = std::move(candidate);
}
} // namespace models
} // namespace tadah
