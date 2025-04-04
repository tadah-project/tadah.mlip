/**
 * @file tadah_cli.h
 * @brief Declaration of the TadahCLI class for parsing command-line arguments and creating tasks.
 *
 * The TadahCLI class uses CLI11 for argument parsing. It reads the blueprint tasks,
 * dynamically registers subcommands and flags, and populates CLITask structures.
 * After parsing, users typically call TaskFactory with the CLITask to produce Task objects.
 *
 * @note This file depends on:
 *  - CLI11 (third-party)
 *  - tadah::mlip::cli::CLITask
 *  - tadah::core::BlueprintParser
 *  - tadah::mlip::EngineFactory
 *  - tadah::mlip::TaskManager
 *  - etc.
 */

#ifndef TADAH_MLIP_CLI_TADAH_CLI_H
#define TADAH_MLIP_CLI_TADAH_CLI_H

#include "CLI/CLI.hpp"
#include <string>
#include <unordered_map>
#include <vector>

#include <tadah.mlip/cli/cli_task.h>             // CLITask inherits ITaskInput
#include <tadah/core/context.h>
#include <tadah/core/blueprint_parser.h>
#include <tadah/core/logger.h>
#include <tadah/mlip/engine_factory.h>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @class TadahCLI
 * @brief Builds and manages a command-line interface for Tadah! tasks.
 *
 * TadahCLI dynamically registers subcommands derived from blueprint tasks,
 * preserving original flags/names where possible. When run, it parses the
 * user's input and creates CLITask objects that can be converted into Task
 * objects by a TaskFactory.
 *
 * ### Example Usage
 * @code
 * int main(int argc, char** argv) {
 *   tadah::mlip::cli::TadahCLI cli;
 *   return cli.run(argc, argv);
 * }
 * @endcode
 */
class TadahCLI {
public:
  /**
   * @brief Constructs the TadahCLI, registering blueprint-based commands.
   *
   * The constructor calls registerModules() internally to build a subcommand
   * hierarchy using the blueprint tasks. For example, a blueprint task named
   * "DATA.SPLIT" will create a top-level "data" command with a "split" subcommand.
   */
  TadahCLI();

  /**
   * @brief Default destructor.
   */
  ~TadahCLI() = default;

  /**
   * @brief Parses the command line arguments and populates one or more CLITask objects.
   *
   * Uses CLI11 to parse the user's input. If the user selected a valid subcommand,
   * the corresponding CLITask is then filled with the specified flags/options.
   *
   * @param argc The number of command-line arguments.
   * @param argv The array of command-line argument C-strings.
   * @return An integer exit code. 0 indicates success.
   */
  int run(int argc, char** argv);

private:
  /**
   * @brief Registers blueprint tasks as CLI subcommands.
   *
   * For each task from the blueprint, creates either a top-level CLI command or
   * a subcommand based on the dot notation. Example: "DATA.SPLIT" => top command
   * "data" with subcommand "split".
   */
  void registerModules();

  /**
   * @brief Ensures a top-level command for the given name exists or creates it.
   *
   * @param cmdName The command name in lowercase (e.g. "data").
   * @param desc A short description for this command if newly created.
   * @return A pointer to the CLI::App that represents the top-level command.
   */
  CLI::App* createTopLevelCommand(const std::string &cmdName, const std::string &desc);

  /**
   * @brief Builds CLI flags/options for a given blueprint task.
   *
   * Iterates over BlueprintTask.keyValuePairs. Each item is mapped to an option
   * in the CLI. The user input is stored in the CLITask's storage map for that subcommand.
   *
   * @param cmd A pointer to the CLI::App subcommand representing this blueprint task.
   * @param bpTask The blueprint data containing keyValuePairs for the task.
   */
  void buildCLITask(CLI::App *cmd, const tadah::core::BlueprintTask &bpTask);

private:
  CLI::App app_; ///< The root CLI11 application.

  /**
   * @brief Maps subcommand pointers to their associated CLITask.
   *
   * Each subcommand is associated with one CLITask. When the user invokes that
   * subcommand, CLI11 populates the CLITask's storage.
   */
  std::unordered_map<CLI::App*, CLITask> cliTasks_;

  /**
   * @brief Stores references to top-level commands by lowercase name.
   *
   * For example, "data" -> CLI::App pointer. This helps in grouping subcommands.
   */
  std::unordered_map<std::string, CLI::App*> topCommands_;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TADAH_CLI_H

