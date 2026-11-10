#include "tadah/core/registry.h"
#include <cctype>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tadah/core/key_notice.h>
#include <tadah/core/number_format.h>
#include <tadah/models/descriptors/d_mjoin.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/dm/embed_spec.h>
#include <vector>
namespace tadah {
namespace models {

namespace {
// parse_context() splits the standard grid keys (and the declared
// data_keys, e.g. the knot-degree lists). A descriptor whose keys fall
// outside the supported set is left untouched by expand_grids; it reads its
// own grids straight from the context. The list keys a descriptor reads
// whole (its context_keys: the CEMBFUNC/SEMBFUNC of a DM_Poly channel
// function) are kept whole on a single line (parse_context). mJoin meta-descriptors are always
// handled (their sub-descriptors are standard). Keyless descriptors (D2_EAM,
// DM_EAM) are trivially handled.
// The list keys parse_context splits among the descriptors of a TYPE2B or
// a TYPEMB line.
const std::set<std::string> &list_keys_of(const std::string &type_str) {
  static const std::set<std::string> sup2b = {"CGRID2B", "SGRID2B",
                                              "KDEG2B"};
  static const std::set<std::string> supmb = {"CGRIDMB", "SGRIDMB",
                                              "CEMBFUNC", "SEMBFUNC",
                                              "AMPGRIDMB", "KDEGMB",
                                              "KDEGEMB"};
  return type_str == "TYPE2B" ? sup2b : supmb;
}

bool mjoin_can_handle(const tadah::core::Context &c, const std::string &type_str) {
  if(type_str=="TYPEMB" && !c.blocks("DESCRIPTOR").empty()) return true;
  if (!c.contains(type_str) || c.size(type_str) == 0) return false;
  const std::string label = c.get<std::string>(type_str, 0);
  if (label.find("mJoin") != std::string::npos) return true;
  const std::set<std::string> &sup = list_keys_of(type_str);
  std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(label));
  for (const auto &k : d->keys)
    if (!sup.count(k)) return false;
  for (const auto &k : d->data_keys)
    if (!sup.count(k)) return false;
  return true;
}

std::string joined_text(const std::vector<std::string> &v, std::size_t from,
                        std::size_t to) {
  std::string s;
  for (std::size_t i = from; i < to; ++i) s += (s.empty() ? "" : " ") + v[i];
  return s;
}
} // namespace


D_mJoin::D_mJoin() {}
D_mJoin::~D_mJoin() {}

// Lossless: each value is written as its shortest round-trip text, so the
// sub-descriptor contexts see bit-identical grids. (A fixed NUMERIC-decimal
// rendering here used to round every expanded grid value, and flush values
// below 10^-NUMERIC to zero, on every mJoin construction.)
std::vector<std::string> convert_to_strings(const tadah::core::v_type& vec) {
  std::vector<std::string> result;
  result.reserve(vec.size());
  for (double num : vec) {
    result.push_back(tadah::core::format_roundtrip(num));
  }
  return result;
}

// The grid of `key` (its values and LIN/GEOM generators) as values; a
// refusal of get_grid names the key, as D_Base::get_grid(Context) does.
static std::vector<std::string> expanded_grid(const std::vector<std::string> &g,
                                              const std::string &key) {
  try {
    return convert_to_strings(D_Base::get_grid(g));
  } catch (const std::invalid_argument &e) {
    throw std::invalid_argument(key + ": " + e.what());
  }
}

// A potential's RCTYPE, RCUT and list values as its descriptors read them
// (parse_context has reported the others): the values the descriptors of
// the line take, in order -- the whole of a key a single descriptor reads
// whole -- and none of a key no descriptor reads. A single descriptor's
// cutoff is the largest RCUT value, which it keeps.
static void keep_read_values(tadah::core::Context &c,
                             const std::vector<tadah::core::Context> &contexts,
                             const std::string &type_str) {
  const std::string sfx = type_str.substr(type_str.size() - 2);
  const std::string rctype = "RC" + type_str, rcut = "RCUT" + sfx;
  const bool joined =
      c.get<std::string>(type_str, 0).find("mJoin") != std::string::npos;
  std::vector<std::string> keys = {rctype, rcut};
  for (const auto &k : list_keys_of(type_str)) keys.push_back(k);
  for (const auto &key : keys) {
    if (!c.contains(key)) continue;
    std::vector<std::string> kept;
    if (key == rcut && !joined) {
      std::size_t largest = 0;
      for (std::size_t i = 1; i < c.size(key); ++i)
        if (c.get<double>(key, i) > c.get<double>(key, largest)) largest = i;
      kept.push_back(c.get<std::string>(key, largest));
    } else {
      for (const auto &c1 : contexts)
        if (c1.contains(key))
          for (std::size_t i = 0; i < c1.size(key); ++i)
            kept.push_back(c1.get<std::string>(key, i));
    }
    c.remove(key);
    for (const auto &v : kept) c.add(key, v);
  }
}

std::vector<D_mJoin::LeftOut> D_mJoin::expand_grids(tadah::core::Context &c) {
  resolve_embed_blocks(c);
  // Training from a potential leaves out what its descriptors do not read,
  // so the training context builds the potential's model.
  const bool potential = c.has_stored("WEIGHTS");
  std::vector<LeftOut> left_out;
  for (const std::string type_str : {"TYPE2B", "TYPEMB"}) {
    if (!mjoin_can_handle(c, type_str)) continue;
    std::vector<tadah::core::Context> contexts = parse_context(c, type_str);
    const bool keep = potential &&
                      !(type_str == "TYPEMB" && !c.blocks("DESCRIPTOR").empty());
    // The potential's values as given, list keys expanded as they are kept
    // (parse_context has read them).
    std::vector<LeftOut> given;
    if (keep) {
      const std::string sfx = type_str.substr(type_str.size() - 2);
      std::vector<std::string> keys = {"RC" + type_str, "RCUT" + sfx};
      for (const auto &k : list_keys_of(type_str)) keys.push_back(k);
      for (const auto &k : keys) {
        if (!c.contains(k)) continue;
        std::vector<std::string> v(c.size(k));
        c.get(k, v);
        if (k.rfind("RC", 0) != 0) v = convert_to_strings(D_Base::get_grid(v));
        given.push_back({k, v, {}});
      }
    }
    expand_grids(c, contexts, type_str);
    if (!keep) continue;
    keep_read_values(c, contexts, type_str);
    for (auto &g : given) {
      if (c.contains(g.key)) {
        g.kept.resize(c.size(g.key));
        c.get(g.key, g.kept);
      }
      if (g.kept.size() < g.given.size()) left_out.push_back(std::move(g));
    }
  }
  return left_out;
}
void D_mJoin::expand_grids(tadah::core::Context &c, std::string type_str) {
  std::vector<tadah::core::Context> contexts = parse_context(c, type_str);
  expand_grids(c,contexts,type_str);
}
void D_mJoin::check_single_lines(const tadah::core::Context &c) {
  for (const std::string type_str : {"TYPE2B", "TYPEMB"}) {
    if (!c.get<bool>(type_str == "TYPE2B" ? "INIT2B" : "INITMB")) continue;
    if (type_str == "TYPEMB" && !c.blocks("DESCRIPTOR").empty()) continue;
    if (!mjoin_can_handle(c, type_str)) continue;
    if (c.get<std::string>(type_str, 0).find("mJoin") != std::string::npos)
      continue;  // D2_mJoin and DM_mJoin split their own lines
    parse_context(c, type_str);
  }
}
void D_mJoin::check_potential_lines(const tadah::core::Context &c,
                                    const std::string &command) {
  if (command.empty())
    throw std::invalid_argument(
        "D_mJoin::check_potential_lines: no command named");
  const std::string tail =
      ", and a potential's value is changed by editing its line in a copy of "
      "the potential (the Training page of the documentation, Retraining "
      "from a potential)";
  for (const std::string type_str : {"TYPE2B", "TYPEMB"}) {
    if (!c.has_stored(type_str)) continue;
    const std::string sfx = type_str.substr(type_str.size() - 2);
    std::vector<std::string> keys = {type_str, "RC" + type_str, "RCUT" + sfx};
    for (const auto &k : list_keys_of(type_str)) keys.push_back(k);
    for (const auto &key : keys) {
      const std::vector<tadah::core::KeyLine> lines = c.key_lines(key);
      // The lines of each potential file, by the order of the lines.
      std::map<std::string, std::vector<std::size_t>> of_file;
      for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i].potential) of_file[lines[i].source].push_back(i);
      for (const auto &[source, at] : of_file) {
        if (at.size() < 2) continue;
        std::string on;
        for (std::size_t k = 0; k < at.size(); ++k)
          on += (k == 0 ? "" : k + 1 == at.size() ? " and on " : ", on ") +
                lines[at[k]].where;
        if (key != type_str)
          throw std::runtime_error(
              "Key " + key + " is set on " + on +
              ", lines of one potential (a file with WEIGHTS): " +
              tadah::core::task_command_words(command) +
              " reads a key of a potential's descriptors from the line the "
              "potential has for it" +
              tail);
        // The TYPE key of a potential as tadah writes it: lines one after
        // another (only comments and blank lines between them,
        // core::KeyLine::ordinal), a new line at each descriptor's name and
        // at each DESCRIPTOR block's ID (D2_mJoin, then a line per
        // descriptor; TYPEMB @ta on a line of its own), the lines
        // Context::written_line_sizes() gives for their values. The lines
        // give the key its values in line order.
        std::vector<std::string> values(c.size(key));
        c.get(key, values);
        std::vector<std::size_t> first(lines.size(), 0);
        std::size_t n = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) {
          first[i] = n;
          n += lines[i].values;
        }
        if (n != values.size())
          throw std::logic_error(
              "D_mJoin::check_potential_lines: the lines of " + key +
              " give " + std::to_string(n) + " values, and it has " +
              std::to_string(values.size()));
        // The first lines of the file's key that follow one another.
        std::size_t run = 1;
        while (run < at.size() &&
               lines[at[run]].ordinal == lines[at[run - 1]].ordinal + 1)
          ++run;
        // Whether the file's lines at[0, m) are those tadah writes for
        // their values.
        auto as_written = [&](std::size_t m) {
          std::vector<std::string> own;
          for (std::size_t k = 0; k < m; ++k) {
            const std::size_t i = at[k];
            own.insert(own.end(),
                       values.begin() + static_cast<std::ptrdiff_t>(first[i]),
                       values.begin() + static_cast<std::ptrdiff_t>(
                                            first[i] + lines[i].values));
          }
          const std::vector<std::size_t> written =
              c.written_line_sizes(key, own);
          if (written.size() != m) return false;
          for (std::size_t k = 0; k < m; ++k)
            if (lines[at[k]].values != written[k]) return false;
          return true;
        };
        const bool first_written = as_written(run);
        if (run == at.size() && first_written) continue;
        // How a potential has its TYPE lines, with the ID of the first
        // DESCRIPTOR block that the key names as an example.
        std::string block;
        for (const auto &v : values)
          if (!v.empty() && v.front() == '@') {
            block = v;
            break;
          }
        const std::string rule =
            tadah::core::task_command_words(command) + " reads the " + key +
            " key of a potential from the lines tadah writes for it, which "
            "follow one another, a new line at each descriptor's name" +
            (block.empty() ? std::string()
                           : " and at each DESCRIPTOR block's ID (" + block +
                                 ")") +
            tail;
        if (!first_written)
          throw std::runtime_error(
              "Key " + key + " is set on " + on +
              ", lines of one potential (a file with WEIGHTS): " + rule);
        // The first lines are as tadah writes them: the lines after them
        // are outside them.
        std::string outside;
        for (std::size_t k = run; k < at.size(); ++k)
          outside += (k == run ? "" : k + 1 == at.size() ? " and " : ", ") +
                     lines[at[k]].where;
        outside[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(outside[0])));
        const std::size_t from = lines[at[0]].line;
        const std::size_t to = lines[at[run - 1]].line;
        const std::string written =
            run == 1 ? "line " + std::to_string(from)
                     : "lines " + std::to_string(from) +
                           (run == 2 ? " and " : " to ") + std::to_string(to);
        throw std::runtime_error(
            outside +
            (at.size() - run == 1 ? " is a " + key + " line"
                                  : " are " + key + " lines") +
            " of a potential (a file with WEIGHTS) outside its " + key +
            (run == 1 ? " line (" : " lines (") + written + "): " + rule);
      }
    }
  }
}
void D_mJoin::expand_grids(tadah::core::Context &c, std::vector<tadah::core::Context> &contexts,std::string type_str) {
  if (type_str=="TYPE2B") {
    c.remove("CGRID2B"); c.remove("SGRID2B");
    for (auto &c1 : contexts) {
      if (c1.contains("CGRID2B"))
        for (size_t i = 0; i < c1.size("CGRID2B"); ++i)
          c.add("CGRID2B", c1.get<double>("CGRID2B", i));
      if (c1.contains("SGRID2B"))
        for (size_t i = 0; i < c1.size("SGRID2B"); ++i)
          c.add("SGRID2B", c1.get<double>("SGRID2B", i));
    }
  }
  else if (type_str=="TYPEMB") {
    c.remove("CGRIDMB"); c.remove("SGRIDMB");
    c.remove("CEMBFUNC"); c.remove("SEMBFUNC");

    for (auto &c1 : contexts) {
      if (c1.contains("CGRIDMB"))
        for (size_t i = 0; i < c1.size("CGRIDMB"); ++i)
          c.add("CGRIDMB", c1.get<double>("CGRIDMB", i));
      if (c1.contains("SGRIDMB"))
        for (size_t i = 0; i < c1.size("SGRIDMB"); ++i)
          c.add("SGRIDMB", c1.get<double>("SGRIDMB", i));
      if (c1.contains("CEMBFUNC"))
        for (size_t i = 0; i < c1.size("CEMBFUNC"); ++i)
          c.add("CEMBFUNC", c1.get<double>("CEMBFUNC", i));
      if (c1.contains("SEMBFUNC"))
        for (size_t i = 0; i < c1.size("SEMBFUNC"); ++i)
          c.add("SEMBFUNC", c1.get<double>("SEMBFUNC", i));
    }
  }
  else {
    std::cerr << "Cannot expand context grid: wrong keyword: " << type_str << std::endl;
  }
  if(type_str=="TYPEMB") resolve_embed_blocks(c);
}
std::vector<tadah::core::Context> D_mJoin::parse_context(const tadah::core::Context &c, std::string type_str) {
  if(type_str=="TYPEMB" && !c.blocks("DESCRIPTOR").empty()) return embed_contexts(c);
  std::string prefix = type_str=="TYPE2B" ? "D2_" : "DM_";
  std::string rctypekey = "RC" + type_str;
  std::string rcutkey = "RCUT" + type_str.substr(type_str.size() - 2);
  std::string cgridkey = "CGRID" + type_str.substr(type_str.size() - 2);
  std::string sgridkey = "SGRID" + type_str.substr(type_str.size() - 2);

  std::map<std::string, std::pair<const std::vector<std::string>*, size_t>> data_map;

  std::vector<std::string> types(c.size(type_str)); // including params TYPE2B D2_Blip 7 7
  c.get(type_str, types);
  //types.erase(types.begin()); // remove D2_mJoin or DM_mJoin
  std::string pattern="mJoin";
  types.erase(
    std::remove_if(types.begin(), types.end(), [&pattern](const std::string& s) {
      return s.find(pattern) != std::string::npos;
    }),
    types.end()
  );
  
  std::vector<std::string> rctypes(c.size(rctypekey));
  c.get(rctypekey, rctypes);
  data_map[rctypekey]= { &rctypes, 0 };

  std::vector<std::string> rcut(c.size(rcutkey));
  c.get(rcutkey, rcut);
  data_map[rcutkey]={&rcut, 0};

  std::vector<std::string> cgrid;
  if (c.contains(cgridkey)) {
    cgrid.resize(c.size(cgridkey));
    c.get(cgridkey, cgrid);
    cgrid = expanded_grid(cgrid, cgridkey);
    data_map[cgridkey] = {&cgrid, 0};
  }
  std::vector<std::string> sgrid;
  if (c.contains(sgridkey)) {
    sgrid.resize(c.size(sgridkey));
    c.get(sgridkey, sgrid);
    sgrid = expanded_grid(sgrid, sgridkey);
    data_map[sgridkey] = {&sgrid, 0};
  }
  std::vector<std::string> cembgrid;
  if (c.contains("CEMBFUNC")) {
    cembgrid.resize(c.size("CEMBFUNC"));
    c.get("CEMBFUNC", cembgrid);
    cembgrid = expanded_grid(cembgrid, "CEMBFUNC");
    data_map["CEMBFUNC"] = {&cembgrid, 0};
  }
  std::vector<std::string> sembgrid;
  if (c.contains("SEMBFUNC")) {
    sembgrid.resize(c.size("SEMBFUNC"));
    c.get("SEMBFUNC", sembgrid);
    sembgrid = expanded_grid(sembgrid, "SEMBFUNC");
    data_map["SEMBFUNC"] = {&sembgrid, 0};
  }
  // additional splittable lists: density amplitudes (optimizable key) and
  // the knot-degree data_keys (split-only, never OPTIM)
  std::vector<std::string> extra_lists[4];
  const char *extra_names[4] = {"AMPGRIDMB", "KDEG2B", "KDEGMB", "KDEGEMB"};
  for (int e = 0; e < 4; ++e) {
    const std::string k = extra_names[e];
    if (c.contains(k)) {
      extra_lists[e].resize(c.size(k));
      c.get(k, extra_lists[e]);
      extra_lists[e] = expanded_grid(extra_lists[e], k);
      data_map[k] = {&extra_lists[e], 0};
    }
  }

  // The descriptors of the line, each with its tokens: its label, its
  // parameters and its element pairs, up to the next label.
  std::vector<std::vector<std::string>> lines;
  for (const auto &t : types) {
    if (lines.empty() || t.find(prefix) == 0) lines.emplace_back();
    lines.back().push_back(t);
  }

  // The descriptors of the line, for a message: "2 descriptors (D2_BP,
  // D2_BP)".
  std::string named = std::to_string(lines.size()) +
                      (lines.size() == 1 ? " descriptor (" : " descriptors (");
  for (std::size_t i = 0; i < lines.size(); ++i)
    named += (i ? ", " : "") + lines[i][0];
  named += ")";
  auto values = [](size_t v) {
    return std::to_string(v) + (v == 1 ? " value" : " values");
  };

  // A potential (WEIGHTS) is read as it was written. tadah 1.5.0 kept
  // RCTYPE, RCUT, AMPGRIDMB and KDEG values that no descriptor takes in the
  // potentials it wrote, and its descriptors read the same values as here:
  // each such key is reported once (on standard error, or to the capture of
  // pair_style tadah) and the potential loads. A configuration with them is
  // refused: the values would be dropped from the model without a word.
  const bool potential = c.has_stored("WEIGHTS");
  // The values of a key are the potential's own when no line outside a
  // potential's own lines gave one of them, nor a descriptor of the line
  // that reads them (core::Context::key_lines(): the lines of a file that
  // includes the potential with CONFIG, above or below the CONFIG line;
  // values the program set have no lines). A key with such a line is read
  // as in a configuration: its values are counted with the potential's, and
  // those the descriptors do not take are refused.
  auto own_lines = [&](const std::string &key) {
    for (const auto &line : c.key_lines(key))
      if (!line.potential) return false;
    return true;
  };
  const bool own_descriptors = potential && own_lines(type_str);
  auto potentials = [&](const std::string &key) {
    return own_descriptors && own_lines(key);
  };
  // ", set on line 1 of run.train and on line 20 of pot.tadah (CONFIG on
  // line 2 of run.train)": the lines that gave the values of @p key, for a
  // refusal of their count; empty for values the program set.
  auto set_on = [&](const std::string &key) {
    const std::vector<tadah::core::KeyLine> at = c.key_lines(key);
    std::string out;
    for (std::size_t i = 0; i < at.size(); ++i) {
      const char *sep =
          i == 0 ? ", set on " : i + 1 == at.size() ? " and on " : ", on ";
      out += sep + at[i].where;
    }
    return out;
  };
  // " (TYPE2B on line 4 of run.train)", every line of the TYPE key: where a
  // refusal of a descriptor of the key's lines is (they cannot be told
  // apart: a line may hold several descriptors), the end of every refusal
  // below, a count too; empty for values the program set.
  const std::string type_on = [&] {
    const std::vector<tadah::core::KeyLine> at = c.key_lines(type_str);
    if (at.empty()) return std::string();
    std::string out = " (" + type_str + " on ";
    for (std::size_t i = 0; i < at.size(); ++i)
      out += (i == 0 ? "" : i + 1 == at.size() ? " and on " : ", on ") +
             at[i].where;
    return out + ")";
  }();
  const bool joined =
      c.get<std::string>(type_str, 0).find("mJoin") != std::string::npos;
  const std::string label0 = lines.empty() ? "" : lines[0][0];
  // "TYPE2B D2_Blip takes" or "TYPE2B names 2 descriptors (...), which take"
  const std::string who =
      joined ? type_str + " names " + named +
                   (lines.size() == 1 ? ", which takes" : ", which take")
             : type_str + " " + label0 + " takes";
  auto report_unread = [&](const std::string &key,
                           const std::vector<std::string> &v,
                           std::size_t taken) {
    const std::size_t n = v.size() - taken;
    const std::string without =
        "; the potential works unchanged without " +
        std::string(n == 1 || taken == 0 ? "it" : "them");
    if (taken == 0) {
      tadah::core::report_ignored_key_once(
          key, "Key " + key + " (" + joined_text(v, 0, v.size()) + ")",
          who + " none of its values" + without);
      return;
    }
    tadah::core::report_ignored_key_once(
        key, "Part of " + key + " (" + joined_text(v, taken, v.size()) + ")",
        who + " its first " +
            (taken == 1 ? std::string("value") : values(taken)) + without);
  };

  // One RCTYPE and one RCUT value per descriptor, in the order of the lines:
  // fewer would be read past their end, more would never be read.
  for (const std::string &key : {rctypekey, rcutkey}) {
    const std::vector<std::string> &v = *data_map[key].first;
    const std::size_t have = v.size();
    if (have == lines.size()) continue;
    if (potentials(key) && have > lines.size() && !lines.empty()) {
      if (joined || key == rctypekey) {
        report_unread(key, v, lines.size());
        continue;
      }
      // A single descriptor's cutoff is the largest RCUT value (RCUT2BMAX,
      // RCUTMBMAX): the other values are the ones it does not read.
      std::size_t largest = 0;
      for (std::size_t i = 1; i < have; ++i)
        if (c.get<double>(key, i) > c.get<double>(key, largest)) largest = i;
      std::vector<std::string> rest;
      for (std::size_t i = 0; i < have; ++i)
        if (i != largest) rest.push_back(v[i]);
      tadah::core::report_ignored_key_once(
          key, "Part of " + key + " (" + joined_text(rest, 0, rest.size()) +
                   ")",
          who + " one " + key + " value, and its cutoff is the largest, " +
              v[largest] + "; the potential works unchanged with " + key +
              " " + v[largest] + " alone");
      continue;
    }
    throw std::runtime_error(
        type_str + " names " + named + " but " + key + " has " +
        values(have) + set_on(key) + ": each descriptor takes one " + key +
        " value, in the order of the " + type_str + " lines" + type_on);
  }

  std::vector<tadah::core::Context> contexts;
  // The list keys some descriptor of the line reads (SIZE 0 included), with
  // the values they take, for the check of leftover values below.
  std::map<std::string, size_t> read_keys;
  std::string last_text;
  for (size_t li = 0; li < lines.size(); ++li) {
    const auto &line = lines[li];
    const std::string &token = line[0];
    std::unique_ptr<D_Base> d(tadah::core::factory<D_Base>(token));
    // The line has the descriptor's parameters and its SIZE parameters are
    // integers (else an error naming the line), so they can be read below.
    std::vector<std::string> elements;
    try {
      elements = d->element_tokens(type_str, line);
    } catch (const std::runtime_error &e) {
      throw std::runtime_error(e.what() + type_on);
    }
    std::string text = type_str;
    for (const auto &t : line) text += " " + t;

    std::vector<std::string> all_list_keys = d->keys;
    all_list_keys.insert(all_list_keys.end(), d->data_keys.begin(),
                         d->data_keys.end());
    const size_t fparam = d->nparams - all_list_keys.size();
    std::vector<size_t> sizes;
    for (size_t k = 0; k < all_list_keys.size(); ++k) {
      const std::string &size_token = line[1 + fparam + k];
      try {
        sizes.push_back(std::stoul(size_token));
      } catch (const std::out_of_range &) {
        throw std::runtime_error(
            text + ": parameter " + std::to_string(fparam + k + 1) +
            ", the number of " + all_list_keys[k] + " values, is too large: " +
            size_token + type_on);
      }
    }

    // Every list key whose SIZE asks for values must be set.
    std::string takes, missing;
    size_t nmissing = 0;
    for (size_t k = 0; k < all_list_keys.size(); ++k) {
      const std::string &key = all_list_keys[k];
      if (sizes[k] == 0 || data_map.find(key) != data_map.end()) continue;
      ++nmissing;
      takes += (takes.empty() ? "" : " and ") + std::to_string(sizes[k]) +
               " " + key + (sizes[k] == 1 ? " value" : " values");
      missing += (missing.empty() ? "" : " and ") + key;
    }
    if (nmissing) {
      throw std::runtime_error(
          type_str + " " + token + " takes " + takes + ", but " + missing +
          (nmissing == 1 ? " is" : " are") + " not set: " + text + type_on);
    }

    tadah::core::Context c1 = c;
    c1.remove(type_str); c1.remove(rctypekey); c1.remove(rcutkey);
    c1.remove(cgridkey); c1.remove(sgridkey);
    c1.remove("CEMBFUNC"); c1.remove("SEMBFUNC");
    c1.remove("AMPGRIDMB");
    c1.remove("KDEG2B"); c1.remove("KDEGMB"); c1.remove("KDEGEMB");
    // The list keys the descriptor reads whole (a DM_Poly channel
    // function's CEMBFUNC/SEMBFUNC): a single line keeps them whole,
    // expanded; a joined line gives each descriptor only its share of a
    // list key, so it gives such a descriptor none, and one that cannot
    // run without them cannot be joined.
    if (!d->context_keys.empty()) {
      if (joined && d->context_keys_required) {
        std::string read;
        for (std::size_t k = 0; k < d->context_keys.size(); ++k)
          read += (k == 0 ? "" : k + 1 == d->context_keys.size() ? " and "
                                                                  : ", ") +
                  d->context_keys[k];
        throw std::runtime_error(
            type_str + " " + token + " (descriptor " + std::to_string(li + 1) +
            " of " + std::to_string(lines.size()) + ") reads " + read +
            " whole and cannot be joined: a " +
            c.get<std::string>(type_str, 0) + " line gives each descriptor "
            "only its share of a list key; " + token + " can be the only " +
            type_str + " descriptor" + type_on);
      }
      if (!joined)
        for (const auto &key : d->context_keys) {
          const auto it = data_map.find(key);
          if (it == data_map.end()) continue;
          for (const auto &v : *it->second.first) c1.add(key, v);
          read_keys[key] = it->second.first->size();
        }
    }
    c1.add(type_str,token);
    c1.add(rctypekey, (*data_map[rctypekey].first)[ data_map[rctypekey].second++ ]);
    c1.add(rcutkey, (*data_map[rcutkey].first)[ data_map[rcutkey].second++ ]);
    for (size_t j=0; j<fparam; ++j) {
      c1.add(type_str, line[1 + j]);
    }
    for (size_t k = 0; k < all_list_keys.size(); ++k) {
      const std::string &key = all_list_keys[k];
      const size_t n = sizes[k];
      // The key (set when n > 0, checked above) must hold n more values:
      // those the descriptors before this one on the line took are theirs.
      read_keys[key] += n;
      const size_t given = n > 0 ? data_map.at(key).first->size() : 0;
      const size_t taken = n > 0 ? data_map.at(key).second : 0;
      if (n > 0 && taken + n > given) {
        std::string where = type_str + " " + token;
        if (lines.size() > 1)
          where += " (descriptor " + std::to_string(li + 1) + " of " +
                   std::to_string(lines.size()) + ")";
        const std::string has =
            taken == 0 ? values(given)
                       : values(given - taken) + " left of " +
                             std::to_string(given) +
                             " (the descriptors before it take " +
                             std::to_string(taken) + ")";
        throw std::runtime_error(where + " takes " + std::to_string(n) + " " +
                                 key + (n == 1 ? " value" : " values") +
                                 ", but " + key + " has " + has +
                                 set_on(key) + ": " + text + type_on);
      }
      c1.add(type_str, n);
      for (size_t j = 0; j < n; ++j) {
        c1.add(key, (*data_map[key].first)[ data_map[key].second++ ]);
      }
    }

    // The element pairs (a configuration file's parser refuses a line
    // without them first: the TYPE key's schema).
    std::string which = type_str + " " + token;
    if (lines.size() > 1)
      which += " (descriptor " + std::to_string(li + 1) + " of " +
               std::to_string(lines.size()) + ")";
    if (elements.empty())
      throw std::runtime_error(
          which + ": the line names no element types; a " + type_str +
          " line ends with its element pairs, such as Ta Ta" + type_on);
    if (elements.size() % 2 != 0) {
      std::string named_types;
      for (const auto &e : elements)
        named_types += (named_types.empty() ? "" : " ") + e;
      throw std::runtime_error(
          which + ": the element types come in pairs, and the line has " +
          std::to_string(elements.size()) + " of them, " + named_types +
          type_on);
    }
    for (const auto &e : elements) c1.add(type_str, e);

    contexts.push_back(c1);
    last_text = text;
  }

  // Every value of a list key of the line's type belongs to a descriptor:
  // values past those the descriptors take, and the values of a key no
  // descriptor of the line reads, would be dropped from the model and the
  // potential without a word. In a potential, a joined line's descriptors
  // take theirs in order, and a single descriptor reads none of a key it
  // does not declare: the others are reported. A single descriptor reads
  // every value of the keys it declares, so their count is checked as in a
  // configuration.
  std::set<std::string> declared;
  for (const auto &[key, taken] : read_keys) declared.insert(key);
  for (const auto &key : list_keys_of(type_str))
    if (data_map.count(key)) read_keys.emplace(key, 0);
  for (const auto &[key, taken] : read_keys) {
    const auto it = data_map.find(key);
    const size_t given = it == data_map.end() ? 0 : it->second.first->size();
    if (given <= taken) continue;
    if (potentials(key) && (joined || !declared.count(key))) {
      report_unread(key, *it->second.first, taken);
      continue;
    }
    const std::string takes = std::to_string(taken) + " " + key +
                              (taken == 1 ? " value" : " values");
    if (lines.size() == 1)
      throw std::runtime_error(type_str + " " + lines[0][0] + " takes " +
                               takes + ", but " + key + " has " +
                               values(given) + set_on(key) + ": " +
                               last_text + type_on);
    throw std::runtime_error(
        type_str + " names " + named + ", which take " + takes + ", but " +
        key + " has " + values(given) + set_on(key) +
        ": each descriptor takes its " + key + " values, in the order of the " +
        type_str + " lines" + type_on);
  }
  return contexts;
}

} // namespace models
} // namespace tadah

