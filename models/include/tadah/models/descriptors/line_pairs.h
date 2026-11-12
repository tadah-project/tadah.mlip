#ifndef TADAH_MODELS_DESCRIPTORS_LINE_PAIRS_H
#define TADAH_MODELS_DESCRIPTORS_LINE_PAIRS_H

// The element pairs of a descriptor's TYPE2B or TYPEMB line.
//
// A descriptor sums over the neighbours of the element pairs its line names
// ("TYPE2B D2_Blip 4 4 Ta W": the Ta-W pairs alone), so the checks and the
// suggestions of its parameters (the pre-flight audit, tadah analysis
// check_bounds and suggest_bounds) read the neighbour statistics of those
// pairs: the distances of the other pairs of the data say nothing about its
// cutoff or its channels.

#include <string>
#include <utility>
#include <vector>

#include <tadah/core/stats/data_stats.h>

namespace tadah {
namespace models {

/** \brief The element pairs a descriptor's TYPE2B or TYPEMB line names, in
 *  the order of the line: "D2_Blip 4 4 Ta Ta Ta W" names (Ta, Ta) and (Ta,
 *  W). A pair names its two elements in either order, and "*" names every
 *  element: "Ta *" every pair with Ta, "* *" every pair. */
class LinePairs {
public:
  /** \brief The pairs of @p elements, the element tokens of the line, two
   *  by two (D_Base::get_init_atoms()). @throws std::invalid_argument for an
   *  odd number of tokens, naming them. */
  explicit LinePairs(const std::vector<std::string> &elements);

  /** \brief The pairs, as the line writes them. */
  const std::vector<std::pair<std::string, std::string>> &pairs() const {
    return pairs_;
  }

  /** \brief Whether a pair of the line names the elements @p a and @p b,
   *  in either order. */
  bool names(const std::string &a, const std::string &b) const;

  /** \brief Whether the line names every element pair ("* *"). */
  bool every() const;

  /** \brief The pairs as a message names them, each once: "(Ta, Ta)",
   *  "(Ta, Ta) or (W, W)", "(Ta, Ta), (Ta, W) or (W, W)", "(Ta, *)"; empty
   *  when the line names every pair, or none. */
  std::string text() const;

  /** \brief @p d with the per_pair entries of the pairs the line names
   *  alone, in their order; its other fields as in @p d. */
  tadah::core::stats::DataStats
  stats(const tadah::core::stats::DataStats &d) const;

private:
  std::vector<std::pair<std::string, std::string>> pairs_;
};

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_LINE_PAIRS_H
