#ifndef TADAH_MLIP_CLI_CLI_PLUGIN_H
#define TADAH_MLIP_CLI_CLI_PLUGIN_H

#include "CLI/CLI.hpp"

/**
 * @brief Abstract interface for CLI plugins.
 *
 * Each module (e.g. TrainCLI, PredictCLI) must implement register_module().
 */
class CLIPlugin {
public:
  /**
   * @brief Registers the module's subcommands with the given CLI::App.
   * @param app The CLI::App instance.
   */
  virtual void register_module(CLI::App &app) = 0;
  virtual ~CLIPlugin() = default;
};

#endif // TADAH_MLIP_CLI_CLI_PLUGIN_H

