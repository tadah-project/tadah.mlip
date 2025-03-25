#ifndef TADAH_MLIP_CLI_PREDICT_CLI_H
#define TADAH_MLIP_CLI_PREDICT_CLI_H

#include "CLI/CLI.hpp"
#include "cli_plugin.h"
#include <string>
#include <vector>

namespace tadah {
namespace cli {

/**
 * @brief Implements the "predict" command.
 *
 * Parses prediction options and delegates to PredictEngine.
 */
class PredictCLI : public CLIPlugin {
public:
  void register_module(CLI::App &app) override;
  int run();

private:
  std::string pot_file_;
  std::string config_file_;
  std::vector<std::string> datasets_;
  std::vector<std::string> sources_;
  bool predict_force_ = false;
  bool predict_stress_ = false;
  bool predict_error_ = false;
  bool analytics_ = false;
  int numeric_precision_ = 0;
};

} // namespace cli
} // namespace tadah

#endif // TADAH_MLIP_CLI_PREDICT_CLI_H

