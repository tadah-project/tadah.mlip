#ifndef TADAH_MLIP_CLI_TADAH_CLI_H
#define TADAH_MLIP_CLI_TADAH_CLI_H

#include "CLI/CLI.hpp"
#include <string>
#include <map>
#include <unordered_map>
#include <vector>

/**
 * Project's internal headers
 */
#include <tadah/core/context.h>
#include <tadah/core/blueprint_parser.h>
#include <tadah/core/logger.h>
#include <tadah/mlip/engine_factory.h>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Represents a CLI task with its associated storage and blueprint task name.
 *
 * The storage maps BlueprintKey.keyName to a vector of strings, allowing for flexible
 * storage of command-line arguments. The blueprintTaskName is the name of the task
 * as defined in the BlueprintTask.taskName.
 */
struct CLITask{
  std::unordered_map<std::string, std::vector<std::string>> storage;
  std::string blueprintTaskName;  ///< Lowercase dot name (e.g. "data.split")
};

/**
 * @class TadahCLI
 * @brief Builds a CLI that uses lowercase for tasks and subcommands, preserves original
 *        flags from blueprint, and allows both command and subcommand to be callable.
 *
 * The steps:
 *  - register iEngines in EngineFactory
 *  - registerModules() creates commands/subcommands from the blueprint, storing
 *    "data.split" in cliOptions_
 *  - run(...) after parse => identify subcommand => build context => engineFactory->create("data.split")
 */
class TadahCLI {
public:
  /// Constructs the CLI, verifying blueprint and registering tasks.
  TadahCLI();

  /**
   * @brief Parses arguments, identifies the chosen command/subcommand, merges context,
   * and calls the correct engine outside subcommand callbacks.
   * @param argc Arg count
   * @param argv Arg array
   * @return 0 if success, else parse error code
   */
  int run(int argc, char** argv);

private:

  /**
   * @brief Creates CLI subcommands from each blueprint task, storing "data.split" in cliOptions_.
   */
  void registerModules();

  /**
   * @brief Ensures a top-level command for "data" or "train" is created (lowercase).
   * @param cmdName Already-lowercased text
   * @param desc from blueprint
   * @return The CLI::App pointer
   */
  CLI::App* createTopLevelCommand(const std::string &cmdName,
                                        const std::string &desc);

  /**
   * @brief Adds all CLI flags/options from the blueprint keys.
   * @param cmd The subcommand pointer
   * @param bpTask The blueprint task containing keyValuePairs
   */
  void buildCLITask(CLI::App *cmd,
                           const tadah::core::BlueprintTask &bpTask);

  /**
   * @brief Builds a new Context from cliTasks_[sub], i.e. user CLI input.
   * @param sub The chosen subcommand
   * @return The resulting tadah::core::Context
   */
  tadah::core::Context buildContextFromCLI(CLI::App *sub);

private:
  CLI::App app_;  ///< Root CLI

  /**
   * @brief Maps each subcommand pointer to cliTask with:
   */
  std::unordered_map<CLI::App*, CLITask> cliTasks_;

  /// For storing references to top commands by name.
  std::map<std::string, CLI::App*> topCommands_;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TADAH_CLI_H

