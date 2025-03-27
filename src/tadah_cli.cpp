#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/core/context.h>
#include <tadah/core/blueprint_parser.h>
#include <iostream>
#include <memory>
#include <algorithm>
#include <stdexcept>
namespace tadah {
namespace mlip {
namespace cli {

TadahCLI::TadahCLI()
  : app_("Tadah CLI - Example. No config merges performed here.\n") {
  // The top-level app automatically has --help to print overall usage.
  // Force the top-level to show help if user typed -h/--help.
  // By default, CLI11 does this automatically, so you typically do not need:
  //   app_.set_help_flag("-h,--help","Show general help.");
  // But it can be done for consistency.

  registerModules();
}

std::vector<std::string> TadahCLI::splitOnDot(const std::string &s) {
  std::vector<std::string> tokens;
  auto pos = s.find('.');
  if (pos == std::string::npos) {
    tokens.push_back(s);
  } else {
    tokens.push_back(s.substr(0, pos));
    tokens.push_back(s.substr(pos + 1));
  }
  return tokens;
}

CLI::App* TadahCLI::findOrCreateTopLevelCommand(const std::string &cmdName,
                                                const std::string &desc) {
  auto it = topCommands_.find(cmdName);
  if (it != topCommands_.end()) {
    // Command already exists, optionally update its description if not empty
    if (!desc.empty()) {
      it->second->description(desc);
    }
    return it->second;
  }

  CLI::App* cmd = app_.add_subcommand(cmdName, desc);
  // Each subcommand will have built-in support for --help printing subcommand help
  // But we can be explicit:
  cmd->set_help_flag("-h,--help", "Show help for this command.");

  topCommands_[cmdName] = cmd;
  hasCallback_[cmd] = false;
  return cmd;
}

void TadahCLI::registerModules() {
  const auto &tasksMap = tadah::core::BlueprintParser::getTasksMapRef();

  for (auto &pair : tasksMap) {
    // e.g. "TRAIN" from blueprint => lowercase "train"
    std::string fullTaskName = pair.first; // uppercase from blueprint
    auto &taskObj            = pair.second;

    std::string lowerTaskName = fullTaskName;
    std::transform(lowerTaskName.begin(), lowerTaskName.end(),
                   lowerTaskName.begin(), ::tolower);

    std::vector<std::string> tokens = splitOnDot(lowerTaskName);

    if (tokens.size() == 1) {
      // Single-level command
      CLI::App* cmd = findOrCreateTopLevelCommand(tokens[0], taskObj.description);

      // If blueprint indicates a real runnable task (has mandatory fields):
      if (!taskObj.description.empty() && !taskObj.longDescription.empty()) {
        cmd->callback([fullTaskName]() {
          // The engine call is here; actual Context usage is external
          auto eng = mlip::EngineFactory::create(fullTaskName);
          // eng->validate(...), eng->run(...), etc.
        });
        hasCallback_[cmd] = true;
      }
    }
    else if (tokens.size() == 2) {
      // Parent + subcommand
      CLI::App* parent = findOrCreateTopLevelCommand(tokens[0]);
      CLI::App* child = parent->add_subcommand(tokens[1], taskObj.description);

      // Let child subcommand define its own help flag
      child->set_help_flag("-h,--help", "Show help for this subcommand.");

      hasCallback_[child] = false;

      if (!taskObj.description.empty() && !taskObj.longDescription.empty()) {
        child->callback([fullTaskName]() {
          auto eng = mlip::EngineFactory::create(fullTaskName);
        });
        hasCallback_[child] = true;
      }
    }
  }

  // If a parent command has no callback but has subcommands => require subcommand
  for (auto &pr : topCommands_) {
    CLI::App* cmd = pr.second;
    if (!hasCallback_[cmd] && !cmd->get_subcommands().empty()) {
      cmd->require_subcommand(1);
    }
  }
}

int TadahCLI::run(int argc, char **argv) {
  try {
    app_.parse(argc, argv);

    // If user typed just `tadah` with no subcommands
    if (app_.get_subcommands().empty()) {
      std::cout << "Type: `tadah -h` for help.\n";
    }
  } catch (const CLI::ParseError &err) {
    return app_.exit(err);
  }
  return 0;
}

} // namespace cli
} // namespace mlip
} // namespace tadah
