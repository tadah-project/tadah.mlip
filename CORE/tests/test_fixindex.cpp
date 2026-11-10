// test_fixindex.cpp
//
// core::parse_fixindex, the one reader of FIXINDEX: the model weights the
// FIXWEIGHT values pin, by position (the i-th value pins the weight of the
// i-th index). The trainers (M_BLR_Train, M_KRR_Train), MLIP's weight
// provisioning and composition check and tadah hpo's OPTIM FIXWEIGHT
// mapping and startup checks all read FIXINDEX with it.

#include "catch2/catch.hpp"
#include <tadah/core/context.h>
#include <tadah/core/utils.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using tadah::core::Context;
using V = std::vector<std::size_t>;

/* FIXINDEX with the values a configuration line gives (Context::add splits
 * on whitespace, as the configuration parser does: 'FIXINDEX 13, 14' is
 * the values '13,' and '14') and `n_fixweight` FIXWEIGHT values. */
Context fixed(const std::string& fixindex, std::size_t n_fixweight) {
  Context c;
  if (!fixindex.empty()) c.add("FIXINDEX", fixindex);
  for (std::size_t k = 0; k < n_fixweight; ++k)
    c.add("FIXWEIGHT", 0.5 + static_cast<double>(k));
  return c;
}

V parsed(const std::string& fixindex, std::size_t n_fixweight,
         std::optional<std::size_t> n_weights = std::nullopt) {
  return tadah::core::parse_fixindex(fixed(fixindex, n_fixweight), n_weights);
}

std::string refusal(const std::string& fixindex, std::size_t n_fixweight,
                    std::optional<std::size_t> n_weights = std::nullopt) {
  try {
    (void)parsed(fixindex, n_fixweight, n_weights);
  } catch (const std::runtime_error& e) {
    return e.what();
  }
  return "<no exception>";
}

const std::string kUsage =
    " (FIXINDEX takes 1-based weight indices i, a-b or a-b:step, separated "
    "by commas or spaces)";

}  // namespace

/* Pins: parse_fixindex(ctx, n_weights) reads the values of FIXINDEX as one
 * list, entries separated by commas or whitespace ('13 14', '13,14' and
 * '13, 14' are the same list), each 'i', 'a-b' or 'a-b:step' with spaces
 * allowed around '-' and ':', and returns the 1-based weight indices in
 * declaration order ('14,13' is {14, 13}): the i-th is the weight the i-th
 * FIXWEIGHT value pins. Neither key: an empty list. A known weight count
 * bounds the indices, an index up to it passes.
 * Before the fix: does not compile (there was no parse_fixindex; every
 * site read FIXINDEX with the lenient core::parse_indices, which joined the
 * values with a space and dropped '13 14' as one malformed segment). */
TEST_CASE("parse_fixindex: the FIXINDEX values form one list in declaration "
          "order",
          "[utils][indices][fixindex]") {
  CHECK(parsed("13 14", 2) == V{13, 14});
  CHECK(parsed("13,14", 2) == V{13, 14});
  CHECK(parsed("13, 14", 2) == V{13, 14});
  CHECK(parsed("14,13", 2) == V{14, 13});
  CHECK(parsed("14 13", 2) == V{14, 13});
  CHECK(parsed("1 - 3", 3) == V{1, 2, 3});
  CHECK(parsed("1-9 : 4", 3) == V{1, 5, 9});
  CHECK(parsed("4,1 7", 3, 7) == V{4, 1, 7});
  CHECK(parsed("13 14", 2, 14) == V{13, 14});
  CHECK(parsed("", 0).empty());
  CHECK(parsed("", 0, 4).empty());
}

/* Pins: each refusal of parse_fixindex, std::runtime_error with one
 * message naming the text:
 *  - a lone FIXINDEX or FIXWEIGHT (the trainers' text);
 *  - an empty entry, a token that is not an index, a descending range and a
 *    step of 0 ("FIXINDEX: index '14-13' is a descending range (...)");
 *  - index 0, and an index above a known weight count ("index 9 (in
 *    '2-9') is outside 1-4 (the model has 4 weights)");
 *  - a count other than FIXWEIGHT's ("FIXINDEX 1,2,3 has 3 indices but
 *    FIXWEIGHT has 1 value; they must pair 1:1 ...");
 *  - a weight named twice ("FIXINDEX 13,13,14 names coefficient 13
 *    twice"), since FIXWEIGHT pairs by position; the text ends with that
 *    rule as a fact, "so FIXINDEX names each coefficient once", and calls
 *    the fitted numbers coefficients, as the help of FIXINDEX and
 *    FIXWEIGHT does.
 * Before the fix: does not compile (no parse_fixindex). The lenient parser
 * removed a repeat with a stderr warning, expanded '14-13' to nothing,
 * dropped 'x' with a stderr line, took '1,,2' as 1, 2 and 0 as an index,
 * and each site reported a count mismatch or "not pinned" instead. The
 * repeat's text then ended with the instruction "so name each weight
 * once": the repeat cases fail on it. The text then said "names weight
 * 13 twice; each FIXWEIGHT value pins one weight": the repeat cases fail
 * on that too. */
TEST_CASE("parse_fixindex: a refusal names the FIXINDEX text",
          "[utils][indices][fixindex]") {
  const std::string lone =
      "FIXINDEX requires FIXWEIGHT (and vice versa). Provide both keys with "
      "matching lengths; the i-th FIXWEIGHT value pins the weight named by "
      "the i-th FIXINDEX entry. There is no auto-seeding from WEIGHTS.";
  const std::string pair =
      "; they must pair 1:1 (the i-th FIXWEIGHT value pins the weight of the "
      "i-th FIXINDEX index).";
  const std::string once =
      " twice; each FIXWEIGHT value fixes one coefficient (the i-th value "
      "the coefficient of the i-th index), so FIXINDEX names each "
      "coefficient once.";
  struct Case {
    std::string fixindex;
    std::size_t n_fixweight;
    std::optional<std::size_t> n_weights;
    std::string message;
  };
  const std::vector<Case> cases = {
      {"2", 0, std::nullopt, lone},
      {"", 1, std::nullopt, lone},
      {"13,13,14", 3, std::nullopt, "FIXINDEX 13,13,14 names coefficient 13" + once},
      {"1-3 2", 4, std::nullopt, "FIXINDEX 1-3 2 names coefficient 2" + once},
      {"14-13", 2, std::nullopt,
       "FIXINDEX: index '14-13' is a descending range" + kUsage},
      {"1-3:0", 1, std::nullopt, "FIXINDEX: index '1-3:0' has a step of 0" + kUsage},
      {"x", 1, std::nullopt, "FIXINDEX: index 'x' is not an index" + kUsage},
      {"1-x", 1, std::nullopt, "FIXINDEX: index '1-x' is not an index range" + kUsage},
      {"1,,2", 2, std::nullopt,
       "FIXINDEX: the index list '1,,2' has an empty entry" + kUsage},
      {"0", 1, std::nullopt,
       "FIXINDEX: index 0 is not a model weight (the weights are numbered "
       "from 1)"},
      {"0-2", 3, std::nullopt,
       "FIXINDEX: index 0 (in '0-2') is not a model weight (the weights are "
       "numbered from 1)"},
      {"0", 1, 4, "FIXINDEX: index 0 is outside 1-4 (the model has 4 weights)"},
      {"5", 1, 4, "FIXINDEX: index 5 is outside 1-4 (the model has 4 weights)"},
      {"2-9", 8, 4,
       "FIXINDEX: index 9 (in '2-9') is outside 1-4 (the model has 4 weights)"},
      {"2", 1, 1, "FIXINDEX: index 2 is outside 1-1 (the model has 1 weight)"},
      {"1,2,3", 1, std::nullopt,
       "FIXINDEX 1,2,3 has 3 indices but FIXWEIGHT has 1 value" + pair},
      {"1", 2, 4, "FIXINDEX 1 has 1 index but FIXWEIGHT has 2 values" + pair},
  };
  for (const Case& c : cases) {
    INFO("FIXINDEX '" << c.fixindex << "', " << c.n_fixweight
                      << " FIXWEIGHT values, n_weights "
                      << (c.n_weights ? std::to_string(*c.n_weights) : "none"));
    CHECK(refusal(c.fixindex, c.n_fixweight, c.n_weights) == c.message);
  }
}

/* Pins: a weight named twice among the first n + 1 indices (n the number
 * of FIXWEIGHT values, in declaration order) is refused as such ("FIXINDEX
 * 13,13,14 names coefficient 13 twice; ...", the first weight named
 * again):
 * so every list with a repeat that names no more distinct weights than
 * FIXWEIGHT has values, the configuration an earlier version accepted by
 * removing the repeat ('13,13,14' with two values, '1,1' with one, '1-3,2'
 * with three, also with a weight count), as with one value per index; and
 * '1,1,2,3' and '1,2,1-4000000000' with two values, whose repeat comes
 * among their first three indices. A list whose first n + 1 indices name
 * n + 1 weights is a count mismatch whatever its later repeats ('1,2,3,1'
 * with two values: "has 4 indices but FIXWEIGHT has 2 values"), also with
 * a range past the values ('1,2,3-4000000000' and '1-4000000000,1',
 * refused at once).
 * Before the fix: the count was compared first, so '13,13,14' with two
 * values was refused as "FIXINDEX 13,13,14 has 3 indices but FIXWEIGHT has
 * 2 values" (this file pinned that), '1,1' with one as "has 2 indices but
 * FIXWEIGHT has 1 value" and '1-3,2' with three as "has 4 indices but
 * FIXWEIGHT has 3 values". The first version of the fix named a repeat
 * only when the list named no more distinct weights than FIXWEIGHT has
 * values, expanding until it named more, so '1,1,2,3' and
 * '1,2,1-4000000000' with two values were count mismatches (this case
 * pinned that); the other count cases pass as well (pins). The repeat's
 * text ended with the instruction "so name each weight once", where it
 * ends "so FIXINDEX names each coefficient once": every repeat case fails
 * on that, and on "names weight 13 twice; each FIXWEIGHT value pins one
 * weight", where it says "names coefficient 13 twice; each FIXWEIGHT value
 * fixes one coefficient". */
TEST_CASE("parse_fixindex: a weight named twice is named before the count",
          "[utils][indices][fixindex]") {
  const std::string pair =
      "; they must pair 1:1 (the i-th FIXWEIGHT value pins the weight of the "
      "i-th FIXINDEX index).";
  const std::string once =
      " twice; each FIXWEIGHT value fixes one coefficient (the i-th value "
      "the coefficient of the i-th index), so FIXINDEX names each "
      "coefficient once.";
  struct Case {
    std::string fixindex;
    std::size_t n_fixweight;
    std::optional<std::size_t> n_weights;
    std::string message;
  };
  const std::vector<Case> cases = {
      {"13,13,14", 2, std::nullopt, "FIXINDEX 13,13,14 names coefficient 13" + once},
      {"13,13,14", 2, 14, "FIXINDEX 13,13,14 names coefficient 13" + once},
      {"1,1", 1, std::nullopt, "FIXINDEX 1,1 names coefficient 1" + once},
      {"1-3,2", 3, std::nullopt, "FIXINDEX 1-3,2 names coefficient 2" + once},
      {"1,1,2,3", 2, std::nullopt, "FIXINDEX 1,1,2,3 names coefficient 1" + once},
      {"1,2,1-4000000000", 2, std::nullopt,
       "FIXINDEX 1,2,1-4000000000 names coefficient 1" + once},
      {"1,2,3,1", 2, std::nullopt,
       "FIXINDEX 1,2,3,1 has 4 indices but FIXWEIGHT has 2 values" + pair},
      {"1,2,3-4000000000", 2, std::nullopt,
       "FIXINDEX 1,2,3-4000000000 has 4000000000 indices but FIXWEIGHT has 2 "
       "values" + pair},
      {"1-4000000000,1", 2, std::nullopt,
       "FIXINDEX 1-4000000000,1 has 4000000001 indices but FIXWEIGHT has 2 "
       "values" + pair},
  };
  for (const Case& c : cases) {
    INFO("FIXINDEX '" << c.fixindex << "', " << c.n_fixweight
                      << " FIXWEIGHT values, n_weights "
                      << (c.n_weights ? std::to_string(*c.n_weights) : "none"));
    CHECK(refusal(c.fixindex, c.n_fixweight, c.n_weights) == c.message);
  }
}

/* Pins: the number of indices is counted per range, before any range is
 * expanded, and compared with FIXWEIGHT's: '1-4000000000' with 2 FIXWEIGHT
 * values is refused at once naming 4000000000 indices, a range to the
 * largest size_t too, and two of them (a count past the largest size_t)
 * as "more than 18446744073709551615 indices"; with a known weight count
 * the bound refuses the range first.
 * Before the fix: does not compile (no parse_fixindex). The lenient parser
 * expanded every range into a vector before any check (32 GB for
 * '1-4000000000'). */
TEST_CASE("parse_fixindex: the count is checked before a range is expanded",
          "[utils][indices][fixindex]") {
  const std::string pair =
      "; they must pair 1:1 (the i-th FIXWEIGHT value pins the weight of the "
      "i-th FIXINDEX index).";
  const std::string max = "18446744073709551615";
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"1-4000000000",
       "FIXINDEX 1-4000000000 has 4000000000 indices but FIXWEIGHT has 2 "
       "values" + pair},
      {"1-" + max,
       "FIXINDEX 1-" + max + " has " + max + " indices but FIXWEIGHT has 2 "
       "values" + pair},
      {"1-" + max + ",1-" + max,
       "FIXINDEX 1-" + max + ",1-" + max + " has more than " + max +
           " indices but FIXWEIGHT has 2 values" + pair},
  };
  for (const auto& c : cases) {
    INFO(c.first);
    CHECK(refusal(c.first, 2) == c.second);
  }
  CHECK(refusal("1-4000000000", 2, 4) ==
        "FIXINDEX: index 4000000000 (in '1-4000000000') is outside 1-4 (the "
        "model has 4 weights)");
}
