#include <tadah.mlip/cli/tadah_cli.h>
#include <tadah/core/context.h>
#include <tadah/core/blueprint_parser.h>
#include <iostream>
#include <memory>

namespace tadah {
namespace mlip {
namespace cli {

TadahCLI::TadahCLI()
    : app_("Tadah! - Machine Learning Interatomic Potentials\n"
           "------------------------------------------------\n"
           "Type 'tadah -h <subcommand>' for help (use --help for extended "
           "help).\n") {
  registerModules();
}

void TadahCLI::registerModules() {

  auto &tasksMap = core::BlueprintParser::getTasksMapRef();
  for (const auto &itemPair : tasksMap) {
    std::string task = itemPair.first;
    std::transform(task.begin(), task.end(), task.begin(), ::toupper);
    std::cout << "Task: " << task << std::endl;
    std::cout << "Task Name: " << itemPair.second.taskName << std::endl;
  }
}

int TadahCLI::run(int argc, char **argv) {
  checkFlagSuggestions(argc, argv);
  try {
    app_.parse(argc, argv);
    if (app_.count_all() == 1) {
      std::cout << "Type 'tadah --help' for help." << std::endl;
    }
  } catch (const CLI::CallForHelp &e) {
    std::cout << app_.help() << std::endl;
    return 0;
  } catch (const CLI::ParseError &e) {
    return app_.exit(e);
  }
  return EXIT_SUCCESS;
}

} // namespace cli
} // namespace mlip
} // namespace tadah

