// The commands of the blueprint ([TASKS.*]) and their command-line options:
// one command per run, no TASK key and no --task option, no option on a
// command that groups others (tadah data, tadah properties, tadah
// analysis), and every name in an option's cli_excludes and cli_needs an
// option of the same command.

#include "catch2/catch.hpp"

#include <tadah/core/blueprint_parser.h>

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace tadah::core;
using Catch::Matchers::Contains;
using Catch::Matchers::Equals;

namespace {

/* Every option of @p task: its own and those of its groups. */
std::vector<const BlueprintKey *> options_of(const BlueprintTask &task) {
  std::vector<const BlueprintKey *> out;
  for (const BlueprintKey &k : task.blueprintKeys) out.push_back(&k);
  for (const BlueprintKeyGroup &g : task.groups)
    for (const BlueprintKey &k : g.blueprintKeys) out.push_back(&k);
  return out;
}

/* Whether a blueprint command has commands of its own (DATA has
 * DATA.convert, ...). */
bool groups_commands(const std::string &name) {
  const std::string prefix = name + ".";
  for (const auto &kv : BlueprintParser::getTasksMapRef())
    if (kv.first.compare(0, prefix.size(), prefix) == 0) return true;
  return false;
}

/* An option "X.<name>" with the long flag --<name>. */
BlueprintKey option(const std::string &name) {
  BlueprintKey k;
  k.keyName = "X." + name;
  k.cli_long_flag = "--" + name;
  return k;
}

/* @p text parsed as TOML, as the blueprint is. */
toml::value parse_toml(const std::string &text) {
  std::istringstream ss(text);
  return toml::parse(ss);
}

/* The fields of a STRING option with the long flag --<name>. */
std::string option_fields(const std::string &name) {
  return "value_type = [\"STRING\"]\nvalue_format = [\"<string>\"]\n"
         "value_N = [1]\nexamples = [\"a\"]\n"
         "description = \"Option " + name + ".\"\n"
         "cli_long_flag = \"--" + name + "\"\n";
}

/* A blueprint (TOML) of one command, demo, with the options --alpha and
 * --beta and, in its group input, --gamma; @p alpha_fields are more fields
 * of --alpha. */
std::string demo_blueprint(const std::string &alpha_fields) {
  return "[TASKS.demo]\ndescription = \"A command.\"\n"
         "long_description = \"\"\nexamples = [\"tadah demo --alpha a\"]\n"
         "[TASKS.demo.keys.alpha]\n" + option_fields("alpha") + alpha_fields +
         "[TASKS.demo.keys.beta]\n" + option_fields("beta") +
         "[TASKS.demo.groups.input]\ndescription = \"One of:\"\n"
         "[TASKS.demo.groups.input.keys.gamma]\n" + option_fields("gamma");
}

/* A blueprint (TOML) with the [TADAH] table, its mandatory fields, and the
 * option ALLOWED_KEYS.verbose (--verbose); @p more follows [TADAH]. */
std::string tadah_blueprint(const std::string &more) {
  return "[ALLOWED_KEYS.verbose]\n" + option_fields("verbose") +
         "[TADAH]\nlogo = \"Tadah!\"\nversion = \"1.6.0\"\n"
         "website = \"https://tadah.readthedocs.io\"\n"
         "description = \"Tadah!\"\nlong_description = \"\"\n"
         "examples = [\"tadah --version\"]\n" + more;
}

}  // namespace

/* Pins: the blueprint has no configuration key TASK, and no command, nor
 * tadah itself, has an option --task or -t: a run is one command, given on
 * the command line, and a configuration file holds the keys of that
 * command. [TADAH] has no keys table, so tadah alone takes only -h and
 * --version (Tadah.MLIP).
 * Before the fix: ALLOWED_KEYS.task (--task, -t, a file of TASK sections)
 * was a key, inherited by train, predict, hpo, the command groups data,
 * properties and analysis and every command of theirs, and by [TADAH.keys]:
 * every check fails. */
TEST_CASE("Blueprint: no key TASK, and no option --task or -t on a command "
          "or on tadah itself",
          "[BlueprintParser][commands]") {
  CHECK(BlueprintParser::getKeysMapRef().count("TASK") == 0);
  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    for (const BlueprintKey *k : options_of(kv.second)) {
      INFO("command " << kv.first << ", option " << k->keyName);
      CHECK(k->cli_long_flag != "--task");
      CHECK(k->cli_short_flag != "-t");
    }
  }
  const BlueprintTadah &top = BlueprintParser::getBlueprintTadahRef();
  CHECK(top.blueprintKeys.empty());
  CHECK(top.groups.empty());
  for (const std::string &example : top.examples) {
    INFO(example);
    CHECK(example.find("--task") == std::string::npos);
  }
}

/* Pins: a command that groups others (tadah data, tadah properties, tadah
 * analysis) has no option and no option group: it runs nothing itself, and
 * a command line names one of its commands. Its examples run one of them.
 * Before the fix: data and properties had --verbose and --task, analysis
 * --task, and the example of data was `tadah data --task tasks.tadah`. */
TEST_CASE("Blueprint: a command that groups others has no option",
          "[BlueprintParser][commands]") {
  std::size_t groups = 0;
  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    if (!groups_commands(kv.first)) continue;
    ++groups;
    INFO("command " << kv.first);
    CHECK(kv.second.blueprintKeys.empty());
    CHECK(kv.second.groups.empty());
    CHECK_FALSE(kv.second.examples.empty());
    for (const std::string &example : kv.second.examples) {
      INFO(example);
      CHECK(example.find("--task") == std::string::npos);
    }
  }
  CHECK(groups == 3);
}

/* Pins: every name in the cli_excludes and cli_needs of an option of a
 * command of the blueprint is the long flag, without "--", of another
 * option of the same command: the command line excludes or needs exactly
 * that option.
 * Before the fix: fails on the option --task of 21 commands, whose
 * cli_excludes was ["ALL"], a word the command line read as every other
 * option of the command and no option is named. */
TEST_CASE("Blueprint: the cli_excludes and cli_needs of each option name "
          "options of its command",
          "[BlueprintParser][commands]") {
  std::size_t names = 0;
  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    const auto options = options_of(kv.second);
    auto is_option = [&](const BlueprintKey *self, const std::string &n) {
      for (const BlueprintKey *o : options)
        if (o != self && o->cli_long_flag == "--" + n) return true;
      return false;
    };
    for (const BlueprintKey *k : options) {
      for (const auto *list : {&k->cli_excludes, &k->cli_needs})
        for (const std::string &n : *list) {
          INFO("command " << kv.first << ", option " << k->keyName
                          << ", name '" << n << "'");
          CHECK(is_option(k, n));
          ++names;
        }
    }
  }
  CHECK(names > 30);
}

/* Pins: the check of a command's options (checkCommandOptions, on commands
 * built here) refuses a command with an option whose cli_excludes or
 * cli_needs names no other option of the command (no long flag --<name>
 * among its options and those of its groups): the message names the
 * command, the option, the field and the name. A name of an option of a
 * group of the command is accepted, and so is a command whose names are
 * all its options; every command of the blueprint passes. The next case
 * loads a blueprint with such an option.
 * Before the fix: the check of a command's options when the blueprint
 * loads (BlueprintParser::checkRetiredOptions, which checkCommandOptions
 * replaces) read only the field only_with of its retired options: every
 * command passed (no exception), and the command line threw
 * CLI::OptionNotFound for such a name when tadah started. */
TEST_CASE("Blueprint: an option whose cli_excludes or cli_needs names no "
          "option of its command is refused",
          "[BlueprintParser][commands]") {
  BlueprintTask task;
  task.taskName = "X";
  BlueprintKey a = option("alpha");
  a.cli_excludes = {"beta"};
  BlueprintKey b = option("beta");
  b.cli_needs = {"gamma"};
  BlueprintKeyGroup group;
  group.groupName = "input";
  group.blueprintKeys = {option("gamma")};
  task.blueprintKeys = {a, b};
  task.groups = {group};
  CHECK_NOTHROW(BlueprintParser::checkCommandOptions(task));

  for (const bool needs : {false, true}) {
    for (const std::string name : {"delta", "alpha", "ALL"}) {
      INFO((needs ? "cli_needs " : "cli_excludes ") << name);
      BlueprintTask bad = task;
      (needs ? bad.blueprintKeys[0].cli_needs
             : bad.blueprintKeys[0].cli_excludes) = {name};
      REQUIRE_THROWS_WITH(
          BlueprintParser::checkCommandOptions(bad),
          Contains("command X") && Contains("--alpha") &&
              Contains(needs ? "cli_needs" : "cli_excludes") &&
              Contains("'" + name + "'") &&
              Contains("names no other option of the command"));
    }
  }
  BlueprintTask in_group = task;
  in_group.groups[0].blueprintKeys[0].cli_excludes = {"epsilon"};
  REQUIRE_THROWS_WITH(BlueprintParser::checkCommandOptions(in_group),
                      Contains("--gamma") && Contains("'epsilon'"));

  for (const auto &kv : BlueprintParser::getTasksMapRef()) {
    INFO(kv.first);
    CHECK_NOTHROW(BlueprintParser::checkCommandOptions(kv.second));
  }
}

/* Pins: loading a blueprint (BlueprintParser::buildTasksMap, which
 * getTasksMapRef() applies to the embedded one) refuses a command with an
 * option whose cli_excludes or cli_needs names no other option of the
 * command. The blueprint of one command, demo, with the options --alpha,
 * --beta and, in its group input, --gamma: --alpha with no such field, with
 * cli_excludes ["beta"] or with cli_needs ["gamma"] loads (the command DEMO
 * with its 3 options); --alpha with cli_excludes or cli_needs ["delta"] (no
 * option), ["alpha"] (itself) or ["ALL"] is refused with exactly "Error
 * building blueprint tasks map: The option --alpha of the command DEMO has
 * cli_excludes 'delta', which names no other option of the command (an
 * option is named by its long flag without --)" (the field and the name of
 * each).
 * Before the fix: passes as well (a pin of the refusal the load makes:
 * with the check of the commands' options taken out of buildTasksMap, each
 * refused blueprint loads, no exception). */
TEST_CASE("Blueprint: loading a blueprint refuses an option whose "
          "cli_excludes or cli_needs names no option of its command",
          "[BlueprintParser][commands]") {
  for (const std::string fields :
       {"", "cli_excludes = [\"beta\"]\n", "cli_needs = [\"gamma\"]\n"}) {
    INFO("--alpha with " << (fields.empty() ? "no cli field" : fields));
    std::unordered_map<std::string, BlueprintTask> tasks;
    REQUIRE_NOTHROW(tasks = BlueprintParser::buildTasksMap(
                        parse_toml(demo_blueprint(fields))));
    REQUIRE(tasks.count("DEMO") == 1);
    CHECK(options_of(tasks.at("DEMO")).size() == 3);
  }
  for (const std::string field : {"cli_excludes", "cli_needs"}) {
    for (const std::string name : {"delta", "alpha", "ALL"}) {
      INFO("--alpha with " << field << " [\"" << name << "\"]");
      REQUIRE_THROWS_WITH(
          BlueprintParser::buildTasksMap(parse_toml(
              demo_blueprint(field + " = [\"" + name + "\"]\n"))),
          Equals("Error building blueprint tasks map: The option --alpha of "
                 "the command DEMO has " + field + " '" + name +
                 "', which names no other option of the command (an option "
                 "is named by its long flag without --)"));
    }
  }
}

/* Pins: loading a blueprint (BlueprintParser::buildBlueprintTadah, which
 * getBlueprintTadahRef() applies to the embedded one) refuses a keys table
 * of [TADAH]: tadah alone takes no option but -h and --version. A [TADAH]
 * with its mandatory fields and no keys table loads (version "1.6.0", no
 * option); the same with a keys table, [TADAH.keys] INHERIT =
 * ["ALLOWED_KEYS.verbose"] (as [TADAH.keys] INHERIT = ["ALLOWED_KEYS.task"]
 * gave tadah alone its option --task), or [TADAH.keys.verbose] written out,
 * is refused with exactly "[TADAH] must not have a keys table: tadah alone
 * takes no option but -h and --version, and an option belongs to a command
 * ([TASKS.<command>.keys])".
 * Before the fix: passes as well (a pin of the refusal the load makes:
 * with the refusal taken out of buildBlueprintTadah, each keys table loads
 * and is not read, no exception). */
TEST_CASE("Blueprint: loading a blueprint refuses a keys table of [TADAH]",
          "[BlueprintParser][commands]") {
  BlueprintTadah top;
  REQUIRE_NOTHROW(top = BlueprintParser::buildBlueprintTadah(
                      parse_toml(tadah_blueprint(""))));
  CHECK(top.version == "1.6.0");
  CHECK(top.blueprintKeys.empty());
  CHECK(top.groups.empty());
  for (const std::string &keys :
       {std::string("[TADAH.keys]\nINHERIT = [\"ALLOWED_KEYS.verbose\"]\n"),
        "[TADAH.keys.verbose]\n" + option_fields("verbose")}) {
    INFO(keys);
    REQUIRE_THROWS_WITH(
        BlueprintParser::buildBlueprintTadah(
            parse_toml(tadah_blueprint(keys))),
        Equals("[TADAH] must not have a keys table: tadah alone takes no "
               "option but -h and --version, and an option belongs to a "
               "command ([TASKS.<command>.keys])"));
  }
}

namespace {

/* demo_blueprint("") with @p group_fields more fields of its group
 * input. */
std::string demo_blueprint_group(const std::string &group_fields) {
  std::string b = demo_blueprint("");
  const std::string at = "[TASKS.demo.groups.input]\ndescription = \"One of:\"\n";
  const auto pos = b.find(at);
  REQUIRE(pos != std::string::npos);
  b.insert(pos + at.size(), group_fields);
  return b;
}

}  // namespace

/* Pins: loading a blueprint refuses a field of an option group that no
 * code reads: a group takes description, required_min, required_max and
 * its keys. A group with required_min 1 and required_max 2 loads (the
 * group input of DEMO, 1 to 2 options); one with cli_excludes, cli_needs
 * (fields of an option, which the command line applies to options only),
 * cli_required (the group's required_min says what it requires) or a
 * misspelt field (required_mn) is refused with "Error building blueprint
 * tasks map: The group input of the command DEMO has the field <field>,
 * which a group does not take ...". The embedded blueprint loads.
 * Before the fix: each loaded; the group-level cli_excludes and cli_needs
 * were read into the group and applied nowhere, and a field the reader did
 * not know was not read, all without a word. */
TEST_CASE("Blueprint: loading a blueprint refuses a field of an option group "
          "that no code reads",
          "[BlueprintParser][commands][groups]") {
  std::unordered_map<std::string, BlueprintTask> tasks;
  REQUIRE_NOTHROW(tasks = BlueprintParser::buildTasksMap(parse_toml(
                      demo_blueprint_group("required_min = 1\n"
                                           "required_max = 2\n"))));
  REQUIRE(tasks.count("DEMO") == 1);
  REQUIRE(tasks.at("DEMO").groups.size() == 1);
  CHECK(tasks.at("DEMO").groups[0].requiredMin == 1);
  CHECK(tasks.at("DEMO").groups[0].requiredMax == 2);
  for (const std::string field :
       {"cli_excludes = [\"beta\"]", "cli_needs = [\"alpha\"]",
        "cli_required = true", "required_mn = 1"}) {
    INFO("group input with " << field);
    const std::string name = field.substr(0, field.find(' '));
    std::string what;
    try {
      (void)BlueprintParser::buildTasksMap(
          parse_toml(demo_blueprint_group(field + "\n")));
    } catch (const std::exception &e) {
      what = e.what();
    }
    CHECK_THAT(what, Contains("Error building blueprint tasks map: The group "
                              "input of the command DEMO has the field " +
                              name + ", which a group does not take"));
  }
  CHECK_NOTHROW(BlueprintParser::getTasksMapRef());
}

/* Pins: the commands that read a command key (BlueprintKey::command_only)
 * are those whose options or group keys list it, without a command whose
 * table retires it: RESCALE and THRESHOLD, retired options of tadah data
 * balance (refused there with the reason), are read by tadah analysis
 * bfunc and by tadah data dedup alone, and a message names those ("RESCALE
 * is the option --rescale of tadah analysis bfunc"). OPTION, whose
 * cli_long_flag does not start with "-", is the positional argument of
 * tadah explain and is named so ("OPTION is the argument of tadah
 * explain"); ANALYTICS stays the option --analytics (-A) of tadah predict
 * (a pin).
 * Before the fix: tadah data balance was named among the commands that
 * read RESCALE and THRESHOLD, and OPTION was "a key of tadah explain", of a
 * command that reads no configuration file. */
TEST_CASE("Blueprint: a command key is named by the commands that read it, "
          "not by one that retires it, and a positional one as the argument "
          "of its command",
          "[BlueprintParser][commands][command_keys]") {
  CHECK_THAT(commands_reading_key("RESCALE"),
             Equals(std::vector<std::string>{"tadah analysis bfunc"}));
  CHECK_THAT(commands_reading_key("THRESHOLD"),
             Equals(std::vector<std::string>{"tadah data dedup"}));
  CHECK(command_key_text("RESCALE") ==
        "RESCALE is the option --rescale of tadah analysis bfunc");
  CHECK(command_key_text("THRESHOLD") ==
        "THRESHOLD is the option --threshold of tadah data dedup");
  CHECK(command_key_text("OPTION") ==
        "OPTION is the argument of tadah explain");
  CHECK(command_key_text("ANALYTICS") ==
        "ANALYTICS is the option --analytics (-A) of tadah predict");
}

/* Pins: every command key of the blueprint (BlueprintKey::command_only, an
 * option of a command line that no configuration file sets) is read by at
 * least one command: commands_reading_key() of it is not empty, so tadah
 * explain describes no option that no command line takes.
 * Before the fix: fails on QUANTITY (ALLOWED_KEYS.quantity, "Generic
 * quantity.", --quantity), which only the commented stub of tadah
 * properties ecurve listed: tadah explain QUANTITY printed it with
 * placeholder examples. */
TEST_CASE("Blueprint: every command key is read by a command",
          "[BlueprintParser][commands][command_keys]") {
  std::size_t command_keys = 0;
  for (const auto &kv : BlueprintParser::getKeysMapRef()) {
    if (!kv.second.command_only) continue;
    ++command_keys;
    INFO("command key " << kv.first);
    CHECK_FALSE(commands_reading_key(kv.first).empty());
  }
  CHECK(command_keys > 10);
}
