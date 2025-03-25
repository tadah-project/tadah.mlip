#ifndef TADAH_MLIP_CLI_CLI_PLUGIN_H
#define TADAH_MLIP_CLI_CLI_PLUGIN_H

#include "CLI/CLI.hpp"

namespace tadah {
namespace mlip {
namespace cli {

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

protected:
  std::string indicesStr_;
  std::string configFile_;
  std::string potFile_;

  std::vector<std::string> datasets_;
  std::vector<std::string> sources_;
  std::vector<std::string> outFiles_;
  std::vector<std::string> types_;

  std::vector<double> range_;

  int numericPrecision_ = 0;
  int verbose_ = false;

  bool force_= false;
  bool stress_= false;
};

} // namespace cli
} // namespace mlip
} // namespace tadah

#endif // TADAH_MLIP_CLI_CLI_PLUGIN_H

