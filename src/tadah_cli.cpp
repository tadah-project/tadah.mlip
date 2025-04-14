#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah.mlip/cli/cli_task.h>

#include <tadah/mlip/task_factory.h>
#include <tadah/mlip/task_manager.h>
#include <tadah/mlip/engines/train_engine.h>
#include <tadah/mlip/engines/engine_factory.h>
#include <tadah/mlip/engines/all_engines.h>

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

  myFormatter->column_width(left_column_width);  // left column width
  myFormatter->right_column_width(right_column_width); // description width for options/flags/subcommands
  myFormatter->description_paragraph_width(description_paragraph_width); // description width for the main command
  myFormatter->footer_paragraph_width(footer_paragraph_width); // description width of footer, used for examples
  app_.formatter(myFormatter);


  app_.require_subcommand(0,0); // 0 or more subcommands are required, no max limit
  registerModules();
  registerRequired();
  registerNeeds();
  registerExclusions();
  createOptionsLabels();


  const auto &blueprintTadah = core::BlueprintParser::getBlueprintTadahRef();
  app_.usage(blueprintTadah.logo);
  for (const auto &kv : blueprintTadah.blueprintKeys) {
    auto &dest = optionsStorage_[kv.keyName];
    app_.add_option(kv.cli_long_flag, dest, kv.description);
  }

  if (!blueprintTadah.examples.empty()) {
    std::string exampleText = "EXAMPLES:\n";
    for (const auto &ex : blueprintTadah.examples) {
      exampleText += "\u00A0\u00A0" + ex + "\n";
    }
    // remove the last newline character from a string
    app_.footer(exampleText);
  }

  app_.immediate_callback(true);
  app_.callback(std::bind(&TadahCLI::handleCmdCallback, this, "tadah"));

  using tadah::mlip::engines::EngineFactory;
  using tadah::mlip::engines::TrainEngine;

  mlip::engines::registerAllEngines();
}

int TadahCLI::run(int argc, char** argv) {
  core::Logger::instance().setLogLevel(tadah::core::LogLevel::INFO);
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

  TaskManager taskManager;
  for (auto &cliTask : cliTasks_) {
    auto tasks = TaskFactory().createTasksFromInput(cliTask);
    taskManager.store(tasks);
  }

  std::cout << "Stored tasks:\n";
  for (const auto &task : taskManager.getTasks()) {
    task.print();
    auto engine = mlip::engines::EngineFactory::create(task.getTaskName());
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
      parent->usage(core::wrapText(bpTask.longDescription, description_paragraph_width,true, ""));
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
      child->usage(core::wrapText(bpTask.longDescription, description_paragraph_width,true, ""));
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
    if (bpKey.value_N.size()==1 && bpKey.value_type[0] == "BOOL") {
      // For boolean options, use add_flag
      cmd->add_flag(spec, dest, bpKey.description);
    } else {
      // For options, use add_option
      cmd->add_option(spec, dest, bpKey.description);
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
    grp->set_help_flag("", "");

    // For each key in this group add it to the group as an option
    for (auto &bpKey : group.blueprintKeys) {
      if (bpKey.cli_long_flag.empty()) {
        continue;
      }

      std::string spec = bpKey.cli_long_flag;
      if (!bpKey.cli_short_flag.empty()) {
        spec += "," + bpKey.cli_short_flag;
      }

      std::vector<std::string> keyTokens;
      boost::split(keyTokens, bpKey.keyName, boost::is_any_of("."));
      auto &dest = optionsStorage_[keyTokens.back()];

      if (bpKey.value_N.size()==1 && bpKey.value_type[0] == "BOOL") {
        // For boolean options, use add_flag
        grp->add_flag(spec, dest, bpKey.description);
      } else {
        // For options, use add_option
        grp->add_option(spec, dest, bpKey.description);
      }
    } // end for each key in group
  } // end for each group
}

void TadahCLI::registerExclusions() {
  auto topLevelAction = [](CLI::App* cmd, CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
      if (bpKey.cli_excludes.size()==1 && bpKey.cli_excludes[0] == "ALL") {
        // Exclude from *all* other options in that subcommand
        for (auto *otherOpt : cmd->get_options()) {
          if (otherOpt != opt && otherOpt->get_name() != "--help") {
            opt->excludes(otherOpt);
          }
        }
      } else {
        // Exclude from the listed options
        for (auto &exclName : bpKey.cli_excludes) {
          CLI::Option *otherOpt = cmd->get_option("--" + exclName);
          if (otherOpt) {
            opt->excludes(otherOpt);
          }
        }
      }
  };

  auto groupAction = [](CLI::App* cmd, CLI::App*, CLI::Option* grpOpt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.cli_excludes.size()==1 && bpKey.cli_excludes[0] == "ALL") {
      // Exclude from *all* other options in that subcommand
      // which are not in the group apart from --help
      for (auto *otherOpt : cmd->get_options()) {
        if (otherOpt != grpOpt && otherOpt->get_name() != "--help") {
          grpOpt->excludes(otherOpt);
        }
      }
    } else {
      // Exclude from the listed options
      for (auto &exclName : bpKey.cli_excludes) {
        CLI::Option *otherOpt = cmd->get_option("--" + exclName);
        if (otherOpt) {
          grpOpt->excludes(otherOpt);
        }
      }
    }
  };

  processBlueprintTasks(topLevelAction, groupAction);
}

void TadahCLI::registerNeeds() {
  auto topLevelAction = [](CLI::App* cmd, CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.cli_needs.size()) {
      for (auto &needName : bpKey.cli_needs) {
        CLI::Option *otherOpt = cmd->get_option("--" + needName);
        opt->needs(otherOpt);
      }
    }
  };

  auto groupAction = [](CLI::App* cmd, CLI::App* , CLI::Option* grpOpt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.cli_needs.size()) {
      for (auto &needName : bpKey.cli_needs) {
        CLI::Option *otherOpt = cmd->get_option("--" + needName);
        grpOpt->needs(otherOpt);
      }
    }
  };

  processBlueprintTasks(topLevelAction, groupAction);
}

void TadahCLI::registerRequired() {
  auto topLevelAction = [](CLI::App* , CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.cli_required) {
      opt->required();
    }
  };

  auto groupAction = [](CLI::App* , CLI::App* , CLI::Option* grpOpt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.cli_required) {
      grpOpt->required();
    }
  };

  processBlueprintTasks(topLevelAction, groupAction);
}

void TadahCLI::createOptionsLabels() {
  auto topLevelAction = [this](CLI::App* , CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
    size_t padding = 12;
    // For options we create custom labels
    // 1. Value format
    std::string valueFormat = bpKey.value_format.size() ? bpKey.value_format[0] : "";
    for (size_t i=1; i < bpKey.value_format.size(); ++i) {
      valueFormat += "\n" + bpKey.value_format[i];
    }
    valueFormat += "\n";

    // 2. Needs
    std::string needsFormat = "";
    auto needs = opt->get_needs();
    if (needs.size() > 0) {
      for (auto &need: needs) {
        needsFormat += " "+need->get_name();
      }
    }
    needsFormat = core::wrapText(needsFormat, description_paragraph_width-padding,true,std::string(padding, ' '));  
    if (needsFormat.size() && needsFormat[0] == '-') needsFormat = "  Needs:    " + needsFormat;

    // 3. Excludes
    std::string excludesFormat = "";
    auto excludes = opt->get_excludes();
    if (excludes.size() > 0) {
      for (auto &excl: excludes) {
        excludesFormat += " "+excl->get_name();
      }
    }
    excludesFormat = core::wrapText(excludesFormat, description_paragraph_width-padding,true,std::string(padding, ' '));  
    if (excludesFormat.size() && excludesFormat[0] == '-') excludesFormat = "  Excludes: " + excludesFormat;

    std::string fullFormat = "";
    if (!core::is_blank_line(valueFormat)) {
      fullFormat += valueFormat;
    }
    if (!core::is_blank_line(needsFormat)) {
      fullFormat += needsFormat;
    }
    if (!core::is_blank_line(excludesFormat)) {
      fullFormat += excludesFormat;
    }
    if (!core::is_blank_line(fullFormat)) {
      opt->option_text(fullFormat);
    }
  };

  auto groupAction = [topLevelAction](CLI::App* cmd, CLI::App* , CLI::Option* grpOpt,
      const tadah::core::BlueprintKey& bpKey) {
    topLevelAction(cmd, grpOpt, bpKey);
  };

  processBlueprintTasks(topLevelAction, groupAction);
}

template <typename TopLevelHandler, typename GroupHandler>
void TadahCLI::processBlueprintTasks(
  TopLevelHandler&& topLevelHandler,
  GroupHandler&& groupHandler)
{
  using tadah::core::BlueprintParser;

  // Retrieve the tasks from the blueprint
  const auto &tasksMap = BlueprintParser::getTasksMapRef();

  // Repetitive loop logic is centralized here
  for (auto& pair : tasksMap) {
    const std::string& blueprintName = pair.first;
    const tadah::core::BlueprintTask& bpTask = pair.second;

    // Split into tokens, e.g.: "DATA.split" -> ["DATA","split"]
    std::vector<std::string> tokens;
    boost::split(tokens, blueprintName, boost::is_any_of("."));

    // Retrieve the CLI::App* for these tokens
    CLI::App* cmd = nullptr;
    if (!tokens.empty()) {
      // First token -> top-level
      cmd = app_.get_subcommand(core::to_lower(tokens[0]));
      // Additional tokens -> subcommands
      for (size_t i = 1; i < tokens.size(); ++i) {
        cmd = cmd->get_subcommand(core::to_lower(tokens[i]));
      }
    }

    // Process top-level options
    for (auto& bpKey : bpTask.blueprintKeys) {
      CLI::Option* currentOpt = cmd->get_option(bpKey.cli_long_flag);
      topLevelHandler(cmd, currentOpt, bpKey);
    }

    // Process group-level options
    for (auto& group : bpTask.groups) {
      CLI::App* grp = cmd->get_option_group(group.groupName);
      for (auto& bpKey : group.blueprintKeys) {
        CLI::Option* groupOpt = grp->get_option(bpKey.cli_long_flag);
        groupHandler(cmd, grp, groupOpt, bpKey);
      }
    }
  }
}

} // namespace cli
} // namespace mlip
} // namespace tadah
