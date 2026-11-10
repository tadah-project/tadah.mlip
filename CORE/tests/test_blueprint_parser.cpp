#include "catch2/catch.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include "tadah/core/blueprint_parser.h"
#include "tadah/core/blueprint_validator.h"
#include <cctype>
#include <sstream>

using namespace tadah::core;

TEST_CASE("BlueprintParser loads ALLOWED_KEYS from blueprint_tadah.toml", "[BlueprintParser]")
{
  // Retrieve the statically cached blueprint map.
  const auto &blueprintMap = BlueprintParser::getKeysMapRef();

  SECTION("The blueprint map is not empty")
  {
    // The blueprint map must contain entries.
    REQUIRE(!blueprintMap.empty());
  }

  SECTION("The CONFIG key is parsed correctly")
  {
    // The CONFIG key should be present in uppercase.
    auto it = blueprintMap.find("CONFIG");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &configKey = it->second;

    // Validate expected fields.
    REQUIRE(configKey.value_type == std::vector<std::string>{"STRING"});
    REQUIRE(configKey.value_format == std::vector<std::string>{"<file>"});
    REQUIRE(configKey.value_N == std::vector<int>{1});
    REQUIRE(configKey.description == "Path to a configuration file.");
    REQUIRE(configKey.examples.size() >= 1);
    REQUIRE(configKey.examples[0] == "config.tadah");

    // Validate CLI-specific fields.
    REQUIRE(configKey.cli_long_flag == "--config");
    REQUIRE(configKey.cli_short_flag == "-c");
  }

  SECTION("The EMBF0 key: STRING, default ZERO, no CLI flag")
  {
    auto it = blueprintMap.find("EMBF0");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &k = it->second;
    REQUIRE(k.value_type == std::vector<std::string>{"STRING"});
    REQUIRE(k.value_N == std::vector<int>{1});
    REQUIRE(k.default_value == std::vector<std::string>{"ZERO"});
    REQUIRE(k.examples == std::vector<std::string>{"ZERO", "FREE"});
    REQUIRE(k.cli_long_flag.empty());
    REQUIRE(k.cli_short_flag.empty());
  }

  SECTION("The VERBOSE key is parsed correctly")
  {
    // The VERBOSE key should be present (converted to uppercase).
    auto it = blueprintMap.find("VERBOSE");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &verboseKey = it->second;

    // Validate expected fields.
    REQUIRE(verboseKey.value_type == std::vector<std::string>{"UINT"});
    REQUIRE(verboseKey.value_format == std::vector<std::string>{"<unsigned integer>"});
    REQUIRE(verboseKey.value_N == std::vector<int>{1});
    REQUIRE(verboseKey.description ==
            "Verbosity level. 0-2: ERROR, WARNING, INFO.");
    REQUIRE(verboseKey.examples == std::vector<std::string>{"2"});

    // Validate default value and CLI options.
    REQUIRE(verboseKey.default_value[0] == "1");
    REQUIRE(verboseKey.cli_long_flag == "--verbose");
    REQUIRE(verboseKey.cli_short_flag == "-v");
  }

  SECTION("The BIAS key is parsed correctly")
  {
    // The BIAS key should be present.
    auto it = blueprintMap.find("BIAS");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &biasKey = it->second;

    // Validate expected fields.
    REQUIRE(biasKey.value_type == std::vector<std::string>{"BOOL"});
    REQUIRE(biasKey.value_format == std::vector<std::string>{"<boolean>"});
    REQUIRE(biasKey.value_N == std::vector<int>{1});
    REQUIRE(biasKey.description.find("Per-species intercept") != std::string::npos);
    REQUIRE(biasKey.examples == std::vector<std::string>{"true", "false"});

    // Validate that the default value (if defined) is as expected.
    REQUIRE(biasKey.default_value[0] == "false");
  }

  SECTION("The OALGO key is parsed correctly")
  {
    // The OALGO key should be present.
    auto it = blueprintMap.find("OALGO");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &oalgoKey = it->second;
    // Validate the value type list.
    std::vector<std::string> expectedValueType = {"INT", "INT DOUBLE"};
    REQUIRE(oalgoKey.value_type == expectedValueType);

    // Validate the expected format list and constraints.
    REQUIRE(oalgoKey.value_format == std::vector<std::string>{"<option>", "<option> <value>"});
    REQUIRE(oalgoKey.value_N == std::vector<int>{-2});

    // Verify that the description contains key text.
    REQUIRE(oalgoKey.description.find("This key controls the optimisation algorithm") != std::string::npos);

    // Verify that example usage strings are present.
    REQUIRE(oalgoKey.examples.size() >= 5);
  }

  SECTION("The NUMERIC key is parsed correctly")
  {
    // The NUMERIC key should be present.
    auto it = blueprintMap.find("NUMERIC");
    REQUIRE(it != blueprintMap.end());
    const BlueprintKey &numericKey = it->second;

    // Validate expected fields.
    REQUIRE(numericKey.value_type == std::vector<std::string>{"UINT"});
    REQUIRE(numericKey.value_format == std::vector<std::string>{"<unsigned integer>"});
    REQUIRE(numericKey.value_N == std::vector<int>{1});
    REQUIRE(numericKey.description ==
            "Numeric output precision.");
    REQUIRE(numericKey.examples == std::vector<std::string>{"18"});
    
    // Validate CLI-specific fields.
    REQUIRE(numericKey.cli_long_flag == "--numeric");
    REQUIRE(numericKey.default_value[0] == "18");
    // A default-inserted NUMERIC must never lower potential-file precision
    // below round-trip (resolve_pot_digits treats any stored value as
    // explicit, so the default has to be lossless on its own).
    REQUIRE(std::stoi(numericKey.default_value[0]) >= 17);
  }

  SECTION("The blueprint map contains only ALLOWED_KEYS entries")
  {
    // Keys not defined under ALLOWED_KEYS (e.g. TASKS) should not be present.
    // However, only ALLOWED_KEYS are processed.
    // Similarly, the GLOBALS section should not appear.
    REQUIRE(blueprintMap.find("GLOBALS") == blueprintMap.end());
  }
}

/* Pins: CHECK (`tadah hpo --check`, CHECK true in a configuration file) is a
 * boolean key with default false and the CLI flag --check, without a short
 * flag, and the only blueprint task with it is hpo: the CLI builds the
 * option of `tadah hpo` from it, and the CLI reference and the
 * configuration-keys page describe it from its description.
 * Before the key: the blueprint has no CHECK. */
TEST_CASE("BlueprintParser: CHECK is the --check flag of tadah hpo alone",
          "[BlueprintParser]")
{
  const auto &keys = BlueprintParser::getKeysMapRef();
  const auto it = keys.find("CHECK");
  REQUIRE(it != keys.end());
  const BlueprintKey &k = it->second;
  CHECK(k.value_type == std::vector<std::string>{"BOOL"});
  CHECK(k.value_N == std::vector<int>{1});
  CHECK(k.default_value == std::vector<std::string>{"false"});
  CHECK(k.cli_long_flag == "--check");
  CHECK(k.cli_short_flag.empty());
  CHECK_FALSE(k.description.empty());
  CHECK_FALSE(k.longDescription.empty());

  std::vector<std::string> tasks_with_check;
  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    for (const auto &key : kv.second.blueprintKeys) {
      const auto dot = key.keyName.rfind('.');
      const std::string name =
          dot == std::string::npos ? key.keyName : key.keyName.substr(dot + 1);
      std::string upper;
      for (char c : name)
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      if (upper == "CHECK") {
        tasks_with_check.push_back(kv.first);
        CHECK(key.cli_long_flag == "--check");
        CHECK(key.default_value == std::vector<std::string>{"false"});
      }
    }
  }
  CHECK(tasks_with_check == std::vector<std::string>{"HPO"});
}

/* Pins: the help of CONFIG (`tadah explain CONFIG` prints its long
 * description; the configuration-keys page shows its description and long
 * description) states what -c names ("CONFIG names the configuration
 * file") and the rules of an include: the configuration is the
 * concatenation of its lines ("concatenation", "in place of the CONFIG
 * line"); a key of one value "other than CONFIG" is set once ("set once"),
 * a second line refused "naming both lines", and so are the lines of a file
 * "included twice"; a setting that differs between the files that include
 * a shared file "goes in each of them"; the lines of a key of several
 * values add up ("DBFILE" lists the datasets of every line); a line that a
 * run does not read sets nothing ("does not read"); a relative path, of a
 * CONFIG line and in an included file, is relative to the "working
 * directory", "not to the directory of the file" that holds it; a file
 * that "includes itself" is refused.
 * Before the fix: CONFIG has no long description (its help is "Path to a
 * configuration file."): every check of the text fails. Until the help
 * opened with a sentence and named the file included twice, the shared
 * setting and CONFIG's own count, "CONFIG names the configuration file",
 * "other than CONFIG", "included twice" and "goes in each of them" were
 * not found. */
TEST_CASE("BlueprintParser: the help of CONFIG states the rules of an "
          "include and the base of its paths",
          "[BlueprintParser][config_include]")
{
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("CONFIG");
  const std::string text = k.description + "\n" + k.longDescription;
  INFO("CONFIG help:\n" << text);
  // The help is wrapped in the blueprint: compare with single blanks.
  std::string flat;
  for (char c : text) {
    const char b = std::isspace(static_cast<unsigned char>(c)) ? ' ' : c;
    if (b != ' ' || (!flat.empty() && flat.back() != ' ')) flat += b;
  }
  for (const char *phrase :
       {"CONFIG names the configuration file", "concatenation",
        "in place of the CONFIG line", "other than CONFIG", "set once",
        "naming both lines", "included twice", "goes in each of them",
        "DBFILE", "does not read", "working directory",
        "not to the directory of the file", "includes itself"}) {
    INFO("phrase: " << phrase);
    CHECK(flat.find(phrase) != std::string::npos);
  }
}

/**
 * @brief Tests that an empty map returns an empty string when searching for a closest key.
 */
TEST_CASE("BlueprintParser::findClosestKey with empty map", "[findClosestKey]")
{
  std::unordered_map<std::string, BlueprintKey> emptyMap;
  std::string result = BlueprintParser::findClosestKey("ANYKEY", emptyMap);

  REQUIRE(result.empty());
}

/**
 * @brief Tests that an exact match returns the exact key if the key exists in the map.
 */
TEST_CASE("BlueprintParser::findClosestKey with exact match", "[findClosestKey]")
{
  BlueprintKey dummyKey;
  std::unordered_map<std::string, BlueprintKey> map{
    {"ALLOWED_KEYS.CONFIG", dummyKey},
    {"INTERNAL_KEYS.DEBUG", dummyKey}
  };

  std::string result = BlueprintParser::findClosestKey("ALLOWED_KEYS.CONFIG", map);
  REQUIRE(result == "ALLOWED_KEYS.CONFIG");
}

/**
 * @brief Tests that the method chooses the key with the smallest edit distance when multiple keys exist.
 *
 * Uses a situation where "ALOWED_KEYS.CONFIG" is closer to "ALLOWED_KEYS.CONFIG" than to "ALLOWED_KEYS.CONFING".
 */
TEST_CASE("BlueprintParser::findClosestKey chooses the minimum edit distance", "[findClosestKey]")
{
  BlueprintKey dummyKey;
  std::unordered_map<std::string, BlueprintKey> map{
    {"ALLOWED_KEYS.CONFIG", dummyKey},
    {"ALLOWED_KEYS.CONFING", dummyKey},
    {"SOME_OTHER_KEY", dummyKey}
  };

  std::string result = BlueprintParser::findClosestKey("ALOWED_KEYS.CONFIG", map);
  REQUIRE(result == "ALLOWED_KEYS.CONFIG");
}

/**
 * @brief Tests behavior when the user input is very different from the available keys.
 *
 * Ensures that some non-empty string is still returned as the best match when the map is not empty.
 */
TEST_CASE("BlueprintParser::findClosestKey with large distance key", "[findClosestKey]")
{
  BlueprintKey dummyKey;
  std::unordered_map<std::string, BlueprintKey> map{
    {"ALLOWED_KEYS.CONFIG", dummyKey},
    {"INTERNAL_KEYS.DEBUG", dummyKey}
  };

  std::string result = BlueprintParser::findClosestKey("COMPLETELY_UNRELATED_KEY", map);

  // A non-empty result is expected since at least one key is in the map.
  REQUIRE_FALSE(result.empty());
}


/* Pins: every example of every key of the blueprint (ALLOWED_KEYS and
 * INTERNAL_KEYS) is a value of that key -- what follows the key on its
 * configuration line -- and an example of several lines is one value per
 * line: every non-blank line passes the key's format (BlueprintValidator on
 * its value_type, as the configuration parser checks a value), is one of
 * the key's allowed_values when it has a list, and does not begin with the
 * key itself or its command-line flag. CoreUserConfigParser shows each line
 * of an example as "<KEY>   <line>" under "Examples:" when it refuses a
 * value, and the configuration-keys page of the docs shows it the same way,
 * so the key is written once.
 * The rule was: a value, or a configuration line that starts with the key
 * or its command-line flag (possibly followed by more lines); the parser
 * printed the key twice for the latter.
 * Before the fix: 64 examples of 36 keys began with the key (BETA's
 * "BETA 0.0001", printed "BETA   BETA 0.0001"; dbfile's
 * "dbfile /path/to/dbfile"; the D2_mJoin block "TYPE2B    D2_mJoin" /
 * "TYPE2B    D2_MIE 11 6 Ti Ti" / ...; the refit blocks' "INIT / PHI XMIN
 * PIN / ENDINIT"), and IN_PLACE's was its flag, "--in-place". */
TEST_CASE("Every blueprint example is a value of its key, one per line",
          "[BlueprintParser]") {
  const auto upper = [](std::string t) {
    for (char &ch : t) ch = static_cast<char>(std::toupper(
                           static_cast<unsigned char>(ch)));
    return t;
  };
  std::size_t checked = 0;
  for (const auto *map : {&BlueprintParser::getKeysMapRef(),
                          &BlueprintParser::getInternalKeysMapRef()}) {
    for (const auto &kv : *map) {
      const BlueprintKey &k = kv.second;
      const BlueprintValidator validator(k.value_type);
      for (const std::string &example : k.examples) {
        INFO("key " << kv.first << ", example:\n" << example);
        std::istringstream lines(example);
        std::string line;
        std::size_t values = 0;
        while (std::getline(lines, line)) {
          std::istringstream words(line);
          std::string first;
          if (!(words >> first)) continue;   /* a blank line */
          ++values;
          ++checked;
          INFO("line '" << line << "'");
          CHECK(upper(first) != kv.first);
          CHECK((k.cli_long_flag.empty() || first != k.cli_long_flag));
          CHECK((k.cli_short_flag.empty() || first != k.cli_short_flag));
          CHECK(validator.validate(line));
          const auto b = line.find_first_not_of(" \t\r");
          const auto e = line.find_last_not_of(" \t\r");
          CHECK(k.isAllowedValue(line.substr(b, e - b + 1)));
        }
        CHECK(values > 0);
      }
    }
  }
  CHECK(checked > 200);
}
