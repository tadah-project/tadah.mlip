// The blueprint field `deprecated` (BlueprintKey::deprecated) and the rules
// BlueprintParser enforces for it. Parser behaviour for deprecated keys in
// configuration files is in test_deprecated_keys.cpp.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>

#include <sstream>
#include <string>
#include <toml.hpp>

using namespace tadah::core;

namespace {

/* The examples line of a deprecated key: an empty list, which also
 * overrides the examples of an inherited TYPES table. */
const std::string kNoExamples = "examples = []\n";

/* One key table in the form the blueprint uses after INHERIT expansion;
 * `examples` is its examples line. */
toml::value key_table(const std::string &extra_fields,
                      const std::string &examples = "examples = [\"20\"]\n") {
  const std::string text = "[KEY]\n"
                           "value_type = [\"UINT\"]\n"
                           "value_format = [\"<unsigned integer>\"]\n"
                           "value_N = [1]\n"
                           "description = \"A key.\"\n" +
                           examples + extra_fields;
  std::istringstream in(text);
  return toml::find(toml::parse(in), "KEY");
}

} // namespace

TEST_CASE("Blueprint: the legacy MPI keys are deprecated, with the reason",
          "[BlueprintParser][deprecated]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string key : {"MPIWPCKG", "MBLOCK", "NBLOCK"}) {
    INFO(key);
    const BlueprintKey &k = keys.at(key);
    CHECK(k.isDeprecated());
    CHECK_THAT(k.deprecated, Catch::Matchers::Contains("1.6.0") &&
                                 Catch::Matchers::Contains("TADAH_BUILD_MPI") &&
                                 Catch::Matchers::Contains("ignored"));
    CHECK(k.default_value.empty());
    CHECK(k.cli_long_flag.empty());
    CHECK(k.cli_short_flag.empty());
    /* TYPES.UINT, which the keys inherit, has examples: the keys clear
     * them, so no page or error message shows how to use them. */
    CHECK(k.examples.empty());
  }
  /* An ordinary key is not deprecated. */
  CHECK_FALSE(keys.at("LAMBDA").isDeprecated());
}

TEST_CASE("BlueprintParser::buildKey reads `deprecated`",
          "[BlueprintParser][deprecated]") {
  const BlueprintKey plain = BlueprintParser::buildKey("KEY", key_table(""));
  CHECK_FALSE(plain.isDeprecated());
  CHECK(plain.deprecated.empty());

  const BlueprintKey dep = BlueprintParser::buildKey(
      "KEY", key_table("deprecated = \"Removed in 9.9; use OTHER.\"\n",
                       kNoExamples));
  CHECK(dep.isDeprecated());
  CHECK(dep.deprecated == "Removed in 9.9; use OTHER.");
  /* The format is kept: a deprecated key's value is still validated. */
  CHECK(dep.value_type == std::vector<std::string>{"UINT"});
}

TEST_CASE("BlueprintParser::buildKey rejects an inconsistent deprecated key",
          "[BlueprintParser][deprecated]") {
  using Catch::Matchers::Contains;
  SECTION("a default value would be inserted by Context::contains()") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", key_table("deprecated = \"Gone.\"\ndefault_value = [\"1\"]\n",
                             kNoExamples)),
        Contains("Deprecated key KEY must not define default_value"));
  }
  SECTION("a CLI flag would offer the key to new users") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", key_table("deprecated = \"Gone.\"\ncli_long_flag = \"--key\"\n",
                             kNoExamples)),
        Contains("Deprecated key KEY must not define a CLI flag"));
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", key_table("deprecated = \"Gone.\"\ncli_short_flag = \"-k\"\n",
                             kNoExamples)),
        Contains("Deprecated key KEY must not define a CLI flag"));
  }
  SECTION("examples would show how to use a key that must be deleted") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey("KEY", key_table("deprecated = \"Gone.\"\n")),
        Contains("Deprecated key KEY must not define examples"));
    /* An empty list is accepted. */
    REQUIRE_NOTHROW(BlueprintParser::buildKey(
        "KEY", key_table("deprecated = \"Gone.\"\n", kNoExamples)));
  }
  SECTION("the reason must be a non-empty string") {
    for (const std::string bad :
         {"deprecated = \"\"\n", "deprecated = \"  \\n\"\n",
          "deprecated = true\n", "deprecated = [\"Gone.\"]\n"}) {
      INFO(bad);
      REQUIRE_THROWS_WITH(
          BlueprintParser::buildKey("KEY", key_table(bad, kNoExamples)),
          Contains("'deprecated' must be a non-empty string") &&
              Contains("KEY"));
    }
  }
}
