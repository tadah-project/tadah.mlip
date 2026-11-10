// The blueprint field `exact_in_potential`: a key whose numbers a potential
// file records exactly whatever NUMERIC is, because a load checks them
// exactly (the cutoffs, which D2_EAM/DM_EAM compare with the setfl file and
// the knot families with their largest knot, and the radial grids those
// checks relate them to). Context::write_potential consults it; the
// configuration-keys page shows it. BlueprintParser::buildKey checks the
// field: a boolean, on a key with a DOUBLE position (on any other key it
// would govern nothing).

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>

#include <sstream>
#include <string>
#include <toml.hpp>

using namespace tadah::core;
using Catch::Matchers::Contains;

namespace {

/* One key table after INHERIT expansion, of `value_type`, plus `extra`. */
toml::value key_table(const std::string &value_type,
                      const std::string &extra) {
  const std::string text = "[KEY]\n"
                           "value_type = " + value_type + "\n"
                           "value_format = [\"<value>\"]\n"
                           "value_N = [-2147483647]\n"
                           "description = \"A key.\"\n"
                           "examples = []\n" + extra;
  std::istringstream in(text);
  return toml::find(toml::parse(in), "KEY");
}

} // namespace

/* Pins: buildKey refuses an `exact_in_potential` that is not a boolean
 * ("yes", 1), naming the field and the key ("'exact_in_potential' must be
 * true or false in key: KEY"), and `exact_in_potential = true` on a key
 * without a DOUBLE position (["{STRING}"], ["INT", "{INT}"]), naming the
 * key and why ("KEY: exact_in_potential = true needs a DOUBLE position in
 * value_type"); it accepts `true` on ["{DOUBLE}"] and on the grid form
 * ["{DOUBLE}", "{STRING INT DOUBLE DOUBLE}"], and `false` on any key.
 * Before the fix: the field was unknown and ignored, so every case built a
 * key and no refusal was thrown. */
TEST_CASE("BlueprintParser::buildKey checks `exact_in_potential`",
          "[BlueprintParser][exact_in_potential]") {
  SECTION("a value that is not a boolean") {
    for (const std::string bad :
         {"exact_in_potential = \"yes\"\n", "exact_in_potential = 1\n"}) {
      INFO(bad);
      REQUIRE_THROWS_WITH(
          BlueprintParser::buildKey("KEY", key_table("[\"{DOUBLE}\"]", bad)),
          Contains("'exact_in_potential' must be true or false in key: KEY"));
    }
  }
  SECTION("true on a key without a DOUBLE position") {
    for (const std::string vt : {"[\"{STRING}\"]", "[\"INT\", \"{INT}\"]"}) {
      INFO(vt);
      REQUIRE_THROWS_WITH(
          BlueprintParser::buildKey(
              "KEY", key_table(vt, "exact_in_potential = true\n")),
          Contains("KEY: exact_in_potential = true needs a DOUBLE position "
                   "in value_type"));
    }
  }
  SECTION("true on a DOUBLE key, false on any key") {
    CHECK_NOTHROW(BlueprintParser::buildKey(
        "KEY", key_table("[\"{DOUBLE}\"]", "exact_in_potential = true\n")));
    CHECK_NOTHROW(BlueprintParser::buildKey(
        "KEY", key_table("[\"{DOUBLE}\", \"{STRING INT DOUBLE DOUBLE}\"]",
                         "exact_in_potential = true\n")));
    CHECK_NOTHROW(BlueprintParser::buildKey(
        "KEY", key_table("[\"{STRING}\"]", "exact_in_potential = false\n")));
  }
}
