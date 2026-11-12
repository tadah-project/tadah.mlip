// The blueprint field `names`: true for a key whose STRING values are
// names (element symbols, descriptor, cutoff and model names), which hold
// no comma and no '+'; CoreUserConfigParser refuses such a value as a
// separator typo (test_core_user_config_parser.cpp). BlueprintParser checks
// the field itself.

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
                           "value_N = [-10]\n"
                           "description = \"A key.\"\n"
                           "examples = []\n" + extra;
  std::istringstream in(text);
  return toml::find(toml::parse(in), "KEY");
}

}  // namespace

/* Pins: BlueprintParser::buildKey refuses a `names` that is not a boolean
 * ("'names' must be true or false in key: KEY") and `names = true` on a key
 * whose value_type has no STRING value ("'names' needs a key whose
 * value_type has STRING values: KEY"); it takes `names = true` on a
 * {STRING} key and on a group with a STRING ({STRING STRING DOUBLE}), and
 * `names = false` on any key.
 * Before the fix: buildKey did not read `names` (no such field): the two
 * refusals threw nothing; the other forms were read as well (a pin). */
TEST_CASE("BlueprintParser::buildKey checks the field `names`",
          "[BlueprintParser][names]") {
  CHECK_THROWS_WITH(
      BlueprintParser::buildKey("KEY", key_table("[\"{STRING}\"]",
                                                 "names = \"yes\"\n")),
      Contains("'names' must be true or false in key: KEY"));
  CHECK_THROWS_WITH(
      BlueprintParser::buildKey("KEY", key_table("[\"{DOUBLE}\"]",
                                                 "names = true\n")),
      Contains("'names' needs a key whose value_type has STRING values: "
               "KEY"));
  CHECK_NOTHROW(BlueprintParser::buildKey(
      "KEY", key_table("[\"{STRING}\"]", "names = true\n")));
  CHECK_NOTHROW(BlueprintParser::buildKey(
      "KEY", key_table("[\"{STRING STRING DOUBLE}\", \"{DOUBLE}\"]",
                       "names = true\n")));
  CHECK_NOTHROW(BlueprintParser::buildKey(
      "KEY", key_table("[\"{DOUBLE}\"]", "names = false\n")));
}
