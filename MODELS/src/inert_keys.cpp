#include <tadah/models/inert_keys.h>

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/models/descriptors/d_base.h>
#include <tadah/models/descriptors/dm/embf0.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>

namespace tadah {
namespace models {

namespace {

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char ch) { return std::toupper(ch); });
  return s;
}

/* The text with its whitespace runs made single blanks, without blanks
 * around it and without a full stop at its end. */
std::string clause(const std::string &text) {
  std::string out;
  bool blank = false;
  for (char ch : text) {
    if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r') {
      blank = !out.empty();
      continue;
    }
    if (blank) out += ' ';
    blank = false;
    out += ch;
  }
  while (!out.empty() && out.back() == '.') out.pop_back();
  return out;
}

/* The stored values of @p key, in order. */
std::vector<std::string> stored_tokens(const core::Context &c,
                                       const std::string &key) {
  std::vector<std::string> out;
  for (std::size_t n = 0;; ++n) {
    const std::optional<std::string> t = c.stored_value(key, n);
    if (!t) break;
    out.push_back(*t);
  }
  return out;
}

/* "KEY v1 v2 ...", the values as stored; a line of more than six values
 * by its first three and the number of the others ("SIGMA 3 5.48e-09
 * 3.2e-09 and 7 more values"), as a refusal of a retired key names one. */
std::string key_text(const std::string &key,
                     const std::vector<std::string> &values) {
  constexpr std::size_t kWhole = 6, kShown = 3;
  const std::size_t n = values.size();
  std::string t = key;
  for (std::size_t i = 0; i < n && (n <= kWhole || i < kShown); ++i)
    t += " " + values[i];
  if (n > kWhole) t += " and " + std::to_string(n - kShown) + " more values";
  return t;
}

/* Where the lines that gave @p key its values are: "line 3 of run.train"
 * (several: "line 3 of a.train and line 5 of b.train"); "" when no line
 * gave them. */
std::string where_of(const core::Context &c, const std::string &key) {
  std::string out;
  for (const core::KeyLine &l : c.key_lines(key))
    out += (out.empty() ? "" : " and ") + l.where;
  return out;
}

/* An INIT key: its stored value, else its blueprint default. */
bool init_flag(const core::Context &c, const std::string &key) {
  std::string v;
  if (const auto s = c.stored_value(key)) {
    v = *s;
  } else {
    const auto &keys = core::BlueprintParser::getKeysMapRef();
    const auto it = keys.find(key);
    if (it != keys.end() && !it->second.default_value.empty())
      v = it->second.default_value.front();
  }
  return upper(v) == "TRUE";
}

/* The descriptor labels the initialised TYPE2B and TYPEMB lines of @p c
 * name (the descriptors of an mJoin line each, without the mJoin label),
 * and "@" for a component. DESCRIPTOR blocks without a TYPEMB line are the
 * many-body term's components, one "@" each, and switch the term on
 * without an INITMB line (resolve_embed_blocks, which refuses them under
 * INITMB false). */
std::vector<std::string> descriptor_labels(const core::Context &c) {
  std::vector<std::string> out;
  const std::size_t blocks = c.blocks("DESCRIPTOR").size();
  for (const auto &pair : {std::make_pair(std::string("INIT2B"),
                                          std::string("TYPE2B")),
                           std::make_pair(std::string("INITMB"),
                                          std::string("TYPEMB"))}) {
    const bool by_blocks = pair.first == "INITMB" && blocks > 0;
    if (!init_flag(c, pair.first) && !(by_blocks && !c.has_stored("INITMB")))
      continue;
    if (by_blocks && !c.has_stored("TYPEMB")) {
      out.insert(out.end(), blocks, "@");
      continue;
    }
    for (const std::string &t : stored_tokens(c, pair.second)) {
      if (!t.empty() && t[0] == '@') {
        out.push_back("@");
      } else if (t.rfind("D2_", 0) == 0 || t.rfind("DM_", 0) == 0) {
        if (t != "D2_mJoin" && t != "DM_mJoin") out.push_back(t);
      }
    }
  }
  return out;
}

/* The descriptors whose D_Base::reads_watom() is false, by label, for a
 * configuration whose descriptors are not built (a test pins the two
 * against each other for every registered descriptor). */
bool reads_no_watom(const std::string &label) {
  return label == "D2_EAM" || label == "DM_EAM" || label == "D2_Dummy" ||
         label == "DM_Dummy";
}

/* Why WATOM changes nothing in @p c (whose descriptors read none). */
std::string watom_reason(const core::Context &c, const std::string &what) {
  const std::vector<std::string> labels = descriptor_labels(c);
  const bool eam = std::any_of(labels.begin(), labels.end(),
                               [](const std::string &l) {
                                 return l == "D2_EAM" || l == "DM_EAM";
                               });
  return "no descriptor of the " + what + " reads it" +
         (eam ? std::string(" (D2_EAM and DM_EAM take their pair and density "
                            "functions from the SETFL file, unweighted)")
              : std::string());
}

}  // namespace

bool context_reads_watom(const core::Context &c) {
  for (const std::string &l : descriptor_labels(c))
    if (l == "@" || !reads_no_watom(l)) return true;
  return false;
}

bool watom_sets_a_weight(const core::Context &c) {
  const std::vector<std::string> t = stored_tokens(c, "WATOM");
  if (t.empty()) return false;
  const bool per_species = D_Base::watom_number_shaped(t.front());
  for (std::size_t i = 0; i < t.size(); ++i) {
    if (!per_species && i % 3 != 2) continue;  // a pair's element symbols
    try {
      if (D_Base::watom_value(t[i], i + 1) != 1.0) return true;
    } catch (const std::exception &) {
      return true;  // refused where WATOM is read
    }
  }
  return false;
}

std::vector<InertKeyLine> inert_key_lines(const core::Context &c,
                                          const InertKeyRules &rules) {
  std::vector<InertKeyLine> out;
  const auto &keys = core::BlueprintParser::getKeysMapRef();
  for (const core::RetiredKeyLine &l : c.deprecated_keys()) {
    std::string where = "line " + std::to_string(l.line);
    if (!l.source.empty()) where += " of " + l.source;
    const auto it = keys.find(l.key);
    out.push_back(InertKeyLine{
        l.key, key_text(l.key, l.values), where,
        it == keys.end() ? std::string("a deprecated key")
                         : clause(it->second.deprecated)});
  }
  if (rules.ream_keys && !context_has_ream(c)) {
    for (const char *key : {"EMBF0", "RHOFORM"}) {
      if (!c.has_stored(key)) continue;
      out.push_back(InertKeyLine{
          key, key_text(key, stored_tokens(c, key)), where_of(c, key),
          "it applies only to a DM_REAM many-body block, and the "
          "configuration has none"});
    }
  }
  if (watom_sets_a_weight(c) &&
      (!rules.watom_never_read.empty() || !context_reads_watom(c)))
    out.push_back(InertKeyLine{
        "WATOM", key_text("WATOM", stored_tokens(c, "WATOM")),
        where_of(c, "WATOM"),
        rules.watom_never_read.empty() ? watom_reason(c, "configuration")
                                       : rules.watom_never_read});
  return out;
}

std::string inert_keys_text(const std::vector<InertKeyLine> &lines,
                            bool also) {
  const bool one = lines.size() == 1;
  const std::string has = also ? "also has " : "has ";
  std::string out =
      "the configuration " + has +
      (one ? "a line that changes nothing a fit gives, and a fit refuses it: "
           : "lines that change nothing a fit gives, and a fit refuses "
             "them: ");
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const InertKeyLine &l = lines[i];
    out += (i == 0 ? "" : "; ") + l.text +
           (l.where.empty() ? std::string() : " (" + l.where + ")") + ": " +
           l.reason;
  }
  if (also)
    out += one ? ". With this line deleted as well, the configuration trains."
               : ". With these lines deleted as well, the configuration "
                 "trains.";
  else
    out += one ? ". With this line deleted, the fit is the same."
               : ". With these lines deleted, the fit is the same.";
  return out;
}

std::string inert_keys_refusal(const std::vector<InertKeyLine> &lines,
                               const std::string &who) {
  return who + ": " + inert_keys_text(lines, false);
}

void require_no_inert_keys(const core::Context &c, const std::string &who,
                           const InertKeyRules &rules) {
  const std::vector<InertKeyLine> lines = inert_key_lines(c, rules);
  if (!lines.empty())
    throw std::runtime_error(inert_keys_refusal(lines, who));
}

void report_unread_watom(const core::Context &c) {
  if (!watom_sets_a_weight(c) || context_reads_watom(c)) return;
  std::string values;
  for (const std::string &v : stored_tokens(c, "WATOM"))
    values += (values.empty() ? "" : " ") + v;
  core::report_ignored_key_once(
      "WATOM", "Key WATOM (" + values + ")",
      watom_reason(c, "potential") +
          "; the potential works unchanged without it");
}

}  // namespace models
}  // namespace tadah
