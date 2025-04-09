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
: app_("", "Tadah!")
{

  auto myFormatter = std::make_shared<CLI::Formatter>();

  myFormatter->column_width(30);  // left column width
  myFormatter->right_column_width(50); // description width for options/flags/subcommands
  myFormatter->description_paragraph_width(80); // description width for the main command
  myFormatter->footer_paragraph_width(80); // description width of footer, used for examples

  app_.formatter(myFormatter);

  app_.require_subcommand(0,0); // 0 or more subcommands are required, no max limit
  registerModules();

  const auto &blueprintTadah = core::BlueprintParser::getBlueprintTadahRef();
  app_.usage(blueprintTadah.logo);
  for (const auto &kv : blueprintTadah.blueprintKeys) {
    auto &dest = optionsStorage_[kv.keyName];
    auto *opt = app_.add_option(kv.cli_long_flag, dest, kv.description);
    if (kv.cli_required) {
      opt->required();
    }
  }

  if (!blueprintTadah.examples.empty()) {
    std::string exampleText = "EXAMPLES:\n";
    for (const auto &ex : blueprintTadah.examples) {
      exampleText += "\u00A0\u00A0" + ex + "\n";
    }
    app_.footer(exampleText);
  }

  app_.immediate_callback(true);
  app_.callback(std::bind(&TadahCLI::handleCmdCallback, this, "tadah"));

}

int TadahCLI::run(int argc, char** argv) {
  tadah::core::Logger::instance().setLogLevel(tadah::core::LogLevel::INFO);
  try {
    app_.parse(argc, argv);
    if(app_.count_all()==1) {
      std::cout << "Type: `tadah -h` for help" << std::endl;
      return 0;
    }
  } catch (const CLI::CallForHelp& e) {
    std::cout << app_.help() << std::endl;
    return 0;
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
      parent->usage(bpTask.longDescription);
      if (!bpTask.examples.empty()) {
        std::string exampleText = "EXAMPLES:\n";
        for (const auto &ex : bpTask.examples) {
          exampleText += "\u00A0\u00A0" + ex + "\n";
        }
        parent->footer(exampleText);
      }
      parent->immediate_callback(true);
      parent->callback(std::bind(&TadahCLI::handleCmdCallback, this, blueprintName));

    }
    // If two tokens => first-level + subcommand
    else if (tokens.size() == 2) {
      CLI::App *parent = createCmdSafe(core::to_lower(tokens[0]));
      CLI::App *child  = parent->add_subcommand(core::to_lower(tokens[1]), bpTask.description);
      addCmdOptions(child, bpTask);
      child->usage(bpTask.longDescription);
      if (!bpTask.examples.empty()) {
        std::string exampleText = "EXAMPLES:\n";
        for (const auto &ex : bpTask.examples) {
          exampleText += "\u00A0\u00A0" + ex + "\n";
        }
        child->footer(exampleText);
      }
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

  // add each key as an option
  for (auto &bpKey : bpTask.blueprintKeys) {
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
  
  // exclude options that are not allowed to coexist
  for (auto &bpKey : bpTask.blueprintKeys) {
    if (bpKey.cli_excludes.empty()) {
      continue;
    }
    for (auto &excl : bpKey.cli_excludes) {
      cmd->get_option(bpKey.cli_long_flag)->excludes("--"+excl);
    }
  }

  // Add each group as an option group
  for (auto &group : bpTask.groups) {
    // Combine description lines into a single string:
    std::string groupDesc;
    for (size_t i = 0; i < group.description.size(); i++) {
      if (i > 0) groupDesc += "\n";
      groupDesc += group.description[i];
    }

    CLI::Option_group *grp = cmd->add_option_group(group.groupName, groupDesc);

    // If "exactly 1" or "at least 1" from the group is required:
    grp->require_option(group.requiredMin, group.requiredMax);

    // For each key in this group:
    for (auto &bpKey : group.blueprintKeys) {
      if (bpKey.cli_long_flag.empty()) {
        continue;
      }

      std::string spec = bpKey.cli_long_flag;
      if (!bpKey.cli_short_flag.empty()) {
        spec += "," + bpKey.cli_short_flag;
      }

      // Use last token for storing results
      std::vector<std::string> keyTokens;
      boost::split(keyTokens, bpKey.keyName, boost::is_any_of("."));
      auto &dest = optionsStorage_[keyTokens.back()];

      /*CLI::Option *opt = */grp->add_option(spec, dest, bpKey.description);

    } // end for each key in group
  } // end for each group
}

} // namespace cli
} // namespace mlip
} // namespace tadah
