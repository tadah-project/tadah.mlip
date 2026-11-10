// The blueprint field `retired` (BlueprintKey::retired) and the rules
// BlueprintParser enforces for it; the keys retired in 1.6.0 and the error
// scales that replace them. Parser behaviour for retired keys in files is
// in test_retired_keys.cpp.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <toml.hpp>
#include <vector>

using namespace tadah::core;
using Catch::Matchers::Contains;

namespace {

const std::string kNoExamples = "examples = []\n";

const std::string kRetired =
    "[KEY.retired]\n"
    "reason = \"It weighted the data.\"\n"
    "replacement = \"OTHER\"\n"
    "conversion = \"OTHER = 1/sqrt(KEY).\"\n";

/* One key table in the form the blueprint uses after INHERIT expansion,
 * followed by @p tables (sub-tables of KEY, such as [KEY.retired]). */
toml::value key_table(const std::string &extra_fields,
                      const std::string &examples = "examples = [\"20\"]\n",
                      const std::string &tables = "") {
  const std::string text = "[KEY]\n"
                           "value_type = [\"DOUBLE\"]\n"
                           "value_format = [\"<double>\"]\n"
                           "value_N = [1]\n"
                           "description = \"A key.\"\n" +
                           examples + extra_fields + tables;
  std::istringstream in(text);
  return toml::find(toml::parse(in), "KEY");
}

/* The keys of task @p task (its own and its groups'), upper case, the
 * last part of their blueprint names. */
std::vector<std::string> task_keys(const std::string &task) {
  const auto &t = BlueprintParser::getTasksMapRef().at(task);
  std::vector<std::string> out;
  auto add = [&](const BlueprintKey &k) {
    const auto dot = k.keyName.rfind('.');
    std::string name =
        dot == std::string::npos ? k.keyName : k.keyName.substr(dot + 1);
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    out.push_back(name);
  };
  for (const auto &k : t.blueprintKeys) add(k);
  for (const auto &g : t.groups)
    for (const auto &k : g.blueprintKeys) add(k);
  return out;
}

bool has(const std::vector<std::string> &v, const std::string &s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

}  // namespace

TEST_CASE("Blueprint: EWEIGHT, FWEIGHT, SWEIGHT, WDBFILE_AUTO and "
          "EWEIGHT_TEMP are retired, with the reason, the replacement and "
          "the conversion",
          "[BlueprintParser][retired]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string key :
       {"EWEIGHT", "FWEIGHT", "SWEIGHT", "WDBFILE_AUTO", "EWEIGHT_TEMP"}) {
    INFO(key);
    const BlueprintKey &k = keys.at(key);
    REQUIRE(k.isRetired());
    CHECK_FALSE(k.isDeprecated());
    CHECK_FALSE(k.retired.reason.empty());
    CHECK_FALSE(k.retired.replacement.empty());
    CHECK_FALSE(k.retired.conversion.empty());
    /* No potential written after the retirement carries the key: it has
     * no default Context::contains() would insert, and no page or error
     * message shows how to use it. */
    CHECK(k.default_value.empty());
    CHECK(k.examples.empty());
  }
  /* The conversions of the fit's objective. A potential of Tadah! 1.0.0
   * to 1.5.0 has FWEIGHT and SWEIGHT lines with FORCE and STRESS false
   * too: deleting such a line converts it (a fit refuses an FSIGMA or
   * SSIGMA other than the default for a quantity that is off). 1.5.0
   * multiplied WDBFILE by n_d^-a. */
  CHECK_THAT(keys.at("FWEIGHT").retired.conversion,
             Contains("FSIGMA = 1/sqrt(FWEIGHT)") && Contains("FORCE false") &&
                 Contains("With FORCE false, deleting the line converts it"));
  CHECK_THAT(keys.at("SWEIGHT").retired.conversion,
             Contains("SSIGMA = 160.2/sqrt(SWEIGHT)") &&
                 Contains("diagonal") && Contains("STRESS false") &&
                 Contains("With STRESS false, deleting the line converts "
                          "it"));
  CHECK_THAT(keys.at("EWEIGHT").retired.conversion,
             Contains("ESIGMA = 1000/(N sqrt(EWEIGHT))") &&
                 Contains("N^2") && Contains("ENERGY false"));
  CHECK_THAT(keys.at("WDBFILE_AUTO").retired.conversion,
             Contains("Each WDBFILE value (1 for every dataset without "
                      "WDBFILE) multiplied by n_d^-a") &&
                 Contains("WDBFILE_AUTO 0, deleting the line converts it"));
  CHECK_THAT(keys.at("EWEIGHT_TEMP").retired.replacement, Contains("WDBFILE"));
  /* The command-line options stay, to give the conversion. */
  CHECK(keys.at("WDBFILE_AUTO").cli_long_flag == "--wdbfile-auto");
  CHECK(keys.at("EWEIGHT_TEMP").cli_long_flag == "--eweight-temp");
  /* An ordinary key is not retired. */
  CHECK_FALSE(keys.at("LAMBDA").isRetired());
  CHECK_FALSE(keys.at("MBLOCK").isRetired());
}

/* Pins: the conversion of WDBFILE_AUTO says where the WDBFILE values of a
 * converted potential go (a retrain does not read a potential's WDBFILE
 * line: on the command line or into a file that includes the potential
 * with CONFIG); those of EWEIGHT and WDBFILE_AUTO give the conversion
 * under M_KRR implementation 2, whose solve of Tadah! 1.5.0, (K + LAMBDA
 * I) w = t on the energies per atom in eV, read neither: deleting the
 * line, with ESIGMA 1000 (sigma = 1 eV) or LAMBDA times 10^6 with ESIGMA
 * 1.
 * Before the fix: the conversion of WDBFILE_AUTO left the WDBFILE values
 * in the potential, where a retrain ignores them, and neither conversion
 * named implementation 2, under which both changed the fit. */
TEST_CASE("Blueprint: the conversions of EWEIGHT and WDBFILE_AUTO under "
          "M_KRR implementation 2, and the WDBFILE values of a potential",
          "[BlueprintParser][retired]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  CHECK_THAT(keys.at("WDBFILE_AUTO").retired.conversion,
             Contains("A retrain does not read a potential's WDBFILE line") &&
                 Contains("go on the command line (--wdbfile) or into a "
                          "file that includes the potential with CONFIG"));
  for (const std::string key : {"EWEIGHT", "WDBFILE_AUTO"}) {
    INFO(key);
    CHECK_THAT(keys.at(key).retired.conversion,
               Contains("Under M_KRR implementation 2 (MODEL M_KRR <kernel> "
                        "2)") &&
                   Contains("deleting the line converts it, and ESIGMA "
                            "1000, or LAMBDA times 10^6 with ESIGMA 1, gives "
                            "the solve of Tadah! 1.5.0, (K + LAMBDA I) w = "
                            "t."));
  }
}

TEST_CASE("Blueprint: the error scales ESIGMA, FSIGMA, SSIGMA, the energy "
          "norm ENORM and the energy switch ENERGY, with their defaults",
          "[BlueprintParser][retired]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  CHECK(keys.at("ESIGMA").default_value == std::vector<std::string>{"1"});
  CHECK(keys.at("FSIGMA").default_value ==
        (std::vector<std::string>{"0.1", "0"}));
  CHECK(keys.at("SSIGMA").default_value ==
        (std::vector<std::string>{"0.5", "0"}));
  CHECK(keys.at("ENORM").default_value == std::vector<std::string>{"ATOM"});
  CHECK(keys.at("ENORM").allowed_values ==
        (std::vector<std::string>{"ATOM", "SQRT"}));
  CHECK(keys.at("ENERGY").default_value == std::vector<std::string>{"true"});
  for (const std::string key : {"ESIGMA", "FSIGMA", "SSIGMA", "ENORM",
                                "ENERGY"}) {
    INFO(key);
    CHECK_FALSE(keys.at(key).isRetired());
    CHECK_FALSE(keys.at(key).description.empty());
    CHECK_FALSE(keys.at(key).longDescription.empty());
  }
  /* FSIGMA and SSIGMA take an absolute part and an optional relative one. */
  CHECK(keys.at("FSIGMA").value_N == std::vector<int>{-2});
  CHECK(keys.at("SSIGMA").value_N == std::vector<int>{-2});
  CHECK_THAT(keys.at("ESIGMA").description, Contains("meV/atom"));
  CHECK_THAT(keys.at("FSIGMA").description, Contains("eV/"));
  CHECK_THAT(keys.at("SSIGMA").description, Contains("GPa"));
}

TEST_CASE("BlueprintParser::buildKey reads `retired`",
          "[BlueprintParser][retired]") {
  const BlueprintKey plain = BlueprintParser::buildKey("KEY", key_table(""));
  CHECK_FALSE(plain.isRetired());
  CHECK(plain.retired.reason.empty());

  const BlueprintKey ret = BlueprintParser::buildKey(
      "KEY", key_table("cli_long_flag = \"--key\"\n", kNoExamples, kRetired));
  CHECK(ret.isRetired());
  CHECK(ret.retired.reason == "It weighted the data.");
  CHECK(ret.retired.replacement == "OTHER");
  CHECK(ret.retired.conversion == "OTHER = 1/sqrt(KEY).");
  /* The format is kept (the value of a retired key is still checked), and
   * so is a CLI flag, which gives the conversion. */
  CHECK(ret.value_type == std::vector<std::string>{"DOUBLE"});
  CHECK(ret.cli_long_flag == "--key");
}

TEST_CASE("BlueprintParser::buildKey rejects an inconsistent retired key",
          "[BlueprintParser][retired]") {
  SECTION("a default value would be inserted by Context::contains()") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", key_table("default_value = [\"1\"]\n", kNoExamples,
                             kRetired)),
        Contains("Retired key KEY must not define default_value"));
  }
  SECTION("examples would show how to use a retired key") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey("KEY", key_table("", "examples = [\"1\"]\n",
                                                   kRetired)),
        Contains("Retired key KEY must not define examples"));
  }
  SECTION("a key is deprecated or retired, not both") {
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey(
            "KEY", key_table("deprecated = \"Gone.\"\n", kNoExamples,
                             kRetired)),
        Contains("KEY") && Contains("deprecated") && Contains("retired"));
  }
  SECTION("`retired` is a table of three non-empty strings") {
    for (const std::string bad :
         {std::string("retired = \"Gone.\"\n"),
          std::string("retired = { reason = \"R\", replacement = \"P\" }\n"),
          std::string("retired = { reason = \"\", replacement = \"P\", "
                      "conversion = \"C\" }\n"),
          std::string("retired = { reason = \"R\", replacement = \" \", "
                      "conversion = \"C\" }\n"),
          std::string("retired = { reason = \"R\", replacement = \"P\", "
                      "conversion = 1 }\n"),
          std::string("retired = { reason = \"R\", replacement = \"P\", "
                      "conversion = \"C\", extra = \"E\" }\n")}) {
      INFO(bad);
      REQUIRE_THROWS_WITH(
          BlueprintParser::buildKey("KEY", key_table(bad, kNoExamples)),
          Contains("'retired'") && Contains("KEY"));
    }
  }
}

TEST_CASE("BlueprintParser::findClosestKey never suggests a retired key",
          "[findClosestKey][retired]") {
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string typo :
       {"EWEIGHTS", "FWEIGT", "SWEIGH", "WDBFILE_AUT", "EWEIGHT_TMP"}) {
    INFO(typo);
    const std::string suggested = BlueprintParser::findClosestKey(typo, keys);
    CHECK_FALSE(suggested.empty());
    CHECK_FALSE(keys.at(suggested).isRetired());
  }
}

/* Pins: tadah data balance shifts energies only. --rescale replaced the
 * priorities of a data file by inverse magnitudes (discontinuous at its
 * threshold); it and the options that only served it, --threshold,
 * --force and --stress, are retired options of data balance (no default,
 * no example; a command line that gives one is refused with the reason).
 * The shared keys stay where other commands use them.
 * Before the fix: the four were no options of data balance, so the command
 * line refused them as unknown options. */
TEST_CASE("Blueprint: tadah data balance shifts energies only; analysis "
          "bfunc --rescale and data dedup --threshold stay",
          "[BlueprintParser][retired]") {
  const auto balance = task_keys("DATA.balance");
  const auto &task = BlueprintParser::getTasksMapRef().at("DATA.balance");
  for (const std::string gone : {"rescale", "threshold", "force", "stress"}) {
    INFO(gone);
    const BlueprintKey *k = nullptr;
    for (const BlueprintKey &key : task.blueprintKeys)
      if (key.keyName == "DATA.balance." + gone) k = &key;
    REQUIRE(k != nullptr);
    CHECK(k->isRetired());
    CHECK(k->default_value.empty());
    CHECK(k->examples.empty());
    CHECK_THAT(k->retired.conversion,
               Contains("tadah data balance shifts energies only"));
  }
  for (const std::string kept : {"ESHIFT", "OUTFILE", "MERGE", "INDEX",
                                 "APPEND", "NUMERIC", "DBFILE"}) {
    INFO(kept);
    CHECK(has(balance, kept));
  }
  CHECK(has(task_keys("ANALYSIS.bfunc"), "RESCALE"));
  CHECK(has(task_keys("DATA.dedup"), "THRESHOLD"));
  const auto &t = BlueprintParser::getTasksMapRef().at("DATA.balance");
  for (const std::string &ex : t.examples) {
    INFO(ex);
    CHECK(ex.find("--rescale") == std::string::npos);
  }
  CHECK(t.longDescription.find("rescal") == std::string::npos);
}

/* Pins: `retired` has no field only_with: a retired table with it (or
 * any other field than reason, replacement, conversion and by_deletion) is
 * refused, naming 'retired' and the key; the message lists the fields
 * without only_with.
 * Before the fix: only_with, the retired option of the same command
 * without which a retired option had no effect (tadah data balance
 * --force acted only with --rescale), was an optional field for the lines
 * of the GLOBAL section of a task file: buildKey read
 * only_with = "other" (no exception), and its message named only_with. */
TEST_CASE("BlueprintParser::buildKey refuses a field only_with of `retired`",
          "[BlueprintParser][retired]") {
  for (const std::string bad :
       {std::string("only_with = \"other\"\n"),
        std::string("only_with = \"other\"\nby_deletion = true\n"),
        std::string("with = \"other\"\n")}) {
    INFO(bad);
    std::string msg;
    try {
      (void)BlueprintParser::buildKey(
          "KEY", key_table("", kNoExamples, kRetired + bad));
    } catch (const std::runtime_error &e) {
      msg = e.what();
    }
    CHECK_THAT(msg, Contains("'retired'") && Contains("by_deletion") &&
                        Contains("KEY"));
    CHECK(msg.find("only_with") == std::string::npos);
  }
}

/* Pins: the optional field by_deletion of `retired` says that deleting a
 * line of the key converts it (nothing replaces the key: UNCERTAINTY,
 * WCOV, WNOISE, ERROR), which a fit's refusal says of each such line.
 * buildKey reads it; a retired key without it is not converted by
 * deletion; a value that is not true or false is refused, naming the
 * field and the key.
 * Before the fix: there was no by_deletion, and a table with it was
 * refused as a fourth field. */
TEST_CASE("BlueprintParser::buildKey reads by_deletion of `retired`",
          "[BlueprintParser][retired]") {
  const BlueprintKey del = BlueprintParser::buildKey(
      "KEY", key_table("", kNoExamples, kRetired + "by_deletion = true\n"));
  CHECK(del.isRetired());
  CHECK(del.retired.by_deletion);
  CHECK_FALSE(BlueprintParser::buildKey(
                  "KEY", key_table("", kNoExamples,
                                   kRetired + "by_deletion = false\n"))
                  .retired.by_deletion);
  CHECK_FALSE(BlueprintParser::buildKey("KEY",
                                        key_table("", kNoExamples, kRetired))
                  .retired.by_deletion);
  for (const std::string bad :
       {std::string("by_deletion = \"true\"\n"),
        std::string("by_deletion = 1\n")}) {
    INFO(bad);
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildKey("KEY",
                                  key_table("", kNoExamples, kRetired + bad)),
        Contains("'retired'") && Contains("by_deletion") && Contains("KEY"));
  }
}

/* Pins: the optional field unconverted of `retired` says that no key
 * converts a line of the key and deleting the line changes the fit
 * (EWEIGHT_TEMP), which a fit's refusal says of each such line (the
 * retired_keys_refusal cases of test_retired_keys.cpp). buildKey reads
 * unconverted = true and false; a value that is not true or false, and
 * unconverted = true with by_deletion = true (a deletion that converts the
 * line and one that changes the fit), are refused, naming the field and
 * the key.
 * Before the fix: there was no unconverted, and a table with it was
 * refused as a fourth field: the first two checks fail. */
TEST_CASE("BlueprintParser::buildKey reads unconverted of `retired`",
          "[BlueprintParser][retired]") {
  CHECK_NOTHROW(BlueprintParser::buildKey(
      "KEY", key_table("", kNoExamples, kRetired + "unconverted = true\n")));
  CHECK_NOTHROW(BlueprintParser::buildKey(
      "KEY", key_table("", kNoExamples, kRetired + "unconverted = false\n")));
  for (const std::string bad :
       {std::string("unconverted = \"true\"\n"),
        std::string("unconverted = 1\n"),
        std::string("unconverted = true\nby_deletion = true\n")}) {
    INFO(bad);
    std::string msg;
    try {
      (void)BlueprintParser::buildKey(
          "KEY", key_table("", kNoExamples, kRetired + bad));
    } catch (const std::runtime_error &e) {
      msg = e.what();
    }
    CHECK_THAT(msg, Contains("'retired'") && Contains("unconverted") &&
                        Contains("KEY"));
  }
}

/* Pins: the long description of a retired key does not repeat its
 * one-sentence description, which a page and tadah explain show before
 * it.
 * Before the fix: each began with that sentence, so it showed twice. */
TEST_CASE("Blueprint: a retired key's long description does not repeat its "
          "description",
          "[BlueprintParser][retired]") {
  auto trimmed = [](const std::string &s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b);
  };
  for (const auto &[name, k] : BlueprintParser::getKeysMapRef()) {
    if (!k.isRetired()) continue;
    INFO(name);
    const std::string first = k.description.substr(0, 40);
    CHECK(trimmed(k.longDescription).rfind(first, 0) != 0);
  }
}

/* Pins: the SIGMA notice of every load of a potential of Tadah! 1.0.0 to
 * 1.5.0 says that uncertainty output is removed, the errors of those
 * releases not being standard deviations and no calibrated estimate
 * replacing them, and names neither WCOV nor a retrain: no fit writes a
 * weight covariance. Neither the notice nor the help of SIGMA names a
 * version of Tadah! without uncertainty output ("1.6.0").
 * Before the fix: it said that a retrain with OALGO 3 writes WCOV and
 * WNOISE, which tadah predict --error needs, once the potential's retired
 * keys are converted; then that the error bars were too narrow for many
 * structures (those of a development build, not of a release); then that
 * uncertainty output was not part of Tadah! 1.6.0. */
TEST_CASE("Blueprint: the SIGMA notice says that uncertainty output is "
          "removed",
          "[BlueprintParser][retired][uncertainty]") {
  const BlueprintKey &k = BlueprintParser::getKeysMapRef().at("SIGMA");
  REQUIRE(k.isDeprecated());
  CHECK_THAT(k.deprecated,
             Contains("Uncertainty output is removed: the errors of Tadah! "
                      "1.0.0 to 1.5.0 were not standard deviations, and no "
                      "calibrated estimate replaces them."));
  CHECK(k.deprecated.find("1.6.0") == std::string::npos);
  CHECK(k.longDescription.find("1.6.0") == std::string::npos);
  CHECK(k.deprecated.find("too narrow") == std::string::npos);
  CHECK(k.deprecated.find("WCOV") == std::string::npos);
  CHECK(k.deprecated.find("retrain") == std::string::npos);
}

/* Pins: an example of tadah --help that explains an option names an
 * option of its command that is not retired (tadah explain shows the
 * conversion of a retired one, not how to use it), and one that explains a
 * key names a key that is not retired.
 * Before the fix: 'tadah explain data.balance.rescale', an option that
 * data balance does not have (tadah explain printed the text of RESCALE of
 * analysis bfunc). */
TEST_CASE("Blueprint: the examples of tadah --help name no retired key",
          "[BlueprintParser][retired]") {
  const auto &tasks = BlueprintParser::getTasksMapRef();
  const auto &keys = BlueprintParser::getKeysMapRef();
  for (const std::string &ex : BlueprintParser::getBlueprintTadahRef().examples) {
    INFO(ex);
    const std::string prefix = "tadah explain ";
    if (ex.rfind(prefix, 0) != 0) continue;
    std::vector<std::string> tokens;
    std::istringstream path(ex.substr(prefix.size()));
    for (std::string t; std::getline(path, t, '.');) tokens.push_back(t);
    REQUIRE_FALSE(tokens.empty());
    std::string upper = tokens.back();
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (tokens.size() == 1) {
      const auto it = keys.find(upper);
      if (it != keys.end()) CHECK_FALSE(it->second.isRetired());
      continue;
    }
    std::string task = tokens[0];
    std::transform(task.begin(), task.end(), task.begin(), ::toupper);
    for (std::size_t i = 1; i + 1 < tokens.size(); ++i) task += "." + tokens[i];
    const auto t = tasks.find(task);
    REQUIRE(t != tasks.end());
    const BlueprintKey *option = nullptr;
    for (const BlueprintKey &k : t->second.blueprintKeys)
      if (k.keyName == task + "." + tokens.back()) option = &k;
    for (const auto &g : t->second.groups)
      for (const BlueprintKey &k : g.blueprintKeys)
        if (k.keyName == task + "." + tokens.back()) option = &k;
    REQUIRE(option != nullptr);
    CHECK_FALSE(option->isRetired());
  }
}
