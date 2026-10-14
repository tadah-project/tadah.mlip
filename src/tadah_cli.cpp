#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah.mlip/cli/cli_task.h>

#include <tadah/mlip/task_factory.h>
#include <tadah/mlip/task_manager.h>
#include <tadah/mlip/scheduler.h>
#include <tadah/mlip/engines/train_engine.h>
#include <tadah/mlip/engines/engine_factory.h>
#include <tadah/mlip/engines/all_engines.h>

#include <tadah/mlip/engines/explain_engine.h>

#ifdef TADAH_ENABLE_NFP
#include <tadah/hpo/engines/all_engines.h>
#include <tadah/hpo/optimizer_help.h>
#endif

#include <tadah/core/periodic_table.h>

#include <boost/algorithm/string.hpp> // for boost::split
#include <algorithm>
#include <iostream>
#include <stdexcept>
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

  // The engines first: a subcommand whose engine this tadah was built
  // without says so in its description (registerModules).
  mlip::engines::registerAllMLIPEngines();
#ifdef TADAH_ENABLE_NFP
  hpo::engines::registerAllHPOEngines();
#else
  // HPO's engines are in the HPO library, which only a build with
  // TADAH_ENABLE_NFP has: tadah hpo (and TASK hpo) is refused before any
  // task runs, with what to do (Scheduler::execute).
  mlip::engines::EngineFactory::registerNotBuilt("HPO",
      "tadah hpo needs the Nested Fitting Procedure, which this tadah was "
      "built without; reconfigure with -DTADAH_ENABLE_NFP=ON (see "
      "Installation)");
#endif

  registerModules();
  registerRequired();
  registerNeeds();
  registerExclusions();
  setDefaultsLabels();
  createOptionsLabels();

  const auto &blueprintTadah = core::BlueprintParser::getBlueprintTadahRef();
  app_.allow_extras(false);
  app_.validate_positionals(true);
  app_.usage(blueprintTadah.logo);
  app_.set_version_flag("-v,--version", blueprintTadah.version, "Print version and exit");
  for (const auto &bpKey : blueprintTadah.blueprintKeys) {
    auto &dest = optionsStorage_["tadah"][bpKey.keyName];
    std::string spec = bpKey.cli_long_flag;
    if (!bpKey.cli_short_flag.empty()) {
      spec = spec + "," + bpKey.cli_short_flag;
    }
    auto *opt = app_.add_option(spec, dest, "");
    std::string valueFormat = bpKey.value_format.size() ? bpKey.value_format[0] : "";
    for (size_t i=1; i < bpKey.value_format.size(); ++i) {
      valueFormat += " | " + bpKey.value_format[i];
    }
    opt->option_text(valueFormat);
  }

  app_.immediate_callback(true);
  app_.callback(std::bind(&TadahCLI::handleCmdCallback, this, "tadah"));
  app_.footer(std::bind(&TadahCLI::handleFooterCallback, this, blueprintTadah));

  /* `tadah explain ALGO`, `tadah explain <LIB>:<ALGO>`: the optimisers of
   * tadah hpo, which MLIP's explain command reaches through its hook; a
   * name that is also a command or configuration key (RANDOM) prints that
   * text first. The engines were registered above. */
#ifdef TADAH_ENABLE_NFP
  mlip::engines::set_explain_hook(&hpo::explain_optimiser,
                                  "an optimiser of `tadah hpo`");
#else
  mlip::engines::set_explain_hook(&explain_without_hpo,
                                  "an optimiser of `tadah hpo`");
#endif

  core::PeriodicTable::initialize();
}

bool explain_without_hpo(const std::string &name, std::ostream &) {
  const std::string upper = boost::to_upper_copy(name);
  if (upper != "ALGO" && upper != "LIB") return false;
  throw std::runtime_error(
      "tadah explain: " + name + " names the optimiser of a `tadah hpo` "
      "target, and this tadah is built without HPO (TADAH_ENABLE_NFP OFF): "
      "configure it with -DTADAH_ENABLE_NFP=ON");
}

int TadahCLI::run(int argc, char** argv) {
  core::Logger::instance().setLogLevel(tadah::core::LogLevel::WARNING);
  Scheduler scheduler;

  bool exit_early = false;
  int exit_code = 0;

  try {
    app_.parse(argc, argv);

    if (app_.count_all() == 1) {
      std::cout << "Type: `tadah -h` for help" << std::endl;
      exit_early = true;
      exit_code = 0;
    }

  } catch (const CLI::CallForHelp& e) {
    if (!app_.remaining(false).empty()) {
      throw CLI::ExtrasError(app_.remaining(false));
    }
    std::cout << app_.help() << std::endl;
    exit_early = true;
    exit_code = 0;

  } catch (const CLI::CallForVersion& e) {
    // If you want exactly major.minor.patch:
    std::cout << app_.version() << std::endl;

    exit_early = true;
    exit_code = 0;

  } catch (const CLI::ParseError &e) {
    exit_early = true;
    exit_code = app_.exit(e);
  }

  if (!exit_early) {
    TaskManager taskManager;
    for (auto &cliTask : cliTasks_) {
      auto tasks = TaskFactory().createTasksFromInput(cliTask);
      taskManager.store(tasks);
    }
    std::vector<Task> tasks = taskManager.getTasks();
    scheduler.addTasks(tasks);
  }

  if (exit_early) {
    return exit_code;
  }

  return scheduler.execute();
}

void TadahCLI::registerModules() {
  using tadah::core::BlueprintParser;
  const auto &tasksMap = BlueprintParser::getTasksMapRef();

  // For each blueprint task, create the necessary CLI subcommands.
  for (const auto &pair : tasksMap) {
    const std::string blueprintName = pair.first;   // e.g. "DATA.split"
    const tadah::core::BlueprintTask &bpTask = pair.second;
    // A command this tadah was built without says so after what it does.
    std::string description = bpTask.description;
    const std::string notBuilt =
        mlip::engines::EngineFactory::notBuiltMessage(blueprintName);
    if (!notBuilt.empty()) description += " " + notBuilt + ".";

    std::vector<std::string> tokens;
    boost::split(tokens, blueprintName, boost::is_any_of("."));

    // If only one token => first-level command
    if (tokens.size() == 1) {
      CLI::App *parent = createCmdSafe(core::to_lower(tokens[0]), description);
      addCmdOptions(parent, bpTask);  // question is how to store??
      std::string ldesc = core::wrapText(bpTask.longDescription, description_paragraph_width,true, "");
      ldesc.pop_back(); // remove last \n
      parent->usage(ldesc);
      parent->immediate_callback(true);
      parent->callback(std::bind(&TadahCLI::handleCmdCallback, this, blueprintName));
      parent->footer(std::bind(&TadahCLI::handleFooterCallback, this, bpTask));

    }
    // If two tokens => first-level + subcommand
    else if (tokens.size() == 2) {
      CLI::App *parent = createCmdSafe(core::to_lower(tokens[0]));
      CLI::App *child  = parent->add_subcommand(core::to_lower(tokens[1]), description);
      addCmdOptions(child, bpTask);
      std::string ldesc = core::wrapText(bpTask.longDescription, description_paragraph_width,true, "");
      ldesc.pop_back(); // remove last \n
      child->usage(ldesc);
      child->immediate_callback(true);
      child->callback(std::bind(&TadahCLI::handleCmdCallback, this, blueprintName));
      child->footer(std::bind(&TadahCLI::handleFooterCallback, this, bpTask));
    }
  }
}

std::string TadahCLI::handleFooterCallback(const core::BlueprintTask &bpTask) {
  std::string footerText = "";
  if (!bpTask.examples.empty()) {
    std::string exampleText = "EXAMPLES:\n";
    for (const auto &ex : bpTask.examples) {
      exampleText += "\u00A0\u00A0" + ex + "\n";
    }
    footerText += exampleText;
  }
  return footerText;
}

void TadahCLI::handleCmdCallback(const std::string &blueprintName) { 
  CLITask cliTask;
  cliTask.blueprintTaskName = blueprintName;
  cliTask.storage = this->optionsStorage_[blueprintName];

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
  for (auto &kv : this->optionsStorage_[blueprintName]) {
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
    std::string spec = bpKey.cli_long_flag;
    if (!bpKey.cli_short_flag.empty()) {
      spec = spec + "," + bpKey.cli_short_flag;
    }

    std::vector<std::string> keyTokens;
    boost::split(keyTokens, bpKey.keyName, boost::is_any_of("."));

    auto &dest = optionsStorage_[bpTask.taskName][keyTokens.back()];
    if (bpKey.value_N.size()==1 && bpKey.value_type[0] == "BOOL") {
      // For boolean options, use add_flag
      cmd->add_flag(spec, dest, bpKey.description);
    } else {
      // For options, use add_option
      auto *opt = cmd->add_option(spec, dest, "");
      if (!bpKey.value_N.empty()) {
        opt->expected(1, std::abs(bpKey.value_N[0]));
      }
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
      auto &dest = optionsStorage_[bpTask.taskName][keyTokens.back()];

      if (bpKey.value_N.size()==1 && bpKey.value_type[0] == "BOOL") {
        // For boolean options, use add_flag
        grp->add_flag(spec, dest, "");
      } else {
        // For options, use add_option
        auto *opt = grp->add_option(spec, dest, "");
        if (!bpKey.value_N.empty()) {
          opt->expected(1, std::abs(bpKey.value_N[0]));
        }
      }
    } // end for each key in group
  } // end for each group
}

namespace {

/* Every option of the subcommand @p cmd: its own and those of its option
 * groups (CLI::App::get_options lists the options of a '+' group only, not
 * those of a named one such as tadah hpo's "input": --config,
 * --validation, --hpotarget and --task). */
std::vector<CLI::Option*> every_option_of(CLI::App* cmd) {
  std::vector<CLI::Option*> all = cmd->get_options();
  const auto groups =
      cmd->get_subcommands([](CLI::App* sub) { return sub->get_name().empty(); });
  for (CLI::App* group : groups)
    for (CLI::Option* opt : group->get_options())
      if (std::find(all.begin(), all.end(), opt) == all.end())
        all.push_back(opt);
  return all;
}

}  // namespace

void TadahCLI::registerExclusions() {
  // cli_excludes = ["ALL"] (--task): the option excludes every other option
  // of the subcommand, those of its option groups too (a run of a task file
  // takes no command-line option: TaskFactory reads none beside --task).
  auto topLevelAction = [](CLI::App* cmd, CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
      if (bpKey.cli_excludes.size()==1 && bpKey.cli_excludes[0] == "ALL") {
        // Exclude from *all* other options in that subcommand
        for (auto *otherOpt : every_option_of(cmd)) {
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
      // Exclude from *all* other options in that subcommand, those of its
      // option groups (this one's too) included, apart from --help
      for (auto *otherOpt : every_option_of(cmd)) {
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
    bool isBool= bpKey.value_type[0]=="BOOL";
    // For options we create custom labels
    // 1.1 Value format - format
    std::string valueFormat="\u00A0"; // pretend string is never empty
    if (!isBool) {
      valueFormat += bpKey.value_format.size() ? bpKey.value_format[0] : "";
      for (size_t i=1; i < bpKey.value_format.size(); ++i) {
        valueFormat += " or " + bpKey.value_format[i];
      }
    }

    // 1.2 Value format - required
    bool required= bpKey.cli_required;
    valueFormat += required ? " (Required)" : "";
 
    // 1.3 Value format - min/max
    if (!isBool) {
      if (bpKey.value_N.size() == 1) {
        int maxint= std::abs(bpKey.value_N[0]);
        std::string maxValue = maxint == 2147483647 ? "MAX_INT" : core::to_string(maxint);
        valueFormat += " (Min: 1, Max: " + maxValue + ")";
      } else if (bpKey.value_N.size() == 2) {
        std::string minValue = core::to_string(std::abs(bpKey.value_N[0]));
        int maxint= std::abs(bpKey.value_N[1]);
        std::string maxValue = maxint == 2147483647 ? "MAX_INT" : core::to_string(maxint);
        valueFormat += " (Min: " + minValue + ", Max: " + maxValue + ")";
      }
    }

    // 2. Needs
    std::string needsFormat;
    auto needs = opt->get_needs();
    if (needs.size() > 0) {
      for (auto &need: needs) {
        needsFormat += need->get_name() + " ";
      }
      // remove last space
      needsFormat = needsFormat.substr(0, needsFormat.size()-1);
    }
    if (needsFormat.size() && needsFormat[0] == '-') 
      needsFormat = " (Needs:    " + needsFormat + ")";


    // 3. Excludes
    std::string excludesFormat= "";
    auto excludes = opt->get_excludes();
    if (excludes.size() > 0) {
      for (auto &excl: excludes) {
        excludesFormat += excl->get_name() + " ";
      }
      // remove last space
      excludesFormat = excludesFormat.substr(0, excludesFormat.size()-1);
    }
    if (excludesFormat.size() && excludesFormat[0] == '-')
      excludesFormat = " (Excludes: " + excludesFormat + ")";

    // 4. Add default_value
    auto defaultValues = opt->get_default_str();
    if (defaultValues.size() > 0) {
      defaultValues = " (Default: " + defaultValues+ ")";
    }

    // 5. If long_description is not empty, prompt user to read it
    std::string longDescFormat;
    if (!bpKey.longDescription.empty()) {
      // strip "input." and "group." if present from the key name
      std::string keyName = core::to_lower(bpKey.keyName);
      {
        std::string toRemove = "input.";
        auto pos = keyName.find(toRemove);
        if (pos != std::string::npos) {
          keyName.erase(pos, toRemove.size());
        }
      }
      {
        std::string toRemove = "group.";
        auto pos = keyName.find(toRemove);
        if (pos != std::string::npos) {
          keyName.erase(pos, toRemove.size());
        }
      }
      // Strip "input." or "group." if found at position 0
      longDescFormat = " (Run `tadah explain " + keyName + "` for more info)";
    }

    std::string currentDescription = isBool ? "\u00A0"+bpKey.description : bpKey.description;
    // std::string currentDescription = opt->get_description();
    currentDescription += needsFormat + excludesFormat + defaultValues + longDescFormat;
    opt->description(currentDescription);
    opt->option_text(valueFormat);
  };

  auto groupAction = [topLevelAction](CLI::App* cmd, CLI::App* , CLI::Option* grpOpt,
      const tadah::core::BlueprintKey& bpKey) {
    topLevelAction(cmd, grpOpt, bpKey);
  };

  processBlueprintTasks(topLevelAction, groupAction);
}

void TadahCLI::setDefaultsLabels() {
  auto topLevelAction = [](CLI::App* , CLI::Option* opt,
      const tadah::core::BlueprintKey& bpKey) {
    if (bpKey.default_value.size()) {
      std::string defaultValue = bpKey.default_value[0];
      for (size_t i=1; i < bpKey.default_value.size(); ++i) {
        defaultValue += " " + bpKey.default_value[i];
      }
      // do not run callback for default values
      // otherwise they tak precedence over user input in the config file
      opt->run_callback_for_default(false);
      opt->default_val(defaultValue);
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

    // Process top-level options. Keys with an empty cli_long_flag are
    // config-file-only (no CLI option was created for them) — skip.
    for (auto& bpKey : bpTask.blueprintKeys) {
      if (bpKey.cli_long_flag.empty()) continue;
      CLI::Option* currentOpt = cmd->get_option(bpKey.cli_long_flag);
      topLevelHandler(cmd, currentOpt, bpKey);
    }

    // Process group-level options (same skip for config-file-only keys)
    for (auto& group : bpTask.groups) {
      CLI::App* grp = cmd->get_option_group(group.groupName);
      for (auto& bpKey : group.blueprintKeys) {
        if (bpKey.cli_long_flag.empty()) continue;
        CLI::Option* groupOpt = grp->get_option(bpKey.cli_long_flag);
        groupHandler(cmd, grp, groupOpt, bpKey);
      }
    }
  }
}

} // namespace cli
} // namespace mlip
} // namespace tadah
