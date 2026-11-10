// intercept_layout.h: the intercept columns of BIAS true. Every atom has a
// one-hot intercept column for its species, one column per ATOM species in
// ATOM order, before the descriptor columns: the layout the descriptors
// (MLIP DescriptorsCalc, MODELS DC_Selector), the ridge, the normaliser,
// the NORM weight conversion and pair_style tadah share.

#include "catch2/catch.hpp"

#include <tadah/core/context.h>
#include <tadah/core/intercept_layout.h>
#include <tadah/core/periodic_table.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

using namespace tadah::core;

namespace {

Context bias_context(bool bias, const std::vector<std::string> &atoms) {
  Context c;
  c.add("BIAS", bias ? "true" : "false");
  for (const auto &a : atoms) c.add("ATOM", a);
  return c;
}

}  // namespace

// Pins: n_intercept_columns is 0 for BIAS false (with and without ATOM),
// 1 for BIAS true with ATOM Ta and 2 with ATOM Nb Ta: one column per ATOM
// species.
// Before the fix: passes as well (a pin of the counts that the refusal of
// BIAS true without ATOM below must leave unchanged).
TEST_CASE("n_intercept_columns: one intercept column per ATOM species when "
          "BIAS is true, none when it is false",
          "[intercept_layout]") {
  CHECK(n_intercept_columns(bias_context(false, {})) == 0);
  CHECK(n_intercept_columns(bias_context(false, {"Nb", "Ta"})) == 0);
  CHECK(n_intercept_columns(bias_context(true, {"Ta"})) == 1);
  CHECK(n_intercept_columns(bias_context(true, {"Nb", "Ta"})) == 2);
}

// Pins: BIAS true without an ATOM key is refused by n_intercept_columns
// with std::runtime_error naming BIAS and ATOM: there is no species to give
// an intercept column to.
// Before the fix: n_intercept_columns returned 0 (no exception), so
// DC_Selector built no intercept column while MLIP's DescriptorsCalc built
// one, and the weights of such a model did not fit the LAMMPS descriptors.
TEST_CASE("n_intercept_columns refuses BIAS true without ATOM",
          "[intercept_layout]") {
  const Context c = bias_context(true, {});
  CHECK_THROWS_AS(n_intercept_columns(c), std::runtime_error);
  CHECK_THROWS_WITH(n_intercept_columns(c), Catch::Contains("BIAS true") &&
                                                Catch::Contains("ATOM"));
}

// Pins: intercept_columns_by_Z maps each atomic number to its intercept
// column in ATOM order (ATOM Nb Ta: Nb (41) -> 0, Ta (73) -> 1; ATOM Ta Nb:
// Ta -> 0, Nb -> 1), every other Z from 0 to 118 to -1, and agrees with
// species_intercept_idx for every Z.
// Before the fix: intercept_columns_by_Z did not exist (run with a stub that
// throws std::logic_error: both sections fail on that exception).
TEST_CASE("intercept_columns_by_Z follows the ATOM order",
          "[intercept_layout]") {
  PeriodicTable::initialize();
  SECTION("ATOM Nb Ta") {
    const Context c = bias_context(true, {"Nb", "Ta"});
    const std::array<int, 119> col = intercept_columns_by_Z(c);
    for (int Z = 0; Z <= 118; ++Z) {
      CAPTURE(Z);
      const int expected = Z == 41 ? 0 : Z == 73 ? 1 : -1;
      CHECK(col[Z] == expected);
      if (Z > 0) CHECK(col[Z] == species_intercept_idx(Z, c));
    }
  }
  SECTION("ATOM Ta Nb") {
    const Context c = bias_context(true, {"Ta", "Nb"});
    const std::array<int, 119> col = intercept_columns_by_Z(c);
    CHECK(col[73] == 0);
    CHECK(col[41] == 1);
    CHECK(col[74] == -1);
    CHECK(col[0] == -1);
  }
}
