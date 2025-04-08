#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah.mlip/cli/cli_task.h>
#include <tadah/mlip/task_factory.h>
#include <tadah/mlip/task_manager.h>

#include <boost/algorithm/string.hpp> // for boost::split
#include <iostream>
#include <vector>

namespace tadah {
namespace mlip {
namespace cli {

TadahCLI::TadahCLI()
: app_("Tadah CLI - Dynamically generated from blueprint tasks.\n")
{
  app_.require_subcommand(0,10);
  registerModules();

  const auto &blueprintTadah = core::BlueprintParser::getBlueprintTadahRef();
  for (const auto &kv : blueprintTadah.keyValuePairs) {
    auto &dest = optionsStorage_[kv.keyName];
    app_.add_option(kv.cli_long_flag, dest, kv.description);
  }
  app_.immediate_callback(true);
  app_.callback(std::bind(&TadahCLI::handleCmdCallback, this, "tadah"));
}

int TadahCLI::run(int argc, char** argv) {
  tadah::core::Logger::instance().setLogLevel(tadah::core::LogLevel::INFO);
  try {
    app_.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    std::cout << "CLI11 Exit code: " << e.get_exit_code() << std::endl;
    std::cerr << "CLI11 Error parsing command line arguments: " << e.what() << "\n";
    return app_.exit(e);
  }
  // std::cout << "Parsed command line arguments successfully.\n";

  TaskManager taskManager;
  for (auto &cliTask : cliTasks_) {
    // std::cout << "Creating tasks from CLI task: " << cliTask.blueprintTaskName << "\n";
    auto tasks = TaskFactory().createTasksFromInput(cliTask);
    taskManager.store(tasks);
  }

  std::cout << "Stored tasks:\n";
  for (const auto &task : taskManager.getTasks()) {
    task.print();
  }

  return 0;
}

void TadahCLI::registerModules() {
  using tadah::core::BlueprintParser;
  const auto &tasksMap = BlueprintParser::getTasksMapRef();

  // For each blueprint task, create the necessary CLI subcommands.
  for (const auto &pair : tasksMap) {
    const std::string blueprintName = pair.first;   // e.g. "DATA.split"
    const tadah::core::BlueprintTask &bpTask = pair.second;

    std::vector<std::string> tokens;
    boost::split(tokens, blueprintName, boost::is_any_of("."));

    // If only one token => first-level command
    if (tokens.size() == 1) {
      CLI::App *parent = createCmdSafe(core::to_lower(tokens[0]), bpTask.description);
      addCmdOptions(parent, bpTask);  // question is how to store??
      parent->immediate_callback(true);
      parent->callback(std::bind(&TadahCLI::handleCmdCallback, this, blueprintName));

    }
    // If two tokens => first-level + subcommand
    else if (tokens.size() == 2) {
      CLI::App *parent = createCmdSafe(core::to_lower(tokens[0]));
      CLI::App *child  = parent->add_subcommand(core::to_lower(tokens[1]), bpTask.description);
      addCmdOptions(child, bpTask);
      child->immediate_callback(true);
      child->callback(std::bind(&TadahCLI::handleCmdCallback, this, blueprintName));
    }
  }
}

void TadahCLI::handleCmdCallback(const std::string &blueprintName) { 
  CLITask cliTask;
  cliTask.blueprintTaskName = blueprintName;
  cliTask.storage = this->optionsStorage_;

  // Check if the storage map is empty. If so, we do not store it.
  bool isEmpty = true;
  for (auto &kv : cliTask.storage) {
    if (!kv.second.empty()) {
      isEmpty = false;
      break;
    }
  }
  if (isEmpty) {
    // This must be first level command
    // without any options, we do not store it.
    return;
  }

  cliTasks_.push_back(cliTask);
  // Clear storage map for next command.
  for (auto &kv : this->optionsStorage_) {
    kv.second.clear();
  }
}

CLI::App* TadahCLI::createCmdSafe(const std::string &cmdName, const std::string &desc) {
  auto it = firstLevelCommands_.find(cmdName);
  if (it != firstLevelCommands_.end()) {
    // Command already exists. Possibly update description if provided.
    if (!desc.empty()) {
      it->second->description(desc);
    }
    return it->second;
  }

  // Create a new command
  CLI::App* newCmd = app_.add_subcommand(cmdName, desc);
  firstLevelCommands_[cmdName] = newCmd;
  return newCmd;
}

void TadahCLI::addCmdOptions(CLI::App *cmd, const tadah::core::BlueprintTask &bpTask) {

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
      spec = spec + "," + bpKey.cli_short_flag;
    }

    std::vector<std::string> keyTokens;
    boost::split(keyTokens, bpKey.keyName, boost::is_any_of("."));

    auto &dest = optionsStorage_[keyTokens.back()];
    CLI::Option *opt = cmd->add_option(spec, dest, bpKey.description);
    if (bpKey.cli_required) {
      opt->required();
    }
  }
}

} // namespace cli
} // namespace mlip
} // namespace tadah
