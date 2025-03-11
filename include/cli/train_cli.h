#ifndef TADAH_MLIP_CLI_TRAIN_CLI_H
#define TADAH_MLIP_CLI_TRAIN_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>

namespace tadah {
namespace cli {

/**
 * @brief Implements the "train" command.
 *
 * Parses training options from the command line and delegates to TrainEngine.
 */
class TrainCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run();

private:
  std::string config_file_;
  bool verbose_ = false;
  bool include_force_ = false;
  bool include_stress_ = false;
};

} // namespace cli
} // namespace tadah

#endif // TADAH_MLIP_CLI_TRAIN_CLI_H

