#ifndef TADAH_MLIP_CLI_TADAH_CLI_H
#define TADAH_MLIP_CLI_TADAH_CLI_H

#include "CLI/CLI.hpp"
#include <string>
#include <map>
#include <tadah/core/blueprint_parser.h>
#include <tadah/mlip/engine_factory.h>

namespace tadah {
namespace mlip {
namespace cli {

/**
 * @brief Main CLI driver that sets up commands/subcommands from the blueprint,
 *        and registers engine callbacks purely by the dot-separated task name.
 *
 * This class does not parse user-config files or merges contexts; that logic
 * is external. The user can call EngineFactory::create(...) in the callback
 * and pass an externally prepared Context if desired.
 */
class TadahCLI {
public:
  /**
   * @brief Constructs a new TadahCLI with a base description.
   */
  TadahCLI();

  /**
   * @brief Parses CLI arguments and lets CLI11 handle subcommand callbacks.
   * @return An integer exit code, typically 0 if OK.
   */
  int run(int argc, char** argv);

private:
  CLI::App app_;

  /**
   * @brief Registers subcommands from the blueprint tasks.
   */
  void registerModules();

  /**
   * @brief Splits a dot notation e.g. "DATA.MERGE" into up to 2 tokens.
   * @param s The string to split.
   * @return A vector of token(s). If '.' is not found, returns 1 token.
   */
  std::vector<std::string> splitOnDot(const std::string &s);

  /**
   * @brief Locates or creates a top-level command in the CLI by name.
   * @param cmdName The uppercase name, e.g. "DATA".
   * @param desc The description to set if creating.
   * @return A pointer to the CLI::App.
   */
  CLI::App* findOrCreateTopLevelCommand(const std::string &cmdName,
                                        const std::string &desc="");

  std::map<std::string, CLI::App*> topCommands_;

private:
  std::map<CLI::App *, bool> hasCallback_;

};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_TADAH_CLI_H
