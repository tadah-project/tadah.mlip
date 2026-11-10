// The blueprint field `allowed_values` (BlueprintKey::allowed_values): the
// only values a STRING key takes. BlueprintParser checks the field;
// CoreUserConfigParser rejects any other value at the line that sets it --
// the line of the file, and the file -- and so does the command-line path
// (validateWithBlueprint). EMBF0 (ZERO|FREE,
// the DM_REAM embedding reference) is the first key that uses it: a
// misspelt value used to reach the model and fail only when a DM_REAM
// descriptor was built -- or, in a potential without one, be dropped with
// a warning. RHOFORM (SIGNED|NONNEG|MONOTONE, the DM_REAM density form)
// uses it too, and has no default.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/logger.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <toml.hpp>
#include <unistd.h>
#include <vector>

using namespace tadah::core;
using Catch::Matchers::Contains;

namespace {

/* One STRING key table after INHERIT expansion, plus `extra_fields`. */
toml::value string_key(const std::string &extra_fields,
                       const std::string &examples =
                           "examples = [\"ZERO\", \"FREE\"]\n",
                       const std::string &value_type = "[\"STRING\"]") {
  const std::string text = "[KEY]\n"
                           "value_type = " + value_type + "\n"
                           "value_format = [\"<string>\"]\n"
                           "value_N = [1]\n"
                           "description = \"A key.\"\n" +
                           examples + extra_fields;
  std::istringstream in(text);
  return toml::find(toml::parse(in), "KEY");
}

/* Silences the Logger's ERROR lines of a rejected value (they go to
 * std::cout); restores stream and level afterwards. */
class Quiet {
public:
  Quiet()
      : level_(Logger::instance().getLogLevel()),
        old_(std::cout.rdbuf(out_.rdbuf())) {}
  ~Quiet() {
    std::cout.rdbuf(old_);
    Logger::instance().setLogLevel(level_);
  }
  std::string out() const { return out_.str(); }

private:
  LogLevel level_;
  std::ostringstream out_;
  std::streambuf *old_;
};

} // namespace

TEST_CASE("Blueprint: EMBF0 takes ZERO or FREE, default ZERO",
          "[BlueprintParser][allowed_values][embf0]") {
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("EMBF0");
  CHECK(k.allowed_values == std::vector<std::string>{"ZERO", "FREE"});
  CHECK(k.default_value == std::vector<std::string>{"ZERO"});
  CHECK(k.value_format == std::vector<std::string>{"ZERO|FREE"});
  /* An ordinary key has no list. */
  CHECK(BlueprintParser::getKeysMapRef().at("DBFILE").allowed_values.empty());
}

TEST_CASE("BlueprintParser::buildKey reads `allowed_values`",
          "[BlueprintParser][allowed_values]") {
  CHECK(BlueprintParser::buildKey("KEY", string_key("")).allowed_values.empty());
  const BlueprintKey k = BlueprintParser::buildKey(
      "KEY", string_key("allowed_values = [\"ZERO\", \"FREE\"]\n"
                        "default_value = [\"zero\"]\n"));
  CHECK(k.allowed_values == std::vector<std::string>{"ZERO", "FREE"});
}

TEST_CASE("BlueprintParser::buildKey rejects an inconsistent allowed_values",
          "[BlueprintParser][allowed_values]") {
  SECTION("an empty list or one that is not a list of strings") {
    for (const std::string bad :
         {"allowed_values = []\n", "allowed_values = \"ZERO\"\n",
          "allowed_values = [1, 2]\n", "allowed_values = [\"\"]\n"}) {
      INFO(bad);
      REQUIRE_THROWS_WITH(BlueprintParser::buildKey("KEY", string_key(bad)),
                          Contains("'allowed_values' must be a non-empty "
                                   "list of non-empty strings") &&
                              Contains("KEY"));
    }
  }
  SECTION("a key that does not hold one string") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", string_key("allowed_values = [\"1\", \"2\"]\n",
                              "examples = [\"1\"]\n", "[\"INT\"]")),
        Contains("allowed_values") && Contains("KEY") &&
            Contains("value_type STRING"));
  }
  SECTION("a default outside the list") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", string_key("allowed_values = [\"ZERO\", \"FREE\"]\n"
                              "default_value = [\"NONE\"]\n")),
        Contains("default_value") && Contains("NONE") && Contains("KEY"));
  }
  SECTION("an example outside the list") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", string_key("allowed_values = [\"ZERO\", \"FREE\"]\n",
                              "examples = [\"ZERO\", \"OTHER\"]\n")),
        Contains("examples") && Contains("OTHER") && Contains("KEY"));
  }
}

TEST_CASE("Config parser: EMBF0 takes ZERO or FREE, any case, and nothing "
          "else", "[config][allowed_values][embf0]") {
  SECTION("the two values, in any case, are stored as written") {
    for (const std::string v : {"ZERO", "FREE", "zero", "Free"}) {
      INFO(v);
      Context c;
      REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(
                          std::vector<std::string>{"ATOM Ta", "EMBF0 " + v}));
      REQUIRE(c.stored_value("EMBF0").has_value());
      CHECK(*c.stored_value("EMBF0") == v);
    }
  }
  SECTION("any other value is an error that names the line, the value and "
          "the allowed values") {
    Quiet q;
    for (const std::string v : {"ZEROO", "NONE", "ZERO,"}) {
      INFO(v);
      REQUIRE_THROWS_WITH(
          CoreUserConfigParser().parse(std::vector<std::string>{
              "# a comment", "ATOM Ta", "EMBF0 " + v}),
          Contains("Invalid value for key EMBF0 on line 3: '" + v + "'") &&
              Contains("allowed: ZERO, FREE"));
    }
  }
  SECTION("a file's error names the file and its line") {
    Quiet q;
    const std::string path =
        "allowed_values_" + std::to_string(::getpid()) + ".tadah";
    {
      std::ofstream os(path);
      os << "# potential\nATOM Ta\n\nEMBF0 frei\n";
    }
    struct Remove {
      std::string p;
      ~Remove() { std::remove(p.c_str()); }
    } remove{path};
    REQUIRE_THROWS_WITH(CoreUserConfigParser().parse(path),
                        Contains("Invalid value for key EMBF0 on line 4 of " +
                                 path + ": 'frei'"));
  }
  // A second value is refused at its line, as any value the key's types do
  // not take ("Invalid schema for key: EMBF0", without the line, before the
  // line was named).
  SECTION("a second value is an error (one value)") {
    Quiet q;
    REQUIRE_THROWS_WITH(CoreUserConfigParser().parse(std::vector<std::string>{
                            "EMBF0 ZERO FREE"}),
                        Contains("Invalid value for key EMBF0 on line 1: "
                                 "'ZERO FREE'"));
  }
}

TEST_CASE("validateWithBlueprint (the CLI and task path) applies "
          "allowed_values", "[config][allowed_values][embf0]") {
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("EMBF0");
  Quiet q;
  CHECK(CoreUserConfigParser::validateWithBlueprint("EMBF0", "FREE", k));
  CHECK(CoreUserConfigParser::validateWithBlueprint("EMBF0", "zero", k));
  CHECK_FALSE(CoreUserConfigParser::validateWithBlueprint("EMBF0", "ZEROO", k));
  CHECK_THAT(q.out(), Contains("Allowed values") && Contains("ZERO") &&
                          Contains("FREE"));
}

TEST_CASE("Blueprint: RHOFORM takes SIGNED, NONNEG or MONOTONE, no default",
          "[BlueprintParser][allowed_values][rhoform]") {
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("RHOFORM");
  CHECK(k.allowed_values ==
        std::vector<std::string>{"SIGNED", "NONNEG", "MONOTONE"});
  CHECK(k.default_value.empty());   // mandatory: nothing supplies it
  CHECK(k.value_format ==
        std::vector<std::string>{"SIGNED|NONNEG|MONOTONE"});
}

TEST_CASE("Config parser: RHOFORM takes SIGNED, NONNEG or MONOTONE, any "
          "case, and nothing else", "[config][allowed_values][rhoform]") {
  SECTION("the three values, in any case, are stored as written") {
    for (const std::string v :
         {"SIGNED", "NONNEG", "MONOTONE", "signed", "NonNeg", "monotone"}) {
      INFO(v);
      Context c;
      REQUIRE_NOTHROW(c = CoreUserConfigParser().parse(
                          std::vector<std::string>{"ATOM Ta", "RHOFORM " + v}));
      REQUIRE(c.stored_value("RHOFORM").has_value());
      CHECK(*c.stored_value("RHOFORM") == v);
    }
  }
  SECTION("any other value is an error at the line of the file") {
    Quiet q;
    const std::string path =
        "allowed_values_rhoform_" + std::to_string(::getpid()) + ".tadah";
    {
      std::ofstream os(path);
      os << "# potential\nATOM Ta\nTYPEMB DM_REAM 0 4 4 4 5 5 0 0 Ta Ta\n"
            "RHOFORM SQUARE\n";
    }
    struct Remove {
      std::string p;
      ~Remove() { std::remove(p.c_str()); }
    } remove{path};
    REQUIRE_THROWS_WITH(
        CoreUserConfigParser().parse(path),
        Contains("Invalid value for key RHOFORM on line 4 of " + path +
                 ": 'SQUARE'") &&
            Contains("allowed: SIGNED, NONNEG, MONOTONE"));
    for (const std::string v : {"SIGN", "NONNEGATIVE", "MONOTONE,"}) {
      INFO(v);
      REQUIRE_THROWS_WITH(
          CoreUserConfigParser().parse(
              std::vector<std::string>{"ATOM Ta", "RHOFORM " + v}),
          Contains("Invalid value for key RHOFORM on line 2: '" + v + "'"));
    }
  }
  SECTION("the CLI and task path (validateWithBlueprint)") {
    const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("RHOFORM");
    Quiet q;
    CHECK(CoreUserConfigParser::validateWithBlueprint("RHOFORM", "MONOTONE", k));
    CHECK(CoreUserConfigParser::validateWithBlueprint("RHOFORM", "nonneg", k));
    CHECK_FALSE(
        CoreUserConfigParser::validateWithBlueprint("RHOFORM", "SQUARE", k));
    CHECK_THAT(q.out(), Contains("Allowed values") && Contains("SIGNED") &&
                            Contains("NONNEG") && Contains("MONOTONE"));
  }
}

TEST_CASE("Blueprint: RETARGET_RHOFORM (tadah refit --rhoform) takes SIGNED, "
          "NONNEG or MONOTONE",
          "[BlueprintParser][allowed_values][rhoform]") {
  const BlueprintKey &k =
      BlueprintParser::getKeysMapRef().at("RETARGET_RHOFORM");
  CHECK(k.allowed_values ==
        std::vector<std::string>{"SIGNED", "NONNEG", "MONOTONE"});
  CHECK(k.default_value.empty());
  CHECK(k.cli_long_flag == "--rhoform");
  Quiet q;
  CHECK(CoreUserConfigParser::validateWithBlueprint("RETARGET_RHOFORM",
                                                    "nonneg", k));
  CHECK_FALSE(CoreUserConfigParser::validateWithBlueprint("RETARGET_RHOFORM",
                                                          "SQUARE", k));
}
