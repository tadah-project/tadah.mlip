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

#include <CLI/CLI11.hpp>
#include <map>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <tadah.mlip/cli/cli_task.h>             // CLITask inherits ITaskInput
#include <tadah/core/context.h>
#include <tadah/core/blueprint_parser.h>
#include <tadah/core/logger.h>
#include <tadah/mlip/engines/engine_factory.h>
#include <tadah/mlip/task_manager.h>

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
   * "DATA.SPLIT" will create a first-level "data" command with a "split" subcommand.
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
  CLI::App app_; ///< The root CLI11 application.

private:
  /**
   * @brief Registers blueprint tasks as CLI subcommands.
   *
   * For each task from the blueprint, creates either a first-level CLI command or
   * a subcommand based on the dot notation. Example: "DATA.SPLIT" => first-level command
   * "data" with subcommand "split".
   */
  void registerModules();

  void registerRequired();
  void registerNeeds();
  void registerExclusions();
  void createOptionsLabels();

  /**
   * @brief Sets default values for CLI options.
   *
   * This method is called after all register commands and before parsing.
   * It ensures that all options have default values set if not provided by the user.
   */
  void setDefaultsLabels();

  /**
   * @brief Ensures a first-level command for the given name exists or creates it.
   *
   * @param cmdName The command name in lowercase (e.g. "data").
   * @param desc A short description for this command if newly created.
   * @return A pointer to the CLI::App that represents the first-level command.
   */
  CLI::App* createCmdSafe(const std::string &cmdName, const std::string &desc="");

  /**
   * @brief Builds CLI flags/options for a given blueprint task.
   *
   * Iterates over BlueprintTask.keyValuePairs. Each item is mapped to an option
   * in the CLI. The user input is stored in the CLITask's storage map for that subcommand.
   *
   * @param cmd A pointer to the CLI::App subcommand representing this blueprint task.
   * @param bpTask The blueprint data containing keyValuePairs for the task.
   */
  void addCmdOptions(CLI::App *cmd, const tadah::core::BlueprintTask &bpTask);

  /**
   * @brief The callback of the command @p cmd, the blueprint task
   *        @p blueprintTaskName (TRAIN, DATA.split): stores its options as
   *        a CLITask. A command that groups others (DATA) stores nothing
   *        (commandLineRefusal() refuses it given alone).
   */
  void handleCmdCallback(const std::string &blueprintTaskName, CLI::App *cmd);

  /**
   * @brief The commands that the words @p words of a command line (those
   *        after "tadah") give, as blueprint tasks, in the order typed,
   *        read before CLI11 checks any command's options.
   *
   * Each option of a command takes the words it takes, as CLI11 reads
   * them: a flag none; another option its first word whatever it is (a
   * file named like a command is its value), and its further words up to
   * the number it takes while they are neither an option (a word starting
   * with '-' that is not a negative number) nor the name of a command; a
   * required positional of a command (the name of tadah explain) its word.
   * Any other word that names a command is a command: a first-level name
   * ("train"; "data" with a member's name after it, "data split", the
   * member, or alone the group), or, after a command of a group, the name
   * of another member of that group ("tadah data print ... split ...").
   * "--" ends the commands.
   */
  std::vector<std::string> commandsOfWords(
      const std::vector<std::string> &words) const;

  /**
   * @brief Why the command line is refused before any command runs (""
   *        when it is not): more than one command ("tadah runs one command
   *        per run: the command line gives 2, tadah train and tadah
   *        predict (each runs as a tadah command of its own)"), or a
   *        command that groups others given alone ("tadah data takes one of its commands:
   *        balance, convert, ...").
   */
  std::string commandLineRefusal() const;
std::string handleFooterCallback(const core::BlueprintTask &bpTask);

private:
  /**
   * @brief Maps subcommand pointers to their associated CLITask.
   *
   * TODO require update
   * Each subcommand is associated with one CLITask. When the user invokes that
   * subcommand, CLI11 populates the CLITask's storage.
   */
  // std::unordered_map<CLI::App*, std::vector<CLITask>> cliTasks_;
  std::vector<CLITask> cliTasks_;

  /// The blueprint tasks of the commands the command line gives, in the
  /// order typed (commandsOfWords()): one per run (TadahCLI::run refuses a
  /// second before CLI11 reads the line).
  std::vector<std::string> commandsTyped_;

  /// The CLI11 app of each command by its blueprint task (TRAIN, DATA,
  /// DATA.split).
  std::map<std::string, CLI::App *> commandApps_;

  /// The blueprint task of each command by its words on the command line
  /// ("train", "data", "data split").
  std::map<std::string, std::string> commandNames_;

  /**
   * @brief Stores references to first-level commands by lowercase name.
   *
   * For example, "data" -> CLI::App pointer. This helps in grouping subcommands.
   */
  std::unordered_map<std::string, CLI::App*> firstLevelCommands_;

  ///
  // std::vector<std::string> optionsStorage_; ///< Storage for options.
  std::unordered_map<std::string,std::unordered_map<std::string, std::vector<std::string>>> optionsStorage_; ///< Storage for options.

  /**
   * @brief Processes all tasks in the blueprint and calls user-defined handlers for top-level options and group-level options.
   * @param topLevelHandler Callback that handles an individual top-level option.
   * @param groupHandler Callback that handles an individual group-level option.
   */
  template <typename TopLevelHandler, typename GroupHandler>
  void processBlueprintTasks(TopLevelHandler&& topLevelHandler,
      GroupHandler&& groupHandler);


  size_t left_column_width = 30;
  size_t right_column_width = 50;
  size_t description_paragraph_width = 80;
  size_t footer_paragraph_width = 80;

};

/**
 * @brief The `tadah explain` hook of a tadah built without HPO
 *        (TADAH_ENABLE_NFP off), which TadahCLI sets in place of HPO's
 *        optimiser help (hpo::explain_optimiser).
 *
 * ALGO and LIB, in any letter case, name the optimiser of a `tadah hpo`
 * target: it throws std::runtime_error "tadah explain: ALGO names the
 * optimiser of a `tadah hpo` target, and this tadah is built without HPO
 * (TADAH_ENABLE_NFP OFF): configure it with -DTADAH_ENABLE_NFP=ON". Any
 * other name is not its own: it returns false and writes nothing.
 */
bool explain_without_hpo(const std::string &name, std::ostream &out);

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TADAH_CLI_H
