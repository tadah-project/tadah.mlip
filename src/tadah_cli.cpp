#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah.mlip/cli/cli_task.h>
#include <tadah/mlip/task_factory.h>
#include <tadah/mlip/task_manager.h>

#include <boost/algorithm/string.hpp> // for boost::split
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace tadah {
namespace mlip {
namespace cli {

//---------------------------------------------------------------------
// TadahCLI constructor
//---------------------------------------------------------------------
TadahCLI::TadahCLI()
  : app_("Tadah CLI - Dynamically generated from blueprint tasks.\n")
{
  registerModules();
}

//---------------------------------------------------------------------
// run()
//---------------------------------------------------------------------
int TadahCLI::run(int argc, char** argv) {
  try {
    // Parse command-line arguments
    app_.parse(argc, argv);
    tadah::core::Logger::instance().setLogLevel(tadah::core::LogLevel::INFO);

    // Identify chosen subcommands
    auto chosen = app_.get_subcommands();
    // If no subcommand was selected, show help or exit gracefully
    if (chosen.empty()) {
      std::cout << "No command/subcommand chosen. Use '--help' for details.\n";
      return 0;
    }

    // Typically the first subcommand is the chosen top-level, then possibly a sub-subcommand
    CLI::App *cmd = chosen.front();
    if (!cmd->get_subcommands().empty()) {
      cmd = cmd->get_subcommands().front();
    }

    // Retrieve the associated CLITask
    auto it = cliTasks_.find(cmd);
    if (it == cliTasks_.end()) {
      std::cout << "No recognized command. Exiting.\n";
      return 0;
    }

    // At this point, we have a fully-populated CLITask (via CLI11).
    // The higher-level logic might:
    //   1) Pass this CLITask to TaskFactory to create real Task objects
    //   2) Store tasks in TaskManager
    //   3) Possibly run an Engine, etc.

    // We'll just print out some debug info here:
    CLITask &parsedTask = it->second;
    std::cout << "Selected blueprintTaskName: " << parsedTask.blueprintTaskName << std::endl;
    std::cout << "Collected CLI data:\n";
    for (auto &kv : parsedTask.storage) {
      std::cout << "  Key='" << kv.first << "' Values=[";
      for (auto &val : kv.second) {
        std::cout << val << " ";
      }
      std::cout << "]\n";
    }

    auto tasks = TaskFactory().createTasksFromInput(parsedTask);
    TaskManager taskManager;
    taskManager.store(tasks);


    // Return success
    return 0;
  }
  catch (const CLI::ParseError &pe) {
    // Let CLI11 handle parse errors
    return app_.exit(pe);
  }
}

//---------------------------------------------------------------------
// registerModules()
//---------------------------------------------------------------------
void TadahCLI::registerModules() {
  using tadah::core::BlueprintParser;
  const auto &tasksMap = BlueprintParser::getTasksMapRef();
  
  // For each blueprint task, create the necessary CLI subcommands.
  for (const auto &pair : tasksMap) {
    const std::string blueprintName = pair.first;   // e.g. "DATA.SPLIT"
    const tadah::core::BlueprintTask &bpTask = pair.second;

    std::vector<std::string> tokens;
    boost::split(tokens, blueprintName, boost::is_any_of("."));

    // If only one token => top-level command
    if (tokens.size() == 1) {
      CLI::App *topCmd = createTopLevelCommand(core::to_lower(tokens[0]), bpTask.description);
      buildCLITask(topCmd, bpTask);
      // Store blueprint name for later reference
      cliTasks_[topCmd].blueprintTaskName = core::to_upper(blueprintName);
    }
    // If two tokens => top-level + subcommand
    else if (tokens.size() == 2) {
      CLI::App *parent = createTopLevelCommand(core::to_lower(tokens[0]), "");
      CLI::App *child  = parent->add_subcommand(core::to_lower(tokens[1]), bpTask.description);
      buildCLITask(child, bpTask);
      cliTasks_[child].blueprintTaskName = core::to_upper(blueprintName);
    }
  }
}

//---------------------------------------------------------------------
// createTopLevelCommand()
//---------------------------------------------------------------------
CLI::App* TadahCLI::createTopLevelCommand(const std::string &cmdName, const std::string &desc) {
  auto it = topCommands_.find(cmdName);
  if (it != topCommands_.end()) {
    // Command already exists. Possibly update description if provided.
    if (!desc.empty()) {
      it->second->description(desc);
    }
    return it->second;
  }

  // Create a new command
  CLI::App* newCmd = app_.add_subcommand(cmdName, desc);
  topCommands_[cmdName] = newCmd;
  return newCmd;
}

//---------------------------------------------------------------------
// buildCLITask()
//---------------------------------------------------------------------
void TadahCLI::buildCLITask(CLI::App *cmd, const tadah::core::BlueprintTask &bpTask) {
  // Retrieve the CLITask object (associated with this subcommand)
  auto &cliTask = cliTasks_[cmd];

  // For each blueprint key, create a CLI option and store user input in cliTask.storage
  for (auto &bpKey : bpTask.keyValuePairs) {
    if (bpKey.cli_long_flag.empty()) {
      // If there's no CLI flag definition, skip
      continue;
    }
    // e.g., "--rcut2b"
    std::string spec = bpKey.cli_long_flag;
    // If we have a short flag, add it
    if (!bpKey.cli_short_flag.empty()) {
      // e.g., "--rcut2b,-r"
      spec += "," + bpKey.cli_short_flag;
    }

    std::vector<std::string> tokens;
    boost::split(tokens, bpKey.keyName, boost::is_any_of("."));

    // Create the CLI option, binding to a std::vector<std::string>& in cliTask.storage[storeKey].
    auto &dest = cliTask.storage[tokens.back()];
    CLI::Option *opt = cmd->add_option(spec, dest, bpKey.description);
    if (bpKey.cli_required) {
      opt->required();
    }
    // Additional constraints or validators can be added here if needed.
  }
}

} // namespace cli
} // namespace mlip
} // namespace tadah

