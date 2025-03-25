#ifndef TADAH_MLIP_CLI_HPO_CLI_H
#define TADAH_MLIP_CLI_HPO_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>

namespace tadah {
namespace cli {

/**
 * @brief Implements the "hpo" command.
 *
 * Parses hyperparameter optimization options and delegates to HPOEngine.
 */
class HPOCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run();

private:
  std::string config_file_;
  std::string hpotarget_file_;
  std::string validation_file_;
  bool include_force_ = false;
  bool include_stress_ = false;
};

} // namespace cli
} // namespace tadah

#endif // TADAH_MLIP_CLI_HPO_CLI_H

