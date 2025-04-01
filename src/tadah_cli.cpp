#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/mlip/all_engines.h>
#include <tadah/mlip/task.h>
#include <tadah/mlip/mlip_user_config_parser.h>
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
    core::Logger::instance().setLogLevel(core::LogLevel::INFO);

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
    auto subCmds = app_.get_subcommands();
    std::cout << "Chosen subcommands size: " << subCmds.size() << std::endl;
    if (subCmds.empty()) {
      std::cout << "No command/subcommand chosen. Use '--help' for details.\n";
      return 0;
    }
    CLI::App *cmd = subCmds.front();

    if (!cmd->get_subcommands().empty()) {
      // If there are subcommands, choose the first one
      cmd = cmd->get_subcommands().front();
    }

    // Retrieve subcommand context
    auto it = cliTasks_.find(cmd);
    CLITask &cliTask = it->second;
    std::cout << "Command Name: " << cliTask.blueprintTaskName << std::endl;

    // Build a Context from user CLI tokens
    tadah::core::Context cliCtx = buildContextFromCLI(cmd);
    std::cout << "Context:\n" << cliCtx << std::endl;

    // Check if a user config file is provided
    if (cliTask.storage.find(cliTask.blueprintTaskName+".config") != cliTask.storage.end()) {
      std::string userConfigFile = cliTask.storage[cliTask.blueprintTaskName+".config"].front();
      std::cout << "User config file: " << userConfigFile << std::endl;
      // Parse the user config file
      tadah::mlip::MLIPUserConfigParser parser;
      auto contexts = parser.parse(userConfigFile, true);
      if (contexts.empty()) {
        throw std::runtime_error("No valid sections found in the user config file.");
      }
      std::cout << "Parsed user config file into " << contexts.size() << " sections.\n";
      for (const auto &ctx : contexts) {
        std::cout << "Section:\n" << ctx << std::endl;
      }
      // Merge the first context with the CLI context
      // cliCtx = parser.merge(contexts[0], cliCtx);
    }
    tadah::core::Context mergedCtx = cliCtx;

    // Create the engine. e.g. "data.split"
    auto eng = tadah::mlip::EngineFactory::create(cliTask.blueprintTaskName);

    // Validate + run
    eng->validate(mergedCtx);
    int code = eng->run(mergedCtx);
    tadah::core::Logger::instance().log(
        tadah::core::LogLevel::INFO,
        "Engine for task '", cliTask.blueprintTaskName, "' finished with code=", code);

  } catch (const CLI::ParseError &pe) {
    return app_.exit(pe);
  }
  return 0;
}

void TadahCLI::registerModules() {
  using tadah::core::BlueprintParser;
  const auto &tasksMap = BlueprintParser::getTasksMapRef();

  for (const auto &pair : tasksMap) {
    std::string taskName = toLower(pair.first);
    const auto &bpTask       = pair.second;

    std::vector<std::string> tokens;
    boost::split(tokens, taskName, boost::is_any_of("."));

    if (tokens.size() == 1) {
      // top-level
      CLI::App *cmd = createTopLevelCommand(tokens[0], bpTask.description);

      buildCLITask(cmd, bpTask);

      cliTasks_[cmd].blueprintTaskName = taskName;
    }
    else if (tokens.size() == 2) {
      CLI::App *parent = createTopLevelCommand(tokens[0], "");
      CLI::App *child = parent->add_subcommand(tokens[1], bpTask.description);

      buildCLITask(child, bpTask);

      cliTasks_[child].blueprintTaskName = taskName;
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

  return newCmd;
}

void TadahCLI::buildCLITask(CLI::App *cmd,
    const tadah::core::BlueprintTask &bpTask) {
  // track duplicates in this subcommand
  // std::unordered_set<std::string> usedSpecs;

  auto &cliTask = cliTasks_[cmd];

  for (auto &bpKey : bpTask.keyValuePairs) {
    if (bpKey.cli_long_flag.empty()) {
      continue; 
    }
    std::string spec = bpKey.cli_long_flag;

    // if cli_short_flag is present, append with comma
    if (!bpKey.cli_short_flag.empty()) {
      spec += "," + bpKey.cli_short_flag;
    }


    // We'll store user tokens under a key: e.g. toLower(bpKey.keyName)
    // or just use bpKey.keyName as is, e.g. "DATA.SPLIT.KEY"? 
    // We'll do a simple approach: keyName -> vector
    std::string storeKey = toLower(bpKey.keyName);
    //cliTask.storage[storeKey];
    auto &dest = cliTask.storage[storeKey];

    CLI::Option *opt = cmd->add_option(spec, dest, bpKey.description);
    if (bpKey.cli_required) {
      opt->required();
    }
    // excludes logic if needed
  }
}

tadah::core::Context TadahCLI::buildContextFromCLI(CLI::App *sub) {
  tadah::core::Context c;
  auto it = cliTasks_.find(sub);
  if (it == cliTasks_.end()) {
    return c;
  }
  auto &cliTask = it->second;
  // copy each key->vector of tokens into the Context
  for (auto &kv : cliTask.storage) {
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

