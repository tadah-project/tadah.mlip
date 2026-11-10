#include <tadah/models/descriptors/line_pairs.h>

#include <algorithm>
#include <stdexcept>

namespace tadah {
namespace models {

namespace {

/* Whether the token @p t of a line names the element @p e ("*": every
 * element). */
bool matches(const std::string &t, const std::string &e) {
  return t == "*" || t == e;
}

}  // namespace

LinePairs::LinePairs(const std::vector<std::string> &elements) {
  if (elements.size() % 2 != 0) {
    std::string named;
    for (const auto &e : elements) named += (named.empty() ? "" : " ") + e;
    throw std::invalid_argument(
        "the element types of a descriptor's line come in pairs, and the "
        "line has " + std::to_string(elements.size()) + " of them, " + named);
  }
  for (std::size_t i = 0; i + 1 < elements.size(); i += 2)
    pairs_.emplace_back(elements[i], elements[i + 1]);
}

bool LinePairs::names(const std::string &a, const std::string &b) const {
  for (const auto &p : pairs_)
    if ((matches(p.first, a) && matches(p.second, b)) ||
        (matches(p.first, b) && matches(p.second, a)))
      return true;
  return false;
}

bool LinePairs::every() const {
  for (const auto &p : pairs_)
    if (p.first == "*" && p.second == "*") return true;
  return false;
}

std::string LinePairs::text() const {
  if (every()) return {};
  std::vector<std::string> named;
  for (const auto &p : pairs_) {
    const std::string t = "(" + p.first + ", " + p.second + ")";
    if (std::find(named.begin(), named.end(), t) == named.end())
      named.push_back(t);
  }
  std::string out;
  for (std::size_t i = 0; i < named.size(); ++i)
    out += (i == 0 ? "" : i + 1 == named.size() ? " or " : ", ") + named[i];
  return out;
}

tadah::core::stats::DataStats
LinePairs::stats(const tadah::core::stats::DataStats &d) const {
  tadah::core::stats::DataStats out = d;
  out.per_pair.clear();
  for (const auto &pp : d.per_pair)
    if (names(pp.elements.first, pp.elements.second))
      out.per_pair.push_back(pp);
  return out;
}

}  // namespace models
}  // namespace tadah
