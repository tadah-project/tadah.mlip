#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/mlip/all_engines.h>
#include <tadah/core/utils.h>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <unordered_set>
#include <boost/algorithm/string.hpp>

namespace tadah {
namespace mlip {
namespace cli {

namespace {
  /**
   * @brief Converts uppercase blueprint name to all-lowercase for CLI usage (task names).
   */
  std::string toLower(const std::string &in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
  }
}

TadahCLI::TadahCLI()
  : app_("Tadah CLI - Lowercase tasks/subcommands, preserves case for flags.\n")
{
  registerAllEngines();
  registerModules();
}

int TadahCLI::run(int argc, char** argv) {
  try {
    app_.parse(argc, argv);

    // logic: 
    // 1. if tadah is called without any subcommand, print help;
    //    othewise parse options and run the tadahEngine
    // 2. if a subcommand is called, parse options and run the subcommandEngine (e.g dataEngine)
    // 3. if a subsubcommand is called, parse options and run the subSubcommandEngine (e.g. dataSplitEngine)
    // 4. For selected engine: 
    //    - create CLI context. if -c,--config create context from user file; then merge into single context.
    //      CLI values overwrite config file values.
    //    - validate
    //    - run

    // Identify the chosen command or subcommand
    auto chosenSubs = app_.get_subcommands();
    std::cout << "Chosen subcommands size: " << chosenSubs.size() << std::endl;
    if (chosenSubs.empty()) {
      std::cout << "No command/subcommand chosen. Use '--help' for details.\n";
      return 0;
    }
    CLI::App *cmd = chosenSubs.front();

    if (!cmd->get_subcommands().empty()) {
      // If there are subcommands, choose the first one
      cmd = cmd->get_subcommands().front();
    }

    // Retrieve subcommand context
    auto it = subcommandContexts_.find(cmd);
    SubcommandContext &scx = it->second;
    std::cout << "Subcommand " << scx.blueprintTaskName << std::endl;

    // Build a Context from user CLI tokens
    tadah::core::Context cliCtx = buildContextFromCLI(cmd);
    std::cout << "Context " << cliCtx << std::endl;

    // Parse a config file & merge.
    tadah::core::Context mergedCtx = cliCtx;

    // Create the engine. e.g. "data.split"
    auto eng = tadah::mlip::EngineFactory::create(scx.blueprintTaskName);

    // Validate + run
    eng->validate(mergedCtx);
    int code = eng->run(mergedCtx);
    tadah::core::Logger::instance().log(
      tadah::core::LogLevel::INFO,
      "Engine for task '", scx.blueprintTaskName, "' finished with code=", code);

  } catch (const CLI::ParseError &pe) {
    return app_.exit(pe);
  }
  return 0;
}

void TadahCLI::registerModules() {
  using tadah::core::BlueprintParser;
  const auto &tasksMap = BlueprintParser::getTasksMapRef();

  for (auto &pair : tasksMap) {
    std::string upperTaskName = pair.first;  // e.g. "DATA.SPLIT"
    const auto &taskObj       = pair.second;

    // Convert to all-lower 
    std::string lowerTaskName = toLower(upperTaskName);
    std::vector<std::string> tokens;
    boost::split(tokens, lowerTaskName, boost::is_any_of("."));

    if (tokens.size() == 1) {
      // top-level
      CLI::App *cmd = createTopLevelCommand(tokens[0], taskObj.description);

      buildOptionsForTask(cmd, taskObj);

      // store "data" or "train" in subcommandContexts_ if it is fully callable
      // Actually we want the entire dot name, e.g. "data". If there's no dot, it's just cmd->blueprint
      subcommandContexts_[cmd].blueprintTaskName = lowerTaskName;
    }
    else if (tokens.size() == 2) {
      CLI::App *parent = createTopLevelCommand(tokens[0], "");
      CLI::App *child = parent->add_subcommand(tokens[1], taskObj.description);

      buildOptionsForTask(child, taskObj);

      // store "data.split"
      subcommandContexts_[child].blueprintTaskName = lowerTaskName;
    }
  }
}

CLI::App* TadahCLI::createTopLevelCommand(const std::string &cmdName,
                                                const std::string &desc) {
  auto it = topCommands_.find(cmdName);
  if (it != topCommands_.end()) {
    if (!desc.empty()) {
      it->second->description(desc);
    }
    return it->second;
  }
  CLI::App* newCmd = app_.add_subcommand(cmdName, desc);
  topCommands_[cmdName] = newCmd;

  // also create subcommand context
  subcommandContexts_[newCmd] = SubcommandContext();
  return newCmd;
}

void TadahCLI::buildOptionsForTask(CLI::App *cmd,
                                   const tadah::core::BlueprintTask &task) {
  // track duplicates in this subcommand
  std::unordered_set<std::string> usedSpecs;

  auto &scx = subcommandContexts_[cmd];

  for (auto &bk : task.keyValuePairs) {
    if (bk.cli_long_flag.empty()) {
      continue; 
    }
    // We do NOT transform them to lower or manipulate them
    // e.g. if blueprint says "cli_long_flag=--SomeFlag", we keep it as is
    std::string spec = bk.cli_long_flag;

    // if cli_short_flag is present, append with comma
    if (!bk.cli_short_flag.empty()) {
      spec += "," + bk.cli_short_flag;
    }

    // check duplicates
    if (!usedSpecs.insert(spec).second) {
      throw std::runtime_error("Duplicate spec '" + spec +
        "' in subcommand: " + cmd->get_name());
    }

    // We'll store user tokens under a key: e.g. toLower(bk.keyName)
    // or just use bk.keyName as is, e.g. "DATA.SPLIT.KEY"? 
    // We'll do a simple approach: keyName -> vector
    std::string storeKey = toLower(bk.keyName);
    scx.storage[storeKey];
    auto &dest = scx.storage[storeKey];

    CLI::Option *opt = cmd->add_option(spec, dest, bk.description);
    if (bk.cli_required) {
      opt->required();
    }
    // excludes logic if needed
  }
}

tadah::core::Context TadahCLI::buildContextFromCLI(CLI::App *sub) {
  tadah::core::Context c;
  auto it = subcommandContexts_.find(sub);
  if (it == subcommandContexts_.end()) {
    return c;
  }
  auto &scx = it->second;
  // copy each key->vector of tokens into the Context
  for (auto &kv : scx.storage) {
    std::vector<std::string> tokens;
    boost::split(tokens, kv.first, boost::is_any_of("."));
    const std::string &key = tokens.back();
    for (auto &val : kv.second) {
      c.add<std::string>(core::to_upper(key), val);
    }
  }
  return c;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

