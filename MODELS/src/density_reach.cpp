#include <tadah/models/descriptors/density_reach.h>
#include <tadah/core/number_format.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tadah {
namespace models {

namespace {

using tadah::core::stats::DataStats;

/* @p v with up to 6 significant digits ("2.85788", "0.2", "30"): a
 * statistic of the data or a distance derived from it. */
std::string num(double v) {
  std::ostringstream os;
  os << v;
  return os.str();
}

/* A value of the seed as its shortest round-trip text ("5.61234567"). */
std::string seed_num(double v) { return tadah::core::format_roundtrip(v); }

/* The element pair of @p pp as a rationale names it ("(Ta, Ta)", as the
 * checks name a pair: LinePairs::text()): a range taken from the shells,
 * peaks or percentiles of the distances is taken from those of the first
 * element pair of the statistics (DataStats sorts per_pair by element
 * pair), which tadah analysis suggest_bounds gives one element pair of the
 * descriptor's line at a time. */
std::string pair_name(const tadah::core::stats::PerPairStats &pp) {
  return "(" + pp.elements.first + ", " + pp.elements.second + ")";
}

/* " (Ta, Ta)", " (Ta, Ta) or (W, W)": the element pairs of the line of the
 * descriptor of @p s (SeedView::line_pairs()) as a message names them
 * before "pair of atoms" or "neighbour distance", whose statistics the
 * caller gives; empty for a view without them, or a line that names every
 * pair. */
std::string of_pairs(const SeedView &s) {
  const LinePairs *p = s.line_pairs();
  if (p == nullptr) return {};
  const std::string t = p->text();
  return t.empty() ? t : " " + t;
}

/* An element pair of the line of the descriptor of a view as a check judges
 * it, on its own: its name as a message gives it before "pair of atoms" or
 * "neighbour distance" (" (Ta, Ta)"; empty for a view without the pairs of
 * its line) and the statistics of the element pairs of the data it names. */
struct JudgedPair {
  std::string name;
  DataStats stats;
};

/* The element pairs a check of the view @p s judges one at a time: each
 * element pair the line of @p s (SeedView::line_pairs()) names, with the
 * statistics of @p d of that pair (LinePairs::stats()). A pair of two
 * elements is judged as the line writes it ("W Ta": (W, Ta)); a pair with
 * "*" stands for each element pair it matches, of the elements of the data
 * (those @p d lists and those of its pairs, whether the statistics hold
 * distances of a pair or not) and those the line names ("Ta *" on Ta-W
 * data: (Ta, Ta) and (Ta, W); "* *": (Ta, Ta), (Ta, W) and (W, W); "Mo *"
 * there: (Mo, Mo), (Mo, Ta) and (Mo, W), as "Mo Mo Mo Ta Mo W"); a pair is
 * judged once, in the order the line first names it. A descriptor sums over the
 * neighbours of each of these pairs, so a pair that a cutoff, or every
 * channel of a density, does not reach adds nothing to it whatever the
 * other pairs give; the verdict is the same for each way of writing the
 * line. A view without the pairs of its line gives @p d as a whole, named
 * by no pair. */
std::vector<JudgedPair> judged_pairs(const DataStats &d, const SeedView &s) {
  const LinePairs *p = s.line_pairs();
  if (p == nullptr || p->pairs().empty()) return {JudgedPair{{}, d}};
  std::vector<std::string> elements = d.elements;
  for (const auto &pp : d.per_pair) {
    elements.push_back(pp.elements.first);
    elements.push_back(pp.elements.second);
  }
  for (const auto &pair : p->pairs())
    for (const std::string *e : {&pair.first, &pair.second})
      if (*e != "*") elements.push_back(*e);
  std::sort(elements.begin(), elements.end());
  elements.erase(std::unique(elements.begin(), elements.end()),
                 elements.end());
  std::vector<std::pair<std::string, std::string>> named;
  auto add = [&](const std::string &a, const std::string &b) {
    for (const auto &n : named)
      if ((n.first == a && n.second == b) || (n.first == b && n.second == a))
        return;
    named.emplace_back(a, b);
  };
  for (const auto &pair : p->pairs()) {
    if (pair.first != "*" && pair.second != "*") {
      add(pair.first, pair.second);
      continue;
    }
    for (const auto &a : elements)
      for (const auto &b : elements)
        if ((pair.first == "*" || pair.first == a) &&
            (pair.second == "*" || pair.second == b))
          add(a, b);
  }
  std::vector<JudgedPair> out;
  for (const auto &n : named) {
    const LinePairs one({n.first, n.second});
    out.push_back(JudgedPair{" " + one.text(), one.stats(d)});
  }
  return out;
}

/* @p items joined as a sentence lists them: "a", "a and b", "a, b and c"
 * (@p last " or ": "a or b"). */
std::string listed(const std::vector<std::string> &items,
                   const std::string &last = " and ") {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i)
    out += (i == 0 ? "" : i + 1 == items.size() ? last : ", ") + items[i];
  return out;
}

/* The pairs @p names (each " (Ta, Ta)", JudgedPair::name) as a message
 * names some of the pairs of a line: " (Ta, Ta)", " (Ta, Ta) or (Mo, Mo)"
 * (@p last " and ": " (Ta, Ta) and (Mo, Mo)"). */
std::string some_pairs(const std::vector<std::string> &names,
                       const std::string &last = " or ") {
  std::vector<std::string> bare;
  for (const auto &n : names)
    if (!n.empty()) bare.push_back(n.substr(1));
  return bare.empty() ? std::string() : " " + listed(bare, last);
}

/* "KEY(n)" for value @p i of @p key of @p s, n its 1-based number in the
 * configuration, as an OPTIM line names an entry (SeedView::entry_number:
 * the entries of a joined line's descriptor follow those of the
 * descriptors before it). */
std::string entry(const SeedView &s, const std::string &key, std::size_t i) {
  return key + "(" + std::to_string(s.entry_number(key, i)) + ")";
}

/* The seed's centre of entry @p arg_pos (-1: the first) of CGRIDMB, when
 * the seed has one. */
bool seed_centre(const SeedView &s, int arg_pos, double &c) {
  if (!s.contains("CGRIDMB")) return false;
  const std::size_t i = arg_pos < 0 ? 0 : static_cast<std::size_t>(arg_pos);
  if (i >= s.size("CGRIDMB")) return false;
  c = s.get<double>("CGRIDMB", i);
  return true;
}

/* The neighbour shell nearest to the distance @p c: the peak of the
 * distances of the first element pair of @p d nearest to it, its position
 * @p mu and its width @p w, the FWHM (0.2 times the position for a peak
 * without one); @p what names the width. False when the distances show no
 * peak. */
bool nearest_shell(const DataStats &d, double c, double &mu, double &w,
                   std::string &what) {
  const auto &peaks = d.per_pair[0].r_peaks;
  if (peaks.empty()) return false;
  const std::string pair = pair_name(d.per_pair[0]);
  const auto *best = &peaks.front();
  for (const auto &p : peaks)
    if (std::abs(p.position - c) < std::abs(best->position - c)) best = &p;
  mu = best->position;
  if (best->fwhm > 0.0) {
    w = best->fwhm;
    what = "the FWHM (" + num(w) + " Å) of the " + pair +
           " neighbour shell nearest to it (" + num(mu) + " Å)";
  } else {
    w = 0.2 * mu;
    what = num(w) + " Å, 0.2 times the distance of the " + pair +
           " neighbour shell nearest to it (" + num(mu) +
           " Å), which has no FWHM";
  }
  return true;
}

/* The first two neighbour shells of @p d: the first two peaks of the
 * distances of its first element pair, or, without them, the 10th
 * percentile and the median; @p what names them. */
void shells(const DataStats &d, double &r1, double &r2, std::string &what) {
  const auto &pp = d.per_pair[0];
  const std::string pair = pair_name(pp);
  if (pp.r_peaks.size() >= 2) {
    r1 = pp.r_peaks[0].position;
    r2 = pp.r_peaks[1].position;
    what = "from the first " + pair + " neighbour shell (" + num(r1) +
           " Å) to the second (" + num(r2) + " Å)";
  } else {
    r1 = pp.r_percentiles[tadah::core::stats::IDX_P10];
    r2 = pp.r_percentiles[tadah::core::stats::IDX_P50];
    what = "from the 10th percentile of the " + pair +
           " neighbour distances (" + num(r1) + " Å) to their median (" +
           num(r2) + " Å)";
  }
}

/* The radial form of a channel of @p kind for a rationale, about its centre
 * ("Gaussian exp(-SGRIDMB (r - CGRIDMB)^2)", "blip b(SGRIDMB (r -
 * CGRIDMB))") or, with @p origin, at the origin ("Gaussian exp(-SGRIDMB
 * r^2)", "blip b(SGRIDMB r)"). */
std::string form(BasisKind kind, bool origin) {
  if (kind == BasisKind::Gaussian)
    return origin ? "Gaussian exp(-SGRIDMB r^2)"
                  : "Gaussian exp(-SGRIDMB (r - CGRIDMB)^2)";
  return origin ? "blip b(SGRIDMB r)" : "blip b(SGRIDMB (r - CGRIDMB))";
}

/* The suggestion of the centre CGRIDMB(@p arg_pos) of a channel of @p kind
 * (gaussian_centre_suggestion, blip_centre_suggestion). */
BoundSuggestion centre_suggestion(const DataStats &d, const SeedView &s,
                                  int arg_pos, BasisKind kind) {
  if (d.per_pair.empty()) return no_pair_suggestion(s);
  double c = 0.0;
  const bool seeded = seed_centre(s, arg_pos, c);
  if (seeded && c == 0.0)
    return BoundSuggestion::no_row(
        form(kind, true) + " centred at the origin (the seed's CGRIDMB 0): "
        "its width SGRIDMB, not its centre, sets its reach");
  const auto &pp = d.per_pair[0];
  double mu = 0.0, w = 0.0;
  std::string width;
  if (seeded && nearest_shell(d, c, mu, w, width)) {
    // About the seed's centre, within the neighbour distances the channel
    // sums over, unless the seed's centre lies outside them: the range
    // always holds it.
    const double rcut = s.contains("RCUTMB") ? s.get<double>("RCUTMB") : 0.0;
    const NeighbourRange range = neighbour_range(d, rcut);
    return BoundSuggestion(
        std::max(c - w, std::min(c, range.lo)),
        std::min(c + w, std::max(c, range.hi)),
        form(kind, false) + " centred on the seed's centre (" + seed_num(c) +
            " Å): +- " + width + ", within the neighbour distances " +
            num(range.lo) + " to " + num(range.hi) +
            " Å unless the seed's centre lies outside them");
  }
  const bool gaussian = kind == BasisKind::Gaussian;
  if (!pp.r_peaks.empty()) {
    mu = pp.r_peaks[0].position;
    w = pp.r_peaks[0].fwhm > 0.0 ? pp.r_peaks[0].fwhm : 0.2 * mu;
    return BoundSuggestion(
        std::max(gaussian ? pp.r_min : 0.0, mu - w), mu + w,
        form(kind, false) + " centred on the first " + pair_name(pp) +
            " neighbour shell: its peak (" + num(mu) + " Å) +- " + num(w) +
            " Å, " +
            (gaussian ? "from the smallest " + pair_name(pp) +
                            " distance (" + num(pp.r_min) + " Å)"
                      : std::string("not below 0")));
  }
  using tadah::core::stats::IDX_P10;
  using tadah::core::stats::IDX_P90;
  return BoundSuggestion(
      pp.r_percentiles[IDX_P10], pp.r_percentiles[IDX_P90],
      form(kind, false) + " centred on the first " + pair_name(pp) +
          " neighbour shell: the 10th to 90th percentiles of the " +
          pair_name(pp) + " neighbour distances (no peak found)");
}

/* The suggestion of the width SGRIDMB(@p arg_pos) of a channel of @p kind
 * (gaussian_width_suggestion, blip_width_suggestion). */
BoundSuggestion width_suggestion(const DataStats &d, const SeedView &s,
                                 int arg_pos, BasisKind kind) {
  if (d.per_pair.empty()) return no_pair_suggestion(s);
  const bool gaussian = kind == BasisKind::Gaussian;
  const auto &pp = d.per_pair[0];
  double c = 0.0;
  const bool seeded = seed_centre(s, arg_pos, c);
  if (seeded && c == 0.0 && gaussian) {
    double r1 = 0.0, r2 = 0.0;
    std::string between;
    shells(d, r1, r2, between);
    const double gap = r2 * r2 - r1 * r1;
    if (!(gap > 0.0))
      return BoundSuggestion::no_row(
          "Gaussian exp(-SGRIDMB r^2) centred at the origin: the " +
          pair_name(pp) +
          " neighbour distances show no second shell beyond the first");
    const double lo = std::log(1.0 / 0.9) / gap;
    const double drop = std::log(20.0) / gap;
    const double reach = gaussian_reach_width(pp.r_min);
    const double hi = std::min(drop, reach);
    std::string why =
        "Gaussian exp(-SGRIDMB r^2) centred at the origin (the seed's "
        "CGRIDMB 0): its value drops by a factor of 0.9 to 1/20 " + between;
    if (reach < drop)
      why += ", up to " + num(reach) + ", where it is 1e-6 at the smallest " +
             pair_name(pp) + " distance (" + num(pp.r_min) + " Å)";
    if (!(lo < hi))
      return BoundSuggestion::no_row(
          why + ": the widths it allows do not reach the smallest " +
          pair_name(pp) + " distance");
    return BoundSuggestion(lo, hi, why);
  }
  if (seeded && c == 0.0) {
    // A blip at the origin reaches the distance x_f / SGRIDMB, where it is
    // kDensityReachFloor: from the nearest neighbour to the farthest one
    // the channel sums over.
    const bool has_cut = s.contains("RCUTMB");
    const double rcut = has_cut ? s.get<double>("RCUTMB") : 0.0;
    const NeighbourRange range = neighbour_range(d, rcut);
    const std::string origin =
        "blip b(SGRIDMB r) centred at the origin (the seed's CGRIDMB 0)";
    if (range.empty)
      return BoundSuggestion::no_row(
          origin + ": no neighbour distance lies within RCUTMB = " +
          seed_num(rcut) + " Å for it to reach");
    const std::string farthest =
        has_cut ? "the farthest neighbour distance within RCUTMB (" +
                      num(range.hi) + " Å)"
                : "the farthest neighbour distance (" + num(range.hi) + " Å)";
    const std::string why =
        origin + ": its reach, the distance at which it falls to 1e-6, from "
        "r_min (" + num(range.lo) + " Å) to " + farthest;
    const double lo = blip_reach_width(range.hi);
    const double hi = blip_reach_width(range.lo);
    if (!(lo < hi))
      return BoundSuggestion::no_row(
          why + ": the two are one distance, a range of zero width");
    return BoundSuggestion(lo, hi, why);
  }
  double mu = 0.0, wn = 0.0;
  std::string width;
  if (seeded && nearest_shell(d, c, mu, wn, width)) {
    if (gaussian)
      return BoundSuggestion(
          0.25 / (wn * wn), 9.0 / (wn * wn),
          form(kind, false) + " centred on the seed's centre (" + seed_num(c) +
              " Å): 0.25/w^2 to 9/w^2 with w " + width);
    return BoundSuggestion(
        1.0 / wn, 6.0 / wn,
        form(kind, false) + " centred on the seed's centre (" + seed_num(c) +
            " Å): its support half-width 2/SGRIDMB from 2 w down to w/3, "
            "with w " + width);
  }
  using tadah::core::stats::IDX_P10;
  using tadah::core::stats::IDX_P90;
  double w = pp.r_percentiles[IDX_P90] - pp.r_percentiles[IDX_P10];
  std::string what = "the spread from the 10th to the 90th percentile of "
                     "the " + pair_name(pp) + " neighbour distances";
  if (!pp.r_peaks.empty() && (!gaussian || pp.r_peaks[0].fwhm > 0.0)) {
    w = pp.r_peaks[0].fwhm;
    what = "the FWHM of the first " + pair_name(pp) + " neighbour shell";
  }
  if (!(w > 0.0))
    return BoundSuggestion::fixed_range(
        0.1, 1.0,
        "fixed range: the " + pair_name(pp) +
            " neighbour distances have no spread to take a width of the " +
            (gaussian ? "Gaussian" : "blip") + " from");
  if (gaussian)
    return BoundSuggestion(
        0.25 / (w * w), 9.0 / (w * w),
        form(kind, false) + " centred on the first " + pair_name(pp) +
            " neighbour shell: 0.25/w^2 to 9/w^2 with w = " + num(w) +
            " Å, " + what);
  return BoundSuggestion(
      1.0 / w, 6.0 / w,
      form(kind, false) + ": its support half-width 2/SGRIDMB from 2 w down "
      "to w/3, with w = " + num(w) + " Å, " + what);
}

}  // namespace

BoundSuggestion no_pair_suggestion(const SeedView &s) {
  return BoundSuggestion::no_row(
      "no" + of_pairs(s) + " pair of atoms of the data lies within the reach "
      "of the neighbour statistics (" + num(kCutoffSearchReach) +
      " times the largest cutoff of the seed): no distance to take a range "
      "from");
}

BoundSuggestion cutoff_suggestion(const DataStats &d, const SeedView &s,
                                  int) {
  if (d.per_pair.empty()) return no_pair_suggestion(s);
  using tadah::core::stats::IDX_P50;
  using tadah::core::stats::IDX_P99;
  const auto &pp = d.per_pair[0];
  const std::string pair = pair_name(pp);
  const double top = pp.r_percentiles[IDX_P99];
  const std::string up_to =
      "the 99th percentile of the " + pair + " neighbour distances (" +
      num(top) + " Å), which the statistics take up to " +
      num(kCutoffSearchReach) + " times the largest cutoff of the seed";
  if (pp.r_peaks.size() >= 2) {
    const double r2 = pp.r_peaks[1].position;
    if (1.05 * r2 < top)
      return BoundSuggestion(1.05 * r2, top,
                             "a cutoff past the second " + pair +
                                 " neighbour shell: from 1.05 times its "
                                 "distance (" + num(r2) + " Å) to " + up_to);
  }
  const double median = pp.r_percentiles[IDX_P50];
  return BoundSuggestion(median, top,
                         "a cutoff from the median of the " + pair +
                             " neighbour distances (" + num(median) +
                             " Å) to " + up_to);
}

HealthVerdict cutoff_reach_check(const DataStats &d, const SeedView &s,
                                 const std::string &key) {
  if (!s.contains(key) || d.n_atoms == 0) return HealthOk{};
  const double rc = s.get<double>(key);
  // No pair of the line within the cutoff: the line as a whole.
  const DataStats line = s.line_pairs() ? s.line_pairs()->stats(d) : d;
  const std::string all = of_pairs(s);
  if (line.per_pair.empty())
    return HealthFail{key + " = " + seed_num(rc) + ": no" + all +
                      " pair of atoms of the dataset lies within it, so the "
                      "descriptor sums over no neighbour and is 0 for every "
                      "atom."};
  const NeighbourRange whole = neighbour_range(line, rc);
  if (whole.empty)
    return HealthFail{key + " = " + seed_num(rc) + " is below the smallest" +
                      all + " neighbour distance of the dataset (" +
                      num(whole.lo) + " Å): the descriptor sums over no "
                      "neighbour and is 0 for every atom."};
  // Each element pair of the line on its own: the pairs whose smallest
  // neighbour distance lies past the cutoff, and those the statistics have
  // no distance of.
  std::vector<std::string> below, below_names, absent;
  for (const auto &p : judged_pairs(d, s)) {
    if (p.stats.per_pair.empty()) {
      absent.push_back(p.name);
      continue;
    }
    const NeighbourRange range = neighbour_range(p.stats, rc);
    if (!range.empty) continue;
    // "the smallest (Ta, Ta) neighbour distance of the dataset (2.86 Å)",
    // a further pair "the smallest (W, W) one (2.5 Å)".
    below.push_back("the smallest" + p.name +
                    (below.empty() ? " neighbour distance of the dataset"
                                   : " one") +
                    " (" + num(range.lo) + " Å)");
    below_names.push_back(p.name);
  }
  if (below.empty() && absent.empty()) return HealthOk{};
  std::vector<std::string> misses;
  if (!below.empty()) misses.push_back("is below " + listed(below));
  if (!absent.empty())
    misses.push_back("holds no" + some_pairs(absent) +
                     " pair of atoms of the dataset");
  std::vector<std::string> missed = below_names;
  missed.insert(missed.end(), absent.begin(), absent.end());
  return HealthFail{key + " = " + seed_num(rc) + " " + listed(misses) +
                    ": the" + some_pairs(missed, " and ") +
                    (missed.size() == 1 ? " pair adds" : " pairs add") +
                    " nothing to the descriptor, which sums over its other "
                    "element pairs alone."};
}

NeighbourRange neighbour_range(const DataStats &d, double rcut) {
  NeighbourRange r;
  if (d.per_pair.empty()) return r;
  r.lo = std::numeric_limits<double>::infinity();
  double top = 0.0;
  for (const auto &pp : d.per_pair) {
    r.lo = std::min(r.lo, pp.r_min);
    top = std::max(top, pp.r_max);
  }
  r.hi = rcut > 0.0 ? std::min(top, rcut) : top;
  r.empty = !(r.lo <= r.hi);
  return r;
}

double gaussian_reach_width(double dist) {
  if (dist == 0.0) return std::numeric_limits<double>::infinity();
  return std::log(1.0 / kDensityReachFloor) / (dist * dist);
}

double blip_reach_width(double dist) {
  if (dist == 0.0) return std::numeric_limits<double>::infinity();
  return (2.0 - std::cbrt(4.0 * kDensityReachFloor)) / dist;
}

HealthVerdict radial_reach_check(const DataStats &d, const SeedView &s,
                                 BasisKind kind) {
  if (kind != BasisKind::Gaussian && kind != BasisKind::Blip)
    throw std::logic_error(
        "radial_reach_check: a Gaussian or blip radial basis, not " +
        std::string(basis_name(kind)));
  if (!s.contains("CGRIDMB") || d.n_atoms == 0) return HealthOk{};
  const bool has_cut = s.contains("RCUTMB");
  const double rcut = has_cut ? s.get<double>("RCUTMB") : 0.0;
  const std::string within =
      has_cut ? "within RCUTMB = " + seed_num(rcut) : "of the dataset";
  // Each element pair of the line on its own: the neighbour distances of
  // each that the density sums over. A pair without one within RCUTMB is
  // the check of the cutoff's (cutoff_reach_check), which gives the finding
  // once; without RCUTMB, a pair the data do not have adds nothing to ρ.
  struct Reach {
    std::string name;
    NeighbourRange range;
    bool reached = false;  // by a channel of the density
  };
  std::vector<Reach> reach;
  std::vector<std::string> absent;
  for (const auto &p : judged_pairs(d, s)) {
    const NeighbourRange range = neighbour_range(p.stats, rcut);
    if (!range.empty)
      reach.push_back(Reach{p.name, range});
    else if (!has_cut)
      absent.push_back(p.name);
  }
  if (reach.empty()) {
    // Every pair beyond the cutoff: the finding of the cutoff's check.
    if (has_cut) return HealthOk{};
    return HealthFail{"the dataset has no" + of_pairs(s) +
                      " pair of atoms: ρ is 0 for every atom."};
  }
  if (!absent.empty())
    return HealthFail{"the dataset has no" + some_pairs(absent) +
                      " pair of atoms: ρ gets nothing from " +
                      (absent.size() == 1 ? "that pair." : "these pairs.")};
  // The distances of the pairs @p of, each by its range, as a Gaussian is
  // below 1e-6 at them (@p word "every": "every (W, W) neighbour distance
  // within RCUTMB = 3.5 (2.5 to 2.7 Å) and at every (Ta, Ta) one (2.86 to
  // 3.5 Å)") or the support of a blip holds none of them ("no": "no (W, W)
  // neighbour distance ... and no (Ta, Ta) one ...").
  auto distances = [&](const std::vector<const Reach *> &of,
                       const std::string &word) {
    std::vector<std::string> where;
    for (const Reach *r : of)
      where.push_back(
          (word == "every" && !where.empty() ? "at " : "") + word + r->name +
          (where.empty() ? " neighbour distance " + within : " one") + " (" +
          num(r->range.lo) + " to " + num(r->range.hi) + " Å)");
    return listed(where);
  };
  // The channels are judged together: a channel that reaches no pair of the
  // line fails (its channel of ρ is 0 for every atom); a pair that no channel
  // reaches fails below (ρ gets nothing from it); a channel that misses a
  // pair another channel reaches is no finding.
  const std::size_t n = s.size("CGRIDMB");
  const std::size_t widths = s.contains("SGRIDMB") ? s.size("SGRIDMB") : 0;
  for (std::size_t i = 0; i < n; ++i) {
    const double c = s.get<double>("CGRIDMB", i);
    if (i >= widths)
      return HealthFail{entry(s, "CGRIDMB", i) + " = " + seed_num(c) +
                        " has no width " + entry(s, "SGRIDMB", i) +
                        ": the reach of its channel of ρ cannot be judged."};
    const double eta = s.get<double>("SGRIDMB", i);
    if (!(eta > 0.0)) {
      // No decay: no reach failure (other rules refuse such a width).
      for (auto &r : reach) r.reached = true;
      continue;
    }
    const double half = 2.0 / eta;  // b(eta (r - c)) is 0 for |r - c| >= 2/eta
    // The pairs whose neighbour distances the channel does not reach, each
    // by the distance from the centre to the nearest of them.
    std::vector<const Reach *> missed;
    for (auto &r : reach) {
      const double dist = c < r.range.lo   ? r.range.lo - c
                          : c > r.range.hi ? c - r.range.hi
                                           : 0.0;
      const bool misses = kind == BasisKind::Gaussian
                              ? eta > gaussian_reach_width(dist)
                              : !(dist < half);
      if (misses)
        missed.push_back(&r);
      else
        r.reached = true;
    }
    if (missed.size() < reach.size()) continue;
    const std::string channel = entry(s, "CGRIDMB", i) + " = " +
                                seed_num(c) + " with " +
                                entry(s, "SGRIDMB", i) + " = " + seed_num(eta);
    if (kind == BasisKind::Gaussian)
      return HealthFail{channel +
                        ": the Gaussian exp(-SGRIDMB (r - CGRIDMB)^2) is "
                        "below 1e-6 at " + distances(missed, "every") +
                        ", so its channel of ρ is ~0 for every atom (a "
                        "smaller SGRIDMB, or a centre nearer the neighbours, "
                        "reaches them)."};
    return HealthFail{channel + ": the support of the blip, |r - CGRIDMB| < "
                      "2/SGRIDMB = " + num(half) + " Å, holds " +
                      distances(missed, "no") +
                      ", so its channel of ρ is 0 for every atom (a smaller "
                      "SGRIDMB widens the support)."};
  }
  // The pairs that no channel reaches.
  std::vector<const Reach *> unreached;
  std::vector<std::string> names;
  for (const auto &r : reach)
    if (!r.reached) {
      unreached.push_back(&r);
      names.push_back(r.name);
    }
  if (unreached.empty()) return HealthOk{};
  const std::string some =
      ", so ρ gets nothing from the" + some_pairs(names) + " pairs of atoms";
  // One channel: named with its centre and width; several: by their entries.
  const double eta0 = s.get<double>("SGRIDMB", 0);
  const std::string channels =
      n == 1 ? entry(s, "CGRIDMB", 0) + " = " +
                   seed_num(s.get<double>("CGRIDMB", 0)) + " with " +
                   entry(s, "SGRIDMB", 0) + " = " + seed_num(eta0)
             : "the channels " + entry(s, "CGRIDMB", 0) +
                   (n == 2 ? " and " : " to ") + entry(s, "CGRIDMB", n - 1) +
                   " with their widths " + entry(s, "SGRIDMB", 0) +
                   (n == 2 ? " and " : " to ") + entry(s, "SGRIDMB", n - 1);
  if (kind == BasisKind::Gaussian)
    return HealthFail{channels + ": " +
                      (n == 1 ? "the Gaussian" : "each Gaussian") +
                      " exp(-SGRIDMB (r - CGRIDMB)^2) is below 1e-6 at " +
                      distances(unreached, "every") + some +
                      " (a smaller SGRIDMB, or a centre nearer the "
                      "neighbours, reaches them)."};
  return HealthFail{channels + ": the support of " +
                    (n == 1 ? "the blip, |r - CGRIDMB| < 2/SGRIDMB = " +
                                  num(2.0 / eta0) + " Å,"
                            : std::string("each blip, |r - CGRIDMB| < "
                                          "2/SGRIDMB,")) +
                    " holds " + distances(unreached, "no") + some +
                    " (a smaller SGRIDMB widens the support)."};
}

BoundSuggestion gaussian_centre_suggestion(const DataStats &d,
                                           const SeedView &s, int arg_pos) {
  return centre_suggestion(d, s, arg_pos, BasisKind::Gaussian);
}

BoundSuggestion gaussian_width_suggestion(const DataStats &d,
                                          const SeedView &s, int arg_pos) {
  return width_suggestion(d, s, arg_pos, BasisKind::Gaussian);
}

BoundSuggestion blip_centre_suggestion(const DataStats &d, const SeedView &s,
                                       int arg_pos) {
  return centre_suggestion(d, s, arg_pos, BasisKind::Blip);
}

BoundSuggestion blip_width_suggestion(const DataStats &d, const SeedView &s,
                                      int arg_pos) {
  return width_suggestion(d, s, arg_pos, BasisKind::Blip);
}

}  // namespace models
}  // namespace tadah
