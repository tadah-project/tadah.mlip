#include <tadah/core/retired_keys.h>

#include <algorithm>
#include <stdexcept>

namespace tadah {
namespace core {

namespace {

/* "line 2 of old.tadah", or "line 2" for a line without a file. */
std::string where(const RetiredKeyLine &l) {
  std::string out = "line " + std::to_string(l.line);
  if (!l.source.empty()) out += " of " + l.source;
  return out;
}

/* "a", "a and b", "a, b and c". */
std::string join_and(const std::vector<std::string> &items) {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) out += i + 1 == items.size() ? " and " : ", ";
    out += items[i];
  }
  return out;
}

/* The text with its whitespace runs (line breaks of the blueprint's
 * multi-line strings included) made single blanks, without blanks around
 * it. */
std::string one_line(const std::string &text) {
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
  return out;
}

/* Whether @p context is M_KRR implementation 2 (MODEL M_KRR <kernel> 2,
 * the third value read as MLIP's is_krr_implementation_2 reads it), whose
 * solve in Tadah! 1.0.0 to 1.5.0, (K + LAMBDA I) w = t on the energies per
 * atom in eV, read none of the retired keys: no weight and no priority of
 * the data. false without a context. */
bool krr_implementation_2(const Context *context) {
  if (!context || !context->has_stored("MODEL") ||
      context->size("MODEL") < 3)
    return false;
  try {
    return context->get<std::string>("MODEL", 0) == "M_KRR" &&
           context->get<int>("MODEL", 2) == 2;
  } catch (const std::exception &) {
    return false;  // not an integer: the fit refuses the MODEL line
  }
}

const BlueprintKey &retired_entry(const std::string &key) {
  const auto &keys = BlueprintParser::getKeysMapRef();
  const auto it = keys.find(key);
  if (it == keys.end() || !it->second.isRetired())
    throw std::logic_error("retired_keys: " + key + " is not a retired key");
  return it->second;
}

/* "; FORCE false here: converted by deleting the line" for a line whose
 * conversion is its deletion: every line under M_KRR implementation 2
 * (@p krr2), every line of a key that nothing replaces (the field
 * by_deletion of `retired`: "; converted by deleting the line"), FWEIGHT
 * and SWEIGHT of a quantity that @p context leaves out (a fit refuses an
 * FSIGMA or SSIGMA other than its default then), and WDBFILE_AUTO 0; ""
 * for another line or without a context (the switches are then
 * unknown). */
std::string deleted_by(const RetiredKeyLine &l, const Context *context,
                       bool krr2) {
  const std::string tail = "converted by deleting the line";
  if (krr2) return "; M_KRR implementation 2 here: " + tail;
  if (retired_entry(l.key).retired.by_deletion) return "; " + tail;
  if (l.key == "WDBFILE_AUTO" && l.values.size() == 1) {
    try {
      if (std::stod(l.values.front()) == 0.0) return "; 0: " + tail;
    } catch (const std::exception &) {
      // not a number: the parse refused it before
    }
    return "";
  }
  if (!context) return "";
  if (l.key == "FWEIGHT" && !context->get<bool>("FORCE"))
    return "; FORCE false here: " + tail;
  if (l.key == "SWEIGHT" && !context->get<bool>("STRESS"))
    return "; STRESS false here: " + tail;
  return "";
}

/* The line @p l as a refusal names it: its key and values ("EWEIGHT
 * 1.0"); a line of more than six values (the WCOV of a potential: N, then
 * N x N values) by its first three and the number of the others ("WCOV 28
 * 1.2e-05 3e-07 and 782 more values"). */
std::string line_text(const RetiredKeyLine &l) {
  constexpr std::size_t kWhole = 6, kShown = 3;
  std::string text = l.key;
  const std::size_t n = l.values.size();
  for (std::size_t i = 0; i < n && (n <= kWhole || i < kShown); ++i)
    text += " " + l.values[i];
  if (n > kWhole)
    text += " and " + std::to_string(n - kShown) + " more values";
  return text;
}

}  // namespace

std::string retired_key_text(const std::string &key, const BlueprintKey &info) {
  return key + " is a retired key: " + one_line(info.retired.reason) +
         " Replaced by: " + one_line(info.retired.replacement) + ". " +
         one_line(info.retired.conversion);
}

std::string retired_keys_notice(const std::vector<RetiredKeyLine> &lines) {
  std::vector<std::string> named;
  std::vector<std::string> keys;
  for (const auto &l : lines) {
    named.push_back(l.key + " (" + where(l) + ")");
    if (std::find(keys.begin(), keys.end(), l.key) == keys.end())
      keys.push_back(l.key);
  }
  const bool one = lines.size() == 1;
  std::vector<std::string> explain;
  for (const auto &k : keys) explain.push_back("tadah explain " + k);
  return std::string(one ? "Retired key " : "Retired keys ") +
         join_and(named) +
         (one ? " is ignored by a prediction, as it changes nothing it gives, "
                "and refused by a fit ("
              : " are ignored by a prediction, as they change nothing it "
                "gives, and refused by a fit (") +
         join_and(explain) +
         (keys.size() == 1 ? " gives the reason and the conversion)."
                           : " give the reasons and the conversions).");
}

std::string retired_keys_refusal(const std::vector<RetiredKeyLine> &lines,
                                 const std::string &who,
                                 const Context *context) {
  const bool krr2 = krr_implementation_2(context);
  const bool one = lines.size() == 1;
  std::vector<std::string> named;
  std::vector<std::string> keys;
  std::size_t marked = 0;       // the lines a deletion converts
  std::size_t unconverted = 0;  // the lines no key converts
  std::vector<std::string> unconverted_keys;
  for (const auto &l : lines) {
    std::string mark = deleted_by(l, context, krr2);
    if (!mark.empty()) {
      ++marked;
    } else if (retired_entry(l.key).retired.unconverted) {
      // Deleting the line changes the fit: it is neither converted by a
      // key nor by its deletion (the field unconverted of `retired`).
      mark = "; no key converts it";
      ++unconverted;
      if (std::find(unconverted_keys.begin(), unconverted_keys.end(),
                    l.key) == unconverted_keys.end())
        unconverted_keys.push_back(l.key);
    }
    named.push_back(line_text(l) + " (" + where(l) + mark + ")");
    if (std::find(keys.begin(), keys.end(), l.key) == keys.end())
      keys.push_back(l.key);
  }
  const std::string lines_here = one ? "this line" : "these lines";
  const std::string file_of_lines =
      std::string(" (in the file that has ") + (one ? "it" : "them") +
      ", a potential's included";
  std::string msg = who + ": the configuration has " +
                    (one ? "a key" : "keys") +
                    " retired in Tadah! 1.6.0, and a fit refuses " +
                    (one ? "it" : "them") + ": " + join_and(named) + ".";
  if (krr2) {
    // The keys' own conversions are those of the other fits: under
    // implementation 2 they would change the fit.
    msg += std::string(" M_KRR implementation 2 of Tadah! 1.0.0 to 1.5.0 ") +
           (one ? "did not read it" : "read none of them") +
           ": it solved (K + LAMBDA I) w = t on the energies per atom in eV "
           "(ESIGMA 1000). With " + lines_here + " deleted" + file_of_lines +
           ") and ESIGMA 1000, or LAMBDA times 10^6 with ESIGMA 1, the "
           "configuration fits with the solve of Tadah! 1.5.0; no "
           "coefficients were fitted.";
    return msg;
  }
  for (const auto &k : keys) {
    const BlueprintKey &info = retired_entry(k);
    // A key that nothing replaces, or that no key converts: its
    // conversion, the deletion, does not say why, so its reason does.
    msg += " " + k + ": " +
           (info.retired.by_deletion || info.retired.unconverted
                ? one_line(info.retired.reason) + " "
                : std::string()) +
           one_line(info.retired.conversion);
  }
  // A retrain does not read a potential's WDBFILE line, so the WDBFILE
  // values of a potential's WDBFILE_AUTO go elsewhere (its conversion).
  const bool wdbfile_auto =
      std::find(keys.begin(), keys.end(), "WDBFILE_AUTO") != keys.end();
  // What to do with the lines: each marked line deleted, each line no key
  // converts deleted, the converted keys in place of the others.
  const std::string unconverted_lines =
      unconverted == 0 ? std::string()
                       : "the " + join_and(unconverted_keys) +
                             (unconverted == 1 ? " line" : " lines");
  std::string what_to_do;
  if (marked + unconverted == lines.size()) {
    what_to_do = " With " + lines_here + " deleted";
  } else {
    what_to_do = " With the converted keys in place of " +
                 (unconverted > 0 ? std::string("the others") : lines_here) +
                 (marked > 0 ? ", or the marked lines deleted" : "");
    if (unconverted > 0)
      what_to_do += std::string(marked > 0 ? "," : "") + " and " +
                    unconverted_lines + " deleted";
  }
  msg += what_to_do + file_of_lines +
         (wdbfile_auto ? "; the WDBFILE values of a potential on the command "
                         "line or in a file that includes it"
                       : "") +
         "), the configuration trains" +
         (unconverted > 0 ? ", and no key reproduces " + unconverted_lines
                          : std::string()) +
         "; no coefficients were fitted.";
  return msg;
}

void require_no_retired_keys(const Context &context, const std::string &who) {
  const std::vector<RetiredKeyLine> lines = context.retired_keys();
  if (!lines.empty())
    throw std::runtime_error(retired_keys_refusal(lines, who, &context));
}

}  // namespace core
}  // namespace tadah
