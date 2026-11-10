#include "tadah/core/core_types.h"
#include "tadah/core/number_format.h"
#include "tadah/core/utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <utility>
#include <vector>
namespace tadah {
namespace models {

namespace {
// The generator as written, for a message: "LIN 9" or "GEOM 3 2 -4".
std::string generator_text(std::vector<std::string>::const_iterator b,
                           std::vector<std::string>::const_iterator e) {
  std::string s;
  for (auto i = b; i != e; ++i) s += (s.empty() ? "" : " ") + *i;
  return s;
}
}  // namespace

core::v_type D_Base::get_grid(const std::vector<std::string>&vold) {
  // The largest number of points a generator makes, as in the embedding
  // grids of a component specification (embed_spec.cpp).
  constexpr double max_points = 1000000;
  core::v_type vnew;
  auto it = vold.begin();
  while (it != vold.end()) {
    const auto first = it;
    std::string token = *it++;
    const std::string gen = core::to_upper(token);
    if (gen == "LIN" || gen == "GEOM") {
      // <n> <start> <stop>: each value is read only if the list holds it.
      if (vold.end() - it < 3)
        throw std::invalid_argument(
            "Invalid grid specification: " + gen +
            " takes three values, <n> <start> <stop>, but the grid ends "
            "after '" + generator_text(first, vold.end()) + "'");
      const std::string text = generator_text(first, it + 3);
      double n = 0.0;
      if (!core::parse_decimal_finite(*it, n) || n < 1 || n > max_points ||
          std::floor(n) != n)
        throw std::invalid_argument(
            "Invalid grid specification: the number of points of '" + text +
            "' must be a whole number from 1 to 1000000");
      // parse_decimal_finite: a finite decimal literal, a subnormal value
      // included (the lossless Context/pot writers produce one).
      double cgmin = 0.0, cgmax = 0.0;
      if (!core::parse_decimal_finite(*(it + 1), cgmin) ||
          !core::parse_decimal_finite(*(it + 2), cgmax))
        throw std::invalid_argument(
            "Invalid grid specification: the start and stop of '" + text +
            "' must be finite decimal numbers");
      it += 3;
      const int cg = static_cast<int>(n);
      core::v_type v;
      if (gen == "LIN") {
        v = core::linspace(cgmin, cgmax, cg);
      } else {
        // A geometric progression from start to stop: both above 0.
        if (!(cgmin > 0.0) || !(cgmax > 0.0))
          throw std::invalid_argument(
              "Invalid grid specification: the start and stop of '" + text +
              "' must be above 0 (GEOM makes a geometric progression)");
        v = core::geomspace(cgmin, cgmax, cg, exp(1));
      }
      // Finite endpoints whose difference is beyond the largest double
      // make an infinite LIN step, and a GEOM point rounded past the
      // largest double is infinite: such points are not numbers.
      for (const double x : v)
        if (!std::isfinite(x))
          throw std::invalid_argument(
              "Invalid grid specification: the points of '" + text +
              "' are not all finite numbers (" +
              (gen == "LIN" ? "its stop minus its start is beyond the "
                              "largest number"
                            : "a point is beyond the largest number") +
              ")");
      vnew.insert(vnew.end(), v.begin(), v.end());
    }
    else {
      // core::parse_number: std::stod that also accepts a subnormal value
      // (the lossless Context/pot writers produce one for every subnormal
      // double).
      double v = 0.0;
      if (!core::parse_number(token, v))
        throw std::invalid_argument("Invalid grid specification: " + token);
      vnew.push_back(v);
    }
  }
  return vnew;
}

void D_Base::get_grid(const tadah::core::Context &c, const std::string &key, tadah::core::v_type &v) {
  std::vector<std::string> grid_line(c.size(key));
  c.get<>(key, grid_line);
  try {
    v = get_grid(grid_line);
  } catch (const std::invalid_argument &e) {
    throw std::invalid_argument(key + ": " + e.what());
  }
}

D_Base::~D_Base() {
  if (this->manage_memory && fcut) {
    delete fcut;
  }
}

D_Base::D_Base() {
  for (int i = 0; i < 119; ++i) std::fill_n(weights[i], 119, 1.0);
}
D_Base::D_Base(const tadah::core::Context &c):
  verbose(c.get<int>("VERBOSE"))
{
  for (int i = 0; i < 119; ++i) std::fill_n(weights[i], 119, 1.0);
  update_weights(c);
}
void D_Base::update_weights(const tadah::core::Context &c) {
  watom_Z_by_arg_pos.clear();
  watom_pair_by_arg_pos.clear();
  watom_mode = WatomMode::Empty;
  WatomSpec spec = parse_watom(c);
  watom_mode = spec.mode;
  if (spec.mode == WatomMode::PerSpecies)
    watom_Z_by_arg_pos = std::move(spec.Zs);
  else if (spec.mode == WatomMode::PairTriple)
    watom_pair_by_arg_pos = std::move(spec.pairs);
  for (const WatomWrite &e : spec.writes) {
    weights[e.Zi][e.Zj] = e.w;
    weights[e.Zj][e.Zi] = e.w;
  }
}
bool D_Base::watom_number_shaped(const std::string &tok) {
  std::size_t i = 0;
  if (i < tok.size() && (tok[i] == '+' || tok[i] == '-')) ++i;
  if (i < tok.size() &&
      (std::isdigit(static_cast<unsigned char>(tok[i])) || tok[i] == '.'))
    return true;
  std::string w = tok.substr(i);
  std::transform(w.begin(), w.end(), w.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return w == "inf" || w == "infinity" || w.rfind("nan", 0) == 0;
}
double D_Base::watom_value(const std::string &tok, std::size_t entry) {
  double v = 0.0;
  if (!tadah::core::parse_decimal_finite(tok, v))
    throw std::runtime_error(
        "WATOM entry " + std::to_string(entry) + " ('" + tok +
        "') is not a decimal number: WATOM takes the number grammar of "
        "potential and configuration files, [+-]digits[.digits][e[+-]digits] "
        "(no hexadecimal, inf or nan, and a finite value)");
  return v;
}
D_Base::WatomSpec D_Base::parse_watom(const tadah::core::Context &c) {
  WatomSpec spec;
  if (!c.contains("WATOM") || c.size("WATOM") == 0) return spec;

  const size_t n = c.size("WATOM");
  const bool per_species =
    watom_number_shaped(c.get<std::string>("WATOM", 0));

  if (per_species) {
    if (!c.contains("ATOM")) return spec;
    std::vector<int> Zs(n);
    for (size_t i = 0; i < n; ++i) {
      Zs[i] = tadah::core::PeriodicTable::find_by_symbol(
                c.get<std::string>("ATOM", i)).Z;
    }
    spec.mode = WatomMode::PerSpecies;
    for (size_t i = 0; i < n; ++i)
      spec.values.push_back(
          watom_value(c.get<std::string>("WATOM", i), i + 1));
    for (size_t i = 0; i < n; ++i) {
      const double wi = spec.values[i];
      spec.writes.push_back({Zs[i], Zs[i], wi});
      for (size_t j = i + 1; j < n; ++j) {
        const double w = std::sqrt(wi * spec.values[j]);
        spec.writes.push_back({Zs[i], Zs[j], w});
      }
    }
    spec.Zs = std::move(Zs);
  } else {
    if (n % 3 != 0) {
      throw std::runtime_error(
        "WATOM pair format: expected sym sym value triples");
    }
    spec.mode = WatomMode::PairTriple;
    spec.pairs.reserve(n / 3);
    for (size_t i = 0; i < n; i += 3) {
      const int Zi = tadah::core::PeriodicTable::find_by_symbol(
                       c.get<std::string>("WATOM", i)).Z;
      const int Zj = tadah::core::PeriodicTable::find_by_symbol(
                       c.get<std::string>("WATOM", i + 1)).Z;
      const double w = watom_value(c.get<std::string>("WATOM", i + 2), i + 3);
      spec.values.push_back(w);
      spec.writes.push_back({Zi, Zj, w});
      spec.pairs.emplace_back(Zi, Zj);
    }
  }
  return spec;
}
void D_Base::require_positive_density_weights(const tadah::core::Context &c,
                                              const std::string &who) {
  const WatomSpec spec = parse_watom(c);
  for (size_t k = 0; k < spec.values.size(); ++k) {
    const double w = spec.values[k];
    if (std::isfinite(w) && w > 0.0) continue;
    const std::string entry =
        spec.mode == WatomMode::PerSpecies
            ? "WATOM entry " + std::to_string(k + 1) + " (" +
                  c.get<std::string>("ATOM", k) + ")"
            : "WATOM pair " + std::to_string(k + 1) + " (" +
                  c.get<std::string>("WATOM", 3 * k) + " " +
                  c.get<std::string>("WATOM", 3 * k + 1) + ")";
    throw DensityFormError(
        who + ": every density pair weight must be finite and > 0; " + entry +
        " is " + core::format_roundtrip(w) + ".");
  }
}
std::vector<double> D_Base::density_pair_weights(const tadah::core::Context &c) {
  std::vector<std::string> tok;
  for (size_t i = 0;; ++i) {
    const auto t = c.stored_value("WATOM", i);
    if (!t) break;
    tok.push_back(*t);
  }
  std::vector<double> w;
  if (tok.empty()) return w;
  auto num = [&](size_t i) { return watom_value(tok[i], i + 1); };
  if (watom_number_shaped(tok[0])) {
    // per species, parallel to ATOM (parse_watom: no ATOM, no weights)
    if (!c.has_stored("ATOM")) return w;
    std::vector<double> ws;
    for (size_t i = 0; i < tok.size(); ++i) ws.push_back(num(i));
    for (size_t i = 0; i < ws.size(); ++i) {
      w.push_back(ws[i]);
      for (size_t j = i + 1; j < ws.size(); ++j)
        w.push_back(std::sqrt(ws[i] * ws[j]));
    }
    return w;
  }
  if (tok.size() % 3 != 0)
    throw std::runtime_error(
        "WATOM pair format: expected sym sym value triples");
  for (size_t i = 2; i < tok.size(); i += 3) w.push_back(num(i));
  return w;
}
int D_Base::get_atom_Z_for_arg_pos(int arg_pos) const {
  if (arg_pos < 1) return 0;
  const size_t idx = static_cast<size_t>(arg_pos - 1);
  if (idx >= watom_Z_by_arg_pos.size()) return 0;
  return watom_Z_by_arg_pos[idx];
}
bool D_Base::has_watom_arg_pos(int arg_pos) const {
  if (arg_pos < 1) return false;
  const size_t idx = static_cast<size_t>(arg_pos - 1);
  switch (watom_mode) {
    case WatomMode::PerSpecies: return idx < watom_Z_by_arg_pos.size();
    case WatomMode::PairTriple: return idx < watom_pair_by_arg_pos.size();
    case WatomMode::Empty:      return false;
  }
  return false;
}
bool D_Base::watom_value_weighs_a_pair(int arg_pos) const {
  if (!has_watom_arg_pos(arg_pos)) return false;
  const size_t idx = static_cast<size_t>(arg_pos - 1);
  if (watom_mode == WatomMode::PerSpecies) {
    // update_weights writes the pairs of ATOM elements: (Z, Zj) for every
    // Zj of ATOM, Z itself included.
    const int Z = watom_Z_by_arg_pos[idx];
    for (const int Zj : watom_Z_by_arg_pos)
      if (is_init_for_atoms(Z, Zj)) return true;
    return false;
  }
  const auto &pair = watom_pair_by_arg_pos[idx];
  return is_init_for_atoms(pair.first, pair.second);
}
bool D_Base::depends_on(const std::string &theta_name, int arg_pos) const {
  if (theta_name == "WATOM")
    return reads_watom() && watom_value_weighs_a_pair(arg_pos);
  return std::find(keys.begin(), keys.end(), theta_name) != keys.end() ||
         std::find(context_keys.begin(), context_keys.end(), theta_name) !=
             context_keys.end();
}
double D_Base::dweight_dw_n(int Zi, int Zj, int arg_pos) const {
  if (arg_pos < 1) return 0.0;
  const size_t idx = static_cast<size_t>(arg_pos - 1);
  if (watom_mode == WatomMode::PerSpecies) {
    if (idx >= watom_Z_by_arg_pos.size()) return 0.0;
    const int Z_target = watom_Z_by_arg_pos[idx];
    const bool zi_eq = (Zi == Z_target);
    const bool zj_eq = (Zj == Z_target);
    if (!zi_eq && !zj_eq) return 0.0;
    if (zi_eq && zj_eq)   return 1.0;
    const double w_n     = weights[Z_target][Z_target];
    const int    other   = zi_eq ? Zj : Zi;
    const double w_other = weights[other][other];
    if (w_n <= 0.0) return 0.0;
    return 0.5 * std::sqrt(w_other / w_n);
  }
  if (watom_mode == WatomMode::PairTriple) {
    if (idx >= watom_pair_by_arg_pos.size()) return 0.0;
    const auto &pair = watom_pair_by_arg_pos[idx];
    const int Za = pair.first;
    const int Zb = pair.second;
    return ((Zi == Za && Zj == Zb) || (Zi == Zb && Zj == Za)) ? 1.0 : 0.0;
  }
  return 0.0;
}
void D_Base::printGrid(core::v_type &v, const std::string &key, core::LogLevel logLevel) {
  core::Logger::instance().log(logLevel, label()," ",key,":\n  ", v);
}

void D_Base::set_fidx(size_t fidx_) { fidx=fidx_; }
size_t D_Base::get_fidx() const { return fidx; }
void D_Base::set_fcut(Cut_Base* cut, bool manage_memory) {
  if (this->manage_memory && fcut != cut) {
    delete fcut;
  }
  fcut = cut;
  this->manage_memory = manage_memory;
}
int D_Base::get_arg_pos(const std::string &key) const {
  auto it = std::find(keys.begin(), keys.end(), key);

  if (it == keys.end()) return -1;  // incl. data_keys: not OPTIM-addressable

  // fparam = leading family params before the per-key size tokens
  const long fparam = static_cast<long>(nparams) -
                      static_cast<long>(keys.size()) -
                      static_cast<long>(data_keys.size());
  return static_cast<int>(std::distance(keys.begin(), it) + fparam);
}
size_t D_Base::size() const { return s; }
double D_Base::get_rcut() const { 
  return fcut->get_rcut(); 
}
bool D_Base::is_init_for_atoms(int Zi, int Zj) const {
  return init_for_atoms_map.is_init(Zi,Zj) ;
}
bool D_Base::applies_to_every_element_pair() const {
  // init_for_atoms() keeps the map symmetric; Z runs over 1..118.
  for (int Zi = 1; Zi < 119; ++Zi)
    for (int Zj = Zi; Zj < 119; ++Zj)
      if (!is_init_for_atoms(Zi, Zj)) return false;
  return true;
}
void D_Base::init_for_atoms(int Zi, int Zj) {
  init_for_atoms_map.init(Zi,Zj);
}
void D_Base::uninit_for_atoms(int Zi, int Zj) {
  init_for_atoms_map.uninit(Zi,Zj);
}
void D_Base::init_for_atoms(const std::vector<std::string> &Zs) {
  if (Zs.size() % 2 != 0) {
    throw std::invalid_argument("The vector size must be even.");
  }
  
  for (size_t i = 0; i < Zs.size(); i += 2) {
    if (Zs[i] == "*" && Zs[i+1] != "*") {
      int Zj = tadah::core::PeriodicTable::find_by_symbol(Zs[i+1]).Z;
      for (int Zi = 1; Zi < 119; ++Zi) {
        init_for_atoms(Zi, Zj);
      }
    } 
    else if (Zs[i] != "*" && Zs[i+1] == "*") {
      int Zi = tadah::core::PeriodicTable::find_by_symbol(Zs[i]).Z;
      for (int Zj = 1; Zj < 119; ++Zj) {
        init_for_atoms(Zi, Zj);
      }
    } 
    else if (Zs[i] == "*" && Zs[i+1] == "*") {
      for (int Zi = 1; Zi < 119; ++Zi) {
        for (int Zj = Zi; Zj < 119; ++Zj) {
          init_for_atoms(Zi, Zj);
        }
      }
    } 
    else {
      int Zi = tadah::core::PeriodicTable::find_by_symbol(Zs[i]).Z;
      int Zj = tadah::core::PeriodicTable::find_by_symbol(Zs[i+1]).Z;
      init_for_atoms(Zi, Zj);
    }
  }
}
std::vector<std::string> D_Base::element_tokens(const std::string &key,
                                                std::vector<std::string> line) const {
  const size_t nsizes = keys.size() + data_keys.size();
  const std::string name =
      line.empty() ? std::string("<no descriptor>") : line[0];
  if (nsizes > nparams) {
    throw std::logic_error(name + " declares " + std::to_string(nsizes) +
                           " list keys but takes " + std::to_string(nparams) +
                           " parameters");
  }
  std::string text = key;
  for (const auto &t : line) text += " " + t;
  const std::string takes =
      key + " " + name + " takes " + std::to_string(nparams) +
      (nparams == 1 ? " parameter" : " parameters") +
      " before its element pairs; ";

  const size_t have = line.empty() ? 0 : line.size() - 1;
  if (have < nparams) {
    throw std::runtime_error(takes + "the line has " + std::to_string(have) +
                             ": " + text);
  }

  // SIZE tokens: line[1 + fparam + k] is the size of the k-th list key.
  const size_t fparam = nparams - nsizes;
  for (size_t k = 0; k < nsizes; ++k) {
    const std::string &token = line[1 + fparam + k];
    if (tadah::core::is_integer(token)) continue;
    const std::string &list_key =
        k < keys.size() ? keys[k] : data_keys[k - keys.size()];
    throw std::runtime_error(takes + "parameter " +
                             std::to_string(fparam + k + 1) +
                             ", the number of " + list_key +
                             " values, must be a non-negative integer, not \"" +
                             token + "\": " + text);
  }

  line.erase(line.begin(),
             line.begin() + static_cast<std::ptrdiff_t>(nparams + 1));
  return line;
}

} // namespace models
} // namespace tadah

